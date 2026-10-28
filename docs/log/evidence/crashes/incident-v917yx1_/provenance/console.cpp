#include "void/console.hpp"
#include "void/runtime.hpp"
#include "void/utf8.hpp"
#include "void/async.hpp"
#include <cstring>
#ifdef _WIN32
#include "void/detail/console_wide.hpp"
#endif
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#else
#include <cerrno>
#include <climits>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <dlfcn.h>
#include <cstdlib>
#endif

namespace vd::console {
namespace {
void native_only() {
    if(auto* runtime=Runtime::attached();runtime && runtime->is_managed())
        throw std::logic_error("console operation requires NativeSafe state");
}
std::size_t index(Stream stream) {
    switch(stream){case Stream::input:return 0;case Stream::output:return 1;case Stream::error:return 2;}
    throw std::invalid_argument("invalid standard stream");
}
bool input(Stream stream) noexcept {return stream==Stream::input;}
using Clock=std::chrono::steady_clock;
Error stopped(const Request& request,const Adapter& adapter,std::int64_t code=0) {
    if(request.cancellation.stop_requested())return {ErrorKind::closed,code};
    if(!request.polling && request.deadline && adapter.now()>=*request.deadline)
        return {ErrorKind::timed_out,code};
    return {};
}
Request request_for(const Adapter& adapter,std::stop_token cancellation,std::int64_t timeout) {
    Request result{cancellation,{},timeout==0};
    if(timeout>0){
        auto now=adapter.now();
        auto limit=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::duration::max()).count();
        if(timeout>limit)result.deadline=Clock::time_point::max();
        else {
            auto delta=std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(timeout));
            result.deadline=now.time_since_epoch()>Clock::duration::max()-delta?Clock::time_point::max():now+delta;
        }
    }
    return result;
}
Clock::duration remaining(const Request& request,const Adapter& adapter) {
    auto now=adapter.now();
    if(*request.deadline<=now)return Clock::duration::zero();
    auto high=request.deadline->time_since_epoch().count(),low=now.time_since_epoch().count();
    if(low<0 && high>Clock::duration::max().count()+low)return Clock::duration::max();
    return *request.deadline-now;
}
struct AdmissionFailure {Error error;};
#ifndef _WIN32
Error native_error(int code) {
    if(code==EAGAIN || code==EWOULDBLOCK)return {ErrorKind::would_block,code};
    if(code==EINTR)return {ErrorKind::interrupted,code};
    if(code==EBADF)return {ErrorKind::closed,code};
    if(code==EPIPE)return {ErrorKind::broken_pipe,code};
    if(code==EINVAL)return {ErrorKind::invalid_argument,code};
    if(code==ENOSYS || code==ENOTSUP)return {ErrorKind::unsupported,code};
    return {ErrorKind::io_failure,code};
}
struct SavedError {int value=errno;~SavedError(){errno=value;}};
struct Wake {
    int descriptors[2]{-1,-1};
    Error error;
    Wake(){
#ifdef __linux__
        if(pipe2(descriptors,O_CLOEXEC|O_NONBLOCK)<0)error=native_error(errno);
#else
        if(pipe(descriptors)<0){error=native_error(errno);return;}
        for(int fd:descriptors){
            if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0 || fcntl(fd,F_SETFL,O_NONBLOCK)<0){error=native_error(errno);break;}
        }
#endif
    }
    ~Wake(){auto saved=errno;for(int fd:descriptors)if(fd>=0)close(fd);errno=saved;}
    void signal() const noexcept {
        auto saved=errno;std::uint8_t byte=1;ssize_t count;
        do{count=::write(descriptors[1],&byte,1);}while(count<0 && errno==EINTR);
        // Only one callback, and its owned read end outlives callback removal.
        // EAGAIN is already signaled; no host descriptor is ever used here.
        errno=saved;
    }
};
// SIGPIPE policy is local to this thread and this write, never process-global.
Transfer guarded_write(int fd,std::span<const std::uint8_t> bytes) {
    sigset_t set,old,pending;sigemptyset(&set);sigaddset(&set,SIGPIPE);
    auto setup=pthread_sigmask(SIG_BLOCK,&set,&old);
    if(setup)return {0,false,native_error(setup)};
    if(sigpending(&pending)<0){
        auto original=errno;auto cleanup=pthread_sigmask(SIG_SETMASK,&old,nullptr);
        auto error=native_error(original);error.cleanup_code=cleanup;return {0,false,error};
    }
    bool existed=sigismember(&pending,SIGPIPE)==1;
    auto count=::write(fd,bytes.data(),bytes.size());auto code=count<0?errno:0;
    int cleanup=0;
    if(code==EPIPE && !existed){
        // Do not consume a SIGPIPE already pending on entry. With an ignored
        // disposition no signal need be pending, hence the zero-time wait.
#ifdef __APPLE__
        // Darwin has sigwait but not sigtimedwait. With SIGPIPE blocked, a
        // pending signal from this EPIPE can be synchronously consumed.
        if(sigpending(&pending)<0)cleanup=errno;
        else if(sigismember(&pending,SIGPIPE)==1){int received=0;cleanup=sigwait(&set,&received);}
#else
        timespec zero{};int result;
        do{result=sigtimedwait(&set,nullptr,&zero);}while(result<0 && errno==EINTR);
        if(result<0 && errno!=EAGAIN)cleanup=errno;
#endif
    }
    auto restore=pthread_sigmask(SIG_SETMASK,&old,nullptr);
    if(!cleanup)cleanup=restore;
    auto error=code?native_error(code):Error{};
    if(cleanup){if(error)error.cleanup_code=cleanup;else error=native_error(cleanup);}
    return {count<0?0:static_cast<std::size_t>(count),false,error};
}
#endif
#ifndef _WIN32
#include "console_terminfo.inc"
#endif
class SystemAdapter final : public Adapter {
#ifdef _WIN32
    struct Owned {
        HANDLE handle=nullptr;
        ~Owned(){auto saved=GetLastError();if(handle && handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);SetLastError(saved);}
    };
    static Error windows_error(DWORD code) {
        if(code==ERROR_NO_UNICODE_TRANSLATION)return {ErrorKind::invalid_encoding,code};
        if(code==ERROR_BROKEN_PIPE || code==ERROR_PIPE_NOT_CONNECTED)return {ErrorKind::broken_pipe,code};
        if(code==ERROR_NO_DATA || code==ERROR_PIPE_LISTENING)return {ErrorKind::would_block,code};
        if(code==ERROR_INVALID_HANDLE)return {ErrorKind::closed,code};
        if(code==ERROR_OPERATION_ABORTED)return {ErrorKind::interrupted,code};
        if(code==ERROR_INVALID_PARAMETER)return {ErrorKind::invalid_argument,code};
        if(code==ERROR_NOT_SUPPORTED || code==ERROR_INVALID_FUNCTION)return {ErrorKind::unsupported,code};
        return {ErrorKind::io_failure,code};
    }
    struct Work {
        HANDLE handle;bool reading;void* bytes;DWORD size;
        bool asynchronous;bool wide=false;OVERLAPPED overlap{};
        BOOL success=FALSE;DWORD count=0,error=0;
        static DWORD WINAPI run(void* raw) noexcept {
            static_assert(sizeof(char16_t)==sizeof(WCHAR));
            auto& work=*static_cast<Work*>(raw);
            auto* ov=work.asynchronous?&work.overlap:nullptr;
            auto* count=work.asynchronous?nullptr:&work.count;
            if(work.wide)work.success=work.reading?ReadConsoleW(work.handle,work.bytes,work.size,&work.count,nullptr):WriteConsoleW(work.handle,work.bytes,work.size,&work.count,nullptr);
            else work.success=work.reading?ReadFile(work.handle,work.bytes,work.size,count,ov):WriteFile(work.handle,work.bytes,work.size,count,ov);
            work.error=work.success?NO_ERROR:GetLastError();
            if(work.asynchronous && (work.success || work.error==ERROR_IO_PENDING || work.error==ERROR_MORE_DATA)){
                work.success=GetOverlappedResult(work.handle,&work.overlap,&work.count,TRUE);
                work.error=work.success?NO_ERROR:GetLastError();
            }
            return 0;
        }
    };
    struct Borrowed { HANDLE handle; DWORD error; };
    std::array<Borrowed,3> borrowed_;
    std::array<std::optional<WORD>,3> defaults_;
    detail::WideConsole wide_; // Minimal transcoding carry, guarded by the input lease.
    static Borrowed capture(DWORD name) {
        SetLastError(NO_ERROR);auto handle=GetStdHandle(name);auto error=GetLastError();
        return {handle,(!handle || handle==INVALID_HANDLE_VALUE)?(error?error:ERROR_INVALID_HANDLE):NO_ERROR};
    }
    static Endpoint classify(Stream stream,Borrowed value) {
        auto handle=value.handle;
        if(value.error)return {stream,Kind::unavailable,value.error};
        SetLastError(NO_ERROR);auto type=GetFileType(handle);auto code=GetLastError();
        if(type==FILE_TYPE_UNKNOWN && code!=NO_ERROR)return {stream,Kind::unavailable,code};
        DWORD mode=0;if(GetConsoleMode(handle,&mode))return {stream,Kind::terminal};
        if(type==FILE_TYPE_PIPE)return {stream,Kind::pipe};
        if(type==FILE_TYPE_DISK)return {stream,Kind::file};
        if(type==FILE_TYPE_CHAR)return {stream,Kind::character};
        return {stream,Kind::other};
    }
public:
    SystemAdapter(){auto saved=GetLastError();borrowed_={capture(STD_INPUT_HANDLE),capture(STD_OUTPUT_HANDLE),capture(STD_ERROR_HANDLE)};for(unsigned n=1;n<3;++n){CONSOLE_SCREEN_BUFFER_INFO info{};if(GetConsoleScreenBufferInfo(borrowed_[n].handle,&info))defaults_[n]=info.wAttributes;}SetLastError(saved);}
#else
    const std::array<int,3> borrowed_{STDIN_FILENO,STDOUT_FILENO,STDERR_FILENO};
    const std::array<Endpoint,3> endpoints_;
    static Endpoint classify(Stream stream,int descriptor) {
        struct stat status{};int result;
        do{result=fstat(descriptor,&status);}while(result<0 && errno==EINTR);
        if(result<0)return {stream,Kind::unavailable,errno};
        if(isatty(descriptor)==1)return {stream,Kind::terminal};
        if(S_ISREG(status.st_mode))return {stream,Kind::file};
        if(S_ISFIFO(status.st_mode))return {stream,Kind::pipe};
        if(S_ISCHR(status.st_mode))return {stream,Kind::character};
        if(S_ISSOCK(status.st_mode))return {stream,Kind::socket};
        return {stream,Kind::other};
    }
    static std::array<Endpoint,3> capture_endpoints() {
        SavedError saved;
        return {classify(Stream::input,STDIN_FILENO),classify(Stream::output,STDOUT_FILENO),classify(Stream::error,STDERR_FILENO)};
    }
public:
    SystemAdapter():endpoints_(capture_endpoints()){}
#endif
#include "console_presentation.inc"
#include "console_keyboard.inc"
    Transfer read(std::span<std::uint8_t> bytes,const Request& request) override {
        if(bytes.empty())return {};
        native_only();
#ifdef _WIN32
        return windows_transfer(Stream::input,bytes.data(),bytes.size(),request);
#else
        SavedError saved;
        return posix_transfer(Stream::input,bytes.data(),bytes.size(),request);
#endif
    }
    Transfer write(Stream stream,std::span<const std::uint8_t> bytes,const Request& request) override {
        if(input(stream))return {0,false,{ErrorKind::invalid_argument}};
        if(bytes.empty())return {};
        native_only();
#ifdef _WIN32
        return windows_transfer(stream,const_cast<std::uint8_t*>(bytes.data()),bytes.size(),request);
#else
        SavedError saved;
        return posix_transfer(stream,const_cast<std::uint8_t*>(bytes.data()),bytes.size(),request);
#endif
    }
#ifndef _WIN32
private:
    Transfer posix_transfer(Stream stream,std::uint8_t* bytes,std::size_t size,const Request& request) {
        if(auto error=stopped(request,*this);error)return {0,false,error};
        auto fd=borrowed_[index(stream)];
        // Inspect BEFORE creating wake descriptors: a closed host fd (notably
        // stdin=0) must not accidentally become the newly allocated self-pipe.
        auto endpoint=endpoints_[index(stream)];
        if(!endpoint.available())return {0,false,native_error(static_cast<int>(endpoint.os_code))};
        Wake wake;if(wake.error)return {0,false,wake.error};
        std::stop_callback notify(request.cancellation,[&wake]{wake.signal();});
        for(;;){
            if(auto error=stopped(request,*this);error)return {0,false,error};
            int timeout=-1;
            if(request.polling)timeout=0;
            else if(request.deadline){
                auto left=remaining(request,*this);
                if(left<=Clock::duration::zero())return {0,false,{ErrorKind::timed_out}};
                auto millis=std::chrono::ceil<std::chrono::milliseconds>(left).count();
                timeout=static_cast<int>(std::min<std::int64_t>(millis,INT_MAX));
            }
            pollfd descriptors[2]{{fd,static_cast<short>(input(stream)?POLLIN:POLLOUT),0},{wake.descriptors[0],POLLIN,0}};
            auto result=poll(descriptors,2,timeout);
            if(result<0){
                auto code=errno;
                if(auto error=stopped(request,*this,code);error)return {0,false,error};
                if(code==EINTR){if(request.polling)return {0,false,native_error(code)};continue;}
                return {0,false,native_error(code)};
            }
            if(auto error=stopped(request,*this);error)return {0,false,error};
            if(!result){
                if(request.polling)return {0,false,{ErrorKind::would_block}};
                continue; // Long deadlines may exceed poll's int timeout.
            }
            if(descriptors[0].revents&POLLNVAL)return {0,false,native_error(EBADF)};
            if(!descriptors[0].revents)continue;
            size=std::min(size,static_cast<std::size_t>(SSIZE_MAX));
            // Bound normal pipe writes to the atomicity limit; this reduces
            // blocking but does NOT reserve capacity against foreign writers.
            if(!input(stream)){
                if(endpoint.kind==Kind::pipe)size=std::min(size,static_cast<std::size_t>(PIPE_BUF));
                return guarded_write(fd,{bytes,size});
            }
            auto count=::read(fd,bytes,size);auto code=count<0?errno:0;
            if(count<0)return {0,false,native_error(code)};
            return {static_cast<std::size_t>(count),count==0,{}};
        }
    }
public:
#endif
#ifdef _WIN32
private:
    Transfer windows_transfer(Stream stream,void* bytes,std::size_t size,const Request& request) {
        struct Saved {DWORD value=GetLastError();~Saved(){SetLastError(value);}} saved;
        if(auto error=stopped(request,*this);error)return {0,false,error};
        auto value=borrowed_[index(stream)];
        if(value.error)return {0,false,windows_error(value.error)};
        auto endpoint=classify(stream,value);
        if(!endpoint.available())return {0,false,windows_error(static_cast<DWORD>(endpoint.os_code))};
        if(endpoint.kind==Kind::terminal){
            if(input(stream))return wide_.read({static_cast<std::uint8_t*>(bytes),size},request,
                [&](std::span<char16_t> out,const Request& req){return windows_native(stream,out.data(),out.size(),req,true);});
            return detail::WideConsole::write({static_cast<const std::uint8_t*>(bytes),size},request,
                [&](std::span<const char16_t> out,const Request& req){return windows_native(stream,const_cast<char16_t*>(out.data()),out.size(),req,true);});
        }
        return windows_native(stream,bytes,size,request,false);
    }
    Transfer windows_native(Stream stream,void* bytes,std::size_t size,const Request& request,bool wide) {
        if(auto error=stopped(request,*this);error)return {0,false,error};
        auto value=borrowed_[index(stream)];
        auto endpoint=classify(stream,value);
        if(!endpoint.available())return {0,false,windows_error(static_cast<DWORD>(endpoint.os_code))};
        bool asynchronous=false;
        if(!wide){
            // Standard handles can be supplied by an embedding host with OVERLAPPED
            // enabled. Determine the real mode before choosing a request shape.
            using Query=NTSTATUS(NTAPI*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS);
            auto module=GetModuleHandleW(L"ntdll.dll");
            auto query=reinterpret_cast<Query>(GetProcAddress(module,"NtQueryInformationFile"));
            if(!query)return {0,false,{ErrorKind::unsupported,ERROR_NOT_SUPPORTED}};
            struct Mode {ULONG flags=0;} mode;
            IO_STATUS_BLOCK status{};
            constexpr NTSTATUS pending=0x103; // STATUS_PENDING
            status.Status=pending;
            auto queried=query(value.handle,&status,&mode,sizeof(mode),static_cast<FILE_INFORMATION_CLASS>(16)); // FileModeInformation
            if(queried==pending){
                // An asynchronous metadata query must also retain its output storage.
                // It has no request event parameter; observe its own IOSB completion,
                // never interpret a foreign request's shared-handle signal as ours.
                auto* observed=static_cast<volatile IO_STATUS_BLOCK*>(&status);
                while(observed->Status==pending)Sleep(1);
                MemoryBarrier();queried=observed->Status;
            }
            if(queried<0){
                using Convert=ULONG(WINAPI*)(NTSTATUS);
                auto convert=reinterpret_cast<Convert>(GetProcAddress(module,"RtlNtStatusToDosError"));
                return {0,false,{ErrorKind::unsupported,convert?static_cast<std::int64_t>(convert(queried)):static_cast<std::int64_t>(queried)}};
            }
            if(status.Information<sizeof(mode))return {0,false,{ErrorKind::unsupported,ERROR_NOT_SUPPORTED}};
            asynchronous=(mode.flags&(0x10u|0x20u))==0; // FILE_SYNCHRONOUS_IO_ALERT/NONALERT
            // An asynchronous disk handle has explicit offsets, not an ordered
            // shared stream position. Do not invent/advance the host's position.
            if(asynchronous && endpoint.kind==Kind::file)return {0,false,{ErrorKind::unsupported,ERROR_NOT_SUPPORTED}};
        }
        Owned wake{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        if(!wake.handle)return {0,false,windows_error(GetLastError())};
        Owned completion;
        if(asynchronous){completion.handle=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!completion.handle)return {0,false,windows_error(GetLastError())};}
        Work work{value.handle,input(stream),bytes,static_cast<DWORD>(std::min<std::size_t>(size,MAXDWORD)),asynchronous,wide};
        work.overlap.hEvent=completion.handle;
        if(auto error=stopped(request,*this);error)return {0,false,error};
        // This thread owns only this native syscall. Cancellation can never
        // target unrelated host I/O issued on a shared executor/host thread.
        Owned worker{CreateThread(nullptr,0,&Work::run,&work,0,nullptr)};
        if(!worker.handle)return {0,false,windows_error(GetLastError())};
        std::stop_callback notify(request.cancellation,[&wake]{auto saved=GetLastError();SetEvent(wake.handle);SetLastError(saved);});
        Error control;
        HANDLE handles[]{worker.handle,wake.handle};
        for(;;){
            if(auto error=stopped(request,*this);error){control=error;break;}
            DWORD timeout=INFINITE;
            if(request.polling)timeout=0;
            else if(request.deadline){
                auto left=remaining(request,*this);
                if(left<=Clock::duration::zero()){control={ErrorKind::timed_out};break;}
                timeout=static_cast<DWORD>(std::min<std::int64_t>(std::chrono::ceil<std::chrono::milliseconds>(left).count(),INFINITE-1));
            }
            auto result=WaitForMultipleObjects(2,handles,FALSE,timeout);
            if(result==WAIT_OBJECT_0)break;
            if(result==WAIT_FAILED){control=windows_error(GetLastError());break;}
            if(result==WAIT_TIMEOUT && request.polling){control={ErrorKind::would_block};break;}
        }
        if(control){
            // Repeat until completion to cover cancellation requested just before
            // the worker enters its syscall. Cancellation is best-effort: retain
            // Work, byte storage, events and lease until the actual thread exits.
            while(WaitForSingleObject(worker.handle,0)!=WAIT_OBJECT_0){
                if(asynchronous)CancelIoEx(value.handle,&work.overlap);
                else CancelSynchronousIo(worker.handle);
                WaitForSingleObject(worker.handle,1);
            }
        }
        // A completion wins a cancellation race. Never invent a byte count or
        // discard successful bytes merely because a stop/deadline also occurred.
        if(work.success){
            if(control && control.kind!=ErrorKind::closed && control.kind!=ErrorKind::timed_out && control.kind!=ErrorKind::would_block)
                return {work.count,false,control};
            return {work.count,input(stream) && work.count==0,{}};
        }
        if(input(stream) && (work.error==ERROR_BROKEN_PIPE || work.error==ERROR_HANDLE_EOF))return {work.count,work.count==0,{}};
        auto error=windows_error(work.error);
        if(work.error==ERROR_OPERATION_ABORTED && control){control.os_code=work.error;error=control;}
        return {work.count,false,error};
    }
public:
#endif
    std::array<Endpoint,3> inspect() const override {
#ifdef _WIN32
        auto saved=GetLastError();
        std::array<Endpoint,3> result{classify(Stream::input,borrowed_[0]),classify(Stream::output,borrowed_[1]),classify(Stream::error,borrowed_[2])};
        SetLastError(saved);return result;
#else
        return endpoints_;
#endif
    }
    std::chrono::steady_clock::time_point now() const noexcept override {return std::chrono::steady_clock::now();}
    // No close/dup of borrowed endpoints, stdio buffering or terminal mode changes.
};
}
namespace {
struct InputBlock {
    std::array<std::uint8_t,4096> bytes;
    std::size_t size=0;
    Error after;
    std::optional<PhysicalKey> key;
};
struct InputQueue {
    std::deque<InputBlock> blocks;
    std::size_t head=0;
    bool eof=false;
    bool crlf=false;
};
}
struct Streams::State {
    std::shared_ptr<Adapter> adapter;
    const std::array<Endpoint,3> endpoints;
    std::mutex mutex;
    std::condition_variable changed;
    bool input_busy=false,output_busy=false;
    struct ModeEntry {std::weak_ptr<Session> session; std::thread::id thread; const void* execution; std::int64_t token; InputMode saved;};
    std::vector<ModeEntry> modes; std::int64_t next_mode=1;
    InputQueue input; // Access only under the existing exclusive input lease.
    explicit State(std::shared_ptr<Adapter> a):adapter(std::move(a)),endpoints(adapter->inspect()) {
        for(std::size_t n=0;n<endpoints.size();++n){
            if(index(endpoints[n].stream)!=n || static_cast<unsigned>(endpoints[n].kind)>static_cast<unsigned>(Kind::other))
                throw std::invalid_argument("invalid standard endpoint metadata");
        }
    }
};
struct Session {
    std::shared_ptr<Streams> streams;
    std::stop_source cancellation;
    bool closing=false;
    std::size_t admitted=0; // Protected by streams' native mutex.
    explicit Session(std::shared_ptr<Streams> s):streams(std::move(s)){}
    ~Session(){
        auto& state=*streams->state_;std::unique_lock lock(state.mutex);
        if(state.modes.empty() || !state.modes.back().session.expired())return;
        if(state.input_busy)std::terminate();state.input_busy=true;
        while(!state.modes.empty() && state.modes.back().session.expired()){
            auto saved=state.modes.back().saved;lock.unlock();auto error=state.adapter->restore_mode(saved);lock.lock();
            if(error)break;state.modes.pop_back();
        }
        state.input_busy=false;lock.unlock();state.changed.notify_all();
    }
};
Streams::Streams(std::shared_ptr<Adapter> adapter) {
    if(!adapter)throw std::invalid_argument("null console adapter");
    state_=std::make_unique<State>(std::move(adapter));
}
Streams::~Streams() {
    if(auto* runtime=Runtime::attached();runtime && runtime->is_managed()){Runtime::Native safe(*runtime);state_.reset();}
    else state_.reset();
}
std::shared_ptr<Streams> Streams::process() {
    native_only();
    // Retain the process input context even between Runtime lifetimes. It owns
    // only native state; borrowed standard handles are never closed.
    static auto streams=std::shared_ptr<Streams>(new Streams(std::make_shared<SystemAdapter>()));
    return streams;
}
std::shared_ptr<Streams> Streams::isolated(std::shared_ptr<Adapter> adapter) {
    native_only();return std::shared_ptr<Streams>(new Streams(std::move(adapter)));
}
Context::Context(std::shared_ptr<Streams> streams) {
    native_only();if(!streams)throw std::invalid_argument("null console streams");
    session_=std::make_shared<Session>(std::move(streams));
}
Context::~Context() {
    if(auto* runtime=Runtime::attached();runtime && runtime->is_managed()){Runtime::Native safe(*runtime);begin_close();session_.reset();}
    else {begin_close();session_.reset();}
}
std::shared_ptr<Streams> Context::streams() const noexcept {return session_->streams;}
Endpoint Context::endpoint(Stream stream) const {return session_->streams->state_->endpoints[index(stream)];}
Context::Snapshot Context::snapshot() const {
    native_only();auto& state=*session_->streams->state_;std::lock_guard lock(state.mutex);
    return {session_->closing,session_->admitted};
}
std::uint64_t Context::capabilities(Stream stream) {
    native_only();auto session=session_;auto& state=*session->streams->state_;index(stream);
    {std::lock_guard lock(state.mutex);if(session->closing)return 0;}
    auto caps=state.adapter->capabilities(stream);
    {std::lock_guard lock(state.mutex);if(session->closing)return 0;if(state.input_busy)caps&=~std::uint64_t(cursor_position);}
    return caps;
}
TerminalResult Context::present(Stream stream,Presentation operation,std::int64_t a,std::int64_t b,std::string_view text) {
    native_only();TerminalResult result;
    if(input(stream)){result.transfer.error={ErrorKind::unsupported};return result;}
    if((operation==Presentation::set_cursor && (a<0 || b<0)) ||
       (operation==Presentation::cursor_visible && a!=0 && a!=1) ||
       (operation==Presentation::color && (a<0 || a>15 || b<0 || b>15))){result.transfer.error={ErrorKind::invalid_argument};return result;}
    if(operation==Presentation::title){
        if(!valid_utf8(text)){result.transfer.error={ErrorKind::invalid_encoding};return result;}
        for(std::size_t i=0;i<text.size();){auto n=utf8_scalar_bytes(text,i);auto c=static_cast<unsigned char>(text[i]);unsigned scalar=n==1?c:c&(n==2?31:n==3?15:7);for(unsigned k=1;k<n;++k)scalar=(scalar<<6)|(static_cast<unsigned char>(text[i+k])&63);if(scalar<32 || (scalar>=127 && scalar<=159)){result.transfer.error={ErrorKind::invalid_argument};return result;}i+=n;}
    }
    auto& adapter=*session_->streams->state_->adapter;
    auto request=request_for(adapter,session_->cancellation.get_token(),operation==Presentation::cursor_position?200:-1);
    try{
        std::optional<Operation> owner;
        if(operation==Presentation::cursor_position){for(auto* active=Operation::active_;active;active=active->previous_)if(active->session_->streams==session_->streams){result.transfer.error={ErrorKind::unsupported};return result;}auto poll=request;poll.polling=true;owner.emplace(acquire(Stream::input,poll));}
        auto output=acquire(stream,request);
        result=adapter.present(stream,operation,a,b,text,request);
        if(result.transfer.error)return result;
        std::size_t ignored=0;
        if(result.response){auto prior=owner->before_report(request);if(prior.transfer.error)return prior;ignored=static_cast<std::size_t>(prior.first);}
        if(!result.sequence.empty()){
            auto written=output.write(result.sequence,request);result.transfer=written;
            if(written.error)return result;
        }
        if(result.response){auto response=owner->cursor_report(request,result.first,result.second,ignored);response.transfer.completed=result.transfer.completed;return response;}
        return result;
    }catch(const AdmissionFailure& error){result.transfer.error=error.error;return result;}
}

