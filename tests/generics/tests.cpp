#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
using namespace vd;
using managed_test::require;
namespace {
void empty(Runtime& r) {
    Runtime::Attachment a(r);Runtime::Managed m(r);
    require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"generic roots/objects survived cleanup");
}
void programs(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"main",7},{"functions",10},{"types",20},{"constraints",16},{"arrays",21},{"initialization",9},{"methods",10},{"failures",17},{"imports",7},{"order",11},{"large",14},{"reload",7}}) {
        auto program=compile_project(root/"examples/generics"/(std::string(name)+".void"));ir::validate(program.module);
        auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);if(code->invoke(7)!=answer)throw std::runtime_error(std::string("generic source result: ")+name);}
        require(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"generic roots/publication or frame balance invalid");
        if(program.module.coordinated)require(probe.roots_checked,"generic managed program did not exercise root inspection");
        if(std::string(name)=="arrays" || std::string(name)=="types")require(runtime->snapshot().completed_epoch,"generic allocation pressure did not collect");
        empty(*runtime);
        if(std::string(name)=="reload") {
            auto entry=std::find_if(program.module.functions.begin(),program.module.functions.end(),[](const auto& f){return f.source_name=="alternate";});
            require(entry!=program.module.functions.end(),"alternate entry missing");
            auto alternate=jit.compile(program.module,entry->name,optimized,{},runtime);require(alternate->invoke(7)==8,"generic selected entry");
        }
    }
}
void failures(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"failures",17},{"initialization",9}}) {
        auto program=compile_project(root/"examples/generics"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);require(code->invoke(7)==answer,"generic failure warmup");empty(*runtime);
        if(std::string(name)=="failures") {
            bool checked=false;try{code->invoke(-1);}catch(const std::runtime_error& error){auto message=std::string(error.what());checked=message.find("failures.void:")!=std::string::npos && message.find("::at")!=std::string::npos;}
            require(checked,"generic bounds failure lost original body source/function");empty(*runtime);
        }
        unsigned failures=0;bool success=false;
        for(int budget=0;budget<4096 && !success;++budget) {
            managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(128);
            {managed_test::Observe observe(probe);
                try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"generic publication result");success=true;}
                catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}
            }
            require(probe.roots_valid && probe.commits_valid && probe.frames.empty(),"generic failure roots/publication/frame leak");empty(*runtime);
            require(code->invoke(7)==answer,"generic failed invocation poisoned retry");empty(*runtime);
        }
        require(success && failures>0,"generic allocation sweep incomplete");
        std::cout<<name<<(optimized?" O2: ":" normal: ")<<failures<<" allocation failures; roots/cleanup/retry passed\n";
    }
}
void diagnostics() {
    const std::string entry="func entry(x: i32) -> i32:\n    return x\n";
    for(const auto& text:{
        "func f<t, t>(x: t) -> t:\n    return x\n",
        "func f<t: i32>(x: t) -> t:\n    return x\n",
        "func f<t>(x: missing) -> t:\n    return x\n",
        "func f<t>(x: [t, -1]) -> t:\n    return x[0]\n",
        "struct base:\n    value: i32\nfunc (b base) read() -> i32:\n    return b.value\nstruct derived extends base:\n    extra: i32\nfunc (d derived) read<t>(value: t) -> t:\n    return value\n",
        "func f<t>(f: t) -> t:\n    return f\n",
        "struct box<t>:\n    item: t\nfunc (b box<u>) get() -> u:\n    return b.item\n"
    })require(!analyze_source({"unused-invalid.void",text+entry}).succeeded(),"invalid unused generic declaration accepted");
    require(analyze_source({"unused-body.void","func f<t>(x: t) -> t:\n    return x.missing\n"+entry}).succeeded(),"unused generic body was checked eagerly");
    for(const auto& source:{
        "func f<t>(a: t, b: t) -> t:\n    return a\nfunc entry(x: i32) -> i32:\n    var y: i64 = 2\n    return f(x, y)\n",
        "func f<t>(a: t) -> t:\n    return a\nfunc entry(x: i32) -> i32:\n    var y = f(null)\n    return x\n",
        "func f<t>(a: [t]) -> i32:\n    return 0\nfunc entry(x: i32) -> i32:\n    return f([])\n",
        "func f<t>() -> t:\n    return t()\nfunc entry(x: i32) -> i32:\n    return f()\n",
        "func f<t>(a: t) -> t:\n    return a\nfunc entry(x: i32) -> i32:\n    return f<i64>(x) as i32\n",
        "interface readable:\n    func read() -> i32\nvalue struct bad:\n    value: i32\nfunc f<t: readable>(a: t) -> i32:\n    return 0\nfunc entry(x: i32) -> i32:\n    return f(bad())\n",
        "struct node:\n    value: i32\nfunc f<t>(a: t) -> t:\n    return a\nfunc entry(x: i32) -> i32:\n    const n = node()\n    var alias = f(n)\n    alias.value = x\n    return x\n",
        "interface readable:\n    func read() -> i32\nvalue struct bad:\n    value: i32\nfunc (b bad) read() -> i64:\n    return 0\nfunc f<t: readable>(a: t) -> i32:\n    return 0\nfunc entry(x: i32) -> i32:\n    return f(bad())\n",
        "struct box<t>:\n    value: t\nfunc (b box<t>) change():\n    b.value = b.value + 1\nfunc entry(x: i32) -> i32:\n    const b = box(value = x)\n    b.change()\n    return x\n",
        "value struct required<t>:\n    item: t\nstruct node:\n    value: i32\nfunc entry(x: i32) -> i32:\n    var bad: required<node>\n    return x\n",
        "struct base:\n    value: i32\nfunc (b base) read() -> i32:\n    return b.value\nstruct derived<u> extends base:\n    extra: u\nfunc (d derived<u>) read<t>(value: t) -> t:\n    return value\nfunc entry(x: i32) -> i32:\n    var d = derived(extra = x)\n    return d.read(x)\n",
        "func f<t>(x: t) -> t:\n    return f<[t]>([x])[0]\nfunc entry(x: i32) -> i32:\n    return f(x)\n"
    })require(!analyze_source({"invalid-use.void",source}).succeeded(),"invalid generic specialization accepted");
    const std::string bad="func f<t>(x: t) -> t:\n    return x.missing\nfunc entry(x: i32) -> i32:\n    return f(x)\n";
    auto result=analyze_source({"body-location.void",bad});require(!result.succeeded(),"bad generic body accepted");
    bool locations=false;for(const auto& d:result.diagnostics)if(d.primary.begin>=bad.find("x.missing") && d.primary.begin<bad.find("func entry"))for(const auto& related:d.related)if(related.span.begin>=bad.find("return f(x)"))locations=true;
    require(locations,"generic diagnostic lost body or call-site source");
}
void identities() {
    const std::string source="value struct left:\n    value: i32\nvalue struct right:\n    value: i32\nfunc identity<t>(value: t) -> t:\n    return value\nfunc recurse<t>(value: t, count: i32) -> t:\n    if count == 0:\n        return value\n    return recurse<t>(value, count - 1)\nfunc entry(x: i32) -> i32:\n    var a = identity(left(value = x))\n    var b = identity(right(value = x))\n    var c = identity(a)\n    return recurse(c.value + b.value, 3)\n";
    auto module=compile_source({"identities.void",source});unsigned identities=0,recursions=0;
    for(const auto& f:module.functions){identities+=f.display_name=="identity";recursions+=f.display_name=="recurse";}
    require(identities==2 && recursions==1,"generic instances duplicated or merged equal-layout nominal types");
    require(module.interface_bindings.empty(),"unconstrained generics introduced interface erasure");
    for(bool optimized:{false,true}){Jit jit;auto code=jit.compile(module,module.entry_symbol,optimized);require(code->invoke(7)==14,"nominal generic identity result");}
}
void tooling(const std::filesystem::path& root) {
    const std::string unused="interface readable:\n    func read() -> i32\nstruct node:\n    value: i32\nfunc unused<t: readable>(value: t, other: node) -> t:\n    return value\nfunc entry(x: i32) -> i32:\n    return x\n";
    auto original=analyze_source({"unused-navigation.void",unused});if(!original.succeeded())throw CompilationError(original.diagnostics);
    ToolingIndex unused_index(original.program);
    require(bool(unused_index.definition_at("unused-navigation.void",unused.find("t: readable")+3)),"unused generic constraint navigation missing");
    require(bool(unused_index.definition_at("unused-navigation.void",unused.find("other: node")+7)),"unused generic annotation navigation missing");
    auto result=analyze_project(root/"examples/generics/types.void");if(!result.succeeded())throw CompilationError(result.diagnostics);
    ToolingIndex index(result.program);std::set<std::string> identities;
    for(const auto& d:result.program->declarations())require(identities.insert(d.identity).second,"duplicate generic source declaration");
    auto source=read_source(root/"examples/generics/types.void").text();
    const auto use=source.find("box(value");require(use!=std::string::npos,"generic class use absent");
    auto definition=index.definition_at("/types.void",use);require(definition && definition->source.begin==source.find("box<t>"),"generic type did not navigate to original declaration");
    bool parameter=false;for(const auto& d:result.program->declarations())if(d.kind==AnalyzedDeclaration::Kind::type_parameter){parameter=true;require(!index.references_to(d.identity).empty(),"generic parameter references absent");}
    require(parameter,"generic source parameters missing");
    auto imported=analyze_project(root/"examples/generics/imports.void");if(!imported.succeeded())throw CompilationError(imported.diagnostics);
    ToolingIndex imports(imported.program);const auto text=read_source(root/"examples/generics/imports.void").text();const auto pos=text.find("library.identity");
    auto target=imports.definition_at("/imports.void",pos+8);require(target && target->module=="/library.void","imported generic definition lost");
}
void retirement(const std::filesystem::path& root,bool optimized) {
    auto program=compile_project(root/"examples/generics/types.void");auto runtime=std::make_shared<Runtime>(1);Jit jit;
    auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
    managed_test::Probe probe;probe.capture=true;probe.capture_at=3;
    {managed_test::Observe observe(probe);require(code->invoke(7)==20,"generic capture setup");}
    require(bool(probe.retained),"generic object was not retained");code.reset();require(jit.live_generations()==0,"generic metadata retained executable generation");
    {Runtime::Attachment a(*runtime);Runtime::Managed m(*runtime);require(runtime->collect()==Runtime::Collection::completed,"generic retirement collection");
        require(runtime->contains(probe.retained->get()) && runtime->snapshot().objects==1,"generic object metadata/identity lost on retirement");
        std::int32_t value=0;std::memcpy(&value,probe.retained->get(),sizeof(value));require(value==9,"generic retained object contents changed");}
    probe.retained.reset();empty(*runtime);
}
void reload(const std::filesystem::path& root,bool optimized) {
    auto path=root/"examples/generics/reload.void";auto original=analyze_project(path);if(!original.succeeded())throw CompilationError(original.diagnostics);
    auto runtime=std::make_shared<Runtime>(1);Jit jit;auto module=original.program->lower();auto old=jit.compile(module,module.entry_symbol,optimized,{},runtime);ReloadSlot slot(old);
    auto source=read_source(path);auto text=source.text();auto p=text.find("return value");text.insert(p+12," + 2");
    auto changed=analyze_project(path,{{"/reload.void",SourceFile(source.path(),text)}});if(!changed.succeeded())throw CompilationError(changed.diagnostics);
    require(jit.replace(slot,*changed.program,optimized) && slot.invoke(7)==9 && old->invoke(7)==7,"generic body reload/old generation lease failed");empty(*runtime);
}
}
int main(int argc,char** argv) {
    try{require(argc==2,"expected repository root");diagnostics();identities();tooling(argv[1]);for(bool optimize:{false,true}){programs(argv[1],optimize);failures(argv[1],optimize);retirement(argv[1],optimize);reload(argv[1],optimize);}std::cout<<"Generic source, diagnostics, tooling, GC, failures and reload passed\n";}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
