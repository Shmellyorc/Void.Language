#include "native_approval.hpp"
#include "frontend/project_digest.hpp"
#include "frontend/project_native.hpp"
#include "runtime/host_posix.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/project_resolution.hpp"
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <map>
#include <system_error>
#include <utility>
#if defined(_WIN32)
#include "native_windows.hpp"
#else
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace vd {
namespace {
namespace fs = std::filesystem;
using namespace project;
constexpr std::string_view record_header = "VOID native approval 1\n";
constexpr std::size_t record_limit = 16 * 1024, approval_limit = 4096;
constexpr auto terminal_review =
    "To review and save approval, open an ordinary interactive terminal in the project and run 'voidc run .'.";

bool valid_id(std::string_view id) {
    return id.size()==64 && std::ranges::all_of(id,[](char c) {
        return (c>='0' && c<='9') || (c>='a' && c<='f');
    });
}

std::string display(std::string_view bytes) {
    // Do not let an untrusted path or label inject terminal controls.
    std::string escaped;
    llvm::raw_string_ostream out(escaped);
    llvm::printEscapedString(llvm::StringRef(bytes.data(),bytes.size()),out);
    return escaped;
}

fs::path store_path() {
    auto path=native_approval_store();
    if(path.empty())throw NativeApprovalRequired("User data is unavailable for native approval. Pass --allow-native for this invocation.");
    return path;
}

void require_private_directory(const fs::path& path) {
#if defined(_WIN32)
    auto pins=native_windows::pin_directories(path);
    native_windows::PrivateAccess{}.verify(pins.back().value);
#else
    const auto root=inspect_creation_root(path);
    const auto descriptor=open(root.path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(descriptor<0)throw std::system_error(errno,std::generic_category(),"Cannot open native approval directory");
    struct Held {int fd;~Held(){::close(fd);}} held{descriptor};
    struct stat info{};
    if(fstat(descriptor, &info) || !S_ISDIR(info.st_mode) || info.st_uid!=geteuid() || (info.st_mode & 0077) || !host::safe_acl(descriptor))
        throw std::runtime_error("Native approvals require a private user directory.");
#endif
}

template<class Action>
void with_approval_store(const fs::path& path,bool create,const Action& action) {
    if(fs::exists(fs::symlink_status(path)) || project_path_alias(path))require_private_directory(path);
    with_user_package_store(path,create,[&](const CreationRoot& root) {
        require_private_directory(root.path);
        action(root);
    },true);
}

std::optional<std::string> read_record(const CreationRoot& root,std::string_view id) {
    const auto path=root.path/utf8_path(id);
    if(!fs::exists(fs::symlink_status(path)) && !project_path_alias(path))return {};
    require_private_directory(path);
    auto entries=fs::directory_iterator(path);
    if(entries==fs::directory_iterator{} || entries->path().filename()!="approval")
        throw std::runtime_error("Malformed native approval record.");
    if(++entries!=fs::directory_iterator{})throw std::runtime_error("Malformed native approval record.");
    auto record=read_project_input(path/"approval",record_limit);
    if(!record.starts_with(record_header) || record.size()<=record_header.size()+id.size()+2 ||
       std::string_view(record).substr(record_header.size(),id.size())!=id ||
       record[record_header.size()+id.size()]!='\n' || record.back()!='\n')
        throw std::runtime_error("Malformed native approval record.");
    const auto label=std::string_view(record).substr(record_header.size()+id.size()+1);
    if(!std::ranges::all_of(label.substr(0,label.size()-1),[](char c){return c>=' ' && c<='~';}))
        throw std::runtime_error("Malformed native approval record.");
    return record;
}

template<class Action>
void each_record(const CreationRoot& root,const Action& action) {
    std::size_t count=0;
    for(const auto& entry:fs::directory_iterator(root.path)) {
        const auto id=path_utf8(entry.path().filename());
        if(id==".void")continue;
        if(++count>approval_limit || !valid_id(id))throw std::runtime_error("Invalid native approval store inventory.");
        action(id);
    }
}

bool interactive() {
#if defined(_WIN32)
    DWORD mode;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode) &&
           GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE),&mode);
#else
    return isatty(STDIN_FILENO) && isatty(STDERR_FILENO) && tcgetpgrp(STDIN_FILENO)==getpgrp();
#endif
}