void Context::begin_close() {
    native_only();auto session=session_;auto& state=*session->streams->state_;
    {std::lock_guard lock(state.mutex);session->closing=true;}
    state.changed.notify_all();
    // Native-only cancellation callbacks execute outside every stream/GC lock.
    // Per-transfer callbacks wake owned platform waits; entered syscalls may finish later.
    session->cancellation.request_stop();
}
void Context::check_drain() const {
    for(auto* op=Operation::active_;op;op=op->previous_)if(op->session_==session_)
        throw std::logic_error("console context cannot drain its own operation");
}
Error Context::close() {
    native_only();check_drain();
    begin_close();auto& state=*session_->streams->state_;std::unique_lock lock(state.mutex);
    state.changed.wait(lock,[&]{return session_->admitted==0;});
    lock.unlock();
    return restore_modes();
}
Operation Context::acquire(Stream stream) {
    try{return acquire(stream,request_for(*session_->streams->state_->adapter,session_->cancellation.get_token(),-1));}
    catch(const AdmissionFailure&){throw Closed();}
}
Operation Context::acquire(Stream stream,const Request& request) {
    native_only();index(stream);auto session=session_;auto& state=*session->streams->state_;
    std::unique_lock lock(state.mutex);
    if(session->closing)throw AdmissionFailure{{ErrorKind::closed}};
    if(input(stream) && !state.modes.empty()){auto& mode=state.modes.back();if(mode.session.lock()!=session || mode.thread!=std::this_thread::get_id() || mode.execution!=vd::async_execution_context())throw AdmissionFailure{{ErrorKind::would_block}};}
    for(auto* op=Operation::active_;op;op=op->previous_)if(op->session_->streams==session->streams && (input(op->stream_)==input(stream) || input(stream)))
        throw std::logic_error("recursive console gate or output-before-input lock order");
    if(session->admitted==std::numeric_limits<std::size_t>::max())throw std::length_error("console admission count overflow");
    ++session->admitted;
    auto& busy=input(stream)?state.input_busy:state.output_busy;
    try{
        while(busy && !session->closing){
            if(request.polling)throw AdmissionFailure{{ErrorKind::would_block}};
            if(auto error=stopped(request,*state.adapter);error)throw AdmissionFailure{error};
            if(request.deadline){
                auto left=remaining(request,*state.adapter);
                if(left<=Clock::duration::zero())throw AdmissionFailure{{ErrorKind::timed_out}};
                state.changed.wait_for(lock,std::min(left,std::chrono::duration_cast<Clock::duration>(std::chrono::hours(24))));
            }else state.changed.wait(lock);
        }
        if(session->closing)throw AdmissionFailure{{ErrorKind::closed}};
        if(input(stream) && !state.modes.empty()){auto& mode=state.modes.back();if(mode.session.lock()!=session || mode.thread!=std::this_thread::get_id() || mode.execution!=vd::async_execution_context())throw AdmissionFailure{{ErrorKind::would_block}};}
        if(auto error=stopped(request,*state.adapter);error)throw AdmissionFailure{error};
    }catch(...){--session->admitted;state.changed.notify_all();throw;}
    busy=true;return Operation(std::move(session),stream);
}
thread_local Operation* Operation::active_=nullptr;
Operation::Operation(std::shared_ptr<Session> session,Stream stream) noexcept
    :session_(std::move(session)),stream_(stream),owner_(std::this_thread::get_id()),previous_(active_){active_=this;}
