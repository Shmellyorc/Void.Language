#include "void/console.hpp"
#include "void/runtime.hpp"
#include "console_posix.hpp"
#include "console_windows_native.hpp"
#include <atomic>
#include <barrier>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <semaphore>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#endif
using namespace vd::console;
using namespace std::chrono_literals;
namespace {
std::atomic<unsigned> checks=0;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);++checks;}
template<class F> void until(F f){auto end=std::chrono::steady_clock::now()+10s;while(!f()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("console transfer synchronization deadline");std::this_thread::yield();}}
Bytes bytes(std::string_view s){return {s.begin(),s.end()};}
struct Step {Transfer result;Bytes data{};std::int64_t advance=0;};
struct Script:Adapter {
    std::deque<Step> steps;
    Bytes output;
    std::vector<Request> requests;
    std::atomic<std::int64_t> tick=0;
    std::function<void()> action;
    std::array<Endpoint,3> inspect() const override{return {{{Stream::input,Kind::pipe},{Stream::output,Kind::file},{Stream::error,Kind::pipe}}};}
    std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::time_point(std::chrono::milliseconds(tick.load()));}
    Transfer next(std::span<std::uint8_t> target,std::span<const std::uint8_t> source,const Request& request){
        requests.push_back(request);if(action)action();
        if(steps.empty())throw std::logic_error("unexpected scripted console transfer");
        auto step=steps.front();steps.pop_front();tick+=step.advance;
        if(!target.empty()){check(step.data.size()<=target.size(),"bad test input");std::copy(step.data.begin(),step.data.end(),target.begin());}
        if(!source.empty()){auto count=std::min(step.result.completed,source.size());output.insert(output.end(),source.begin(),source.begin()+count);}
        return step.result;
    }
    Transfer read(std::span<std::uint8_t> target,const Request& request) override{return next(target,{},request);}
    Transfer write(Stream,std::span<const std::uint8_t> source,const Request& request) override{return next({},source,request);}
};
struct Harness {
    std::shared_ptr<Script> adapter=std::make_shared<Script>();
    vd::Runtime runtime{0,Streams::isolated(adapter)};
    std::shared_ptr<Context> context=runtime.console_context();
};
void progress(){
    Harness h;
    h.adapter->steps={{{2,false,{}},{}},{{0,false,{ErrorKind::interrupted,4}},{}},{{1,false,{}},{}},{{2,false,{}},{}}};
    auto written=h.context->write(Stream::error,bytes("abcde"));
    check(!written.error && written.completed==5 && !written.eof && h.adapter->output==bytes("abcde"),"short/interrupt write completion");
    check(h.adapter->requests.size()==4 && !h.adapter->requests[0].deadline,"infinite request changed");
    h.adapter->steps={{{2,false,{ErrorKind::io_failure,28,9}},bytes("xy")}};
    auto input=h.context->read(8);
    check(input.bytes==bytes("xy") && input.transfer.completed==2 && input.transfer.error.os_code==28 && input.transfer.error.cleanup_code==9,"read error lost partial bytes/native/cleanup code");
    h.adapter->steps={{{1,false,{}},bytes("z")}};
    input=h.context->read(8);check(input.bytes==bytes("z") && !input.transfer.error && !input.transfer.eof,"short read waited to fill");
    h.adapter->steps={{{0,true,{}},{}}};input=h.context->read(8);check(input.bytes.empty() && input.transfer.eof && !input.transfer.error,"EOF became failure");
    h.adapter->steps={{{0,false,{}},{}}};written=h.context->write(Stream::output,bytes("x"));check(written.error.kind==ErrorKind::io_failure && !written.completed,"zero-progress write accepted");
    h.adapter->steps={{{0,false,{}},{}}};input=h.context->read(2);check(input.transfer.error.kind==ErrorKind::io_failure,"zero input without EOF accepted");
    h.adapter->steps={{{2,false,{}},{}},{{1,false,{ErrorKind::broken_pipe,32}}, {}}};written=h.context->write(Stream::output,bytes("four"));
    check(written.completed==3 && written.error.kind==ErrorKind::broken_pipe && written.error.os_code==32,"partial failing output count lost");
    h.adapter->steps={{{0,false,{ErrorKind::interrupted,4,22}},{}}};written=h.context->write(Stream::output,bytes("x"));
    check(written.error.kind==ErrorKind::interrupted && written.error.os_code==4 && written.error.cleanup_code==22,"cleanup failure disappeared during retry");
    auto count=h.adapter->requests.size();
    check(h.context->read(0).bytes.empty() && !h.context->write(Stream::output,{}).error && h.adapter->requests.size()==count,"zero request touched adapter");
    check(h.context->read(-1).transfer.error.kind==ErrorKind::invalid_argument && h.context->read(1,-2).transfer.error.kind==ErrorKind::invalid_argument,"invalid read count/timeout");
    check(h.context->write(Stream::input,bytes("x")).error.kind==ErrorKind::invalid_argument && h.context->write(Stream::output,bytes("x"),-2).error.kind==ErrorKind::invalid_argument,"invalid write endpoint/timeout");
    h.adapter->action=[]{throw std::bad_alloc();};
    bool failed=false;try{h.context->read(1);}catch(const std::bad_alloc&){failed=true;}
    check(failed && h.context->snapshot().admitted==0,"native allocation failure converted to IO error or leaked lease");
}
void deadlines(){
    Harness h;
    h.adapter->steps={{{2,false,{}},{},4},{{0,false,{ErrorKind::interrupted,4}},{},4},{{1,false,{}},{},4}};
    auto result=h.context->write(Stream::output,bytes("abcde"),10);
    check(result.completed==3 && result.error.kind==ErrorKind::timed_out,"partial writes restarted deadline");
    check(h.adapter->requests.size()==3,"deadline allowed extra transfer");
    for(const auto& request:h.adapter->requests)check(request.deadline==h.adapter->requests.front().deadline,"deadline changed across retries");
    h.adapter->steps={{{0,false,{ErrorKind::interrupted,123}},{},10}};
    auto input=h.context->read(8,10);check(input.transfer.error.kind==ErrorKind::timed_out && input.transfer.error.os_code==123,"interrupted read timeout lost native code");
    h.adapter->steps={{{0,false,{ErrorKind::would_block,11}},{}},{{1,false,{}},bytes("x")}};
    input=h.context->read(8,10);check(input.bytes==bytes("x") && !input.transfer.error,"readiness race not retried");
    h.adapter->steps={{{1,false,{}},{}},{{0,false,{ErrorKind::would_block,11}},{}}};
    result=h.context->write(Stream::output,bytes("xy"),0);check(result.completed==1 && result.error.kind==ErrorKind::would_block && result.error.os_code==11,"poll lost output prefix");
    h.adapter->steps={{{0,false,{ErrorKind::interrupted,4}},{}}};
    input=h.context->read(1,0);check(input.transfer.error.kind==ErrorKind::interrupted && !input.transfer.eof,"poll interrupt incorrectly restarted");
    h.adapter->steps={{{1,false,{}},bytes("m")}};
    input=h.context->read(1,INT64_MAX);check(input.bytes==bytes("m") && h.adapter->requests.back().deadline==std::chrono::steady_clock::time_point::max(),"large timeout overflow");
}
void cancellation(){
    Harness h;
    h.adapter->steps={{{2,false,{}},{}}};h.adapter->action=[&]{h.context->begin_close();};
    auto result=h.context->write(Stream::output,bytes("abcd"));
    check(result.completed==2 && result.error.kind==ErrorKind::closed,"cancellation lost completed prefix");
    check(h.context->read(2).transfer.error.kind==ErrorKind::closed && h.context->write(Stream::output,bytes("x")).error.kind==ErrorKind::closed,"closed session admitted IO");
    Harness completed;completed.adapter->steps={{{2,false,{}},bytes("ok")}};completed.adapter->action=[&]{completed.context->begin_close();};
    auto input=completed.context->read(8);check(input.bytes==bytes("ok") && !input.transfer.error,"successful entered read discarded on cancellation");
    Harness output;output.adapter->steps={{{2,false,{}},{}}};output.adapter->action=[&]{output.context->begin_close();};
    result=output.context->write(Stream::output,bytes("ok"));check(result.completed==2 && !result.error,"fully completed write incorrectly failed cancellation race");
}
void admission(){
    auto adapter=std::make_shared<Script>();auto streams=Streams::isolated(adapter);
    vd::Runtime first(0,streams),second(0,streams);auto a=first.console_context(),b=second.console_context();
    auto held=a->acquire(Stream::output);
    auto polled=std::async(std::launch::async,[&]{return b->write(Stream::error,bytes("x"),0);}).get();
    check(polled.error.kind==ErrorKind::would_block && b->snapshot().admitted==0 && adapter->requests.empty(),"poll acquired occupied gate");
    // Real clock for a condition-variable wait; injected transfer clocks above
    // prove that the deadline is never renewed per short transfer.
    struct RealClock final:Script {std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::now();}};
    auto real=std::make_shared<RealClock>();auto other=Streams::isolated(real);vd::Runtime r1(0,other),r2(0,other);auto c=r1.console_context(),d=r2.console_context();auto gate=c->acquire(Stream::output);
    auto timed=std::async(std::launch::async,[&]{return d->write(Stream::output,bytes("x"),10);}).get();
    check(timed.error.kind==ErrorKind::timed_out && d->snapshot().admitted==0 && real->requests.empty(),"queued wait ignored deadline");
    auto queued=std::async(std::launch::async,[&]{return b->write(Stream::output,bytes("x"));});until([&]{return b->snapshot().admitted==1;});
    second.close();auto cancelled=queued.get();check(cancelled.error.kind==ErrorKind::closed && !held.cancelled() && b->snapshot().admitted==0,"queued cancellation changed active other-session lease");
}
void records(){
    struct Chunked final:Adapter {
        Bytes data;std::atomic<unsigned> inside=0,peak=0;
        std::array<Endpoint,3> inspect() const override{return {{{Stream::input,Kind::pipe},{Stream::output,Kind::pipe},{Stream::error,Kind::pipe}}};}
        std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::now();}
        Transfer write(Stream,std::span<const std::uint8_t> bytes,const Request&) override {auto n=++inside;auto p=peak.load();while(p<n && !peak.compare_exchange_weak(p,n)){}data.push_back(bytes[0]);std::this_thread::yield();--inside;return {1,false,{}};}
        Transfer read(std::span<std::uint8_t> bytes,const Request&) override {auto n=++inside;auto p=peak.load();while(p<n && !peak.compare_exchange_weak(p,n)){}bytes[0]=42;std::this_thread::yield();--inside;return {1,false,{}};}
    };
    auto adapter=std::make_shared<Chunked>();vd::Runtime runtime(0,Streams::isolated(adapter));auto c=runtime.console_context();std::barrier start(3);
    auto one=std::async(std::launch::async,[&]{start.arrive_and_wait();return c->write(Stream::output,Bytes(512,'a'));});
    auto two=std::async(std::launch::async,[&]{start.arrive_and_wait();return c->write(Stream::error,Bytes(512,'b'));});start.arrive_and_wait();
    check(one.get().completed==512 && two.get().completed==512,"concurrent record lost bytes");
    Bytes ab(512,'a');ab.insert(ab.end(),512,'b');Bytes ba(512,'b');ba.insert(ba.end(),512,'a');
    check((adapter->data==ab || adapter->data==ba) && adapter->peak==1,"stdout/stderr short transfers interleaved records");
    auto reader=[&]{for(unsigned n=0;n<64;++n){auto input=c->read(8);check(input.bytes==Bytes{42} && !input.transfer.error,"concurrent input lost bytes");}};
    auto r1=std::async(std::launch::async,reader),r2=std::async(std::launch::async,reader);r1.get();r2.get();check(adapter->peak==1,"concurrent reader transfer escaped input gate");
}
void capabilities(){
    Harness h;bool rejected=false;
    {auto op=h.context->acquire(Stream::input);Bytes data(1);try{op.read(data,Request{});}catch(const std::logic_error&){rejected=true;}check(rejected,"forged cancellation token accepted");}
    h.adapter->steps={{{2,false,{}},bytes("a")}};rejected=false;try{h.context->read(1);}catch(const std::logic_error&){rejected=true;}check(rejected,"forged adapter progress accepted");
    vd::Runtime::Attachment a(h.runtime);vd::Runtime::Managed m(h.runtime);rejected=false;try{h.context->read(1);}catch(const std::logic_error&){rejected=true;}check(rejected,"Managed native read accepted");
}
#ifndef _WIN32
using console_native_test::flags;
using console_native_test::flags_unchanged;
void descriptor_invariants(){
    console_native_test::Flags before{},after{};before.values={0,0,0,1,0,1};after=before;
    check(console_native_test::same_flags(before,after),"identical descriptor snapshot rejected");
    for(int bit:{O_NONBLOCK,O_APPEND,O_ACCMODE}){after=before;after.values[3]^=bit;check(!console_native_test::same_flags(before,after),"configured status flag mutation accepted");}
    after=before;after.values[2]^=FD_CLOEXEC;check(!console_native_test::same_flags(before,after),"descriptor ownership flag mutation accepted");
    after=before;after.errors[1]=EBADF;check(!console_native_test::same_flags(before,after),"descriptor native error mutation accepted");
#ifdef __APPLE__
    after=before;after.values[3]|=0x10000;check(console_native_test::same_flags(before,after),"Darwin host write bookkeeping mistaken for a configured flag");
    check(!console_native_test::same_flags(after,before),"Darwin write bookkeeping unexpectedly cleared");
#endif
}

