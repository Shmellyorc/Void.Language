// Diagnostic link wrapper only. No input injection, delay, or Runtime edits.
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
extern "C" ssize_t __real_read(int,void*,size_t);
static long long now(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return static_cast<long long>(t.tv_sec)*1000000000+t.tv_nsec;}
extern "C" ssize_t __wrap_read(int fd,void* data,size_t size){
    auto start=now();auto count=__real_read(fd,data,size);auto code=errno;auto end=now();
    if(fd==0)if(auto path=std::getenv("VOID_CI97_READ_TRACE")){
        char buffer[8500];auto n=std::snprintf(buffer,sizeof buffer,"{\"pid\":%d,\"start_ns\":%lld,\"end_ns\":%lld,\"requested\":%zu,\"returned\":%zd,\"errno\":%d,\"hex\":\"",getpid(),start,end,size,count,count<0?code:0);
        auto bytes=static_cast<unsigned char*>(data);
        for(ssize_t at=0;at<std::min<ssize_t>(count,4096);++at){buffer[n++]="0123456789abcdef"[bytes[at]>>4];buffer[n++]="0123456789abcdef"[bytes[at]&15];}
        buffer[n++]='"';buffer[n++]='}';buffer[n++]='\n';
        auto log=open(path,O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC,0600);
        if(log>=0){auto ignored=write(log,buffer,n);(void)ignored;close(log);}
    }
    errno=code;return count;
}
