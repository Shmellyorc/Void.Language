#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/cache.hpp"
#include "void/async.hpp"
#include <cstring>
#include "void/reference_capabilities.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <condition_variable>
#include <map>
#include <set>
#include <thread>
#include <stdexcept>
namespace {
void require(bool condition,const char* text){if(!condition)throw std::runtime_error(text);}
auto analyze(const std::filesystem::path& path){auto result=vd::analyze_project(path);if(!result.succeeded())throw vd::CompilationError(result.diagnostics);return result.program;}
char fifo_generation;
std::vector<int> fifo;
struct FifoFrame {void* task;int step,index;};
void fifo_resume(void*,vd::RuntimeFailure*);
void* fifo_create(int index,vd::RuntimeFailure* failure){
    auto& runtime=vd::Runtime::current();void* task=nullptr;void* frame=nullptr;const std::size_t zero=0;
    vd::Runtime::Slot slots[]={{&task,sizeof(task),&zero,1},{&frame,sizeof(frame),&zero,1}};vd::Runtime::Frame roots(runtime,slots,2);
    vd_async_new(&task,&frame,sizeof(FifoFrame),&zero,1,8,nullptr,0,"parallel-fifo-task",&fifo_generation,fifo_resume,failure,index>=0);
    if(failure->message)return nullptr;*static_cast<FifoFrame*>(frame)={task,0,index};vd_async_start(task,failure);return task;
}
bool foreign_rejected=false;
void* fifo_start(std::int32_t,vd::RuntimeFailure* failure){void* task=nullptr;void* frame=nullptr;const std::size_t zero=0;vd::Runtime::Slot slots[]={{&task,sizeof(task),&zero,1},{&frame,sizeof(frame),&zero,1}};vd::Runtime::Frame roots(vd::Runtime::current(),slots,2);char foreign;vd::RuntimeFailure wrong;vd_async_new(&task,&frame,8,nullptr,0,8,nullptr,0,"parallel-fifo-task",&foreign,fifo_resume,&wrong,1);foreign_rejected=wrong.message && !task && !frame;return fifo_create(-1,failure);}
void fifo_resume(void* raw,vd::RuntimeFailure* failure){auto& frame=*static_cast<FifoFrame*>(raw);
    if(frame.index>=0){fifo.push_back(frame.index);vd_async_complete(frame.task);return;}
    if(!frame.step){frame.step=1;for(int n=0;n<24 && !failure->message;++n)fifo_create(n,failure);}
    if(!vd_async_drain(raw,failure))return;vd_async_finish(failure);if(failure->message)return;int count=fifo.size();std::memcpy(static_cast<char*>(frame.task)+4,&count,4);vd_async_complete(frame.task);
}
void fifo_test(){vd::Runtime runtime;runtime.async_worker_limit(1);vd::NominalDescriptor descriptor{"parallel-fifo-task","","plain",{8,{}}};runtime.admit_types({&descriptor,1});fifo.reserve(24);fifo.clear();vd::Runtime::Attachment attachment(runtime);vd::Runtime::Managed managed(runtime);vd::RuntimeFailure failure;require(vd_async_root(fifo_start,0,4,&fifo_generation,&failure)==24 && !failure.message,"worker FIFO drain");require(foreign_rejected,"foreign-generation worker publication accepted");for(int n=0;n<24;++n)require(fifo[n]==n,"single worker admission is not FIFO");runtime.collect();require(runtime.snapshot().objects==0,"FIFO roots retained");}
struct Gate {
    std::thread::id owner;
    std::mutex mutex;std::condition_variable changed;
    std::map<std::thread::id,unsigned> polls;
    std::set<std::thread::id> reached;
    bool released=false,cancel=false;
    unsigned owner_polls=0;
};
Gate* gate=nullptr;
std::thread::id collecting_owner;
std::atomic<unsigned> worker_collections{0};
void collect_poll(){if(std::this_thread::get_id()==collecting_owner)return;if(vd::Runtime::current().collect()!=vd::Runtime::Collection::completed)std::terminate();++worker_collections;}
void fail_poll(){if(std::this_thread::get_id()!=collecting_owner)managed_test::allocation_budget(0);}

void poll(){auto& g=*gate;auto id=std::this_thread::get_id();
    if(id==g.owner){if(!g.cancel)return;auto n=++g.owner_polls;
        if(n==2){vd::Runtime::Native native(vd::Runtime::current());std::unique_lock lock(g.mutex);g.changed.wait(lock,[&]{return !g.reached.empty();});}
        if(n==3){{std::lock_guard lock(g.mutex);g.released=true;}g.changed.notify_all();}return;
    }
    // Third generated CFG poll: wrapper, synchronous target entry, then its loop.
    std::unique_lock lock(g.mutex);if(++g.polls[id]!=3)return;g.reached.insert(id);g.changed.notify_all();lock.unlock();
    vd::Runtime::Native native(vd::Runtime::current());lock.lock();g.changed.wait(lock,[&]{return g.released;});
}
struct Observe {explicit Observe(Gate& g){gate=&g;managed_test::concurrent_poll.store(poll,std::memory_order_release);}~Observe(){managed_test::concurrent_poll.store(nullptr,std::memory_order_release);gate=nullptr;}};
}
void async_parallel_tests(const std::filesystem::path& repository,const std::filesystem::path& fixtures){
    fifo_test();
    std::filesystem::create_directories(fixtures);auto source=fixtures/"main.void";
    auto write=[&](const std::string& text){std::ofstream stream(source);stream<<text;require(bool(stream),"parallel fixture write");};
    auto original=analyze(repository/"examples/async/spawn.void");
    for(bool optimized:{false,true})for(unsigned count:{1u,3u}){vd::Jit jit;auto code=jit.compile_async(*original,optimized);code->runtime_owner()->async_worker_limit(count);require(code->invoke(7)==126,"parallel value/string/group result");}
    {vd::Jit jit;auto code=jit.compile_async(*original,false);code->runtime_owner()->async_worker_limit(2);collecting_owner=std::this_thread::get_id();managed_test::concurrent_poll=collect_poll;auto value=code->invoke(7);managed_test::concurrent_poll=nullptr;require(value==126 && worker_collections>0,"worker private roots under forced collection");}
    {vd::Jit jit;auto code=jit.compile_async(*original,false);code->runtime_owner()->async_worker_limit(1);collecting_owner=std::this_thread::get_id();managed_test::concurrent_poll=fail_poll;bool failed=false;try{code->invoke(7);}catch(const std::runtime_error&){failed=true;}managed_test::concurrent_poll=nullptr;require(failed,"worker allocation failure disappeared");auto runtime=code->runtime_owner();vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);runtime->collect();require(runtime->snapshot().objects==0,"worker allocation failure retained roots");}
    // A real source loop is concurrently active on two distinct Runtime workers.
    // Neither may finish until the controller observes both; no clock/sleep proof.
    const std::string work="func work(value: i32) -> i32:\n    var result = 0\n    for i in 0..<value:\n        result += 1\n    return result\n";
    const std::string root="async func entry(value: i32) -> i32:\n    var first = spawn work(value)\n    var second = spawn work(value + 1)\n    return await first + await second\n";
    write(work+root);auto before=analyze(source);
    for(bool optimized:{false,true}){
        vd::Jit jit({fixtures,std::string(vd::compiler_build_identity())});auto code=jit.compile_async(*before,optimized);auto runtime=code->runtime_owner();runtime->async_worker_limit(2);vd::ReloadSlot slot(code);Gate g;Observe observe(g);
        auto execution=std::async(std::launch::async,[&]{g.owner=std::this_thread::get_id();return code->invoke(100);});
        {std::unique_lock lock(g.mutex);g.changed.wait(lock,[&]{return g.reached.size()==2;});require(g.polls.size()==2,"worker bound exceeded");}
        {vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed,"worker handoff GC rendezvous");}
        write(work+root.substr(0,root.size()-1)+" + 1\n");auto after=analyze(source);require(jit.replace(slot,*after,optimized),"parallel compatible reload rejected");require(jit.live_generations()==2,"running worker lease released early");
        {std::lock_guard lock(g.mutex);g.released=true;}g.changed.notify_all();require(execution.get()==201,"running old worker result");code.reset();require(jit.live_generations()==1,"old worker generation retained after drain");
        require(slot.invoke(7)==16,"replacement worker result");write(work+root);
    }
    // Cancellation is requested by the owner after a worker is demonstrably in
    // its synchronous loop, and before that worker is released to its checkpoint.
    write(work+"async func entry(value: i32) -> i32:\n    var pending = spawn work(value)\n    if value > 0:\n        pending.cancel()\n    return await pending\n");auto cancelling=analyze(source);
    for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*cancelling,optimized);code->runtime_owner()->async_worker_limit(1);Gate g;g.cancel=true;Observe observe(g);g.owner=std::this_thread::get_id();bool cancelled=false;try{code->invoke(100);}catch(const std::runtime_error& error){cancelled=std::string(error.what()).find("Async task cancelled")!=std::string::npos;}require(cancelled && g.reached.size()==1,"running worker cancellation lost");}
    write("func cleanup(value: i32):\n    defer {\n        var second = 11 / (value - value)\n    }\n    var first = 10 / (value - value)\nfunc work(value: i32) -> i32:\n    defer cleanup(value)\n    var result = 0\n    for i in 0..<value:\n        result += 1\n    return result\nasync func entry(value: i32) -> i32:\n    var pending = spawn work(value)\n    if value > 0:\n        pending.cancel()\n    return await pending\n");
    {auto program=analyze(source);for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*program,optimized);code->runtime_owner()->async_worker_limit(1);Gate g;g.cancel=true;Observe observe(g);g.owner=std::this_thread::get_id();bool failed=false;try{code->invoke(100);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("Division by zero")!=std::string::npos && std::string(error.what()).find("Async task cancelled")==std::string::npos && std::string(error.what()).find("Additional failure during defer")!=std::string::npos;}require(failed,"worker cleanup failure must supersede cancellation");}}
    // Closing the Runtime drains an admitted worker; no pointer/code lease can
    // be retired while it still uses its private activation and roots.
    write(work+"async func entry(value: i32) -> i32:\n    return await spawn work(value)\n");auto closing=analyze(source);
    {vd::Jit jit;auto code=jit.compile_async(*closing,false);code->runtime_owner()->async_worker_limit(1);Gate g;Observe observe(g);auto execution=std::async(std::launch::async,[&]{g.owner=std::this_thread::get_id();return code->invoke(100);});{std::unique_lock lock(g.mutex);g.changed.wait(lock,[&]{return g.reached.size()==1;});}code->runtime_owner()->close();{std::lock_guard lock(g.mutex);g.released=true;}g.changed.notify_all();require(execution.get()==100,"close did not drain admitted worker");require(code->runtime_owner()->snapshot().threads==0,"worker attachment leaked");}
    write("func loop(value: i32) -> i32:\n    for:\n        pass\nfunc fail(value: i32) -> i32:\n    return 10 / value\nasync func entry(value: i32) -> i32:\n    var busy = spawn loop(value)\n    var broken = spawn fail(0)\n    return await busy\n");
    {auto program=analyze(source);for(bool optimized:{false,true}){vd::Jit jit;auto code=jit.compile_async(*program,optimized);code->runtime_owner()->async_worker_limit(2);bool failed=false;try{code->invoke(7);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("Division by zero")!=std::string::npos;}require(failed,"worker sibling failure/drain lost");}}
    write("func fail(value: i32) -> i32:\n    return 10 / value\nasync func entry(value: i32) -> i32:\n    var broken = spawn fail(0)\n    for:\n        pass\n");
    {auto program=analyze(source);vd::Jit jit;auto code=jit.compile_async(*program,false);code->runtime_owner()->async_worker_limit(1);bool failed=false;try{code->invoke(7);}catch(const std::runtime_error& error){failed=std::string(error.what()).find("Division by zero")!=std::string::npos;}require(failed,"running owner did not observe worker completion at checkpoint");}
    // Actual source/cache handoff plus allocation-failure rollback.
    {vd::Jit jit({fixtures,std::string(vd::compiler_build_identity())});auto code=jit.compile_async(*original,false);bool persistent=jit.cache_status()=="miss";auto hit=jit.compile_async(*original,false);require(!persistent || jit.cache_status()=="hit","parallel Debug cache did not hit");code->runtime_owner()->async_worker_limit(1);require(code->invoke(7)==126 && hit->invoke(7)==126,"parallel cached object activation");bool completed=false;unsigned failures=0;
        for(int budget=0;budget<512 && !completed;++budget){try{managed_test::AllocationFailure failure(budget);require(code->invoke(7)==126,"allocation retry result");completed=true;}catch(const std::bad_alloc&){++failures;}catch(const std::runtime_error&){++failures;}
            auto runtime=code->runtime_owner();vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"worker failure retained roots");}
        require(completed && failures,"worker allocation sweep incomplete");}
    for(const auto& text:{
        "struct box:\n    n: i32 = 0\nfunc build(value: i32) -> box:\n    return box(n = value)\nasync func entry(value: i32) -> i32:\n    var p = spawn build(value)\n    return value\n",
        "struct box:\n    n: i32 = 0\nvalue struct packet:\n    owner: box\nfunc read(const p: packet) -> i32:\n    return p.owner.n\nasync func entry(value: i32) -> i32:\n    var p = spawn read(packet(owner = box()))\n    return value\n",
        "func id(value: i32) -> i32:\n    return value\nfunc entry(value: i32) -> i32:\n    var p = spawn id(value)\n    return value\n",
        "struct box:\n    n: i32 = 0\nfunc read(const b: box) -> i32:\n    return b.n\nasync func entry(value: i32) -> i32:\n    var p = spawn read(box())\n    return await p\n",
        "func id(value: [i32]) -> i32:\n    return value.count\nasync func entry(value: i32) -> i32:\n    var a: [i32] = [1]\n    return await spawn id(a)\n",
        "async func id(value: i32) -> i32:\n    return value\nasync func entry(value: i32) -> i32:\n    return await spawn id(value)\n",
        "func indirect(value: i32) -> i32:\n    var f = func() -> i32:\n        return value\n    return f()\nasync func entry(value: i32) -> i32:\n    return await spawn indirect(value)\n",
        "async func child(value: i32) -> i32:\n    return value\nfunc hidden(value: i32) -> i32:\n    var t = child(value)\n    return value\nasync func entry(value: i32) -> i32:\n    return await spawn hidden(value)\n"
    })require(!vd::analyze_source({"/invalid-spawn.void",text}).succeeded(),"unsafe worker source accepted");
    {auto invalid=original->lower();for(auto& f:invalid.functions)if(f.name==invalid.entry_symbol)f.worker=true;bool rejected=false;try{vd::ir::validate(invalid);}catch(const std::invalid_argument& error){rejected=std::string(error.what()).find("worker")!=std::string::npos;}require(rejected,"worker suspension/effects accepted by IR");}
    {auto invalid=original->lower();invalid.imports.push_back("unsafe_native");for(auto& f:invalid.functions)if(f.source_name=="pair"){vd::ir::Instruction call{vd::ir::Op::call};call.callee="unsafe_native";call.arguments={0};f.blocks={{{{vd::ir::Op::argument,0},call},{vd::ir::Terminator::Kind::return_value,1}}};}bool rejected=false;try{vd::ir::validate(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"worker unknown native effect accepted by IR");}
    auto module=original->lower();auto worker=std::find_if(module.functions.begin(),module.functions.end(),[](const auto& f){return f.worker;});require(worker!=module.functions.end(),"missing worker typed IR");
    {auto invalid=module;auto index=worker-module.functions.begin();invalid.functions[index].worker=false;invalid.functions[index].asynchronous=false;invalid.functions[index].task=vd::ir::Type::none;bool rejected=false;try{vd::ir::validate(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"malformed worker bypass accepted");}
    {auto invalid=module;for(auto& f:invalid.functions)if(f.worker){f.parameters[0]=f.task;break;}bool rejected=false;try{vd::ir::validate(invalid);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"worker task transfer accepted");}
}
