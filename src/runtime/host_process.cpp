#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "host_internal.hpp"
#include "void/utf8.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <set>
#include <thread>
#include <utility>
#ifndef _WIN32
#include <fcntl.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace vd::host {
#ifndef _WIN32
namespace {
struct Descriptor {
    int value=-1;
    Descriptor()=default;
    Descriptor(const Descriptor&)=delete;
    Descriptor& operator=(const Descriptor&)=delete;
    ~Descriptor(){close();}
    std::error_code close() noexcept {
        const auto fd=std::exchange(value,-1);
        return fd>=0 && ::close(fd)<0?std::error_code(errno,std::generic_category()):std::error_code{};
    }
};
struct Child final : CaptureChild {
    std::mutex mutex;
    std::condition_variable changed;
    std::array<Descriptor,2> readers;
    pid_t pid=-1;
    int status=0;
    bool waited=false;
    int wait_error=0;
    ~Child() override {close();}
    std::error_code terminate() noexcept {
        std::lock_guard lock(mutex);
        std::error_code error;
        if(pid>0 && ::kill(pid,SIGKILL)<0 && errno!=ESRCH)
            error={errno,std::generic_category()};
        changed.notify_all();
        return error;
    }
    void stop() noexcept override {(void)terminate();}
    std::optional<Result> poll_locked() noexcept {
        while(!waited && !wait_error) {
            int native=0;auto result=::waitpid(pid,&native,WNOHANG);
            if(result==pid) {status=native;pid=-1;waited=true;break;}
            if(result<0) {if(errno==EINTR)continue;wait_error=errno;pid=-1;break;}
            return {};
        }
        if(wait_error)return native_error(wait_error);
        Result r;
        if(WIFEXITED(status))r.number=WEXITSTATUS(status);
        else if(WIFSIGNALED(status)){r.flag=true;r.detail=WTERMSIG(status);}
        else return error(17);
        return r;
    }
    std::optional<Result> poll_exit() noexcept override {
        std::lock_guard lock(mutex);
        return poll_locked();
    }
    Result wait_for_exit() noexcept {
        std::unique_lock lock(mutex);
        for(;;) {
            if(auto result=poll_locked())return *result;
            changed.wait_for(lock,std::chrono::milliseconds(10));
        }
    }
    PipeRead read(unsigned stream,std::span<char> buffer) noexcept override {
        const auto count=::read(readers[stream].value,buffer.data(),buffer.size());
        if(count>0)return {static_cast<std::size_t>(count)};
        if(count==0)return {0,0,true};
        const auto code=errno;
        return {0,(code==EINTR || code==EAGAIN || code==EWOULDBLOCK)?0:code};
    }
    std::error_code close_readers() noexcept override {
        auto first=readers[0].close(),second=readers[1].close();
        return first?first:second;
    }
    std::error_code close() noexcept override {
        {std::lock_guard lock(mutex);if(pid<0 && !waited)return wait_error?std::error_code(wait_error,std::generic_category()):std::error_code{};}
        const auto termination=terminate();
        auto r=wait_for_exit();
        if(termination)return termination;
        return r.kind?std::error_code(static_cast<int>(r.code),std::generic_category()):std::error_code{};
    }
    std::error_code quiesce() noexcept override {return close();}
};
bool text(const std::string& value) {return value.find('\0')==std::string::npos && valid_utf8(value);}
bool absolute(const std::string& value) {return !value.empty() && value[0]=='/' && text(value);}
bool name(std::string_view value) {
    if(value.empty())return false;
    for(std::size_t n=0;n<value.size();++n) {
        auto c=value[n];if(c!='_' && !(c>='A'&&c<='Z') && !(c>='a'&&c<='z') && !(n && c>='0'&&c<='9'))return false;
    }
    return true;
}
struct Actions {
    posix_spawn_file_actions_t value{};
    bool active=false;
    ~Actions(){if(active)posix_spawn_file_actions_destroy(&value);}
};
struct Attributes {
    posix_spawnattr_t value{};
    bool active=false;
    ~Attributes(){if(active)posix_spawnattr_destroy(&value);}
};
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
#if __GLIBC_PREREQ(2,34)
#define VOID_CAPTURE_POSIX 1
int capture_pipe(Descriptor& reader,Descriptor& writer) {
    int pair[2];
    if(::pipe2(pair,O_CLOEXEC)<0)return errno;
    reader.value=pair[0];writer.value=pair[1];
    for(auto* end:{&reader,&writer})if(end->value<3) {
        auto fd=::fcntl(end->value,F_DUPFD_CLOEXEC,3);
        if(fd<0)return errno;
        end->close();end->value=fd;
    }
    return ::fcntl(reader.value,F_SETFL,O_NONBLOCK)<0?errno:0;
}
#endif
#endif
}
#endif
Result process_execute(Request& q,const CaptureLimits* capture) {
#ifdef _WIN32
    return windows_process_execute(q,capture);
#else
    if(q.operation==wait)return std::static_pointer_cast<Child>(q.objects[0])->wait_for_exit();
    if(q.operation!=spawn)throw std::logic_error("Invalid process operation");
    if(!absolute(q.texts[0]) || !absolute(q.texts[1]))return error(4);
    std::vector<char*> argv,env;argv.reserve(q.arrays[0].size()+2);env.reserve(q.arrays[1].size()+1);
    std::set<std::string_view> names;std::size_t bytes=q.texts[0].size()+q.texts[1].size();
    if(bytes>1048576)return error(12);
    argv.push_back(q.texts[0].data());
    for(auto& value:q.arrays[0]) {
        if(!text(value))return error(20);
        if(value.size()+1>1048576-bytes)return error(12);bytes+=value.size()+1;argv.push_back(value.data());
    }
    argv.push_back(nullptr);
    for(auto& value:q.arrays[1]) {
        auto equals=value.find('=');
        if(!text(value) || equals==std::string::npos || !name(std::string_view(value).substr(0,equals)) || !names.insert(std::string_view(value).substr(0,equals)).second)return error(20);
        if(value.size()+1>1048576-bytes)return error(12);bytes+=value.size()+1;env.push_back(value.data());
    }
    env.push_back(nullptr);
    auto child=std::make_shared<Child>();Actions actions;Attributes attributes;
    int code=posix_spawn_file_actions_init(&actions.value);if(code)return native_error(code);actions.active=true;
    code=posix_spawnattr_init(&attributes.value);if(code)return native_error(code);attributes.active=true;
#if defined(__linux__) || defined(__APPLE__)
    code=posix_spawn_file_actions_addchdir_np(&actions.value,q.texts[1].c_str());if(code)return native_error(code);
#else
    return error(11);
#endif
    std::array<Descriptor,2> writers;
    if(capture) {
#ifdef VOID_CAPTURE_POSIX
        for(unsigned n=0;n<writers.size();++n)if((*capture)[n]>=0) {
            code=capture_pipe(child->readers[n],writers[n]);if(code)return native_error(code);
            code=posix_spawn_file_actions_adddup2(&actions.value,writers[n].value,static_cast<int>(n)+1);
            if(code)return native_error(code);
        }
        code=posix_spawn_file_actions_addclosefrom_np(&actions.value,3);if(code)return native_error(code);
#else
        return error(11);
#endif
    }
    sigset_t empty,defaults;sigemptyset(&empty);sigemptyset(&defaults);
    for(int signal:{SIGINT,SIGTERM,SIGQUIT,SIGPIPE})sigaddset(&defaults,signal);
    if((code=posix_spawnattr_setsigmask(&attributes.value,&empty)) ||
       (code=posix_spawnattr_setsigdefault(&attributes.value,&defaults)) ||
       (code=posix_spawnattr_setflags(&attributes.value,POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF)))return native_error(code);
    pid_t pid=-1;code=posix_spawn(&pid,q.texts[0].c_str(),&actions.value,&attributes.value,argv.data(),env.data());
    if(code)return native_error(code);child->pid=pid;q.acquired=child;return {};
#endif
}