// Explicit, bounded stdio protocol for an editor that presents the compiler's
// verified identity. Ordinary redirected invocations never enter this path.
bool confirm_editor(std::string_view label,std::string_view id,const std::string& session) {
    std::string message;
    llvm::raw_string_ostream out(message);
    llvm::json::OStream json(out);
    json.object([&] {json.attribute("schema",1);json.attribute("session",session);json.attribute("id",std::string(id));json.attribute("provider",std::string(label));});
    std::cerr<<"\x1eVOID_NATIVE_APPROVAL "<<message<<'\n'<<std::flush;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    std::string response;
    while(std::chrono::steady_clock::now()<deadline && response.size()<80) {
        char byte=0;
#if defined(_WIN32)
        const auto input=GetStdHandle(STD_INPUT_HANDLE);
        DWORD available=0,read=0;
        if(!PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr))return false;
        if(!available){std::this_thread::sleep_for(std::chrono::milliseconds(50));continue;}
        if(!ReadFile(input,&byte,1,&read,nullptr) || read!=1)return false;
#else
        pollfd input{STDIN_FILENO,POLLIN,0};
        const auto ready=poll(&input,1,100);
        if(ready<0){if(errno==EINTR)continue;return false;}
        if(!ready)continue;
        if(::read(STDIN_FILENO,&byte,1)!=1)return false;
#endif
        if(byte=='\n')return response=="approve "+std::string(id);
        response+=byte;
    }
    return false;
}

bool confirm(std::string_view label,std::string_view id) {
    std::cerr<<"Verified native provider for this application:\n  "<<label
             <<"\n  Approval ID (SHA-256): "<<id
             <<"\nNative code can act with your user privileges. Type approve to authorize this exact identity: "<<std::flush;
    for(char c:std::string_view("approve"))if(std::cin.get()!=c)return false;
    const auto end=std::cin.get();
    return end=='\n' || (end=='\r' && std::cin.get()=='\n');
}
}