void report(const char* path,const Transfer& result){std::ofstream out(path);out<<result.completed<<' '<<result.eof<<' '<<static_cast<int>(result.error.kind)<<' '<<result.error.os_code<<' '<<result.error.cleanup_code<<' '<<checks<<'\n';check(bool(out),"probe report failed");}
void native_records(const char* path,bool reading){
    auto before=flags();vd::Runtime first,second;auto a=first.console_context(),b=second.console_context();
    std::barrier start(3);Bytes one,two;
    auto work=[&](std::shared_ptr<Context> c,Stream stream,std::uint8_t letter,Bytes& received){
        start.arrive_and_wait();
        if(reading){for(unsigned n=0;n<100;++n){auto read=c->read(1);check(!read.transfer.error && read.bytes.size()==1,"concurrent native read");received.push_back(read.bytes[0]);}}
        else {Bytes data(8193,letter);for(unsigned n=0;n<64;++n){data[0]=static_cast<std::uint8_t>(n);auto write=c->write(stream,data);check(!write.error && write.completed==data.size(),"concurrent native record");}}
    };
    auto left=std::async(std::launch::async,work,a,Stream::output,'A',std::ref(one));
    auto right=std::async(std::launch::async,work,b,Stream::error,'B',std::ref(two));start.arrive_and_wait();left.get();right.get();
    if(reading){one.insert(one.end(),two.begin(),two.end());check(one.size()==200,"native reader counts");auto output=a->write(Stream::output,std::move(one));check(!output.error && output.completed==200,"native reader output");}
    first.close();second.close();check(flags_unchanged(before,path),"concurrent IO changed flags");report(path,{reading?200u:128u*8193u,false,{}});
}
void copy(const char* path,std::int64_t expected){
    auto before=flags();vd::Runtime runtime;auto c=runtime.console_context();std::size_t total=0;bool eof=false;
    while(expected<0 || total<static_cast<std::uint64_t>(expected)){
        auto input=c->read(expected<0?8192:std::min<std::int64_t>(8192,expected-total));check(!input.transfer.error,"native copy read failed");
        if(input.transfer.eof){eof=true;break;}
        total+=input.bytes.size();auto out=c->write(Stream::output,std::move(input.bytes));check(!out.error && out.completed==input.transfer.completed,"native copy write failed");
    }
    auto error=c->write(Stream::error,Bytes{'E',0,255});check(!error.error && error.completed==3,"stderr binary write failed");
    runtime.close();check(flags_unchanged(before,path),"borrowed flags changed by transfers");report(path,{total,eof,{}});
}
#ifdef __linux__
bool in_poll(pid_t tid){std::ifstream in("/proc/self/task/"+std::to_string(tid)+"/syscall");long call=-1;in>>call;return in && call==SYS_poll;}
#endif
void cancel_native(const char* path,bool writing,bool missing_input=false){
    auto before=flags();vd::Runtime runtime;auto c=runtime.console_context();std::atomic<bool> started=false;Transfer result;std::atomic<long> tid=0;
    int initial_write_poll=-1;short initial_write_revents=0;
    if(writing){
        pollfd descriptor{STDOUT_FILENO,POLLOUT,0};
        do{initial_write_poll=poll(&descriptor,1,0);}while(initial_write_poll<0 && errno==EINTR);
        initial_write_revents=descriptor.revents;
        check(initial_write_poll==1 && (initial_write_revents&POLLOUT) && !(initial_write_revents&(POLLERR|POLLHUP|POLLNVAL)),"native output pipe not initially writable");
    }
    auto blocked=std::async(std::launch::async,[&]{
        vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);void* root=nullptr;std::size_t offset=0;vd::Runtime::Slot slot{&root,sizeof(root),&offset,1};vd::Runtime::Frame f(runtime,&slot,1);runtime.allocate_into(&root,{8,{}});
        {vd::Runtime::Native native(runtime);
#ifdef __linux__
            tid=syscall(SYS_gettid);
#endif
            started=true;
            if(writing)result=c->write(Stream::output,Bytes(1024*1024,'W'));
            else result=c->read(16).transfer;
        }
        check(runtime.contains(root),"blocked native operation lost frozen root");
    });
    until([&]{return started && c->snapshot().admitted==1;});
    // Do not let /proc inspection itself temporarily occupy fd 0 before the
    // output wait creates its wake pipe. fcntl observes without opening an fd.
    if(missing_input)until([&]{return fcntl(STDIN_FILENO,F_GETFD)>=0;});
