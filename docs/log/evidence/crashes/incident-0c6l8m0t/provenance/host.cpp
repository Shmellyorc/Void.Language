#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/cache.hpp"
#include "void/console.hpp"
#include "void/detail/filesystem_hooks.hpp"
#include "../managed/probe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>
#include <optional>
#include <semaphore>
#include <thread>
#include <future>
#ifndef _WIN32
#include <sys/stat.h>
#endif
namespace fs=std::filesystem;
using Op=vd::filesystem::Operation;
namespace {
unsigned checks=0;
void check(bool value,const std::string& why){if(!value)throw std::runtime_error(why);++checks;}
std::string read(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const fs::path& path,const std::string& text){std::ofstream f(path,std::ios::binary);f<<text;check(bool(f),"native fixture write");}
std::uint64_t identity(const fs::path& path){
#ifndef _WIN32
    struct stat s{};check(::stat(path.c_str(),&s)==0,"native identity observation");return s.st_ino;
#else
    throw std::runtime_error("Native Windows identity gate requires its platform host implementation");
#endif
}
unsigned descriptors(){
#ifdef __linux__
    return std::distance(fs::directory_iterator("/proc/self/fd"),fs::directory_iterator{});
#else
    return 0;
#endif
}
void clear(const std::string& name){for(int i=0;i<=3;++i)fs::remove_all(name+(i?"."+std::to_string(i):""));}
struct Endpoint:vd::console::Adapter {
    std::string bytes;unsigned calls=0;bool fail=false;std::function<void()> after;
    std::array<vd::console::Endpoint,3> inspect()const override {using namespace vd::console;return {{{Stream::input,Kind::pipe},{Stream::output,Kind::pipe},{Stream::error,Kind::pipe}}};}
    std::chrono::steady_clock::time_point now()const noexcept override{return std::chrono::steady_clock::now();}
    vd::console::Transfer read(std::span<std::uint8_t>,const vd::console::Request&)override{return {0,true,{}};}
    vd::console::Transfer write(vd::console::Stream,std::span<const std::uint8_t> data,const vd::console::Request&)override{
        ++calls;auto n=fail?std::min<std::size_t>(2,data.size()):data.size();bytes.append(reinterpret_cast<const char*>(data.data()),n);
        if(after)after();return {n,false,fail?vd::console::Error{vd::console::ErrorKind::io_failure,87,19,1}:vd::console::Error{}};
    }
};
auto analyze(const fs::path& source){auto a=vd::analyze_project(source);if(!a.succeeded())throw vd::CompilationError(a.diagnostics);check(a.program->warnings().empty(),"source documentation warnings");return a.program;}
auto compile(vd::Jit& jit,const fs::path& source,bool optimize,std::shared_ptr<Endpoint> endpoint={},bool asynchronous=false){
    auto a=analyze(source);auto module=a->lower();module.async_root=asynchronous;vd::ir::validate(module);
    auto runtime=endpoint?std::make_shared<vd::Runtime>(1,vd::console::Streams::isolated(endpoint)):std::make_shared<vd::Runtime>(1);
    return jit.compile(module,module.entry_symbol,optimize,{},runtime);
}
void collect(vd::Runtime& runtime){vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.collect()==vd::Runtime::Collection::completed,"forced collection");}
void transports(const fs::path& root,bool optimize){
    vd::Jit jit;auto endpoint=std::make_shared<Endpoint>();auto code=compile(jit,root/"tests/log/native.void",optimize,endpoint);
    const std::string record="INFO \"\" \"A🔥\\n\"\n";check(record.size()==18,"independent record oracle");
    auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();hooks->write_chunk=1;
    unsigned closes=0,writes=0,metadata=0;hooks->before=[&](Op op){if(op==Op::close)++closes;if(op==Op::write_bytes)++writes;if(op==Op::metadata)++metadata;};
    vd::filesystem::testing::Observe observe(hooks);
    for(int partial=-2;partial<18;++partial){
        clear("native.log");endpoint->bytes.clear();endpoint->fail=false;hooks->zero_write=false;hooks->write_failure_after=-1;
        check(code->invoke(1)==7,"native acquisition");unsigned acquired_metadata=metadata;
        if(partial==-2){check(code->invoke(2)==7,"short-chunk complete record");check(read("native.log")==record,"short-chunk byte oracle");}
        else {
            if(partial==-1)hooks->zero_write=true;else hooks->write_failure_after=partial;
            check(code->invoke(100+std::max(0,partial))==7,"typed partial file receipt");
            const auto before=writes;check(code->invoke(6)==7 && before==writes,"faulted writer retried native transfer");
            check(read("native.log")==record.substr(0,std::max(0,partial)),"partial native bytes differ");
        }
        check(metadata==acquired_metadata,"ordinary record performed metadata I/O");
        check(endpoint->bytes==record,"file error suppressed independently completed console");
        hooks->zero_write=false;hooks->write_failure_after=-1;
        check(code->invoke(0)==7,"partial cleanup");
    }
    clear("native.log");check(code->invoke(1)==7,"two-sink acquisition");endpoint->fail=true;
    check(code->invoke(5)==7,"console failure lost file receipt");endpoint->fail=false;check(read("native.log")==record,"console failure suppressed file");check(code->invoke(6)==7,"console failure did not fault");check(code->invoke(0)==7,"two-sink cleanup");
    clear("native.log");check(code->invoke(1)==7,"retired-close acquisition");auto before=closes;hooks->close_error=5;check(code->invoke(4)==7 && closes==before+1,"retired close retried or lost cause");hooks->close_error=0;check(code->invoke(0)==7,"retired close reset");
    clear("native.log");check(code->invoke(1)==7,"console unwind acquisition");
    std::optional<managed_test::AllocationFailure> fault;endpoint->after=[&]{fault.emplace(0,true);};
    bool failed=false;try{code->invoke(2);}catch(const std::exception&){failed=true;}fault.reset();endpoint->after={};
    check(failed && code->invoke(6)==7,"uncertain console Runtime unwind did not fault");check(read("native.log").empty(),"Runtime unwind unexpectedly entered file");check(code->invoke(0)==7,"console unwind cleanup");
    clear("native.log");check(code->invoke(1)==7,"forced-GC acquisition");
    managed_test::Probe p;p.inspect_roots=true;p.collect_before=true;p.frames.reserve(32768);
    {managed_test::Observe probe(p);check(code->invoke(2)==7,"forced-GC record");}
    check(p.roots_valid && p.commits_valid && p.frames.empty() && p.roots_checked,"logger GC/root publication");check(code->invoke(0)==7,"forced-GC release");collect(*code->runtime_owner());
    std::cout<<"transport optimize="<<optimize<<" partial_boundaries=18 native_closes="<<closes<<" passed\n";
}
void rotations(const fs::path& root,bool optimize){
    vd::Jit jit;auto endpoint=std::make_shared<Endpoint>();auto code=compile(jit,root/"tests/log/native.void",optimize,endpoint);
    auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();unsigned moves=0;int selected=0;bool flushing=false;bool deleting=false;bool opening=false;
    hooks->before=[&](Op op){
        if(op==Op::move_file){++moves;if(selected && moves==unsigned(selected))write("native.log."+std::to_string(4-selected),"collision sentinel\n");}
        if(op==Op::flush && flushing)throw fs::filesystem_error("injected native fsync failure",std::make_error_code(std::errc::io_error));
        if(op==Op::remove_file && deleting){deleting=false;fs::rename("native.log.3","preserved-oldest");fs::create_directory("native.log.3");}
        if(op==Op::open && opening){opening=false;write("native.log","new-active collision\n");}
    };
    vd::filesystem::testing::Observe observe(hooks);
    write("external-sentinel","external contents\n");auto external_id=identity("external-sentinel");
    for(int variant=0;variant<=6;++variant){
        clear("native.log");fs::remove("preserved-oldest");moves=0;selected=0;flushing=false;deleting=false;opening=false;
        check(code->invoke(1)==7,"rotation acquisition");for(int i=0;i<3;++i)check(code->invoke(2)==7,"fill bounded active");
        write("native.log.1","newer\n");write("native.log.2","older\n");write("native.log.3","oldest\n");
        auto active=identity("native.log"),newer=identity("native.log.1"),older=identity("native.log.2");auto active_bytes=read("native.log");
        if(variant<=2)selected=variant+1;
        if(variant==3)opening=true;
        if(variant==4)deleting=true;
        if(variant==5){flushing=true;hooks->close_error=5;}
        if(variant==6){check(code->invoke(2)==7,"successful native rotation");check(identity("native.log.1")==active && identity("native.log.2")==newer && identity("native.log.3")==older,"successful rename changed file identity");}
        else {
            check(code->invoke(8)==7,"rotation did not preserve typed failing boundary");
            auto written=endpoint->calls;check(code->invoke(6)==7 && endpoint->calls==written,"rotation fault allowed another output");
            if(variant<=2){
                check(read("native.log."+std::to_string(3-variant))=="collision sentinel\n","no-replace collision overwritten");
                check(identity("native.log")==active && read("native.log")==active_bytes,"failed rotation changed active identity/content");
                if(variant>=1)check(identity("native.log.3")==older,"first completed rename not preserved");
                if(variant>=2)check(identity("native.log.2")==newer,"second completed rename not preserved");
            }
            if(variant==3){check(identity("native.log.1")==active && read("native.log.1")==active_bytes,"completed active rename not retained");check(read("native.log")=="new-active collision\n","exclusive creation replaced destination");}
            if(variant==4)check(read("preserved-oldest")=="oldest\n" && fs::is_directory("native.log.3") && moves==0,"failed exact delete mutated other paths");
            if(variant==5)check(moves==0 && identity("native.log")==active,"flush/close failure allowed rename");
        }
        flushing=false;hooks->close_error=0;selected=0;check(code->invoke(0)==7,"rotation cleanup");
        check(identity("external-sentinel")==external_id && read("external-sentinel")=="external contents\n","rotation touched external sentinel");
    }
    std::cout<<"rotation optimize="<<optimize<<" boundaries=7 independent_identity_observations passed\n";
}
void allocations(const fs::path& root,bool optimize){
    vd::Jit jit;auto code=compile(jit,root/"tests/log/publication.void",optimize);
    check(code->invoke(0)==7,"initialize default before fault sweep");unsigned failures=0,post_effect=0;bool success=false;
    auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();unsigned writes=0;hooks->before=[&](Op op){if(op==Op::write_bytes)++writes;};vd::filesystem::testing::Observe observe(hooks);
    for(int budget=0;budget<=65536 && !success;budget=budget<64?budget+1:budget*2){
        clear("publication.log");const auto before=descriptors();bool failed=false;
        managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(32768);
        {managed_test::Observe source(p);try{managed_test::AllocationFailure injection(budget,true);check(code->invoke(1)==7,"acquisition allocation success");}catch(const std::bad_alloc&){failed=true;}catch(const std::runtime_error& error){auto text=std::string(error.what());if(text.find("allocation")==std::string::npos && text.find("bad_alloc")==std::string::npos && text.find("String operation failed")==std::string::npos && text.find("UTF-8 text operation failed")==std::string::npos && text.find("Array operation failed")==std::string::npos)throw;failed=true;}}
        check(p.roots_valid && p.commits_valid && p.frames.empty(),"acquisition failure root imbalance");check(code->invoke(0)==7,"acquisition failure cleanup state");collect(*code->runtime_owner());check(descriptors()==before,"failed acquisition lost native ownership");
        if(failed)++failures;else success=true;
    }
    check(success && failures,"acquisition allocation sweep missed failure/success");
    for(int budget: {0,1,2,4,8,16,32,64,128,256,512}){
        clear("publication.log");check(code->invoke(1)==7,"write sweep acquisition");auto before=writes;bool failed=false;
        try{managed_test::AllocationFailure injection(budget,true);check(code->invoke(2)==7,"write allocation success");}catch(const std::bad_alloc&){failed=true;}catch(const std::runtime_error& error){auto text=std::string(error.what());if(text.find("allocation")==std::string::npos && text.find("bad_alloc")==std::string::npos && text.find("String operation failed")==std::string::npos && text.find("UTF-8 text operation failed")==std::string::npos && text.find("Array operation failed")==std::string::npos)throw;failed=true;}
        auto observed=code->invoke(3);check(observed==7 || observed==8,"write failure stranded busy guard");if(failed && writes>before && observed==8)++post_effect;
        check(code->invoke(0)==7,"write sweep cleanup");
    }
    clear("publication.log");check(code->invoke(1)==7,"post-transfer publication acquisition");
    bool transferred=false;std::optional<managed_test::AllocationFailure> late;
    hooks->before=[&](Op op){if(op==Op::write_bytes){++writes;transferred=true;}};
    managed_test::Probe late_probe;late_probe.inspect_roots=true;late_probe.frames.reserve(32768);
    late_probe.before_construct=[&](std::size_t,const void*,std::size_t){if(transferred){transferred=false;late.emplace(0,true);}};
    bool late_failed=false;{managed_test::Observe p(late_probe);try{code->invoke(2);}catch(const std::exception&){late_failed=true;}}late.reset();
    check(late_failed && code->invoke(3)==8 && !read("publication.log").empty(),"post-transfer Runtime failure permitted replay");++post_effect;
    check(late_probe.roots_valid && late_probe.commits_valid && late_probe.frames.empty(),"post-transfer failure roots");
    hooks->before=[&](Op op){if(op==Op::write_bytes)++writes;};check(code->invoke(0)==7,"post-transfer release");
    clear("publication.log");check(code->invoke(1)==7,"builder unwind acquisition");bool unwound=false;try{code->invoke(4);}catch(const std::runtime_error& error){unwound=std::string(error.what()).find("zero")!=std::string::npos;}
    check(unwound && code->invoke(5)==7,"builder unwind retained busy or faulted before transport");check(code->invoke(0)==7,"builder release");
    std::cout<<"allocation optimize="<<optimize<<" acquisition_failures="<<failures<<" post_effect_faults="<<post_effect<<" passed\n";
}
void cancellation(const fs::path& root,bool optimize){
    vd::Jit jit;auto code=compile(jit,root/"tests/log/cancel.void",optimize,{},true);auto runtime=code->runtime_owner();runtime->async_worker_limit(1);
    for(Op blocked:{Op::open,Op::write_bytes,Op::move_file,Op::close}){
        check(code->invoke(0)==7,"reset cancelled source ownership");clear("cancel.log");
        bool armed=false;std::atomic<bool> held=false;std::binary_semaphore entered{0},release{0};unsigned closes=0;
        auto hooks=std::make_shared<vd::filesystem::testing::Hooks>();hooks->before=[&](Op op){if(op==Op::close)++closes;if(armed && op==blocked && !held.exchange(true)){entered.release();release.acquire();}};
        vd::filesystem::testing::Observe observe(hooks);
        if(blocked!=Op::open){check(code->invoke(1)==7,"async cancel acquisition");if(blocked==Op::move_file)for(int i=0;i<2;++i)check(code->invoke(6)==7,"async fill before rotation");}
        managed_test::Probe p;p.inspect_roots=true;p.frames.reserve(32768);bool progressed=false;
        p.before_construct=[&](std::size_t,const void* payload,std::size_t size){if(size!=sizeof(std::int32_t))return;std::int32_t value;std::memcpy(&value,payload,size);if(value!=1234567)return;{vd::Runtime::Native safe(*runtime);check(entered.try_acquire_for(std::chrono::seconds(15)),"cancel did not enter selected native boundary");}progressed=true;release.release();};
        armed=true;bool cancelled=false;{managed_test::Observe source(p);try{code->invoke(blocked==Op::open?2:blocked==Op::close?7:3);}catch(const std::runtime_error& error){cancelled=std::string(error.what()).find("cancelled")!=std::string::npos;}}
        armed=false;check(cancelled && held && progressed,"entered native cancellation was not drained");check(p.roots_valid && p.commits_valid && p.frames.empty(),"cancelled logger root imbalance");
        if(blocked!=Op::open){check(code->invoke(4)==7,"cancelled uncertain transport permitted retry");check(code->invoke(5)==7,"explicit close after cancellation");}
        collect(*runtime);check(closes>=1,"cancelled native ownership not retired");
        std::cout<<"cancel optimize="<<optimize<<" native_operation="<<static_cast<unsigned>(blocked)<<" closes="<<closes<<" passed\n";
    }
}
}
int main(int argc,char** argv){try{check(argc>=3,"root and workspace required");fs::current_path(argv[2]);auto root=fs::absolute(argv[1]);for(bool optimize:{false,true}){transports(root,optimize);rotations(root,optimize);allocations(root,optimize);cancellation(root,optimize);}std::cout<<checks<<" logging native checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
