#include "probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <cstring>
#include <future>
#include <iostream>
#include <set>
using namespace managed_test;
namespace {
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto r=vd::analyze_source({"/managed.void",text});
    if (!r.succeeded()) throw vd::CompilationError(r.diagnostics);
    return r.program;
}
const std::string node="struct node:\n    value: i32\n    next: node?\n\n";
void invalid(const std::string& text,const std::string& reason) {
    auto r=vd::analyze_source({"/managed.void",text});
    require(!r.succeeded(),"invalid managed source accepted");
    bool found=false;
    for(const auto& d:r.diagnostics) {
        require(d.primary.source->text()==text && d.primary.end>d.primary.begin,"managed diagnostic source/span");
        found|=d.description.find(reason)!=std::string::npos;
    }
    if (!found) throw vd::CompilationError(r.diagnostics);
}
void diagnostics() {
    invalid(node+"func entry(x: i32) -> i32:\n    var original = node()\n    const view = original\n    original = view\n    return 0\n","readonly");
    invalid(node+"func entry(x: i32) -> i32:\n    const a = node()\n    var b: node = a\n    b.value = 3\n    return 0\n","readonly");
    invalid(node+"func change(a: node):\n    var alias = a\n    alias.value = 2\n\nfunc entry(x: i32) -> i32:\n    const a = node()\n    change(a)\n    return 0\n","readonly");
    invalid(node+"func identity(const a: node) -> node:\n    return a\n\nfunc entry(x: i32) -> i32:\n    const a = node()\n    var b = identity(a)\n    b.value = 2\n    return 0\n","readonly");
    invalid(node+"func entry(x: i32) -> i32:\n    const a = node(next = node())\n    var b = a.next as node\n    b.value = 2\n    return 0\n","readonly");
    invalid(node+"func hold(a: node) -> node:\n    return node(next = a)\n\nfunc entry(x: i32) -> i32:\n    const a = node()\n    var b = hold(a)\n    return 0\n","readonly");
    invalid(node+"func hold(const a: node) -> node:\n    return node(next = a)\n","readonly");
    invalid(node+"func (p node) disguised():\n    var alias = p\n    alias.value++\n\nfunc entry(x: i32) -> i32:\n    const a = node()\n    a.disguised()\n    return 0\n","readonly");
    invalid(node+"func fresh() -> node:\n    const n = node()\n    return n\n\nfunc entry(x: i32) -> i32:\n    var a = fresh()\n    a.value = 3\n    return 0\n","readonly");
    invalid(node+"func entry(x: i32) -> i32:\n    var a: node\n    return 0\n","non-nullable");
    invalid(node+"struct holder:\n    child: node\n\nfunc entry(x: i32) -> i32:\n    return holder().child.value\n","non-nullable");
    invalid(node+"func entry(x: i32) -> i32:\n    var a: node?\n    return a.value\n","nullable");
    invalid(node+"func entry(x: i32) -> i32:\n    var a = null\n    return 0\n","nullable");
    invalid(node+"func (n node) init():\n    n.value = 1\n","deferred");
    invalid(node+"value struct unsupported:\n    reference: node?\n","deferred");
    invalid(node+"const a = node()\n","compile-time");
    invalid(node+"func entry(x: i32) -> i32:\n    return node(value = true).value\n","Type mismatch");
    invalid(node+"func entry(x: i32) -> i32:\n    return node(value = 1, value = 2).value\n","Duplicate");
    invalid(node+"func entry(x: i32) -> i32:\n    var a = node()\n    a.init()\n    return 0\n","Lifecycle");
}
void roots(const std::filesystem::path& root,bool optimized) {
    auto analysis=vd::analyze_project(root/"examples/managed_structs/main.void");
    require(analysis.succeeded(),"managed project analysis");
    vd::Jit jit; auto runtime=std::make_shared<vd::Runtime>(1);
    auto module=analysis.program->lower(); auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    {
        Probe p;
        { Observe observe(p); require(code->invoke(10)==23,"managed cyclic source result"); }
        require(p.constructions==1002 && p.commits_valid,"managed source publication/collection coverage");
        require(runtime->snapshot().completed_epoch>=1000 && runtime->snapshot().objects<=2,"unreachable source cycle retained");
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"source roots after return");
    }
    Probe p; p.collect_before=true; p.capture=true;
    { Observe observe(p); require(code->invoke(10)==23,"retained source cyclic result"); }
    code.reset(); module={}; analysis={};
    require(jit.live_generations()==0,"source code generation lease leak");
    vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
    require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==2,"source object descriptors depend on retired code");
    std::int32_t value=0; std::memcpy(&value,p.retained->get(),sizeof(value));
    require(value==12,"source alias mutation not retained");
    p.retained.reset(); runtime->collect(); require(runtime->snapshot().objects==0,"unrooted source cycle not reclaimed");
}
void allocation_failures(bool optimized) {
    auto analysis=analyze(node+"func entry(x: i32) -> i32:\n    var current = node(value = 7)\n    current = node(value = 9, next = current)\n    return current.value\n");
    unsigned failures=0;
    for (std::size_t collection_budget:{0u,1u}) for (int budget=0;budget<9;++budget) {
        auto runtime=std::make_shared<vd::Runtime>(collection_budget); vd::Jit jit;
        auto module=analysis->lower(); auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
        Probe p; p.capture=true; p.fail_construction=2; p.fail_allocation=budget;
        bool failed=false;
        { Observe observe(p); try { require(code->invoke(0)==9,"allocation recovery result"); } catch (const std::runtime_error& e) { failed=std::string(e.what()).find("allocation failed")!=std::string::npos; require(failed,"wrong allocation diagnostic"); } }
        require(p.commits_valid && p.retained,"allocation output publication protection");
        vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime);
        std::int32_t value; std::memcpy(&value,p.retained->get(),sizeof(value)); require(value==7,"failed replacement changed old object");
        runtime->collect(); require(runtime->snapshot().objects==1,"failed parent or trace cleanup leak");
        p.retained.reset(); runtime->collect(); require(runtime->snapshot().objects==0,"allocation failure cleanup");
        if (failed) ++failures;
        require(code->invoke(0)==9,"source runtime failed to recover");
    }
    require(failures>=4,"insufficient source allocation failure sites");
}
void source_polls(bool optimized) {
    auto analysis=analyze("struct marker:\n    x: i32\n\nfunc entry(n: i32) -> i32:\n    for n > 0:\n        n--\n    return n\n");
    vd::Jit jit; auto code=jit.compile(*analysis,optimized); auto runtime=code->runtime_owner();
    Probe p; p.gate_poll=4;
    auto worker=std::async(std::launch::async,[&] { Observe o(p); return code->invoke(1000000); });
    struct Release { Probe& p; bool done=false; void now(){if(!done){done=true;p.release.release();}} ~Release(){now();} } release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(2)),"source backedge poll absent");
    auto collector=std::async(std::launch::async,[&] { vd::Runtime::Attachment a(*runtime); vd::Runtime::Managed m(*runtime); return runtime->collect(std::chrono::seconds(2)); });
    bool pending=runtime->wait_for_request(std::chrono::seconds(2)); release.now();
    require(collector.get()==vd::Runtime::Collection::completed && pending,"source rendezvous overlap");
    require(worker.get()==0 && runtime->snapshot().cooperative_parks>0,"source numeric loop did not park");
}
void concurrent_source(const std::filesystem::path& root,bool optimized) {
    auto a=vd::analyze_project(root/"examples/managed_structs/main.void");
    require(a.succeeded(),"concurrent source analysis");
    vd::Jit jit; auto runtime=std::make_shared<vd::Runtime>(8);
    auto module=a.program->lower(); auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
    std::vector<std::future<void>> workers;
    for (int n=0;n<4;++n) workers.push_back(std::async(std::launch::async,[&,n] {
        Probe p; Observe observe(p);
        for (int i=0;i<5;++i) require(code->invoke(n+i)==2*(n+i)+3,"concurrent parsed-source result");
        require(p.commits_valid,"concurrent source publication");
    }));
    for (auto& worker:workers) worker.get();
    vd::Runtime::Attachment attached(*runtime); vd::Runtime::Managed managed(*runtime);
    require(runtime->snapshot().completed_epoch>0,"concurrent automatic GC missing");
    runtime->collect(); require(runtime->snapshot().objects==0,"concurrent source roots leaked");
}
void language_edges(bool optimized) {
    auto text=node+"func readonly_identity(const n: node) -> node:\n    return n\n\nfunc entry(x: i32) -> i32:\n    var original = node(value = x)\n    const view = original\n    original.value += 1\n    var alias: node = readonly_identity(view)\n    var optional: node? = original\n    optional = null\n    if optional == null:\n        return alias.value\n    return -999\n";
    vd::Jit jit; require(jit.compile(*analyze(text),optimized)->invoke(10)==11,"readonly parameter/return and nullable annotation");
    auto empty=analyze("struct empty:\n\nfunc entry(x: i32) -> i32:\n    var a = empty()\n    var b = empty()\n    if a != b:\n        return 1\n    return 0\n");
    require(jit.compile(*empty,optimized)->invoke(0)==1,"empty managed identities");
    auto holder=analyze(node+"struct holder:\n    child: node\n\nfunc (n node) rebind(other: node):\n    n = other\n\nfunc entry(x: i32) -> i32:\n    var old = node(value = 7)\n    var other = node(value = 9)\n    old.rebind(other)\n    return holder(child = old).child.value\n");
    require(jit.compile(*holder,optimized)->invoke(0)==7,"required child and receiver reference rebinding");
}
void metadata() {
    vd::ir::Type retained;
    {
        auto a=analyze(node+"func entry(x: i32) -> i32:\n    return node().value\n");
        for (const auto& d:a->declarations()) if (d.kind==vd::AnalyzedDeclaration::Kind::managed_structure) retained=d.type;
    }
    vd::ir::Type next=retained.definition->fields.at(1).type;
    require(next.nullable && next.definition==retained.definition,"cyclic nominal query lifetime");
    vd::Jit jit;
    auto a=analyze(node+"func entry(x: i32) -> i32:\n    return node(value = x).value\n");
    vd::ReloadSlot slot(jit.compile(*a)); auto owner=slot.acquire()->runtime_owner();
    Probe live; live.capture=true;
    { Observe observe(live); require(slot.invoke(2)==2,"source object before reload"); }
    auto b=analyze(node+"func entry(x: i32) -> i32:\n    return node(value = x + 1).value\n");
    require(jit.replace(slot,*b) && slot.invoke(2)==3 && slot.acquire()->runtime_owner()==owner,"managed source compatible reload/runtime owner");
    auto c=analyze(node+"struct extra:\n    x: i32\n\nfunc entry(x: i32) -> i32:\n    return x\n");
    require(!jit.replace(slot,*c),"managed layout change reloaded");
    { vd::Runtime::Attachment attached(*owner); vd::Runtime::Managed managed(*owner);
      owner->collect(); require(owner->snapshot().objects==1,"live source object lost during reload");
      std::int32_t value=0; std::memcpy(&value,live.retained->get(),sizeof(value));
      require(value==2,"reload reinitialized an existing source object");
      live.retained.reset(); owner->collect();
    }
    vd::Runtime other;
    { vd::Runtime::Attachment attached(other); vd::Runtime::Managed managed(other);
      bool rejected=false; try { slot.invoke(1); } catch(const std::logic_error&) { rejected=true; }
      require(rejected && other.is_managed(),"wrong-runtime source entry did not preserve caller state");
    }
}
}
int main(int argc,char** argv) {
    try {
        if (argc==5 && std::string(argv[1])=="--emit-library") { auto p=vd::compile_project(argv[2]); vd::emit_runtime_object(p.module,argv[3],std::string(argv[4])=="optimized"); return 0; }
        require(argc==2,"managed test root argument"); std::filesystem::path root=argv[1];
        diagnostics(); metadata();
        for (bool optimized:{false,true}) {
            language_edges(optimized); roots(root,optimized); allocation_failures(optimized); source_polls(optimized); concurrent_source(root,optimized);
            for (const auto& name:{"handoffs","order","imports","numeric"}) {
                auto a=vd::analyze_project(root/(std::string("examples/managed_structs/")+name+".void"));
                if (!a.succeeded()) throw vd::CompilationError(a.diagnostics);
                vd::Jit jit; auto code=jit.compile(*a.program,optimized);
                Probe p; p.collect_before=true; Observe observe(p);
                int expected=std::string(name)=="handoffs" ? 48 : std::string(name)=="order" ? 221 : std::string(name)=="numeric" ? 600 : 12;
                require(code->invoke(10)==expected && p.commits_valid,"source order/receiver/import result");
            }
        }
        std::cout<<"managed source: readonly, cyclic metadata, publication, GC, failure cleanup, source polls and reload passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