#ifdef __linux__
    until([&]{return in_poll(static_cast<pid_t>(tid.load()));});
#endif
    int write_poll=-1;short write_revents=0;
    if(writing){
        // The parent keeps this fresh pipe's reader open and does not drain it
        // until we finish. Not-writable therefore proves actual native writes
        // filled it. Admission (or the first readiness poll) alone proves none.
        // poll observes only: it creates no fd and cannot reopen missing stdin.
        until([&]{pollfd descriptor{STDOUT_FILENO,POLLOUT,0};write_poll=poll(&descriptor,1,0);write_revents=descriptor.revents;
            if(write_poll<0 && errno==EINTR)return false;
            if(write_poll<0 || (write_revents&(POLLERR|POLLHUP|POLLNVAL)))check(false,"native output progress poll failed");
            return write_poll==0;
        });
    }
    Transfer absent;
    if(missing_input)absent=c->read(1,0).transfer;
    {vd::Runtime::Attachment a(runtime);vd::Runtime::Managed m(runtime);check(runtime.collect(5s)==vd::Runtime::Collection::completed && runtime.snapshot().objects==1,"blocked real IO obstructed precise GC");}
    runtime.close();blocked.get();
    if(missing_input)check(absent.error.kind==ErrorKind::closed && absent.error.os_code==EBADF,"initially absent stdin was mistaken for another operation's wake descriptor");
    check(result.error.kind==ErrorKind::closed && !result.eof,"real readiness wait not cancelled");
    if(writing){
        std::ofstream observed(std::string(path)+".progress.json");
        observed<<"{\"initial_poll_result\":"<<initial_write_poll<<",\"initial_poll_revents\":"<<initial_write_revents<<",\"poll_result\":"<<write_poll<<",\"poll_revents\":"<<write_revents
                <<",\"completed\":"<<result.completed<<",\"error\":"<<static_cast<unsigned>(result.error.kind)
                <<",\"missing_input\":"<<(missing_input?"true":"false")<<"}\n";
        check(bool(observed),"native output progress report failed");
    }
    if(writing)check(result.completed>0 && result.completed<1024*1024,"cancelled write lost native prefix");
    else check(result.completed==0,"cancelled empty read consumed bytes");
    check(flags_unchanged(before,path) && c->snapshot().admitted==0,"cancelled native resources/borrowed flags");report(path,result);
}
void best_effort(){
    struct Blocking final:Script {
        std::binary_semaphore entered{0},release{0};
        Transfer write(Stream,std::span<const std::uint8_t> data,const Request& request) override {
            entered.release();release.acquire();
            // A real native call can return successful progress after stop.
            check(request.cancellation.stop_requested(),"entered transfer missed stop");
            output.assign(data.begin(),data.end());return {data.size(),false,{}};
        }
    };
    auto adapter=std::make_shared<Blocking>();std::weak_ptr<Blocking> weak=adapter;
    auto runtime=std::make_unique<vd::Runtime>(0,Streams::isolated(adapter));auto c=runtime->console_context();
    auto operation=std::async(std::launch::async,[&]{return c->write(Stream::output,bytes("retained"));});adapter->entered.acquire();
    auto close=std::async(std::launch::async,[&]{runtime->close();});until([&]{return c->snapshot().closing;});
    check(close.wait_for(0ms)==std::future_status::timeout && c->snapshot().admitted==1 && !weak.expired(),"shutdown abandoned entered transfer/buffer");
    adapter->release.release();auto result=operation.get();close.get();
    check(!result.error && result.completed==8 && adapter->output==bytes("retained") && c->snapshot().admitted==0,"entered completion lost bytes/lifetime");
}
volatile std::sig_atomic_t signal_count=0;
void handler(int){signal_count=signal_count+1;}
void broken(const char* path,std::string mode){
    struct sigaction original{},installed{};sigaction(SIGPIPE,nullptr,&original);installed=original;
    if(mode=="handler"){installed.sa_handler=handler;sigemptyset(&installed.sa_mask);installed.sa_flags=0;sigaction(SIGPIPE,&installed,nullptr);}
    if(mode=="ignore"){installed.sa_handler=SIG_IGN;sigaction(SIGPIPE,&installed,nullptr);}
    sigset_t set,old,pending;sigemptyset(&set);sigaddset(&set,SIGPIPE);pthread_sigmask(SIG_SETMASK,nullptr,&old);
    if(mode=="pending"){pthread_sigmask(SIG_BLOCK,&set,nullptr);pthread_kill(pthread_self(),SIGPIPE);}
    sigset_t before;pthread_sigmask(SIG_SETMASK,nullptr,&before);auto descriptor_flags=flags();
    vd::Runtime runtime;auto result=runtime.console_context()->write(Stream::output,bytes("broken"));runtime.close();
    check(result.error.kind==ErrorKind::broken_pipe && result.error.os_code==EPIPE && result.completed==0,"broken pipe result");
    struct sigaction after{};sigaction(SIGPIPE,nullptr,&after);sigset_t after_mask;pthread_sigmask(SIG_SETMASK,nullptr,&after_mask);
    check(after.sa_handler==installed.sa_handler && signal_count==0,"Runtime changed host SIGPIPE disposition or dispatched handler");
    for(int n=1;n<NSIG;++n)check(sigismember(&before,n)==sigismember(&after_mask,n),"Runtime changed signal mask");
    sigpending(&pending);check((sigismember(&pending,SIGPIPE)==1)==(mode=="pending"),"Runtime consumed prior pending SIGPIPE or leaked its own");
    if(mode=="pending"){int received=0;sigwait(&set,&received);check(received==SIGPIPE,"host pending signal lost");}
    pthread_sigmask(SIG_SETMASK,&old,nullptr);sigaction(SIGPIPE,&original,nullptr);
    check(flags_unchanged(descriptor_flags,path),"broken pipe changed descriptors");report(path,result);
}
void interrupted_native(const char* path){
    struct sigaction old{},action{};sigemptyset(&action.sa_mask);action.sa_handler=handler;sigaction(SIGUSR1,&action,&old);
    vd::Runtime runtime;auto c=runtime.console_context();std::atomic<long> tid=0;std::promise<pthread_t> identity;auto id=identity.get_future();
    auto reader=std::async(std::launch::async,[&]{identity.set_value(pthread_self());
#ifdef __linux__
        tid=syscall(SYS_gettid);
#endif
        return c->read(4,10000);
    });auto thread=id.get();until([&]{return c->snapshot().admitted==1;});
#ifdef __linux__
    until([&]{return in_poll(static_cast<pid_t>(tid.load()));});
#endif
    check(pthread_kill(thread,SIGUSR1)==0,"native interrupt delivery");
    auto ack=c->write(Stream::output,Bytes{'!'});check(!ack.error,"interrupt acknowledgment");
    auto input=reader.get();check(input.bytes==Bytes{'X'} && !input.transfer.error && signal_count==1,"native EINTR retry lost input/deadline");
    sigaction(SIGUSR1,&old,nullptr);report(path,input.transfer);
}
void simple(const char* mode,const char* path){
    vd::Runtime runtime;auto c=runtime.console_context();Transfer result;
    if(std::string(mode)=="--poll" || std::string(mode)=="--timeout"){
        check(!c->read(0).transfer.error && !c->read(0).transfer.eof,"empty read blocked/probed EOF");
        result=c->read(4,std::string(mode)=="--poll"?0:10).transfer;
        check(result.error.kind==(std::string(mode)=="--poll"?ErrorKind::would_block:ErrorKind::timed_out),"native poll/deadline result");
    }else if(std::string(mode)=="--read-error")result=c->read(4).transfer;
    else result=c->write(Stream::output,bytes("test"));
    report(path,result);
}
#endif
}
int main(int argc,char** argv){try{
#ifdef _WIN32
    if(argc==3 && vd_windows_console_native(argv[1],argv[2]))return 0;
    vd_windows_console_closed_peers();
#endif
#ifndef _WIN32
    if(argc>=3){std::string mode=argv[1];
        if(mode=="--host-flags"){
            auto before=flags();auto count=::write(STDOUT_FILENO,"HOST",4);auto code=count<0?errno:0;
            check(flags_unchanged(before,argv[2]),"plain host write changed configured flags");
            report(argv[2],{count<0?0:static_cast<std::size_t>(count),false,code?Error{ErrorKind::io_failure,code}:Error{}});return 0;
        }
        if(mode=="--records" || mode=="--readers"){native_records(argv[2],mode=="--readers");return 0;}
        if(mode=="--copy"){copy(argv[2],argc>3?std::stoll(argv[3]):-1);return 0;}
        if(mode=="--closed-input-while-output"){cancel_native(argv[2],true,true);return 0;}
        if(mode=="--cancel-read" || mode=="--cancel-write"){cancel_native(argv[2],mode=="--cancel-write");return 0;}
        if(mode=="--interrupt"){interrupted_native(argv[2]);return 0;}
        if(mode=="--broken"){broken(argv[2],argc>3?argv[3]:"default");return 0;}
        simple(argv[1],argv[2]);return 0;
    }
#endif
    progress();deadlines();cancellation();admission();records();capabilities();
#ifndef _WIN32
    descriptor_invariants();best_effort();
#endif
    std::cout<<checks<<" checked console transfer/deadline/cancellation checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
