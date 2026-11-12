#include "host_internal.hpp"
#include "void/utf8.hpp"
#include "host_posix.hpp"
#include "host_save.hpp"
#include <cerrno>
#include <utility>
#if defined(__linux__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef __linux__
#include <linux/magic.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#else
#include <stdio.h>
#endif
#include <unistd.h>
#endif
namespace vd::host {
#if defined(__linux__) || defined(__APPLE__)
namespace {
struct Descriptor {
    int value=-1;
    explicit Descriptor(int fd):value(fd){}
    ~Descriptor(){if(value>=0)::close(value);}
    std::error_code close() noexcept {
        auto old=std::exchange(value,-1);
        if(old>=0 && ::close(old))return {errno,std::generic_category()};return {};
    }
};
struct Directory final : Object {
    std::shared_ptr<Descriptor> fd,parent;
    std::string name;
    ~Directory() override {close();}
    std::error_code close() noexcept override {
        std::error_code result;
        if(fd && fd.use_count()==1)result=fd->close();fd.reset();
        if(parent && parent.use_count()==1) {auto code=parent->close();if(!result)result=code;}
        parent.reset();return result;
    }
};
struct Lock final : Object {
    int fd=-1;
    std::shared_ptr<Descriptor> parent;
    ~Lock() override {close();}
    std::error_code close() noexcept override {
        int old=std::exchange(fd,-1);parent.reset();
        if(old>=0 && ::close(old))return {errno,std::generic_category()};return {};
    }
};
// One literal component. No normalization, traversal, embedded separators or NUL.
bool component(const std::string& name) {
    return !name.empty() && name.size()<=255 && name!="." && name!=".." && name.find_first_of("/\\\0",0,3)==std::string::npos && valid_utf8(name);
}
bool private_directory(int fd) {
    struct stat info{};return !::fstat(fd,&info) && S_ISDIR(info.st_mode) && info.st_uid==::geteuid() && !(info.st_mode&0077);
}
Result private_regular_file(int fd) {
    struct stat info{};
    if (::fstat(fd, &info)) return native_error(errno);
    if (!S_ISREG(info.st_mode) || info.st_uid != ::geteuid() || info.st_nlink != 1 ||
        (info.st_mode & 0077) || !safe_acl(fd)) return error(2);
    return {};
}
Result lock_file(Lock& lock, int parent, const char* name) {
    lock.fd = ::openat(parent, name, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (lock.fd < 0) return native_error(errno);
    auto checked = private_regular_file(lock.fd);
    if (checked.kind) return checked;
    if (::flock(lock.fd, LOCK_EX | LOCK_NB)) return native_error(errno);
    return {};
}
bool safe_ancestor(int fd) {
    struct stat info{};if(::fstat(fd,&info) || !S_ISDIR(info.st_mode))return false;
    if(info.st_uid!=0 && info.st_uid!=::geteuid())return false;
    return safe_acl(fd) && (!(info.st_mode&0022) || (info.st_uid==0 && (info.st_mode&S_ISVTX)));
}
std::shared_ptr<Descriptor> held(int fd) {
    if(fd<0)return {};
    try{return std::make_shared<Descriptor>(fd);}catch(...){::close(fd);throw;}
}
std::shared_ptr<Directory> root(const std::string& path,bool create,Result& r) {
    if(path.empty() || path[0]!='/' || path.size()>32768 || path.back()=='/' || path.find('\0')!=std::string::npos || !valid_utf8(path)) {r=error(4);return {};}
    auto current=held(::open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
    if(!current){r=native_error(errno);return {};}
    std::size_t start=1;
    while(start<path.size()) {
        auto end=path.find('/',start);auto name=path.substr(start,end==std::string::npos?end:end-start);
        if(!component(name)){r=error(4);return {};}
        if(!safe_ancestor(current->value)){r=error(2);return {};}
        const bool last=end==std::string::npos;
        if(last && create && ::mkdirat(current->value,name.c_str(),0700)) {r=native_error(errno);return {};}
        auto child=held(::openat(current->value,name.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        if(!child){r=native_error(errno);return {};}
        if(last) {
            if(!private_directory(child->value) || !safe_acl(child->value)){r=error(2);return {};}
            auto result=std::make_shared<Directory>();result->fd=std::move(child);result->parent=std::move(current);result->name=std::move(name);return result;
        }
        current=std::move(child);start=end+1;
    }
    r=error(4);return {};
}
Result sync(int fd) {if(::fsync(fd))return native_error(errno);return {};}
Result published(const std::shared_ptr<Descriptor>& source,const std::shared_ptr<Descriptor>& target) {
    Result r;r.flag=true;
    auto first=sync(target->value);auto second=sync(source->value);
    if(first.kind){r.kind=first.kind;r.code=first.code;r.detail=second.code;}
    else if(second.kind){r.kind=second.kind;r.code=second.code;}
    // flag records namespace mutation even when durability is uncertain.
    return r;
}
bool same(int fd,int parent,const std::string& name) {
    struct stat a{},b{};return !::fstat(fd,&a) && !::fstatat(parent,name.c_str(),&b,AT_SYMLINK_NOFOLLOW) && a.st_dev==b.st_dev && a.st_ino==b.st_ino;
}
int rename_held(int source,const char* name,int target,const char* destination,bool replace) {
#ifdef __APPLE__
    return ::renameatx_np(source,name,target,destination,replace?0:RENAME_EXCL);
#else
    return ::syscall(SYS_renameat2,source,name,target,destination,replace?0:RENAME_NOREPLACE);
#endif
}
#ifdef __linux__
constexpr char save_lock_name[] = ".void-save-lock";
constexpr char save_stage_name[] = ".void-save-stage";

class SaveStage {
    Directory& directory;
    const std::string& name;
    int parent;
    Descriptor staged{-1}, previous{-1};
    Lock lock;

    Result verify_directory() const {
        if (!directory.parent || !same(parent, directory.parent->value, directory.name)) return error(4);
        if (!private_directory(parent) || !safe_acl(parent)) return error(2);
        return {};
    }
public:
    SaveStage(Directory& owner, const std::string& destination)
        : directory(owner), name(destination), parent(owner.fd->value) {}

    Result prepare(bool replace) {
        auto checked = verify_directory();
        if (checked.kind) return checked;
        struct statfs volume{};
        if (::fstatfs(parent, &volume)) return native_error(errno);
        if (volume.f_type != XFS_SUPER_MAGIC && volume.f_type != EXT4_SUPER_MAGIC &&
            volume.f_type != TMPFS_MAGIC) return error(11);
        checked = lock_file(lock, parent, save_lock_name);
        if (checked.kind) return checked;
        previous.value = ::openat(parent, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        if (previous.value < 0) return errno == ENOENT ? Result{} : native_error(errno);
        if (!replace) return error(3, EEXIST);
        return private_regular_file(previous.value);
    }

    Result create() {
        staged.value = ::openat(parent, save_stage_name,
            O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
        return has_file() ? private_regular_file(staged.value) : native_error(errno);
    }

    Result publish() {
        auto checked = verify_directory();
        if (checked.kind) return checked;
        if (!same(staged.value, parent, save_stage_name)) return error(4, ESTALE);
        checked = private_regular_file(staged.value);
        if (checked.kind) return checked;
        if (previous.value >= 0) {
            if (!same(previous.value, parent, name)) return error(4, ESTALE);
            checked = private_regular_file(previous.value);
            if (checked.kind) return checked;
            struct stat owner{}, target{};
            if (::fstat(parent, &owner) || ::fstat(previous.value, &target)) return native_error(errno);
            if (owner.st_dev != target.st_dev) return native_error(EXDEV);
        }
        if (rename_held(parent, save_stage_name, parent, name.c_str(), previous.value >= 0))
            return native_error(errno);
        Result result;
        result.flag = true;
        return result;
    }

    int file() const { return staged.value; }
    bool has_file() const { return file() >= 0; }
    Result sync_file() const { return sync(file()); }
    Result sync_directory() const { return sync(parent); }
    int remove() const {
        if (!same(file(), parent, save_stage_name)) return ESTALE;
        return ::unlinkat(parent, save_stage_name, 0) ? errno : 0;
    }
    int close_file() { return staged.close().value(); }
    int close_target() { return previous.close().value(); }
    int close_lock() { return lock.close().value(); }
};
#endif
}
#endif
Result directory_execute(Request& q) {
#ifdef _WIN32
    return windows_directory_execute(q);
#elif !defined(__linux__) && !defined(__APPLE__)
    (void)q;return error(11);
#else
    if(q.operation==file_mode_matches) {
        if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);
        auto native=filesystem::private_mode_matches(*q.file->file(),q.numbers[0]!=0);
        auto result=error(static_cast<std::int64_t>(native.error),native.os_code);result.number=native.number;return result;
    }
    if(q.operation==file_mode) {
        if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);
        auto r=filesystem::private_mode(*q.file->file(),q.numbers[0]!=0);return error(static_cast<std::int64_t>(r.error),r.os_code);
    }
    if(q.operation==dir_create || q.operation==dir_admit) {
        Result r;q.acquired=root(q.texts[0],q.operation==dir_create,r);return r;
    }
    auto d=std::static_pointer_cast<Directory>(q.objects[0]);if(!d->fd)return error(7);
    int fd=d->fd->value;
    if(q.operation==dir_save) {
#ifdef __linux__
        const auto& name = q.texts.front();
        if (!component(name) || name == save_lock_name || name == save_stage_name) return error(4);
        SaveStage stage(*d, name);
        return save_atomic(stage, q);
#else
        return error(11);
#endif
    }
    if(q.operation==dir_sync)return sync(fd);
    if(q.operation==dir_child || q.operation==dir_open) {
        if(!component(q.texts[0]))return error(4);
        if(q.operation==dir_child && ::mkdirat(fd,q.texts[0].c_str(),0700))return native_error(errno);
        auto child=held(::openat(fd,q.texts[0].c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        if(!child)return native_error(errno);
        if(!private_directory(child->value) || !safe_acl(child->value))return error(2);
        auto value=std::make_shared<Directory>();value->fd=std::move(child);value->parent=d->fd;value->name=q.texts[0];q.acquired=value;return {};
    }
    if(q.operation==dir_file) {
        if(!component(q.texts[0]))return error(4);
        if(q.numbers[0]!=0 && q.numbers[0]!=1)return error(6);
        const bool create=q.numbers[0]==1;
        int file=::openat(fd,q.texts[0].c_str(),O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK|(create?O_RDWR|O_CREAT|O_EXCL:O_RDONLY),0600);
        if(file<0)return native_error(errno);
        auto guard=held(file);auto checked=private_regular_file(file);if(checked.kind)return checked;
        // Transfer directly to the ordinary IO file ledger, retaining identity.
        guard->value=-1;q.acquired_file=filesystem::adopt_descriptor(file,create);return {};
    }
    if(q.operation==dir_lock) {
        if(!component(q.texts[0]))return error(4);
        auto lock=std::make_shared<Lock>();lock->parent=d->fd;
        auto checked=lock_file(*lock,fd,q.texts[0].c_str());if(checked.kind)return checked;
        q.acquired=lock;return {};
    }
    auto target=std::static_pointer_cast<Directory>(q.objects[1]);if(!target->fd)return error(7);
    if(q.operation==dir_publish) {
        if(!component(q.texts[0]) || !d->parent)return error(4);
        if(!same(fd,d->parent->value,d->name))return error(4);
        auto ready=sync(fd);if(ready.kind)return ready;
        if(rename_held(d->parent->value,d->name.c_str(),target->fd->value,q.texts[0].c_str(),false))return native_error(errno);
        auto source=d->parent;
        // Allocate all replacement bookkeeping before rename in the caller below.
        d->parent=target->fd;d->name.swap(q.texts[0]);
        return published(source,target->fd);
    }
    if(q.operation==dir_replace) {
        if(!component(q.texts[0]) || !component(q.texts[1]) || (q.numbers[0]!=0 && q.numbers[0]!=1))return error(4);
        int file=filesystem::descriptor(*q.file->file());
        if(file<0 || !same(file,fd,q.texts[0]))return error(4);
        auto checked=private_regular_file(file);if(checked.kind)return checked;
        auto ready=sync(file);if(ready.kind)return ready;
        if(rename_held(fd,q.texts[0].c_str(),target->fd->value,q.texts[1].c_str(),q.numbers[0]!=0))return native_error(errno);
        return published(d->fd,target->fd);
    }
    throw std::logic_error("Invalid directory operation");
#endif
}
}