void Operation::require_owner() const {
    native_only();if(!session_ || owner_!=std::this_thread::get_id())throw std::logic_error("console operation used outside its owning thread/lifetime");
}
void Operation::unlink() noexcept {
    for(auto** at=&active_;*at;at=&(*at)->previous_)if(*at==this){*at=previous_;return;}
    std::terminate();
}
Operation::Operation(Operation&& other):stream_(other.stream_),owner_(other.owner_) {
    other.require_owner();other.unlink();session_=std::move(other.session_);previous_=active_;active_=this;
}
Operation::~Operation() {
    if(!session_)return;
    // Reject misuse before releasing another thread's gate or resuming Managed
    // while native serialization is still held.
    try{require_owner();}catch(...){std::terminate();}
    unlink();auto& state=*session_->streams->state_;
    {std::lock_guard lock(state.mutex);auto& busy=input(stream_)?state.input_busy:state.output_busy;if(!busy || !session_->admitted)std::terminate();busy=false;--session_->admitted;}
    state.changed.notify_all();
}
Endpoint Operation::endpoint() const {require_owner();return session_->streams->state_->endpoints[index(stream_)];}
Adapter& Operation::adapter() const {require_owner();return *session_->streams->state_->adapter;}
std::stop_token Operation::cancellation() const {require_owner();return session_->cancellation.get_token();}
bool Operation::cancelled() const {return cancellation().stop_requested();}

