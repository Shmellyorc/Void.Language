#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
static void hex(const char* value) {for(const unsigned char* p=(const unsigned char*)value;*p;++p)printf("%02x",*p);puts("");}
int main(int argc,char** argv) {
    if(argc==2 && !strcmp(argv[1],"--signal")){raise(SIGTERM);return 99;}
    if(argc==2 && !strcmp(argv[1],"--sleep")){sleep(30);return 0;}
    puts("child");
    for(int n=1;n<argc;++n)hex(argv[n]);
    const char* value=getenv("VOID_CHILD");hex(value?value:"");
    printf("environment %d %d\n",getenv("PATH")==NULL,getenv("LD_PRELOAD")==NULL);
    char cwd[4096];
    puts("cwd");if(!getcwd(cwd,sizeof(cwd)))return 91;hex(cwd);
    // New Runtime resource descriptors must not escape an explicit launch.
    DIR* directory=opendir("/proc/self/fd");int leaked=0;
    if(directory) {
        struct dirent* entry;char path[128],target[4096];
        while((entry=readdir(directory))) {
            int fd=atoi(entry->d_name);if(fd<=2 || fd==dirfd(directory))continue;
            snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);ssize_t count=readlink(path,target,sizeof(target)-1);
            if(count>0){target[count]=0;if(strstr(target,"capability-private") || strstr(target,"package.zip"))++leaked;}
        }
        closedir(directory);
    }
    printf("leaked %d\n",leaked);return 23;
}
