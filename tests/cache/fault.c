/* Linux syscall faults scoped to this test's exact cache temporary files. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

static void observed(const char* mode) {
    ssize_t (*output)(int,const void*,size_t)=dlsym(RTLD_NEXT,"write");
    const char prefix[]="G95 cache syscall fault: ";
    output(2,prefix,sizeof(prefix)-1);output(2,mode,strlen(mode));output(2,"\n",1);
}
static int selected(int fd) {
    const char* directory=getenv("VOID_CACHE_FAULT_DIRECTORY");
    if(!directory)return 0;
    char link[64],path[4096];
    snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
    ssize_t size=readlink(link,path,sizeof(path)-1);
    if(size<0)return 0;
    path[size]=0;
    size_t length=strlen(directory);
    return !strncmp(path,directory,length) && !strncmp(path+length,"/tmp-",5);
}
static int cache_directory(int fd) {
    const char* directory=getenv("VOID_CACHE_FAULT_DIRECTORY");
    if(!directory)return 0;
    char link[64],path[4096];snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
    ssize_t size=readlink(link,path,sizeof(path)-1);if(size<0)return 0;
    path[size]=0;return !strcmp(path,directory);
}
int openat(int fd,const char* name,int flags,...) {
    static int (*real_openat)(int,const char*,int,...);
    if(!real_openat)real_openat=dlsym(RTLD_NEXT,"openat");
    mode_t permissions=0;
    if(flags&O_CREAT){va_list args;va_start(args,flags);permissions=va_arg(args,int);va_end(args);}
    const char* mode=getenv("VOID_CACHE_FAULT");
    if(mode && !strcmp(mode,"noatime") && (flags&O_NOATIME) && cache_directory(fd)) {
        observed(mode);errno=EPERM;return -1;
    }
    return real_openat(fd,name,flags,permissions);
}
int fstat(int fd,struct stat* information) {
    static int (*real_fstat)(int,struct stat*);
    if(!real_fstat)real_fstat=dlsym(RTLD_NEXT,"fstat");
    int result=real_fstat(fd,information);
    const char* mode=getenv("VOID_CACHE_FAULT");
    if(!result && mode && !strcmp(mode,"other-owner")) {
        const char* directory=getenv("VOID_CACHE_FAULT_DIRECTORY");
        char link[64],path[4096];snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
        ssize_t size=readlink(link,path,sizeof(path)-1);
        if(directory && size>=0) {
            path[size]=0;size_t length=strlen(directory),suffix=strlen("/.void/cache/v1");
            if(length>suffix && !strncmp(path,directory,length-suffix) &&
               (path[length-suffix]==0 || !strcmp(path+length-suffix,"/.void") ||
                !strcmp(path+length-suffix,"/.void/cache") || !strncmp(path+length-suffix,"/.void/cache/v1",15))) {
                observed(mode);information->st_uid=geteuid()+1;
            }
        }
    }
    return result;
}
ssize_t write(int fd,const void* data,size_t size) {
    static ssize_t (*real_write)(int,const void*,size_t);
    static int partial;
    if(!real_write)real_write=dlsym(RTLD_NEXT,"write");
    const char* mode=getenv("VOID_CACHE_FAULT");
    if(mode && !strcmp(mode,"partial") && selected(fd)) {
        if(partial){observed(mode);errno=EDQUOT;return -1;}
        partial=1;
        return real_write(fd,data,size>1?size/2:1);
    }
    return real_write(fd,data,size);
}
int fsync(int fd) {
    static int (*real_fsync)(int);
    if(!real_fsync)real_fsync=dlsym(RTLD_NEXT,"fsync");
    const char* mode=getenv("VOID_CACHE_FAULT");
    if(mode && !strcmp(mode,"fsync") && selected(fd)){observed(mode);errno=EIO;return -1;}
    return real_fsync(fd);
}
int linkat(int olddir,const char* oldname,int newdir,const char* newname,int flags) {
    static int (*real_linkat)(int,const char*,int,const char*,int);
    if(!real_linkat)real_linkat=dlsym(RTLD_NEXT,"linkat");
    const char* mode=getenv("VOID_CACHE_FAULT");
    if(mode && !strcmp(mode,"link") && !strncmp(oldname,"tmp-",4)) {
        char link[64],path[4096];const char* directory=getenv("VOID_CACHE_FAULT_DIRECTORY");
        snprintf(link,sizeof(link),"/proc/self/fd/%d",olddir);
        ssize_t size=readlink(link,path,sizeof(path)-1);
        if(size>=0){path[size]=0;if(directory && !strcmp(path,directory)){observed(mode);errno=EACCES;return -1;}}
    }
    return real_linkat(olddir,oldname,newdir,newname,flags);
}
