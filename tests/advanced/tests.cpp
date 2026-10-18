#include "../managed/probe.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <cstring>
#include <iostream>
using namespace vd;
using managed_test::require;
namespace {
void empty(Runtime& runtime) {
    Runtime::Attachment attachment(runtime);Runtime::Managed managed(runtime);
    require(runtime.collect()==Runtime::Collection::completed && !runtime.snapshot().objects,"advanced source roots leaked");
}
void diagnostics() {
    unsigned count=0;auto reject=[&](std::string source){++count;auto result=analyze_source({"rejected.void",std::move(source)});if(result.succeeded())throw std::runtime_error("invalid advanced source accepted: "+std::to_string(count));};
    reject("func entry(x: i32) -> i32:\n    var f: func() -> i32\n    return f()\n");
    reject("func entry(x: i32) -> i32:\n    var f = func(y: i64) -> i64:\n        return y\n    return f(x)\n");
    reject("struct box:\n    n: i32\nfunc f(const b: box) -> func() -> i32:\n    return func() -> i32:\n        b.n += 1\n        return b.n\n");
    reject("struct box:\n    n: i32\nfunc f(const b: box) -> func() -> box:\n    return func() -> box:\n        return b\n");
    reject("value struct item:\n    n: i32\nfunc (i item) deinit():\n    pass\n");
    reject("struct item:\n    n: i32\nfunc (i item) deinit() -> i32:\n    return 1\n");
    reject("struct item:\n    n: i32\nfunc (i item) deinit():\n    pass\nfunc f(const i: item):\n    i.deinit()\n");
    reject("func f():\n    defer {\n        return\n    }\n");
    reject("func f() -> result<i32, i32>:\n    return Ok(1)\nfunc g():\n    defer {\n        var n = f()?\n    }\n");
    reject("func g():\n    defer 1\n");
    reject("value struct item:\n    using callback: func() -> i32\n");
    reject("value struct item:\n    n: i32\nfunc (i item) call() -> i32:\n    return i.n\nconst callback: func() -> i32 = item()\n");
    reject("func f():\n    with 1 as n:\n        pass\n");
    reject("struct item:\n    event changed: func(i32) -> i32\n");
    reject("struct item:\n    event changed: func(i32)\nfunc f(i: item):\n    i.changed.emit(1)\n");
    reject("struct item:\n    event changed: func(i32)\nfunc f(i: item):\n    var handlers = i.changed\n");
    reject("struct item:\n    event changed: func(i32)\nfunc handler(n: i32):\n    pass\nfunc f(const i: item):\n    i.changed += handler\n");
    reject("struct item:\n    event changed: func(i32)\nfunc f() -> item:\n    return item(changed=[])\n");
    reject("struct item:\n    n: i32\n    callback: (func() -> i32)?\nfunc (i item) init():\n    i.callback = func() -> i32:\n        return i.n\n");
}
void independent_roots(){
    const std::string source="struct emitter:\n    event changed: func(i32)\nfunc ignore(n: i32):\n    pass\nfunc entry(n: i32) -> i32:\n    var e = emitter()\n    e.changed += ignore\n    return n\n";
    auto runtime=std::make_shared<Runtime>(1);Jit jit;
    for(const auto* path:{"first.void","second.void"}){auto analysis=analyze_source({path,source});require(analysis.succeeded(),"independent callback analysis");auto module=analysis.program->lower();auto code=jit.compile(module,module.entry_symbol,false,{},runtime);require(code->invoke(7)==7,"callback schema changed between compilation roots");empty(*runtime);}
}
void reload(bool optimized) {
    const std::string source="func make(n: i32) -> func() -> i32:\n    var state = n\n    return func() -> i32:\n        state += 1\n        return state\nfunc entry(n: i32) -> i32:\n    var callback = make(n)\n    return callback()\n";
    auto analysis=analyze_source({"closure-reload.void",source});require(analysis.succeeded(),"closure reload analysis");
    auto runtime=std::make_shared<Runtime>(1);Jit jit;auto module=analysis.program->lower();auto old=jit.compile(module,module.entry_symbol,optimized,{},runtime);ReloadSlot slot(old);
    auto body=source;body.replace(body.find("state += 1"),10,"state += 2");auto changed=analyze_source({"closure-reload.void",body});require(changed.succeeded(),"closure body analysis");
    require(jit.replace(slot,*changed.program,optimized) && slot.invoke(7)==9 && old->invoke(7)==8,"closure body reload or old lease");empty(*runtime);
    auto capture=source;capture.replace(capture.find("var state = n"),13,"var state: i64 = n as i64");capture.replace(capture.find("return state"),12,"return state as i32");auto incompatible=analyze_source({"closure-reload.void",capture});require(incompatible.succeeded(),"capture-layout analysis");require(!jit.replace(slot,*incompatible.program,optimized),"changed capture layout accepted");empty(*runtime);
}
void run(const std::filesystem::path& root,bool optimized) {
    for(auto [name,answer]:{std::pair{"closures",17},{"captures",127},{"callbacks",38},{"defer",227},{"with",186},{"events",122},{"cleanup_failure",321},{"propagation",17},{"event_snapshot",129},{"nullable",14},{"large",17},{"fixed_capture",17},{"inheritance",17},{"scope",13},{"with_transaction",1},{"combined",47},{"function_arrays",28},{"identity",30},{"generic_callbacks",35},{"dictionary_closures",21},{"construction_cleanup",10},{"composite_defaults",17}}) {
        auto program=compile_project(root/"examples/advanced"/(std::string(name)+".void"));auto runtime=std::make_shared<Runtime>(1);Jit jit;auto code=jit.compile(program.module,program.module.entry_symbol,optimized,{},runtime);
        managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(256);
        {managed_test::Observe observe(probe);if(code->invoke(7)!=answer)throw std::runtime_error(std::string("advanced source result: ")+name);}
        require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"advanced roots/publication/collection");empty(*runtime);
        if(std::string(name)=="with_transaction")for(unsigned construction:{5u,6u}){
            managed_test::Probe failed;failed.inspect_roots=true;failed.capture=true;failed.fail_construction=construction;failed.fail_allocation=0;failed.frames.reserve(256);bool raised=false;
            {managed_test::Observe observe(failed);try{code->invoke(7);}catch(const std::runtime_error&){raised=true;}}
            require(raised && failed.retained && failed.frames.empty() && failed.roots_valid && failed.commits_valid,"with allocation failure/root publication");
            {Runtime::Attachment attachment(*runtime);Runtime::Managed managed(*runtime);std::int32_t total;std::memcpy(&total,failed.retained->get(),sizeof(total));require(total==(construction==5?0:1),"with lost an acquired resource during allocation failure");}failed.retained.reset();empty(*runtime);
        }
        if(std::string(name)=="combined"){require(code->invoke(-1)==-3,"combined propagation cleanup");empty(*runtime);}
        if(std::string(name)=="propagation"){require(code->invoke(0)==-10,"Err bypassed defer");empty(*runtime);}
        if(std::string(name)=="cleanup_failure") {
            managed_test::Probe failed;failed.inspect_roots=true;failed.collect_before=true;failed.capture=true;failed.frames.reserve(256);bool raised=false;
            {managed_test::Observe observe(failed);try{code->invoke(0);}catch(const std::runtime_error& error){const std::string text=error.what();raised=text.find("30 / divisor")!=std::string::npos && text.find("Additional failure during defer")!=std::string::npos;}}
            require(raised && failed.retained && failed.frames.empty() && failed.roots_valid,"defer primary/secondary failure or roots");
            {Runtime::Attachment attachment(*runtime);Runtime::Managed managed(*runtime);std::int32_t total;std::memcpy(&total,failed.retained->get(),sizeof(total));require(total==321,"cleanup failure skipped remaining defers");}failed.retained.reset();empty(*runtime);
        }
        if(std::string(name)=="captures" || std::string(name)=="with" || std::string(name)=="events" || std::string(name)=="fixed_capture") {
            bool complete=false;unsigned failures=0;
            for(int budget=0;budget<1536 && !complete;++budget){managed_test::Probe failed;failed.inspect_roots=true;failed.frames.reserve(256);
                {managed_test::Observe observe(failed);try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"advanced sweep result");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}}
                require(failed.frames.empty() && failed.roots_valid && failed.commits_valid,"advanced allocation cleanup/publication");empty(*runtime);
            }
            require(complete && failures>0,"advanced allocation sweep incomplete");std::cout<<(optimized?"O2":"normal")<<' '<<name<<" allocation failures: "<<failures<<'\n';require(code->invoke(7)==answer,"advanced failure poisoned retry");empty(*runtime);
        }
    }
}
}
int main(int argc,char** argv){try{require(argc==2,"repository argument required");diagnostics();independent_roots();run(argv[1],false);run(argv[1],true);reload(false);reload(true);return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
