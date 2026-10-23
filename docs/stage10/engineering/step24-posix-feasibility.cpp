// Architecture probe only, not production I/O or an acceptance substitute.
// Linux /proc supplies deterministic proof of the entered blocking system call.
#include "void/console.hpp"
#include "void/runtime.hpp"
#include <aio.h>
#include <linux/io_uring.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <poll.h>
#include <semaphore>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
using namespace std::chrono_literals;
namespace {
unsigned checks=0;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);++checks;}
struct Pipe {
    int fd[2];
    Pipe(){if(pipe(fd))throw std::runtime_error("probe pipe failed");}
    ~Pipe(){close(fd[0]);close(fd[1]);}
    Pipe(const Pipe&)=delete;
};
bool blocked(pid_t tid,long syscall_number,int descriptor){
    std::ifstream in("/proc/self/task/"+std::to_string(tid)+"/syscall");
    std::string number,arg;
    if(!(in>>number>>arg) || number=="running")return false;
    return std::stol(number,0,0)==syscall_number && std::stol(arg,0,0)==descriptor;
}
template<class F> void until(F f){
    auto end=std::chrono::steady_clock::now()+10s;
    while(!f()){
        if(std::chrono::steady_clock::now()>=end){std::cerr<<"probe synchronization failed\n";std::_Exit(2);}
        std::this_thread::yield();
    }
}
struct Metadata final:vd::console::Adapter {
    vd::console::Transfer read(std::span<std::uint8_t>,const vd::console::Request&) override {throw std::logic_error("metadata-only probe read");}
    vd::console::Transfer write(vd::console::Stream,std::span<const std::uint8_t>,const vd::console::Request&) override {throw std::logic_error("metadata-only probe write");}
    std::array<vd::console::Endpoint,3> inspect() const override {
        using namespace vd::console;
        return {{{Stream::input,Kind::pipe},{Stream::output,Kind::pipe},{Stream::error,Kind::pipe}}};
    }
    std::chrono::steady_clock::time_point now() const noexcept override{return std::chrono::steady_clock::now();}
};
void readiness_cancellation(){
    Pipe data,wakeup;
    auto original_flags=fcntl(data.fd[1],F_GETFL);
    check(original_flags>=0 && !(original_flags&O_NONBLOCK),"probe requires blocking pipe");
    auto capacity=fcntl(data.fd[1],F_GETPIPE_SZ);
    check(capacity>0,"pipe capacity unavailable");
    vd::Runtime runtime(0,vd::console::Streams::isolated(std::make_shared<Metadata>()));
    auto context=runtime.console_context();
    std::binary_semaphore ready(0),filled(0);
    std::atomic<pid_t> tid=0;
    std::atomic<ssize_t> transferred=-2;
    std::atomic<short> readiness=0;
    auto writer=std::async(std::launch::async,[&]{
        auto op=context->acquire(vd::console::Stream::output);
        std::stop_callback notify(op.cancellation(),[&]{
            char byte=1;ssize_t count;
            do{count=write(wakeup.fd[1],&byte,1);}while(count<0 && errno==EINTR);
            if(count!=1)std::_Exit(3);
        });
        pollfd p{data.fd[1],POLLOUT,0};
        if(poll(&p,1,0)!=1)std::_Exit(4);
        readiness=p.revents;ready.release();filled.acquire();
        tid=static_cast<pid_t>(syscall(SYS_gettid));
        // Even a ONE-byte blocking write is unsafe after readiness is lost.
        char byte='V';transferred=write(data.fd[1],&byte,1);
    });
    ready.acquire();check(readiness.load()&POLLOUT,"initial readiness missing");
    // Independent host/process writers are permitted by the current output
    // contract. Fill exactly the empty pipe's capacity, without flag changes.
    std::vector<char> payload(static_cast<std::size_t>(capacity),'H');
    check(write(data.fd[1],payload.data(),payload.size())==capacity,"host fill failed");
    filled.release();until([&]{return tid.load() && blocked(tid,SYS_write,data.fd[1]);});
    auto shutdown=std::async(std::launch::async,[&]{runtime.close();});
    pollfd p{wakeup.fd[0],POLLIN,0};
    check(poll(&p,1,10000)==1 && (p.revents&POLLIN),"owned cancellation wake missing");
    check(blocked(tid,SYS_write,data.fd[1]) && transferred==-2,"wake unexpectedly interrupted blocking write");
    check(shutdown.wait_for(0ms)==std::future_status::timeout && context->snapshot().closing && context->snapshot().admitted==1,"close did not retain blocked admitted operation");
    // Always release and join the request; no detached worker or leaked buffer.
    std::vector<char> drained(static_cast<std::size_t>(capacity));
    check(read(data.fd[0],drained.data(),drained.size())==capacity && drained==payload,"host prefix changed");
    writer.get();shutdown.get();
    check(transferred==1 && context->snapshot().admitted==0,"release/drain failed");
    char tail=0;check(read(data.fd[0],&tail,1)==1 && tail=='V',"final byte changed");
    check(fcntl(data.fd[1],F_GETFL)==original_flags,"borrowed flag contract changed");
    std::cout<<"poll/POLLOUT -> permitted independent host fill -> one-byte write blocked; "
                "session stop woke self-pipe but Runtime close could not drain until host read; all joined\n";
}
void aio_cancellation(){
    Pipe data;char byte=0;aiocb request{};
    request.aio_fildes=data.fd[0];request.aio_buf=&byte;request.aio_nbytes=1;
    request.aio_sigevent.sigev_notify=SIGEV_NONE;
    check(aio_read(&request)==0,"aio_read admission failed");
    pid_t worker=0;
    until([&]{
        for(const auto& entry:std::filesystem::directory_iterator("/proc/self/task")){
            auto tid=static_cast<pid_t>(std::stol(entry.path().filename().string()));
            if(blocked(tid,SYS_read,data.fd[0])){worker=tid;return true;}
        }
        return false;
    });
    check(aio_error(&request)==EINPROGRESS,"AIO request not in progress");
    auto cancellation=aio_cancel(data.fd[0],&request);
    check(cancellation==AIO_NOTCANCELED,"expected in-flight libc AIO cancellation limitation not observed");
    check(aio_error(&request)==EINPROGRESS && blocked(worker,SYS_read,data.fd[0]),"AIO read unexpectedly cancelled");
    char sent='A';check(write(data.fd[1],&sent,1)==1,"AIO release failed");
    const aiocb* list[]{&request};
    while(aio_error(&request)==EINPROGRESS){if(aio_suspend(list,1,nullptr)<0 && errno!=EINTR)std::_Exit(5);}
    check(aio_error(&request)==0 && aio_return(&request)==1 && byte=='A',"AIO completion/progress failed");
    std::cout<<"per-request aio_cancel returned AIO_NOTCANCELED for entered pipe read; completed/drained after explicit host write\n";
}
}
int main(int argc,char** argv){try{
    if(argc==2 && std::string(argv[1])=="--uring"){
        io_uring_params parameters{};errno=0;
        auto result=syscall(SYS_io_uring_setup,2,&parameters);auto error=errno;
        std::cout<<"io_uring_setup entries=2, flags=0, struct_bytes="<<sizeof(parameters)<<", result="<<result<<", errno="<<error<<"\n";
        if(result>=0)close(static_cast<int>(result));
        return 0;
    }
    readiness_cancellation();aio_cancellation();
    std::cout<<checks<<" deterministic native feasibility checks passed (not Step 24 acceptance)\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
