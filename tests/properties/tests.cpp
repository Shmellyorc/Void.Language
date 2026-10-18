#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void empty(Runtime& runtime) {
    Runtime::Attachment attachment(runtime);Runtime::Managed managed(runtime);
    require(runtime.collect()==Runtime::Collection::completed && !runtime.snapshot().objects,"property roots leaked");
}
void diagnostics() {
    const std::string type="struct item:\n    value: i32\n";
    const std::string getter="property (i item) number: i32:\n    get:\n        return i.value\n";
    for(const auto& source:{
        type+"property (i item) number: i32:\n    set(value):\n        i.value = value\n",
        type+"property (i item) number: i32:\n    get:\n        i.value = 3\n        return i.value\n",
        type+"func (i item) change():\n    i.value += 1\nproperty (i item) number: i32:\n    get:\n        i.change()\n        return i.value\n",
        type+getter+"func entry(x: i32) -> i32:\n    var i = item()\n    i.number = x\n    return x\n",
        type+getter+"func entry(x: i32) -> i32:\n    var i = item()\n    return i.number()\n",
        type+"property (i item) value: i32:\n    get:\n        return 1\n",
        type+"property (i item) number: i32:\n    get:\n        return true\n",
        type+getter+"func (i item) number() -> i32:\n    return 2\n",
        type+"property (i item) number: i32:\n    get:\n        return i.value\n    set(value):\n        i.value = value\nfunc entry(x: i32) -> i32:\n    const i = item()\n    i.number = x\n    return x\n",
        type+getter+"interface reader:\n    func number() -> i32\nfunc entry(x: i32) -> i32:\n    var i: reader = item()\n    return x\n"
    })require(!analyze_source({"invalid-property.void",source}).succeeded(),"invalid property accepted");
    const std::string noop="struct item:\n    value: i32\nproperty (i item) ignored: i32:\n    get:\n        return i.value\n    set(value):\n        pass\nfunc entry(x: i32) -> i32:\n    const frozen = item()\n    var alias = frozen\n    alias.ignored = x\n    return x\n";
    require(!analyze_source({"readonly-noop-setter.void",noop}).succeeded(),"no-op setter bypassed readonly authority");
    const std::string callable="value struct counter:\n    value: i32\nfunc (c counter) call(x: i32) -> i32:\n    c.value += x\n    return c.value\n";
    for(const auto& body:{"    const c = counter()\n    return c(x)\n", "    const items = [counter()]\n    return items[0](x)\n"})
        require(!analyze_source({"readonly-callable.void",callable+"func entry(x: i32) -> i32:\n"+body}).succeeded(),"readonly callable accepted");
    const std::string indexed="value struct point:\n    value: i32\n    other: i32\nfunc (p point) call(items: [point]):\n    items[0].other = 99\n    p.value = 5\nfunc entry(x: i32) -> i32:\n    var items = [point()]\n    items[0](items)\n    return items[0].other\n";
    require(!analyze_source({"array-receiver-alias.void",indexed}).succeeded(),"indexed inline receiver exposed unsafe copy-back");
    const auto source=type+getter+"func entry(x: i32) -> i32:\n    var i = item(value = x)\n    return i.number\n";
    auto analysis=analyze_source({"property-symbols.void",source});require(analysis.succeeded(),"property analysis failed");
    bool use=false;for(const auto& ref:analysis.program->references())if(ref.spelling=="::number")use=true;
    require(use,"property reference absent from tooling");
}
void reload(bool optimized) {
    const std::string original="struct item:\n    backing: i32\nproperty (i item) value: i32:\n    get:\n        return i.backing\n    set(value):\n        i.backing = value\nfunc entry(x: i32) -> i32:\n    var i = item()\n    i.value = x\n    return i.value\n";
    auto analysis=analyze_source({"reload-property.void",original});require(analysis.succeeded(),"property reload source");
    auto runtime=std::make_shared<Runtime>(1);Jit jit;auto module=analysis.program->lower();auto old=jit.compile(module,module.entry_symbol,optimized,{},runtime);ReloadSlot slot(old);
    auto check=[&](const std::string& from,const std::string& to,int answer) {
        auto text=original;auto pos=text.find(from);require(pos!=std::string::npos,"reload replacement missing");text.replace(pos,from.size(),to);
        auto changed=analyze_source({"reload-property.void",text});require(changed.succeeded(),"property reload analysis");
        require(jit.replace(slot,*changed.program,optimized) && slot.invoke(7)==answer && old->invoke(7)==7,"property body reload/old code lease");empty(*runtime);
    };
    check("return i.backing","return i.backing + 1",8);
    check("i.backing = value","i.backing = value + 2",9);
    auto text=original;auto begin=text.find("    set(value):");auto end=text.find("func entry",begin);text.erase(begin,end-begin);
    text.replace(text.find("    i.value = x\n"),std::string("    i.value = x\n").size(),"    i.backing = x\n");
    auto changed=analyze_source({"reload-property.void",text});require(changed.succeeded(),"readonly property signature analysis");
    require(!jit.replace(slot,*changed.program,optimized),"property setter removal accepted as a body reload");empty(*runtime);
}
void run(const std::filesystem::path& root,bool optimized) {
    auto hidden=analyze_project(root/"examples/properties/imports.void",{{"/imports.void",SourceFile((root/"examples/properties/imports.void").string(),"use \"./library.void\" as lib\nfunc entry(value: i32) -> i32:\n    var d = lib.base()\n    return d.hidden\n")}});
    require(!hidden.succeeded(),"private imported property accepted");
    for(auto [name,answer]:{std::pair{"main",20},{"generic",9},{"order",19},{"references",12},{"methods",9},{"callable",22},{"selection",22},{"imports",17},{"failure",14},{"receiver",7}}) {
        auto program=compile_project(root/"examples/properties"/(std::string(name)+".void"));
        auto runtime=std::make_shared<Runtime>(1);Jit jit;
        auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;
        {managed_test::Observe observe(probe);require(code->invoke(7)==answer,"property source result");}
        require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"property roots/publication");empty(*runtime);
        if(std::string(name)=="failure")for(int input:{0,-1}) {
            managed_test::Probe failed;failed.inspect_roots=true;bool raised=false;
            {managed_test::Observe observe(failed);try{code->invoke(input);}catch(const std::runtime_error& error){raised=std::string(error.what()).find("failure.void:")!=std::string::npos;}}
            require(raised && failed.frames.empty() && failed.roots_valid,"property source failure/cleanup");empty(*runtime);
            require(code->invoke(7)==answer,"property checked failure poisoned retry");empty(*runtime);
        }
        if(std::string(name)=="receiver") {
            require(code->invoke(-1)==99,"nonmutating setter overwrote RHS effects");empty(*runtime);
            bool raised=false;try{code->invoke(0);}catch(const std::runtime_error& error){raised=std::string(error.what()).find("receiver.void:")!=std::string::npos;}
            require(raised,"computed receiver getter ran after RHS");empty(*runtime);
        }
        if(std::string(name)=="main") {
            bool complete=false;unsigned failures=0;
            for(int budget=0;budget<512 && !complete;++budget) {
                managed_test::Probe failed;failed.inspect_roots=true;failed.frames.reserve(192);
                {managed_test::Observe observe(failed);try {managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"property failure sweep result");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
                require(failed.frames.empty() && failed.roots_valid && failed.commits_valid,"property failure cleanup");empty(*runtime);
            }
            require(complete && failures>0,"property allocating sweep incomplete");
            std::cout<<(optimized?"O2":"normal")<<" property allocation failures: "<<failures<<'\n';
            require(code->invoke(7)==answer,"property retry failed");empty(*runtime);
        }

    }
}
}
int main(int argc,char** argv) {
    try {require(argc==2,"repository argument required");diagnostics();run(argv[1],false);run(argv[1],true);reload(false);reload(true);return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
