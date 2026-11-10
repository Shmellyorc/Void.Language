#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { puts(dlerror()); return 3; }
    void (*init)(int,int,const char*) = (void (*)(int,int,const char*))dlsym(lib, "InitWindow");
    bool (*ready)(void) = (bool (*)(void))dlsym(lib, "IsWindowReady");
    void (*close_window)(void) = (void (*)(void))dlsym(lib, "CloseWindow");
    if (!init || !ready || !close_window) return 4;
    init(360,240,"Raylib native ASan driver control");
    if (!ready()) return 5;
    FILE* source = fopen("/proc/self/maps","r");
    FILE* target = fopen("native-control-maps.txt","w");
    if (!source || !target) return 6;
    char buf[4096];
    while (fgets(buf,sizeof buf,source)) fputs(buf,target);
    fclose(source);
    fclose(target);
    close_window();
    if (ready()) return 7;
    dlclose(lib);
    puts("C-only raylib window closed and library unloaded");
    fflush(stdout);
    return 0;
}