CaptureResult capture_execute(Request& request,const CaptureLimits& limits,Runtime& runtime,const std::atomic<bool>& cancelled) {
    using Clock=std::chrono::steady_clock;
    constexpr std::int64_t maximum=16*1024*1024;
    CaptureResult capture;
    if(limits[0]<-1 || limits[0]>maximum || limits[1]<-1 || limits[1]>maximum ||
       (limits[0]<0 && limits[1]<0) || limits[2]<0 || limits[2]>maximum || limits[3]<1 || limits[3]>300000) {
        capture.launch=error(6);
        return capture;
    }
    auto stopped=[&]{return cancelled.load(std::memory_order_relaxed) || runtime.native_stop_requested();};
    if(stopped()){capture.end=CaptureEnd::cancelled;return capture;}
    capture.launch=process_execute(request,&limits);
    if(capture.launch.kind)return capture;
    const auto deadline=Clock::now()+std::chrono::milliseconds(limits[3]);
    auto child=std::static_pointer_cast<CaptureChild>(std::move(request.acquired));
    std::size_t remaining=static_cast<std::size_t>(limits[2]);
    std::array<char,8192> buffer;
    for(;;) {
        if(stopped()){capture.end=CaptureEnd::cancelled;break;}
        if(Clock::now()>=deadline){capture.end=CaptureEnd::deadline;break;}
        if(!capture.exited)if(auto status=child->poll_exit()) {
            if(status->kind){capture.end=CaptureEnd::io_failure;capture.io_code=static_cast<int>(status->code);break;}
            capture.status=*status;capture.exited=true;
        }
        bool progress=false, complete=true;
        for(unsigned n=0;n<2;++n) {
            if(limits[n]<0 || capture.eof[n])continue;
            auto& output=capture.output[n];
            const auto budget=std::min(remaining,static_cast<std::size_t>(limits[n])-output.size());
            // Read one additional byte at the boundary to distinguish EOF
            // from a true overflow without retaining bytes beyond the limit.
            auto read=child->read(n,std::span(buffer).first(std::min(buffer.size(),budget+1)));
            if(read.error){capture.end=CaptureEnd::io_failure;capture.io_code=read.error;break;}
            capture.eof[n]=read.eof;complete&=read.eof;
            if(read.size) {
                const auto retained=std::min(budget,read.size);
                output.append(buffer.data(),retained);remaining-=retained;progress=true;
                if(read.size>retained){capture.end=CaptureEnd::output_limit;break;}
            }
        }
        if(capture.end!=CaptureEnd::complete || (complete && capture.exited))break;
        if(!progress)std::this_thread::sleep_until(std::min(deadline,Clock::now()+std::chrono::milliseconds(5)));
    }
    auto cleanup=child->close();
    auto pipes=child->close_readers();
    capture.cleanup_code=(cleanup?cleanup:pipes).value();
    if(capture.cleanup_code && capture.end==CaptureEnd::complete)capture.end=CaptureEnd::io_failure;
    return capture;
}
}
