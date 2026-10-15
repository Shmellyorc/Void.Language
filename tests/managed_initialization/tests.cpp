#include "../storage_proof.hpp"
#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <cstring>
#include <future>
#include <iostream>
using namespace managed_test;
namespace {
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/initialization.void",text});
    if (!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    return result.program;
}
const std::string types="struct leaf:\n    value: i32\n\nstruct owner:\n    child: leaf\n    value: i32\n    next: owner?\n\n";
const std::string declaration="func (o owner) init(c: leaf, flag: bool = true):\n";
void invalid(const std::string& text,const std::string& reason,bool related=false) {
    auto result=vd::analyze_source({"/initialization.void",text});
    require(!result.succeeded(),"invalid construction accepted"); bool found=false;
    for (const auto& d:result.diagnostics) {
        require(d.primary.end>d.primary.begin && d.primary.source->path()=="/initialization.void","construction diagnostic location");
        if (d.description.find(reason)!=std::string::npos) { found=true; if (related) require(!d.related.empty(),"unsafe helper missing related source use"); }
    }
    if (!found) throw vd::CompilationError(result.diagnostics);
}
void diagnostics() {
    for (const auto& body:{"    o.value = 1\n","    o.value = o.child.value\n    o.child = c\n","    if flag:\n        o.child = c\n","    if flag:\n        return\n    o.child = c\n","    for flag:\n        o.child = c\n        break\n","    o.child.value++\n    o.child = c\n"}) invalid(types+declaration+body,"initialized");
    invalid(types+"func initialize(o: owner, c: leaf):\n    o.child = c\n\n"+declaration+"    initialize(o, c)\n","initialized");
    invalid(types+declaration+"    o.child = c\n    o.next = o\n","stored");
    invalid(types+declaration+"    o.child = c\n    var alias = o\n    alias = owner(c)\n","rebound");
    invalid(types+declaration+"    o.child = c\n    o = owner(c)\n","rebound");
    invalid(types+declaration+"    o.child = c\n    var alias = o as owner\n","cast");
    invalid(types+declaration+"    o.child = c\n    var alias: owner? = o\n","nullable");
    invalid(types+"func same(o: owner) -> bool:\n    return o == o\n\n"+declaration+"    o.child = c\n    if same(o):\n        o.value = 1\n","Unsafe construction helper",true);
    invalid(types+"func identity(o: owner) -> owner:\n    return o\n\n"+declaration+"    o.child = c\n    var alias = identity(o)\n","Unsafe construction helper",true);
    invalid(types+"func replace(o: owner):\n    o = owner(leaf())\n\n"+declaration+"    o.child = c\n    replace(o)\n","Unsafe construction helper",true);
    invalid(types+"struct wrapper:\n    child: owner\n\nfunc (w wrapper) init(o: owner):\n    w.child = o\n\n"+declaration+"    o.child = c\n    var w = wrapper(o)\n","constructor argument");
    invalid(types+declaration+"    o.child = c\n    const view = o\n    var alias = view\n    alias.value++\n","readonly");
    invalid(types+"func (o owner) init(const c: leaf):\n    o.child = c\n","readonly");
    invalid(types+declaration+"    o.child = c\n\nfunc entry(x: i32) -> i32:\n    var o = owner(leaf())\n    o.init(leaf())\n    return 0\n","Lifecycle");
    invalid(types+"func (o owner) init(c: leaf) -> i32:\n    return 1\n","must not declare");
    invalid(types+declaration+"    o.child = c\n\nfunc entry(x: i32) -> i32:\n    return owner(flag = true, c = leaf(), flag = false).value\n","more than once");
    invalid(types+declaration+"    o.child = c\n\nfunc entry(x: i32) -> i32:\n    return owner(child = leaf()).value\n","Unknown initializer parameter");
    invalid(types+declaration+"    o.child = c\n\nfunc entry(x: i32) -> i32:\n    var o: owner\n    return 0\n","non-nullable");
    invalid(types+"func (o owner) init(c: leaf = leaf()):\n    o.child = c\n","compile-time");
    invalid("value struct required:\n    value: i32\n\nfunc (r required) init(value: i32):\n    r.value = value\n\nstruct parent:\n    child: required\n\nfunc (p parent) init():\n    p.child = required(1)\n","required argument");
}
void context_limit() {
    std::string source="struct node:\n    value: i32\n\nfunc fanout(";
    for(unsigned p=0;p<11;++p) {if(p) source+=", ";source+="p"+std::to_string(p)+": node";}
    source+="):\n    return\n\nfunc (n node) init(ordinary: node):\n";
    // One declaration, many passing masks. Dynamic objects/call instances must
    // never create an unbounded specialization scheme or silently drop checks.
    for(unsigned mask=1;mask<=1024;++mask) {
        source+="    fanout(";
        for(unsigned p=0;p<11;++p) {if(p) source+=", ";source+=(mask & (1u<<p)) ? "n" : "ordinary";}
        source+=")\n";
    }
    invalid(source,"Construction helper context limit");
}
void flow(bool optimized) {
    auto a=analyze(types+declaration+"    if flag:\n        var alias: owner = o\n        alias.child = c\n    else:\n        for:\n            o.child = c\n            break\n    o.value = o.child.value\n\nfunc entry(x: i32) -> i32:\n    var a = owner(leaf(value = x), false)\n    var b = owner(leaf(value = x))\n    return a.value + b.value\n");
    vd::Jit jit; require(jit.compile(*a,optimized)->invoke(10)==20,"required-reference control-flow/readiness");
    auto empty=analyze("struct empty:\n\nfunc (e empty) init():\n    return\n\nfunc entry(x: i32) -> i32:\n    if empty() != empty():\n        return 1\n    return 0\n");
    require(jit.compile(*empty,optimized)->invoke(0)==1,"empty managed initializer identity");
}
std::shared_ptr<const vd::ValidatedAnalysis> project(const std::filesystem::path& root,const std::string& name) {
    auto a=vd::analyze_project(root/("examples/managed_initialization/"+name+".void"));
    if (!a.succeeded()) throw vd::CompilationError(a.diagnostics); return a.program;
}
void roots(const std::filesystem::path& root,bool optimized) {
    auto a=project(root,"stress"); auto module=a->lower(); vd::Jit jit;
    auto runtime=std::make_shared<vd::Runtime>(1); auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    Probe p; p.inspect_roots=true; p.capture=true; bool observed=false;
    p.before_construct=[&](auto count,const void*,auto) { if (count==303) { std::int32_t value=0; std::memcpy(&value,p.retained->get(),sizeof(value)); observed=value==213; } };
    { Observe o(p); require(code->invoke(10)==246,"managed init order/recursive helpers/result"); }
    require(observed && p.constructions==303 && p.commits_valid && p.roots_valid && p.frames.empty() && p.roots_checked>1000,"parent publication/precise stage roots");
    require(runtime->snapshot().completed_epoch>=302,"automatic collection during staged init missing");
    { vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime); runtime->collect(); require(runtime->snapshot().objects==1,"staging roots escaped after return"); p.retained.reset(); runtime->collect(); }
    Probe graph; graph.inspect_roots=true; graph.collect_before=true; graph.capture=true; graph.capture_at=303;
    { Observe o(graph); require(code->invoke(10)==246,"forced staged root collections"); }
    code.reset(); module={}; a.reset(); require(jit.live_generations()==0,"construction code lease leaked");
    vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime); runtime->collect();
    require(runtime->snapshot().objects==2 && graph.roots_valid && graph.frames.empty(),"completed graph depends on retired init code");
    void* child=nullptr; std::memcpy(&child,graph.retained->get(),sizeof(child)); std::int32_t value=0; std::memcpy(&value,child,sizeof(value)); require(value==3,"staged child did not survive optimized calls");
    graph.retained.reset(); runtime->collect(); require(runtime->snapshot().objects==0,"completed graph not reclaimed");
}
void failures(const std::filesystem::path& root,bool optimized) {
    for (const auto& name:{"failure","fail_default","fail_argument","fail_child"}) {
        vd::Jit jit; auto code=jit.compile(*project(root,name),optimized); auto runtime=code->runtime_owner();
        Probe p; p.inspect_roots=true; p.collect_before=true; p.capture=true;
        { Observe observe(p); bool failed=false; try { code->invoke(0); } catch(const std::runtime_error& e) { auto message=std::string(e.what()); failed=message.find("zero")!=std::string::npos && message.find(std::string(name)+".void:")!=std::string::npos && message.find("construction::")==std::string::npos; } require(failed,"managed init failure source trace"); }
        require(p.commits_valid && p.roots_valid && p.frames.empty() && p.retained,"failed init publication/root cleanup");
        vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime);
        std::int32_t value=0; std::memcpy(&value,p.retained->get(),sizeof(value));
        require(value==((std::string(name)=="failure" || std::string(name)=="fail_child") ? 8 : 7),"initializer effects rolled back/reordered");
        runtime->collect(); require(runtime->snapshot().objects==1,"failed parent allocated or child roots leaked"); p.retained.reset(); runtime->collect(); require(runtime->snapshot().objects==0,"failed stage cleanup");
    }
    const std::string source="struct node:\n    value: i32\n    next: node?\n\nfunc (n node) init(value: i32, old: node? = null):\n    if old != null:\n        var previous = old as node\n        previous.value++\n    n.value = value\n\nfunc entry(x: i32) -> i32:\n    var current = node(7)\n    current = node(9, old = current)\n    return current.value\n";
    auto a=analyze(source); unsigned failures=0;
    for (auto collection_budget:{0u,1u}) for (int budget=0;budget<9;++budget) {
        vd::Jit jit; auto runtime=std::make_shared<vd::Runtime>(collection_budget); auto module=a->lower(); auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
        Probe p; p.inspect_roots=true; p.capture=true; p.fail_construction=2; p.fail_allocation=budget;
        { Observe observe(p); try { require(code->invoke(0)==9,"constructor success result"); } catch(const std::runtime_error& e) { require(std::string(e.what()).find("allocation failed")!=std::string::npos,"unexpected commit error"); ++failures; } }
        require(p.commits_valid && p.roots_valid && p.frames.empty() && p.retained,"commit failure exposed output");
        vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime);
        std::int32_t value=0; std::memcpy(&value,p.retained->get(),sizeof(value)); require(value==8,"body must precede commit failure without effect rollback");
        runtime->collect(); require(runtime->snapshot().objects==1,"failed/unused parent retained"); p.retained.reset(); runtime->collect(); require(code->invoke(0)==9,"constructor runtime recovery");
    }
    require(failures>=4,"constructor commit failure sites not exercised");
}
void parked(const std::filesystem::path& root,bool optimized) {
    vd::Jit jit; auto code=jit.compile(*project(root,"stress"),optimized); auto runtime=code->runtime_owner();
    Probe p; p.inspect_roots=true; p.gate_after_constructions=2;
    auto worker=std::async(std::launch::async,[&] { Observe observe(p); return code->invoke(10); });
    struct Release { Probe& p; bool done=false; void now(){if(!done){done=true;p.release.release();}} ~Release(){now();} } release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(5)),"staged helper safepoint absent");
    require(runtime->snapshot().objects==2,"parent acquired identity before initializer success");
    auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);return runtime->collect(std::chrono::seconds(5));});
    bool requested=runtime->wait_for_request(std::chrono::seconds(5)); release.now();
    require(collector.get()==vd::Runtime::Collection::completed && requested,"staged rendezvous overlap");
    require(worker.get()==246 && p.roots_valid && p.frames.empty() && runtime->snapshot().cooperative_parks>0,"staged helper did not retain/poll actual roots");
}
void timeout(const std::filesystem::path& root,bool optimized) {
    vd::Jit jit; auto module=project(root,"stress")->lower(); auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    Probe p; p.inspect_roots=true; p.capture=true;
    std::binary_semaphore at_commit{0},commit{0},blocked{0},unblock{0};
    p.before_construct=[&](auto count,const void*,auto) { if(count==303) {at_commit.release();commit.acquire();} };
    auto worker=std::async(std::launch::async,[&] {
        Observe observe(p);
        try { code->invoke(10); return false; }
        catch(const std::runtime_error& e) { return std::string(e.what()).find("allocation failed")!=std::string::npos; }
    });
    struct Release { std::binary_semaphore& semaphore; bool done=false; void now(){if(!done){done=true;semaphore.release();}} ~Release(){now();} } release_commit{commit};
    require(at_commit.try_acquire_for(std::chrono::seconds(5)),"missing final commit boundary");
    const auto before=runtime->snapshot();
    // Admit the deliberately noncooperating participant only AFTER the body's
    // automatic collections. No runtime/collector-required lock is held here.
    auto blocker=std::async(std::launch::async,[&] {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);blocked.release();unblock.acquire();});
    Release release_blocker{unblock};
    require(blocked.try_acquire_for(std::chrono::seconds(5)),"timeout participant missing");
    auto collector=std::async(std::launch::async,[&] {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);return runtime->collect(std::chrono::seconds(1));});
    bool requested=runtime->wait_for_request(std::chrono::seconds(5)); release_commit.now();
    auto result=collector.get(); bool failed=worker.get();
    auto after=runtime->snapshot(); release_blocker.now(); blocker.get();
    require(requested && result==vd::Runtime::Collection::timed_out && failed && after.cooperative_parks>before.cooperative_parks,"commit did not join failed rendezvous");
    require(after.objects==before.objects && !after.requested && p.roots_valid && p.frames.empty() && p.commits_valid,"failed rendezvous swept/published parent or leaked frames");
    { vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
      std::int32_t value=0;std::memcpy(&value,p.retained->get(),sizeof(value));require(value==213,"timeout rolled back completed body effects");
      runtime->collect();require(runtime->snapshot().objects==1,"failed rendezvous left construction roots");p.retained.reset();runtime->collect(); }
    require(code->invoke(10)==246,"managed construction did not recover after rendezvous timeout");
}
void contracts() {
    const std::string source=types+"func helper(o: owner):\n    o.value++\n\n"+declaration+"    o.child = c\n    helper(o)\n\nfunc entry(x: i32) -> i32:\n    return owner(leaf(value = x)).value\n";
    auto a=analyze(source); vd::Jit jit; vd::ReloadSlot slot(jit.compile(*a));
    auto changed=source; auto at=changed.find("o.value++"); changed.replace(at,9,"o.value += 2");
    require(!jit.replace(slot,*analyze(changed)),"initialization helper body changed through reload");
    changed=source; at=changed.find("return owner(leaf(value = x)).value"); changed.replace(at,std::string("return owner(leaf(value = x)).value").size(),"return owner(leaf(value = x)).value + 1");
    require(jit.replace(slot,*analyze(changed)) && slot.invoke(10)==2,"unrelated source body could not reload with staged helpers");
    bool facts=false;
    for (const auto& d:a->declarations()) if(d.kind==vd::AnalyzedDeclaration::Kind::initializer) {
        facts=d.construction_parameters==std::vector<std::vector<unsigned>>{{0}} && d.required_initialization_fields==std::vector<std::string>{"child"};
    }
    require(facts,"validated initialization readiness/context facts missing");
    auto module=a->lower(); unsigned variants=0; for(const auto& f:module.functions) for(auto t:f.parameters) variants+=t.construction;
    require(variants>=2 && variants<=4,"bounded helper contexts missing");
    auto corrupt=module; bool mutated=false;
    for(auto& f:corrupt.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::call && i.initialization && i.receiver && f.locals[i.receiver->slot].construction) { i.initialization=false; mutated=true; }
    auto rejected=[](const auto& ir) {try {vd::ir::validate(ir);}catch(const std::invalid_argument&){return true;}return false;};
    require(mutated && rejected(corrupt),"IR accepted unmarked init");
    corrupt=module; mutated=false;
    for(auto& f:corrupt.functions) if(f.initializer && f.parameters[0].construction)
        for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::gc_write && i.immediate==0) {i.op=vd::ir::Op::gc_poll;mutated=true;}
    require(mutated && rejected(corrupt),"IR accepted required-field use without first assignment");
    corrupt=module; mutated=false;
    for(auto& f:corrupt.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::gc_construct && i.receiver) {i.receiver.reset();mutated=true;}
    require(mutated && rejected(corrupt),"IR accepted custom construction without its proven staging place");
    corrupt=module;
    for(auto& f:corrupt.functions) if(!f.construction_origins.empty()) {f.construction_origins.emplace(static_cast<unsigned>(f.locals.size()),0);break;}
    require(rejected(corrupt),"IR accepted out-of-range construction origin");
    corrupt=module; mutated=false;
    for(auto& f:corrupt.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::gc_construct && i.receiver) {i.op=vd::ir::Op::gc_allocate;i.receiver.reset();mutated=true;}
    require(mutated && rejected(corrupt),"IR legacy allocation bypassed custom initialization");
    corrupt=module; mutated=false;
    for(auto& f:corrupt.functions) if(!f.receiver && !f.parameters.empty() && f.parameters[0].construction) {
        f.locals.clear();f.construction_origins.clear();f.blocks={vd::ir::Block{{},{vd::ir::Terminator::Kind::return_void}}};mutated=true;break;
    }
    require(mutated && rejected(corrupt),"IR accepted construction parameter without owned formal storage");
}
}
int main(int argc,char** argv) {
    try {
        const bool storage_proof = argc==6 && std::string(argv[5])=="--storage-proof";
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-library") {auto p=vd::compile_project(argv[2]);storage_fixture::emit(p.module,argv[3],std::string(argv[4])=="optimized",storage_proof);return 0;}
        require(argc==2,"managed init test root argument"); std::filesystem::path root=argv[1]; diagnostics(); context_limit(); contracts();
        for(bool optimized:{false,true}) {flow(optimized);roots(root,optimized);failures(root,optimized);parked(root,optimized);timeout(root,optimized);}
        std::cout<<"managed initialization: staged views, readiness, helpers, roots, commit/failure/reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
