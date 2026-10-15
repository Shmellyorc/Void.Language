#include "../storage_proof.hpp"
#include "../reference_values/proof.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <future>
#include <iostream>
using namespace managed_test;
namespace {
const std::string movement="value struct movement:\n    x: i32\n\nfunc (m movement) move(n: i32):\n    m.x += n\n\n";
const std::string component="struct node:\n    value: i32\n\nfunc (n node) bump():\n    n.value++\n\n";
const std::string wrapper=movement+"value struct wrap:\n    using movement\n\n";
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto a=vd::analyze_source({"/composition.void",text});
    if(!a.succeeded()) throw vd::CompilationError(a.diagnostics);return a.program;
}
void rejects(const std::string& text,const std::string& reason) {
    auto a=vd::analyze_source({"/composition.void",text});require(!a.succeeded(),"invalid composition accepted");bool found=false;
    for(const auto& d:a.diagnostics) {require(d.primary.source && d.primary.end>d.primary.begin,"composition diagnostic lost span");found|=d.description.find(reason)!=std::string::npos;}
    if(!found) throw vd::CompilationError(a.diagnostics);
}
void run(const std::string& text,int expected) {
    auto a=analyze(text);vd::Jit jit;
    for(bool opt:{false,true}) require(jit.compile(*a,opt)->invoke(10)==expected,"composition source result");
}
const std::string entry="func entry(v: i32) -> i32:\n";
void diagnostics() {
    rejects(movement+"struct bad:\n    using movement\n    using movement\n","Duplicate field");
    rejects(movement+"struct bad:\n    using movement\n    movement: movement\n","Duplicate field");
    rejects("struct bad:\n    using n: i32\n","non-nullable nominal");
    auto nullable=vd::analyze_source({"/composition.void",component+entry+"    var n: node?\n    n.value = 1\n    return 0\n"});
    require(!nullable.succeeded() && nullable.diagnostics.front().code==vd::DiagnosticCode::type_mismatch,"nullable field assignment changed diagnostic code");
    rejects(component+"struct bad:\n    using child: node?\n","non-nullable nominal");
    rejects("value struct bad:\n    using bad\n","Recursive value layout");
    const auto two=movement+"value struct two:\n    using left: movement\n    using right: movement\n\n";
    analyze(two); // Overlapping promoted names do not invalidate declarations.
    for(const auto& use:{"    return p.x\n","    p.x = 1\n    return 0\n","    p.move(1)\n    return 0\n"})
        rejects(two+entry+"    var p = two()\n"+use,"Ambiguous promoted member");
    const auto mixed="value struct ints:\n    x: i32\n\nvalue struct floats:\n    x: f32\n\nvalue struct both:\n    using left: ints\n    using right: floats\n\n";
    rejects(mixed+entry+"    var p = both()\n    var n: i32 = p.x\n    return n\n","Ambiguous promoted member");
    const auto overloads="value struct a:\n    x: i32\n\nvalue struct b:\n    x: i32\n\nfunc (p a) f(n: i32) -> i32:\n    return n\n\nfunc (p b) f(n: bool) -> bool:\n    return n\n\nvalue struct both:\n    using a\n    using b\n\n";
    rejects(overloads+entry+"    var p = both()\n    var n: i32 = p.f(1)\n    return n\n","Ambiguous promoted member");
    rejects(wrapper+entry+"    var p = wrap()\n    var m: movement = p\n    return 0\n","Type mismatch");
    rejects(wrapper+entry+"    var p = wrap(x = 1)\n    return 0\n","constructor field");
    rejects(wrapper+entry+"    var p = wrap()\n    p.init()\n    return 0\n","Lifecycle");
    rejects(wrapper+"value struct outer:\n    using wrap\n\n"+entry+"    var p = outer()\n    return p.x\n","Unknown field");
    rejects(wrapper+entry+"    const p = wrap()\n    p.move(1)\n    return 0\n","readonly");
    const auto refs=component+"value struct packet:\n    using node\n    count: i32\n\n";
    rejects(refs+entry+"    const p = packet(node = node())\n    var q = p\n    q.bump()\n    return q.count\n","readonly");
    rejects(refs+entry+"    const p = packet(node = node())\n    var target = packet(node = node())\n    target = p\n    return 0\n","readonly");
    rejects(refs+"func (n node) identity() -> node:\n    return n\n\n"+entry+"    const p = packet(node = node())\n    var alias = p.identity()\n    alias.bump()\n    return 0\n","readonly");
    rejects(component+"struct owner:\n    using child: node\n\n"+entry+"    const p = owner(child = node())\n    var alias = p\n    alias.value++\n    return 0\n","readonly");
    rejects(refs+"func (p packet) init(n: node):\n    p.node = n\n","valid before");
    rejects(component+"struct owner:\n    using child: node\n\nfunc (o owner) init():\n    o.value = 2\n","initialized before");
    rejects(component+"struct owner:\n    using child: node\n\nfunc (o owner) init():\n    o.bump()\n","initialized before");
    rejects(component+"struct owner:\n    using child: node\n\nfunc (o owner) init(n: node):\n    o.child = n\n    o.child = o\n","Type mismatch");
    rejects(wrapper+entry+"    var p = wrap()\n    p.movement.move(true)\n    return 0\n","Type mismatch");
}
void programs() {
    run(wrapper+entry+"    const p = wrap(movement = movement(x = 3))\n    var copy = p\n    copy.move(2)\n    return copy.x + p.x\n",8);
    run(component+"value struct counted:\n    using node\n    count: i32\n\nfunc (p counted) inc():\n    p.count++\n\n"+entry+"    var n = node(value = v)\n    const original = counted(node = n)\n    var copy = original\n    copy.inc()\n    n.bump()\n    return copy.count + copy.value\n",12);
    run(wrapper+entry+"    var p = wrap()\n    p.move(v)\n    p.x += 2\n    var q = p\n    q.move(1)\n    return p.x + q.x\n",25);
    run(wrapper+"value struct outer:\n    using wrap\n\n"+entry+"    var p = outer()\n    p.wrap.move(v)\n    return p.wrap.x\n",10);
    run(movement+"value struct direct:\n    using movement\n    x: bool\n\nfunc (p direct) move(n: i32):\n    p.x = true\n\n"+entry+"    var p = direct()\n    p.move(1)\n    if p.x:\n        return p.movement.x + 3\n    return 0\n",3);
    rejects(movement+"value struct direct:\n    using movement\n    move: i32\n\n"+entry+"    var p = direct()\n    p.move(1)\n    return 0\n","not callable");
    run(component+"struct owner:\n    using child: node\n\nfunc (o owner) init(n: node):\n    o.child = n\n    o.bump()\n\n"+entry+"    var n = node(value = v)\n    var o = owner(n)\n    return o.value\n",11);
    run(movement+"value struct holder:\n    using: movement\n\n"+entry+"    var h = holder()\n    h.using.move(3)\n    return h.using.x\n",3);
    run("value struct wide:\n    x: i64\n\nvalue struct wrap:\n    using wide\n\n"+entry+"    var p = wrap(wide = wide(x = 2147483648))\n    return (2147483649 - p.x) as i32\n",1);
}
void tooling() {
    const auto text=wrapper+entry+"    var p = wrap()\n    p.x = v\n    p.move(1)\n    return p.x\n";
    auto a=analyze(text);vd::ToolingIndex tools(a);
    for(auto offset:{text.find("p.x =")+2,text.rfind("p.x")+2}) {
        auto d=tools.definition_at("/composition.void",offset);
        require(d && d->identity=="/composition.void::movement::field::x","promoted field navigation");
    }
    auto method=tools.definition_at("/composition.void",text.rfind("p.move")+2);
    require(method && method->identity=="/composition.void::movement::method::move","promoted method navigation");
    auto field=tools.declaration("/composition.void::wrap::field::movement");require(field && field->using_field,"using metadata absent");
    auto changed=text;changed.replace(changed.find("using movement"),14,"movement: movement");
    // Keep source valid after removing promotion while comparing storage-equivalent declarations.
    for(const auto& [from,to]:{std::pair{"p.x","p.movement.x"},std::pair{"p.move(","p.movement.move("}}) {
        std::size_t pos=0;while((pos=changed.find(from,pos))!=std::string::npos){changed.replace(pos,std::string(from).size(),to);pos+=std::string(to).size();}
    }
    auto b=analyze(changed);
    require(vd::plan_invalidation(*a,{"/composition.void"},b.get()).source_compatibility==vd::SourceCompatibility::restart_required,"composition marker permitted reload");
    vd::Jit jit;vd::ReloadSlot slot(jit.compile(*a));require(!jit.replace(slot,*b) && slot.invoke(10)==11,"composition reload safety");
    auto body=text;body.replace(body.rfind("return p.x"),10,"return p.x + 1");auto c=analyze(body);
    require(jit.replace(slot,*c) && slot.invoke(10)==12,"compatible composition body reload");
}
void imports(const std::filesystem::path& root) {
    auto path=root/"examples/composition/imports.void";
    const std::string types="value struct movement:\n    x: i32\n\nfunc (m movement) move(n: i32):\n    m.x += n\n\nprivate func (m movement) hidden():\n    m.x++\n";
    const std::string wrap="use \"/lib/movement.void\" as t\nvalue struct wrapper:\n    using t.movement\n";
    const std::string main="use \"/lib/wrapper.void\" as w\nfunc entry(x: i32) -> i32:\n    var p = w.wrapper()\n    p.move(1)\n    return p.x\n";
    auto check=[&](const std::string& source) {
        vd::SourceOverrides overrides{{"/imports.void",vd::SourceFile(path.string(),source)},
            {"/lib/movement.void",vd::SourceFile("/lib/movement.void",types)},
            {"/lib/wrapper.void",vd::SourceFile("/lib/wrapper.void",wrap)}};
        return vd::analyze_project(path,overrides);
    };
    auto a=check(main);require(!a.succeeded(),"wrapper transitively exported embedded method");
    bool visibility=false;for(const auto& d:a.diagnostics) visibility|=d.code==vd::DiagnosticCode::visibility;
    require(visibility,"missing owner import lost visibility diagnostic");
    auto imported="use \"/lib/movement.void\" as t\n"+main;
    require(check(imported).succeeded(),"explicit owner import rejected");
    imported.replace(imported.find("p.move(1)"),9,"p.hidden()");
    require(!check(imported).succeeded(),"private method promoted publicly");
}
void selected_and_ir(const std::filesystem::path& root,bool opt) {
    for(auto [name,expected]:{std::pair{"main",111}, {"order",44}, {"staged",19}, {"imports",12}, {"receiver_order",103}}) {
        auto project=vd::compile_project(root/"examples/composition"/(std::string(name)+".void"));
        vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
        auto code=jit.compile(project.module,project.module.entry_symbol,opt,{},runtime);
        Probe p;p.inspect_roots=true;p.collect_before=true;
        {Observe observe(p);require(code->invoke(10)==expected,"promoted selected owner/default/source-order result");}
        require(p.roots_valid && p.frames.empty() && p.commits_valid,"promoted selected owner roots/cleanup");
        if(std::string(name)=="order") require(p.constructions==262 && runtime->snapshot().completed_epoch>=256,"selected owner collection not exercised");
    }
    auto a=analyze(wrapper+entry+"    var p = wrap()\n    p.move(1)\n    return p.x\n");
    auto bad=a->lower();bool changed=false;
    for(auto& f:bad.functions) for(auto& block:f.blocks) for(auto& i:block.instructions)
        if(i.op==vd::ir::Op::call && i.receiver && !i.receiver->fields.empty()) {i.receiver->fields[0]=99;changed=true;}
    bool rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){rejected=true;}
    require(changed && rejected,"independent IR accepted forged promoted receiver path");
    bad=a->lower();
    for(auto& f:bad.functions) if(f.receiver && f.source_name=="movement::method::move") f.receiver_mutates=false;
    rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"independent IR trusted forged receiver effect");
    const auto managed=component+movement+"struct object:\n    using motion: movement\n\nvalue struct carrier:\n    using reference: object\n\nfunc (c carrier) mutate():\n    c.reference.move(1)\n\n"+entry+"    var c = carrier(reference = object())\n    c.mutate()\n    return c.reference.x\n";
    auto valid=analyze(managed);bad=valid->lower();
    for(auto& f:bad.functions) if(f.source_name=="entry") for(auto& local:f.locals)
        if(local.kind==vd::ir::Type::aggregate && local.definition->name=="carrier") local=vd::ir::readonly_view(local);
    rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"independent IR accepted readonly owner mutation through promoted inline storage");
    const auto staged=component+"struct owner:\n    using child: node\n\nfunc (o owner) init(n: node):\n    o.child = n\n    o.bump()\n\n"+entry+"    return owner(node()).value\n";
    valid=analyze(staged);bad=valid->lower();changed=false;
    for(auto& f:bad.functions) for(auto& block:f.blocks) for(auto& i:block.instructions)
        if(i.initialization) {i.initialization=false;changed=true;}
    rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){rejected=true;}
    require(changed && rejected,"independent IR accepted unmarked initializer through composition");
}
void roots(const std::filesystem::path& root,bool opt) {
    auto project=vd::compile_project(root/"examples/composition/stress.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(project.module,project.module.entry_symbol,opt,{},runtime);
    for(bool forced:{false,true}) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;bool witnessed=false;
        p.before_construct=[&](auto n,const void*,auto){reference_value_test::inspect_carriers(p,witnessed,n);};
        {Observe observe(p);require(code->invoke(10)==2024,"composition GC result");}
        require(witnessed && p.constructions==1002 && p.roots_valid && p.commits_valid && p.frames.empty(),"composition sole aggregate roots");
        vd::Runtime::Attachment attach(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"composition cycle leak");
    }
    Probe p;p.inspect_roots=true;p.fail_construction=3;p.fail_allocation=0;
    bool failed=false;{Observe observe(p);try{code->invoke(10);}catch(const std::exception&){failed=true;}}
    require(failed && p.frames.empty() && p.roots_valid,"composition failure cleanup");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"composition failure leak");}
    require(code->invoke(10)==2024,"composition failure recovery");
}
void parked(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/composition/stress.void");vd::Jit jit;
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
    auto project=vd::compile_project(root/"examples/composition/stress.void");vd::Jit jit;
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
}
int main(int argc,char** argv) {
    try {
        const bool storage_proof = argc==6 && std::string(argv[5])=="--storage-proof";
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-library") {auto p=vd::compile_project(argv[2]);storage_fixture::emit(p.module,argv[3],std::string(argv[4])=="optimized",storage_proof);return 0;}
        require(argc==2,"composition project root required");diagnostics();programs();tooling();imports(argv[1]);
        for(bool opt:{false,true}) {
            auto imports=vd::compile_project(std::filesystem::path(argv[1])/"examples/composition/imports.void");
            require(!imports.module.coordinated,"CRT-link regression must remain runtime-free and not pull a managed runtime archive member");
            vd::Jit jit;require(jit.compile(imports.module,imports.module.entry_symbol,opt)->invoke(10)==12,"imported promoted call");
            selected_and_ir(argv[1],opt);roots(argv[1],opt);parked(argv[1],opt);timeout(argv[1],opt);}
        std::cout<<"composition: lookup, storage, readonly, readiness, GC, tooling and reload passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
