#include "native_cache.hpp"
#include "void/path.hpp"
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/SHA256.h>
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
bool private_file(int fd,bool directory) {
    struct stat info{};
    return ::fstat(fd,&info)==0 && info.st_uid==::geteuid() && !(info.st_mode&0077) &&
        (directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode) && info.st_nlink==1);
}
int child(int parent,const char* name) {
    if(::mkdirat(parent,name,0700)!=0 && errno!=EEXIST)return -1;
    int fd=::openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd>=0 && !private_file(fd,true)){::close(fd);return -1;}return fd;
}
int candidate(int directory,const char* name) {
    int flags=O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK;
#ifdef O_NOATIME
    flags|=O_NOATIME; // Rejected candidates must not become recent entries.
#endif
    return ::openat(directory,name,flags);
}
std::optional<NativeArtifact> read_artifact(int fd,std::string_view key) {
    if(fd<0 || !private_file(fd,false))return {};
    struct stat info{};if(::fstat(fd,&info)!=0 || info.st_size<129 ||
        static_cast<std::uintmax_t>(info.st_size)>std::string{}.max_size())return {};
    std::string bytes(static_cast<std::size_t>(info.st_size),'\0');std::size_t position=0;
    while(position<bytes.size()) {auto n=::read(fd,bytes.data()+position,bytes.size()-position);if(n<0 && errno==EINTR)continue;if(n<=0)return {};position+=n;}
    char extra;if(::read(fd,&extra,1)!=0)return {};
    if(bytes.compare(0,64,digest(std::string_view(bytes).substr(64))) || bytes.compare(64,64,key) || (bytes[128]!='0' && bytes[128]!='1'))return {};
    return NativeArtifact{bytes[128]=='1',bytes.substr(129)};
}
bool cache_key(std::string_view name) {
    return name.size()==64 && std::all_of(name.begin(),name.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});
}
void recent(int fd) {
    const timespec times[2]{{0,UTIME_NOW},{0,UTIME_OMIT}};
    ::futimens(fd,times);
}
// Caller holds the directory's exclusive advisory lock. Readers copy bytes
// under a shared lock, then own their object independently of the disk name.
// No recursion, temporary-file scavenging, or authority over unknown contents.
void retain(int directory,std::string_view current) {
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
               S_ISREG(info.st_mode) && info.st_uid==::geteuid() && !(info.st_mode&0077) && info.st_nlink==1)
                entries.push_back({item->d_name,info});
        }
    } catch(...) {::closedir(stream);throw;}
    ::closedir(stream);
    if(entries.size()<=10)return; // Warm lookup normally only scans names/stat.
    // Validate every envelope before giving its name deletion authority. Unsafe
    // or corrupt entries are preserved, just as on the original miss path.
    std::erase_if(entries,[&](const Entry& entry){File file(candidate(directory,entry.name.c_str()));return !read_artifact(file.fd,entry.name);});
    std::sort(entries.begin(),entries.end(),[&](const Entry& a,const Entry& b) {
        if(a.name==current || b.name==current)return b.name==current && a.name!=current;
#ifdef __APPLE__
        auto x=a.info.st_atimespec,y=b.info.st_atimespec;
#else
        auto x=a.info.st_atim,y=b.info.st_atim;
#endif
        if(x.tv_sec!=y.tv_sec)return x.tv_sec<y.tv_sec;
        if(x.tv_nsec!=y.tv_nsec)return x.tv_nsec<y.tv_nsec;
        return a.name<b.name;
    });
    bool removed=false;
    for(std::size_t i=0;i+10<entries.size();++i) {
        const auto& entry=entries[i];struct stat now{};
        if(::fstatat(directory,entry.name.c_str(),&now,AT_SYMLINK_NOFOLLOW)==0 &&
           now.st_dev==entry.info.st_dev && now.st_ino==entry.info.st_ino &&
           now.st_uid==::geteuid() && S_ISREG(now.st_mode) && !(now.st_mode&0077) && now.st_nlink==1)
            removed|=::unlinkat(directory,entry.name.c_str(),0)==0;
    }
    if(removed)::fsync(directory);
}
}
#endif
NativeCache::NativeCache(const std::filesystem::path& root,std::string_view identity) noexcept {
#ifndef _WIN32
    try {
        key_=digest(identity);
        File project(::open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        struct stat info{};
        if(project.fd<0 || ::fstat(project.fd,&info)!=0 || info.st_uid!=::geteuid())return;
        File dot(child(project.fd,".void"));if(dot.fd<0)return;
        File cache(child(dot.fd,"cache"));if(cache.fd<0)return;
        directory_=child(cache.fd,"v1");
    } catch(...) {}
#else
    (void)root;(void)identity; // No unverified Windows ACL/path publication fallback.
#endif
}
NativeCache::~NativeCache(){
#ifndef _WIN32
    if(directory_>=0)::close(directory_);
#endif
}
std::optional<NativeArtifact> NativeCache::read() const noexcept {
#ifndef _WIN32
    try {
        if(!available() || !private_file(directory_,true))return {};
        Lock lock(directory_,LOCK_SH);if(lock.fd<0)return {};
        File file(candidate(directory_,key_.c_str()));
        return read_artifact(file.fd,key_);
    } catch(...) {}
#endif
    return {};
}
void NativeCache::validated() const noexcept {
#ifndef _WIN32
    try {
        if(!available() || !private_file(directory_,true))return;
        Lock lock(directory_,LOCK_EX);if(lock.fd<0)return;
        File file(candidate(directory_,key_.c_str()));
        if(!read_artifact(file.fd,key_))return;
        recent(file.fd);
        retain(directory_,key_);
    } catch(...) {}
#endif
}
bool NativeCache::write(bool coordinated,std::string_view object) const noexcept {
    (void)coordinated;(void)object;
#ifndef _WIN32
    try {
        if(!available() || !private_file(directory_,true))return false;
        auto body=key_+(coordinated ? "1" : "0");body+=object;
        auto bytes=digest(body)+body;
        // Avoid SIGXFSZ: optional persistence must respect the process file limit.
        struct rlimit limit{};
        if(::getrlimit(RLIMIT_FSIZE,&limit)!=0 ||
            (limit.rlim_cur!=RLIM_INFINITY && bytes.size()>limit.rlim_cur))return false;
        Lock lock(directory_,LOCK_EX);if(lock.fd<0)return false;
        static std::atomic<unsigned long long> serial{0};
        auto name="tmp-"+std::to_string(::getpid())+"-"+std::to_string(++serial);
        File file(::openat(directory_,name.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));if(file.fd<0)return false;
        bool okay=true;std::size_t position=0;
        while(position<bytes.size()){auto n=::write(file.fd,bytes.data()+position,bytes.size()-position);if(n<0 && errno==EINTR)continue;if(n<=0){okay=false;break;}position+=n;}
        if(okay)okay=::fsync(file.fd)==0;
        if(::close(file.fd)!=0)okay=false;file.fd=-1;
        // Same-directory link installs complete bytes without replacing a winner.
        if(okay)okay=::linkat(directory_,name.c_str(),directory_,key_.c_str(),0)==0;
        ::unlinkat(directory_,name.c_str(),0);
        if(okay){retain(directory_,key_);::fsync(directory_);}
        return okay;
    } catch(...) {}
#endif
    return false;
}
}
