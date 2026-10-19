#include "void/backend.hpp"
#include "void/async.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>

namespace {
char generation, foreign_generation;
void* foreign_start(std::int32_t,vd::RuntimeFailure* failure) {
    void* task=nullptr;void* frame=nullptr;
    vd_async_new(&task,&frame,0,nullptr,0,0,nullptr,0,"",&foreign_generation,nullptr,failure);
    return task;
}
void* nested_start(std::int32_t input,vd::RuntimeFailure* failure) {
    vd_async_root(foreign_start,input,0,&generation,failure);
    return nullptr;
}
void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }
std::shared_ptr<const vd::ValidatedAnalysis> analysis(const std::filesystem::path& source,const std::string& text) {
    {std::ofstream out(source);out<<text;require(bool(out),"async lifetime source write");}
    auto result=vd::analyze_project(source);
    if(!result.succeeded())throw vd::CompilationError(result.diagnostics);
    return result.program;
}
struct Release {
    managed_test::Probe& probe;
    bool released=false;
    void now(){if(!released){released=true;probe.release.release();}}
    ~Release(){now();}
};
}
void async_lifetime_tests(const std::filesystem::path& source) {
    // A native reentry cannot publish a resumption from an unleased generation,
    // or create an executor nested in the current owning scope.
    {
        vd::Runtime runtime;vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
        for(auto start:{foreign_start,nested_start}) {
            vd::RuntimeFailure failure;
            vd_async_root(start,0,0,&generation,&failure);
            require(failure.message,"foreign generation or nested executor admitted");
            runtime.collect();require(runtime.snapshot().objects==0,"rejected activation leaked roots");
        }
    }
    for(bool optimize:{false,true}) {
        auto program=analysis(source,
            "struct box:\n    number: i32 = 0\n"
            "async func grandchild(owner: box):\n    defer {\n        owner.number += 100\n    }\n    owner.number += 1\n"
            "async func child(owner: box):\n    defer {\n        owner.number += 10\n    }\n    var ignored = grandchild(owner)\n"
            "async func entry(value: i32) -> i32:\n    var owner = box()\n    var ignored = child(owner)\n    return value\n");
        vd::Jit jit;auto code=jit.compile_async(*program,optimize);auto runtime=code->runtime_owner();
        managed_test::Probe probe;probe.capture=true;
        {managed_test::Observe observe(probe);require(code->invoke(7)==7,"successful unawaited root result");}
        vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);
        std::int32_t number=0;require(bool(probe.retained),"unawaited task owner missing");std::memcpy(&number,probe.retained->get(),sizeof(number));
        require(number==111,"adapter failed to drain descendants created after root completion");
        probe.retained.reset();runtime->collect();require(runtime->snapshot().objects==0,"successful drain retains roots");
    }
    // Child failure cancels the suspended sibling through reached cleanup.
    // A failing root cancels children before their bodies start (Delivery 2).
    const std::string failures=
        "struct box:\n    number: i32 = 0\n"
        "async func delay(value: i32) -> i32:\n    return value\n"
        "async func first(owner: box, value: i32) -> i32:\n"
        "    defer {\n        owner.number += 10\n    }\n"
        "    var pending = delay(value)\n    var operand = await pending\n    return 1 / operand\n"
        "async func second(owner: box, value: i32) -> i32:\n"
        "    defer {\n        owner.number += 100\n    }\n"
        "    return (value - value - 1) as u8 as i32\n"
        "async func entry(value: i32) -> i32:\n"
        "    var owner = box(number = 1)\n"
        "    var a = first(owner, 0)\n    var b = second(owner, value)\n";
    for(bool optimize:{false,true})for(int exit:{0,1,2}) {
        auto program=analysis(source,failures+(exit==1?"    return 1 / (value - value)\n":exit==2?"    return await a\n":"    return value\n"));
        vd::Jit jit;auto code=jit.compile_async(*program,optimize);auto runtime=code->runtime_owner();
        managed_test::Probe probe;probe.capture=true;probe.inspect_roots=true;probe.frames.reserve(1024);
        std::string diagnostic;
        {managed_test::Observe observe(probe);try{code->invoke(7);}catch(const std::runtime_error& failure){diagnostic=failure.what();}}
        require(!diagnostic.empty() && probe.retained && probe.frames.empty() && probe.roots_valid,"draining failure roots");
        if(exit==1)require(diagnostic.find("::entry")<diagnostic.find("Additional unobserved"),"root failure precedence");
        else require(diagnostic.find("::second")!=std::string::npos && diagnostic.find("::first")==std::string::npos,"child failure did not cancel suspended sibling");
        if(exit==2)require(diagnostic.find("Called from /main.void::entry")!=std::string::npos,"await failure lost caller trace");
        vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);
        std::int32_t number=0;std::memcpy(&number,probe.retained->get(),sizeof(number));
        require(number==(exit==1?1:111),"structured failure cleanup or cancellation-before-start");
        probe.retained.reset();runtime->collect();require(runtime->snapshot().objects==0,"drained failures retain roots");
    }
    // Hold a child while its parent is suspended. A collector, replacement and
    // competing root overlap this live old generation without owning its code.
    const std::string prefix=
        "async func child(value: i32) -> i32:\n    return value + 1\n"
        "async func entry(value: i32) -> i32:\n    var task = child(value)\n    var answer = await task\n";
    for(bool optimize:{false,true}) {
        auto original=analysis(source,prefix+"    return answer\n");
        auto edited=analysis(source,prefix+"    var again = await task\n    return answer + again - value\n");
        auto synchronous=analysis(source,"async func child(value: i32) -> i32:\n    return value + 1\nfunc entry(value: i32) -> i32:\n    return value + 1\n");
        vd::Jit jit;auto old=jit.compile_async(*original,optimize);auto runtime=old->runtime_owner();vd::ReloadSlot slot(old);
        managed_test::Probe probe;probe.gate_poll=2;probe.inspect_roots=true;probe.frames.reserve(1024);
        auto worker=std::async(std::launch::async,[lease=slot.acquire(),&probe]{managed_test::Observe observe(probe);return lease->invoke(7);});
        Release release{probe};require(probe.reached.try_acquire_for(std::chrono::seconds(10)),"async child did not reach poll gate");
        require(jit.replace(slot,*edited,optimize),"async live replacement rejected");old.reset();
        require(!jit.replace(slot,*synchronous,optimize),"reload changed the root async effect");
        require(jit.live_generations()==2,"suspended old code retired early");
        bool rejected=false;try{slot.invoke(7);}catch(const std::runtime_error& failure){rejected=std::string(failure.what()).find("owning async executor")!=std::string::npos;}
        require(rejected,"competing executor admitted to same Runtime");
        auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);return runtime->collect(std::chrono::seconds(10));});
        const bool pending=runtime->wait_for_request(std::chrono::seconds(10));release.now();
        require(collector.get()==vd::Runtime::Collection::completed && pending,"suspended async GC rendezvous");
        require(worker.get()==8 && probe.frames.empty() && probe.roots_valid,"old async generation resumed incorrectly");
        require(jit.live_generations()==1 && slot.invoke(7)==9,"old async generation retirement or new result");
        vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();
        require(runtime->snapshot().objects==0,"resumed activation roots leaked");
    }
}
