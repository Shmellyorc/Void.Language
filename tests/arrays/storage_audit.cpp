#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "void/llvm_storage_audit.hpp"
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/TargetParser/Triple.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace vd;
using namespace vd::llvm_backend;
namespace {
void check(bool v, const std::string &why) { if (!v) throw std::runtime_error(why); }
template<class F> void rejects(F f, const std::string &needle) {
    try { f(); } catch (const std::exception &e) {
        check(std::string(e.what()).find(needle)!=std::string::npos,"wrong rejection: "+std::string(e.what())); return;
    }
    throw std::runtime_error("missing rejection: "+needle);
}
std::string read(const std::filesystem::path &p) { std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
std::unique_ptr<llvm::Module> parse(llvm::LLVMContext &c, AotObjectEmitter &e, const std::string &body) {
    llvm::SMDiagnostic d; auto m=llvm::parseAssemblyString(body,d,c);
    if(!m) throw std::runtime_error("invalid fixture LLVM IR");
    m->setTargetTriple(llvm::Triple(e.triple())); m->setDataLayout(e.data_layout()); return m;
}
LoweringInventory inventory(llvm::Module &m,bool arrays=true,bool omit=false,bool incomplete=false) {
    LoweringInventory result;
    for(auto &f:m) if(!f.isDeclaration()) {
        result.function(f,arrays && f.getName()!="wrapper");
        unsigned n=0;
        for(auto &b:f) for(auto &i:b) if(auto *a=llvm::dyn_cast<llvm::AllocaInst>(&i)) {
            if(omit) continue;
            result.home(*a,StorageRole::local,HomeKind::value,"original/"+std::to_string(n++));
        }
        auto roles=all_storage_roles();if(incomplete)roles.erase(StorageRole::roots);
        result.complete(f,roles);
    }
    return result;
}
std::string single(unsigned n) { return "declare void @touch(ptr, i64)\ndefine void @sample() {\n %a = alloca ["+std::to_string(n)+" x i32], align 4\n call void @touch(ptr %a,i64 "+std::to_string(n)+")\n ret void\n}\n"; }
void privateFixedProof(const std::filesystem::path& out,bool optimized) {
    auto analysis=analyze_source({"private-types.void","func shape(a: [u64, 1048576]) -> [u64, 1048576]:\n    return a\n"});
    check(analysis.succeeded(),"large private signature analysis failed");
    auto function=analysis.program->lower().functions.front();function.fixed_ownership=true;
    function.parameters.assign(8,function.result);
    AotObjectEmitter emitter(llvm::sys::getDefaultTargetTriple(),optimized);
    const auto layout=emitter.fixed_call_layout(function);
    check(layout.profile==emitter.identity() && layout.operands.size()==10 && layout.result_is_home,"private target/profile signature mismatch");
    check(layout.handle.bytes==emitter.data_layout().getPointerSize() && layout.handle.bytes<1048576,"payload entered private call home");
    for(unsigned n=0;n<8;++n) check(layout.operands[n].mode==FixedCallOperand::Mode::snapshot_home &&
        layout.operands[n].storage.bytes==layout.handle.bytes,"array argument is not a rooted handle home");
    check(layout.operands[8].mode==FixedCallOperand::Mode::result_home && layout.operands[9].mode==FixedCallOperand::Mode::failure,"result/failure convention changed");
    // Native ABI checks, independently compiled C++ layout on this runner only.
    check(layout.handle.bytes==sizeof(void*) && layout.slot.bytes==sizeof(Runtime::Slot) && layout.slot.alignment==alignof(Runtime::Slot) &&
        layout.slot_offsets==std::vector<std::uint64_t>{offsetof(Runtime::Slot,storage),offsetof(Runtime::Slot,size),offsetof(Runtime::Slot,offsets),offsetof(Runtime::Slot,count)} &&
        layout.failure.bytes==sizeof(RuntimeFailure) && layout.failure.alignment==alignof(RuntimeFailure) &&
        layout.failure_offsets==std::vector<std::uint64_t>{offsetof(RuntimeFailure,message),offsetof(RuntimeFailure,count),offsetof(RuntimeFailure,frames)},"private root/failure ABI mismatch");
    auto altered=function;altered.parameters[0]=ir::Type::i32;
    check(emitter.fixed_call_layout(altered).logical_identity!=layout.logical_identity,"signature identity omitted scalar kind");
    altered.parameters[0]=ir::Type::i64;
    auto other=emitter.fixed_call_layout(altered).logical_identity;altered.parameters[0]=ir::Type::i32;
    check(other!=emitter.fixed_call_layout(altered).logical_identity,"distinct scalar kinds collide");
    altered.fixed_ownership=false;rejects([&]{emitter.fixed_call_layout(altered);},"ownership contract");
    altered.fixed_ownership=true;altered.parameters={ir::Type::i32};altered.result=ir::Type::i32;
    rejects([&]{emitter.fixed_call_layout(altered);},"Array-free ABI");
    // The current independent auditor must NOT be bypassed for this pointer-home
    // call shape. No runtime root/ABI certificate is inferred from small homes.
    llvm::LLVMContext context;
    auto module=parse(context,emitter,
        "declare void @bridge(ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr,ptr)\n"
        "define void @private_call(ptr %a,ptr %b,ptr %c,ptr %d,ptr %e,ptr %f,ptr %g,ptr %h,ptr %out,ptr %fail) {\n"
        " %home=alloca ptr,align 8\n %payload=load ptr,ptr %a\n store ptr %payload,ptr %home\n"
        " call void @bridge(ptr %home,ptr %b,ptr %c,ptr %d,ptr %e,ptr %f,ptr %g,ptr %h,ptr %out,ptr %fail)\n ret void\n}\n");
    auto original=inventory(*module);emitter.freeze(*module,original);emitter.optimize();
    auto object=out/(optimized?"private-fixed-O2.o":"private-fixed-normal.o");
    {std::ofstream sentinel(object);sentinel<<"private owner sentinel";}
    bool rejected=false;
    try {emitter.deliver(object);} catch(const std::exception& error) {
        rejected=true;std::cout<<"private fixed "<<(optimized?"O2":"normal")<<" certification retained: "<<error.what()<<'\n';
    }
    check(rejected && read(object)=="private owner sentinel","uncertified private call delivered or sentinel damaged");
    {std::ofstream report(object.string()+".certificate.json");report<<emitter.report();}
    // Deliberately never link or execute the rejected object.
}
llvm::json::Value report(const AotObjectEmitter &e) {
    auto j=llvm::json::parse(e.report()); check(bool(j),"invalid certificate JSON"); return std::move(*j);
}
void outgoingFixtures(const std::filesystem::path &out, bool optimized) {
    const std::string seven="declare i64 @sink7(ptr,i64,i64,i64,i64,i64,i64)\n";
    const std::string eleven="declare i64 @sink11(ptr,i64,i64,i64,i64,i64,i64,i64,i64,i64,i64)\n";
    const std::string call7="call i64 @sink7(ptr %a,i64 1001,i64 1002,i64 1003,i64 1004,i64 1005,i64 1006)";
    const std::string call11="call i64 @sink11(ptr %a,i64 1001,i64 1002,i64 1003,i64 1004,i64 1005,i64 1006,i64 1007,i64 1008,i64 1009,i64 1010)";
    for(const std::string kind:{"reserved","reserved-padding","padding","multiple","branch","loop","near-under","near-limit","hidden-result","tail"}) {
        const unsigned bytes=kind=="near-limit"?65520:kind=="near-under"?65456:32;
        std::string source=seven+eleven+"declare i64 @sink0(ptr)\ndeclare {i64,i64,i64,i64,i64,i64,i64,i64} @large()\n";
        source+="define i64 @sample(i32 %go){\nentry:\n %a=alloca ["+std::to_string(bytes)+" x i8],align 8\n";
        if(kind=="reserved") source+=" %r=call i64 @sink0(ptr %a)\n ret i64 %r\n";
        else if(kind=="multiple") source+=" %x="+call7+"\n %y="+call11+"\n %r=add i64 %x,%y\n ret i64 %r\n";
        else if(kind=="branch") source+=" %test=icmp ne i32 %go,0\n br i1 %test,label %yes,label %no\nyes:\n %x="+call7+"\n br label %done\nno:\n %y="+call11+"\n br label %done\ndone:\n %r=phi i64 [%x,%yes],[%y,%no]\n ret i64 %r\n";
        else if(kind=="loop") source+=" br label %body\nbody:\n %n=phi i32 [0,%entry],[%next,%body]\n %x="+call11+"\n %next=add i32 %n,1\n %test=icmp ult i32 %next,3\n br i1 %test,label %body,label %done\ndone:\n ret i64 %x\n";
        else if(kind=="hidden-result") source+=" %x=call {i64,i64,i64,i64,i64,i64,i64,i64} @large()\n %r=extractvalue {i64,i64,i64,i64,i64,i64,i64,i64} %x,7\n ret i64 %r\n";
        else if(kind=="tail") { source="declare i64 @tail_target(i32)\ndefine i64 @sample(i32 %go){ %r=tail call i64 @tail_target(i32 %go)\n ret i64 %r\n"; }
        else source+=" %r="+(kind=="reserved-padding"?call7:call11)+"\n ret i64 %r\n";
        source+="}\n";
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,source);auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
        const auto base=out/("outgoing-"+kind+(optimized?"-O2":"-normal"));
        {std::error_code ec;llvm::raw_fd_ostream ir(base.string()+".ll",ec);check(!ec,"cannot save outgoing IR");m->print(ir,nullptr);}
        auto object=base.string()+".o";
        if(kind=="reserved" || kind=="reserved-padding") {e.deliver(object);std::filesystem::remove(object);}
        else {
            {std::ofstream owner(object);owner<<"existing owner object";}
            rejects([&]{e.deliver(object);},kind=="hidden-result"?"unmatched allocation":kind=="tail"?"tail":"unproved nonreserved outgoing");
            check(read(object)=="existing owner object","outgoing rejection damaged destination");std::filesystem::remove(object);
        }
        {std::ofstream evidence(base.string()+".json");evidence<<e.report();}
        auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("sample");
        auto *observation=r->getObject("outgoing_sp_observation");check(observation,"missing outgoing CFG evidence");
        check(!observation->getBoolean("final_encoded_cfg_bound").value_or(true) && observation->getInteger("reconciliation_credit")==0,"unproved encoded CFG earned credit");
        if(kind=="tail") check(!observation->getBoolean("complete_machine_observation").value_or(true),"tail ownership falsely observed");
        else {
            check(observation->getBoolean("complete_machine_observation").value_or(false),"outgoing fixture observation incomplete: "+kind);
            check(*observation->getInteger("peak_entry_sp_depth")>=*r->getInteger("frame"),"SP peak omitted static frame");
            if(kind=="padding") check(r->getInteger("outgoing")==40 && observation->getInteger("additional_below_F")==48,"alignment padding missed");
            if(kind=="near-limit") check(r->getInteger("C")==65536 && *observation->getInteger("additional_below_F")>0,"near-limit omission not exposed");
        }
    }
    // A repeated dynamic home cannot be bounded by the largest call area.
    // Indirect calls and realignment also lack this observation profile.
    for(const std::string kind:{"cycle","indirect","realigned"}) {
        std::string source;
        if(kind=="cycle") source="declare void @touch(ptr,i64)\ndefine void @sample(i32 %n){br label %body\nbody:\n %i=phi i32 [0,%0],[%next,%body]\n %a=alloca [32 x i8],align 8\n call void @touch(ptr %a,i64 32)\n %next=add i32 %i,1\n %test=icmp ult i32 %next,%n\n br i1 %test,label %body,label %done\ndone:\n ret void }\n";
        else if(kind=="indirect") source="define void @sample(ptr %target){ %a=alloca [32 x i8],align 8\n call void %target(ptr %a,i64 1001,i64 1002,i64 1003,i64 1004,i64 1005,i64 1006,i64 1007,i64 1008,i64 1009,i64 1010)\n ret void }\n";
        else source="declare void @touch(ptr,i64)\ndefine void @sample(){ %a=alloca [32 x i8],align 32\n call void @touch(ptr %a,i64 32)\n ret void }\n";
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,source);auto inv=inventory(*m,kind!="cycle");e.freeze(*m,inv);e.optimize();
        const auto base=out/("outgoing-unsupported-"+kind+(optimized?"-O2":"-normal"));
        const auto object=base.string()+".o";{std::ofstream owner(object);owner<<"existing owner object";}
        rejects([&]{e.deliver(object);},kind=="cycle"?"dynamic/opaque":kind=="indirect"?"unproved nonreserved outgoing":"realigned");
        check(read(object)=="existing owner object","unsupported outgoing shape damaged destination");std::filesystem::remove(object);
        auto j=report(e);auto *o=j.getAsObject()->getObject("functions")->getObject("sample")->getObject("outgoing_sp_observation");
        check(!o->getBoolean("complete_machine_observation").value_or(true) && o->get("peak_entry_sp_depth")->kind()==llvm::json::Value::Null,"unsupported outgoing shape received a bound");
        std::ofstream evidence(base.string()+".json");evidence<<e.report();
    }
}
void researchCFGFixtures(const std::filesystem::path &out, bool optimized) {
    llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
    auto m=parse(c,e,"declare i64 @one(ptr)\ndeclare i64 @two(ptr)\ndefine i64 @sample(i32 %go){\n %a=alloca [32 x i8],align 8\n %test=icmp eq i32 %go,0\n br i1 %test,label %yes,label %no\nyes:\n %x=call i64 @one(ptr %a)\n ret i64 %x\nno:\n %y=call i64 @two(ptr %a)\n ret i64 %y\n}\n");
    auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
    const auto base=out/(optimized?"research-CFG-O2":"research-CFG-normal");auto object=base.string()+".o";
    e.deliver(object);const auto original=read(object);
    auto result=e.research_encoded_cfg(object,"sample",e.report());auto j=llvm::json::parse(result);
    check(j && j->getAsObject()->getBoolean("supported").value_or(false),"research CFG rejected original: "+result);
    check(!j->getAsObject()->getBoolean("delivery_credit").value_or(true),"research CFG authorized delivery");
    {std::ofstream evidence(base.string()+".json");evidence<<result;}
    auto parsed=llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(original,"fixture"));check(bool(parsed),"fixture object parser failed");
    size_t fileOffset=0;
    for(auto &[symbol,size]:llvm::object::computeSymbolSizes(**parsed)) {
        auto name=symbol.getName();check(bool(name),"unreadable fixture symbol");if(*name!="sample")continue;
        auto section=symbol.getSection();auto address=symbol.getAddress();check(bool(section)&&bool(address),"unreadable fixture section");
        auto content=(**section).getContents();check(bool(content),"unreadable fixture bytes");
        fileOffset=size_t(content->data()-original.data())+size_t(*address-(**section).getAddress());
    }
    check(fileOffset>0,"missing fixture function offset");
    auto *instructions=j->getAsObject()->getArray("instructions");check(instructions,"missing decoded instructions");
    const llvm::json::Object *branch=nullptr;
    for(const auto &v:*instructions) {auto *n=v.getAsObject();auto op=n->getString("opcode").value_or("");if(op=="JCC_1" || op=="JCC_4"){branch=n;break;}}
    check(branch,"fixture lost conditional branch");
    auto offset=*branch->getInteger("offset"),size=*branch->getInteger("size");unsigned width=size==2?1:4;
    for(bool middle:{false,true}) {
        auto bytes=original;int64_t delta=(middle?1:0)-offset-size;
        check(width==4 || (delta>=-128 && delta<=127),"fixture displacement does not fit");
        for(unsigned n=0;n<width;++n) bytes[fileOffset+size_t(offset+size-width+n)]=char(uint64_t(delta)>>(8*n));
        const auto negative=base.string()+(middle?"-middle.never-executed.o":"-cycle.never-executed.o");
        {std::ofstream file(negative,std::ios::binary);file.write(bytes.data(),std::streamsize(bytes.size()));}
        auto rejection=e.research_encoded_cfg(negative,"sample",e.report());auto r=llvm::json::parse(rejection);
        check(r && !r->getAsObject()->getBoolean("supported").value_or(true),"mutated branch received supported CFG");
        check(r->getAsObject()->getString("error").value_or("").contains(middle?"middle of instruction":"inconsistent join/growing"),"wrong branch rejection: "+rejection);
        check(read(negative)==bytes && read(object)==original,"read-only verifier changed objects");
        std::ofstream evidence(negative+".json");evidence<<rejection;
    }
}
void fixtures(const std::filesystem::path &out, bool optimized) {
    const auto suffix=optimized?"-O2":"-normal";
    auto path=out/(std::string("fixture")+suffix+".o");
    for(bool oversized:{false,true}) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(oversized?16384:16));auto inv=inventory(*m);
        e.freeze(*m,inv);e.optimize();
        if(oversized) {
            {std::ofstream f(path,std::ios::binary);f<<"owner object";}
            rejects([&]{e.deliver(path);},"65536-byte policy exceeded");
            check(read(path)=="owner object","rejection damaged existing object");std::filesystem::remove(path);
        } else {
            e.deliver(path);check(std::filesystem::file_size(path)>0,"no certified object");
            {std::ofstream evidence(out/(std::string("accepted")+suffix+".json"));evidence<<e.report();}
            auto j=report(e);check(j.getAsObject()->getBoolean("certified_delivery").value_or(false),"uncertified delivery");
            check(j.getAsObject()->getBoolean("array_storage_policy_applicable").value_or(false),"lost array policy");
            rejects([&]{e.deliver(path);},"lifecycle");std::filesystem::remove(path);
        }
    }
    for(bool omit:{false,true}) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16));auto inv=inventory(*m,true,omit,!omit);
        rejects([&]{e.freeze(*m,inv);},omit?"omitted":"categories");
        check(!std::filesystem::exists(path),"incomplete inventory delivered object");
    }
    const std::string inlined="declare void @touch(ptr,i64)\ndefine void @worker(){ %a=alloca [256 x i32],align 4\n call void @touch(ptr %a,i64 256)\n ret void }\ndefine void @wrapper(){ call void @worker()\n ret void }\n";
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,inlined);auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
        if(optimized) {rejects([&]{e.deliver(path);},"unmatched allocation");check(!std::filesystem::exists(path),"inline clone delivered object");}
        else {e.deliver(path);std::filesystem::remove(path);}
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,"declare void @touch(ptr,i64)\ndeclare void @llvm.lifetime.start.p0(i64 immarg,ptr nocapture)\ndeclare void @llvm.lifetime.end.p0(i64 immarg,ptr nocapture)\ndefine void @sample(){\n %a=alloca [64 x i32],align 4\n %b=alloca [64 x i32],align 4\n call void @llvm.lifetime.start.p0(i64 256,ptr %a)\n call void @touch(ptr %a,i64 64)\n call void @llvm.lifetime.end.p0(i64 256,ptr %a)\n call void @llvm.lifetime.start.p0(i64 256,ptr %b)\n call void @touch(ptr %b,i64 64)\n call void @llvm.lifetime.end.p0(i64 256,ptr %b)\n ret void }\n");
        auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();e.deliver(path);
        auto j=report(e);auto *f=j.getAsObject()->getObject("functions")->getObject("sample");
        check(f->getInteger("P")==512 && f->getInteger("R")==0,"reuse received subtraction credit");
        unsigned homes=0;for(auto &o:*f->getArray("objects")) if(!o.getAsObject()->getBoolean("spill").value_or(true)) ++homes;
        check(homes==1,"fixture did not actually reuse storage");std::filesystem::remove(path);
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16));auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
        auto *f=m->getFunction("sample");llvm::IRBuilder<> b(&*f->getEntryBlock().begin());
        auto *extra=b.CreateAlloca(b.getInt32Ty());b.CreateCall(m->getFunction("touch"),{extra,b.getInt64(1)});
        rejects([&]{e.deliver(path);},"content mismatch");check(!std::filesystem::exists(path),"unplanned machine home delivered");
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16));auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
        m->setDataLayout("e-p:32:32");rejects([&]{e.deliver(path);},"identity mismatch");
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,"declare void @touch(ptr,i64)\ndefine void @sample(){\n %a=alloca [8 x i8],align 8\n %b=alloca [8 x i8],align 8\n %c=alloca [8 x i8],align 8\n %d=alloca [8 x i8],align 8\n %e=alloca [8 x i8],align 8\n %f=alloca [8 x i8],align 8\n %g=alloca [8 x i8],align 8\n %h=alloca [8 x i8],align 8\n call void @touch(ptr %a,i64 8)\n call void @touch(ptr %b,i64 8)\n call void @touch(ptr %c,i64 8)\n call void @touch(ptr %d,i64 8)\n call void @touch(ptr %e,i64 8)\n call void @touch(ptr %f,i64 8)\n call void @touch(ptr %g,i64 8)\n call void @touch(ptr %h,i64 8)\n ret void }\n");
        LoweringInventory inv;auto &f=*m->getFunction("sample");inv.function(f,true);
        const StorageRole roles[]={StorageRole::parameter,StorageRole::result,StorageRole::copy,StorageRole::roots,StorageRole::roots,StorageRole::roots,StorageRole::staging,StorageRole::known_abi};
        const HomeKind kinds[]={HomeKind::argument,HomeKind::result,HomeKind::copy,HomeKind::root,HomeKind::frame,HomeKind::slots,HomeKind::staging,HomeKind::native_guard};
        unsigned n=0;for(auto &b:f)for(auto &i:b)if(auto *a=llvm::dyn_cast<llvm::AllocaInst>(&i)){inv.home(*a,roles[n],kinds[n],"distinct/"+std::to_string(n));++n;}
        inv.complete(f,all_storage_roles());e.freeze(*m,inv);e.optimize();e.deliver(path);
        auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("sample");
        check(r->getInteger("P")==64 && r->getArray("immutable_homes")->size()==8,"lost purpose/extent inventory");std::filesystem::remove(path);
    }
    // Existing array-free ABI has no newly imposed array storage cap.
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16384));auto inv=inventory(*m,false);e.freeze(*m,inv);e.optimize();e.deliver(path);
        auto j=report(e);check(!j.getAsObject()->getBoolean("array_storage_policy_applicable").value_or(true),"array-free ABI newly capped");
        check(j.getAsObject()->getObject("functions")->getObject("sample")->get("C")->kind()==llvm::json::Value::Null,"array-free storage falsely certified");
        std::filesystem::remove(path);
    }
    // Research checker limitations: preserve their rejection rather than
    // exempt ordinary ABI homes or credit guessed physical extents.
    if(!optimized) {
        const std::string bodies[]={
            "declare void @touch(ptr,i64)\ndefine void @sample(i1 %go){\n br i1 %go,label %allocate,label %done\nallocate:\n %home=alloca [272 x i8],align 8\n call void @touch(ptr %home,i64 272)\n br label %done\ndone:\n ret void }\n",
            "declare void @touch(ptr,i64)\ndefine void @sample(){\n %slots=alloca [0 x {ptr,i64,ptr,i64}],align 8\n call void @touch(ptr %slots,i64 0)\n ret void }\n"};
        for(unsigned n=0;n<2;++n) {
            llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),false);
            auto m=parse(c,e,bodies[n]);auto inv=inventory(*m,false);e.freeze(*m,inv);e.optimize();
            if(n==0) {
                rejects([&]{e.deliver(path);},"unsupported dynamic/opaque frame");
                check(!std::filesystem::exists(path),"unproved dynamic shape delivered object");
            } else {
                e.deliver(path);auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("sample");
                check(r->getInteger("P")==0 && r->getInteger("R")==0,"zero home earned credit");
                check(r->getArray("zero_extent_homes")->size()==1,"missing physical zero-home reconciliation");
                std::filesystem::remove(path);
            }
            std::ofstream evidence(out/(n?"zero-slots-proof.json":"conditional-unmarked-rejection.json"));evidence<<e.report();
        }
    }
    {
        const std::string cli="declare void @touch(ptr,i64)\n%Failure=type {ptr,i32,[32 x ptr]}\ndefine i32 @main(i32 %argc,ptr %argv){\nentry:\n %test=icmp eq i32 %argc,1\n br i1 %test,label %run,label %parse\nparse:\n %end=alloca ptr,align 8\n store ptr null,ptr %end\n call void @touch(ptr %end,i64 1)\n br label %run\nrun:\n %failure=alloca %Failure,align 8\n store %Failure zeroinitializer,ptr %failure\n call void @touch(ptr %failure,i64 1)\n ret i32 0 }\n";
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);auto m=parse(c,e,cli);
        auto &f=*m->getFunction("main");LoweringInventory inv;inv.function(f,true);
        std::vector<llvm::AllocaInst*> originals;std::vector<llvm::Instruction*> initializers;
        for(auto &b:f)for(auto &i:b) {
            if(auto *a=llvm::dyn_cast<llvm::AllocaInst>(&i)) {
                originals.push_back(a);inv.home(*a,StorageRole::known_abi,a->getName()=="end"?HomeKind::cli_parse:HomeKind::cli_failure,a->getName().str());
            }
            if(llvm::isa<llvm::StoreInst>(i)) initializers.push_back(&i);
        }
        inv.complete(f,all_storage_roles());e.freeze(*m,inv);
        for(auto *a:originals) check(a->isStaticAlloca(),"CLI home was not moved by identity");
        check(initializers[0]->getParent()->getName()=="parse" && initializers[1]->getParent()->getName()=="run","CLI initialization moved across control flow");
        e.optimize();e.deliver(path);auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("main");
        check(r->getArray("entry_home_moves")->size()==2 && !r->getBoolean("has_variable_objects").value_or(true),"CLI normalization still dynamic");
        std::set<std::string> original_blocks;
        for(const auto &move:*r->getArray("entry_home_moves"))
            original_blocks.insert(move.getAsObject()->getString("original_block").value_or("").str());
        check(original_blocks==std::set<std::string>{"parse","run"},"frozen block names did not survive O2 deletion");
        std::ofstream evidence(out/(std::string("cli-home-proof")+suffix+".json"));evidence<<e.report();std::filesystem::remove(path);
        for(const std::string reason:{"repeated","alignment","stack-control","variable","shape"}) {
            llvm::LLVMContext nc;AotObjectEmitter ne(llvm::sys::getDefaultTargetTriple(),optimized);auto text=cli;
            if(reason=="repeated") text.replace(text.find(" br label %run"),14," br label %parse");
            if(reason=="alignment") text.replace(text.find("alloca ptr,align 8"),18,"alloca ptr,align 32");
            if(reason=="variable") text.replace(text.find("alloca ptr,align 8"),18,"alloca ptr,i32 %argc,align 8");
            if(reason=="shape") text.replace(text.find("alloca ptr,align 8"),18,"alloca i64,align 8");
            if(reason=="shape") text.replace(text.find("store ptr null,ptr %end"),23,"store i64 0,ptr %end");
            if(reason=="stack-control") {text="declare ptr @llvm.stacksave.p0()\n"+text;text.insert(text.find(" %test=")," %saved=call ptr @llvm.stacksave.p0()\n");}
            auto nm=parse(nc,ne,text);auto &nf=*nm->getFunction("main");LoweringInventory ni;ni.function(nf,true);
            for(auto &b:nf)for(auto &i:b)if(auto *a=llvm::dyn_cast<llvm::AllocaInst>(&i)) ni.home(*a,StorageRole::known_abi,a->getName()=="end"?HomeKind::cli_parse:HomeKind::cli_failure,a->getName().str());
            ni.complete(nf,all_storage_roles());rejects([&]{ne.freeze(*nm,ni);},reason=="repeated"?"repeatedly":reason=="stack-control"?"stack control":"shape/alignment");
            check(!std::filesystem::exists(path),"unproved CLI case delivered object");
        }
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,"declare void @touch(ptr,i64)\ndefine void @worker(){ %zero=alloca [0 x i32],align 4\n call void @touch(ptr %zero,i64 0)\n ret void }\ndefine void @wrapper(){call void @worker()\n ret void }\n");
        auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
        if(optimized) {rejects([&]{e.deliver(path);},"unmatched allocation");check(!std::filesystem::exists(path),"untracked zero clone received certificate");}
        else {e.deliver(path);std::filesystem::remove(path);}
    }
    if(!optimized) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),false);
        auto m=parse(c,e,"declare void @touch(ptr,i64)\ndefine void @sample(i1 %go){br i1 %go,label %body,label %done\nbody:\n %zero=alloca [0 x i32],align 4\n call void @touch(ptr %zero,i64 0)\n br label %done\ndone:\n ret void }\n");
        auto inv=inventory(*m,false);e.freeze(*m,inv);e.optimize();
        rejects([&]{e.deliver(path);},"unsupported dynamic/opaque frame");check(!std::filesystem::exists(path),"dynamic zero-byte home bypassed certificate");
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16));auto inv=inventory(*m);e.freeze(*m,inv);
        // The caller's mutable inventory is not consulted after freezing.
        inv.complete(*m->getFunction("sample"),{});e.optimize();e.deliver(path);
        check(report(e).getAsObject()->getString("inventory_sha256")->size()==64,"inventory identity absent");
        std::filesystem::remove(path);
    }
    if(!optimized) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),false);
        auto m=parse(c,e,"define available_externally void @sample(){ret void}\n");
        auto inv=inventory(*m,false);e.freeze(*m,inv);e.optimize();
        rejects([&]{e.deliver(path);},"incomplete machine-function coverage");
        check(!std::filesystem::exists(path),"incomplete machine coverage delivered");
    }
    // Array-free inlining must not bypass provenance coverage either.
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,inlined);auto inv=inventory(*m,false);e.freeze(*m,inv);e.optimize();
        if(optimized) rejects([&]{e.deliver(path);},"unmatched allocation");
        else {e.deliver(path);std::filesystem::remove(path);}
    }
    for(const std::string attribute:{"target-cpu","tune-cpu","target-features","target-abi"}) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16));m->getFunction("sample")->addFnAttr(attribute,"unbound-profile");
        auto inv=inventory(*m);rejects([&]{e.freeze(*m,inv);},"target profile mismatch");
        check(!std::filesystem::exists(path),"mismatched function profile delivered");
    }
    for(bool before_optimizer:{false,true}) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,single(16));auto inv=inventory(*m);e.freeze(*m,inv);
        if(!before_optimizer) e.optimize();
        m->getFunction("sample")->addFnAttr("noinline");
        {std::ofstream owner(path);owner<<"existing owner object";}
        if(before_optimizer) rejects([&]{e.optimize();},"content mismatch");
        else rejects([&]{e.deliver(path);},"content mismatch");
        check(read(path)=="existing owner object","profile/content rejection damaged destination");std::filesystem::remove(path);
    }
    // Only the exact observed byte-count rewrite of declared opaque homes is
    // recoverable. It keeps P and every physical byte in F, with zero R credit.
    for(const std::string variant:{"frame","staging","missing","wrong-role","wrong-type","over-budget"}) {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        unsigned bytes=variant=="over-budget"?40000:32;
        auto text="declare void @touch(ptr,i64)\ndefine void @sample(){ %a=alloca "+std::string(variant=="wrong-type"?"i16,i64 16":"i8,i64 "+std::to_string(bytes))+",align 8\n call void @touch(ptr %a,i64 "+std::to_string(bytes)+")\n ret void }\n";
        auto m=parse(c,e,text);auto &f=*m->getFunction("sample");auto &a=llvm::cast<llvm::AllocaInst>(*f.getEntryBlock().begin());
        LoweringInventory inv;inv.function(f,true);
        const bool staging=variant=="staging";
        inv.home(a,variant=="wrong-role"?StorageRole::local:staging?StorageRole::staging:StorageRole::roots,
                 variant=="missing"?HomeKind::value:staging?HomeKind::staging:HomeKind::frame,"opaque/original");
        inv.complete(f,all_storage_roles());e.freeze(*m,inv);e.optimize();
        bool missing=variant=="missing" || variant=="wrong-role" || variant=="wrong-type";
        if(optimized && (missing || variant=="over-budget")) {
            {std::ofstream owner(path);owner<<"existing owner object";}
            rejects([&]{e.deliver(path);},missing?"unmatched allocation":"65536-byte policy exceeded");
            check(read(path)=="existing owner object","canonical rejection damaged destination");std::filesystem::remove(path);
        } else {
            e.deliver(path);auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("sample");
            check(r->getInteger("P")==bytes,"canonical rewrite changed immutable P");
            if(optimized) {
                check(r->getInteger("R")==0 && r->getArray("canonical_byte_array_homes")->size()==1,"canonical rewrite granted credit or lost lineage");
                check(r->getInteger("C")==((bytes+*r->getInteger("frame")+*r->getInteger("E")+15)/16)*16,"canonical charge omitted physical storage");
            }
            std::filesystem::remove(path);
        }
        std::ofstream evidence(out/(std::string("canonical-")+variant+suffix+".json"));evidence<<e.report();
    }
    {
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,"declare void @touch(ptr,i64)\ndeclare void @llvm.lifetime.start.p0(i64 immarg,ptr nocapture)\ndeclare void @llvm.lifetime.end.p0(i64 immarg,ptr nocapture)\ndefine void @sample(){ %a=alloca i8,i64 32,align 8\n %b=alloca i8,i64 32,align 8\n call void @llvm.lifetime.start.p0(i64 32,ptr %a)\n call void @touch(ptr %a,i64 32)\n call void @llvm.lifetime.end.p0(i64 32,ptr %a)\n call void @llvm.lifetime.start.p0(i64 32,ptr %b)\n call void @touch(ptr %b,i64 32)\n call void @llvm.lifetime.end.p0(i64 32,ptr %b)\n ret void }\n");
        auto &f=*m->getFunction("sample");LoweringInventory inv;inv.function(f,true);unsigned id=0;
        for(auto &bb:f)for(auto &instruction:bb)if(auto *a=llvm::dyn_cast<llvm::AllocaInst>(&instruction))
            inv.home(*a,StorageRole::roots,HomeKind::frame,"distinct/"+std::to_string(id++));
        inv.complete(f,all_storage_roles());e.freeze(*m,inv);e.optimize();e.deliver(path);
        auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("sample");
        check(r->getInteger("P")==64 && r->getInteger("R")==0,"canonical reuse changed P or granted credit");
        unsigned physical=0;for(const auto &o:*r->getArray("objects")) if(!o.getAsObject()->getBoolean("spill").value_or(true))++physical;
        check(physical==1,"canonical reuse fixture did not share storage");
        if(optimized) check(j.getAsObject()->getArray("canonical_rewrite_observations")->size()==2,"distinct canonical identities were conflated");
        std::ofstream evidence(out/(std::string("canonical-reuse")+suffix+".json"));evidence<<e.report();std::filesystem::remove(path);
    }
    const llvm::Triple native(llvm::sys::getDefaultTargetTriple());
    if(native.isOSLinux() && native.getArch()==llvm::Triple::x86_64) {
        outgoingFixtures(out,optimized);
        researchCFGFixtures(out,optimized);
        llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),optimized);
        auto m=parse(c,e,"declare void @many(ptr,i64,i64,i64,i64,i64,i64,i64,i64,i64,i64)\ndefine void @sample(){ %a=alloca [65520 x i8],align 8\n call void @many(ptr %a,i64 1,i64 2,i64 3,i64 4,i64 5,i64 6,i64 7,i64 8,i64 9,i64 10)\n ret void }\n");
        auto inv=inventory(*m);e.freeze(*m,inv);e.optimize();
        {std::ofstream owner(path);owner<<"existing owner object";}
        rejects([&]{e.deliver(path);},"unproved nonreserved outgoing");
        check(read(path)=="existing owner object","unreserved call frame delivered");std::filesystem::remove(path);
        auto j=report(e);auto *r=j.getAsObject()->getObject("functions")->getObject("sample");
        check(!r->getBoolean("reserved_call_frame").value_or(true) && *r->getInteger("outgoing")>0,"fixture failed to expose outgoing ownership gap");
        std::ofstream evidence(out/(std::string("unreserved-call-frame")+suffix+".json"));evidence<<e.report();
    }
    AotObjectEmitter profile(llvm::sys::getDefaultTargetTriple(),optimized);
    check(profile.identity().find(optimized?"PassBuilder(no-TM)":"verify-only")!=std::string::npos,"optimizer identity incomplete");
}
}
int main(int argc,char **argv) {
    try {
        if(argc==7 && std::string(argv[1])=="--research-cfg") {
            auto aliases=[](const std::filesystem::path &a,const std::filesystem::path &b) {
                std::error_code ec;
                return std::filesystem::weakly_canonical(a)==std::filesystem::weakly_canonical(b) || std::filesystem::equivalent(a,b,ec);
            };
            check(!aliases(argv[2],argv[5]) && (std::string(argv[4])=="-" || !aliases(argv[4],argv[5])),"Research report must not replace input evidence");
            AotObjectEmitter emitter(llvm::sys::getDefaultTargetTriple(),std::string(argv[6])=="O2");
            auto result=emitter.research_encoded_cfg(argv[2],argv[3],std::string(argv[4])=="-"?"":read(argv[4]));
            {std::ofstream output(argv[5]);output<<result;}
            auto parsed=llvm::json::parse(result);return parsed->getAsObject()->getBoolean("supported").value_or(false)?0:2;
        }
        if(argc==4 && std::string(argv[1])=="--emit-architecture-proof") {
            emit_aot_storage_proof(ir::architecture_proof(),argv[2],std::string(argv[3])=="O2",
                std::string(argv[2])+".certificate.json",ProofAdapter::architecture);return 0;
        }
        if(argc==5 && std::string(argv[1])=="--emit-proof") {
            auto p=compile_project(argv[2]);emit_aot_storage_proof(p.module,argv[3],std::string(argv[4])=="O2",std::string(argv[3])+".certificate.json");return 0;
        }
        check(argc==3,"expected artifact directory and project root");std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
        fixtures(out,false);fixtures(out,true);
        privateFixedProof(out,false);privateFixedProof(out,true);
        // A zero-count frame must neither read Slot storage nor hide its live
        // parent frame. Null storage makes any accidental Slot read observable.
        {
            Runtime runtime(1);Runtime::Attachment attach(runtime);Runtime::Managed managed(runtime);
            void* live=nullptr;const std::size_t offset=0;
            {
                const Runtime::Slot slot{&live,sizeof(live),&offset,1};Runtime::Frame parent(runtime,&slot,1);
                runtime.allocate_into(&live,{sizeof(void*),{}});
                {Runtime::Frame empty(runtime,nullptr,0);
                    check(runtime.collect()==Runtime::Collection::completed && runtime.contains(live),"empty Slot frame hid parent roots");}
            }
            check(runtime.collect()==Runtime::Collection::completed && runtime.snapshot().objects==0,"empty Slot frame leaked root lifetime");
        }
        const llvm::Triple target(llvm::sys::getDefaultTargetTriple());
        const bool linux_x64=target.isOSLinux() && target.getArch()==llvm::Triple::x86_64;
        // Exact source failure classifications below are native Linux evidence.
        // Other targets retain the portable service/scalar fixtures, without
        // inventing incoming-ABI or final-encoding expectations for them.
        if(linux_x64) for(bool optimized:{false,true}) for(const auto *file:{"main","handoffs"}) {
            auto p=compile_project(std::filesystem::path(argv[2])/"examples/managed_structs"/(std::string(file)+".void"));
            const auto object=out/(std::string(file)+(optimized?"-O2.o":"-normal.o"));
            if (optimized && std::string(file)=="main") {
                // LLVM 23 O2 inlines node.add into make_cycle and clones four
                // homes. This research checker deliberately cannot authorize
                // cloned ownership. Require its exact fail-closed disposition;
                // ordinary optimized AOT/JIT execution remains covered below
                // and by the independently linked source CLI suites.
                {std::ofstream owner(object);owner<<"existing owner object";}
                rejects([&]{emit_aot_storage_proof(p.module,object,true,object.string()+".certificate.json");},
                        "unmatched allocation (including clone/merge) receives no subtraction credit");
                check(read(object)=="existing owner object","cloned-home rejection damaged destination");
                auto certificate=llvm::json::parse(read(object.string()+".certificate.json"));
                check(bool(certificate),"cloned-home rejection certificate missing");
                auto *errors=certificate->getAsObject()->getObject("errors");
                auto *reasons=errors ? errors->getArray("__vd_2f6d61696e2e766f69643a3a6d616b655f6379636c65__void") : nullptr;
                check(errors && errors->size()==1 && reasons && reasons->size()==4,
                      "LLVM 23 cloned-home rejection changed");
                for(const auto& reason:*reasons)
                    check(reason.getAsString()=="unmatched allocation (including clone/merge) receives no subtraction credit",
                          "unexpected LLVM 23 cloned-home rejection");
            } else {
                emit_aot_storage_proof(p.module,object,optimized,object.string()+".certificate.json");
                check(std::filesystem::exists(object),"exact AOT fixture did not certify");
            }
            if(optimized) {
                // Same real module without compiler-owned home identities must
                // retain the former rejection; shape/byte equality is no proof.
                llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),true);
                auto original=parse(c,e,read(object.string()+".entry.ll"));auto missing=inventory(*original,false);
                e.freeze(*original,missing);e.optimize();auto rejected=out/(std::string(file)+"-missing-lineage.o");
                {std::ofstream owner(rejected);owner<<"existing owner object";}
                rejects([&]{e.deliver(rejected);},"unmatched allocation");
                check(read(rejected)=="existing owner object","actual optimized AOT rejection damaged destination");
                std::filesystem::remove(rejected);
            }
            if(!optimized) {
                llvm::LLVMContext c;AotObjectEmitter e(llvm::sys::getDefaultTargetTriple(),false);
                auto original=parse(c,e,read(object.string()+".original.ll"));auto incomplete=inventory(*original,false);
                e.freeze(*original,incomplete);e.optimize();
                const auto rejected=out/(std::string(file)+"-unmarked.o");
                {std::ofstream owner(rejected);owner<<"existing owner object";}
                rejects([&]{e.deliver(rejected);},"unsupported dynamic/opaque frame");
                check(read(rejected)=="existing owner object","exact AOT rejection damaged destination");
                std::ofstream evidence(out/(std::string(file)+"-unmarked-rejection.json"));evidence<<e.report();
                std::filesystem::remove(rejected);
            }
        }
        if(linux_x64) {
            auto p=compile_project(std::filesystem::path(argv[2])/"examples/inheritance/stress.void");
            auto object=out/"virtual-tail-O2.o";
            {std::ofstream owner(object);owner<<"existing owner object";}
            // The stderr adapter changes LLVM's module-wide inline budget. Keep
            // the real virtual tail dispatcher outlined so this negative checks
            // final-byte reconciliation rather than an incidental inline choice.
            const auto seed=out/"virtual-tail-source-O2.o";
            {std::ofstream owner(seed);owner<<"existing seed object";}
            // LLVM 23's inliner also clones four homes in this seed. Keep its
            // exact rejection; only the independently verified original IR is
            // needed to construct the existing outlined-dispatcher negative.
            rejects([&]{emit_aot_storage_proof(p.module,seed,true,seed.string()+".certificate.json");},
                    "unmatched allocation (including clone/merge) receives no subtraction credit");
            check(read(seed)=="existing seed object","rejected virtual seed damaged destination");
            auto seed_report=llvm::json::parse(read(seed.string()+".certificate.json"));
            check(bool(seed_report),"virtual seed rejection report missing");
            unsigned clones=0,encoded=0;
            for(const auto& entry:*seed_report->getAsObject()->getObject("errors"))
                for(const auto& reason:*entry.second.getAsArray()) {
                    if(reason.getAsString()=="unmatched allocation (including clone/merge) receives no subtraction credit")++clones;
                    else if(reason.getAsString()=="final encoded calls differ from machine certificate")++encoded;
                    else check(false,"unexpected LLVM 23 virtual seed rejection");
                }
            check(clones==4 && encoded==1,"LLVM 23 virtual seed rejection changed");
            llvm::LLVMContext context;AotObjectEmitter emitter(llvm::sys::getDefaultTargetTriple(),true);
            auto original=parse(context,emitter,read(seed.string()+".original.ll"));
            unsigned dispatchers=0;
            for(auto& function:*original)if(function.getName().starts_with("virtual_")) {
                function.addFnAttr(llvm::Attribute::NoInline);++dispatchers;
            }
            check(dispatchers>0,"virtual tail fixture lost its dispatcher");
            auto homes=inventory(*original,false);emitter.freeze(*original,homes);emitter.optimize();
            rejects([&]{emitter.deliver(object);},"final encoded calls differ");
            {std::ofstream evidence(object.string()+".certificate.json");evidence<<emitter.report();}
            check(read(object)=="existing owner object","unproved tail encoding damaged destination");std::filesystem::remove(object);
            emit_object(p.module,out/"ordinary-virtual-tail-O2.o",true,true);
            Jit jit;auto code=jit.compile(p.module,p.module.entry_symbol,true);
            check(code->invoke(10)==1110,"encoded-call checker rejection blocked correct optimized JIT");
        }
        // Valid production programs can exceed this research checker's ability
        // to recover ABI home ownership. A rejection must not govern normal
        // compilation. The CLI suite also links and executes these exact files.
        if(linux_x64) for(bool optimized:{false,true}) for(const auto *file:{"main","numeric"}) {
            auto p=compile_project(std::filesystem::path(argv[2])/"examples/value_structs"/(std::string(file)+".void"));
            auto object=out/(std::string("aggregate-")+file+(optimized?"-O2.o":"-normal.o"));
            {std::ofstream owner(object);owner<<"existing owner object";}
            rejects([&]{emit_aot_storage_proof(p.module,object,optimized,object.string()+".certificate.json");},"unproved incoming ownership");
            check(read(object)=="existing owner object","aggregate ABI rejection damaged destination");
            auto j=llvm::json::parse(read(object.string()+".certificate.json"));check(bool(j),"aggregate rejection report missing");
            bool incoming=false,unmatched=false;
            for(const auto &entry:*j->getAsObject()->getObject("functions")) {
                const auto *f=entry.second.getAsObject();
                incoming|=!f->getArray("incoming")->empty();
                unmatched|=!f->getArray("unmatched_homes")->empty();
            }
            check(incoming,"actual aggregate incoming evidence lost");
            const auto ordinary=out/(std::string("ordinary-aggregate-")+file+(optimized?"-O2.o":"-normal.o"));
            emit_object(p.module,ordinary,optimized,true);
            check(std::filesystem::file_size(ordinary)>0,"research rejection blocked ordinary AOT");
            check(!std::filesystem::exists(ordinary.string()+".certificate.json"),"ordinary AOT requested research evidence");
            Jit jit;auto code=jit.compile(p.module,p.module.entry_symbol,optimized);
            check(code->invoke(10)==(std::string(file)=="main"?46:15),"research rejection blocked correct JIT execution");
            if(!optimized && std::string(file)=="numeric") check(unmatched,"actual ABI-created unmatched home evidence lost");
            std::filesystem::remove(object);
        }
        for(bool optimized:{false,true}) {
            auto p=compile_project(std::filesystem::path(argv[2])/"examples/architecture.void");
            auto object=out/(optimized?"scalar-O2.o":"scalar-normal.o");
            emit_aot_storage_proof(p.module,object,optimized,object.string()+".certificate.json");
            check(std::filesystem::exists(object),"actual scalar CLI certification failed");
        }
        auto source=analyze_source({"arrays.void","func entry(x: i32) -> i32:\n    var values: [i32, 2] = [1, 2]\n    return values[0]\n"});
        check(source.succeeded(),"array analysis rejected");
        auto object=out/"source-array.o";
        emit_object(source.program->lower(),object,true,true);
        check(std::filesystem::file_size(object)>0,"ordinary source-array native emission failed");
        std::cout<<"AOT storage service fixtures passed (normal/O2); ordinary source arrays emit without research certification\n";
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
