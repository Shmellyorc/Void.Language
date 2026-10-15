#include "../storage_proof.hpp"
#include "proof.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <future>
#include <algorithm>
#include <iostream>
using namespace managed_test;
namespace {
const std::string base="struct base:\n    n: i32\n\n";
const std::string chain=base+"struct derived extends base:\n    own: i32\n\n";
const std::string entry="func entry(v: i32) -> i32:\n";
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::string& text) {
    auto a=vd::analyze_source({"/inheritance.void",text});if(!a.succeeded()) throw vd::CompilationError(a.diagnostics);return a.program;
}
void rejects(const std::string& text,const std::string& fragment) {
    auto a=vd::analyze_source({"/inheritance.void",text});require(!a.succeeded(),"invalid inheritance accepted");bool found=false;
    for(const auto& d:a.diagnostics) {require(d.primary.source && d.primary.end>d.primary.begin,"inheritance diagnostic lost source span");found|=d.description.find(fragment)!=std::string::npos;}
    if(!found) throw vd::CompilationError(a.diagnostics);
}
void run(const std::string& text,int expected) {
    auto a=analyze(text);vd::Jit jit;
    for(bool opt:{false,true}) require(jit.compile(*a,opt)->invoke(10)==expected,"inheritance execution result");
}
void diagnostics() {
    std::string deep="struct limit0:\n    n: i32\n";
    for(unsigned n=1;n<130;++n) deep+="struct limit"+std::to_string(n)+" extends limit"+std::to_string(n-1)+":\n\n";
    rejects(deep,"Physical layout depth");
    std::string combined="value struct inline0:\n    n: i32\n";
    for(unsigned n=1;n<65;++n) combined+="value struct inline"+std::to_string(n)+":\n    child: inline"+std::to_string(n-1)+"\n";
    combined+="struct limit0:\n    payload: inline64\n";
    for(unsigned n=1;n<65;++n) combined+="struct limit"+std::to_string(n)+" extends limit"+std::to_string(n-1)+":\n\n";
    rejects(combined,"Physical layout depth");
    rejects("struct bad extends i32:\n    n: i32\n","Only a managed");
    rejects("value struct a:\n    n: i32\nstruct bad extends a:\n    m: i32\n","Only a managed");
    rejects(base+"value struct bad extends base:\n    m: i32\n","Only a managed");
    rejects(base+"struct bad extends base?:\n    m: i32\n","Only a managed");
    rejects("struct a extends b:\n    x: i32\nstruct b extends a:\n    y: i32\n","Recursive");
    rejects(base+"struct bad extends base:\n    n: f32\n","Duplicate field");
    rejects(base+"func (b base) read() -> i32:\n    return b.n\nstruct bad extends base:\n    read: i32\n","hide inherited");
    rejects(chain+"func (b base) read() -> i32:\n    return b.n\nfunc (d derived) read() -> i32:\n    return d.own\n","cannot be hidden");
    rejects(chain+"override func (d derived) read() -> i32:\n    return d.n\n","requires an inherited");
    rejects(chain+"virtual func (b base) read() -> i32:\n    return b.n\nfunc (d derived) read() -> i32:\n    return d.own\n","requires override");
    rejects(chain+"virtual func (b base) read(n: i32) -> i32:\n    return n\noverride func (d derived) read(n: bool) -> i32:\n    return d.own\n","match the inherited");
    rejects(chain+"virtual func (b base) read(const n: i32) -> i32:\n    return n\noverride func (d derived) read(n: i32) -> i32:\n    return n\n","match the inherited");
    rejects(base+"private virtual func (b base) read() -> i32:\n    return b.n\n","public or internal");
    rejects("virtual func read() -> i32:\n    return 0\n","managed receiver");
    rejects("value struct a:\n    n: i32\nvirtual func (a a) read() -> i32:\n    return a.n\n","managed receiver");
    rejects(chain+entry+"    var b = base()\n    var d: derived = b\n    return 0\n","Type mismatch");
    rejects(chain+entry+"    var b = base()\n    var d = b as derived\n    return 0\n","reference conversion");
    rejects(chain+entry+"    var d: derived?\n    var b: base = d\n    return 0\n","Type mismatch");
    rejects(chain+entry+"    const d = derived()\n    var b = d as base\n    b.n++\n    return 0\n","readonly");
    rejects(chain+"virtual func (b base) read() -> i32:\n    return b.n\noverride func (d derived) read() -> i32:\n    d.own++\n    return d.n\n"+entry+"    const b: base = derived()\n    return b.read()\n","readonly");
    rejects(chain+"func (b base) init():\n    b.n = 1\n","init-bearing ancestor");
    rejects(chain+"func (d derived) init():\n    d.n = 1\n","begin with super.init");
    rejects(chain+"func (d derived) init():\n    super.init(n = d.n)\n","self");
    rejects(chain+"func (d derived) init():\n    super.init()\n    super.init()\n","first derived initializer");
    rejects(base+"func (b base) init():\n    super.init()\n","super");
    const auto virtuals=chain+"virtual func (b base) read() -> i32:\n    return b.n\n";
    rejects(virtuals+"func (d derived) init():\n    super.init()\n    d.read()\n","Virtual-family");
    rejects(virtuals+"func (d derived) init():\n    super.init()\n    super.read()\n","Virtual-family");
    rejects(virtuals+"func helper(b: base):\n    b.read()\nfunc (d derived) init():\n    super.init()\n    helper(d)\n","Virtual-family");
    rejects(chain+"struct sibling extends base:\n    m: i32\n"+entry+"    if derived() == sibling():\n        return 1\n    return 0\n","common-base");
    rejects("private struct base:\n    n: i32\nstruct derived extends base:\n    m: i32\n","less-visible");
}
void programs() {
    // Exercise repeated Structure-vector relocation with engaged base Types and
    // uniquely owned field defaults; no declaration may copy or lose its AST.
    std::string relocated;
    for (unsigned n=0;n<32;++n) {
        auto suffix=std::to_string(n);
        relocated += "value struct payload_"+suffix+":\n    value: i32 = "+suffix+" + 1\n";
        relocated += "struct root_"+suffix+":\n    stored: payload_"+suffix+"\n";
        relocated += "struct leaf_"+suffix+" extends root_"+suffix+":\n    own: i32 = "+suffix+" * 2\n";
    }
    relocated += entry+"    var total = v\n";
    for (unsigned n=0;n<32;++n) {
        auto suffix=std::to_string(n);
        relocated += "    var node_"+suffix+" = leaf_"+suffix+"()\n";
        relocated += "    total += node_"+suffix+".stored.value + node_"+suffix+".own\n";
    }
    run(relocated+"    return total\n",1530);
    run("struct cell:\n    value: i32\nstruct owner:\n    child: cell\n    value: i32\nfunc (o owner) get() -> cell:\n    return o.child\nfunc (o owner) init(c: cell):\n    o.child = c\n    const alias = o\n    var child = alias.get()\n    o.value = child.value\n"+entry+"    return owner(cell(value = v)).value\n",10);
    run("struct empty:\n\nstruct derived extends empty:\n    own: i8\nvirtual func (e empty) read() -> i32:\n    return 0\noverride func (d derived) read() -> i32:\n    return d.own as i32\n"+entry+"    var a = empty()\n    var b: empty = derived(own = v as i8)\n    if a != b:\n        return b.read()\n    return 0\n",10);
    run(chain+"func helper(b: base):\n    b.n++\nfunc (d derived) init():\n    super.init(n = 4)\n    var alias: base = d\n    helper(alias)\n    d.own = 2\n"+entry+"    return derived().n\n",5);
    run(chain+"func view(const d: derived) -> base:\n    return d\n"+entry+"    var d = derived(n = v)\n    var b = view(d)\n    d.n++\n    return b.n\n",11);
    run(chain+entry+"    var d = derived(n = v, own = 2)\n    var b: base = d\n    b.n++\n    if b == d:\n        return d.n + d.own\n    return 0\n",13);
    run(chain+"func forward(b: base) -> base?:\n    return b\n"+entry+"    var d = derived(n = v)\n    var b = forward(d) as base\n    return b.n\n",10);
    run(chain+"value struct box:\n    root: base\n    count: i32\n"+entry+"    var d = derived(n = v)\n    var b = box(root = d)\n    var c = b\n    c.count = 3\n    c.root.n++\n    return b.root.n + b.count + c.count\n",14);
    run(chain+"func (b base) init(n: i32):\n    b.n = n\nfunc (d derived) init(n: i32):\n    super.init(n)\n    d.own = 4\n"+entry+"    return derived(v).n + derived(2).own\n",14);
    run(chain+"func (b base) bump():\n    b.n++\nfunc (d derived) init():\n    super.init(n = 5)\n    super.bump()\n    d.own = 3\n"+entry+"    return derived().n + derived().own\n",9);
    const auto v=chain+"struct last extends derived:\n    extra: i32\nvirtual func (b base) read() -> i32:\n    return b.n\noverride func (d derived) read() -> i32:\n    return super.read() + d.own\noverride func (l last) read() -> i32:\n    return super.read() + l.extra\nfunc (b base) indirect() -> i32:\n    return b.read()\n";
    run(v+entry+"    var l = last(n = 1, own = 2, extra = 3)\n    var b: base = l\n    return b.indirect() + l.read()\n",12);
    run(base+"value struct movement:\n    x: i32\nfunc (m movement) move():\n    m.x++\nstruct embedded extends base:\n    using movement\nstruct top extends embedded:\n    own: i32\n"+entry+"    var p = top(n = v)\n    p.move()\n    p.x += 2\n    return p.n + p.movement.x\n",13);
    rejects(base+"value struct movement:\n    x: i32\nstruct embedded extends base:\n    using movement\nstruct wrapper:\n    using embedded\n"+entry+"    var p = wrapper(embedded = embedded())\n    return p.n\n","Unknown field");
}
void construction_and_imports(const std::filesystem::path& root,bool optimized) {
    vd::Jit jit;
    for(auto [file,expected]:{std::pair{"main",108},{"initialization",92},{"order",123602},{"imports",13},{"receiver_order",113},{"nested",17},{"results",28}}) {
        auto a=vd::compile_project(root/"examples/inheritance"/(std::string(file)+".void"));
        auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(a.module,a.module.entry_symbol,optimized,{},runtime);
        Probe p;p.inspect_roots=true;p.collect_before=true;
        {Observe observe(p);require(code->invoke(10)==expected,"constructor/source-order/imported dispatch result");}
        require(p.roots_valid && p.commits_valid && p.frames.empty(),"constructor/import root cleanup");
        if(std::string(file)=="initialization") require(p.constructions==2,"base construction allocated a separate managed identity");
        if(std::string(file)=="order") require(p.constructions==2,"multi-level construction published prefix objects");
    }
    auto a=vd::compile_project(root/"examples/inheritance/failure.void");auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(a.module,a.module.entry_symbol,optimized,{},runtime);
    Probe p;p.inspect_roots=true;bool failed=false;
    {Observe observe(p);try{code->invoke(0);}catch(const std::runtime_error& e){failed=std::string(e.what()).find("zero")!=std::string::npos;}}
    require(failed && p.constructions==1 && p.frames.empty() && p.roots_valid && p.commits_valid,"base failure published incomplete parent or leaked frames");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"failed inherited initializer retained child");}
    require(code->invoke(10)==19,"inherited numeric failure recovery");
    const auto path=root/"examples/inheritance/imports.void";
    vd::SourceOverrides alias_override{{"/imports.void",vd::SourceFile(path.string(),"use \"./lib/base.void\" as super\n"+entry+"    return super.base(value = v).read()\n")}};
    auto alias=vd::analyze_project(path,alias_override);require(alias.succeeded(),"contextual super broke existing qualified module bindings");
    require(jit.compile(*alias.program,optimized)->invoke(10)==10,"qualified super module call changed meaning");
    const std::string parent="struct base:\n    n: i32\nprivate func (b base) init():\n    b.n = 1\n";
    const std::string child="use \"./base.void\" as b\nstruct derived extends b.base:\n    own: i32\nfunc (d derived) init():\n    super.init()\n";
    vd::SourceOverrides overrides{{"/imports.void",vd::SourceFile(path.string(),"use \"./lib/derived.void\" as d\n"+entry+"    return 0\n")},{"/lib/base.void",vd::SourceFile("/lib/base.void",parent)},{"/lib/derived.void",vd::SourceFile("/lib/derived.void",child)}};
    auto rejected=vd::analyze_project(path,overrides);require(!rejected.succeeded(),"super bypassed file-private initializer");
    bool visibility=false;for(const auto& d:rejected.diagnostics) visibility|=d.code==vd::DiagnosticCode::visibility;require(visibility,"private base init lost visibility diagnostic");
}
void performance(const std::filesystem::path& root) {
    auto p=vd::compile_project(root/"examples/inheritance/performance.void");vd::Jit jit;
    for(bool optimized:{false,true}) {
        auto code=jit.compile(p.module,p.module.entry_symbol,optimized);
        for(int mode:{0,1}) {
            for(unsigned warm=0;warm<3;++warm) require(code->invoke(mode)==1700000,"dispatch measurement changed result");
            std::vector<double> samples;
            for(unsigned repeat=0;repeat<7;++repeat) {
                auto start=std::chrono::steady_clock::now();require(code->invoke(mode)==1700000,"dispatch measurement result");
                samples.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
            }
            std::sort(samples.begin(),samples.end());
            std::cout<<"{\"optimized\":"<<(optimized ? "true" : "false")<<",\"dispatch\":\""<<(mode ? "virtual" : "direct")<<"\",\"iterations\":100000,\"samples\":7,\"median_us\":"<<samples[3]<<"}\n";
        }
    }
}
void ir_negative() {
    auto valid=analyze(chain+"virtual func (b base) read() -> i32:\n    return b.n\noverride func (d derived) read() -> i32:\n    return d.own\n"+entry+"    var b: base = derived(own = v)\n    return b.read()\n");
    auto reject=[](const vd::ir::Module& m) {bool failed=false;try{vd::ir::validate(m);}catch(const std::invalid_argument&){failed=true;}require(failed,"independent IR accepted forged inheritance contract");};
    auto bad=valid->lower();bad.nominal_types.clear();reject(bad);
    bad=valid->lower();std::erase_if(bad.nominal_types,[](auto t){return t.definition->name=="derived";});reject(bad);
    bad=valid->lower();for(auto& f:bad.functions) for(auto& block:f.blocks) for(auto& i:block.instructions) if(i.op==vd::ir::Op::gc_construct) {i.op=vd::ir::Op::gc_allocate;i.arguments.clear();}reject(bad);
    bad=valid->lower();for(auto& f:bad.functions) if(f.overriding) f.virtual_slot.clear();reject(bad);
    bad=valid->lower();for(auto& f:bad.functions) if(f.overriding) f.parameters.push_back(vd::ir::Type::boolean);reject(bad);
    bad=valid->lower();bool found=false;
    for(auto& f:bad.functions) for(auto& block:f.blocks) for(auto& i:block.instructions) if(i.op==vd::ir::Op::upcast) {auto fake=std::make_shared<vd::ir::ValueType>();fake->identity="unrelated";fake->name="unrelated";fake->managed=true;i.type=vd::ir::Type(fake,vd::ir::Type::managed_ref);found=true;}
    require(found,"upcast regression did not exercise explicit IR operation");reject(bad);
    bad=valid->lower();for(auto& f:bad.functions) for(auto& block:f.blocks) for(auto& i:block.instructions) if(i.op==vd::ir::Op::virtual_call) i.callee="missing";reject(bad);
    auto private_read=analyze("struct cell:\n    value: i32\nstruct owner:\n    child: cell\nfunc (o owner) get() -> cell:\n    return o.child\nfunc (o owner) init(c: cell):\n    o.child = c\n    var alias = o\n    var child = alias.get()\n    child.value++\n"+entry+"    return owner(cell(value = v)).child.value\n");
    bad=private_read->lower();bool private_alias=false;
    for(auto& f:bad.functions) if(f.initializer) for(unsigned slot=1;slot<f.locals.size();++slot) if(f.locals[slot].construction && f.construction_origins.contains(slot)) {f.locals[slot].readonly=true;private_alias=true;}
    require(private_alias,"readonly private-view getter regression omitted alias");
    bool readonly_rejected=false;try{vd::ir::validate(bad);}catch(const std::invalid_argument& e){readonly_rejected=std::string(e.what()).find("readonly")!=std::string::npos;}
    require(readonly_rejected,"private-view getter discarded readonly child authority");
    auto stage=analyze(chain+"func (d derived) init():\n    super.init()\n    d.own = 2\n"+entry+"    return derived().own\n");
    bad=stage->lower();found=false;
    for(auto& f:bad.functions) for(auto& block:f.blocks) for(std::size_t n=0;n<block.instructions.size();++n) if(block.instructions[n].op==vd::ir::Op::base_init) {block.instructions[n].op=vd::ir::Op::gc_poll;block.instructions[n].receiver.reset();block.instructions[n].callee.clear();found=true;break;}
    require(found,"base phase regression omitted dedicated operation");reject(bad);
    bad=stage->lower();for(auto& f:bad.functions) if(f.initializer) for(auto& block:f.blocks) for(auto& i:block.instructions) if(i.op==vd::ir::Op::base_init) i.type=f.parameters[0];reject(bad);
    auto reject_reason=[](const vd::ir::Module& m,const std::string& reason) {
        bool failed=false;try{vd::ir::validate(m);}catch(const std::invalid_argument& e){failed=std::string(e.what()).find(reason)!=std::string::npos;if(!failed) throw;}
        require(failed,"independent IR did not reach targeted construction invariant");
    };
    bad=stage->lower();
    for(auto& f:bad.functions) if(f.initializer && !f.parameters[0].definition->base.expired()) {
        vd::ir::Instruction zero{vd::ir::Op::constant};
        vd::ir::Instruction base_call{vd::ir::Op::base_init};base_call.type=vd::ir::Type(f.parameters[0].definition->base.lock(),vd::ir::Type::managed_ref);base_call.receiver=vd::ir::Place{0,{}};base_call.arguments={0};
        vd::ir::Instruction condition{vd::ir::Op::constant};condition.type=vd::ir::Type::boolean;condition.immediate=1;
        vd::ir::Instruction write{vd::ir::Op::gc_write,0,1};write.immediate=1;
        f.blocks={{{},{vd::ir::Terminator::Kind::jump,0,1}},{{zero,base_call,condition},{vd::ir::Terminator::Kind::branch,2,1,2}},{{{vd::ir::Op::load,0},zero,write},{vd::ir::Terminator::Kind::return_void}}};
    }
    reject_reason(bad,"base initialization out of order or repeated");
    bad=stage->lower();
    for(auto& f:bad.functions) if(f.source_name=="entry") for(auto& block:f.blocks) {
        for(unsigned n=0;n<block.instructions.size();++n) if(block.instructions[n].op==vd::ir::Op::gc_construct && block.instructions[n].receiver) {
            const auto owner=block.instructions[n].receiver->slot;
            auto prefix=vd::ir::Type(f.locals[owner].definition->base.lock(),vd::ir::Type::managed_ref);prefix.construction=true;
            auto alias=static_cast<unsigned>(f.locals.size());f.locals.push_back(prefix);f.construction_origins[alias]=owner;
            block.instructions.resize(n);
            block.instructions.push_back({vd::ir::Op::load,owner});
            vd::ir::Instruction project{vd::ir::Op::stage_prefix,n};project.type=prefix;block.instructions.push_back(project);
            block.instructions.push_back({vd::ir::Op::store,alias,n+1});block.instructions.push_back({vd::ir::Op::load,alias});
            vd::ir::Instruction read{vd::ir::Op::gc_read,n+3};block.instructions.push_back(read);
            vd::ir::Instruction commit{vd::ir::Op::gc_construct};commit.type=prefix;commit.type.construction=false;commit.receiver=vd::ir::Place{alias,{}};commit.arguments={n+4};block.instructions.push_back(commit);
            block.instructions.push_back({vd::ir::Op::constant});block.end={vd::ir::Terminator::Kind::return_value,n+6};break;
        }
    }
    reject_reason(bad,"construction prefix cannot publish");
    auto b=std::make_shared<vd::ir::ValueType>();b->name=b->identity="b";b->managed=true;b->fields={{"x",vd::ir::Type::i64,"b"},{"y",vd::ir::Type::i8,"b"}};
    auto d=std::make_shared<vd::ir::ValueType>();d->name=d->identity="d";d->managed=true;d->base=b;d->fields=b->fields;d->fields.push_back({"z",vd::ir::Type::i8,"d"});
    vd::ir::Type dt(d,vd::ir::Type::managed_ref);require(vd::ir::physical_field_path(dt,0)==std::vector<unsigned>{0,0} && vd::ir::physical_field_path(dt,2)==std::vector<unsigned>{1},"logical field mapping did not preserve full base");
    bad=valid->lower();bad.nominal_types.push_back(dt);d->fields[0].type=vd::ir::Type::boolean;reject(bad);
}
void tooling() {
    const auto text=chain+"virtual func (b base) read() -> i32:\n    return b.n\noverride func (d derived) read() -> i32:\n    return super.read() + d.own\n"+entry+"    var d = derived(n = v)\n    d.n++\n    var b: base = d\n    return b.read()\n";
    auto a=analyze(text);vd::ToolingIndex tools(a);
    auto field=tools.definition_at("/inheritance.void",text.find("d.n++")+2);require(field && field->identity=="/inheritance.void::base::field::n","inherited write navigation lost original owner");
    auto method=tools.definition_at("/inheritance.void",text.rfind("b.read()")+2);require(method && method->identity=="/inheritance.void::base::method::read","virtual navigation guessed override");
    auto super=tools.definition_at("/inheritance.void",text.find("super.read()")+6);require(super && super->identity==method->identity,"super navigation lost static implementation");
    auto derived=tools.declaration("/inheritance.void::derived");require(derived && derived->base_identity=="/inheritance.void::base","analysis base metadata absent");
    vd::Jit jit;vd::ReloadSlot slot(jit.compile(*a));auto body=text;body.replace(body.find("return super.read() + d.own"),27,"return super.read() + d.own + 1");auto replacement=analyze(body);
    require(jit.replace(slot,*replacement) && slot.invoke(10)==12,"compatible override body reload");
}
void admission() {
    vd::Runtime runtime;
    vd::NominalDescriptor base{"base","","base shape",{16,{}}};
    vd::NominalDescriptor derived{"derived","base","derived shape",{32,{16,24}}};
    const std::vector<vd::NominalDescriptor> good{derived,base};runtime.admit_types(good);
    auto original=runtime.descriptor("base");runtime.admit_types(good);require(runtime.descriptor("base")==original,"compatible admission did not intern descriptor");
    auto conflict=base;conflict.shape="different";vd::NominalDescriptor extra{"extra","","extra shape",{1,{}}};
    bool rejected=false;try{runtime.admit_types(std::vector{extra,conflict});}catch(const std::invalid_argument&){rejected=true;}
    require(rejected && runtime.descriptor("base")==original,"conflicting admission changed descriptor");
    rejected=false;try{runtime.descriptor("extra");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"failed batch left partial descriptor debris");
    auto cycle=base;cycle.identity="cycle";cycle.base="cycle";
    rejected=false;try{runtime.admit_types(std::vector{cycle});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"runtime accepted cyclic nominal ancestry");
    unsigned failures=0;
    for(int budget=0;budget<32;++budget) {
        vd::Runtime isolated;
        bool failed=false;try{AllocationFailure injection(budget);isolated.admit_types(good);}catch(const std::bad_alloc&){failed=true;}
        if(!failed) {require(isolated.descriptor("derived")->base=="base","admission recovery");break;}
        ++failures;bool absent=false;try{isolated.descriptor("base");}catch(const std::invalid_argument&){absent=true;}require(absent && isolated.snapshot().objects==0,"admission OOM published partial batch or swept");
    }
    require(failures>=4,"admission allocation failure paths not exercised");
    vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);
    rejected=false;try{runtime.admit_types(good);}catch(const std::logic_error&){rejected=true;}require(rejected,"Managed thread admitted metadata");
    void* object=nullptr;const std::size_t offset=0;const vd::Runtime::Slot slot{&object,sizeof(object),&offset,1};vd::Runtime::Frame frame(runtime,&slot,1);
    auto layout=runtime.descriptor("derived")->layout;layout.descriptor=runtime.descriptor("derived");
    inheritance_test::Derived payload{};runtime.construct_into(&object,layout,&payload);
    const char* keys[]={"base","derived"};require(runtime.dynamic_type(object,keys)==1 && runtime.dynamic_type(nullptr,keys)==-1,"exact nominal query/null guard");
    const char* missing[]={"base"};require(runtime.dynamic_type(object,missing)==-1,"generation catalog silently guessed unknown type");
    auto forged=std::make_shared<vd::NominalDescriptor>(*layout.descriptor);layout.descriptor=forged;
    auto previous=object;rejected=false;try{runtime.construct_into(&object,layout,&payload);}catch(const std::invalid_argument&){rejected=true;}require(rejected && object==previous,"uninterned descriptor escaped output protection");
    vd::RuntimeFailure failure;vd_gc_construct_typed(&object,sizeof(payload),nullptr,0,&payload,"missing",&failure);
    require(failure.message && object==previous,"typed ABI descriptor failure changed existing output");
}
void retirement(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/inheritance/stress.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);
    auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    Probe p;p.capture=true;p.capture_at=2;
    {Observe observe(p);require(code->invoke(10)==1110,"retained derived result");}
    require(p.retained && jit.live_generations()==1,"retained object setup");
    const auto descriptor=runtime->descriptor("/stress.void::derived");code.reset();require(jit.live_generations()==0,"managed identity pinned allocating code");
    {
        vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();
        require(runtime->snapshot().objects==2,"Base-root descriptor lost derived-only child after code retirement");
        const char* keys[]={"/stress.void::base","/stress.void::derived","/stress.void::leaf"};
        require(runtime->dynamic_type(p.retained->get(),keys)==1,"retired code left a dangling nominal identity");
    }
    auto next=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    require(runtime->descriptor("/stress.void::derived")==descriptor && next->invoke(10)==1110,"new generation did not reuse immutable descriptor");
    p.retained.reset();vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"retired allocation retained graph");
}
std::binary_semaphore entered{0},resume{0};
extern "C" std::int32_t native_gate(std::int32_t n) noexcept {entered.release();resume.acquire();return n;}
void reload_and_threads(bool optimized) {
    const auto source=chain+"virtual func (b base) read() -> i32:\n    return b.n\noverride func (d derived) read() -> i32:\n    return d.own\nfunc gate(n: i32) -> i32:\n    return n\n"+entry+"    var b: base = derived(own = v)\n    gate(0)\n    return b.read()\n";
    auto a=analyze(source);auto module=a->lower();module.imports.push_back("native_gate");
    std::erase_if(module.functions,[](const auto& f){return f.source_name=="gate";});
    for(auto& f:module.functions) for(auto& block:f.blocks) for(auto& i:block.instructions) if(i.callee=="/inheritance.void::gate") i.callee="native_gate";
    vd::Jit jit;jit.bind("native_gate",native_gate);vd::ReloadSlot slot(jit.compile(module,module.entry_symbol,optimized));
    auto runtime=slot.acquire()->runtime_owner();Probe p;p.inspect_roots=true;
    auto worker=std::async(std::launch::async,[&]{Observe observe(p);return slot.invoke(10);});
    struct Release {bool done=false;void now(){if(!done){done=true;resume.release();}}~Release(){now();}} release;
    require(entered.try_acquire_for(std::chrono::seconds(5)),"virtual generation lease overlap absent");
    auto updated=source;auto at=updated.find("return d.own");updated.replace(at,std::string("return d.own").size(),"return d.own + 1");auto b=analyze(updated);
    require(jit.replace(slot,*b,optimized) && jit.live_generations()==2 && slot.invoke(10)==11,"new virtual generation publication");
    {vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"collection during native gate");}
    release.now();require(worker.get()==10 && p.frames.empty() && p.roots_valid && jit.live_generations()==1,"old active object used new code or outlived generation lease");
    auto hierarchy=chain+"struct other:\n    n: i32\n"+entry+"    return derived().own\n";
    auto before=analyze(hierarchy);hierarchy.replace(hierarchy.find("extends base"),12,"extends other");auto after=analyze(hierarchy);
    require(vd::plan_invalidation(*before,{},after.get()).source_compatibility==vd::SourceCompatibility::restart_required,"base edge change did not require restart");
    const auto construction=base+"virtual func (b base) read() -> i32:\n    return b.n\nstruct owner:\n    value: i32\nfunc (o owner) init(b: base):\n    o.value = b.read()\n"+entry+"    return owner(base(n = v)).value\n";
    before=analyze(construction);auto changed=construction;changed.replace(changed.find("return b.n"),10,"return b.n + 1");after=analyze(changed);
    require(vd::plan_invalidation(*before,{},after.get()).source_compatibility==vd::SourceCompatibility::restart_required,"constructor virtual dependency allowed reload");
}
void roots(const std::filesystem::path& root,bool optimized) {
    auto project=vd::compile_project(root/"examples/inheritance/stress.void");vd::Jit jit;auto runtime=std::make_shared<vd::Runtime>(1);auto code=jit.compile(project.module,project.module.entry_symbol,optimized,{},runtime);
    for(bool forced:{false,true}) {
        Probe p;p.inspect_roots=true;p.collect_before=forced;bool witnessed=false;
        p.before_construct=[&](auto n,const void* payload,auto size){inheritance_test::inspect(p,witnessed,n,payload,size);};
        {Observe observe(p);require(code->invoke(10)==1110,"sole Base carrier source result");}
        require(witnessed && p.constructions==2002 && p.roots_valid && p.commits_valid && p.frames.empty(),"sole Base aggregate root/publication proof");
        vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"inherited cycle leak");
    }
    Probe p;p.inspect_roots=true;p.fail_construction=3;p.fail_allocation=0;bool failed=false;
    {Observe observe(p);try{code->invoke(10);}catch(const std::exception&){failed=true;}}
    require(failed && p.frames.empty() && p.roots_valid && p.commits_valid,"inherited failure cleanup");
    {vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"inherited failure leak");}
    require(code->invoke(10)==1110,"inherited failure recovery");
    Probe gate;gate.inspect_roots=true;gate.gate_after_constructions=2;
    auto worker=std::async(std::launch::async,[&]{Observe observe(gate);return code->invoke(10);});
    struct Release {Probe& p;bool done=false;void now(){if(!done){done=true;p.release.release();}}~Release(){now();}} release{gate};
    require(gate.reached.try_acquire_for(std::chrono::seconds(5)),"inherited deterministic collection overlap absent");
    auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment a(*runtime);vd::Runtime::Managed m(*runtime);return runtime->collect(std::chrono::seconds(5));});
    bool requested=runtime->wait_for_request(std::chrono::seconds(5));release.now();
    require(requested && collector.get()==vd::Runtime::Collection::completed && worker.get()==1110 && runtime->snapshot().cooperative_parks>0,"inherited worker failed to park during collection");
}
}
int main(int argc,char** argv) {
    try {
        const bool storage_proof = argc==6 && std::string(argv[5])=="--storage-proof";
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-reload") {
            auto source=vd::read_source(argv[2]);auto text=source.text();const std::string body="return d.child.value + (d.wide as i32)";
            auto offset=text.find(body);require(offset!=std::string::npos,"reload fixture body absent");text.replace(offset,body.size(),body+" + 1");
            vd::SourceOverrides overrides{{"/stress.void",vd::SourceFile(source.path(),text)}};auto a=vd::analyze_project(argv[2],overrides);if(!a.succeeded()) throw vd::CompilationError(a.diagnostics);
            storage_fixture::emit(a.program->lower(),argv[3],std::string(argv[4])=="optimized",storage_proof,"__reloaded");return 0;
        }
        if((argc==5 || storage_proof) && std::string(argv[1])=="--emit-library") {auto p=vd::compile_project(argv[2]);storage_fixture::emit(p.module,argv[3],std::string(argv[4])=="optimized",storage_proof);return 0;}
        if(argc==3 && std::string(argv[1])=="--measure") {performance(argv[2]);return 0;}
        require(argc==2,"inheritance project root required");diagnostics();programs();ir_negative();tooling();admission();for(bool opt:{false,true}) {roots(argv[1],opt);retirement(argv[1],opt);construction_and_imports(argv[1],opt);reload_and_threads(opt);}
        std::cout<<"inheritance: source, exact contracts, prefix stages, virtual effects, IR, sole Base GC roots, reload and navigation passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