ReadResult Context::read(std::int64_t max_bytes,std::int64_t timeout_ms) {return read_bytes(max_bytes,timeout_ms);}
ReadResult Context::read_bytes(std::int64_t max_bytes,std::int64_t timeout_ms) {
    native_only();
    if(max_bytes<0 || timeout_ms< -1)return {{},{0,false,{ErrorKind::invalid_argument}}};
    if(max_bytes==0)return {};
    if(static_cast<std::uint64_t>(max_bytes)>std::numeric_limits<std::size_t>::max())throw std::length_error("console input size overflow");
    auto request=request_for(*session_->streams->state_->adapter,session_->cancellation.get_token(),timeout_ms);
    try{auto op=acquire(Stream::input,request);return op.read_bytes(static_cast<std::size_t>(max_bytes),request);}
    catch(const AdmissionFailure& failure){return {{},{0,false,failure.error}};}
}
TextResult Context::read_char(std::int64_t timeout_ms){return read_text_impl(1,false,timeout_ms);}
TextResult Context::read_text(std::int64_t max_scalars,std::int64_t timeout_ms){return read_text_impl(max_scalars,false,timeout_ms);}
TextResult Context::read_line(std::int64_t timeout_ms){return read_text_impl(0,true,timeout_ms);}
TextResult Context::read_text_impl(std::int64_t count,bool line,std::int64_t timeout_ms) {
    native_only();
    if(count<0 || timeout_ms< -1)return {{},{},{0,false,{ErrorKind::invalid_argument}}};
    if(!line && !count)return {};
    if(static_cast<std::uint64_t>(count)>std::numeric_limits<std::size_t>::max())throw std::length_error("console scalar count overflow");
    auto request=request_for(*session_->streams->state_->adapter,session_->cancellation.get_token(),timeout_ms);
    try{auto op=acquire(Stream::input,request);return op.read_text(static_cast<std::size_t>(count),line,request);}
    catch(const AdmissionFailure& failure){return {{},{},{0,false,failure.error}};}
}
Transfer Context::write(Stream stream,Bytes bytes,std::int64_t timeout_ms) {
    native_only();
    if(stream!=Stream::output && stream!=Stream::error)return {0,false,{ErrorKind::invalid_argument}};
    if(timeout_ms< -1)return {0,false,{ErrorKind::invalid_argument}};
    auto request=request_for(*session_->streams->state_->adapter,session_->cancellation.get_token(),timeout_ms);
    if(bytes.empty())return {};
    try{auto op=acquire(stream,request);return op.write(bytes,request);}
    catch(const AdmissionFailure& failure){return {0,false,failure.error};}
}
// A read cursor is transactional over the ONE shared queue. OS buffers are
// appended before decoding; no global byte is removed until the owned result has
// been materialized. Allocation failure therefore leaves every byte recoverable.
struct Operation::InputReader {
    Operation& op;
    InputQueue& queue;
    const Request& request;
    std::size_t block=0,pos,consumed=0;
    bool pending,committed=false;
    std::optional<std::size_t> control;
    struct Item {int byte=-1;bool eof=false,unavailable=false;Error error{};bool event=false;std::optional<PhysicalKey> key;};
    struct Mark {std::size_t block,pos,consumed;bool pending;};
    InputReader(Operation& operation,const Request& req)
        :op(operation),queue(operation.session_->streams->state_->input),request(req),pos(queue.head),pending(queue.crlf){}
    ~InputReader(){clear_control();}
    Mark mark() const{return {block,pos,consumed,pending};}
    void rewind(Mark saved){block=saved.block;pos=saved.pos;consumed=saved.consumed;pending=saved.pending;}
    void clear_control() noexcept {
        if(!committed && control)queue.blocks[*control].after={};
    }
    void commit(bool eof=false) noexcept {
        clear_control();
        if(consumed)for(std::size_t n=0;n<queue.blocks.size() && n<=block;++n)if(n<block || pos>0)queue.blocks[n].key.reset();
        if(eof)queue.eof=false; // Deliver an EOF observation, not a permanent host-stream close.
        while(block<queue.blocks.size() && pos==queue.blocks[block].size && !queue.blocks[block].after && !queue.blocks[block].key){++block;pos=0;}
        for(std::size_t n=0;n<block;++n)queue.blocks.pop_front();
        queue.head=queue.blocks.empty()?0:pos;queue.crlf=pending;committed=true;
    }
    Item peek(bool fetch=true,bool poll=false,std::size_t capacity=4096) {
        for(;;){
            if(block<queue.blocks.size()){
                auto& data=queue.blocks[block];
                if(data.key && !request.keys && (!data.key->down || data.key->repeat!=1))return {-1,false,false,{ErrorKind::unsupported}};
                if(data.key && pos==0 && !data.size)return {-1,false,false,{},false,data.key};
                if(pos<data.size)return {data.bytes[pos],false,false,{},false,data.key};
                if(data.after)return {-1,false,false,data.after,true};
                ++block;pos=0;continue;
            }
            if(queue.eof)return {-1,true};
            if(!fetch)return {-1,false,true};
            if(auto error=stopped(request,op.adapter());error)return {-1,false,false,error};
            // Allocate queue storage BEFORE the OS can consume any input. deque
            // preserves block references during append; indices survive append.
            queue.blocks.emplace_back();auto& data=queue.blocks.back();
            Transfer native;
            auto next=request;if(poll)next.polling=true;
            try{native=op.read_native(std::span(data.bytes).first(std::min(capacity,data.bytes.size())),next);}
            catch(...){queue.blocks.pop_back();throw;}
            data.size=native.completed;data.after=native.error;data.key=native.key;
            if(native.eof){queue.eof=true;queue.blocks.pop_back();continue;}
            bool temporary=native.error.kind==ErrorKind::would_block || native.error.kind==ErrorKind::timed_out ||
                native.error.kind==ErrorKind::interrupted || (native.error.kind==ErrorKind::closed && request.cancellation.stop_requested());
            if(temporary){
                if(poll && !native.completed && native.error.kind==ErrorKind::would_block){queue.blocks.pop_back();return {-1,false,true};}
                control=queue.blocks.size()-1; // This request's stop/timeout must not poison another session.
            }
        }
    }
    void take(bool line){
        if(consumed==std::numeric_limits<std::size_t>::max())throw std::length_error("console read progress overflow");
        ++pos;++consumed;if(!line)pending=false;
    }
    Error boundary_error(){
        while(block<queue.blocks.size() && pos==queue.blocks[block].size){
            auto error=queue.blocks[block].after;
            ++block;pos=0;
            if(error)return error;
        }
        return {};
    }
    void copy(std::size_t skip,std::span<std::uint8_t> out) const {
        std::size_t at=0;
        for(std::size_t n=0;n<queue.blocks.size() && at<out.size();++n){
            auto& data=queue.blocks[n];auto start=n?0:queue.head;
            auto count=data.size-start;
            if(skip>=count){skip-=count;continue;}
            start+=skip;skip=0;count=std::min(data.size-start,out.size()-at);
            std::copy_n(data.bytes.begin()+start,count,out.begin()+at);at+=count;
        }
        if(at!=out.size())throw std::logic_error("invalid shared input cursor range");
    }
    ReadResult bytes(std::size_t max) {
        auto item=peek(true,false,std::min<std::size_t>(max,4096));
        Error error=item.key && item.byte<0?Error{ErrorKind::unsupported}:item.error;bool eof=item.eof;
        while(item.byte>=0 && consumed<max){
            take(false);
            // A raw read returns a short available batch, never waits to fill.
            item=peek(false);
            if(item.error){error=item.error;break;}
            if(item.byte<0)break;
        }
        if(error && block<queue.blocks.size() && pos==queue.blocks[block].size && queue.blocks[block].after)boundary_error();
        ReadResult result;result.bytes.resize(consumed);copy(0,result.bytes);
        result.transfer={consumed,eof && !consumed,error};commit(result.transfer.eof);return result;
    }
    struct Scalar {unsigned length=0;std::uint8_t first=0;bool eof=false,unavailable=false;Error error{};std::size_t offset=0;bool event=false;};
    Scalar scalar(bool line,bool fetch) {
        auto start=consumed;auto first=peek(fetch);
        if(first.byte<0)return {0,0,first.eof,first.unavailable,first.key?Error{ErrorKind::unsupported}:first.error,start,first.event};
        std::array<std::uint8_t,4> data{};data[0]=static_cast<std::uint8_t>(first.byte);
        auto width=utf8_sequence_bytes(data[0]);take(line);
        bool invalid=width==0;
        for(unsigned n=1;n<width;++n){
            auto item=peek(fetch);
            if(item.unavailable)return {0,0,false,true,{},start};
            if(item.error)return {0,0,false,false,item.error,consumed,item.event};
            if(item.eof){invalid=true;break;}
            if((item.byte&0xc0)!=0x80){invalid=true;break;}
            data[n]=static_cast<std::uint8_t>(item.byte);take(line);
        }
        if(!invalid && !utf8_scalar_bytes({reinterpret_cast<const char*>(data.data()),width},0))invalid=true;
        if(invalid){
            // Extend only through immediately available non-starters. Once the
            // error is known, never wait for a writer to send a recovery byte.
            for(;;){
                auto item=peek(true,true);
                if(item.byte<0 || utf8_sequence_bytes(static_cast<unsigned char>(item.byte)))break;
                take(line);
            }
            return {0,0,false,false,{ErrorKind::invalid_encoding},start};
        }
        return {width,data[0]};
    }
    TextResult text(std::size_t max,bool line) {
        std::size_t skip=0,scalars=0,ending=0,error_offset=0;
        Error error;bool eof=false,event=false;
        if(line && pending){
            auto next=peek();
            if(next.error){error=next.error;error_offset=consumed;event=next.event;}
            else if(next.eof){eof=true;pending=false;}
            else {pending=false;if(next.byte=='\n'){take(true);skip=1;}}
        }
        while(!error && !eof && (line || scalars<max)){
            auto saved=mark();
            auto value=scalar(line,line || scalars==0);
            if(value.unavailable){rewind(saved);break;}
            if(value.error){error=value.error;error_offset=value.offset;event=value.event;break;}
            if(value.eof){eof=consumed==skip;break;}
            ++scalars;
            if(line && (value.first=='\n' || value.first=='\r') && value.length==1){
                ending=1;pending=value.first=='\r';break; // CR never probes a following byte.
            }
        }
        // Preserve a native failure following the last consumed payload; a
        // completed logical line leaves that later event for the next read.
        if(!error && !ending){error=boundary_error();if(error)error_offset=consumed;}
        else if(error && event)boundary_error();
        TextResult result;
        if(error){result.partial.resize(consumed);copy(0,result.partial);}
        else {
            result.text.resize(consumed-skip-ending);
            copy(skip,{reinterpret_cast<std::uint8_t*>(result.text.data()),result.text.size()});
        }
        result.transfer={consumed,eof,error};result.error_offset=error_offset;commit(result.transfer.eof);return result;
    }
};

