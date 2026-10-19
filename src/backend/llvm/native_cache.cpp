#include "native_cache.hpp"
#include "void/path.hpp"
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/SHA256.h>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/resource.h>
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
bool private_file(int fd,bool directory) {
    struct stat info{};
    return ::fstat(fd,&info)==0 && info.st_uid==::geteuid() && !(info.st_mode&0077) &&
        (directory ? S_ISDIR(info.st_mode) : S_ISREG(info.st_mode));
}
int child(int parent,const char* name) {
    if(::mkdirat(parent,name,0700)!=0 && errno!=EEXIST)return -1;
    int fd=::openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd>=0 && !private_file(fd,true)){::close(fd);return -1;}return fd;
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
        if(!available())return {};
        File file(::openat(directory_,key_.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK));
        if(file.fd<0 || !private_file(file.fd,false))return {};
        struct stat info{};if(::fstat(file.fd,&info)!=0 || info.st_size<129 ||
            static_cast<std::uintmax_t>(info.st_size)>std::string{}.max_size())return {};
        std::string bytes(static_cast<std::size_t>(info.st_size),'\0');std::size_t position=0;
        while(position<bytes.size()) {auto n=::read(file.fd,bytes.data()+position,bytes.size()-position);if(n<0 && errno==EINTR)continue;if(n<=0)return {};position+=n;}
        char extra;if(::read(file.fd,&extra,1)!=0)return {};
        // Header fields and all object bytes are protected by one envelope digest.
        if(bytes.compare(0,64,digest(std::string_view(bytes).substr(64))) || bytes.compare(64,64,key_) || (bytes[128]!='0' && bytes[128]!='1'))return {};
        return NativeArtifact{bytes[128]=='1',bytes.substr(129)};
    } catch(...) {}
#endif
    return {};
}
bool NativeCache::write(bool coordinated,std::string_view object) const noexcept {
    (void)coordinated;(void)object;
#ifndef _WIN32
    try {
        if(!available())return false;
        auto body=key_+(coordinated ? "1" : "0");body+=object;
        auto bytes=digest(body)+body;
        // Avoid SIGXFSZ: optional persistence must respect the process file limit.
        struct rlimit limit{};
        if(::getrlimit(RLIMIT_FSIZE,&limit)!=0 ||
            (limit.rlim_cur!=RLIM_INFINITY && bytes.size()>limit.rlim_cur))return false;
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
        if(okay)::fsync(directory_);
        return okay;
    } catch(...) {}
#endif
    return false;
}
}
