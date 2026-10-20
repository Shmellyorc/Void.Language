#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include "void/cache.hpp"
#include "void/native_runtime.hpp"
#include "../managed/probe.hpp"
#include "probe.hpp"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <set>

namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
auto analyze(const std::filesystem::path& path){auto a=vd::analyze_project(path,vd::native_fixture_project());if(!a.succeeded())throw vd::CompilationError(a.diagnostics);return a.program;}
void write(const std::filesystem::path& path,const std::string& text){std::ofstream out(path,std::ios::binary);out<<text;require(bool(out),"integrated source write");}
void empty(vd::Runtime& runtime){vd::Runtime::Attachment attached(runtime);vd::Runtime::Managed managed(runtime);require(runtime.collect()==vd::Runtime::Collection::completed && runtime.snapshot().objects==0,"integrated precise roots retained after drain");}
std::thread::id collector_owner;
std::atomic<unsigned> foreign_collections{0};
void collect(){if(std::this_thread::get_id()==collector_owner)return;require(vd::Runtime::current().collect()==vd::Runtime::Collection::completed,"integrated foreign collection");++foreign_collections;}
struct Workers {
    std::thread::id owner;
    std::mutex mutex;std::condition_variable changed;
    std::map<std::thread::id,unsigned> polls;
    std::set<std::thread::id> running;
    unsigned published=0;
    bool released=false,cancelling=false;
    void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
};
// Unwind a failed assertion without stranding the real provider/worker threads.
struct Release {
    Workers& workers;std::binary_semaphore* semaphore=nullptr;bool signalled=false;
    ~Release(){workers.release();if(semaphore && !signalled)semaphore->release();}
};
Workers* workers=nullptr;
void* foreign_identity=nullptr;
bool foreign_rejected=false;
void foreign_enqueue(std::int32_t){vd::RuntimeFailure failure;vd_native_fixture_call(21,static_cast<std::uint32_t>(-7),foreign_identity,nullptr,0,nullptr,nullptr,nullptr,"",&failure);foreign_rejected=failure.message && std::string(failure.message).find("Async activation requires a valid owning root adapter")!=std::string::npos;}