TerminalResult Operation::available(const Request& request) {
    InputReader reader(*this,request);auto item=reader.peek(false);TerminalResult result;
    if(item.unavailable){if(auto native=adapter().key_ready())return *native;item=reader.peek(true,true);}
    result.first=item.byte>=0 || item.key?1:0;
    result.transfer.error=item.error;return result; // No commit, no CRLF change.
}
KeyResult Operation::read_key(const Request& original) {
    Request request=original;request.keys=true;InputReader reader(*this,request);KeyResult result;
    auto fail=[&](Error error,bool event=false)->KeyResult {
        result.bytes.resize(reader.consumed);reader.copy(0,result.bytes);
        if(event)reader.boundary_error();result.transfer={reader.consumed,false,error};reader.commit();return std::move(result);
    };
    auto first=reader.peek();
    if(first.error)return fail(first.error,first.event);
    if(first.eof){result.transfer.error={ErrorKind::closed};reader.commit(true);return result;}
    if(first.key){
        auto key=*first.key;result.key=key.key;result.modifiers=key.modifiers;result.repeat=key.repeat;result.modifiers_known=key.known;result.key_down_known=key.down_known;result.key_down=key.down;
        auto count=reader.queue.blocks[reader.block].size-reader.pos;
        result.bytes.resize(count);reader.copy(0,result.bytes);
        auto error=reader.queue.blocks[reader.block].after;
        if(result.key==Key::character){std::string_view text(reinterpret_cast<const char*>(result.bytes.data()),result.bytes.size());if(!valid_utf8(text)){if(!error)error={ErrorKind::invalid_encoding};}else result.character.assign(result.bytes.begin(),result.bytes.end());}
        // No allocations after retiring the event.
        reader.queue.blocks[reader.block].key.reset();for(std::size_t n=0;n<count;++n)reader.take(false);if(error)reader.boundary_error();reader.pending=false;reader.commit();result.transfer={count,false,error};return result;
    }
    if(first.byte!=27){
        auto value=reader.scalar(false,true);
        if(value.error)return fail(value.error,value.event);
        result.bytes.resize(reader.consumed);reader.copy(0,result.bytes);
        result.key=value.length==1 && (first.byte==10 || first.byte==13)?Key::enter:first.byte==9?Key::tab:first.byte==8 || first.byte==127?Key::backspace:Key::character;
        if(result.key==Key::character)result.character.assign(result.bytes.begin(),result.bytes.end());
    }else {
        auto continuation=*request_for(adapter(),request.cancellation,30).deadline;
        if(!request.deadline || continuation<*request.deadline)request.deadline=continuation;
        auto sequences=adapter().key_sequences();std::string sequence;
        // Polling retains polling semantics; never turn a zero-timeout request
        // into a 30ms wait. All continuations share the original absolute limit.
        for(;;){
            // Decode bytes already owned by the shared queue before testing a
            // continuation wait. Terminfo lookup or scheduling can consume the
            // budget without any native input delay; splitting that queued key
            // would report Escape even though the complete sequence arrived.
            auto item=reader.peek(false);
            if(!sequence.empty())if(auto error=stopped(request,adapter());error && (item.unavailable || error.kind!=ErrorKind::timed_out)){if(error.kind==ErrorKind::timed_out){result.key=sequence.size()==1?Key::escape:Key::unknown;break;}return fail(error);}
            if(item.unavailable)item=reader.peek();
            if(item.error || item.unavailable || item.eof){
                if(item.error && item.error.kind!=ErrorKind::timed_out && item.error.kind!=ErrorKind::would_block)return fail(item.error,item.event);
                result.key=sequence.size()==1?Key::escape:Key::unknown;break;
            }
            sequence.push_back(static_cast<char>(item.byte));reader.take(false);
            bool prefix=false,exact=false;
            for(const auto& [bytes,key]:sequences){if(bytes==sequence){result.key=key;exact=true;break;}if(bytes.starts_with(sequence))prefix=true;}
            if(exact)break;
            // Unknown CSI/SS3 sequences retain their complete bytes, including
            // modifier parameters; no platform modifier guesses.
            bool protocol=sequence=="\033" || sequence=="\033[" || sequence=="\033O" ||
                (sequence.starts_with("\033[") && static_cast<unsigned char>(sequence.back())<0x40);
            if(!prefix && !protocol){result.key=Key::unknown;break;}
        }
        result.bytes.assign(sequence.begin(),sequence.end());
    }
    auto boundary=reader.peek(false);if(boundary.error && boundary.error.kind!=ErrorKind::timed_out && boundary.error.kind!=ErrorKind::would_block)return fail(boundary.error,boundary.event);
    result.transfer.completed=reader.consumed;reader.commit();return result;
}
ModeResult Context::begin_mode(ModeOptions options) {
    native_only();return begin_mode(options,request_for(*session_->streams->state_->adapter,session_->cancellation.get_token(),0));
}
ModeResult Context::begin_mode(ModeOptions options,const Request& request) {
    native_only();ModeResult result;auto& state=*session_->streams->state_;
    try{
        auto input=acquire(Stream::input,request);InputMode saved;
        if(auto error=stopped(request,*state.adapter);error){result.error=error;return result;}
        result.error=state.adapter->save_mode(saved);if(result.error)return result;
        {
            std::lock_guard lock(state.mutex);if(auto error=stopped(request,*state.adapter);error){result.error=error;return result;}
            if(state.next_mode==std::numeric_limits<std::int64_t>::max())throw std::length_error("terminal session identity exhausted");
            result.token=state.next_mode++;state.modes.emplace_back(Streams::State::ModeEntry{session_,std::this_thread::get_id(),vd::async_execution_context(),result.token,saved});
        }
        result.error=stopped(request,*state.adapter);
        bool applied=false;if(!result.error){applied=true;result.error=state.adapter->apply_mode(saved,options);}
        if(!result.error)result.error=stopped(request,*state.adapter);
        if(result.error){
            auto cleanup=applied?state.adapter->restore_mode(saved):Error{};
            if(cleanup)result.error.cleanup_code=cleanup.os_code?cleanup.os_code:-1;
            else {std::lock_guard lock(state.mutex);if(state.modes.empty() || state.modes.back().token!=result.token)throw std::logic_error("terminal mode stack changed during apply");state.modes.pop_back();}
            result.token=0;
        }
        return result;
    }catch(const AdmissionFailure& failure){result.error=failure.error;return result;}
}
Error Context::end_mode(std::int64_t token) {
    native_only();auto& state=*session_->streams->state_;auto request=request_for(*state.adapter,session_->cancellation.get_token(),0);
    try{auto input=acquire(Stream::input,request);InputMode saved;
        {
            std::lock_guard lock(state.mutex);auto found=std::find_if(state.modes.begin(),state.modes.end(),[&](auto& m){return m.token==token && m.session.lock()==session_;});
            if(found==state.modes.end())return {ErrorKind::closed};
            if(found+1!=state.modes.end() || found->thread!=std::this_thread::get_id() || found->execution!=vd::async_execution_context())return {ErrorKind::would_block};saved=found->saved;
        }
        auto error=state.adapter->restore_mode(saved);
        if(!error){std::lock_guard lock(state.mutex);if(state.modes.empty() || state.modes.back().token!=token)throw std::logic_error("terminal mode stack changed during restore");state.modes.pop_back();}return error;
    }catch(const AdmissionFailure& failure){return failure.error;}
}
std::int64_t Context::mode_checkpoint() const {
    native_only();auto& state=*session_->streams->state_;std::lock_guard lock(state.mutex);return state.next_mode;
}
Error Operation::restore_modes(std::int64_t checkpoint,bool owning_thread) {
    require_owner();auto& state=*session_->streams->state_;
    for(;;){InputMode saved;std::int64_t token;
        {std::lock_guard lock(state.mutex);if(state.modes.empty())return {};auto& mode=state.modes.back();if(mode.session.lock()!=session_ || mode.token<checkpoint || (owning_thread && mode.thread!=std::this_thread::get_id()))return {};saved=mode.saved;token=mode.token;}
        auto error=state.adapter->restore_mode(saved);if(error)return error;
        {std::lock_guard lock(state.mutex);if(state.modes.empty() || state.modes.back().token!=token)throw std::logic_error("terminal mode stack changed during cleanup");state.modes.pop_back();}
    }
}
Error Context::finish_modes(std::int64_t checkpoint) {
    native_only();auto& state=*session_->streams->state_;std::optional<Operation> input;
    {
        std::lock_guard lock(state.mutex);if(session_->closing || state.modes.empty())return {};auto& mode=state.modes.back();
        if(mode.session.lock()!=session_ || mode.thread!=std::this_thread::get_id() || mode.token<checkpoint)return {};
        if(state.input_busy)return {ErrorKind::would_block};
        state.input_busy=true;++session_->admitted;Operation lease(session_,Stream::input);input.emplace(std::move(lease));
    }
    return input->restore_modes(checkpoint,true);
}
Error Context::restore_modes() {
    native_only();check_drain();auto& state=*session_->streams->state_;std::optional<Operation> input;
    {
        std::unique_lock lock(state.mutex);state.changed.wait(lock,[&]{return session_->admitted==0;});
        if(state.modes.empty() || state.modes.back().session.lock()!=session_)return {};
        if(state.input_busy)return {ErrorKind::would_block};
        state.input_busy=true;++session_->admitted;Operation lease(session_,Stream::input);input.emplace(std::move(lease));
    }
    return input->restore_modes(1,false);
}

