// Bootstrap platform glue only. Catalog authentication, toolchain transactions,
// activation, recovery and every proxy launch remain in the existing VOID app.
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <string>
#include <vector>
#include <chrono>
#ifdef _WIN32
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <sys/xattr.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <copyfile.h>
#include <sys/acl.h>
#endif
#endif
#ifndef VOIDUP_BOOTSTRAP_ID
#error A bootstrap content identity is required
#endif
namespace fs = std::filesystem;
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
fs::path native_path(const fs::path& path) {
#ifdef _WIN32
    auto value=fs::absolute(path).lexically_normal().make_preferred().wstring();
    if(value.rfind(L"\\\\?\\",0)==0)return value;
    if(value.rfind(L"\\\\",0)==0)return L"\\\\?\\UNC\\"+value.substr(2);
    return L"\\\\?\\"+value;
#else
    return path;
#endif
}
#ifndef _WIN32
struct descriptor {
    int fd=-1;explicit descriptor(int value):fd(value){}~descriptor(){if(fd>=0)close(fd);}
    descriptor(const descriptor&)=delete;descriptor& operator=(const descriptor&)=delete;
};
int installation_fd=-1;
fs::path installation_path;
bool beneath_installation(const fs::path& path) {
    auto relative=path.lexically_relative(installation_path);
    return installation_fd>=0 && !relative.empty() && relative!=fs::path(".") && *relative.begin()!="..";
}
int parent_descriptor(const fs::path& path) {
    auto relative=path.lexically_relative(installation_path).parent_path();int fd=dup(installation_fd);require(fd>=0,"Cannot retain integration root");
    for(const auto& component:relative){if(component==".")continue;int next=openat(fd,component.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(fd);require(next>=0,"Unsafe integration ancestor");fd=next;
        struct stat s{};if(fstat(fd,&s)||!S_ISDIR(s.st_mode)){close(fd);throw std::runtime_error("Unsafe integration ancestor type");}}
    return fd;
}
void root_still_visible() {
    struct stat a{},b{};require(installation_fd>=0&&!fstat(installation_fd,&a)&&!lstat(installation_path.c_str(),&b)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&S_ISDIR(b.st_mode),"Integration root changed; retry setup");
}
void write_fd(int fd,const std::string& bytes) {
    std::size_t offset=0;
    while(offset<bytes.size()){auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);if(n<0&&errno==EINTR)continue;require(n>0,"Cannot write integration file");offset+=std::size_t(n);}
    require(!fsync(fd),"Cannot synchronize integration file");
}
#endif
std::string env(const char* name) { auto* value=std::getenv(name); return value?value:""; }
fs::path image() {
#ifdef _WIN32
    std::wstring buffer(32768,L'\0');auto n=GetModuleFileNameW(nullptr,buffer.data(),DWORD(buffer.size()));
    require(n && n<buffer.size(),"Cannot locate bootstrap image");buffer.resize(n);return buffer;
#elif defined(__APPLE__)
    std::uint32_t n=0;_NSGetExecutablePath(nullptr,&n);std::string buffer(n,'\0');
    require(!_NSGetExecutablePath(buffer.data(),&n),"Cannot locate bootstrap image");return fs::canonical(buffer.c_str());
#else
    return fs::read_symlink("/proc/self/exe");
#endif
}
fs::path home() {
#ifdef _WIN32
    PWSTR raw=nullptr;require(SHGetKnownFolderPath(FOLDERID_Profile,0,nullptr,&raw)==S_OK,"Cannot locate user home");
    fs::path result(raw);CoTaskMemFree(raw);return result;
#else
    auto value=env("HOME");if(!value.empty())return value;
    auto* user=getpwuid(geteuid());require(user && user->pw_dir,"Cannot locate user home");return user->pw_dir;
#endif
}
fs::path default_root() {
#ifdef _WIN32
    PWSTR raw=nullptr;require(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&raw)==S_OK,"Cannot locate user data");
    fs::path result(raw);CoTaskMemFree(raw);return result/"voidup";
#elif defined(__APPLE__)
    return home()/"Library/Application Support/voidup";
#else
    auto value=env("XDG_DATA_HOME");return (value.empty()?home()/".local/share":fs::path(value))/"voidup";