void poll(){auto& g=*workers;auto id=std::this_thread::get_id();if(id==g.owner)return;
    std::unique_lock lock(g.mutex);if(++g.polls[id]!=3)return;g.running.insert(id);g.changed.notify_all();lock.unlock();
    vd::Runtime::Native native(vd::Runtime::current());lock.lock();g.changed.wait(lock,[&]{return g.released;});
}
void published(std::int32_t){auto& g=*workers;if(std::this_thread::get_id()!=g.owner)return;
    vd::Runtime::Native native(vd::Runtime::current());std::unique_lock lock(g.mutex);++g.published;g.changed.notify_all();
    if(g.cancelling){if(g.published==1)g.changed.wait(lock,[&]{return !g.running.empty();});else{g.released=true;g.changed.notify_all();}}
}
struct Observe {
    explicit Observe(Workers& g){workers=&g;managed_test::concurrent_poll=poll;native_test::scalar_poll=published;}
    ~Observe(){native_test::scalar_poll=nullptr;managed_test::concurrent_poll=nullptr;workers=nullptr;}
};
void wait(Workers& g,unsigned jobs){std::unique_lock lock(g.mutex);g.changed.wait(lock,[&]{return !g.running.empty() && g.published>=jobs;});}
}
void native_integrated_tests(const std::filesystem::path& repository,const std::filesystem::path& fixtures){
    std::filesystem::create_directories(fixtures);auto file=fixtures/"main.void",wrapper=fixtures/"wrappers.void";
    const auto original_wrapper=vd::read_source(repository/"examples/native/wrappers.void").text();
    const auto original_text=vd::read_source(repository/"examples/native/integrated.void").text();
    write(wrapper,original_wrapper);write(file,original_text);auto original=analyze(file);
    for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile_async(*original,optimized);code->runtime_owner()->async_worker_limit(2);
        collector_owner=std::this_thread::get_id();foreign_collections=0;managed_test::concurrent_poll=collect;
        auto answer=code->invoke(7);managed_test::concurrent_poll=nullptr;
        require(answer==32 && foreign_collections>0,"combined native/task/worker/callback result or rendezvous");
        require(!vd::native_fixture_live() && code->runtime_owner()->snapshot().threads==0,"combined owner did not drain");empty(*code->runtime_owner());
    }
    for(auto name:{"integrated_cancel","integrated_failure"}){auto program=analyze(repository/"examples/native"/(std::string(name)+".void"));for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile_async(*program,optimized);std::string failure;try{code->invoke(7);}catch(const std::runtime_error& error){failure=error.what();}
        if(std::string(name)=="integrated_cancel")require(failure.find("Async task cancelled")!=std::string::npos,"combined cancellation disappeared");
        else require(failure.starts_with("native text cannot contain NUL") && failure.find("Additional failure during defer")!=std::string::npos && failure.find("Native resource is closed or invalid")!=std::string::npos,"combined native parent/cleanup precedence");
        require(!vd::native_fixture_live() && code->runtime_owner()->snapshot().threads==0,"combined failure leaked owner/worker");empty(*code->runtime_owner());
    }}
    // Caller allocation failures span task/worker publication, retained C storage,
    // callback registration, async cleanup and returned buffers in one Runtime.
    {vd::Jit jit;auto code=jit.compile_async(*original,false);bool complete=false;unsigned failures=0;for(int budget=0;budget<1536 && !complete;++budget){try{managed_test::AllocationFailure fault(budget);require(code->invoke(7)==32,"combined allocation retry");complete=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}require(!vd::native_fixture_live() && code->runtime_owner()->snapshot().threads==0,"combined failed allocation leaked native owner/participant");empty(*code->runtime_owner());}require(complete && failures,"combined allocation sweep incomplete");}
    // Owner requests cancellation only after the worker is inside its real loop;
    // the second compiled-C boundary releases it after the request is published.
    const std::string work="func work(value: i32) -> i32:\n    var total = 0\n    for i in 0..<value:\n        total += 1\n    return total\n";
    write(file,"use \"/wrappers.void\" as library\nuse \"/native_bridge.void\" as bridge\n"+work+
        "async func entry(value: i32) -> i32:\n    with library.open() as owner:\n        owner.connect(value)\n        owner.retain(\"running cancellation\")\n        var pending = spawn work(value)\n        var started = bridge.scalar_i32(value)\n        pending.cancel()\n        var released = bridge.scalar_i32(value)\n        return await pending\n");
    auto cancelled=analyze(file);for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*cancelled,optimized);code->runtime_owner()->async_worker_limit(1);Workers gate;gate.cancelling=true;gate.owner=std::this_thread::get_id();Observe observe(gate);bool rejected=false;try{code->invoke(100);}catch(const std::runtime_error& error){rejected=std::string(error.what()).find("Async task cancelled")!=std::string::npos;}require(rejected && gate.running.size()==1 && !vd::native_fixture_live(),"running worker cancellation released native owner prematurely");empty(*code->runtime_owner());}
    // Registered callbacks outlive an old suspended root and its queued/running
    // workers. Invoke/unregister on the original resource owner after code reset.
    const auto prefix=original_text.substr(0,original_text.find("async func entry"));
    const std::string persistent=prefix+"async func entry(value: i32) -> i32:\n    var owner = library.open()\n    owner.connect(value)\n    owner.retain(\"old retained bytes\")\n    var first = step(value)\n    var second = step(value + 1)\n    var answer = await first + await second\n    return owner.invoke(answer)\n";
    for(bool optimized:{false,true}){
        write(wrapper,original_wrapper);write(file,persistent);auto before=analyze(file);auto cache=fixtures/"reload-cache";std::filesystem::remove_all(cache);std::filesystem::create_directories(cache);vd::Jit jit({cache,std::string(vd::compiler_build_identity())});auto code=jit.compile_async(*before,optimized);bool cache_supported=!optimized && jit.cache_status()=="miss";if(!optimized){auto hit=jit.compile_async(*before,false);require(jit.cache_status()==(cache_supported?"hit":"unavailable"),"combined cached old root did not hit");code=std::move(hit);}auto runtime=code->runtime_owner();runtime->async_worker_limit(1);vd::ReloadSlot slot(code);
        Workers gate;Observe observe(gate);std::binary_semaphore finished{0},retire{0};int old_result=0;bool old_callback=false;
        auto execution=std::async(std::launch::async,[&]{gate.owner=std::this_thread::get_id();old_result=code->invoke(7);finished.release();retire.acquire();vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);vd::Runtime::Handle identity(*runtime,vd::native_fixture_identity(*runtime));vd::RuntimeFailure failure;auto value=vd_native_fixture_call(21,2,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);old_callback=!failure.message && value==9;vd_native_fixture_call(23,0,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);vd_native_fixture_call(15,0,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(!failure.message,"old combined native close");});
        Release release{gate,&retire};
        wait(gate,2);require(vd::native_fixture_live()==1,"old owner missing while worker queue is held");std::size_t retained=0;{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"combined suspended GC");retained=runtime->snapshot().objects;}
        auto changed_wrapper=original_wrapper;auto at=changed_wrapper.find("return owner.bias + value");changed_wrapper.replace(at,strlen("return owner.bias + value"),"return owner.bias + value + 100");write(wrapper,changed_wrapper);
        auto edited=persistent;at=edited.find("    return owner.invoke(answer)");edited.replace(at,strlen("    return owner.invoke(answer)"),"    var again = await first\n    return owner.invoke(answer + again - value - 1) + 1");write(file,edited);auto after=analyze(file);
        require(jit.replace(slot,*after,optimized) && jit.live_generations()==2,"combined live replacement failed");
        write(file,edited+"\nstruct incompatible:\n    field: i32 = 0\n");auto incompatible=analyze(file);require(!jit.replace(slot,*incompatible,optimized),"combined incompatible layout accepted");write(file,edited);
        gate.release();finished.acquire();require(old_result==24 && retained>0,"old async/worker frame migrated");code.reset();require(jit.live_generations()==2,"native callback lease ended with async root");release.signalled=true;retire.release();execution.get();require(old_callback && jit.live_generations()==1 && !vd::native_fixture_live(),"old callback migration/retirement");
        // A switch to a with-generated capture shape is a restart, not a body
        // replacement. Keep the original public/capture contracts for repeats.
        managed_test::concurrent_poll=nullptr;native_test::scalar_poll=nullptr;
        write(wrapper,original_wrapper);write(file,original_text);auto clean=analyze(file);require(!jit.replace(slot,*clean,optimized),"combined replacement changed cleanup capture contract");
        write(wrapper,changed_wrapper);
        for(int n=0;n<4;++n){auto text=edited;auto end=text.find("return owner.invoke(answer + again - value - 1) + 1");text.replace(end,strlen("return owner.invoke(answer + again - value - 1) + 1"),"return owner.invoke(answer + again - value - 1) + "+std::to_string(n));write(file,text);auto next=analyze(file);require(jit.replace(slot,*next,optimized) && slot.invoke(7)==124+n && jit.live_generations()==1,"repeated combined replacement retained old code");{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);vd::Runtime::Handle identity(*runtime,vd::native_fixture_identity(*runtime));vd::RuntimeFailure failure;vd_native_fixture_call(23,0,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);vd_native_fixture_call(15,0,identity.get(),nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(!failure.message,"repeated owner cleanup");}empty(*runtime);}
        std::cout<<"combined cached old root="<<cache_supported<<"; old roots at barrier="<<retained<<"; retired generations=1; post-drain objects=0\n";
    }
    write(wrapper,original_wrapper);
    // Worker is held while a real foreign C dispatch is reserved before Runtime
    // admission. Shutdown must unregister/quiesce before freeing that provider.
    write(file,"use \"/wrappers.void\" as library\nuse \"/native_bridge.void\" as bridge\n"+work+
        "async func entry(value: i32) -> i32:\n    with library.open() as owner:\n        owner.connect(value)\n        var pending = spawn work(value)\n        var started = bridge.scalar_i32(value)\n        return owner.foreign(5) + await pending\n");
    auto closing=analyze(file);for(bool optimized:{false,true}){
        vd::Jit jit;auto code=jit.compile_async(*closing,optimized);auto runtime=code->runtime_owner();runtime->async_worker_limit(1);Workers worker;Observe observe(worker);native_test::Gate native;native_test::gate=&native;
        auto run=std::async(std::launch::async,[lease=code,&worker,&native]{worker.owner=native.owner=std::this_thread::get_id();try{lease->invoke(7);return false;}catch(const std::runtime_error& error){return std::string(error.what()).find("Native callback admission")!=std::string::npos;}});
        std::future<void> close;Release release{worker,&native.release};
        native.reached.acquire();wait(worker,1);{vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"pre-admission callback plus worker GC");}
        code.reset();close=std::async(std::launch::async,[&]{runtime->close();});native.unregistered.acquire();require(vd::native_fixture_live()==1 && jit.live_generations()==1,"reserved callback storage/code freed before admission drain");release.signalled=true;native.release.release();close.get();require(!vd::native_fixture_live() && jit.live_generations()==1,"held worker lost root generation at native shutdown");worker.release();require(run.get(),"combined shutdown lost native failure");native_test::gate=nullptr;require(runtime->snapshot().threads==0 && jit.live_generations()==0,"combined shutdown retained participant/code");
    }
    // A leased old C callback may run on its code, but cannot enqueue that old
    // generation's task into a replacement's active executor.
    auto deferred=original_wrapper;auto point=deferred.find("    return owner.bias + value");deferred.replace(point,strlen("    return owner.bias + value"),"    if value < 0:\n        var ignored = later(value)\n    return owner.bias + value");deferred+="\nasync func later(value: i32) -> i32:\n    return value\n";
    const std::string native_imports="use \"/wrappers.void\" as library\nuse \"/native_bridge.void\" as bridge\n";
    for(bool optimized:{false,true}){write(wrapper,deferred);const std::string body="async func entry(value: i32) -> i32:\n    var forwarded = bridge.scalar_i32(value)\n    var owner = library.open()\n    owner.connect(value)\n    var answer = owner.invoke(1)\n    if value > 0:\n        owner.close()\n    return forwarded + answer";write(file,native_imports+body+"\n");auto before=analyze(file);vd::Jit jit;auto old=jit.compile_async(*before,optimized);require(old->invoke(-7)==-13,"foreign native generation setup");auto runtime=old->runtime_owner();vd::ReloadSlot slot(old);{vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);foreign_identity=vd::native_fixture_identity(*runtime);}write(file,native_imports+body+" + 1\n");auto after=analyze(file);require(jit.replace(slot,*after,optimized),"foreign native generation replacement");old.reset();require(jit.live_generations()==2,"foreign native lease retired early");foreign_rejected=false;native_test::scalar_poll=foreign_enqueue;auto value=slot.invoke(7);native_test::scalar_poll=nullptr;require(value==16 && foreign_rejected,"old callback published foreign task into replacement root");{vd::Runtime::Attachment attached(*runtime);vd::Runtime::Managed managed(*runtime);vd::RuntimeFailure failure;vd_native_fixture_call(23,0,foreign_identity,nullptr,0,nullptr,nullptr,nullptr,"",&failure);vd_native_fixture_call(15,0,foreign_identity,nullptr,0,nullptr,nullptr,nullptr,"",&failure);require(!failure.message,"foreign native lease release");}foreign_identity=nullptr;require(jit.live_generations()==1 && !vd::native_fixture_live(),"foreign native generation did not retire");empty(*runtime);}
    write(wrapper,original_wrapper);
    // Tooling has no trust descriptor: valid async/spawn snapshots remain useful,
    // but importing the native fixture cannot silently authorize it in the LSP.
    const std::string plain="func work(value: i32) -> i32:\n    return value + 1\nasync func entry(value: i32) -> i32:\n    return await spawn work(value)\n";
    write(file,plain);require(vd::read_source(file).text()==plain,"integrated LF fixture bytes changed");vd::ToolingSession session(file);auto snapshot=session.select();auto good=vd::ToolingSession::analyze(snapshot);require(good->result().succeeded() && session.accept(good),"combined tooling baseline");auto definition=session.current_index()->definition_at("/main.void",plain.rfind("work(value)"));require(definition.has_value(),"spawn source reference missing");
    session.open("/main.void",1,"use \"/native_bridge.void\" as bridge\n"+plain);auto bad=vd::ToolingSession::analyze(session.select());require(!bad->result().succeeded() && session.accept(bad) && !session.current_index(),"tooling silently authorized native fixture");session.replace("/main.void",2,plain);auto repaired=vd::ToolingSession::analyze(session.select());require(!session.accept(good) && !session.accept(bad) && repaired->result().succeeded() && session.accept(repaired) && session.current_index(),"combined tooling stale result or repair");require(good->index()->definition_at("/main.void",plain.rfind("work(value)")).has_value(),"immutable historical source reference changed");
    // Disk fixtures and queries use exact bytes. Reproduce Windows text-mode
    // translation explicitly so this offset regression also runs on LF hosts.
    require(definition->source.source->text()==plain,"integrated LF analysis bytes changed");
    std::string crlf;for(char c:plain){if(c=='\n')crlf+='\r';crlf+=c;}
    write(file,crlf);require(vd::read_source(file).text()==crlf,"integrated CRLF fixture bytes changed");
    vd::ToolingSession crlf_session(file);auto crlf_result=vd::ToolingSession::analyze(crlf_session.select());
    require(crlf_result->result().succeeded() && crlf_session.accept(crlf_result),"integrated CRLF tooling baseline");
    require(!crlf_session.current_index()->definition_at("/main.void",plain.rfind("work(value)")),"LF offset unexpectedly matched CRLF reference");
    auto crlf_definition=crlf_session.current_index()->definition_at("/main.void",crlf.rfind("work(value)"));
    require(crlf_definition && crlf_definition->identity==definition->identity && crlf_definition->source.source->text()==crlf,"integrated CRLF source reference or snapshot bytes changed");
    for(const auto& source:{
        "use \"/wrappers.void\" as library\nasync func task(owner: library.resource) -> i32:\n    return 1\nfunc entry(value: i32) -> i32:\n    return value\n",
        "use \"/wrappers.void\" as library\nfunc worker(value: i32) -> i32:\n    with library.open() as owner:\n        return owner.invoke(value)\nasync func entry(value: i32) -> i32:\n    return await spawn worker(value)\n"
    }){write(file,source);require(!vd::analyze_project(file,vd::native_fixture_project()).succeeded(),"unsupported integrated native/task/worker combination accepted");}
    require(!vd::native_fixture_live(),"integrated native resources remain");
}