namespace {
// Keep exceptional diagnostics with this call, never in shared session state:
// another entry/shutdown thread must not steal the original caller's cleanup.
template<class F> auto scoped_input_mode(Context& context,std::int64_t token,std::stop_token stop,F action) -> decltype(action()) {
    auto restore=[&]{auto error=context.end_mode(token);if(error.kind==ErrorKind::closed && !error.os_code && stop.stop_requested())error={};return error;};
    try{
        auto result=action();auto error=restore();
        if(error){if(result.transfer.error){if(!result.transfer.error.cleanup_code)result.transfer.error.cleanup_code=error.os_code?error.os_code:-1;}else result.transfer.error=error;}
        return result;
    }catch(...){
        auto primary=std::current_exception();Error cleanup;std::exception_ptr secondary;
        try{cleanup=restore();}catch(...){secondary=std::current_exception();}
        if(cleanup || secondary)throw ModeCleanupFailure(primary,cleanup,secondary);
        std::rethrow_exception(primary);
    }
}
}
KeyResult Context::read_key(bool echo,std::int64_t timeout) {
    native_only();KeyResult result;if(timeout< -1){result.transfer.error={ErrorKind::invalid_argument};return result;}
    auto& state=*session_->streams->state_;auto request=request_for(*state.adapter,session_->cancellation.get_token(),timeout);request.keys=true;
    auto mode=begin_mode({false,false,false,true},request);if(mode.error){result.transfer.error=mode.error;return result;}
    return scoped_input_mode(*this,mode.token,request.cancellation,[&]{
        try{auto input=acquire(Stream::input,request);result=input.read_key(request);
            if(echo && !result.transfer.error && !result.character.empty() && (!result.key_down_known || result.key_down)){
                auto output=acquire(Stream::output,request);Bytes bytes;auto repeat=std::max(1u,result.repeat);if(result.character.size()>bytes.max_size()/repeat)throw std::length_error("key echo extent");bytes.reserve(result.character.size()*repeat);for(unsigned n=0;n<repeat;++n)bytes.insert(bytes.end(),result.character.begin(),result.character.end());auto written=output.write(bytes,request);if(written.error)result.transfer.error=written.error;
            }
        }catch(const AdmissionFailure& failure){result.transfer.error=failure.error;}
        return std::move(result);
    });
}
TerminalResult Context::key_available() {
    native_only();TerminalResult result;auto& state=*session_->streams->state_;auto request=request_for(*state.adapter,session_->cancellation.get_token(),0);request.keys=true;
    try{auto input=acquire(Stream::input,request);InputMode saved;result.transfer.error=state.adapter->save_mode(saved);if(result.transfer.error)return result;return input.available(request);}
    catch(const AdmissionFailure& failure){result.transfer.error=failure.error;return result;}
}