#endif
}
#if defined(__APPLE__)
// Only the default bootstrap root may admit the OS /var alias. The manager
// receives the resulting physical spelling; Runtime admission remains no-follow.
struct macos_root_selection {
    struct pin {int parent;std::string name;std::unique_ptr<descriptor> held;struct stat identity;bool system;};
    fs::path path;
    std::vector<pin> pins;
    std::vector<std::string> names;
    std::size_t admitted=0;
    struct stat alias{};
    std::string target,physical;
    bool translated=false;
    static uid_t system_uid() { return 0; }
    static bool same(const struct stat& a,const struct stat& b) {
        return a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_mode==b.st_mode && a.st_uid==b.st_uid;
    }
    static void native(bool ok,const char* message) {
        if(!ok)throw std::system_error(errno,std::generic_category(),message);
    }
    static bool system_acl(int fd) {
#ifdef __APPLE__
        auto acl=acl_get_fd_np(fd,ACL_TYPE_EXTENDED);if(!acl)return errno==ENOENT;
        acl_entry_t entry{};bool safe=true;
        for(int selected=ACL_FIRST_ENTRY;;selected=ACL_NEXT_ENTRY) {
            if(acl_get_entry(acl,selected,&entry)){safe=errno==EINVAL;break;}
            acl_tag_t tag{};
            if(acl_get_tag_type(entry,&tag) || tag!=ACL_EXTENDED_DENY){safe=false;break;}
        }
        acl_free(acl);return safe;
#else
        (void)fd;return true; // Linux test model; not a Darwin ACL claim.
#endif
    }
    static void trusted(int fd,const struct stat& info) {
        require(S_ISDIR(info.st_mode) && info.st_uid==system_uid() && !(info.st_mode&0022) && system_acl(fd),
                "Unsafe macOS system directory");
    }
    bool retain(int parent,const std::string& name,bool system,bool missing=false) {
        auto held=std::make_unique<descriptor>(-1);
        held->fd=openat(parent,name.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(held->fd<0) {
            auto code=errno;
            if(missing && code==ENOENT) {
                struct stat visible{};auto status=fstatat(parent,name.c_str(),&visible,AT_SYMLINK_NOFOLLOW);
                require(status<0 && errno==ENOENT,"Default root ancestor changed; retry setup");
                return false;
            }
            throw std::system_error(code,std::generic_category(),"Cannot open default root ancestor");
        }
        struct stat info{};native(!fstat(held->fd,&info),"Cannot inspect default root descriptor");
        require(S_ISDIR(info.st_mode),"Default root ancestor is not a directory");
        if(system)trusted(held->fd,info);
        pins.push_back({parent,name,std::move(held),info,system});return true;
    }
    std::string read_target() const {
        char bytes[4096];auto n=readlinkat(pins.front().held->fd,"var",bytes,sizeof bytes);
        native(n>=0,"Cannot read macOS system alias");
        require(n<ssize_t(sizeof bytes),"macOS system alias exceeds its bound");return std::string(bytes,std::size_t(n));
    }
    explicit macos_root_selection(const fs::path& requested):path(requested) {
        const fs::path system_root = "/";
        auto alias_path=(system_root/"var").string();auto raw=requested.string();
        if(raw.compare(0,alias_path.size()+1,alias_path+"/")!=0)return;
        auto suffix=raw.substr(alias_path.size()+1);
        require(raw.size()<=32768 && raw.find('\0')==std::string::npos,"Invalid default root path");
        for(std::size_t start=0;start<=suffix.size();) {
            auto end=suffix.find('/',start);auto name=suffix.substr(start,end==std::string::npos?end:end-start);
            require(!name.empty() && name!="." && name!=".." && name.find('\\')==std::string::npos,"Invalid default root component");
            names.push_back(name);if(end==std::string::npos)break;start=end+1;
        }
        retain(AT_FDCWD,system_root.string(),true);
        native(!fstatat(pins.front().held->fd,"var",&alias,AT_SYMLINK_NOFOLLOW),"Cannot inspect macOS system alias");
        if(S_ISDIR(alias.st_mode))return; // Already physical; no alias exception needed.
        if(!S_ISLNK(alias.st_mode))throw std::system_error(ENOTDIR,std::generic_category(),"macOS var is not a directory or system alias");
        require(alias.st_uid==system_uid(),"Refusing a user-controlled macOS system alias");
        physical=(system_root/"private/var").string();target=read_target();
        require(target=="private/var" || target==physical,"Unexpected macOS system alias target");
        retain(pins.front().held->fd,"private",true);
        retain(pins.back().held->fd,"var",true);
        path=fs::path(physical)/suffix;translated=true;
        for(;admitted<names.size();++admitted)if(!retain(pins.back().held->fd,names[admitted],false,true))break;
        verify();
    }
    void complete() {
        if(!translated)return;
        verify();
        for(;admitted<names.size();++admitted)retain(pins.back().held->fd,names[admitted],false);
        verify();
    }
    void verify() const {
        if(!translated)return;
        struct stat visible{};
        native(!fstatat(pins.front().held->fd,"var",&visible,AT_SYMLINK_NOFOLLOW),"Cannot recheck macOS system alias");
        auto current_target=read_target();struct stat after{};
        native(!fstatat(pins.front().held->fd,"var",&after,AT_SYMLINK_NOFOLLOW),"Cannot recheck macOS system alias");
        require(same(alias,visible) && same(visible,after) && current_target==target,"macOS system alias changed; retry setup");
        for(const auto& saved:pins) {
            struct stat held{};native(!fstat(saved.held->fd,&held),"Cannot recheck default root descriptor");
            native(!fstatat(saved.parent,saved.name.c_str(),&visible,AT_SYMLINK_NOFOLLOW),"Cannot recheck default root ancestor");
            require(same(saved.identity,held) && same(held,visible) && S_ISDIR(visible.st_mode),"Default root ancestor changed; retry setup");
            if(saved.system)trusted(saved.held->fd,held);
        }
    }
};
#endif
void safe(const fs::path& path, bool directory, bool private_mode=true) {
    require(!fs::is_symlink(fs::symlink_status(native_path(path))),"Refusing a linked integration path");
#ifdef _WIN32
    auto attributes=GetFileAttributesW(native_path(path).c_str());require(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"Refusing a reparse integration path");
    require(directory?bool(attributes&FILE_ATTRIBUTE_DIRECTORY):!(attributes&FILE_ATTRIBUTE_DIRECTORY),"Unexpected integration file type");
    // VOIDUP pins ordinary no-reparse directories; new children inherit ACLs.
    (void)private_mode;
#else
    (void)private_mode;
    struct stat s{};require(!::lstat(path.c_str(),&s),"Cannot inspect integration path");
    require(directory?S_ISDIR(s.st_mode):(S_ISREG(s.st_mode)&&s.st_nlink==1),"Integration path is not an ordinary object");
#endif
}
void directory(const fs::path& path,bool exclusive=false) {
#ifndef _WIN32
    if(beneath_installation(path)) {
        descriptor parent(parent_descriptor(path));auto leaf=path.filename();
        require(!mkdirat(parent.fd,leaf.c_str(),0777)||(!exclusive&&errno==EEXIST),"Cannot create integration directory");
        descriptor child(openat(parent.fd,leaf.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat s{};
        require(child.fd>=0&&!fstat(child.fd,&s)&&S_ISDIR(s.st_mode),"Unsafe integration directory");return;
    }
#endif
    auto created=fs::create_directory(native_path(path));require(created||!exclusive,"Cannot exclusively create integration directory");
    safe(path,true);
}
struct integration_lock {
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
    explicit integration_lock(const fs::path& root) {
        file=CreateFileW(native_path(root/"writer.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        require(file!=INVALID_HANDLE_VALUE,"VOIDUP is busy; retry setup");
    }
    ~integration_lock(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
#else
    int file=-1;
    explicit integration_lock(const fs::path& root) {
        file=open((root/"writer.lock").c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC);
        require(file>=0,"Cannot open VOIDUP writer lock");struct stat s{};
        if(fstat(file,&s)||!S_ISREG(s.st_mode)||s.st_nlink!=1||flock(file,LOCK_EX|LOCK_NB)){
            close(file);file=-1;throw std::runtime_error("VOIDUP is busy or its writer lock is unsafe");
        }
    }
    ~integration_lock(){if(file>=0)close(file);}
#endif
};
std::string read(const fs::path& path, std::size_t limit) {
#ifndef _WIN32
    if(beneath_installation(path)) {
        descriptor parent(parent_descriptor(path));descriptor file(openat(parent.fd,path.filename().c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));struct stat s{};
        require(file.fd>=0&&!fstat(file.fd,&s)&&S_ISREG(s.st_mode)&&s.st_nlink==1&&s.st_size>=0&&std::uintmax_t(s.st_size)<=limit,"Unsafe retained bootstrap file");
        std::string result;char bytes[8192];for(;;){auto n=::read(file.fd,bytes,sizeof bytes);if(n<0&&errno==EINTR)continue;require(n>=0,"Cannot read retained bootstrap");if(!n)break;result.append(bytes,n);require(result.size()<=limit,"Retained bootstrap grew beyond its bound");}return result;
    }
#endif
    require(fs::file_size(native_path(path))<=limit,"Integration input exceeds its bound");
    std::ifstream in(native_path(path),std::ios::binary);require(bool(in),"Cannot open integration input");
    std::string result;char buffer[8192];
    while(in){in.read(buffer,sizeof buffer);auto count=std::size_t(in.gcount());require(count<=limit-result.size(),"Integration input grew beyond its bound");result.append(buffer,count);}
    require(in.eof()&&!in.bad(),"Cannot read integration input");return result;
}
void write_new(const fs::path& path,const std::string& bytes,bool executable=false) {
#ifdef _WIN32
    HANDLE f=CreateFileW(native_path(path).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(f!=INVALID_HANDLE_VALUE,"Cannot exclusively create integration file");DWORD written=0;
    bool ok=WriteFile(f,bytes.data(),DWORD(bytes.size()),&written,nullptr) && written==bytes.size() && FlushFileBuffers(f);CloseHandle(f);
    require(ok,"Cannot complete integration file");
#else
    descriptor parent(beneath_installation(path)?parent_descriptor(path):-1);
    descriptor f(parent.fd>=0?openat(parent.fd,path.filename().c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,executable?0777:0666):open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,executable?0777:0666));
    require(f.fd>=0,"Cannot exclusively create integration file");write_fd(f.fd,bytes);
#endif
}
void replace(const fs::path& source,const fs::path& destination) {
#ifdef _WIN32
    require(MoveFileExW(native_path(source).c_str(),native_path(destination).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Cannot publish integration file");
#else
    descriptor from(parent_descriptor(source)),to(parent_descriptor(destination));
    require(!renameat(from.fd,source.filename().c_str(),to.fd,destination.filename().c_str()),"Cannot publish integration file");
    require(!fsync(to.fd),"Integration published but directory sync failed; retry setup");
#endif
}
#ifdef _WIN32
std::wstring quote(const std::wstring& value) {
    std::wstring out=L"\"";unsigned slashes=0;
    for(auto c:value){if(c==L'\\'){++slashes;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');slashes=0;out+=c;}
    out.append(slashes*2,L'\\');return out+L'\"';
}
#endif
#ifdef _WIN32
struct native_handle {
    HANDLE value=nullptr;
    explicit native_handle(HANDLE handle):value(handle){}
    ~native_handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    native_handle(const native_handle&)=delete;
    native_handle& operator=(const native_handle&)=delete;
};
struct process_attributes {
    LPPROC_THREAD_ATTRIBUTE_LIST list=nullptr;
    process_attributes() {
        SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
        require(size>0,"Cannot size launcher process attributes");
        list=static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(),0,size));
        require(list!=nullptr,"Cannot allocate launcher process attributes");
        if(!InitializeProcThreadAttributeList(list,1,0,&size)){
            HeapFree(GetProcessHeap(),0,list);list=nullptr;
            throw std::runtime_error("Cannot initialize launcher process attributes");
        }
    }
    ~process_attributes(){if(list){DeleteProcThreadAttributeList(list);HeapFree(GetProcessHeap(),0,list);}}
    process_attributes(const process_attributes&)=delete;
    process_attributes& operator=(const process_attributes&)=delete;
};
#endif
// Editor tasks and LSP clients signal the directly spawned proxy. Keep its
// redirected manager/compiler descendants in one group and relay cancellation.
// Interactive terminals retain their foreground group and normal job control.
#ifndef _WIN32
volatile sig_atomic_t forwarded_group=0;
void forward_cancellation(int signal) {
    const auto saved=errno;const auto group=forwarded_group;
    if(group>0)kill(-group,signal);
    errno=saved;
}
struct cancellation_scope {
    bool active=false;
    sigset_t blocked{},original{};
    struct sigaction previous[4]{};
    const int signals[4]={SIGINT,SIGTERM,SIGQUIT,SIGHUP};
    explicit cancellation_scope(bool enabled) {
        if(!enabled)return;
        sigemptyset(&blocked);for(auto signal:signals)sigaddset(&blocked,signal);
        require(!sigprocmask(SIG_BLOCK,&blocked,&original),"Cannot prepare launcher cancellation");
        struct sigaction action{};action.sa_handler=forward_cancellation;sigemptyset(&action.sa_mask);
        unsigned installed=0;
        for(;installed<4;++installed)if(sigaction(signals[installed],&action,&previous[installed]))break;
        if(installed!=4){while(installed){--installed;sigaction(signals[installed],&previous[installed],nullptr);}sigprocmask(SIG_SETMASK,&original,nullptr);throw std::runtime_error("Cannot relay launcher cancellation");}
        active=true;
    }
    void child() {
        if(!active)return;
        if(setpgid(0,0))_exit(127);
        for(unsigned i=0;i<4;++i)sigaction(signals[i],&previous[i],nullptr);
        sigprocmask(SIG_SETMASK,&original,nullptr);
    }
    void watch(pid_t pid) {
        if(!active)return;
        // Both sides set the group, so cancellation cannot race child startup.
        const auto status=setpgid(pid,pid);
        require(!status||errno==EACCES||errno==ESRCH,"Cannot group launcher descendants");
        forwarded_group=pid;
        require(!sigprocmask(SIG_SETMASK,&original,nullptr),"Cannot enable launcher cancellation");
    }
    ~cancellation_scope() {
        if(!active)return;
        sigprocmask(SIG_BLOCK,&blocked,nullptr);forwarded_group=0;
        for(unsigned i=0;i<4;++i)sigaction(signals[i],&previous[i],nullptr);
        sigprocmask(SIG_SETMASK,&original,nullptr);
    }
};
#endif
int run(const fs::path& executable,const std::vector<std::string>& args) {
#ifdef _WIN32
    auto wide=[](const std::string& s){auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);require(n||s.empty(),"Invalid UTF-8 argument");std::wstring w(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),w.data(),n);return w;};
    auto command=quote(executable.wstring());for(const auto& a:args)command+=L" "+quote(wide(a));
    // Creation-time assignment avoids the orphan window between starting a
    // suspended process and assigning it. This job handle is never inherited.
    native_handle job(CreateJobObjectW(nullptr,nullptr));require(job.value!=nullptr,"Cannot create launcher job");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof limits),"Cannot configure launcher job");
    process_attributes attributes;
    require(UpdateProcThreadAttribute(attributes.list,0,PROC_THREAD_ATTRIBUTE_JOB_LIST,&job.value,sizeof job.value,nullptr,nullptr),"Cannot assign launcher job at process creation");
    STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof si;si.lpAttributeList=attributes.list;PROCESS_INFORMATION pi{};
    si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.StartupInfo.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.StartupInfo.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&si.StartupInfo,&pi),"Cannot launch native VOIDUP");
    native_handle process(pi.hProcess),thread(pi.hThread);
    require(WaitForSingleObject(process.value,INFINITE)==WAIT_OBJECT_0,"Cannot wait for native VOIDUP");
    DWORD code=1;require(GetExitCodeProcess(process.value,&code),"Cannot read native VOIDUP exit code");return int(code);
#else
    std::vector<char*> argv{const_cast<char*>(executable.c_str())};for(const auto& a:args)argv.push_back(const_cast<char*>(a.c_str()));argv.push_back(nullptr);
    cancellation_scope cancellation(!isatty(STDIN_FILENO)&&!isatty(STDERR_FILENO));
    auto pid=fork();require(pid>=0,"Cannot start native VOIDUP");if(!pid){cancellation.child();execv(executable.c_str(),argv.data());_exit(127);}
    cancellation.watch(pid);
    int status=0;while(waitpid(pid,&status,0)<0)require(errno==EINTR,"Cannot wait for native VOIDUP");
    return WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
#endif
}
std::string shell_quote(std::string value) {std::string out="'";for(char c:value)out+=c=='\''?"'\"'\"'":std::string(1,c);return out+"'";}
#ifndef _WIN32
void copy_profile_access(int source,int target,const struct stat& original) {
    struct stat staged{};require(!fstat(target,&staged),"Cannot inspect staged shell profile");
    // ACL USER_OBJ/GROUP_OBJ entries refer to these identities. Copying ACL
    // bytes onto a different owning group silently changes legitimate access.
    if(staged.st_uid!=original.st_uid||staged.st_gid!=original.st_gid)
        require(!fchown(target,original.st_uid,original.st_gid),"Cannot preserve shell profile ownership");
    require(!fchmod(target,original.st_mode&0777),"Cannot preserve shell profile permissions");
#ifdef __linux__
    constexpr auto name="system.posix_acl_access";
    auto size=fgetxattr(source,name,nullptr,0);
    if(size<0){
        require(errno==ENODATA||errno==ENOTSUP,"Cannot read shell profile ACL");
        // A staged file may inherit a default ACL that the original never had.
        require(!fremovexattr(target,name)||errno==ENODATA||errno==ENOTSUP,"Cannot preserve absent shell profile ACL");
        require(!fchmod(target,original.st_mode&0777),"Cannot preserve shell profile permissions");return;
    }
    require(size<=65536,"Shell profile ACL exceeds its bound");
    std::vector<char> acl(static_cast<std::size_t>(size));
    require(fgetxattr(source,name,acl.data(),acl.size())==size,"Shell profile ACL changed; retry setup");
    require(!fsetxattr(target,name,acl.data(),acl.size(),0),"Cannot preserve shell profile ACL");
#elif defined(__APPLE__)
    require(!fcopyfile(source,target,nullptr,COPYFILE_ACL),"Cannot preserve shell profile ACL");
#endif
}
#endif
void integrate_path(const fs::path& bin) {
#ifdef _WIN32
    struct registry_key {HKEY value=nullptr;~registry_key(){if(value)RegCloseKey(value);}} key;
    require(RegCreateKeyExW(HKEY_CURRENT_USER,L"Environment",0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&key.value,nullptr)==ERROR_SUCCESS,"Cannot open per-user environment");
    DWORD type=REG_EXPAND_SZ,size=0;
    constexpr auto flags=RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ|RRF_NOEXPAND;
    auto status=RegGetValueW(key.value,nullptr,L"Path",flags,&type,nullptr,&size);
    require((status==ERROR_SUCCESS||status==ERROR_FILE_NOT_FOUND)&&size<=65534&&size%sizeof(wchar_t)==0,"Invalid per-user PATH");
    std::wstring value;
    if(status==ERROR_SUCCESS){
        value.resize(size/sizeof(wchar_t)+1,L'\0');DWORD capacity=DWORD(value.size()*sizeof(wchar_t));
        require(RegGetValueW(key.value,nullptr,L"Path",flags,&type,value.data(),&capacity)==ERROR_SUCCESS,"Cannot read per-user PATH");
        value.resize(capacity/sizeof(wchar_t));while(!value.empty()&&value.back()==L'\0')value.pop_back();
        require(value.find(L'\0')==value.npos,"Per-user PATH contains an embedded terminator");
    } else type=REG_EXPAND_SZ;
    auto candidate=bin.wstring();require(candidate.find(L';')==candidate.npos,"Install root contains the PATH separator");
    auto comparable=[&](std::wstring part){
        if(part.size()>=2&&part.front()==L'"'&&part.back()==L'"')part=part.substr(1,part.size()-2);
        if(type==REG_EXPAND_SZ){
            auto length=ExpandEnvironmentStringsW(part.c_str(),nullptr,0);require(length>0&&length<=32767,"Cannot compare expanded PATH entry");
            std::wstring expanded(length,L'\0');auto written=ExpandEnvironmentStringsW(part.c_str(),expanded.data(),length);
            require(written>0&&written<=length,"PATH environment changed; retry setup");expanded.resize(written-1);part=std::move(expanded);
        }
        std::replace(part.begin(),part.end(),L'/',L'\\');
        part=fs::path(part).lexically_normal().wstring();
        while(part.size()>3&&part.back()==L'\\')part.pop_back();return part;
    };
    auto expected=comparable(candidate);bool present=false;std::size_t start=0;
    while(start<=value.size()){
        auto end=value.find(L';',start);auto part=comparable(value.substr(start,end-start));
        if(_wcsicmp(part.c_str(),expected.c_str())==0)present=true;
        if(end==value.npos)break;start=end+1;
    }
    if(!present){value=candidate+(value.empty()?L"":L";"+value);require(value.size()<32767,"Per-user PATH is too long");require(RegSetValueExW(key.value,L"Path",0,type,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS,"Cannot save per-user PATH");}
    DWORD_PTR ignored{};SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"Environment"),SMTO_ABORTIFHUNG,5000,&ignored);
#else
    auto location=home();safe(location,true,false);int dir=open(location.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);require(dir>=0,"Cannot open home for PATH integration");
    auto legacy="\n# VOIDUP terminal tools\nexport PATH="+shell_quote(bin.string())+":\"$PATH\"\n";
    auto line="\n# VOIDUP terminal tools\ncase \":${PATH-}:\" in\n  *:"+shell_quote(bin.string())+":*) ;;\n  *) export PATH="+shell_quote(bin.string())+"${PATH:+:\"$PATH\"} ;;\nesac\n";
    require(bin.string().find(':')==std::string::npos,"Install root contains the PATH separator");
    std::vector<std::string> profiles{".profile",".bashrc",".zshenv"};
    for(const auto& optional:{".bash_profile",".bash_login"})if(fs::exists(location/optional))profiles.emplace_back(optional);
    for(const auto& name:profiles){
        descriptor source(openat(dir,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));std::string bytes;struct stat old{};bool exists=source.fd>=0;
        if(!exists)require(errno==ENOENT,"Refusing an unsafe shell profile");
        else {require(!fstat(source.fd,&old)&&S_ISREG(old.st_mode)&&old.st_nlink==1&&old.st_size<=1048576,"Refusing an unsafe shell profile");
            char buffer[8192];for(;;){auto n=::read(source.fd,buffer,sizeof buffer);if(n<0&&errno==EINTR)continue;require(n>=0,"Cannot read shell profile");if(!n)break;bytes.append(buffer,n);require(bytes.size()<=1048576,"Shell profile grew beyond its bound");}}
        if(bytes.find(line)!=std::string::npos)continue;
        // Migrate only our exact previous snippet, preserving other contents.
        auto previous=bytes.find(legacy);
        if(previous!=std::string::npos)bytes.erase(previous,legacy.size());
        auto temporary=".voidup-profile-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        descriptor output(openat(dir,temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0666));require(output.fd>=0,"Cannot stage shell profile");
        if(exists){descriptor writable(openat(dir,name.c_str(),O_WRONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));require(writable.fd>=0,"Shell profile is not writable");struct stat current{};require(!fstat(writable.fd,&current)&&current.st_dev==old.st_dev&&current.st_ino==old.st_ino,"Shell profile changed; retry setup");copy_profile_access(source.fd,output.fd,old);}
        write_fd(output.fd,bytes+line);
        struct stat now{};auto found=fstatat(dir,name.c_str(),&now,AT_SYMLINK_NOFOLLOW);
        require(exists?(!found&&now.st_dev==old.st_dev&&now.st_ino==old.st_ino&&now.st_size==old.st_size&&now.st_mtime==old.st_mtime):(found<0&&errno==ENOENT),"Shell profile changed; retry setup");
        require(!renameat(dir,temporary.c_str(),dir,name.c_str()),"Cannot publish shell profile");require(!fsync(dir),"Profile published but sync failed; retry setup");
    }close(dir);
#endif
}
} // namespace

