#include "void/backend.hpp"
#include "void/async.hpp"
#include <future>
#include <chrono>
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include "../managed/probe.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::shared_ptr<const vd::ValidatedAnalysis> analyze(const std::filesystem::path& source){auto result=vd::analyze_project(source);if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}
char native_generation;
struct NativeFrame {void* task;void* parent;void* child;int mode;int step;};
constexpr std::size_t native_offsets[]={offsetof(NativeFrame,task),offsetof(NativeFrame,parent),offsetof(NativeFrame,child)};
void native_resume(void*,vd::RuntimeFailure*);
bool duplicate_rejected=false;
void* native_create(int mode,void* parent,vd::RuntimeFailure* failure){
    auto& runtime=vd::Runtime::current();void* task=nullptr;void* frame=nullptr;const std::size_t pointer=0;
    vd::Runtime::Slot slots[]={{&task,sizeof(task),&pointer,1},{&frame,sizeof(frame),&pointer,1}};vd::Runtime::Frame roots(runtime,slots,2);
    vd_async_new(&task,&frame,sizeof(NativeFrame),native_offsets,3,8,nullptr,0,"lifecycle-test-task",&native_generation,native_resume,failure);
    if(failure->message)return nullptr;
    *static_cast<NativeFrame*>(frame)={task,parent,nullptr,mode,0};vd_async_start(task,failure);
    vd::RuntimeFailure duplicate;vd_async_start(task,&duplicate);duplicate_rejected|=duplicate.message!=nullptr;return task;
}
void* native_start(std::int32_t mode,vd::RuntimeFailure* failure){return native_create(mode,nullptr,failure);}
void native_resume(void* raw,vd::RuntimeFailure* failure){
    auto& frame=*static_cast<NativeFrame*>(raw);
    if(frame.step==0){
        if(frame.mode==0 || frame.parent){vd_async_ready(frame.parent?frame.parent:frame.task,failure);frame.step=2;}
        else {frame.child=native_create(frame.mode,frame.task,failure);frame.step=failure->message?2:1;}
    }
    if(frame.step==1){if(!vd_async_ready(frame.child,failure)){vd_async_suspend(raw,frame.child,failure);if(!failure->message)return;}frame.step=2;}
    if(!vd_async_drain(raw,failure))return;
    vd_async_finish(failure);if(!failure->message)vd_async_complete(frame.task);
}
void native_cycles(){
    vd::Runtime runtime;vd::NominalDescriptor descriptor{"lifecycle-test-task","","plain",{8,{}}};runtime.admit_types({&descriptor,1});
    vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);
    for(int mode:{0,1}){vd::RuntimeFailure failure;vd_async_root(native_start,mode,4,&native_generation,&failure);require(failure.message && std::string(failure.message).find(mode?"cycle":"itself")!=std::string::npos,"invalid async dependency was not rejected");require(duplicate_rejected,"duplicate task publication was accepted");runtime.collect();require(runtime.snapshot().objects==0,"cyclic task cleanup retained roots");}
}

}
void async_lifecycle_tests(const std::filesystem::path& repository,const std::filesystem::path& fixtures){
    std::filesystem::create_directories(fixtures);native_cycles();
    for(bool optimize:{false,true}){
        for(auto [name,answer]:{std::pair{"lifecycle_values",17},std::pair{"lifecycle_resources",30},std::pair{"lifecycle",8},std::pair{"lifecycle_exits",14}}){
            auto program=analyze(repository/"examples/async"/(std::string(name)+".void"));vd::Jit jit;auto code=jit.compile_async(*program,optimize);auto runtime=code->runtime_owner();
            managed_test::Probe probe;probe.inspect_roots=true;probe.collect_before=true;probe.frames.reserve(1024);
            {managed_test::Observe observe(probe);require(code->invoke(7)==answer,"structured lifecycle fixture result");}
            require(probe.frames.empty() && probe.roots_valid && probe.commits_valid,"structured lifecycle precise roots");
            bool succeeded=false;unsigned failed=0;
            for(int budget=0;budget<2048 && !succeeded;++budget){managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(1024);
                {managed_test::Observe observe(p);try{managed_test::AllocationFailure inject(budget);require(code->invoke(7)==answer,"structured allocation retry result");succeeded=true;}catch(const std::bad_alloc&){++failed;}catch(const std::runtime_error&){++failed;}}
                require(p.frames.empty() && p.roots_valid,"structured allocation cleanup roots");vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"structured allocation retained roots");
            }
            require(succeeded && failed,"structured allocation sweep incomplete");
        }
        for(const char* name:{"lifecycle_cancelled","lifecycle_failures","lifecycle_primary"}){
            auto program=analyze(repository/"examples/async"/(std::string(name)+".void"));vd::Jit jit;auto code=jit.compile_async(*program,optimize);auto runtime=code->runtime_owner();
            managed_test::Probe p;p.capture=true;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(1024);std::string diagnostic;
            {managed_test::Observe observe(p);try{code->invoke(7);}catch(const std::runtime_error& error){diagnostic=error.what();}}
            require(!diagnostic.empty() && p.retained && p.frames.empty() && p.roots_valid,"structured failed/cancelled roots");
            vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);
            std::int32_t number=0;std::memcpy(&number,p.retained->get(),sizeof(number));
            if(std::string(name)=="lifecycle_cancelled")require(diagnostic.find("Async task cancelled")!=std::string::npos && number==1,"suspended cancellation did not run reached cleanup");
            else if(std::string(name)=="lifecycle_primary"){require(diagnostic.find("Division by zero")<diagnostic.find("Additional failure during defer") && diagnostic.find("Additional failure during defer")<diagnostic.find("conversion is out of range"),"parent failure or additional cleanup failure lost");require(number==1101,"parent cleanup ran before child cleanup or skipped reached cleanup");}
            else {auto first=diagnostic.find("var invalid = 1 / zero"),second=diagnostic.find("var invalid = (value - value - 1) as u8"),trigger=diagnostic.find("::trigger");if(!(first<second && second<trigger))throw std::runtime_error("unobserved cleanup failure order:\n"+diagnostic);require(number==11,"cancellation discarded reached cleanup after a cleanup failure");}
            p.retained.reset();runtime->collect();require(runtime->snapshot().objects==0,"structured failed/cancelled roots retained");
        }
    }

    {
        auto source=fixtures/"main.void";auto text=vd::read_source(repository/"examples/async/lifecycle.void").text();{std::ofstream out(source);out<<text;}
        auto program=analyze(source);vd::Jit jit({fixtures,std::string(vd::compiler_build_identity())});auto first=jit.compile_async(*program,false);require(first->invoke(7)==8,"group cache miss result");const bool supported=jit.cache_status()=="miss";auto hit=jit.compile_async(*program,false);require(hit->invoke(7)==8 && (!supported || jit.cache_status()=="hit"),"group persistent cache hit");
        auto where=text.find("return value + result");require(where!=std::string::npos,"group reload fixture");text.replace(where,21,"return value + result + 1");{std::ofstream out(source);out<<text;}auto edited=analyze(source);vd::ReloadSlot slot(first);require(jit.replace(slot,*edited,false) && slot.invoke(7)==9 && first->invoke(7)==8,"group reload leases");
    }


    // Suspend at deterministic poll gates: no sleeps or timing-based ordering.
    for(bool optimize:{false,true}){
        auto source=fixtures/"cancelled.void";auto text=vd::read_source(repository/"examples/async/lifecycle_cancelled.void").text();{std::ofstream out(source);out<<text;}auto program=analyze(source);vd::Jit jit;auto old=jit.compile_async(*program,optimize);auto runtime=old->runtime_owner();vd::ReloadSlot slot(old);
        managed_test::Probe probe;probe.gate_poll=3;probe.capture=true;probe.inspect_roots=true;probe.frames.reserve(1024);
        auto worker=std::async(std::launch::async,[lease=slot.acquire(),&probe]{managed_test::Observe observe(probe);try{lease->invoke(7);return std::string{};}catch(const std::runtime_error& error){return std::string(error.what());}});
        struct Release{managed_test::Probe& probe;bool done=false;void now(){if(!done){done=true;probe.release.release();}}~Release(){now();}}release{probe};
        require(probe.reached.try_acquire_for(std::chrono::seconds(10)),"cancellation resumption gate");
        auto at=text.find("owner.number += 1");require(at!=std::string::npos,"cancelled reload fixture");text.replace(at,17,"owner.number += 2");{std::ofstream out(source);out<<text;}auto edited=analyze(source);require(jit.replace(slot,*edited,optimize),"cancelled frame reload rejected");old.reset();require(jit.live_generations()==2,"cancelled generation retired before cleanup");
        auto collector=std::async(std::launch::async,[&]{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);return runtime->collect(std::chrono::seconds(10));});
        const bool requested=runtime->wait_for_request(std::chrono::seconds(10));release.now();require(collector.get()==vd::Runtime::Collection::completed && requested,"cancelled suspended GC rendezvous");require(worker.get().find("Async task cancelled")!=std::string::npos && probe.frames.empty() && probe.roots_valid,"cancelled old resumption/cleanup roots");require(jit.live_generations()==1,"cancelled generation retained after drain");
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);std::int32_t number=0;require(bool(probe.retained),"cancelled receiver missing");std::memcpy(&number,probe.retained->get(),sizeof(number));require(number==1,"old generation ran replacement cleanup");probe.retained.reset();runtime->collect();require(runtime->snapshot().objects==0,"cancelled reload retained roots");}
    }
    for(bool optimize:{false,true}){
        auto program=analyze(repository/"examples/async/lifecycle.void");vd::Jit jit;auto code=jit.compile_async(*program,optimize);auto runtime=code->runtime_owner();managed_test::Probe probe;probe.gate_poll=2;probe.inspect_roots=true;probe.frames.reserve(1024);
        auto worker=std::async(std::launch::async,[lease=code,&probe]{managed_test::Observe observe(probe);try{lease->invoke(7);return false;}catch(const std::runtime_error&){return true;}});
        const bool reached=probe.reached.try_acquire_for(std::chrono::seconds(10));if(!reached)probe.release.release();require(reached,"shutdown activation gate");runtime->close();probe.release.release();require(worker.get() && probe.frames.empty() && probe.roots_valid && runtime->snapshot().threads==0,"closing executor did not drain safely");
    }
    // Contextual task groups must preserve ordinary visible-name resolution.
    {auto source=vd::analyze_source({"/bound-tasks.void",
        "value struct factory:\n    number: i32 = 0\nconst tasks = factory()\nstruct resource:\n    number: i32 = 0\nfunc (owner resource) deinit():\n    pass\nfunc (owner factory) group() -> resource:\n    return resource(number = 9)\nasync func entry(value: i32) -> i32:\n    with tasks.group() as owner:\n        return owner.number\n"});require(source.succeeded(),"task group builtin shadowed a visible binding");for(bool optimize:{false,true}){vd::Jit jit;require(jit.compile_async(*source.program,optimize)->invoke(7)==9,"bound tasks.group resource behavior");}}
    // An indirect synchronous cleanup helper must not publish work after drain.
    for(const std::string cleanup:{
        "async func entry(value: i32) -> i32:\n    defer enqueue()\n    return value\n",
        "struct resource:\n    number: i32 = 0\nfunc (owner resource) deinit():\n    enqueue()\nasync func entry(value: i32) -> i32:\n    with resource() as owner:\n        pass\n    return value\n"
    }){auto program=vd::analyze_source({"/cleanup-enqueue.void","async func child():\n    pass\nfunc enqueue():\n    var ignored = child()\n"+cleanup});require(program.succeeded(),"cleanup helper source");for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile_async(*program.program,optimize);bool rejected=false;try{code->invoke(7);}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("cannot enqueue during structured cleanup")!=std::string::npos;}require(rejected,"cleanup published an undrained async child");}}
    for(const std::string text:{
        "async func entry(value: i32) -> i32:\n    with tasks.group() as jobs:\n        var escaped = func() -> u64:\n            return jobs\n    return value\n",
        "func entry(value: i32) -> i32:\n    with tasks.group() as jobs:\n        pass\n    return value\n",
        "async func entry(value: i32) -> i32:\n    with tasks.group() as jobs:\n        var escaped = jobs\n    return value\n",
        "async func entry(value: i32) -> i32:\n    with tasks.group() as jobs:\n        jobs.spawn(value)\n    return value\n",
        "async func entry(value: i32) -> i32:\n    with tasks.group(value) as jobs:\n        pass\n    return value\n",
        "async func leaf(value: i32) -> i32:\n    return value\nasync func entry(value: i32) -> i32:\n    var job = leaf(value)\n    job.cancel(value)\n    return value\n",
        "async func leaf(value: i32) -> i32:\n    return value\nasync func entry(value: i32) -> i32:\n    var job = leaf(value)\n    defer {\n        await job\n    }\n    return value\n"
    })require(!vd::analyze_source({"/invalid-lifecycle.void",text}).succeeded(),"unsupported lifecycle source accepted");
}