TerminalResult Operation::before_report(const Request& request) {
    InputReader reader(*this,request);TerminalResult result;
    for(;;){if(auto error=stopped(request,adapter());error){result.transfer.error=error;return result;}auto item=reader.peek(true,true);if(item.byte>=0){reader.take(false);continue;}if(item.error || item.eof){result.transfer.error=item.error?item.error:Error{ErrorKind::unsupported};return result;}if(reader.consumed>static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))throw std::length_error("terminal queue extent");result.first=static_cast<std::int64_t>(reader.consumed);return result;}
}
TerminalResult Operation::cursor_report(const Request& request,std::int64_t columns,std::int64_t rows,std::size_t ignored) {
    InputReader reader(*this,request);TerminalResult result;unsigned state=0;std::int64_t row=0,column=0;std::size_t start=0;
    auto restart=[&](int byte,std::size_t offset){state=byte==27?1:0;start=offset;row=column=0;};
    for(;;){
        if(auto error=stopped(request,adapter());error){result.transfer.error=error;return result;}
        auto item=reader.peek();if(item.byte<0){result.transfer.error=item.error?item.error:Error{ErrorKind::io_failure};return result;}
        auto offset=reader.consumed;reader.take(false);auto byte=item.byte;
        if(offset<ignored)continue;
        if(!state){restart(byte,offset);continue;}
        if(state==1){if(byte=='[')state=2;else restart(byte,offset);continue;}
        if(byte>='0' && byte<='9'){
            auto& number=state==2?row:column;
            if(number>(std::numeric_limits<std::int64_t>::max()-(byte-'0'))/10){restart(byte,offset);continue;}
            number=number*10+(byte-'0');continue;
        }
        if(state==2 && byte==';' && row>0){state=3;continue;}
        if(state==3 && byte=='R' && row>0 && column>0 && row<=rows && column<=columns){
            auto boundary=reader.peek(false);if(boundary.error){result.transfer.error=boundary.error;return result;}
            // Erase ONLY the report in place. Keep all unrelated bytes, native
            // error boundaries and CRLF continuation in the one shared queue.
            auto end=reader.consumed;std::size_t base=0;
            auto& queue=reader.queue;
            for(std::size_t n=0;n<queue.blocks.size();++n){auto& block=queue.blocks[n];auto head=n?0:queue.head;auto count=block.size-head;auto lo=std::max(start,base),hi=std::min(end,base+count);if(lo<hi){auto first=head+lo-base,last=head+hi-base;std::move(block.bytes.begin()+last,block.bytes.begin()+block.size,block.bytes.begin()+first);block.size-=last-first;}base+=count;}
            result.first=column-1;result.second=row-1;return result;
        }
        restart(byte,offset);
    }
}