struct NativeApproval::Payload {
#if defined(_WIN32)
    [[noreturn]] static void io_error(const std::string& operation,DWORD error) {
        throw std::system_error(static_cast<int>(error),std::system_category(),
            operation+" (Windows error "+std::to_string(error)+")");
    }
    static void write_bytes(HANDLE file,std::string_view bytes,const std::string& operation) {
        while(!bytes.empty()) {
            const auto count=static_cast<DWORD>(std::min<std::size_t>(bytes.size(),65536));
            DWORD written=0;
            if(!WriteFile(file,bytes.data(),count,&written,nullptr))io_error(operation,GetLastError());
            if(!written)io_error(operation,ERROR_WRITE_FAULT);
            bytes.remove_prefix(written);
        }
    }
    static fs::path storage(const fs::path& project) {
        auto pins=native_windows::pin_directories(project);
        native_windows::PrivateAccess access;
        auto root=project;
        for(const auto* name:{L".void",L"run"}) {
            root/=name;
            if(!CreateDirectoryW(root.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
                native_windows::fail("Cannot create project artifact directory");
            auto child=native_windows::pin_directories(root);
        }
        unsigned scanned=0;
        for(const auto& entry:fs::directory_iterator(root)) {
            if(++scanned>128)break;
            if(!path_utf8(entry.path().filename()).starts_with("native-"))continue;
            try {
                auto child=native_windows::pin_directories(entry.path());
                access.verify(child.back().value);
                std::string record;
                {
                    auto owner=native_windows::read_file(entry.path()/L".owner",true);
                    const auto size=native_windows::size(owner.value);
                    if(!size || size>16384)continue;
                    record.resize(static_cast<std::size_t>(size));
                    if(native_windows::read(owner.value,record.data(),static_cast<DWORD>(size))!=size)continue;
                }
                constexpr std::string_view header="VOID native run 1\n";
                std::string_view rest(record);
                if(!rest.starts_with(header))continue;
                rest.remove_prefix(header.size());
                std::vector<fs::path> files;
                while(!rest.empty()) {
                    auto end=rest.find('\n');
                    if(end==rest.npos || files.size()>=64)throw std::runtime_error("Invalid native run ownership record");
                    auto name=rest.substr(0,end);
                    if(name.empty() || name=="." || name==".." || name==".owner" || name.size()>255 ||
                       !std::ranges::all_of(name,[](unsigned char c){return (c>='a' && c<='z') ||
                           (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-';}))
                        throw std::runtime_error("Invalid native run filename");
                    files.push_back(utf8_path(name));rest.remove_prefix(end+1);
                }
                for(const auto& name:files) {
                    const auto file=entry.path()/name;
                    if(!fs::exists(fs::symlink_status(file)))continue;
                    {auto held=native_windows::read_file(file,true);}
                    fs::remove(file);
                }
                bool unknown=false;
                for(const auto& file:fs::directory_iterator(entry.path()))
                    if(file.path().filename()!=L".owner")unknown=true;
                if(!unknown) {
                    fs::remove(entry.path()/L".owner");child.clear();fs::remove(entry.path());
                }
            } catch(const std::exception&) { /* live lease, alias or unknown files: leave in place */ }
        }
        return root;
    }
    static fs::path candidate(){return native_windows::unique_leaf("native-%%%%%%%%%%%%%%%%");}
    native_windows::Directory directory;
    // Close the lease before Directory cleanup, including constructor failures.
    native_windows::Handle lease;
    Payload(const fs::path& root,const std::map<std::string,std::string>& files)
        :directory(storage(root),false,candidate) {
        // No sharing keeps a live run distinguishable from an abandoned run.
        // The Directory pins all ancestors and uses an owner-only protected DACL.
        lease.value=CreateFileW((directory.directory/L".owner").c_str(),
            GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(lease.value==INVALID_HANDLE_VALUE)native_windows::fail("Cannot lease native run storage");
        std::string record="VOID native run 1\n";
        for(const auto& [name,bytes]:files)record+=name+'\n';
        const auto operation="Cannot record native run ownership: "+display(path_utf8(directory.directory/L".owner"));
        write_bytes(lease.value,record,operation);
        if(!FlushFileBuffers(lease.value))io_error(operation,GetLastError());
    }
    void write_file(const std::string& name,std::string_view bytes) {
        const auto path=directory.directory/utf8_path(name);
        const auto operation="Cannot stage approved native artifact: "+display(path_utf8(path));
        native_windows::Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(file.value==INVALID_HANDLE_VALUE)io_error(operation,GetLastError());
        write_bytes(file.value,bytes,operation);
        if(!FlushFileBuffers(file.value))io_error(operation,GetLastError());
        const auto handle=std::exchange(file.value,INVALID_HANDLE_VALUE);
        if(!CloseHandle(handle))io_error(operation,GetLastError());
    }
    ~Payload() {
        // Release the no-sharing record before Directory's owned cleanup.
        if(lease.value!=INVALID_HANDLE_VALUE) {CloseHandle(lease.value);lease.value=INVALID_HANDLE_VALUE;}
    }
    const fs::path& path() const { return directory.directory; }
#else
    fs::path directory;
    int parent=-1,stage=-1,lease=-1;
    bool created=false,identified=false,recorded=false;
    struct stat created_identity{};
    std::string leaf;
    static constexpr std::string_view marker="VOID native run 1\n";
    static bool private_entry(int fd,bool folder) {
        struct stat info{};
        return fd>=0 && !fstat(fd,&info) && info.st_uid==geteuid() && !(info.st_mode&0077) &&
            (folder?S_ISDIR(info.st_mode):(S_ISREG(info.st_mode) && info.st_nlink==1));
    }
    static int child(int fd,const char* name) {
        if(mkdirat(fd,name,0777) && errno!=EEXIST)
            throw std::system_error(errno,std::generic_category(),"Cannot create project artifact directory");
        const auto result=openat(fd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(result<0)throw std::system_error(errno,std::generic_category(),
            "Cannot open project artifact directory: "+std::string(name));
        struct stat info{};
        if(fstat(result,&info)) {
            const auto error=errno;close(result);
            throw std::system_error(error,std::generic_category(),
                "Cannot inspect project artifact directory: "+std::string(name));
        }
        if(!S_ISDIR(info.st_mode)) {
            close(result);
            throw std::runtime_error("Project .void/run storage must be an accessible real directory.");
        }
        return result;
    }
    static bool safe_name(std::string_view name) {
        return !name.empty() && name!="." && name!=".." && name!=".owner" && name.size()<=255 &&
            std::ranges::all_of(name,[](unsigned char c){return (c>='a' && c<='z') ||
                (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-';});
    }
    // Only an exact ownership record grants deletion rights to listed files.
    // Unknown files, links, directories and active leases are never removed.
    static void clean(int parent,const char* name,int fd,int owner) noexcept {
        if(!private_entry(fd,true) || !private_entry(owner,false) || flock(owner,LOCK_EX|LOCK_NB))return;
        struct stat held{},named{};
        if(fstat(fd,&held) || fstatat(parent,name,&named,AT_SYMLINK_NOFOLLOW) ||
           held.st_dev!=named.st_dev || held.st_ino!=named.st_ino)return;
        char data[16385];const auto size=pread(owner,data,sizeof(data),0);
        if(size<=0 || size>16384)return;
        std::string_view text(data,static_cast<std::size_t>(size));
        if(!text.starts_with(marker))return;
        text.remove_prefix(marker.size());
        // Validate the complete record before removing anything.
        auto remaining=text;unsigned count=0;
        while(!remaining.empty()) {
            auto end=remaining.find('\n');
            if(end==remaining.npos || !safe_name(remaining.substr(0,end)) || ++count>64)return;
            remaining.remove_prefix(end+1);
        }
        while(!text.empty()) {
            auto end=text.find('\n');std::string file(text.substr(0,end));text.remove_prefix(end+1);
            struct stat info{};
            if(!fstatat(fd,file.c_str(),&info,AT_SYMLINK_NOFOLLOW) && S_ISREG(info.st_mode) &&
               info.st_uid==geteuid() && info.st_nlink==1)unlinkat(fd,file.c_str(),0);
        }
        // Leave the marker when user files prevent removal of the directory.
        auto* entries=fdopendir(openat(fd,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        if(!entries)return;
        bool unknown=false;
        while(auto* entry=readdir(entries)) {
            std::string_view n(entry->d_name);
            if(n!="." && n!=".." && n!=".owner"){unknown=true;break;}
        }
        closedir(entries);
        if(!unknown) {unlinkat(fd,".owner",0);unlinkat(parent,name,AT_REMOVEDIR);}
    }
    void release() noexcept {
        if(stage>=0 && lease>=0 && recorded)clean(parent,leaf.c_str(),stage,lease);
        else if(created && identified) {
            // Constructor failure before a complete marker: remove only the
            // record we exclusively created and the verified empty leaf. The
            // directory open itself may have failed (e.g. EMFILE), so ownership
            // must not depend on stage being a valid descriptor.
            struct stat named{};
            if(!fstatat(parent,leaf.c_str(),&named,AT_SYMLINK_NOFOLLOW) &&
               S_ISDIR(named.st_mode) && named.st_uid==geteuid() && !(named.st_mode&0077) &&
               named.st_dev==created_identity.st_dev && named.st_ino==created_identity.st_ino) {
                struct stat held{},record{},record_name{};
                if(stage>=0 && lease>=0 && !fstat(stage,&held) &&
                   held.st_dev==created_identity.st_dev && held.st_ino==created_identity.st_ino &&
                   !fstat(lease,&record) && !fstatat(stage,".owner",&record_name,AT_SYMLINK_NOFOLLOW) &&
                   record.st_dev==record_name.st_dev && record.st_ino==record_name.st_ino)
                    unlinkat(stage,".owner",0);
                // AT_REMOVEDIR refuses nonempty directories; unknown user files
                // and a substituted directory never become cleanup targets.
                unlinkat(parent,leaf.c_str(),AT_REMOVEDIR);
            }
        }
        if(lease>=0)close(lease);
        if(stage>=0)close(stage);
        if(parent>=0)close(parent);
        lease=stage=parent=-1;
    }
    Payload(const fs::path& root,const std::map<std::string,std::string>& files) {
        try {
            const auto project=open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            struct stat info{};
            if(project<0)throw std::system_error(errno,std::generic_category(),
                "Cannot open project artifact root: "+display(path_utf8(root)));
            if(fstat(project,&info)) {
                const auto error=errno;close(project);
                throw std::system_error(error,std::generic_category(),"Cannot inspect project artifact root");
            }
            int dot=-1;
            try {dot=child(project,".void");parent=child(dot,"run");}
            catch(...) {if(dot>=0)close(dot);close(project);throw;}
            close(dot);close(project);
            // Bounded recovery after SIGKILL/crash. flock distinguishes live runs
            // without PID reuse assumptions or signal handlers doing unsafe IO.
            auto* entries=fdopendir(openat(parent,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
            if(entries) {
                unsigned scanned=0;
                while(auto* entry=readdir(entries)) {
                    if(++scanned>128)break;
                    if(!std::string_view(entry->d_name).starts_with("native-"))continue;
                    const auto old=openat(parent,entry->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
                    if(old<0)continue;
                    const auto owner=openat(old,".owner",O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
                    if(owner>=0){clean(parent,entry->d_name,old,owner);close(owner);}
                    close(old);
                }
                closedir(entries);
            }
            for(unsigned attempt=0;attempt<32;++attempt) {
                llvm::SmallString<64> name;
                llvm::sys::fs::createUniquePath("native-%%%%%%%%%%%%%%%%",name,true);
                leaf=path_utf8(utf8_path(name.str().str()).filename());
                if(!mkdirat(parent,leaf.c_str(),0700)) {
                    created=true;
                    if(fstatat(parent,leaf.c_str(),&created_identity,AT_SYMLINK_NOFOLLOW))
                        throw std::system_error(errno,std::generic_category(),"Cannot identify newly created native run storage");
                    if(!S_ISDIR(created_identity.st_mode) || created_identity.st_uid!=geteuid() ||
                       (created_identity.st_mode&0077))throw std::runtime_error("New native run storage is not private and owned.");
                    identified=true;
                    break;
                }
                if(errno!=EEXIST || attempt==31)throw std::system_error(errno,std::generic_category(),"Cannot reserve native run storage");
            }
            stage=openat(parent,leaf.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            if(stage<0)throw std::system_error(errno,std::generic_category(),
                "Cannot open newly created native run storage: "+display(path_utf8(root/".void/run"/leaf)));
            if(!host::clear_inherited_acl(stage) || !host::safe_acl(stage))
                throw std::runtime_error("Cannot secure native run storage ACL.");
            if(fstat(stage,&info))throw std::system_error(errno,std::generic_category(),"Cannot inspect native run storage");
            if(info.st_uid!=geteuid() || (info.st_mode&0077) || info.st_dev!=created_identity.st_dev ||
               info.st_ino!=created_identity.st_ino)throw std::runtime_error("Native run storage identity or privacy changed.");
            lease=openat(stage,".owner",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
            if(lease<0 || flock(lease,LOCK_EX|LOCK_NB))throw std::system_error(errno,std::generic_category(),"Cannot lease native run storage");
            std::string record(marker);
            for(const auto& [name,bytes]:files) {
                if(!safe_name(name))throw std::runtime_error("Unsafe native payload filename.");
                record+=name+'\n';
            }
            if(record.size()>16384 || files.size()>64)throw std::runtime_error("Native run inventory exceeds its bound.");
            std::string_view remaining(record);
            while(!remaining.empty()) {
                const auto written=write(lease,remaining.data(),remaining.size());
                if(written<0 && errno==EINTR)continue;
                if(written<=0)throw std::system_error(written<0?errno:EIO,std::generic_category(),"Cannot record native run ownership");
                remaining.remove_prefix(static_cast<std::size_t>(written));
            }
            if(fsync(lease))throw std::system_error(errno,std::generic_category(),"Cannot flush native run ownership");
            recorded=true;
            directory=root/".void/run"/leaf;
        } catch(...) {release();throw;}
    }
    void write_file(const std::string& name,std::string_view bytes) {
        const auto fd=openat(stage,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(fd<0)throw std::system_error(errno,std::generic_category(),"Cannot stage approved native artifact: "+name);
        int error=0;
        while(!bytes.empty()) {
            auto n=write(fd,bytes.data(),bytes.size());
            if(n<0 && errno==EINTR)continue;
            if(n<=0){error=errno?errno:EIO;break;}
            bytes.remove_prefix(static_cast<std::size_t>(n));
        }
        if(close(fd) && !error)error=errno;
        if(error)throw std::system_error(error,std::generic_category(),"Cannot stage approved native artifact: "+name);
    }
    ~Payload() {release();}
    const fs::path& path() const {return directory;}
#endif
};

NativeApproval::NativeApproval(Mode mode,std::string editor_session):mode_(mode),editor_session_(std::move(editor_session)) {}
NativeApproval::~NativeApproval() = default;

fs::path project_build_directory(const fs::path& root) {
#ifdef _WIN32
    auto pins=native_windows::pin_directories(root);
    native_windows::PrivateAccess access;
    auto directory=root;
    for(const auto* name:{L".void",L"build"}) {
        directory/=name;
        if(!CreateDirectoryW(directory.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
            native_windows::fail("Cannot create project build directory");
        auto child=native_windows::pin_directories(directory);
    }
#else
    int parent=open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    struct stat info{};
    if(parent<0)throw std::system_error(errno,std::generic_category(),"Cannot open project build root: "+display(path_utf8(root)));
    if(fstat(parent,&info)) {
        const auto error=errno;close(parent);
        throw std::system_error(error,std::generic_category(),"Cannot inspect project build root");
    }
    try {
        for(const auto* name:{".void","build"}) {
            if(mkdirat(parent,name,0777) && errno!=EEXIST)
                throw std::system_error(errno,std::generic_category(),"Cannot create project build storage");
            const auto child=openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            if(child<0)throw std::system_error(errno,std::generic_category(),"Cannot open project build storage: "+std::string(name));
            if(fstat(child,&info)) {
                const auto error=errno;close(child);
                throw std::system_error(error,std::generic_category(),"Cannot inspect project build storage: "+std::string(name));
            }
            if(!S_ISDIR(info.st_mode)) {
                close(child);
                throw std::runtime_error("Project .void/build storage must be an accessible real directory.");
            }
            close(parent);parent=child;
        }
        close(parent);
    } catch(...) {close(parent);throw;}
#endif
    return root/".void/build";
}

fs::path NativeApproval::authorize(const ManagedNativeApprovalSubject& subject,bool jit) {
    if(llvm::sys::Process::GetEnv("CI"))
        throw NativeApprovalRequired("CI requires explicit --allow-native for this invocation.");
    if(mode_==Mode::terminal && !interactive())
        throw NativeApprovalRequired(std::string("This project declares native code. Pass --allow-native to explicitly authorize this invocation.\n")+terminal_review);
    const auto store=store_path();
    const auto& dependencies=subject.dependencies;
    const auto& native=*dependencies.native();
    require_outside_native_approval_store(subject.root,true);
    require_outside_native_approval_store(native.dynamic_root(),true);

    Digest digest;
    digest.field("void-managed-native-approval-v1");
    digest.field(path_utf8(subject.root));
    digest.field(dependencies.root_identity());
    digest.field(subject.manifest.project().id);
    digest.field(subject.manifest.application()->id);
    digest.field(subject.entry);
    digest.field(dependencies.identity());
    digest.field(dependencies.native_catalog_identity());
    digest.field(native.dynamic_contract()); // Descriptor bytes, runtime hashes, target, transport, canonical path.
    digest.field(std::to_string(native.abi_revision()));
    digest.field(subject.binding);
    digest.field(subject.binding_source);
    digest.field(compiler_build_identity());
    const auto id=digest.finish();
    const auto& requirement=*subject.manifest.native_requirement();
    const auto label=display(requirement.id)+" "+requirement.version.text()+" | "+native.target()+
        " ABI "+std::to_string(native.abi_revision())+" | "+display(path_utf8(subject.root))+
        " | "+display(subject.manifest.application()->id)+" | "+display(subject.entry);
    const auto expected=std::string(record_header)+id+'\n'+label+'\n';
    if(expected.size()>record_limit)throw NativeApprovalRequired("Native approval identity is too long. Pass --allow-native for this invocation.");
    bool found=false;
    try {
        with_approval_store(store,false,[&](const CreationRoot& root){
            if(auto record=read_record(root,id)) {
                if(*record!=expected)throw std::runtime_error("Malformed native approval record.");
                found=true;
            }
        });
    } catch(const std::runtime_error& error) {
        if(mode_==Mode::reuse_only)
            throw std::runtime_error(std::string(error.what())+"\nResolve the approval store error first. "+terminal_review);
        throw;
    }
    if(!found) {
        if(mode_==Mode::reuse_only)
            throw NativeApprovalRequired(std::string("No saved native approval matches this project and provider.\n")+terminal_review);
        if(!(mode_==Mode::editor?confirm_editor(label,id,editor_session_):confirm(label,id)))throw NativeApprovalRequired("Native approval declined. No approval was saved.");
        pending_=expected;
    }
    id_=id;
    if(!jit || subject.binding.empty())return {};
    // The project can change during a prompt and during JIT compilation. Load
    // the exact payload from private files, never from the writable project
    // directory after comparing its hashes. The Runtime still verifies it.
    auto files=read_managed_payload(native);
    payload_=std::make_unique<Payload>(subject.root,files);
    for(const auto& [name,bytes]:files) {
        payload_->write_file(name,bytes);
    }
    return payload_->path();
}

void NativeApproval::finish_editor_review() {
    if(mode_!=Mode::editor)return;
    // This seal always precedes application/native startup, including projects
    // with no native requirements and exact saved approvals. The editor stops
    // interpreting control messages before application stderr can be emitted.
    std::cerr<<"\x1eVOID_NATIVE_APPROVAL_END "<<editor_session_<<'\n'<<std::flush;
}

void NativeApproval::remember() {
    if(mode_==Mode::reuse_only || pending_.empty())return;
    with_approval_store(store_path(),true,[&](const CreationRoot& root){
        if(const auto old=read_record(root,id_)) {
            if(*old!=pending_)throw std::runtime_error("Malformed native approval record.");
            return;
        }
        std::size_t count=0;
        each_record(root,[&](const auto&){++count;});
        if(count>=approval_limit)throw std::runtime_error("Native approval record limit reached; revoke an earlier approval.");
        publish_project_directory(root,id_,{{"approval",pending_}},[&](const fs::path& stage,const ProjectDirectoryReader& reader){
            if(reader.read(stage/"approval",record_limit)!=pending_)
                throw std::runtime_error("Native approval record changed before publication.");
        },{},{},true);
    });
    std::cerr<<"Saved native approval "<<id_<<". Revoke with: voidc native revoke "<<id_<<'\n';
    pending_.clear();
}

std::vector<std::string> native_approvals() {
    std::vector<std::string> rows;
    with_approval_store(store_path(),false,[&](const CreationRoot& root){
        each_record(root,[&](const std::string& id) {
            const auto record=read_record(root,id);
            if(!record)throw std::runtime_error("Native approval disappeared during inspection.");
            rows.push_back(id+" | "+record->substr(record_header.size()+id.size()+1,record->size()-record_header.size()-id.size()-2));
        });
    });
    std::sort(rows.begin(),rows.end());
    return rows;
}

void revoke_native_approval(std::string_view id) {
    if(!valid_id(id))throw NativeApprovalRequired("Expected an exact approval ID from 'voidc native approvals'.");
    bool removed=false;
    with_approval_store(store_path(),false,[&](const CreationRoot& root){
        if(!read_record(root,id))return;
        remove_user_registration(root,std::string(id)); // Serialized, atomic removal from the store.
        removed=true;
    });
    if(!removed)throw std::runtime_error("No native approval with that ID.");
}
}
