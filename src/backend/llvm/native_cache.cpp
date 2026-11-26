#include "native_cache.hpp"
#include "void/path.hpp"
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/RandomNumberGenerator.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Path.h>
#include "../../runtime/host_posix.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <algorithm>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#ifdef __linux__
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif
#endif
namespace vd::llvm_backend {
std::string digest(std::string_view text) {
    auto hash=llvm::SHA256::hash({reinterpret_cast<const std::uint8_t*>(text.data()),text.size()});
    constexpr char hex[]="0123456789abcdef";std::string result;
    for(auto byte:hash){result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
void cache_field(std::string& out,std::string_view text) {out+=std::to_string(text.size());out+=':';out+=text;}
#ifndef _WIN32
namespace {
struct File {int fd=-1;~File(){if(fd>=0)::close(fd);} explicit File(int value):fd(value){} };
struct Lock {
    int fd;
    Lock(int directory,int operation):fd(directory) {
        while(::flock(fd,operation)!=0) {if(errno!=EINTR){fd=-1;break;}}
    }
    ~Lock(){if(fd>=0)::flock(fd,LOCK_UN);}
};
// RFC 2104 HMAC-SHA-256. The key is always 32 OS-random bytes, so it
// fits in SHA-256's 64-byte block without pre-hashing.
std::string authenticate(std::string_view key,std::string_view body) {
    std::array<std::uint8_t,64> inner{},outer{};
    inner.fill(0x36);outer.fill(0x5c);
    for(std::size_t i=0;i<key.size();++i) {
        inner[i]^=static_cast<std::uint8_t>(key[i]);
        outer[i]^=static_cast<std::uint8_t>(key[i]);
    }
    llvm::SHA256 hash;hash.update(inner);hash.update(llvm::StringRef("VOID native cache v2\n"));
    hash.update(llvm::StringRef(body.data(),body.size()));auto bytes=hash.final();
    llvm::SHA256 final;final.update(outer);final.update(bytes);auto result=final.final();
    constexpr char hex[]="0123456789abcdef";std::string tag;
    for(auto byte:result){tag+=hex[byte>>4];tag+=hex[byte&15];}return tag;
}
std::optional<std::string> authentication_key(int directory) {
    // Only this internal credential is private. Project/cache directories may
    // be shared, and each account gets an independent authenticated namespace.
    Lock lock(directory,LOCK_EX);if(lock.fd<0)return {};
    auto name=".authentication-"+std::to_string(::geteuid());
    File file(::openat(directory,name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));
    if(file.fd<0) {
        if(errno!=ENOENT)return {};
        std::string key(32,'\0');
        if(llvm::getRandomBytes(key.data(),key.size()))return {};
        struct rlimit limit{};
        if(::getrlimit(RLIMIT_FSIZE,&limit))return {};
        // A real process file-size limit prevents persistence, not compilation.
        if(limit.rlim_cur!=RLIM_INFINITY && limit.rlim_cur<key.size())return key;
        std::string nonce(16,'\0');if(llvm::getRandomBytes(nonce.data(),nonce.size()))return {};
        const auto temporary=".authentication-stage-"+digest(nonce);
        file.fd=::openat(directory,temporary.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(file.fd<0)return {};
        struct FreshKey {
            int parent,fd;const std::string& name;bool published=false;
            ~FreshKey() {
                if(published)return;
                struct stat held{},named{};
                if(!::fstat(fd,&held) && !::fstatat(parent,name.c_str(),&named,AT_SYMLINK_NOFOLLOW) &&
                   held.st_dev==named.st_dev && held.st_ino==named.st_ino)::unlinkat(parent,name.c_str(),0);
            }
        } fresh{directory,file.fd,temporary};
#ifdef __APPLE__
        // A new credential must not inherit access grants from a shared ACL.
        auto acl=::acl_init(0);if(!acl)return {};
        auto status=::acl_set_fd_np(file.fd,acl,ACL_TYPE_EXTENDED);::acl_free(acl);
        if(status)return {};
#endif
        struct stat info{};
        if(::fstat(file.fd,&info) || !S_ISREG(info.st_mode) || info.st_uid!=::geteuid() ||
           info.st_nlink!=1 || (info.st_mode&0077) || !vd::host::safe_acl(file.fd))return {};
        std::size_t position=0;
        while(position<key.size()) {
            auto n=::write(file.fd,key.data()+position,key.size()-position);
            if(n<0 && errno==EINTR)continue;
            if(n<=0)return {};position+=n;
        }
        if(::fsync(file.fd))return {};
#ifdef __linux__
        if(::syscall(SYS_renameat2,directory,temporary.c_str(),directory,name.c_str(),1))return {};
#elif defined(__APPLE__)
        if(::renameatx_np(directory,temporary.c_str(),directory,name.c_str(),RENAME_EXCL))return {};
#else
        if(::linkat(directory,temporary.c_str(),directory,name.c_str(),0) || ::unlinkat(directory,temporary.c_str(),0))return {};
#endif
        fresh.published=true;
        if(::fsync(directory))return {};
        return key;
    }
    struct stat info{};
    if(::fstat(file.fd,&info) || !S_ISREG(info.st_mode) || info.st_uid!=::geteuid() ||
       info.st_nlink!=1 || (info.st_mode&0077) || info.st_size!=32 || !vd::host::safe_acl(file.fd))return {};
    std::string key(32,'\0');std::size_t position=0;
    while(position<key.size()) {
        auto n=::read(file.fd,key.data()+position,key.size()-position);
        if(n<0 && errno==EINTR)continue;if(n<=0)return {};position+=n;
    }
    char extra;if(::read(file.fd,&extra,1)!=0)return {};
    return key;
}
std::optional<std::string> cache_authentication(int project_cache) {
    if(auto key=authentication_key(project_cache))return key;
    // Shared/removable filesystems may not represent private files. Keep the
    // credential in ordinary user cache storage in that case; project objects
    // remain project-local and are still authenticated before execution.
    llvm::SmallString<256> base;if(!llvm::sys::path::cache_directory(base))return {};
    const auto path=utf8_path(base.str().str())/"void"/"native-cache-authentication";
    std::error_code error;std::filesystem::create_directories(path,error);if(error)return {};
    File directory(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    return directory.fd<0?std::nullopt:authentication_key(directory.fd);
}
bool cache_object(int fd,bool directory) {
    struct stat info{};
    return ::fstat(fd,&info)==0 && (directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode) && info.st_nlink==1);
}
int child(int parent,const char* name) {
    if(::mkdirat(parent,name,0777)!=0 && errno!=EEXIST)return -1;
    int fd=::openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd>=0 && !cache_object(fd,true)){::close(fd);return -1;}return fd;
}
int candidate(int directory,const char* name) {
    int flags=O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK;
#ifdef O_NOATIME
    flags|=O_NOATIME; // Rejected candidates must not become recent entries.
#endif
    auto fd=::openat(directory,name,flags);
#ifdef O_NOATIME
    // Reading another account's group-accessible artifact may not grant
    // O_NOATIME. Retain normal cache access rather than turning it into a miss.
    if(fd<0 && (errno==EPERM || errno==EACCES || errno==EINVAL))
        fd=::openat(directory,name,flags&~O_NOATIME);
#endif
    return fd;
}
timespec access_time(const struct stat& info) {
#ifdef __APPLE__
    return info.st_atimespec;
#else
    return info.st_atim;
#endif
}
void set_access_time(int fd,timespec access) {
    const timespec times[2]{access,{0,UTIME_OMIT}};
    ::futimens(fd,times);
}
std::optional<NativeArtifact> read_artifact(int fd,std::string_view key,std::string_view authentication) {
    if(fd<0 || !cache_object(fd,false))return {};
    struct stat info{};if(::fstat(fd,&info)!=0 || info.st_size<193 ||
        static_cast<std::uintmax_t>(info.st_size)>std::string{}.max_size())return {};
    std::string bytes(static_cast<std::size_t>(info.st_size),'\0');std::size_t position=0;
    while(position<bytes.size()) {auto n=::read(fd,bytes.data()+position,bytes.size()-position);if(n<0 && errno==EINTR)continue;if(n<=0)return {};position+=n;}
    char extra;if(::read(fd,&extra,1)!=0)return {};
    if(bytes.compare(0,64,digest(std::string_view(bytes).substr(64))) || bytes.compare(64,64,key) || (bytes[128]!='0' && bytes[128]!='1'))return {};
    auto body=std::string_view(bytes).substr(64,bytes.size()-128);
    auto tag=authenticate(authentication,body);
    unsigned difference=0;
    for(std::size_t i=0;i<tag.size();++i)difference|=static_cast<unsigned char>(tag[i])^static_cast<unsigned char>(bytes[bytes.size()-64+i]);
    if(difference)return {};
#ifndef O_NOATIME
    // Validation may inspect the whole cache. Keep that read from changing
    // authenticated entries' eviction order on hosts without O_NOATIME.
    struct stat after{};
    if(info.st_uid==::geteuid() && ::fstat(fd,&after)==0) {
        const auto before=access_time(info), observed=access_time(after);
        if(before.tv_sec!=observed.tv_sec || before.tv_nsec!=observed.tv_nsec)
            set_access_time(fd,before);
    }
#endif
    return NativeArtifact{bytes[128]=='1',bytes.substr(129,bytes.size()-193)};
}
bool cache_key(std::string_view name) {
    return name.size()==64 && std::all_of(name.begin(),name.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});
}
// Caller holds the directory's exclusive advisory lock. Readers copy bytes
// under a shared lock, then own their object independently of the disk name.
// No recursion, temporary-file scavenging, or authority over unknown contents.
void retain(int directory,std::string_view current,std::string_view authentication) {
    struct Entry {std::string name;struct stat info;};
    std::vector<Entry> entries;
    File listing(::openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    auto* stream=listing.fd<0 ? nullptr : ::fdopendir(listing.fd);
    if(!stream)return;
    listing.fd=-1; // The directory stream now owns this descriptor.
    try {
        while(auto* item=::readdir(stream)) {
            if(!cache_key(item->d_name))continue;
            struct stat info{};
            if(::fstatat(directory,item->d_name,&info,AT_SYMLINK_NOFOLLOW)==0 &&
               S_ISREG(info.st_mode) && info.st_nlink==1)
                entries.push_back({item->d_name,info});
        }
    } catch(...) {::closedir(stream);throw;}
    ::closedir(stream);
    if(entries.size()<=10)return; // Warm lookup normally only scans names/stat.
    // Validate every envelope before giving its name deletion authority. Unsafe
    // or corrupt entries are preserved, just as on the original miss path.
    std::erase_if(entries,[&](const Entry& entry){File file(candidate(directory,entry.name.c_str()));return !read_artifact(file.fd,entry.name,authentication);});
    std::sort(entries.begin(),entries.end(),[&](const Entry& a,const Entry& b) {
        if(a.name==current || b.name==current)return b.name==current && a.name!=current;
        const auto x=access_time(a.info),y=access_time(b.info);
        if(x.tv_sec!=y.tv_sec)return x.tv_sec<y.tv_sec;
        if(x.tv_nsec!=y.tv_nsec)return x.tv_nsec<y.tv_nsec;
        return a.name<b.name;
    });
    bool removed=false;
    for(std::size_t i=0;i+10<entries.size();++i) {
        const auto& entry=entries[i];struct stat now{};
        if(::fstatat(directory,entry.name.c_str(),&now,AT_SYMLINK_NOFOLLOW)==0 &&
           now.st_dev==entry.info.st_dev && now.st_ino==entry.info.st_ino &&
           S_ISREG(now.st_mode) && now.st_nlink==1)
            removed|=::unlinkat(directory,entry.name.c_str(),0)==0;
    }
    if(removed)::fsync(directory);
}
}
#endif
NativeCache::NativeCache(const std::filesystem::path& root,std::string_view identity) noexcept {
#ifndef _WIN32
    try {
        File project(::open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        struct stat info{};
        if(project.fd<0 || ::fstat(project.fd,&info)!=0)return;
        File dot(child(project.fd,".void"));if(dot.fd<0)return;
        File cache(child(dot.fd,"cache"));if(cache.fd<0)return;
        auto authentication=cache_authentication(cache.fd);if(!authentication)return;
        authentication_=std::move(*authentication);
        key_=digest(std::string(identity)+digest(authentication_));
        directory_=child(cache.fd,"v1");
    } catch(...) {}
#else
    (void)root;(void)identity; // No unverified Windows ACL/path publication fallback.
#endif
}
NativeCache::~NativeCache(){
    // Avoid leaving the cache credential in ordinary released string storage.
    volatile char* secret=authentication_.data();
    for(std::size_t i=0;i<authentication_.size();++i)secret[i]=0;
#ifndef _WIN32
    if(directory_>=0)::close(directory_);
#endif
}
std::optional<NativeArtifact> NativeCache::read() const noexcept {
#ifndef _WIN32
    try {
        if(!available() || !cache_object(directory_,true))return {};
        Lock lock(directory_,LOCK_SH);if(lock.fd<0)return {};
        File file(candidate(directory_,key_.c_str()));
        return read_artifact(file.fd,key_,authentication_);
    } catch(...) {}
#endif
    return {};
}
void NativeCache::validated() const noexcept {
#ifndef _WIN32
    try {
        if(!available() || !cache_object(directory_,true))return;
        Lock lock(directory_,LOCK_EX);if(lock.fd<0)return;
        File file(candidate(directory_,key_.c_str()));
        if(!read_artifact(file.fd,key_,authentication_))return;
        set_access_time(file.fd,{0,UTIME_NOW});
        retain(directory_,key_,authentication_);
    } catch(...) {}
#endif
}
bool NativeCache::write(bool coordinated,std::string_view object) const noexcept {
    (void)coordinated;(void)object;
#ifndef _WIN32
    try {
        if(!available() || !cache_object(directory_,true))return false;
        auto body=key_+(coordinated ? "1" : "0");body+=object;
        body+=authenticate(authentication_,body);
        auto bytes=digest(body)+body;
        // Avoid SIGXFSZ: optional persistence must respect the process file limit.
        struct rlimit limit{};
        if(::getrlimit(RLIMIT_FSIZE,&limit)!=0 ||
            (limit.rlim_cur!=RLIM_INFINITY && bytes.size()>limit.rlim_cur))return false;
        Lock lock(directory_,LOCK_EX);if(lock.fd<0)return false;
        static std::atomic<unsigned long long> serial{0};
        auto name="tmp-"+std::to_string(::getpid())+"-"+std::to_string(++serial);
        File file(::openat(directory_,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0666));if(file.fd<0)return false;
        bool okay=true;std::size_t position=0;
        while(position<bytes.size()){auto n=::write(file.fd,bytes.data()+position,bytes.size()-position);if(n<0 && errno==EINTR)continue;if(n<=0){okay=false;break;}position+=n;}
        if(okay)okay=::fsync(file.fd)==0;
        if(::close(file.fd)!=0)okay=false;file.fd=-1;
        // Same-directory link installs complete bytes without replacing a winner.
        if(okay)okay=::linkat(directory_,name.c_str(),directory_,key_.c_str(),0)==0;
        ::unlinkat(directory_,name.c_str(),0);
        if(okay){retain(directory_,key_,authentication_);::fsync(directory_);}
        return okay;
    } catch(...) {}
#endif
    return false;
}
}
