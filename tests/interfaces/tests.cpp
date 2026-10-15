#include "../storage_proof.hpp"
#include "proof.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <cstring>
#include <algorithm>
#include <future>
#include <iostream>
using namespace managed_test;
namespace {
const std::string declaration="interface readable:\n    func read() -> i32\nstruct cell:\n    n: i32\nfunc (c cell) read() -> i32:\n    return c.n\n";
const std::string entry="func entry(v: i32) -> i32:\n";
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto result=vd::analyze_source({"/interfaces.void",text});if(!result.succeeded()) throw vd::CompilationError(result.diagnostics);return result.program;
}
void rejects(const std::string& text,const std::string& fragment) {
    auto result=vd::analyze_source({"/interfaces.void",text});require(!result.succeeded(),"invalid interface source accepted");bool found=false;
    for(const auto& d:result.diagnostics) {require(d.primary.source && d.primary.end>d.primary.begin,"interface diagnostic lost source span");found|=d.description.find(fragment)!=std::string::npos;}
    if(!found) throw vd::CompilationError(result.diagnostics);
}
void run(const std::string& text,int expected) {
    auto program=analyze(text);vd::Jit jit;
    for(bool opt:{false,true}) require(jit.compile(*program,opt)->invoke(10)==expected,"interface source execution");
}
void diagnostics() {
    rejects("interface bad:\n    func init()\n","Lifecycle");
    rejects("interface bad:\n    func read(n: i32 = 1)\n","default");
    rejects("interface bad:\n    func read() -> i32:\n        return 1\n","Expected");
    rejects("interface bad:\n    func read()\n    func read()\n","Duplicate");
    rejects(declaration+entry+"    var x = readable()\n    return 0\n","cannot be constructed");
    rejects(declaration+entry+"    var x: readable\n    return 0\n","requires an initializer");
    rejects(declaration+"struct empty:\n\n"+entry+"    var x: readable = empty()\n    return 0\n","Missing requirement");
    rejects("interface readable:\n    func read() -> i32\nstruct cell:\n    read: i32\n"+entry+"    var x: readable = cell()\n    return 0\n","field cannot satisfy");
    rejects("interface readable:\n    func read() -> i32\nstruct cell:\n    n: i32\nfunc (c cell) read() -> f32:\n    return 1f\n"+entry+"    var x: readable = cell()\n    return 0\n","signature mismatch");
    rejects("interface readable:\n    func read(const n: i32) -> i32\nstruct cell:\n    n: i32\nfunc (c cell) read(n: i32) -> i32:\n    return n\n"+entry+"    var x: readable = cell()\n    return 0\n","signature mismatch");
    rejects(declaration+"struct owner:\n    using left: cell\n    using right: cell\n"+entry+"    var x: readable = owner(left = cell(), right = cell())\n    return 0\n","Ambiguous");
    rejects("interface readable:\n    func read() -> i32\nvalue struct cell:\n    n: i32\nfunc (c cell) read() -> i32:\n    return c.n\n"+entry+"    var x: readable = cell()\n    return 0\n","standalone value erasure");
    rejects(declaration+"interface other:\n    func read() -> i32\n"+entry+"    var x: readable = cell()\n    var y: other = x\n    return 0\n","Type mismatch");
    rejects(declaration+entry+"    var x: readable = cell()\n    return x.n\n","Field access requires");
    rejects(declaration+entry+"    var x: readable?\n    return x.read()\n","Unwrap");
    rejects(declaration+entry+"    var x: cell?\n    var y: readable = x\n    return 0\n","Unwrap");
    rejects("interface changing:\n    func bump()\nstruct cell:\n    n: i32\nfunc (c cell) bump():\n    c.n++\n"+entry+"    const c = cell()\n    var x: changing = c\n    x.bump()\n    return 0\n","readonly");
    rejects(declaration+"func consume(r: readable) -> i32:\n    return r.read()\n"+entry+"    const c = cell()\n    var x: readable = c\n    return consume(x)\n","readonly");
    rejects(declaration+"struct holder:\n    view: readable\nfunc (h holder) init():\n    return\n"+entry+"    return 0\n","initialized field");
    rejects(declaration+"value struct holder:\n    view: readable\nfunc (h holder) init():\n    return\n"+entry+"    return 0\n","every field to be valid");
    rejects(declaration+"func (c cell) init():\n    var x: readable = c\n"+entry+"    return 0\n","Construction views cannot be cast");
    rejects(declaration+"struct noisy:\n    n: i32\nfunc (n noisy) read() -> i32:\n    n.n++\n    return n.n\n"+entry+"    const c = cell()\n    var r: readable = c\n    return r.read()\n","readonly");
    rejects(declaration+"struct holder:\n    view: readable\n"+entry+"    const c = cell()\n    var h = holder(view = c)\n    return 0\n","readonly");
    rejects(declaration+"interface owner_view:\n    func child() -> cell\nstruct owner:\n    c: cell\nfunc (o owner) child() -> cell:\n    return o.c\n"+entry+"    const o = owner(c = cell())\n    var r: owner_view = o\n    var c = r.child()\n    c.n++\n    return 0\n","readonly");

    std::string bounded;
    for(unsigned n=0;n<260;++n) bounded+="struct node"+std::to_string(n)+":\n    n: i32\n";
    for(unsigned n=0;n<257;++n) bounded+="interface view"+std::to_string(n)+":\n    func missing() -> i32\n";
    rejects(bounded,"65536 scope/view/interface");
    bounded.clear();
    for(unsigned n=0;n<64;++n) {
        auto name="node"+std::to_string(n);bounded+="struct "+name+":\n    n: i32\n";
        for(unsigned method=0;method<32;++method) bounded+="func (n "+name+") method"+std::to_string(method)+"() -> i32:\n    return n.n\n";
    }
    for(unsigned n=0;n<64;++n) bounded+="interface view"+std::to_string(n)+":\n    func missing() -> i32\n";
    rejects(bounded,"8388608 visits");

}
void programs() {
    run(declaration+"func forward(const r: readable) -> readable:\n    return r\n"+entry+"    const c = cell(n = v)\n    var x = forward(c)\n    return x.read()\n",10);
    run(declaration+entry+"    var c = cell(n = v)\n    var x: readable = c\n    var y = c as readable\n    if x == y:\n        return y.read()\n    return 0\n",10);
    run("interface readable:\n    func read() -> i32\nvalue struct meter:\n    n: i32 = 7\nfunc (m meter) read() -> i32:\n    return m.n\nstruct base:\n    using meter\nstruct derived extends base:\n    n: i32 = 20\nfunc (d derived) read() -> i32:\n    return d.n\n"+entry+"    var d = derived()\n    var b: base = d\n    var x: readable = b\n    var y: readable = d\n    if x == y:\n        return x.read() * 100 + y.read()\n    return 0\n",720);
    run("interface readable:\n    func read() -> i32\nstruct base:\n    n: i32\nstruct derived extends base:\n    extra: i32\nvirtual func (b base) read() -> i32:\n    return b.n\noverride func (d derived) read() -> i32:\n    return super.read() + d.extra\n"+entry+"    var d = derived(n = v, extra = 4)\n    var b: base = d\n    var x: readable = b\n    return x.read()\n",14);
    run(declaration+"struct holder:\n    view: readable\nfunc (h holder) init(c: cell):\n    h.view = c\n"+entry+"    return holder(cell(n = v)).view.read()\n",10);
    run(declaration+"interface factory:\n    func child() -> cell\nstruct owner:\n    n: i32\nfunc (o owner) child() -> cell:\n    return cell(n = 4)\n"+entry+"    const o = owner()\n    var r: factory = o\n    var c = r.child()\n    c.n++\n    return c.n\n",5);
    run(declaration+"interface updater:\n    func update(other: cell)\nstruct worker:\n    n: i32\nfunc (w worker) update(other: cell):\n    other.n++\n"+entry+"    const w = worker()\n    const view: updater = w\n    var other = cell(n = v)\n    view.update(other)\n    return other.n\n",11);
    run(declaration+"const nothing: readable? = null as readable?\n"+entry+"    var r = nothing\n    r = cell(n = v)\n    return (r as readable).read()\n",10);
    run("interface actions:\n    func read() -> i32\n    func move(amount: i32)\nstruct cell:\n    n: i32\nfunc (c cell) read() -> i32:\n    return c.n\nfunc (c cell) move(amount: i32):\n    c.n += amount\n"+entry+"    var c = cell(n = v)\n    const r: actions = c\n    c.move(1)\n    return r.read()\n",11);
    run("interface left:\n    func right() -> right?\ninterface right:\n    func left() -> left?\nstruct cell:\n\nfunc (c cell) right() -> right?:\n    return null\nfunc (c cell) left() -> left?:\n    return null\n"+entry+"    var l: left = cell()\n    if l.right() == null:\n        return v\n    return 0\n",10);
    run(declaration+"const nil_cell: cell? = null\nconst nil_view: readable? = nil_cell\n"+entry+"    var x = nil_view\n    x = cell(n = v)\n    return (x as readable).read()\n",10);
    run("interface sampleable:\n    func sample(flag: bool) -> i32\nstruct child:\n    n: i32\nfunc (c child) sample(flag: bool) -> i32:\n    if flag:\n        c.n++\n    return c.n\nstruct owner:\n    using child\nfunc create() -> owner:\n    return owner(child = child(n = 10))\nfunc (o owner) change() -> bool:\n    o.child = child(n = 90)\n    return true\n"+entry+"    var o = create()\n    var r: sampleable = o\n    return r.sample(o.change() and v > 0) * 100 + o.child.n\n",1190);
    auto null=analyze(declaration+entry+"    var x: readable?\n    return (x as readable).read()\n");vd::Jit jit;
    for(bool opt:{false,true}) {bool failed=false;try{jit.compile(*null,opt)->invoke(10);}catch(const std::exception& e){failed=std::string(e.what()).find("Null interface reference")!=std::string::npos;}require(failed,"null interface unwrap must report a checked failure");}
}
void tooling_and_imports(const std::filesystem::path& root) {
    auto project=vd::compile_project(root/"examples/interfaces/imports.void");vd::Jit jit;
    for(bool opt:{false,true}) {
        auto cycle=vd::compile_project(root/"examples/interfaces/cycles.void");auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(cycle.module,cycle.module.entry_symbol,opt,{},runtime);
        for(int n=0;n<64;++n) require(code->invoke(n)==n,"nested interface managed cycle/value-copy identity");
        vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"interface managed cycle retained witness or target");
    }
    for(bool opt:{false,true}) require(jit.compile(project.module,project.module.entry_symbol,opt)->invoke(10)==10,"private factory interface exposure failed");
    const auto source=declaration+entry+"    var x: readable = cell(n = v)\n    return x.read()\n";
    auto program=analyze(source);vd::ToolingIndex index(program);
    auto requirement=index.definition_at("/interfaces.void",source.rfind("read()"));
    require(requirement && requirement->identity=="/interfaces.void::readable::method::read" && requirement->kind==vd::AnalyzedDeclaration::Kind::interface_requirement,"interface call navigation lost requirement ownership");
    auto annotation=index.definition_at("/interfaces.void",source.find("readable ="));
    require(annotation && annotation->kind==vd::AnalyzedDeclaration::Kind::interface_type,"interface annotation navigation category");
    const auto changed=source+"struct unrelated:\n    n: i32\nfunc (u unrelated) read() -> i32:\n    u.n++\n    return u.n\n";
    auto after=analyze(changed);auto plan=vd::plan_invalidation(*program,{},after.get());
    require(plan.source_compatibility==vd::SourceCompatibility::restart_required,"interface provider/effect universe change allowed hot reload");
    const auto body=source;auto replaced=body;auto position=replaced.find("return c.n");replaced.replace(position,10,"return c.n + 1");
    auto next=analyze(replaced);vd::ReloadSlot slot(jit.compile(*program,false));require(jit.replace(slot,*next,false) && slot.invoke(10)==11,"interface body-only reload failed");
    const auto construction=declaration+"struct holder:\n    n: i32\nfunc (h holder) init(const r: readable):\n    h.n = r.read()\n"+entry+"    return holder(cell(n = v)).n\n";
    auto before=analyze(construction);auto changed_construction=construction;changed_construction.replace(changed_construction.find("return c.n"),10,"return c.n + 1");auto later=analyze(changed_construction);
    require(vd::plan_invalidation(*before,{},later.get()).source_compatibility==vd::SourceCompatibility::restart_required,"interface initializer provider body dependency allowed reload");

}
void independent_ir() {
    auto valid=analyze(declaration+entry+"    var c = cell(n = v)\n    var x: readable = c\n    return x.read()\n")->lower();
    auto reject=[&](vd::ir::Module bad) {bool rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"independent interface IR proof accepted corruption");};
    auto bad=valid;bad.interface_bindings.clear();reject(bad);
    bad=valid;std::erase_if(bad.nominal_types,[](auto t){return t.kind==vd::ir::Type::managed_ref;});reject(bad);
    bad=valid;bad.interface_bindings[0].methods.begin()->second.first.push_back(0);bad.interface_bindings[0].key=vd::ir::binding_key(bad.interface_bindings[0]);reject(bad);
    bad=valid;bad.interface_requirements[0].blocks.push_back({});reject(bad);
    bad=valid;for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::store && f.locals[i.a].kind==vd::ir::Type::prepared_receiver) i.op=vd::ir::Op::gc_poll;reject(bad);
    bad=valid;for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::interface_pack) {i.op=vd::ir::Op::constant;i.immediate=0;}reject(bad);
    bad=valid;for(auto& f:bad.functions) if(!f.receiver) for(auto& t:f.locals) if(t.kind==vd::ir::Type::managed_ref) t.readonly=true;reject(bad);
    bad=valid;bad.interface_requirements[0].method_name="init";reject(bad);
    bad=valid;for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::interface_pack) i.callee="forged";reject(bad);
    bad=valid;for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::interface_prepare) i.type.preparation="other";reject(bad);
    bad=valid;for(auto& f:bad.functions) for(auto& b:f.blocks) for(auto& i:b.instructions) if(i.op==vd::ir::Op::interface_call) i.op=vd::ir::Op::call;reject(bad);
}
void handoffs(const std::filesystem::path& root,bool opt) {
    auto project=vd::compile_project(root/"examples/interfaces/handoffs.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(project.module,project.module.entry_symbol,opt,{},runtime);
    Probe p;p.inspect_roots=true;p.collect_before=true;bool witnessed=false;
    void* target=nullptr;p.before_construct=[&](auto count,const void*,auto){interface_test::handoff(p,witnessed,target,count);};
    {Observe observe(p);require(code->invoke(10)==110,"nested interface value result");}
    require(witnessed && p.constructions==101 && p.roots_valid && p.frames.empty(),"nested interface precise owner word offset/cleanup");
    vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"nested interface value leak");
}
void admission() {
    const vd::NominalDescriptor owner{"owner","","owner-layout",{sizeof(void*),{}}};
    const vd::NominalDescriptor extra{"extra","","extra-layout",{sizeof(void*),{}}};
    const vd::InterfaceWitness witness{"interface:1:I5:owner","I","owner","contract0"};
    const vd::InterfaceWitness second{"interface:1:J5:owner","J","owner","contract1"};
    vd::Runtime runtime;runtime.admit_metadata({&owner,1},{&witness,1});
    auto wrong=witness;wrong.contract="different";bool rejected=false;try{runtime.admit_metadata({&extra,1},{&wrong,1});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"conflicting witness admission accepted");
    rejected=false;try{runtime.descriptor("extra");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"failed witness admission published a type");
    wrong=witness;wrong.view_identity="extra";rejected=false;try{runtime.admit_metadata({&extra,1},{&wrong,1});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"forged witness key/domain admitted");
    for(int budget=0;budget<24;++budget) {
        vd::Runtime attempt;attempt.admit_metadata({&owner,1},{&witness,1});bool failed=false;
        {AllocationFailure allocation(budget);try{attempt.admit_metadata({&extra,1},{&second,1});}catch(const std::bad_alloc&){failed=true;}}
        bool present=true;try{attempt.descriptor("extra");}catch(const std::invalid_argument&){present=false;}
        vd::Runtime::Attachment a(attempt);vd::Runtime::Managed m(attempt);require(present==!failed && bool(attempt.witness(second.key))==!failed,"metadata allocation failure published a partial transaction");
    }
    vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);void* object=nullptr;const std::size_t offset=0;const vd::Runtime::Slot slot{&object,sizeof(object),&offset,1};vd::Runtime::Frame frame(runtime,&slot,1);
    auto layout=owner.layout;layout.descriptor=runtime.descriptor(owner.identity);runtime.allocate_into(&object,layout);
    auto* stable=runtime.witness(witness.key);const char* keys[]={witness.key.c_str()};
    require(stable && runtime.interface_binding(object,stable,keys)==0,"admitted witness domain selection failed");
    require(runtime.interface_binding(nullptr,stable,keys)==-1 && runtime.interface_binding(object,nullptr,keys)==-1 && runtime.interface_binding(object,reinterpret_cast<const void*>(1),keys)==-1,"invalid interface pair or opaque witness accepted");
    const char* unknown[]={"unknown"};require(runtime.interface_binding(object,stable,unknown)==-1,"generation guessed unknown witness key");
    rejected=false;try{runtime.admit_metadata({},{});}catch(const std::logic_error&){rejected=true;}require(rejected,"Managed metadata admission allowed");
}
void selected(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/interfaces/selection.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    Probe p;p.inspect_roots=true;p.collect_before=true;bool witnessed=false;void* target=nullptr;
    p.before_construct=[&](auto count,const void* payload,auto){interface_test::selected(p,witnessed,target,count,payload);};
    {Observe observe(p);require(code->invoke(10)==1200,"selected interface receiver changed after argument mutation");}
    require(witnessed && p.constructions==4 && p.roots_valid && p.frames.empty(),"selected receiver precise root offsets/cleanup");
    vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"selected receiver leaked graph");
}
void retirement(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/interfaces/stress.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    Probe p;p.capture=true;p.capture_at=2;
    {Observe observe(p);require(code->invoke(10)==1510,"old interface holder setup");}
    require(p.retained && jit.live_generations()==1,"old interface capture absent");
    auto binding=std::find_if(project.module.interface_bindings.begin(),project.module.interface_bindings.end(),[](const auto& b){return b.interface.definition->name=="readable" && b.view.definition->name=="cell";});require(binding!=project.module.interface_bindings.end(),"retirement binding absent");const auto key=binding->key;const void* witness=nullptr;void* object=nullptr;
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);std::memcpy(&object,p.retained->get(),sizeof(object));witness=runtime->witness(key);require(witness,"old witness absent");}
    code.reset();require(jit.live_generations()==0,"interface data pinned allocating code generation");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();const char* keys[]={key.c_str()};require(runtime->interface_binding(object,witness,keys)==0 && runtime->snapshot().objects==2,"retired code invalidated reachable interface binding data");}
    auto next=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);require(runtime->witness(key)==witness,"compatible generation replaced stable witness identity");}
    require(next->invoke(10)==1510,"new interface generation result");p.retained.reset();
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"interface metadata retained managed objects");}
}
void parked(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/interfaces/handoffs.void");vd::Jit jit;auto code=jit.compile(project.module,project.module.entry_symbol,optimized);auto runtime=code->runtime_owner();
    Probe p;p.inspect_roots=true;p.gate_after_constructions=1;
    auto worker=std::async(std::launch::async,[&]{Observe observe(p);return code->invoke(10);});
    struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(5)),"interface handoff safepoint absent");
    auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);return runtime->collect(std::chrono::seconds(5));});
    bool requested=runtime->wait_for_request(std::chrono::seconds(5));release.now();
    require(collector.get()==vd::Runtime::Collection::completed && requested,"interface collection did not overlap execution");
    require(worker.get()==110 && p.roots_valid && p.frames.empty() && runtime->snapshot().cooperative_parks>0,"interface worker did not park with precise roots");
}
void active_reload(bool optimized) {
    const auto text=declaration+entry+"    var r: readable = cell(n = v)\n    return r.read()\n";
    auto before=analyze(text);auto changed=text;changed.replace(changed.find("return c.n"),10,"return c.n + 1");auto after=analyze(changed);
    vd::Jit jit;vd::ReloadSlot slot(jit.compile(*before,optimized));Probe p;p.inspect_roots=true;p.gate_after_constructions=1;
    auto worker=std::async(std::launch::async,[&]{Observe observe(p);return slot.invoke(10);});
    struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(5)),"active interface generation overlap absent");
    require(jit.replace(slot,*after,optimized) && jit.live_generations()==2 && slot.invoke(10)==11,"new interface generation publication/lease");
    release.now();require(worker.get()==10 && p.roots_valid && p.frames.empty() && jit.live_generations()==1,"old interface call used new body or outlived lease");
}
void timeout(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/interfaces/handoffs.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    Probe p;p.inspect_roots=true;p.gate_after_constructions=1;
    auto worker=std::async(std::launch::async,[&]{Observe observe(p);return code->invoke(10);});
    struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{p};
    require(p.reached.try_acquire_for(std::chrono::seconds(5)),"interface timeout overlap absent");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);void* unreachable=nullptr;const std::size_t offset=0;const vd::Runtime::Slot slot{&unreachable,sizeof(unreachable),&offset,1};
        {vd::Runtime::Frame frame(*runtime,&slot,1);runtime->allocate_into(&unreachable,{sizeof(void*),{}});}
        auto before=runtime->snapshot();auto result=runtime->collect(std::chrono::milliseconds(20));auto after=runtime->snapshot();
        require(result==vd::Runtime::Collection::timed_out && before.objects==2 && after.objects==before.objects && runtime->contains(unreachable) && after.completed_epoch==after.epoch && !after.requested,"failed interface rendezvous swept or stayed requested");}
    release.now();require(worker.get()==110 && p.roots_valid && p.frames.empty(),"interface worker failed timeout recovery");
    vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"interface collector failed timeout recovery");
}
void roots(const std::filesystem::path& root,bool opt) {
    auto project=vd::compile_project(root/"examples/interfaces/stress.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(project.module,project.module.entry_symbol,opt,{},runtime);
    for(bool forced:{false,true}) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;
        {Observe observe(p);require(code->invoke(10)==1510,"interface GC result");}
        require(p.constructions==1502 && p.roots_valid && p.commits_valid && p.frames.empty(),"interface roots/publication cleanup");
        vd::Runtime::Attachment attach(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"interface reachability leaked metadata");
    }
    Probe p;p.inspect_roots=true;p.fail_construction=3;p.fail_allocation=0;bool failed=false;
    {Observe observe(p);try{code->invoke(10);}catch(const std::exception&){failed=true;}}
    require(failed && p.frames.empty() && p.roots_valid,"interface failure cleanup");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"interface failure leak");}
    require(code->invoke(10)==1510,"interface failure recovery");
}
}
int main(int argc,char** argv) {
    try {
        const bool storage_proof = argc==6 && std::string(argv[5])=="--storage-proof";
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-library") {auto p=vd::compile_project(argv[2]);storage_fixture::emit(p.module,argv[3],std::string(argv[4])=="optimized",storage_proof);return 0;}
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-reload") {
            auto source=vd::read_source(argv[2]);auto text=source.text();auto position=text.find("return c.n");require(position!=std::string::npos,"interface reload body missing");text.replace(position,10,"return c.n + 1");
            // Match the original fixture's owned source identity in both native generations.
            auto original=vd::analyze_source({"/stress.void",text});if(!original.succeeded()) throw vd::CompilationError(original.diagnostics);
            storage_fixture::emit(original.program->lower(),argv[3],std::string(argv[4])=="optimized",storage_proof,"__reloaded");return 0;
        }
        require(argc==2,"interfaces project root required");
        auto phase=[](const char* name,auto work) {
            std::cerr<<"interfaces: "<<name<<" begin"<<std::endl;
            work();
            std::cerr<<"interfaces: "<<name<<" passed"<<std::endl;
        };
        phase("diagnostics",[&]{diagnostics();});phase("programs",[&]{programs();});
        phase("independent_ir",[&]{independent_ir();});phase("admission",[&]{admission();});
        phase("tooling_and_imports",[&]{tooling_and_imports(argv[1]);});
        for(bool opt:{false,true}) {
            std::cerr<<"interfaces: mode "<<(opt?"O2":"normal")<<std::endl;
            phase("roots",[&]{roots(argv[1],opt);});phase("handoffs",[&]{handoffs(argv[1],opt);});
            phase("selected",[&]{selected(argv[1],opt);});phase("retirement",[&]{retirement(argv[1],opt);});
            phase("parked",[&]{parked(argv[1],opt);});phase("timeout",[&]{timeout(argv[1],opt);});
            phase("active_reload",[&]{active_reload(opt);});
        }
        std::cout<<"structural interface checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