int run_main(int argc,char** argv) try {
    auto self=image();auto name=self.stem().string();std::vector<std::string> args(argv+1,argv+argc);
    if(name=="voidc"||name=="voidls"||name=="voidup") {
        auto root=self.parent_path().parent_path();auto manager=root/"bootstraps"/VOIDUP_BOOTSTRAP_ID/"bin"/
#ifdef _WIN32
            "voidup.exe";
#else
            "voidup";
#endif
        safe(root,true);safe(manager,false);
        bool explicit_root=false;
        if(name=="voidup")for(std::size_t i=0;i<args.size();i+=2){
            if(args[i]=="--root"){explicit_root=true;break;}
            if(args[i]!="--catalog"&&args[i]!="--ca-file"&&args[i]!="--timeout-ms"&&args[i]!="--status")break;
        }
        std::vector<std::string> forwarded;
        if(!explicit_root)forwarded={"--root",root.u8string()};
        if(name!="voidup")forwarded.insert(forwarded.end(),{"run","default",name,"--"});forwarded.insert(forwarded.end(),args.begin(),args.end());return run(manager,forwarded);
    }
    bool path_enabled=true;args.erase(std::remove_if(args.begin(),args.end(),[&](const auto& a){if(a=="--no-path"){path_enabled=false;return true;}return false;}),args.end());
    if(std::find(args.begin(),args.end(),"--help")!=args.end()){std::cout<<"Run this installer to install the latest compatible stable VOID.\n--no-path leaves shell/registry settings untouched. Other arguments go to native VOIDUP.\n";return 0;}
    auto root=default_root();for(std::size_t i=0;i+1<args.size();++i)if(args[i]=="--root")root=fs::u8path(args[i+1]);
    require(root.is_absolute(),"Installation root must be absolute");auto bundle=self.parent_path().parent_path();auto manager=self.parent_path()/
#ifdef _WIN32
        "voidup.exe";
#else
        "voidup";
#endif
#ifdef __APPLE__
    std::unique_ptr<macos_root_selection> selected_root;
    // Foundation's application-support lookup need not follow an overridden
    // HOME. Install into the same root that this bootstrap will integrate.
    if(std::find(args.begin(),args.end(),"--root")==args.end()) {
        selected_root=std::make_unique<macos_root_selection>(root);root=selected_root->path;
        args.insert(args.begin(),{"--root",root.u8string()});
        selected_root->verify();
    }
#endif
    auto status=run(manager,args);if(status)return status;
#ifdef __APPLE__
    if(selected_root)selected_root->complete();
#endif
    safe(root,true);integration_lock lock(root);
#ifndef _WIN32
    descriptor anchor(open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));require(anchor.fd>=0,"Cannot retain integration root");installation_fd=anchor.fd;installation_path=root;root_still_visible();
