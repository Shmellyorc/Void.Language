#include "void/project_creation.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/utf8.hpp"
#include "project_files.hpp"
#include <llvm/Support/RandomNumberGenerator.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <map>
#include <set>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../native_windows.hpp"
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif
#endif

namespace vd::project {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t entry_limit = 16384;
constexpr std::string_view manifest_name = "void.project.toml";
std::string fold(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
std::string random_hex(CreationEntropy entropy = nullptr) {
    std::array<unsigned char,16> bytes{};
    auto error = (entropy ? entropy : llvm::getRandomBytes)(bytes.data(),bytes.size());
    if (error) throw std::system_error(error,"Cannot obtain project creation entropy");
    constexpr char digits[]="0123456789abcdef";
    std::string result;
    for (auto byte : bytes) {result += digits[byte >> 4];result += digits[byte & 15];}
    return result;
}
std::string toml_string(std::string_view text) {
    std::string result = "\"";
    constexpr char digits[]="0123456789abcdef";
    for (unsigned char c : text) {
        if (c < 32 || c == 127) {
            result += "\\u00";result += digits[c >> 4];result += digits[c & 15];
        } else {
            if (c == '"' || c == '\\') result += '\\';
            result += static_cast<char>(c);
        }
    }
    return result + '"';
}
bool named(const fs::path& directory, const std::string& leaf, bool require_exact = false) {
    std::size_t seen=0, matches=0;
    bool exact=false;
#ifdef _WIN32
    const auto expected=utf8_path(leaf);
#else
    const auto expected=fold(leaf);
#endif
    for (const auto& child : fs::directory_iterator(directory)) {
        if (++seen > entry_limit) throw std::runtime_error("Project creation directory-entry limit exceeded.");
        auto name=path_utf8(child.path().filename());
#ifdef _WIN32
        if (native_windows::same_filename(child.path().filename(),expected)) {++matches;exact|=name==leaf;}
#else
        if (fold(name)==expected) {++matches;exact|=name==leaf;}
#endif
    }
    if (require_exact && (matches!=1 || !exact))
        throw std::runtime_error("Project creation path has wrong case or a case collision: "+path_utf8(directory/utf8_path(leaf)));
    return matches!=0;
}
fs::path absolute_target(const fs::path& path) {
#ifdef _WIN32
    if (!path.is_absolute() && (path.has_root_name() || path.has_root_directory()))
        throw std::invalid_argument("Project creation paths must be absolute or relative to the current directory.");
#endif
    for (const auto& part : path.relative_path()) if (part=="..")
        throw std::invalid_argument("Project creation paths cannot contain parent references.");
    auto normalized=fs::absolute(path).lexically_normal();
    if (normalized.has_relative_path() && normalized.filename().empty()) normalized=normalized.parent_path();
    return normalized;
}
void require_destination(const fs::path& root, bool fresh, bool nested) {
    if (fresh && named(root.parent_path(),path_utf8(root.filename())))
        throw CreationCollision("Project destination already exists, including a possible case sibling: "+path_utf8(root));
    if (!fresh && named(root,std::string(manifest_name)))
        throw CreationCollision("Project manifest already exists, including a possible case sibling: "+path_utf8(root/manifest_name));
    if (!nested) {
        auto ancestor=root.parent_path();
        while (!ancestor.empty()) {
            if (named(ancestor,std::string(manifest_name)))
                throw std::runtime_error("An enclosing project exists at "+path_utf8(ancestor)+". Use --nested for a deliberate child project.");
            auto parent=ancestor.parent_path();
            if (parent==ancestor) break;
            ancestor=std::move(parent);
        }
    }
}
std::string starter_parent(const std::string& starter) {
    if (starter.empty() || starter.front()=='/' || starter.find('\\')!=std::string::npos)
        throw std::invalid_argument("Use a portable project-relative .void starter path.");
    require_module_identity("/"+starter);
    if(std::count(starter.begin(),starter.end(),'/')>64)
        throw std::invalid_argument("Starter path exceeds the project directory-depth limit.");
    auto parent=utf8_path(starter).parent_path();
    Sources defaults;
    bool first=true;
    for (const auto& part : utf8_path(starter)) {
        auto raw=path_utf8(part), name=fold(raw);
        if (name==".git" || name==".void" || name=="build" || name=="libs" || name==manifest_name)
            throw std::invalid_argument("Starter path uses a reserved discovery component.");
        if (first) for (const auto& item:defaults.exclude) if (name==item)
            throw std::invalid_argument("Starter path uses a default excluded discovery component.");
        first=false;
    }
    return parent.empty()?".":path_utf8(parent);
}

#ifdef _WIN32
using NativeHandle = native_windows::Handle;
using NativeInfo = BY_HANDLE_FILE_INFORMATION;
NativeInfo identity(const NativeHandle& handle) {return native_windows::information(handle.value);}
std::string key(const NativeInfo& info) {
    return std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(info.nFileIndexHigh)+":"+std::to_string(info.nFileIndexLow);
}
struct Node {
    fs::path path;
    NativeHandle handle;
    NativeInfo original;
    bool directory;
    Node(fs::path p, NativeHandle h, bool d)
        :path(std::move(p)),handle(std::move(h)),original(identity(handle)),directory(d) {}
};
NativeHandle open_node(const fs::path& path, bool directory, bool pin) {
    NativeHandle handle(CreateFileW(path.c_str(),DELETE|FILE_READ_ATTRIBUTES|(directory?FILE_LIST_DIRECTORY|READ_CONTROL:0),
        FILE_SHARE_READ|FILE_SHARE_WRITE|(pin?0:FILE_SHARE_DELETE),nullptr,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|(directory?FILE_FLAG_BACKUP_SEMANTICS:0),nullptr));
    if(handle.value==INVALID_HANDLE_VALUE)native_windows::fail("Cannot pin project staging object");
    if(bool(identity(handle).dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory)
        throw std::runtime_error("Unexpected project staging object type.");
    return handle;
}
Node make_directory(const fs::path& path, bool private_stage) {
    std::optional<native_windows::PrivateAccess> access;
    if(private_stage)access.emplace();
    if(!CreateDirectoryW(path.c_str(),access?&access->attributes:nullptr))
        native_windows::fail("Cannot create project staging directory");
    try {
        auto handle=open_node(path,true,private_stage);
        if(access)access->verify(handle.value);
        return {path,std::move(handle),true};
    } catch(...) {throw std::runtime_error("Cannot prove staging ownership; retained "+path_utf8(path));}
}
Node make_file(const fs::path& path) {
    NativeHandle handle(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE|FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(handle.value==INVALID_HANDLE_VALUE)native_windows::fail("Cannot create staged project file");
    return {path,std::move(handle),false};
}
void write_file(Node& node,std::string_view text) {
    DWORD written=0;
    if(!WriteFile(node.handle.value,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) || written!=text.size() || !FlushFileBuffers(node.handle.value))
        native_windows::fail("Cannot finish staged project file");
}
bool matches_file(const Node& node, std::string_view expected) {
    LARGE_INTEGER start{};
    if(!SetFilePointerEx(node.handle.value,start,nullptr,FILE_BEGIN))return false;
    std::string observed(expected.size()+1,'\0');
    DWORD read=0;
    return ReadFile(node.handle.value,observed.data(),static_cast<DWORD>(observed.size()),&read,nullptr) &&
        read==expected.size() && std::string_view(observed.data(),read)==expected;
}
bool owns(const Node& node) {
    NativeHandle observed(CreateFileW(node.path.c_str(),FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(observed.value==INVALID_HANDLE_VALUE)return false;
    auto observed_info=identity(observed);
    return key(observed_info)==key(node.original) && (node.directory || observed_info.nNumberOfLinks==1);
}
struct Parent {
    fs::path path;
    std::vector<NativeHandle> pins;
    HANDLE leaf_handle;
    explicit Parent(fs::path requested, bool exact_spelling=true,
                    const native_windows::DirectoryReservation* reservation=nullptr)
        :path(absolute_target(requested)) {
        pins=native_windows::pin_directories(path,reservation,&leaf_handle);
        if(exact_spelling) {
            auto current=path.root_path();
            for (const auto& part:path.relative_path()) {named(current,path_utf8(part),true);current/=part;}
        }
        path=fs::canonical(path);
    }
    std::string identity_key() const {return key(native_windows::information(leaf_handle));}
};
void erase(Node& node) {
    if(node.handle.value==INVALID_HANDLE_VALUE) {
        auto reopened=open_node(node.path,node.directory,false);
        auto current=identity(reopened);
        if(key(current)!=key(node.original) || (!node.directory && current.nNumberOfLinks!=1))
            throw std::runtime_error("Staging object changed before rollback: "+path_utf8(node.path));
        node.handle.value=std::exchange(reopened.value,INVALID_HANDLE_VALUE);
    }
    FILE_DISPOSITION_INFO disposition{TRUE};
    if(!owns(node) || !SetFileInformationByHandle(node.handle.value,FileDispositionInfo,&disposition,sizeof disposition))
        throw std::runtime_error("Staging object could not be safely removed: "+path_utf8(node.path));
    CloseHandle(std::exchange(node.handle.value,INVALID_HANDLE_VALUE));
}
void publish_file(const Node& node,const fs::path& destination,bool replace=false) {
    auto text=destination.native();
    std::vector<std::byte> bytes(sizeof(FILE_RENAME_INFO)+text.size()*sizeof(wchar_t));
    auto* rename=reinterpret_cast<FILE_RENAME_INFO*>(bytes.data());
    rename->ReplaceIfExists=replace;rename->RootDirectory=nullptr;
    rename->FileNameLength=static_cast<DWORD>(text.size()*sizeof(wchar_t));
    std::copy(text.begin(),text.end(),rename->FileName);
    if(SetFileInformationByHandle(node.handle.value,FileRenameInfo,rename,static_cast<DWORD>(bytes.size())))return;
    auto error=GetLastError();
    if(error==ERROR_ALREADY_EXISTS || error==ERROR_FILE_EXISTS)
        throw CreationCollision("Exclusive project publication collided with an existing destination.");
    native_windows::fail("Cannot publish project object",error);
}
#else
struct NativeHandle {
    int fd=-1;
    explicit NativeHandle(int value=-1):fd(value) {}
    NativeHandle(const NativeHandle&)=delete;
    NativeHandle& operator=(const NativeHandle&)=delete;
    NativeHandle(NativeHandle&& other) noexcept:fd(std::exchange(other.fd,-1)) {}
    NativeHandle& operator=(NativeHandle&& other) noexcept {
        if(this!=&other){if(fd>=0)::close(fd);fd=std::exchange(other.fd,-1);}
        return *this;
    }
    ~NativeHandle(){if(fd>=0)::close(fd);}
};
[[noreturn]] void system_failure(const char* message) {
    throw std::system_error(errno,std::generic_category(),message);
}
struct stat identity(const NativeHandle& handle) {
    struct stat result{};
    if(fstat(handle.fd,&result))system_failure("Cannot inspect project object");
    return result;
}
std::string key(const struct stat& info) {
    return std::to_string(info.st_dev)+":"+std::to_string(info.st_ino);
}
constexpr int dir_flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC;
struct Parent {
    fs::path path;
    NativeHandle handle;
    explicit Parent(fs::path requested,bool exact_spelling=true):path(absolute_target(requested)),handle(open(path.root_path().c_str(),dir_flags)) {
        if(handle.fd<0)system_failure("Cannot open project filesystem root");
        auto current=path.root_path();
        for (const auto& part:path.relative_path()) {
            if(exact_spelling)named(current,path_utf8(part),true);
            NativeHandle next(openat(handle.fd,part.c_str(),dir_flags));
            if(next.fd<0)system_failure("Cannot pin project directory");
            handle=std::move(next);current/=part;
        }
        if(key(identity(handle))!=Parent::path_key(path))throw std::runtime_error("Project directory changed during admission.");
    }
    static std::string path_key(const fs::path& path) {
        struct stat value{};
        if(lstat(path.c_str(),&value) || !S_ISDIR(value.st_mode))throw std::runtime_error("Cannot verify project directory path.");
        return key(value);
    }
    std::string identity_key() const {return key(identity(handle));}
};
struct Node {
    int parent;
    std::string name;
    NativeHandle handle;
    bool directory;
};
Node make_directory(int parent,std::string name) {
    if(mkdirat(parent,name.c_str(),0700))system_failure("Cannot create project staging directory");
    NativeHandle handle(openat(parent,name.c_str(),dir_flags));
    if(handle.fd<0)throw std::runtime_error("Cannot prove staging directory ownership; retained "+name);
    return {parent,std::move(name),std::move(handle),true};
}
Node make_file(int parent,std::string name) {
    NativeHandle handle(openat(parent,name.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    if(handle.fd<0)system_failure("Cannot create staged project file");
    return {parent,std::move(name),std::move(handle),false};
}
void write_file(Node& node,std::string_view text) {
    while(!text.empty()) {
        auto count=write(node.handle.fd,text.data(),text.size());
        if(count<0 && errno==EINTR)continue;
        if(count<=0)system_failure("Cannot write staged project file");
        text.remove_prefix(static_cast<std::size_t>(count));
    }
    if(fsync(node.handle.fd))system_failure("Cannot flush staged project file");
}
bool matches_file(const Node& node, std::string_view expected) {
    std::string observed(expected.size()+1,'\0');
    std::size_t total=0;
    while(total<observed.size()) {
        auto count=pread(node.handle.fd,observed.data()+total,observed.size()-total,static_cast<off_t>(total));
        if(count<0 && errno==EINTR)continue;
        if(count<0)return false;
        if(count==0)break;
        total+=static_cast<std::size_t>(count);
    }
    return total==expected.size() && std::string_view(observed.data(),total)==expected;
}
bool owns(const Node& node) {
    struct stat observed{};
    if(fstatat(node.parent,node.name.c_str(),&observed,AT_SYMLINK_NOFOLLOW))return false;
    return key(observed)==key(identity(node.handle)) && (node.directory || observed.st_nlink==1);
}
void erase(Node& node) {
    if(!owns(node) || unlinkat(node.parent,node.name.c_str(),node.directory?AT_REMOVEDIR:0))
        throw std::runtime_error("Staging object could not be safely removed: "+node.name);
}
void rename_exclusive(const Node& node,int parent,const std::string& destination) {
#if defined(__linux__)
    if(!syscall(SYS_renameat2,node.parent,node.name.c_str(),parent,destination.c_str(),RENAME_NOREPLACE))return;
#elif defined(__APPLE__)
    if(!renameatx_np(node.parent,node.name.c_str(),parent,destination.c_str(),RENAME_EXCL))return;
#else
    errno=ENOTSUP;
#endif
    if(errno==EEXIST || errno==ENOTEMPTY)
        throw CreationCollision("Exclusive project publication collided with an existing destination.");
    system_failure("Cannot publish project exclusively");
}
#endif
}

CreationRoot inspect_creation_root(const fs::path& path) {
    Parent admitted(path);
    return {admitted.path,admitted.identity_key()};
}
bool creation_manifest_present(const fs::path& directory) {
    return named(directory,std::string(manifest_name));
}

bool project_path_alias(const fs::path& path) {
    std::error_code error;
    const auto status=fs::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory)return false;
    if(error)throw fs::filesystem_error("Cannot inspect project path",path,error);
    if(fs::is_symlink(status))return true;
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return false;
#endif
}

namespace {
std::string read_project_input(const fs::path& path, std::size_t limit, const Parent& directory) {
    const auto fail=[&](const char* message) { return std::runtime_error(std::string(message)+" "+path_utf8(path)); };
    // The caller inventories exact spellings. Open beneath a pinned, no-alias
    // parent and check that the held file still names the same directory entry.
    if(directory.path!=path.parent_path())throw fail("Project input parent spelling changed.");
#ifdef _WIN32
    NativeHandle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(file.value==INVALID_HANDLE_VALUE)throw fail("Cannot open project input.");
    const auto info=identity(file);
    if(info.nNumberOfLinks!=1 || info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
        throw fail("Project input must be a regular file without aliases.");
    const auto size=(std::uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
#else
    const auto leaf=path.filename();
    NativeHandle file(openat(directory.handle.fd,leaf.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    if(file.fd<0)throw fail("Cannot open project input without following aliases.");
    const auto info=identity(file);
    if(!S_ISREG(info.st_mode) || info.st_nlink!=1 || info.st_size<0)
        throw fail("Project input must be a regular file without aliases.");
    const auto size=static_cast<std::uint64_t>(info.st_size);
#endif
    if(size>limit)throw fail("Project input byte limit exceeded.");
    std::string bytes(static_cast<std::size_t>(size)+1,'\0');
    std::size_t total=0;
    while(total<bytes.size()) {
#ifdef _WIN32
        DWORD count=0;
        if(!ReadFile(file.value,bytes.data()+total,static_cast<DWORD>(bytes.size()-total),&count,nullptr))
            throw fail("Cannot read project input.");
#else
        auto count=read(file.fd,bytes.data()+total,bytes.size()-total);
        if(count<0 && errno==EINTR)continue;
        if(count<0)throw fail("Cannot read project input.");
#endif
        if(count==0)break;
        total+=static_cast<std::size_t>(count);
    }
#ifdef _WIN32
    NativeHandle current(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(current.value==INVALID_HANDLE_VALUE)throw fail("Project input disappeared during reading.");
    const auto observed=identity(current);
    if(key(observed)!=key(info) || observed.nNumberOfLinks!=1 ||
       CompareFileTime(&observed.ftLastWriteTime,&info.ftLastWriteTime)!=0)
        throw fail("Project input identity changed during reading.");
#else
    struct stat current{};
    if(fstatat(directory.handle.fd,leaf.c_str(),&current,AT_SYMLINK_NOFOLLOW) ||
       key(current)!=key(info) || current.st_nlink!=1 ||
       Parent::path_key(directory.path)!=directory.identity_key())
        throw fail("Project input identity changed during reading.");
#endif
    if(total!=size)throw fail("Project input size changed during reading.");
    bytes.resize(total);
    return bytes;
}
}

std::string read_project_input(const fs::path& path,std::size_t limit) {
    return read_project_input(path,limit,Parent(path.parent_path(),false));
}

std::optional<std::string> read_dependency_lock(const fs::path& root) {
    const std::string name(lock_name);
    if(!named(root,name))return {};
    named(root,name,true);
    return read_project_input(root/lock_name,lock_byte_limit);
}

bool publish_dependency_lock(const CreationRoot& root,const std::optional<std::string>& previous,
                             std::string_view desired,const std::function<void()>& recheck) {
    Parent parent(root.path);
    if(parent.identity_key()!=root.identity)throw std::runtime_error("Project root changed before lock publication.");
    const auto work=parent.path/".void";
#ifdef _WIN32
    if(!CreateDirectoryW(work.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
        native_windows::fail("Cannot create project lock workspace");
#else
    if(flock(parent.handle.fd,LOCK_EX))system_failure("Cannot serialize project lock publication");
    if(mkdirat(parent.handle.fd,".void",0700) && errno!=EEXIST)system_failure("Cannot create project lock workspace");
#endif
    Parent workspace(work);
#ifdef _WIN32
    NativeHandle writer(CreateFileW((work/"dependency-writer").c_str(),GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(writer.value==INVALID_HANDLE_VALUE)native_windows::fail("Cannot open project lock writer");
    const auto info=identity(writer);
    if(info.nNumberOfLinks!=1 || info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Project lock writer must be a regular file without aliases.");
    OVERLAPPED offset{};
    if(!LockFileEx(writer.value,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&offset))
        native_windows::fail("Cannot serialize project lock publication");
#endif
    auto compare_destination=[&] {
        const auto existing=read_dependency_lock(parent.path);
        if(existing && *existing==desired)return false;
        if(existing!=previous)throw std::runtime_error("Dependency lock changed during resolution; retry.");
        return true;
    };
    if(!compare_destination()) {
        recheck();
        return false;
    }
    const auto temporary="resolve-"+random_hex();
    std::vector<Node> staged;
    staged.reserve(2);
    bool published=false;
    try {
#ifdef _WIN32
        staged.push_back(make_directory(work/temporary,true));
        staged.push_back(make_file(staged[0].path/lock_name));
#else
        staged.push_back(make_directory(workspace.handle.fd,temporary));
        staged.push_back(make_file(staged[0].handle.fd,std::string(lock_name)));
#endif
        auto& file=staged[1];
        write_file(file,desired);
        recheck();
        if(inspect_creation_root(parent.path).identity!=root.identity ||
           inspect_creation_root(work).identity!=workspace.identity_key() ||
           !owns(staged[0]) || !owns(file) || !matches_file(file,desired))
            throw std::runtime_error("Project lock staging changed before publication.");
        if(!compare_destination())throw std::runtime_error("Dependency lock changed during publication; retry.");
#ifdef _WIN32
        publish_file(file,parent.path/lock_name,previous.has_value());
#else
        if(previous) {
            if(renameat(file.parent,file.name.c_str(),parent.handle.fd,lock_name.data()))
                system_failure("Cannot replace project dependency lock");
        } else rename_exclusive(file,parent.handle.fd,std::string(lock_name));
#endif
        published=true;
        erase(staged[0]);
        return true;
    } catch(const std::exception& error) {
        if(published)throw std::runtime_error(std::string(error.what())+" Lock publication completed; inspect void.lock.");
        bool uncertain=false;
        for(auto it=staged.rbegin();it!=staged.rend();++it)try{erase(*it);}catch(const std::exception&){uncertain=true;}
        if(uncertain)throw std::runtime_error(std::string(error.what())+" Retained uncertain lock staging at "+path_utf8(work/temporary)+".");
        throw;
    }
}

void publish_git_snapshot(const CreationRoot& root, const std::string& key,
                          const std::map<std::string, std::string_view>& files,
                          const VerifyProjectDirectory& verify) {
    Parent parent(root.path);
    if (parent.identity_key() != root.identity) throw std::runtime_error("Project root changed before Git snapshot publication.");
    const auto work = root.path / ".void";
    const auto store = work / git_store_name;
    if (named(root.path, ".void")) named(root.path, ".void", true);
#ifdef _WIN32
    auto ensure_directory = [](const fs::path& path) {
        if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            native_windows::fail("Cannot create Git snapshot store");
    };
    ensure_directory(work);
    Parent workspace(work);
    if (named(work, std::string(git_store_name))) named(work, std::string(git_store_name), true);
    ensure_directory(store);
#else
    if (mkdirat(parent.handle.fd, ".void", 0700) && errno != EEXIST) system_failure("Cannot create Git snapshot workspace");
    Parent workspace(work);
    if (named(work, std::string(git_store_name))) named(work, std::string(git_store_name), true);
    if (mkdirat(workspace.handle.fd, git_store_name.data(), 0700) && errno != EEXIST)
        system_failure("Cannot create Git snapshot store");
#endif
    publish_project_directory(inspect_creation_root(store), key, files, verify, [&] {
        if (inspect_creation_root(root.path).identity != root.identity)
            throw std::runtime_error("Git snapshot store changed before publication.");
    });
}

namespace {
#ifdef _WIN32
ProjectDirectoryReader staged_reader(const native_windows::DirectoryReservation& reservation) {
    return {
        [&](const fs::path& path) {
            Parent admitted(path,true,&reservation);
            return CreationRoot{admitted.path,admitted.identity_key()};
        },
        [&](const fs::path& path, std::size_t limit) {
            return read_project_input(path,limit,Parent(path.parent_path(),false,&reservation));
        },
        &reservation,
    };
}
#endif
template<class Work>
void with_project_stage(const CreationRoot& directory, const ProjectDirectoryReader& parent_reader, Work work,
                        std::string_view private_child={}) {
    const auto& store = directory.path;
#ifdef _WIN32
    Parent destination(store,true,parent_reader.reservation);
#else
    Parent destination(store);
#endif
    if (destination.identity_key() != directory.identity)
        throw std::runtime_error("Staged directory parent changed before publication.");
    const auto temporary = private_child.empty() ? "acquire-" + random_hex() : std::string(private_child);
    const auto stage = store / utf8_path(temporary);
#ifdef _WIN32
    auto staged = make_directory(stage, true);
#else
    auto staged = make_directory(destination.handle.fd, temporary);
#endif
    bool keep = false;
    auto cleanup = [&] {
        if (!owns(staged) || parent_reader.inspect(store).identity != destination.identity_key())
            throw std::runtime_error("Staged directory ownership changed; retained " + path_utf8(stage) + ".");
#ifdef _WIN32
        CloseHandle(std::exchange(staged.handle.value, INVALID_HANDLE_VALUE));
#endif
        fs::remove_all(stage);
    };
    try {
        work(destination, staged, stage, keep);
    } catch (...) {
        if (!keep) cleanup();
        throw;
    }
    if (!keep) cleanup();
}
}

void publish_project_directory(const CreationRoot& directory, const std::string& leaf,
                               const std::map<std::string, std::string_view>& files,
                               const VerifyProjectDirectory& verify,
                               const std::function<void()>& recheck,
                               const ProjectDirectoryReader& parent_reader) {
    std::string_view private_child;
#ifdef _WIN32
    // NTFS also reserves the target parent during a rename. For a payload
    // inside our unpublished package, create its final child exclusively in
    // that stage. The outer rename publishes the executable and payload together.
    if (parent_reader.reservation) private_child = leaf;
#endif
    with_project_stage(directory, parent_reader, [&](const Parent& destination, Node& staged, const fs::path& stage, bool& keep) {
#ifdef _WIN32
        const native_windows::DirectoryReservation reservation{stage,staged.handle.value,parent_reader.reservation};
#endif
        for (const auto& [relative, bytes] : files) {
            const bool directory_entry = relative.ends_with('/');
            const auto path = stage / utf8_path(directory_entry ? relative.substr(0, relative.size() - 1) : relative);
#ifdef _WIN32
            Parent directory(path.parent_path(),true,&reservation);
#else
            Parent directory(path.parent_path());
#endif
            if (directory_entry) {
#ifdef _WIN32
                (void)make_directory(path, false);
#else
                (void)make_directory(directory.handle.fd, path_utf8(path.filename()));
#endif
                continue;
            }
#ifdef _WIN32
            auto file = make_file(path);
#else
            auto file = make_file(directory.handle.fd, path_utf8(path.filename()));
#endif
            write_file(file, bytes);
        }
#ifdef _WIN32
        verify(stage,staged_reader(reservation));
#else
        verify(stage,{});
#endif
        if (recheck) recheck();
        if (parent_reader.inspect(directory.path).identity != destination.identity_key() || !owns(staged))
            throw std::runtime_error("Staged directory parent changed before publication.");
        try {
#ifdef _WIN32
            if (private_child.empty()) publish_file(staged, directory.path / utf8_path(leaf));
#else
            rename_exclusive(staged, destination.handle.fd, leaf);
#endif
            keep = true;
        } catch (const CreationCollision&) {
            verify(directory.path / leaf,parent_reader);
        }
    }, private_child);
}

void publish_application_directory(const CreationRoot& directory, const std::string& leaf,
                                   const VerifyProjectDirectory& prepare,
                                   const std::function<void(const fs::path&)>& verify_replacement) {
#ifdef _WIN32
    const auto output = directory.path / utf8_path(leaf);
    auto require_absent = [&] {
        // The exact directory-entry comparison catches case siblings, and the
        // physical lookup also catches existing alternate (8.3) spellings.
        const auto attributes = GetFileAttributesW(output.c_str());
        const auto error = GetLastError();
        if (attributes != INVALID_FILE_ATTRIBUTES || named(directory.path, leaf)) {
            if (verify_replacement)
                throw CreationCollision("Windows application packaging cannot replace an existing destination: " + path_utf8(output));
            throw CreationCollision("Application package destination already exists: " + path_utf8(output));
        }
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
            native_windows::fail("Cannot inspect application package destination", error);
    };
    Parent destination(directory.path);
    if (destination.identity_key() != directory.identity)
        throw std::runtime_error("Application package destination parent changed.");
    require_absent();
    with_project_stage(directory, {}, [&](const Parent&, Node& staged, const fs::path& stage, bool& keep) {
        const native_windows::DirectoryReservation reservation{stage,staged.handle.value,nullptr};
        prepare(stage,staged_reader(reservation));
        if (inspect_creation_root(directory.path).identity != directory.identity || !owns(staged))
            throw std::runtime_error("Application package staging or destination parent changed.");
        require_absent();
        try {
            publish_file(staged,output);
        } catch (const std::runtime_error&) {
            require_absent();
            throw;
        }
        keep = true;
    });
#elif defined(__linux__)
    const auto output = directory.path / utf8_path(leaf);
    auto collision = [&] {
        return CreationCollision("Application package destination already exists: " + path_utf8(output));
    };
    if (!verify_replacement && named(directory.path, leaf)) throw collision();
    with_project_stage(directory, {}, [&](const Parent& destination, Node& staged, const fs::path& stage, bool& keep) {
        auto package = make_directory(staged.handle.fd, "package");
        prepare(stage / package.name,{});
        // Serialize checks and exchange with other package publishers in this parent.
        if (flock(destination.handle.fd, LOCK_EX | LOCK_NB)) system_failure("Cannot lock package destination parent");
        auto recheck = [&] {
            if (inspect_creation_root(directory.path).identity != directory.identity || !owns(staged) || !owns(package))
                throw std::runtime_error("Application package staging or destination parent changed.");
        };
        recheck();
        if (!named(directory.path, leaf)) {
            rename_exclusive(package, destination.handle.fd, leaf);
            return;
        }
        if (!verify_replacement) throw collision();
        named(directory.path, leaf, true);
        Node prior{destination.handle.fd, leaf, NativeHandle(openat(destination.handle.fd, leaf.c_str(), dir_flags)), true};
        if (prior.handle.fd < 0) system_failure("Cannot admit existing application package");
        verify_replacement(output);
        recheck();
        if (!owns(prior)) throw std::runtime_error("Existing application package changed before replacement.");
        if (syscall(SYS_renameat2, package.parent, package.name.c_str(), prior.parent, prior.name.c_str(), RENAME_EXCHANGE))
            system_failure("Cannot atomically replace application package");
        prior.parent = package.parent;
        prior.name = package.name;
        if (!owns(prior)) {
            keep = true;
            throw std::runtime_error("Package replacement displaced an unexpected directory. Retained " + path_utf8(stage));
        }
    });
#else
    (void)directory; (void)leaf; (void)prepare; (void)verify_replacement;
    throw std::runtime_error("Application packaging requires Linux x86-64 GNU.");
#endif
}

// Source-package publication uses the same pinned parents, exclusive staging,
// atomic file rename and dependency writer as D3. The short-lived journal is a
// rollback record, never a package index or source authority.
ProjectFiles read_package_directory(const fs::path& root, const ProjectDirectoryReader& reader, bool edit_record) {
    const auto admitted=reader.inspect(root);
    ProjectFiles files;
    std::size_t bytes=0, visited=0;
    std::function<void(const fs::path&,const std::string&,unsigned)> walk;
    walk=[&](const fs::path& directory,const std::string& prefix,unsigned depth) {
        if(depth>64)throw std::runtime_error("Package directory-depth limit exceeded.");
        reader.inspect(directory);
        std::set<std::string> names;
        for(const auto& child:fs::directory_iterator(directory)) {
            if(++visited>32768)throw std::runtime_error("Package directory-entry limit exceeded.");
            auto name=path_utf8(child.path().filename());
            if(!portable_path_component(name) || !names.insert(fold(name)).second || project_path_alias(child.path()))
                throw std::runtime_error("Package path has a collision or alias: "+path_utf8(child.path()));
            auto relative=prefix+name;
            auto status=child.symlink_status();
            if(fs::is_directory(status)) {
                files.emplace(relative+"/","");
                if(!edit_record || !prefix.empty() || (name!="displaced" && name!="discarded"))walk(child.path(),relative+"/",depth+1);
            } else if(fs::is_regular_file(status)) {
                auto content=reader.read(child.path(),64*1024*1024);
                bytes+=content.size();
                if(bytes>256*1024*1024)throw std::runtime_error("Package source byte limit exceeded.");
                files.emplace(std::move(relative),std::move(content));
            } else throw std::runtime_error("Package source must contain regular files and directories.");
        }
    };
    walk(root,"",0);
    if(reader.inspect(root).identity!=admitted.identity)throw std::runtime_error("Package directory changed during inventory.");
    return files;
}

void require_no_package_edit(const fs::path& root) {
    if(!named(root,".void"))return;
    named(root,".void",true);inspect_creation_root(root/".void");
    if(named(root/".void","package-edit"))
        throw std::runtime_error("Interrupted package edit. Run voidc tidy to recover before using this project.");
}
namespace {
struct PackageWriter {
    Parent parent;
#ifdef _WIN32
    NativeHandle writer;
#endif
    explicit PackageWriter(const CreationRoot& root):parent(root.path) {
        if(parent.identity_key()!=root.identity)throw std::runtime_error("Package edit root changed.");
        if(named(root.path,".void"))named(root.path,".void",true);
#ifdef _WIN32
        if(!CreateDirectoryW((root.path/".void").c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
            native_windows::fail("Cannot create package workspace");
#else
        if(flock(parent.handle.fd,LOCK_EX))system_failure("Cannot serialize package edit");
        if(mkdirat(parent.handle.fd,".void",0700) && errno!=EEXIST)system_failure("Cannot create package workspace");
#endif
        Parent workspace(root.path/".void");
#ifdef _WIN32
        writer.value=CreateFileW((root.path/".void/dependency-writer").c_str(),GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(writer.value==INVALID_HANDLE_VALUE)native_windows::fail("Cannot open package writer");
        auto info=identity(writer);
        if(info.nNumberOfLinks!=1 || info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Package writer must be a regular file without aliases.");
        OVERLAPPED offset{};
        if(!LockFileEx(writer.value,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&offset))native_windows::fail("Cannot serialize package edit");
#endif
    }
};
void replace_edit_file(const fs::path& path,std::string_view bytes) {
    Parent parent(path.parent_path());
    const auto temporary=".void-edit-"+random_hex();
#ifdef _WIN32
    auto file=make_file(parent.path/temporary);
#else
    auto file=make_file(parent.handle.fd,temporary);
#endif
    try {
        write_file(file,bytes);
        bool exists=named(parent.path,path_utf8(path.filename()));
        if(exists) {named(parent.path,path_utf8(path.filename()),true);(void)vd::project::read_project_input(path,lock_byte_limit);}
#ifdef _WIN32
        publish_file(file,path,exists);
#else
        if(exists) {
            if(renameat(file.parent,file.name.c_str(),parent.handle.fd,path.filename().c_str()))system_failure("Cannot publish package edit file");
        } else rename_exclusive(file,parent.handle.fd,path_utf8(path.filename()));
#endif
    } catch(...) {if(owns(file))erase(file);throw;}
}
std::map<std::string,std::string_view> borrowed_files(const ProjectFiles& files) {
    std::map<std::string,std::string_view> result;
    for(const auto& [path,bytes]:files)result.emplace(path,bytes);
    return result;
}
void publish_edit_directory(const fs::path& parent,const std::string& leaf,const ProjectFiles& files) {
    publish_project_directory(inspect_creation_root(parent),leaf,borrowed_files(files),
        [&](const fs::path& staged,const ProjectDirectoryReader& reader) {
            if(staged==parent/utf8_path(leaf))throw CreationCollision("Package destination already exists: "+path_utf8(staged));
            if(read_package_directory(staged,reader)!=files)throw std::runtime_error("Package staging content changed.");
        });
}
void rename_edit_directory(const fs::path& source,const fs::path& destination) {
    Parent from(source.parent_path()),to(destination.parent_path());
    named(from.path,path_utf8(source.filename()),true);
#ifdef _WIN32
    Node directory{source,open_node(source,true,false),true};
    publish_file(directory,destination);
#else
    Node directory{from.handle.fd,path_utf8(source.filename()),NativeHandle(openat(from.handle.fd,source.filename().c_str(),dir_flags)),true};
    if(directory.handle.fd<0)system_failure("Cannot pin package directory rename");
    rename_exclusive(directory,to.handle.fd,path_utf8(destination.filename()));
#endif
}
void finish_package_edit(const fs::path& journal) {
    // Atomic removal of the gate is the commit point. Interrupted garbage
    // collection cannot strand a partly deleted recovery record at that name.
    auto garbage=journal.parent_path()/("package-done-"+random_hex());
    rename_edit_directory(journal,garbage);
    std::error_code error;fs::remove_all(garbage,error);
}
void rollback_package_edit(const CreationRoot& root) {
    const auto journal=root.path/".void/package-edit";
    if(!named(root.path/".void","package-edit"))return;
    named(root.path/".void","package-edit",true);
    auto files=read_package_directory(journal,{},true);
    if(!files.contains("before-manifest") || !files.contains("views/"))throw std::runtime_error("Invalid package edit recovery record; retained for inspection.");
    const auto current=vd::project::read_project_input(root.path/manifest_name,manifest_byte_limit);
    if(current!=files.at("before-manifest") && (!files.contains("after-manifest") || current!=files.at("after-manifest")))
        throw std::runtime_error("Manifest changed after interrupted edit; recovery record retained.");
    const auto lock=read_dependency_lock(root.path);
    const auto before=files.contains("before-lock")?std::optional(files.at("before-lock")):std::nullopt;
    const auto after=files.contains("after-lock")?std::optional(files.at("after-lock")):std::nullopt;
    if(lock!=before && lock!=after)throw std::runtime_error("Lock changed after interrupted edit; recovery record retained.");
    std::map<std::string,ProjectFiles> views;
    for(const auto& [path,bytes]:files)if(path.starts_with("views/") && path!="views/") {
        const auto tail=path.substr(6); const auto slash=tail.find('/');
        if(slash==std::string::npos || !portable_path_component(tail.substr(0,slash)))throw std::runtime_error("Invalid package recovery path.");
        auto& view=views[tail.substr(0,slash)];
        if(slash+1<tail.size())view.emplace(tail.substr(slash+1),bytes);
    }
    std::map<std::string,ProjectFiles> replacements;
    for(const auto& [path,bytes]:files)if(path.starts_with("previous/") && path!="previous/") {
        const auto tail=path.substr(9);const auto slash=tail.find('/');
        if(slash==std::string::npos || !views.contains(tail.substr(0,slash)))throw std::runtime_error("Invalid package replacement recovery path.");
        auto& previous=replacements[tail.substr(0,slash)];
        if(slash+1<tail.size())previous.emplace(tail.substr(slash+1),bytes);
    }
    for(const auto& [alias,expected]:replacements) {
        auto backup=journal/"displaced"/utf8_path(alias);
        if(fs::exists(backup) && read_package_directory(backup)!=expected)
            throw std::runtime_error("Displaced package changed after interrupted edit; retained for inspection.");
    }
    // Validate every removal before changing any project-owned state.
    for(const auto& [alias,expected]:views) {
        auto path=root.path/"libs"/utf8_path(alias);
        if(fs::exists(path) || project_path_alias(path)) {
            auto actual=read_package_directory(path);
            if(actual!=expected && (!replacements.contains(alias) || actual!=replacements.at(alias)))
                throw std::runtime_error("Installed source changed after interrupted edit; retained "+path_utf8(path));
        }
    }
    replace_edit_file(root.path/manifest_name,files.at("before-manifest"));
    if(before)replace_edit_file(root.path/lock_name,*before);
    else if(lock)fs::remove(root.path/lock_name);
    for(const auto& [alias,expected]:views) {
        const auto path=root.path/"libs"/utf8_path(alias);
        if(fs::exists(path)) {
            if(replacements.contains(alias) && read_package_directory(path)==replacements.at(alias))continue;
            rename_edit_directory(path,journal/"discarded"/utf8_path(alias));
        }
        if(replacements.contains(alias)) {
            auto backup=journal/"displaced"/utf8_path(alias);
            if(fs::exists(backup))rename_edit_directory(backup,path);
            else publish_edit_directory(root.path/"libs",alias,replacements.at(alias));
        }
    }
    if(files.contains("created-libs") && fs::exists(root.path/"libs") && fs::is_empty(root.path/"libs"))fs::remove(root.path/"libs");
    if(read_package_directory(journal,{},true)!=files)throw std::runtime_error("Package recovery record changed; retained.");
    finish_package_edit(journal);
}
}
void recover_package_edit(const CreationRoot& root) {
    // No workspace creation on a project with no pending transaction.
    try {require_no_package_edit(root.path);return;}catch(const std::runtime_error&) {}
    PackageWriter writer(root);
    rollback_package_edit(root);
}
void publish_package_edit(const CreationRoot& root,std::string_view previous_manifest,
                         const std::optional<std::string>& previous_lock,
                         const std::map<std::string,ProjectFiles>& additions,
                         const std::function<std::pair<std::string,std::optional<std::string>>()>& prepare,
                         CreationCheckpoint checkpoint,
                         const std::map<std::string,ProjectFiles>& replacements) {
    PackageWriter writer(root);
    require_no_package_edit(root.path);
    if(vd::project::read_project_input(root.path/manifest_name,manifest_byte_limit)!=previous_manifest || read_dependency_lock(root.path)!=previous_lock)
        throw std::runtime_error("Project inputs changed before package edit; retry.");
    ProjectFiles record{{"before-manifest",std::string(previous_manifest)},{"views/",""},{"displaced/",""},{"discarded/",""}};
    if(previous_lock)record.emplace("before-lock",*previous_lock);
    if(!additions.empty()) {
        if(named(root.path,"libs")) {named(root.path,"libs",true);inspect_creation_root(root.path/"libs");}
        else record.emplace("created-libs","");
    }
    for(const auto& [alias,files]:additions) {
        if(!portable_path_component(alias))throw std::runtime_error("Invalid package destination alias.");
        if(fs::exists(root.path/"libs") && named(root.path/"libs",alias)) {
            if(!replacements.contains(alias))throw CreationCollision("Package destination already exists: libs/"+alias);
            if(read_package_directory(root.path/"libs"/utf8_path(alias))!=replacements.at(alias))throw std::runtime_error("Previous installed package changed before replacement.");
        }
        if(replacements.contains(alias)) {
            record.emplace("previous/","");record.emplace("previous/"+alias+"/","");
            for(const auto& [name,bytes]:replacements.at(alias))record.emplace("previous/"+alias+"/"+name,bytes);
        }
        record.emplace("views/"+alias+"/","");
        for(const auto& [name,bytes]:files)record.emplace("views/"+alias+"/"+name,bytes);
    }
    publish_edit_directory(root.path/".void","package-edit",record);
    const auto journal=root.path/".void/package-edit";
    try {
        if(!additions.empty() && record.contains("created-libs"))publish_edit_directory(root.path,"libs",{});
        for(const auto& [alias,files]:additions) {
            if(replacements.contains(alias)) {
                if(read_package_directory(root.path/"libs"/utf8_path(alias))!=replacements.at(alias))throw std::runtime_error("Installed package changed during replacement.");
                rename_edit_directory(root.path/"libs"/utf8_path(alias),journal/"displaced"/utf8_path(alias));
                if(checkpoint)checkpoint(journal/"displaced"/utf8_path(alias));
            }
            publish_edit_directory(root.path/"libs",alias,files);
        }
        auto [manifest,lock]=prepare();
        replace_edit_file(journal/"after-manifest",manifest);
        if(lock)replace_edit_file(journal/"after-lock",*lock);
        if(checkpoint)checkpoint(journal); // Includes process-death qualification.
        if(vd::project::read_project_input(root.path/manifest_name,manifest_byte_limit)!=previous_manifest || read_dependency_lock(root.path)!=previous_lock)
            throw std::runtime_error("Project inputs changed during package edit; retry.");
        if(manifest!=previous_manifest)replace_edit_file(root.path/manifest_name,manifest);
        if(lock!=previous_lock) {
            if(lock)replace_edit_file(root.path/lock_name,*lock);
            else if(previous_lock)fs::remove(root.path/lock_name);
        }
        if(checkpoint)checkpoint(root.path); // Before commit, rollback still applies.
        for(const auto& [alias,files]:additions)if(read_package_directory(root.path/"libs"/utf8_path(alias))!=files)
            throw std::runtime_error("Installed source changed during publication.");
        finish_package_edit(journal); // Readers can now admit the complete project.
    } catch(...) {rollback_package_edit(root);throw;}
}

CreationPlan plan_creation(const fs::path& requested,const CreationOptions& options,CreationEntropy entropy) {
    auto root=absolute_target(requested);
    auto leaf=path_utf8(root.filename());
    auto portable=leaf;
    for(auto& c:portable)if(static_cast<unsigned char>(c)>=128)c='x';
    if(options.new_directory && (!root.has_relative_path() || !valid_utf8(leaf) || !portable_path_component(portable) || fold(leaf)==manifest_name))
        throw std::invalid_argument("New project destination requires a valid directory basename.");
    auto parent=inspect_creation_root(options.new_directory?root.parent_path():root);
    root=options.new_directory?parent.path/utf8_path(leaf):parent.path;
    require_destination(root,options.new_directory,options.nested);
    const auto basename=path_utf8(root.filename());
    auto generated=[&](const std::optional<std::string>& supplied,std::string_view prefix) {
        return supplied?*supplied:std::string(prefix)+random_hex(entropy);
    };
    const bool app=options.kind==Kind::application;
    if((app && (options.package_id || options.package_name)) || (!app && options.application_id) || (!options.new_directory && options.source))
        throw std::invalid_argument("Creation options conflict with the project kind or command.");
    auto package=options.package_name.value_or(basename);
    std::optional<std::string> starter;
    if(options.new_directory)starter=options.source.value_or(app?"src/app.void":"src/"+package+".void");
    auto version=options.version.value_or("0.1.0");
    std::string text="schema = 1\n\n[project]\nid = "+toml_string(generated(options.project_id,"local.p"))+
        "\nname = "+toml_string(options.name.value_or(basename))+"\nversion = "+toml_string(version)+
        "\nkind = "+toml_string(app?"application":"library");
    text+=app?"\n\n[application]\nid = ":"\n\n[package]\nid = ";
    text+=toml_string(app?generated(options.application_id,"local.a"):generated(options.package_id,"local.l"));
    if(!app)text+="\nname = "+toml_string(package);
    if(!app && starter)text+="\nentry = "+toml_string(*starter);
    if(starter)text+="\n\n[sources]\nroots = ["+toml_string(starter_parent(*starter))+"]";
    text+='\n';
    auto parsed=parse_manifest(SourceFile(path_utf8(root/manifest_name),std::move(text)));
    if(!parsed.succeeded())throw CompilationError(std::move(parsed.diagnostics));
    return {std::move(parent),std::move(root),std::move(starter),std::move(parsed.manifest),options.nested};
}

void publish_creation(const CreationPlan& plan,CreationCheckpoint before_publish) {
    const auto& root=plan.root;
    const bool fresh=plan.starter.has_value();
    Parent parent(fresh?root.parent_path():root);
    if(parent.path!=plan.parent.path || parent.identity_key()!=plan.parent.identity)
        throw std::runtime_error("Project directory changed after review.");
#ifndef _WIN32
    if(flock(parent.handle.fd,LOCK_EX))system_failure("Cannot serialize project publication in its directory");
#endif
    require_destination(root,fresh,plan.nested);
    const auto temporary=".void-create-"+random_hex();
    const auto stage=parent.path/temporary;
    std::map<fs::path,std::set<std::string>> layout{{stage,{std::string(manifest_name)}}};
    if(fresh) {
        auto directory=stage;
        for(const auto& part:utf8_path(*plan.starter)) {layout[directory].insert(path_utf8(part));directory/=part;}
    }
    std::vector<Node> objects;
    objects.reserve(68);
    const auto& manifest_text=plan.manifest->source().text();
    const std::string_view starter_text=plan.manifest->kind()==Kind::application?
        "func main() -> i32:\n    return 0\n":"public func answer() -> i32:\n    return 42\n";
    bool published=false;
    try {
#ifdef _WIN32
        objects.push_back(make_directory(parent.path/temporary,true));
        objects.push_back(make_file(objects[0].path/manifest_name));
#else
        objects.push_back(make_directory(parent.handle.fd,temporary));
        objects.push_back(make_file(objects[0].handle.fd,std::string(manifest_name)));
#endif
        write_file(objects.back(),manifest_text);
        if(fresh) {
            (void)starter_parent(*plan.starter);
            auto path=utf8_path(*plan.starter);
#ifdef _WIN32
            auto directory=objects[0].path;
            for(const auto& part:path.parent_path()) {directory/=part;objects.push_back(make_directory(directory,false));}
            objects.push_back(make_file(directory/path.filename()));
#else
            int directory=objects[0].handle.fd;
            for(const auto& part:path.parent_path()) {
                objects.push_back(make_directory(directory,path_utf8(part)));directory=objects.back().handle.fd;
            }
            objects.push_back(make_file(directory,path_utf8(path.filename())));
#endif
            write_file(objects.back(),starter_text);
        }
        if(before_publish)before_publish(stage);
        auto rechecked=inspect_creation_root(parent.path);
        if(rechecked.identity!=plan.parent.identity)throw std::runtime_error("Project directory changed before publication.");
        require_destination(root,fresh,plan.nested);
        for(const auto& object:objects)if(!owns(object))
            throw std::runtime_error("Project staging ownership changed before publication.");
        for(const auto& [directory,expected]:layout) {
            std::set<std::string> actual;
            for(const auto& item:fs::directory_iterator(directory)) {
                if(actual.size()>=expected.size())throw std::runtime_error("Unexpected project staging content.");
                actual.insert(path_utf8(item.path().filename()));
            }
            if(actual!=expected)throw std::runtime_error("Unexpected project staging content.");
        }
        if(!matches_file(objects[1],manifest_text) || (fresh && !matches_file(objects.back(),starter_text)))
            throw std::runtime_error("Project staging contents changed before publication.");
        const auto& selected=objects[fresh?0:1];
#ifdef _WIN32
        // NTFS will not rename a directory with open descendants. The root
        // stays pinned; rollback must reopen descendants by their saved IDs.
        if(fresh)for(auto it=objects.begin()+1;it!=objects.end();++it)
            CloseHandle(std::exchange(it->handle.value,INVALID_HANDLE_VALUE));
        publish_file(selected,fresh?root:root/manifest_name);
#else
        rename_exclusive(selected,parent.handle.fd,fresh?path_utf8(root.filename()):std::string(manifest_name));
#endif
        published=true;
        if(!fresh)erase(objects[0]);
        named(fresh?parent.path:root,fresh?path_utf8(root.filename()):std::string(manifest_name),true);
        if(inspect_creation_root(parent.path).identity!=plan.parent.identity)
            throw std::runtime_error("Project directory changed during publication.");
    } catch(const std::exception& error) {
        if(published)
            throw std::runtime_error(std::string(error.what())+" Publication may have completed; inspect "+path_utf8(root)+".");
        bool uncertain=false;
        for(auto it=objects.rbegin();it!=objects.rend();++it)try{erase(*it);}catch(const std::exception&){uncertain=true;}
        if(uncertain)throw std::runtime_error(std::string(error.what())+" Retained uncertain staging content at "+path_utf8(parent.path/temporary)+".");
        throw;
    }
}
}
