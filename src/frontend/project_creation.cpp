#include "void/project_creation.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/utf8.hpp"
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
    auto expected=fold(leaf);
    for (const auto& child : fs::directory_iterator(directory)) {
        if (++seen > entry_limit) throw std::runtime_error("Project creation directory-entry limit exceeded.");
        auto name=path_utf8(child.path().filename());
        if (fold(name)==expected) {++matches;exact|=name==leaf;}
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
struct Parent {
    fs::path path;
    std::vector<NativeHandle> pins;
    explicit Parent(fs::path requested):path(absolute_target(requested)),pins(native_windows::pin_directories(path)) {
        auto current=path.root_path();
        for (const auto& part:path.relative_path()) {named(current,path_utf8(part),true);current/=part;}
        path=fs::canonical(path);
    }
    std::string identity_key() const {return key(identity(pins.back()));}
};
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
void rename_exclusive(const Node& node,const fs::path& destination) {
    auto text=destination.native();
    std::vector<std::byte> bytes(sizeof(FILE_RENAME_INFO)+text.size()*sizeof(wchar_t));
    auto* rename=reinterpret_cast<FILE_RENAME_INFO*>(bytes.data());
    rename->ReplaceIfExists=FALSE;rename->RootDirectory=nullptr;
    rename->FileNameLength=static_cast<DWORD>(text.size()*sizeof(wchar_t));
    std::copy(text.begin(),text.end(),rename->FileName);
    if(SetFileInformationByHandle(node.handle.value,FileRenameInfo,rename,static_cast<DWORD>(bytes.size())))return;
    auto error=GetLastError();
    if(error==ERROR_ALREADY_EXISTS || error==ERROR_FILE_EXISTS)
        throw CreationCollision("Exclusive project publication collided with an existing destination.");
    native_windows::fail("Cannot publish project exclusively",error);
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
    explicit Parent(fs::path requested):path(absolute_target(requested)),handle(open(path.root_path().c_str(),dir_flags)) {
        if(handle.fd<0)system_failure("Cannot open project filesystem root");
        auto current=path.root_path();
        for (const auto& part:path.relative_path()) {
            named(current,path_utf8(part),true);
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
        rename_exclusive(selected,fresh?root:root/manifest_name);
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