#endif
    directory(root/"bootstraps");directory(root/"bin");auto retained=root/"bootstraps"/VOIDUP_BOOTSTRAP_ID;
    auto marker=root/"bin/voidup-integration.txt";
    if(!fs::exists(native_path(marker))){
        require(fs::is_empty(native_path(root/"bin")),"Refusing to adopt unrelated integration files");write_new(marker,"VOIDUP platform integration v1\n");
    } else {safe(marker,false);require(read(marker,128)=="VOIDUP platform integration v1\n","Unknown integration marker");}
    if(!fs::exists(native_path(retained))) {
        require(std::distance(fs::directory_iterator(native_path(root/"bootstraps")),fs::directory_iterator{})<64,"Retained bootstrap copy limit reached");
        auto pending=root/"bootstraps"/(std::string(VOIDUP_BOOTSTRAP_ID)+".pending-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));directory(pending,true);
        std::size_t total=0,count=0;
        for(const auto& item:fs::recursive_directory_iterator(native_path(bundle))) {
            require(!item.is_symlink(),"Bootstrap packages must contain no links");auto destination=pending/item.path().lexically_relative(native_path(bundle));
            if(item.is_directory()){directory(destination);continue;}
            require(item.is_regular_file()&&++count<=10000,"Unexpected bootstrap member");auto bytes=read(item.path(),128*1024*1024);total+=bytes.size();require(total<=512*1024*1024,"Bootstrap copy exceeds local limit");
            write_new(destination,bytes,item.path().parent_path()==self.parent_path());
        }
#ifdef _WIN32
        require(MoveFileW(native_path(pending).c_str(),native_path(retained).c_str()),"Cannot publish retained bootstrap");
#else
        descriptor parent(parent_descriptor(pending));
#ifdef __APPLE__
        require(!renameatx_np(parent.fd,pending.filename().c_str(),parent.fd,retained.filename().c_str(),RENAME_EXCL),"Cannot publish retained bootstrap");
#else
        require(!syscall(SYS_renameat2,parent.fd,pending.filename().c_str(),parent.fd,retained.filename().c_str(),1),"Cannot publish retained bootstrap");
#endif
        require(!fsync(parent.fd),"Bootstrap published but sync failed; retry setup");
#endif
    } else {
        safe(retained,true);
        std::size_t source_count=0,target_count=0;
        for(const auto& item:fs::recursive_directory_iterator(native_path(bundle))) {
            require(!item.is_symlink(),"Bootstrap source contains a link");auto target=retained/item.path().lexically_relative(native_path(bundle));safe(target,item.is_directory());
            if(item.is_regular_file())require(read(item.path(),128*1024*1024)==read(target,128*1024*1024),"Retained bootstrap differs; refusing replacement");++source_count;
        }
        for(const auto& item:fs::recursive_directory_iterator(native_path(retained))){(void)item;++target_count;}
        require(source_count==target_count,"Unexpected retained bootstrap contents");
    }
    auto copied=read(self,16*1024*1024);
    for(const auto& tool:{"voidc","voidls","voidup"}) {
        auto target=root/"bin"/tool;
#ifdef _WIN32
        target+=".exe";
#endif
        if(fs::exists(native_path(target)))safe(target,false);auto temporary=target;temporary+=".new-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());write_new(temporary,copied,true);replace(temporary,target);
    }
#ifndef _WIN32
    root_still_visible();
#endif
#ifdef __APPLE__
    if(selected_root)selected_root->verify();
#endif
    if(path_enabled)integrate_path(root/"bin");
    std::cout<<"VOID is installed. Open a new terminal and run: voidc --version\n";return 0;
} catch(const std::exception& e) {std::cerr<<"VOIDUP setup: "<<e.what()<<"\n";return 1;}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> storage;storage.reserve(argc);
    for(int i=0;i<argc;++i){auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(!n)return 2;
        std::string value(n,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,value.data(),n,nullptr,nullptr);value.pop_back();storage.push_back(std::move(value));}
    std::vector<char*> args;for(auto& s:storage)args.push_back(s.data());return run_main(argc,args.data());
}
#else
int main(int argc,char** argv){return run_main(argc,argv);}
#endif
