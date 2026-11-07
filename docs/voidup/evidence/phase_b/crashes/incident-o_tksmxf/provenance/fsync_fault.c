/* Test process only: inject a real directory durability failure after rename.
 * No installer decisions and no production linkage. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int fsync(int fd) {
    static int (*original)(int);
    if (!original) original=dlsym(RTLD_NEXT,"fsync");
    char name[64],path[32769];
    const char *target=getenv("VOIDUP_TEST_FAIL_SYNC");
    snprintf(name,sizeof(name),"/proc/self/fd/%d",fd);
    ssize_t count=readlink(name,path,sizeof(path)-1);
    if(target && count>=0) {
        path[count]=0;
        size_t size=strlen(target);
        if(!strcmp(path,target) || (size && target[size-1]=='*' && !strncmp(path,target,size-1))) {errno=EIO;return -1;}
    }
    return original(fd);
}