ReadResult Operation::read_bytes(std::size_t count,const Request& request) {
    require_owner();if(!input(stream_) || request.cancellation!=cancellation())throw std::logic_error("invalid shared input lease/request");
    if(!count)return {};
    InputReader reader(*this,request);return reader.bytes(count);
}
TextResult Operation::read_text(std::size_t count,bool line,const Request& request) {
    require_owner();if(!input(stream_) || request.cancellation!=cancellation())throw std::logic_error("invalid shared text lease/request");
    InputReader reader(*this,request);return reader.text(count,line);
}
Transfer Operation::read(std::span<std::uint8_t> bytes,const Request& request) {
    auto result=read_bytes(bytes.size(),request);std::copy(result.bytes.begin(),result.bytes.end(),bytes.begin());return result.transfer;
}
Transfer Operation::read_native(std::span<std::uint8_t> bytes,const Request& request) {
    require_owner();
    if(!input(stream_) || request.cancellation!=cancellation())throw std::logic_error("invalid console read lease/request");
    if(bytes.empty())return {};
    for(;;){
        if(auto error=stopped(request,adapter());error)return {0,false,error};
        auto result=request.keys?adapter().read_key(bytes,request):adapter().read(bytes,request);
        if(result.completed>bytes.size() || (result.eof && (result.completed || result.error)))throw std::logic_error("invalid console adapter read result");
        if(result.completed || result.eof || result.key)return result;
        if(!result.error.cleanup_code && (result.error.kind==ErrorKind::interrupted || result.error.kind==ErrorKind::would_block)){
            if(auto error=stopped(request,adapter(),result.error.os_code);error)return {0,false,error};
            if(!request.polling)continue;
        }
        if(!result.error)result.error={ErrorKind::io_failure};
        return result;
    }
}
Transfer Operation::write(std::span<const std::uint8_t> bytes,const Request& request) {
    require_owner();
    if(input(stream_) || request.cancellation!=cancellation())throw std::logic_error("invalid console write lease/request");
    Transfer result;
    while(result.completed<bytes.size()){
        if(auto error=stopped(request,adapter());error){result.error=error;return result;}
        auto part=adapter().write(stream_,bytes.subspan(result.completed),request);
        if(part.completed>bytes.size()-result.completed || part.eof)throw std::logic_error("invalid console adapter write result");
        result.completed+=part.completed;
        if(!part.error.partial_utf16_units && !part.error.cleanup_code && (part.error.kind==ErrorKind::interrupted || part.error.kind==ErrorKind::would_block)){
            if(auto error=stopped(request,adapter(),part.error.os_code);error){result.error=error;return result;}
            if(!request.polling)continue;
        }
        if(part.error){result.error=part.error;return result;}
        if(!part.completed){result.error={ErrorKind::io_failure};return result;}
    }
    return result;
}
}
