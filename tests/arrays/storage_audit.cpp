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
    m->setTargetTriple(e.triple()); m->setDataLayout(e.data_layout()); return m;
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
llvm::json::Value report(const AotObjectEmitter &e) {
    auto j=llvm::json::parse(e.report()); check(bool(j),"invalid certificate JSON"); return std::move(*j);
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
    // Confirmed activation blockers: preserve their rejection rather than
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
        if(argc==4 && std::string(argv[1])=="--emit-architecture-proof") {
            emit_aot_storage_proof(ir::architecture_proof(),argv[2],std::string(argv[3])=="O2",
                std::string(argv[2])+".certificate.json",ProofAdapter::architecture);return 0;
        }
        if(argc==5 && std::string(argv[1])=="--emit-proof") {
            auto p=compile_project(argv[2]);emit_aot_storage_proof(p.module,argv[3],std::string(argv[4])=="O2",std::string(argv[3])+".certificate.json");return 0;
        }
        check(argc==3,"expected artifact directory and project root");std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
        fixtures(out,false);fixtures(out,true);
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
            emit_aot_storage_proof(p.module,object,optimized,object.string()+".certificate.json");
            check(std::filesystem::exists(object),"exact AOT fixture did not certify");
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
            rejects([&]{emit_aot_storage_proof(p.module,object,true,object.string()+".certificate.json");},"final encoded calls differ");
            check(read(object)=="existing owner object","unproved tail encoding damaged destination");std::filesystem::remove(object);
        }
        // The corpus exposes existing raw-aggregate ABI objects, independently
        // of future array lowering. Never certify their ownership by guessing.
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
        rejects([&]{emit_object(source.program->lower(),object,true,true);},"native lowering requires P23-2");
        check(!std::filesystem::exists(object),"source arrays executed natively");
        std::cout<<"AOT storage service fixtures passed (normal/O2); source arrays remain blocked\n";
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
