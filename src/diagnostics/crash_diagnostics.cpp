#include "void/crash_diagnostics.hpp"
#include "void/path.hpp"
#include "void/lsp.hpp"
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include "../native_windows.hpp"
#else
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace vd::crash {
namespace {
namespace fs = std::filesystem;
constexpr unsigned history_size = 32, report_limit = 16, byte_limit = 16384, scan_limit = 256;
constexpr const char* operations[]{"startup", "cli", "frontend", "jit_compile", "jit_execute",
    "aot_build", "lsp_request", "lsp_analysis", "lsp_completion", "lsp_hover", "lsp_shutdown",
    "external_execute", "finished"};
static_assert(std::size(operations) == static_cast<unsigned>(Operation::count));
// Only atomic integers cross the boundary. No pointers or variable-size data
// from a possibly corrupted worker are dereferenced by the supervisor.
struct Shared {
    std::atomic<unsigned> next{0};
    std::atomic<unsigned> sanitizer{0};
    std::array<std::atomic<unsigned>, history_size> events{};
};
static_assert(std::atomic<unsigned>::is_always_lock_free);
Shared* current = nullptr;
unsigned current_owner = 0;
unsigned process_id() noexcept {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned>(getpid());
#endif
}
bool owns_context() noexcept {return current && current_owner==process_id();}
std::string_view current_build;
#if VOID_CRASH_SANITIZERS
extern "C" void __sanitizer_set_death_callback(void (*)());
void sanitizer_death() {
    // A forked child inherits both this callback and the MAP_SHARED address.
    // It may never mark its still-running parent as a sanitizer casualty.
    if(owns_context())current->sanitizer.store(current_owner,std::memory_order_relaxed);
}
#endif
void arm(Shared* shared) {
    current=shared;
    current_owner=process_id();
    context(Operation::startup);
#if VOID_CRASH_SANITIZERS
    __sanitizer_set_death_callback(sanitizer_death);
#endif
}
struct File {
    FILE* value = nullptr;
    explicit File(const fs::path& path) {
#ifdef _WIN32
        value = _wfopen(path.c_str(), L"rb");
#else
        auto fd=open(path.c_str(),O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        struct stat st{};
        if(fd>=0) {
            if(!fstat(fd,&st) && S_ISREG(st.st_mode))value=fdopen(fd,"rb");
            if(!value)close(fd);
        }
#endif
    }
    ~File(){if(value)fclose(value);}
};
std::string hash(File& file) {
    if(!file.value)return "unavailable";
    // Fixed memory and a finite read budget even if an executable is replaced
    // or extended during collection. Huge/unseekable binaries remain unknown.
    if(fseek(file.value,0,SEEK_END))return "unavailable";
    auto length=ftell(file.value);
    if(length<0 || length>1024L*1024*1024 || fseek(file.value,0,SEEK_SET))return "unavailable";
    llvm::SHA256 digest;
    std::array<std::uint8_t, 65536> bytes;
    auto remaining=static_cast<std::size_t>(length);
    while(remaining) {
        auto size=fread(bytes.data(),1,std::min(remaining,bytes.size()),file.value);
        if(!size)return "unavailable";
        digest.update({bytes.data(),size});remaining-=size;
    }
    if(ferror(file.value))return "unavailable";
    auto result=digest.final();
    std::string text;
    for(auto b:result){text+="0123456789abcdef"[b>>4];text+="0123456789abcdef"[b&15];}
    return text;
}
fs::path normalized_directory(fs::path path) {
    if(!path.is_absolute())throw std::runtime_error("report directory must be absolute");
    path=path.lexically_normal();
    // lexically_normal retains a terminal separator for "leaf/" and "leaf/.".
    // Strip only that empty component; preserve the root and all leaf checks.
    while(path.has_relative_path() && path.filename().empty())path=path.parent_path();
    if(path.filename().empty())throw std::runtime_error("report directory must have a private leaf");
    return path;
}
fs::path report_directory() {
#ifdef _WIN32
    if(auto explicit_path=_wgetenv(L"VOID_CRASH_DIRECTORY"); explicit_path && *explicit_path) {
        fs::path path(explicit_path);
        if(!path.is_absolute())throw std::runtime_error("report directory must be absolute");
        return normalized_directory(std::move(path));
    }
#else
    if(auto explicit_path=std::getenv("VOID_CRASH_DIRECTORY"); explicit_path && *explicit_path) {
        auto path=utf8_path(explicit_path);
        if(!path.is_absolute())throw std::runtime_error("report directory must be absolute");
        return normalized_directory(std::move(path));
    }
#endif
#ifdef _WIN32
    auto root=_wgetenv(L"LOCALAPPDATA");
    if(!root || !*root)throw std::runtime_error("LOCALAPPDATA unavailable");
    return normalized_directory(fs::path(root)/"VOID-Crashes");
#elif defined(__APPLE__)
    auto root=std::getenv("HOME");
    if(!root || !*root)throw std::runtime_error("HOME unavailable");
    return normalized_directory(utf8_path(root)/"Library/Logs/VOID-Crashes");
#else
    if(auto root=std::getenv("XDG_STATE_HOME");root && *root) {
        auto path=utf8_path(root);
        if(!path.is_absolute())throw std::runtime_error("XDG_STATE_HOME must be absolute");
        return normalized_directory(path/"VOID-Crashes");
    }
    auto root=std::getenv("HOME");
    if(!root || !*root)throw std::runtime_error("HOME unavailable");
    // The existing parent must exist; diagnostic collection never creates a
    // directory hierarchy or changes permissions on a developer's directory.
    return normalized_directory(utf8_path(root)/".local/state/VOID-Crashes");
#endif
}
bool owned_name(const fs::path& path) {
    auto name=path.filename().string();
    return name.starts_with("crash-") && name.ends_with(".json") && name.size()<100 &&
        std::all_of(name.begin()+6,name.end()-5,[](char c){return (c>='0' && c<='9') || c=='-';});
}
#ifndef _WIN32
struct Fd {int value;~Fd(){if(value>=0)close(value);}};
bool private_object(int fd, bool directory) {
    struct stat st{};
    return fstat(fd,&st)==0 && st.st_uid==geteuid() && !(st.st_mode&0077) &&
        (directory?S_ISDIR(st.st_mode):S_ISREG(st.st_mode)) && (directory || st.st_nlink==1);
}
#endif
#ifdef _WIN32
// Both owned-worker and explicit native launches inherit only the admitted
// standard streams (plus the worker's context mapping, when present).
struct WindowsLaunch {
    std::vector<native_windows::Handle> standards;
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;
    STARTUPINFOEXW startup{};
    bool inherit = false;
    explicit WindowsLaunch(HANDLE extra=nullptr) {
        using native_windows::fail;
        std::vector<HANDLE> handles;
        if(extra)handles.push_back(extra);
        startup.StartupInfo.cb=sizeof(STARTUPINFOW);
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
        HANDLE* destinations[]{&startup.StartupInfo.hStdInput,&startup.StartupInfo.hStdOutput,&startup.StartupInfo.hStdError};
        DWORD identifiers[]{STD_INPUT_HANDLE,STD_OUTPUT_HANDLE,STD_ERROR_HANDLE};
        for(unsigned i=0;i<3;++i) {
            HANDLE copy=nullptr;
            auto original=GetStdHandle(identifiers[i]);
            if(original && original!=INVALID_HANDLE_VALUE &&
               !DuplicateHandle(GetCurrentProcess(),original,GetCurrentProcess(),&copy,0,TRUE,DUPLICATE_SAME_ACCESS))
                fail("Cannot inherit diagnostic standard stream");
            *destinations[i]=copy;
            if(copy){standards.emplace_back(copy);handles.push_back(copy);}
        }
        if(handles.empty())return;
        SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
        storage.resize(size);
        auto list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if(!InitializeProcThreadAttributeList(list,1,0,&size))fail("Cannot initialize diagnostic launch");
        if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles.data(),handles.size()*sizeof(HANDLE),nullptr,nullptr)) {
            auto error=GetLastError();DeleteProcThreadAttributeList(list);
            fail("Cannot isolate diagnostic handles",error);
        }
        attributes=list;
        startup.lpAttributeList=list;
        startup.StartupInfo.cb=sizeof(startup);
        inherit=true;
    }
    ~WindowsLaunch(){if(attributes)DeleteProcThreadAttributeList(attributes);}
    DWORD flags() const noexcept {
        // Debug attachment bypasses SetUnhandledExceptionFilter callbacks.
        // Preserve application behavior; Windows exception observation is unsupported.
        return CREATE_UNICODE_ENVIRONMENT|(attributes?EXTENDED_STARTUPINFO_PRESENT:0);
    }
};
struct WindowsExit {
    DWORD code = 1;
    bool known = false;
};
WindowsExit wait_windows_process(HANDLE process) noexcept {
    // A DWORD alone cannot distinguish main/ExitProcess from an exception.
    // Only the owned worker's sanitizer callback can authorize a report.
    DWORD code=1;
    if(WaitForSingleObject(process,INFINITE)==WAIT_OBJECT_0 && GetExitCodeProcess(process,&code))
        return {code,true};
    std::fputs("VOID process exit status unavailable.\n",stderr);
    return {};
}
BOOL WINAPI collector_console(DWORD event) {return event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT;}
#endif
void save(const fs::path& directory,const std::string& name,const std::string& bytes) {
    if(bytes.size()>byte_limit)throw std::runtime_error("report exceeded bound");
#ifdef _WIN32
    using namespace native_windows;
    auto pins=pin_directories(directory.parent_path());
    PrivateAccess access;
    if(!CreateDirectoryW(directory.c_str(),&access.attributes) && GetLastError()!=ERROR_ALREADY_EXISTS)
        fail("Cannot create crash directory");
    pins.push_back(pin_directory(directory));access.verify(pins.back().value);
    Handle lock(CreateFileW((directory/L".lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,
        &access.attributes,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(lock.value==INVALID_HANDLE_VALUE)fail("Cannot lock crash directory");
    information(lock.value);
#else
    Fd parent{open(directory.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC)};
    if(parent.value<0)throw std::runtime_error("report parent unavailable");
    auto leaf=directory.filename().string();
    if(mkdirat(parent.value,leaf.c_str(),0700) && errno!=EEXIST)
        throw std::runtime_error("cannot create report directory");
    Fd dir{openat(parent.value,leaf.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
    if(dir.value<0 || !private_object(dir.value,true) || flock(dir.value,LOCK_EX|LOCK_NB))
        throw std::runtime_error("report directory is not private or is busy");
    // Enumerate through the admitted directory handle, never a redirected path.
    DIR* listing=fdopendir(dup(dir.value));
    if(!listing)throw std::runtime_error("cannot inspect retention");
    struct Listing {DIR* p;~Listing(){closedir(p);}} listing_owner{listing};
#endif
    std::vector<std::string> retained;
    unsigned scanned=0;
#ifdef _WIN32
    for(const auto& entry:fs::directory_iterator(directory)) {
        if(++scanned>scan_limit)throw std::runtime_error("report directory entry bound exceeded");
        if(!owned_name(entry.path()))continue;
        if(!fs::is_regular_file(entry.symlink_status()) || entry.file_size()>byte_limit)
            throw std::runtime_error("unexpected report entry");
        retained.push_back(entry.path().filename().string());
    }
#else
    errno=0;
    while(auto entry=readdir(listing)) {
        if(++scanned>scan_limit)throw std::runtime_error("report directory entry bound exceeded");
        if(!owned_name(entry->d_name))continue;
        Fd report{openat(dir.value,entry->d_name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK)};
        struct stat st{};
        if(report.value<0 || !private_object(report.value,false) || fstat(report.value,&st) || st.st_size>byte_limit)
            throw std::runtime_error("unexpected report entry");
        retained.emplace_back(entry->d_name);
        errno=0;
    }
    if(errno)throw std::runtime_error("cannot inspect retention");
#endif
    std::sort(retained.begin(),retained.end());
    // Bound storage before publication, including concurrent incidents.
    while(retained.size()>=report_limit) {
#ifdef _WIN32
        if(!DeleteFileW((directory/retained.front()).c_str()))fail("Cannot rotate crash report");
#else
        if(unlinkat(dir.value,retained.front().c_str(),0))throw std::runtime_error("cannot rotate report");
#endif
        retained.erase(retained.begin());
    }
#ifdef _WIN32
    Handle output(CreateFileW((directory/name).c_str(),GENERIC_WRITE,0,&access.attributes,
        CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(output.value==INVALID_HANDLE_VALUE)fail("Cannot create crash report");
    DWORD written=0;
    if(!WriteFile(output.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) || written!=bytes.size())
        fail("Cannot write crash report");
#else
    // Optional persistence must not replace the worker's status with SIGXFSZ.
    struct rlimit limit{};
    if(getrlimit(RLIMIT_FSIZE,&limit) || (limit.rlim_cur!=RLIM_INFINITY && bytes.size()>limit.rlim_cur))
        throw std::runtime_error("report file size limit");
    Fd output{openat(dir.value,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)};
    if(output.value<0)throw std::runtime_error("cannot create report");
    std::size_t position=0;
    while(position<bytes.size()) {
        auto count=write(output.value,bytes.data()+position,bytes.size()-position);
        if(count<0 && errno==EINTR)continue;
        if(count<=0){unlinkat(dir.value,name.c_str(),0);throw std::runtime_error("cannot write report");}
        position+=count;
    }
#endif
}
void report(const char* role, unsigned long pid, std::int64_t status, bool signal,
            File& executable, std::string_view build, Shared* shared,
            bool child=false, bool timeout_possible=false) noexcept {
    try {
        llvm::json::Array recent;
        const unsigned end=shared->next.load(std::memory_order_acquire);
        const unsigned size=std::min(end,history_size);
        const char* last="unavailable";
        for(unsigned i=end-size;i!=end;++i) {
            unsigned op=shared->events[i%history_size].load(std::memory_order_acquire);
            const char* text=op<std::size(operations)?operations[op]:"unavailable";
            recent.emplace_back(text);last=text;
        }
        auto milliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        static std::atomic<unsigned> serial{0};
#ifdef _WIN32
        auto collector_pid=GetCurrentProcessId();
#else
        auto collector_pid=getpid();
#endif
        const auto name="crash-"+std::to_string(milliseconds)+"-"+std::to_string(collector_pid)+"-"+std::to_string(++serial)+".json";
        const bool sanitizer=pid && shared->sanitizer.load(std::memory_order_relaxed)==pid;
        std::string classification=signal?"fatal_signal":"sanitizer_finding";
        if(sanitizer)classification="sanitizer_finding";
        if(child)classification=timeout_possible?"child_crash_or_timeout":"child_crash";
        std::string boundary=role;
        if(boundary=="voidc" && std::string_view(last)=="jit_execute")boundary="jit-program-or-runtime";
        llvm::json::Object evidence{{"schema",1},{"incident",name},{"process",role},
            {"boundary",boundary},{"pid",pid?llvm::json::Value(pid):llvm::json::Value(nullptr)},{"unix_time_ms",milliseconds},
            {"collector_version",VOID_COMPILER_VERSION},
            {"version",std::string_view(role)=="voidls"?llvm::json::Value(vd::lsp::server_version):
                std::string_view(role)=="voidc"?llvm::json::Value(VOID_COMPILER_VERSION):llvm::json::Value(nullptr)},
            {"llvm",LLVM_VERSION_STRING},
            {"target",LLVM_DEFAULT_TARGET_TRIPLE},{"binary_sha256",hash(executable)},
            {"compiler_build_identity",std::string(build)},{"classification",classification},
            {"termination_code",status},{"termination_namespace",(child && status==-2)?"llvm_execute_and_wait":signal?"posix_signal":
#ifdef _WIN32
                "windows_exit"
#else
                "posix_exit"
#endif
            },
            {"recent_operations",std::move(recent)},{"context_trust","best-effort worker breadcrumbs; termination is OS-observed"},
            {"stack",nullptr},{"stack_unavailable_reason","no safe native stack collector attached; use an explicitly collected OS dump"},
            {"source_location",nullptr},{"explanation",nullptr},{"delegated_child_exit",false},
            {"identity_observed_at",child?"after_child_termination":"supervised_executable_opened_at_start"}};
        std::string bytes;
        llvm::raw_string_ostream out(bytes);out<<llvm::formatv("{0:2}",llvm::json::Value(std::move(evidence)))<<'\n';
        auto directory=report_directory();save(directory,name,bytes);
        std::fprintf(stderr,"VOID crash report: %s\n",path_utf8(directory/name).c_str());
    } catch(...) {std::fputs("VOID crash report unavailable (private storage, retention or write failure).\n",stderr);}
}
#ifndef _WIN32
volatile sig_atomic_t child_pid=0;
void forward(int signal, siginfo_t* info, void*) {
    auto saved=errno;
    // Terminal-generated signals already reach the foreground process group.
    // Forwarding them again would give an interactive program duplicate input.
    if(child_pid>0 && (!info || info->si_pid!=0))kill(child_pid,signal);
    errno=saved;
}
bool fatal(unsigned signal) {
    return signal==SIGSEGV || signal==SIGABRT || signal==SIGBUS || signal==SIGILL ||
        signal==SIGFPE || signal==SIGTRAP
#ifdef SIGSYS
        || signal==SIGSYS
#endif
        ;
}
#endif
}
void context(Operation op) noexcept {
    if(!owns_context())return;
    auto next=current->next.load(std::memory_order_relaxed);
    current->events[next%history_size].store(static_cast<unsigned>(op),std::memory_order_release);
    current->next.store(next+1,std::memory_order_release);
}
std::optional<int> start(const char* role,const char* executable,std::string_view build,bool force) noexcept {
#ifdef _WIN32
    // A one-use inherited mapping handle prevents worker recursion. Clear it
    // before any compiler-owned or developer child can inherit the environment.
    if(auto worker=std::getenv("VOID_CRASH_WORKER")) {
        char* end=nullptr;auto number=std::strtoull(worker,&end,10);
        auto handle=reinterpret_cast<HANDLE>(static_cast<uintptr_t>(number));
        const bool valid=end!=worker && !*end && number;
        _putenv_s("VOID_CRASH_WORKER","");
        if(valid) {
            current=static_cast<Shared*>(MapViewOfFile(handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
            CloseHandle(handle);
            if(current)arm(current);
        }
        current_build=build;
        return {}; // Fail open, without relaunching if a mapping is unavailable.
    }
#endif
    current_build=build;
#ifdef _WIN32
    // The explicit native boundary launches the actual application directly;
    // there is no status-delegating voidc worker to confuse with the application.
    if(std::string_view(role)=="aot-program")return {};
#endif
    auto enabled=std::getenv("VOID_CRASH_DIAGNOSTICS");
    if(!force && (!enabled || std::strcmp(enabled,"1")))return {};
    try {
        auto self=utf8_path(llvm::sys::fs::getMainExecutable(executable,reinterpret_cast<void*>(&start)));
        File binary(std::string_view(role)=="aot-program"?utf8_path(executable):self);
#ifdef _WIN32
        using namespace native_windows;
        SECURITY_ATTRIBUTES inherit{sizeof(inherit),nullptr,TRUE};
        Handle mapping(CreateFileMappingW(INVALID_HANDLE_VALUE,&inherit,PAGE_READWRITE,0,sizeof(Shared),nullptr));
        if(!mapping.value)fail("Cannot allocate crash context");
        auto shared=static_cast<Shared*>(MapViewOfFile(mapping.value,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
        if(!shared)fail("Cannot map crash context");
        new(shared) Shared{};
        struct View {void* p;~View(){UnmapViewOfFile(p);}} view{shared};
        // Preserve the exact Unicode command line, including literal arguments.
        std::wstring command=GetCommandLineW();
        auto raw=GetEnvironmentStringsW();if(!raw)fail("Cannot copy launch environment");
        std::vector<std::wstring> settings;
        for(auto p=raw;*p;p+=wcslen(p)+1)settings.emplace_back(p);
        FreeEnvironmentStringsW(raw);
        settings.push_back(L"VOID_CRASH_WORKER="+std::to_wstring(reinterpret_cast<uintptr_t>(mapping.value)));
        std::sort(settings.begin(),settings.end(),[](const auto& a,const auto& b){
            return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_LESS_THAN;
        });
        std::wstring environment;
        for(const auto& setting:settings){environment+=setting;environment+=L'\0';}
        environment+=L'\0';
        WindowsLaunch launch(mapping.value);
        PROCESS_INFORMATION info{};
        if(!CreateProcessW(self.c_str(),command.data(),nullptr,nullptr,launch.inherit,
            launch.flags(),environment.data(),nullptr,&launch.startup.StartupInfo,&info))
            fail("Cannot start diagnostic worker");
        Handle process(info.hProcess),thread(info.hThread);
        SetConsoleCtrlHandler(collector_console,TRUE);
        const auto observed=wait_windows_process(process.value);
        const bool sanitizer=shared->sanitizer.load(std::memory_order_relaxed)==info.dwProcessId;
        if(observed.known && sanitizer)
            report(role,info.dwProcessId,observed.code,false,binary,build,shared);
        ExitProcess(observed.code);
#else
        auto shared=static_cast<Shared*>(mmap(nullptr,sizeof(Shared),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0));
        if(shared==MAP_FAILED)throw std::runtime_error("cannot allocate crash context");
        new(shared) Shared{};
        struct sigaction inherited_chld{},normal_chld{};
        normal_chld.sa_handler=SIG_DFL;sigemptyset(&normal_chld.sa_mask);
        // An inherited SIG_IGN/SA_NOCLDWAIT must not auto-reap our worker.
        if(sigaction(SIGCHLD,&normal_chld,&inherited_chld)) {
            munmap(shared,sizeof(Shared));throw std::runtime_error("cannot supervise child exit");
        }
        // Block forwarding signals across fork/handler installation. The worker
        // restores the exact inherited mask and dispositions before doing work.
        sigset_t blocked,previous;
        sigemptyset(&blocked);for(int s:{SIGINT,SIGTERM,SIGHUP,SIGQUIT})sigaddset(&blocked,s);
        if(sigprocmask(SIG_BLOCK,&blocked,&previous)){sigaction(SIGCHLD,&inherited_chld,nullptr);munmap(shared,sizeof(Shared));throw std::runtime_error("cannot block supervisor signals");}
        auto child=fork();
        if(child<0){sigaction(SIGCHLD,&inherited_chld,nullptr);sigprocmask(SIG_SETMASK,&previous,nullptr);munmap(shared,sizeof(Shared));throw std::runtime_error("cannot start diagnostic worker");}
        if(!child) {
            sigaction(SIGCHLD,&inherited_chld,nullptr);
            arm(shared);
            sigprocmask(SIG_SETMASK,&previous,nullptr);
            return {};
        }
        child_pid=child;
        struct sigaction action{};action.sa_sigaction=forward;action.sa_flags=SA_SIGINFO;sigemptyset(&action.sa_mask);
        for(int s:{SIGINT,SIGTERM,SIGHUP,SIGQUIT})sigaction(s,&action,nullptr);
        sigprocmask(SIG_SETMASK,&previous,nullptr);
        // Writing a report to a disconnected output must not replace the exit.
        signal(SIGPIPE,SIG_IGN);
        int status=0;pid_t waited;
        do {waited=waitpid(child,&status,0);}while(waited<0 && errno==EINTR);
        child_pid=0;
        if(waited<0)_exit(1);
        if(WIFSIGNALED(status) && fatal(WTERMSIG(status)))report(role,child,WTERMSIG(status),true,binary,build,shared);
        else if(WIFEXITED(status) && shared->sanitizer.load(std::memory_order_relaxed)==static_cast<unsigned>(child))
            report(role,child,WEXITSTATUS(status),false,binary,build,shared);
        munmap(shared,sizeof(Shared));
        if(WIFSIGNALED(status)) {
            auto signal_number=WTERMSIG(status);
            // Native crash/core belongs to the worker. Avoid a second core of
            // the collector while preserving the parent's WIFSIGNALED result.
            struct rlimit no_core{0,0};setrlimit(RLIMIT_CORE,&no_core);
            signal(signal_number,SIG_DFL);
            sigset_t one;sigemptyset(&one);sigaddset(&one,signal_number);sigprocmask(SIG_UNBLOCK,&one,nullptr);
            kill(getpid(),signal_number);_exit(128+signal_number);
        }
        _exit(WIFEXITED(status)?WEXITSTATUS(status):1);
#endif
    } catch(...) {std::fputs("VOID crash diagnostics unavailable; continuing without collection.\n",stderr);return {};}
}
void child_termination(const fs::path& executable,int status,bool timeout_possible) noexcept {
#ifdef _WIN32
    // LLVM's Windows result alone cannot distinguish a normal return from an
    // exception (including -2). Preserve existing tool diagnostics; never guess.
    (void)executable;(void)status;(void)timeout_possible;
#else
    if(status!=-2)return;
    auto enabled=std::getenv("VOID_CRASH_DIAGNOSTICS");
    if(!enabled || std::strcmp(enabled,"1"))return;
    try {
        File binary(executable);
        Shared empty{};
        // No child PID or signal is exposed by LLVM ExecuteAndWait. Do not
        // derive one from an error message or attribute it to the compiler.
        report("native-tool",0,status,false,binary,current_build,owns_context()?current:&empty,true,timeout_possible);
    } catch(...) {std::fputs("VOID child crash report unavailable.\n",stderr);}
#endif
}
int execute_external(int argc,char** argv) {
    if(argc<1 || !utf8_path(argv[0]).is_absolute())throw std::invalid_argument("crashes run requires an absolute executable path");
    context(Operation::external_execute);
#ifdef _WIN32
    std::wstring command;
    for(int i=0;i<argc;++i) {
        if(i)command+=L' ';command+=L'"';std::size_t slashes=0;
        for(auto c:utf8_path(argv[i]).wstring()) {
            if(c==L'\\'){++slashes;continue;}
            command.append(slashes,L'\\');if(c==L'"')command.append(slashes+1,L'\\');slashes=0;command+=c;
        }
        command.append(2*slashes,L'\\');command+=L'"';
    }
    WindowsLaunch launch;
    PROCESS_INFORMATION info{};
    if(!CreateProcessW(utf8_path(argv[0]).c_str(),command.data(),nullptr,nullptr,launch.inherit,
        launch.flags(),nullptr,nullptr,&launch.startup.StartupInfo,&info))
        throw std::runtime_error("cannot launch diagnostic program");
    native_windows::Handle process(info.hProcess),thread(info.hThread);
    SetConsoleCtrlHandler(collector_console,TRUE);
    // No debugger or target handler: unsupported native exception collection
    // is declined, while the application's exact exit value is preserved.
    const auto observed=wait_windows_process(process.value);
    ExitProcess(observed.code);
#else
    execv(argv[0],argv);
    throw std::runtime_error("cannot launch diagnostic program");
#endif
}
}
