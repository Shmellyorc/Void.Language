#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void empty(Runtime& runtime) {
    Runtime::Attachment attachment(runtime);Runtime::Managed managed(runtime);
    require(runtime.collect()==Runtime::Collection::completed && !runtime.snapshot().objects,"pattern/result roots leaked");
}
void diagnostics() {
    unsigned rejected=0;auto reject=[&](std::string source){++rejected;if(analyze_source({"rejected.void",std::move(source)}).succeeded())throw std::runtime_error("invalid pattern/result accepted: "+std::to_string(rejected));};
    reject("func entry(x: i32) -> i32:\n    var value = Ok(x)\n    return x\n");
    reject("func entry(x: i32) -> i32:\n    return x?\n");
    reject("func f(x: i32) -> result<i32, i32>:\n    return Err(x)\nfunc entry(x: i32) -> result<i32, string>:\n    return Ok(f(x)?)\n");
    reject("func entry(x: i32) -> i32:\n    if x is var x:\n        return x\n    return 0\n");
    reject("func entry(x: i32) -> i32:\n    if x is var capture:\n        pass\n    return capture\n");
    reject("func entry(x: i32) -> i32:\n    if not (x is var capture):\n        return capture\n    return 0\n");
    reject("func entry(x: i32) -> i32:\n    if x is var capture or true:\n        return capture\n    return 0\n");
    reject("enum item:\n    first\n    first\nfunc entry(x: i32) -> i32:\n    return x\n");
    reject("enum item:\n    first\nfunc entry(x: i32) -> i32:\n    var forged = item(x)\n    return x\n");
    reject("enum item:\n    first\nfunc entry(x: i32) -> i32:\n    var missing = item.absent\n    return x\n");
    reject("struct item:\n    value: i32\nfunc mutate(const value: result<item, string>):\n    if value is Ok(var object):\n        object.value = 99\n");
    reject("func entry(x: i32) -> i32:\n    if x is Ok(var value):\n        return value\n    return 0\n");
    reject("func entry(x: i32) -> i32:\n    var items: [result<i32, string>, 2]\n    return x\n");
    auto warning=analyze_source({"warning.void","func entry(x: i32) -> i32:\n    return match x:\n        0: 1\n"});
    require(warning.succeeded() && warning.diagnostics.size()==1 && warning.diagnostics[0].severity==Severity::warning && warning.diagnostics[0].code==DiagnosticCode::incomplete_match,"incomplete match warning absent");
    auto missing=analyze_source({"return.void","func entry(x: i32) -> i32:\n    match x:\n        0:\n            return 1\n"});
    require(!missing.succeeded(),"match warning hid missing return");
    auto complete=analyze_source({"complete.void","enum item:\n    first\n    second(value: i32)\nfunc entry(x: i32) -> i32:\n    var value = item.second(x)\n    return match value:\n        item.first: 0\n        item.second(var n): n\n"});
    require(complete.succeeded() && complete.diagnostics.empty(),"exhaustive enum match warned");
    auto nullable=analyze_source({"nullable.void","struct item:\n    value: i32\nfunc f(x: item?) -> i32:\n    return match x:\n        Ok(var value): value.value\n        null: 0\n"});
    require(nullable.succeeded() && nullable.diagnostics.empty(),"exhaustive nullable match warned");
}
void reload(bool optimized) {
    const std::string source="enum choice:\n    first\n    second(value: i32)\nfunc entry(x: i32) -> i32:\n    var value = choice.second(x)\n    return match value:\n        choice.first: 0\n        choice.second(var n): n + 1\n";
    auto analysis=analyze_source({"reload-pattern.void",source});require(analysis.succeeded(),"reload pattern analysis");
    auto runtime=std::make_shared<Runtime>(1);Jit jit;auto module=analysis.program->lower();auto old=jit.compile(module,module.entry_symbol,optimized,{},runtime);ReloadSlot slot(old);
    auto body=source;body.replace(body.find("n + 1"),5,"n + 2");auto changed=analyze_source({"reload-pattern.void",body});require(changed.succeeded(),"changed pattern analysis");
    require(jit.replace(slot,*changed.program,optimized) && slot.invoke(7)==9 && old->invoke(7)==8,"pattern body reload/lease");empty(*runtime);
    auto reordered=source;auto first=reordered.find("    first\n    second(value: i32)");reordered.replace(first,std::string("    first\n    second(value: i32)").size(),"    second(value: i32)\n    first");
    auto incompatible=analyze_source({"reload-pattern.void",reordered});require(incompatible.succeeded(),"enum reorder analysis");require(!jit.replace(slot,*incompatible.program,optimized),"enum ordinal change accepted as body reload");empty(*runtime);
}
void run(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"results",28},{"main",36},{"values",10},{"interfaces",7},{"generic",7},{"large",11},{"nullable",7},{"control",27},{"combined",35},{"unmatched",42},{"transactions",16},{"nested",17},{"types",14},{"fields",14}}) {
        auto program=compile_project(root/"examples/patterns"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        const auto input=std::string(name)=="unmatched"?1:7;
        managed_test::Probe probe;probe.inspect_roots=true;probe.frames.reserve(192);
        {managed_test::Observe observe(probe);if(code->invoke(input)!=answer)throw std::runtime_error(std::string("pattern/result source result: ")+name);}
        require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"pattern/result roots/publication");empty(*runtime);
        if(std::string(name)=="results"){require(code->invoke(0)==-1,"Err propagation");empty(*runtime);}
        if(std::string(name)=="nullable"){require(code->invoke(0)==-1,"Ok(null) confused with Err");empty(*runtime);}
        if(std::string(name)=="transactions") {
            bool raised=false;try{code->invoke(0);}catch(const std::runtime_error& failure){raised=std::string(failure.what()).find("transactions.void:")!=std::string::npos;}require(raised,"partial enum argument failure absent");empty(*runtime);require(code->invoke(7)==answer,"enum transaction retry failed");empty(*runtime);
        }
        if(std::string(name)=="unmatched") {
            managed_test::Probe failed;failed.inspect_roots=true;failed.frames.reserve(192);bool raised=false;
            {managed_test::Observe observe(failed);try{code->invoke(2);}catch(const std::runtime_error& failure){const auto text=std::string(failure.what());raised=text.find("unmatched.void:")!=std::string::npos && text.find("No match expression arm")!=std::string::npos;}}
            require(raised && failed.frames.empty() && failed.roots_valid,"unmatched expression failure/cleanup");empty(*runtime);require(code->invoke(1)==42,"match failure poisoned retry");empty(*runtime);
        }
        if(std::string(name)=="values" || std::string(name)=="large" || std::string(name)=="combined") {
            bool complete=false;unsigned failures=0;
            for(int budget=0;budget<768 && !complete;++budget) {
                managed_test::Probe failed;failed.inspect_roots=true;failed.frames.reserve(192);
                {managed_test::Observe observe(failed);try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"sum failure sweep result");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
                require(failed.frames.empty() && failed.roots_valid && failed.commits_valid,"sum failure cleanup/publication");empty(*runtime);
            }
            require(complete && failures>0,"sum allocating sweep incomplete");std::cout<<(optimized?"O2":"normal")<<' '<<name<<" allocation failures: "<<failures<<'\n';require(code->invoke(7)==answer,"sum failure poisoned retry");empty(*runtime);
        }
    }
}
}
int main(int argc,char** argv) {try{require(argc==2,"repository argument required");diagnostics();run(argv[1],false);run(argv[1],true);reload(false);reload(true);return 0;}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}}
