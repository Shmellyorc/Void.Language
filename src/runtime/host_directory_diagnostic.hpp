#pragma once
// Diagnostic only: no admission decisions, namespace changes or file contents.
// Compiled only into explicitly instrumented qualification Runtime builds.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <limits.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace vd::host::directory_diagnostic {
inline bool scoped(const std::string& requested) noexcept {
    auto raw=std::getenv("HOME");if(!raw)return false;
    std::string_view home(raw),leaf="/first user 雪 ' $() # %";
    auto work=home.rfind("/voidup-first-run-");
    return (home.starts_with("/tmp/voidup-first-run-") || home.starts_with("/var/folders/") || home.starts_with("/private/var/folders/")) &&
        home.size()<4096 && home.ends_with(leaf) && work!=home.npos &&
        home.find('/',work+1)==home.size()-leaf.size() &&
        !requested.empty() && requested[0]=='/' && requested.size()<=4096;
}
struct Snapshot {bool enabled=false;struct stat info{};int code=0;};
inline Snapshot snapshot(const std::string& requested,int parent,const char* name) noexcept {
    Snapshot result;result.enabled=scoped(requested);if(!result.enabled)return result;
    auto saved=errno;if(::fstatat(parent,name,&result.info,AT_SYMLINK_NOFOLLOW))result.code=errno;
    errno=saved;return result;
}
inline std::string quote(std::string_view text) {
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:text) {
        if(c=='"' || c=='\\'){out+='\\';out+=char(c);}
        else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
inline void metadata(std::ostream& out,const struct stat& info,int code) {
    out<<"{\"errno\":"<<code;
    if(!code)out<<",\"device\":"<<info.st_dev<<",\"inode\":"<<info.st_ino<<",\"uid\":"<<info.st_uid
        <<",\"mode\":"<<info.st_mode<<",\"type\":"<<quote(S_ISLNK(info.st_mode)?"symlink":S_ISDIR(info.st_mode)?"directory":S_ISREG(info.st_mode)?"file":"other");
    out<<'}';
}
inline bool recordable(std::string_view value,const std::string& requested) {
    auto raw=std::getenv("HOME");if(!raw)return false;std::string home(raw);
    auto related=[&](const std::string& base){return value==base || value=="/" ||
        (value.size()<base.size() && base.compare(0,value.size(),value)==0 && base[value.size()]=='/') ||
        (value.size()>base.size() && value.substr(0,base.size())==base && value[base.size()]=='/');};
    return related(requested) || related(home) || (home.starts_with("/var/") && related("/private"+home));
}
inline void failure(const std::string& requested,const char* operation,int parent,const char* name,
                    int code,const Snapshot& before) noexcept {
    if(!before.enabled)return;
    auto saved=errno;
    try {
        struct stat after{},held{};int after_code=0,parent_code=0;
        if(::fstatat(parent,name,&after,AT_SYMLINK_NOFOLLOW))after_code=errno;
        if(::fstat(parent,&held))parent_code=errno;
        char physical[PATH_MAX+1]{};int physical_code=0;
#ifdef __APPLE__
        if(::fcntl(parent,F_GETPATH,physical))physical_code=errno;
#else
        char fd_path[64];std::snprintf(fd_path,sizeof fd_path,"/proc/self/fd/%d",parent);
        auto n=::readlink(fd_path,physical,PATH_MAX);if(n<0)physical_code=errno;else physical[n]=0;
#endif
        std::ostringstream out;out<<"VOIDUP_DIRECTORY_DIAGNOSTIC {\"schema\":1,\"requested\":"<<quote(requested)
            <<",\"operation\":"<<quote(operation)<<",\"component\":"<<quote(name)<<",\"errno\":"<<code
            <<",\"open_flags\":"<<(std::string_view(operation)=="mkdirat"?0:O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW)
            <<",\"before\":";metadata(out,before.info,before.code);
        out<<",\"after\":";metadata(out,after,after_code);out<<",\"held_parent\":";metadata(out,held,parent_code);
        out<<",\"physical_parent_errno\":"<<physical_code<<",\"physical_parent\":"
            <<quote(!physical_code && recordable(physical,requested)?physical:"<unavailable or outside request scope>");
        if(!after_code && S_ISLNK(after.st_mode)) {
            char target[PATH_MAX+1];auto n=::readlinkat(parent,name,target,PATH_MAX);auto link_code=n<0?errno:0;
            out<<",\"readlink_errno\":"<<link_code;
            bool standard=n==12 && std::string_view(target,12)=="/private/var" &&
                !physical_code && std::string_view(physical)=="/" && std::string_view(name)=="var";
            out<<",\"link_target\":"<<quote(standard?"/private/var":"<redacted non-system target>");
        }
        out<<"}\n";auto text=out.str();std::size_t offset=0;
        while(offset<text.size()) {auto n=::write(STDERR_FILENO,text.data()+offset,text.size()-offset);
            if(n<0 && errno==EINTR)continue;if(n<=0)break;offset+=std::size_t(n);}
    } catch(...) {} // Diagnostic failure must not replace the original error.
    errno=saved;
}
}
