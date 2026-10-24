#pragma once
#ifndef _WIN32
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <string>

namespace console_native_test {
struct Flags {std::array<int,6> values{},errors{};};
inline Flags flags(){
    Flags result;
    for(int fd=0;fd<3;++fd)for(int n=0;n<2;++n){
        auto at=fd*2+n;auto value=fcntl(fd,n?F_GETFL:F_GETFD);
        result.values[at]=value;result.errors[at]=value<0?errno:0;
    }
    return result;
}
inline bool same_flags(const Flags& before,const Flags& after){
    bool same=before.errors==after.errors;
    for(unsigned n=0;n<6;++n){
        auto a=before.values[n],b=after.values[n];
#ifdef __APPLE__
        // XNU dofilewrite marks FWASWRITTEN (0x10000); F_GETFL exposes it.
        // It is not a caller-set status flag. Preserve every other bit and
        // every F_GETFD value, and retain the unmasked native observations.
        if(n%2 && a>=0 && b>=0)b&=~(b&~a&0x10000);
#endif
        same=same && a==b;
    }
    return same;
}
inline bool flags_unchanged(const Flags& before,const char* path){
    auto after=flags();bool same=same_flags(before,after);
    std::ofstream out(std::string(path)+".flags.json");
    out<<"{\"equivalent\":"<<(same?"true":"false")<<",\"descriptors\":[";
    for(unsigned fd=0;fd<3;++fd){
        if(fd)out<<',';
        out<<"{\"fd\":"<<fd<<",\"before\":["<<before.values[fd*2]<<','<<before.values[fd*2+1]
           <<"],\"after\":["<<after.values[fd*2]<<','<<after.values[fd*2+1]
           <<"],\"before_errno\":["<<before.errors[fd*2]<<','<<before.errors[fd*2+1]
           <<"],\"after_errno\":["<<after.errors[fd*2]<<','<<after.errors[fd*2+1]<<"]}";
    }
    out<<"]}\n";return same && bool(out);
}
}
#endif
