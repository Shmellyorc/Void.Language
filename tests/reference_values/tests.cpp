#include "../storage_proof.hpp"
#include "proof.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <future>
#include <iostream>
using namespace managed_test;
namespace {
const std::string types="struct node:\n    value: i32\n\nvalue struct packet:\n    owner: node?\n    count: i32\n\nvalue struct outer:\n    nested: packet\n\n";
const std::string nested_receiver="value struct point:\n    x: i32\n\nstruct object:\n    child: point\n\nvalue struct carrier:\n    reference: object\n\nfunc (p point) bump():\n    p.x++\n\nfunc (c carrier) mutate():\n    c.reference.child.bump()\n\nfunc entry(value: i32) -> i32:\n    var original = carrier(reference = object())\n    var copy = original\n    copy.mutate()\n    return copy.reference.child.x\n";
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/reference.void",text});
    if(!result.succeeded()) throw vd::CompilationError(result.diagnostics);
    return result.program;
}
void invalid(const std::string& source,const std::string& reason) {
    auto result=vd::analyze_source({"/reference.void",source});
    require(!result.succeeded(),"unsafe reference value accepted");bool found=false;
    for(const auto& d:result.diagnostics) {
        require(d.primary.source && d.primary.source->text()==source && d.primary.end>d.primary.begin,"reference value diagnostic lost source span");
        found|=d.description.find(reason)!=std::string::npos;
    }
    if(!found) throw vd::CompilationError(result.diagnostics);
}
void diagnostics() {
    auto body=[](const std::string& value){return "func entry(x: i32) -> i32:\n"+value+"    return 0\n";};
    for(const auto& s:{
        "    const p = outer(nested = packet(owner = node()))\n    var copy = p\n    var n = copy.nested.owner as node\n    n.value++\n",
        "    const p = packet()\n    var target = packet()\n    target = p\n",
        "    const p = packet()\n    var target = outer()\n    target.nested = p\n",
        "    const p = node()\n    var target = packet()\n    target.owner = p\n",
        "    const p = packet(owner = node())\n    var copy: packet = p\n    copy.owner = node()\n    var n = copy.owner as node\n    n.value++\n"}) invalid(types+body(s),"readonly");
    invalid(types+"func change(p: packet):\n    p.count++\n\n"+body("    const p = packet()\n    change(p)\n"),"readonly");
    invalid(types+"struct holder:\n    payload: outer\n\n"+body("    const p = outer()\n    var h = holder(payload = p)\n"),"readonly");
    invalid(types+"struct holder:\n    payload: outer\n\n"+body("    var h = holder()\n    const p = packet()\n    h.payload.nested = p\n"),"readonly");
    invalid(types+"func (p packet) get() -> node?:\n    return p.owner\n\n"+body("    const p = packet(owner = node())\n    var n = p.get() as node\n    n.value++\n"),"readonly");
    invalid(types+"func (p packet) wrapped():\n    var q = packet(owner = p.owner)\n    var n = q.owner as node\n    n.value++\n\n"+body("    const p = packet(owner = node())\n    p.wrapped()\n"),"readonly");
    invalid(types+"func (p packet) init(const n: node):\n    var earlier = p\n    p.owner = n\n    var alias = earlier.owner as node\n    alias.value++\n\n","readonly");
    invalid(types+"func (p packet) init(const n: node):\n    p.owner = n\n\n"+body("    var p = packet(node())\n    var n = p.owner as node\n    n.value++\n"),"readonly");
    invalid(types+"func (p packet) init(const n: node? = null):\n    p.owner = n\n\nstruct holder:\n    payload: packet\n\n"+body("    var h = holder()\n"),"readonly");
    invalid(types+body("    var empty: node?\n    const view = empty\n    var alias = view\n    alias = node()\n    var n = alias as node\n    n.value++\n"),"readonly");
    const std::string required="struct node:\n    value: i32\n\nvalue struct required:\n    child: node\n\n";
    invalid(required+body("    var p = required()\n"),"non-nullable");
    invalid(required+body("    var p: required\n"),"non-nullable");
    invalid(required+"value struct wrap:\n    child: required\n\n"+body("    var p = wrap()\n"),"non-nullable");
    invalid(required+"func (p required) init(n: node):\n    p.child = n\n\n","valid before");
    invalid(required+"struct owner:\n    payload: required\n\nfunc (o owner) init(n: node):\n    o.payload = required(child = n)\n\n","non-nullable");
    invalid(types+"value struct impossible:\n    child: impossible\n","Recursive");
    invalid(types+body("    var p = packet(owner = node(), owner = node())\n"),"Duplicate");
    invalid(types+body("    var p = packet(owner = 1)\n"),"Type mismatch");
    invalid(types+"const p = packet(owner = node())\n","compile-time");
    const auto stage=types+"value struct boxed:\n    reference: owner?\n\nstruct owner:\n    child: node\n    payload: boxed\n\nfunc (o owner) init(n: node):\n    o.child = n\n";
    invalid(stage+"    var hidden = boxed(reference = o)\n","constructor argument");
    invalid(stage+"    o.payload.reference = o\n","stored or published");
    auto readonly_owner=nested_receiver;readonly_owner.replace(readonly_owner.find("var original"),3,"const");invalid(readonly_owner,"readonly");
    const auto boundary=types+"func plain(p: packet) -> i32:\n    return p.count\n\nfunc (o outer) inspect() -> i32:\n    var alias = o.nested\n    return plain(alias)\n\nfunc entry(x: i32) -> i32:\n    const original = outer(nested = packet(owner = node(), count = x))\n    var copy = original\n    return copy.inspect()\n";
    invalid(boundary,"mutable authority");
    auto result=vd::analyze_source({"/reference.void",boundary});bool located=false;
    for(const auto& d:result.diagnostics) located|=d.description.find("outer.nested.owner")!=std::string::npos && !d.related.empty();
    require(located,"indirect mutable parameter diagnostic lost reference path/declaration");
    invalid(types+"func plain(n: node) -> i32:\n    return n.value\n\nfunc (p packet) inspect() -> i32:\n    var alias = p.owner as node\n    return plain(alias)\n\n"+body("    const p = packet(owner = node())\n    return p.inspect()\n"),"mutable authority");
    invalid(types+"func (p packet) copied() -> node?:\n    var target: node?\n    target = p.owner\n    return target\n\n"+body("    const p = packet(owner = node())\n    var alias = p.copied() as node\n    return alias.value\n"),"mutable authority");
}
void programs(const std::filesystem::path& root,bool optimized) {
    for(auto [name,expected]:{std::pair{"copies",32}, {"readonly",36}, {"initialization",18}, {"templates",11}, {"methods",20}, {"staged",19}, {"order",44}, {"imports",12}, {"required",11}}) {
        auto p=vd::compile_project(root/"examples/reference_values"/(std::string(name)+".void"));
        vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
        auto code=jit.compile(p.module,p.module.entry_symbol,optimized,{},runtime);
        Probe probe;probe.inspect_roots=true;probe.collect_before=true;
        {Observe observe(probe);require(code->invoke(10)==expected,"reference value source execution");}
        require(probe.roots_valid && probe.frames.empty() && probe.commits_valid,"source argument/selected-owner roots");
        if(std::string(name)=="required") require(probe.constructions==257 && runtime->snapshot().completed_epoch>=256,"nested required payload publication/automatic collection coverage");
        if(std::string(name)=="order") require(probe.constructions==262 && runtime->snapshot().completed_epoch>=256,"selected heap owner did not survive post-replacement collection");
    }
    const auto mixed=types+"value struct mixed:\n    left: node?\n    right: node?\n\nfunc identity(const n: node) -> node:\n    return n\n\nfunc (p mixed) init(const n: node, m: node, choose: bool):\n    if choose:\n        p.left = identity(n)\n    else:\n        p.left = m\n    p.right = m\n\nfunc entry(x: i32) -> i32:\n    var n = node(value = 5)\n    var m = node(value = x)\n    var p = mixed(n, m, false)\n    var copy = p\n    var right = copy.right as node\n    right.value++\n    return right.value + (p.left as node).value\n";
    auto analysis=analyze(mixed);vd::ToolingIndex index(analysis);
    auto init=index.declaration("/reference.void::mixed::method::init");
    require(init && vd::ir::readonly_paths(init->receiver)==vd::ir::ReferencePaths{{0}},"fixed mixed initializer profile absent");
    vd::Jit jit;require(jit.compile(*analysis,optimized)->invoke(10)==22,"mixed fixed profile constrained independent leaf");
    auto bad=mixed;bad.replace(bad.find("    var right = copy.right"),0,"    var left = copy.left as node\n    left.value++\n");invalid(bad,"readonly");
    auto receiver=analyze(nested_receiver);require(jit.compile(*receiver,optimized)->invoke(10)==1,"mutable heap-owned inline receiver");
    auto forged=receiver->lower();
    for(auto& f:forged.functions) if(f.source_name=="entry") for(auto& local:f.locals)
        if(local.kind==vd::ir::Type::aggregate && local.definition->name=="carrier") local=vd::ir::readonly_view(local);
    bool rejected=false;try{vd::ir::validate(forged);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"IR missed heap owner mutation through reference-free inline receiver");
    const auto getter=types+"func plain(const p: packet) -> i32:\n    return p.count\n\nfunc (p packet) increment():\n    p.count++\n\nfunc (o outer) inspect() -> i32:\n    var alias = o.nested\n    return plain(alias)\n\nfunc entry(x: i32) -> i32:\n    const original = outer(nested = packet(owner = node(), count = x))\n    var copy = original\n    copy.nested.increment()\n    return copy.inspect()\n";
    auto g=analyze(getter);require(jit.compile(*g,optimized)->invoke(10)==11,"readonly receiver aliases/const helper rejected");
    auto invalid_ir=g->lower();bool changed=false;
    for(auto& f:invalid_ir.functions) if(f.source_name=="plain") {
        f.parameters[0]=vd::ir::with_readonly_paths(f.parameters[0],{});f.locals[0]=f.parameters[0];changed=true;
    }
    rejected=false;try{vd::ir::validate(invalid_ir);}catch(const std::invalid_argument&){rejected=true;}
    require(changed && rejected,"IR failed to propagate a mutable helper boundary through a readonly receiver");
    const auto self=types+"func (p packet) copied() -> node?:\n    var target = p.owner\n    target = p.owner\n    return target\n\nfunc entry(x: i32) -> i32:\n    const p = packet(owner = node(value = x))\n    var alias = p.copied() as node\n    return alias.value\n";
    require(jit.compile(*analyze(self),optimized)->invoke(10)==10,"readonly-dependent local capacity rejected identical-origin assignment");
    const auto neutral=types+"const absent: node? = null\nfunc entry(x: i32) -> i32:\n    var alias = absent\n    alias = node(value = x)\n    var selected = alias as node\n    selected.value++\n    return selected.value\n";
    require(jit.compile(*analyze(neutral),optimized)->invoke(10)==11,"pure nullable module template lost neutral authority");
}
void roots(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/reference_values/stress.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    for(bool forced:{false,true}) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;bool witnessed=false;
        p.before_construct=[&](auto count,const void*,auto){reference_value_test::inspect_carriers(p,witnessed,count);};
        {Observe observe(p);require(code->invoke(10)==2024,"reference value GC result");}
        require(witnessed && p.constructions==1002 && p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked>1000,"aggregate-only precise root proof");
        require(runtime->snapshot().completed_epoch>=1000,"aggregate automatic collection absent");
        vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"aggregate cycle retained after return");
    }
    Probe p;p.inspect_roots=true;p.fail_construction=3;p.fail_allocation=0;bool failed=false;
    {Observe observe(p);try{code->invoke(10);}catch(const std::runtime_error& e){failed=std::string(e.what()).find("allocation failed")!=std::string::npos;}}
    require(failed && p.commits_valid && p.roots_valid && p.frames.empty(),"aggregate allocation failure cleanup");
    {vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"failed aggregate roots retained");}
    require(code->invoke(10)==2024,"aggregate recovery after failure");
}
void parked(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/reference_values/stress.void");vd::Jit jit;
    auto code=jit.compile(project.module,project.module.entry_symbol,optimized);auto runtime=code->runtime_owner();
    Probe p;p.inspect_roots=true;p.gate_after_constructions=2;
    auto worker=std::async(std::launch::async,[&]{Observe observe(p);return code->invoke(10);});
    struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(5)),"aggregate loop/entry safepoint absent");
    auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);return runtime->collect(std::chrono::seconds(5));});
    bool requested=runtime->wait_for_request(std::chrono::seconds(5));release.now();
    require(collector.get()==vd::Runtime::Collection::completed && requested,"aggregate collection did not overlap execution");
    require(worker.get()==2024 && p.roots_valid && p.frames.empty() && runtime->snapshot().cooperative_parks>0,"aggregate worker did not park with precise roots");
}
void timeout(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/reference_values/stress.void");vd::Jit jit;
    auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    Probe p;p.inspect_roots=true;p.gate_after_constructions=2;
    auto worker=std::async(std::launch::async,[&]{Observe observe(p);return code->invoke(10);});
    struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(5)),"aggregate timeout overlap absent");
    {
        vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
        void* unreachable=nullptr;const std::size_t offset=0;
        const vd::Runtime::Slot slot{&unreachable,sizeof(unreachable),&offset,1};
        {vd::Runtime::Frame frame(*runtime,&slot,1);runtime->allocate_into(&unreachable,{sizeof(std::int32_t),{}});}
        auto before=runtime->snapshot();auto result=runtime->collect(std::chrono::milliseconds(20));auto after=runtime->snapshot();
        // completed_epoch acknowledges a settled rendezvous, including abort;
        // it is not a count of successful collections. Object count and the
        // returned outcome independently prove that this epoch did not sweep.
        require(result==vd::Runtime::Collection::timed_out && before.objects==3 && after.objects==before.objects && runtime->contains(unreachable) && after.epoch==before.epoch+1 && after.completed_epoch==after.epoch && !after.requested,"failed aggregate rendezvous swept or stayed requested");
    }
    release.now();require(worker.get()==2024 && p.roots_valid && p.frames.empty(),"aggregate worker failed to recover from timeout");
    vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);
    require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"aggregate collector failed to recover from timeout");
}
void retired_layout(const std::filesystem::path& root,bool optimized) {
    vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);Probe p;p.inspect_roots=true;p.capture=true;p.capture_at=2;
    {
        auto project=vd::compile_project(root/"examples/reference_values/stress.void");
        auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
        Observe observe(p);require(code->invoke(10)==2024,"retired-layout source result");
    }
    require(jit.live_generations()==0 && p.retained && p.frames.empty(),"reference value code lease leaked");
    vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();
    require(runtime->snapshot().objects==2,"nested heap descriptor depends on retired LLVM generation");
    std::int32_t value=0;std::memcpy(&value,static_cast<const char*>(p.retained->get())+offsetof(reference_value_test::NodeLayout,value),sizeof(value));
    require(value==11,"retired nested layout lost object identity/value");
    p.retained.reset();runtime->collect();require(runtime->snapshot().objects==0,"retired nested cyclic graph leaked");
}
void contracts() {
    const auto source=types+"func identity(const p: outer) -> outer:\n    return p\n\nfunc entry(x: i32) -> i32:\n    var p = outer(nested = packet(owner = node(value = x)))\n    return (identity(p).nested.owner as node).value\n";
    auto a=analyze(source);vd::ToolingIndex tools(a);
    auto definition=tools.definition_at("/reference.void",source.rfind(".owner")+1);
    require(definition && definition->identity=="/reference.void::packet::field::owner","nested reference field navigation");
    auto d=tools.declaration("/reference.void::identity");
    require(d && vd::ir::readonly_paths(d->type)==vd::ir::ReferencePaths{{0,0}},"validated aggregate return capability missing");
    auto changed=source;changed.replace(changed.find("const p: outer"),14,"p: outer");auto b=analyze(changed);
    require(vd::plan_invalidation(*a,{"/reference.void"},b.get()).source_compatibility==vd::SourceCompatibility::restart_required,"capability change permitted hot reload");
    vd::Jit jit;vd::ReloadSlot slot(jit.compile(*a));
    require(!jit.replace(slot,*b) && slot.invoke(10)==10,"runtime accepted a changed aggregate capability contract");
    auto body=source;body.replace(body.rfind(".value\n"),7,".value + 1\n");auto c=analyze(body);
    require(jit.replace(slot,*c) && slot.invoke(10)==11,"unrelated body replacement failed with reference values");
}
}
int main(int argc,char** argv) {
    try {
        const bool storage_proof = argc==6 && std::string(argv[5])=="--storage-proof";
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-library") {auto p=vd::compile_project(argv[2]);storage_fixture::emit(p.module,argv[3],std::string(argv[4])=="optimized",storage_proof);return 0;}
        require(argc==2,"reference value test root argument");std::filesystem::path root=argv[1];diagnostics();contracts();
        for(bool optimized:{false,true}) {programs(root,optimized);roots(root,optimized);parked(root,optimized);timeout(root,optimized);retired_layout(root,optimized);}
        std::cout<<"reference values: copies, capabilities, construction, sole aggregate roots, failure, tooling and reload passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
