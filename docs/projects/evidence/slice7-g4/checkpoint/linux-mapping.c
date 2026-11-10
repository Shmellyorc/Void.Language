#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

typedef int (*Value)(void);
typedef Value (*DependencyAddress)(void);

static void require(int okay, const char* step) {
    if (okay) return;
    const char* error = dlerror();
    fprintf(stderr, "%s failed: %s, errno=%d\n", step, error ? error : "no loader message", errno);
    exit(1);
}

static int sealed(const char* path) {
    int input = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    require(input >= 0, "open input");
    int image = memfd_create("void-g4-checkpoint", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    require(image >= 0, "memfd_create");
    char block[8192];
    ssize_t size;
    while ((size = read(input, block, sizeof block)) > 0)
        require(write(image, block, (size_t)size) == size, "copy input");
    require(size == 0 && close(input) == 0, "finish input");
    int seals = F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE;
    require(fcntl(image, F_ADD_SEALS, seals) == 0, "seal memfd");
    require(fcntl(image, F_GET_SEALS) == seals, "verify seals");
    require(pwrite(image, "X", 1, 0) < 0 && errno == EPERM, "reject mutation after seal");
    printf("sealed file %s (seals=%#x), write rejected\n", path, seals);
    return image;
}

static void* load(Lmid_t ns, int image) {
    char path[64];
    snprintf(path, sizeof path, "/proc/self/fd/%d", image);
    void* handle = dlmopen(ns, path, RTLD_NOW | RTLD_LOCAL);
    require(handle != NULL, "dlmopen sealed fd");
    return handle;
}

static void mapped(const char* label, void* symbol, int image) {
    Dl_info info;
    require(dladdr(symbol, &info), "dladdr");
    struct stat expected;
    require(fstat(image, &expected) == 0, "fstat sealed fd");
    FILE* maps = fopen("/proc/self/maps", "r");
    require(maps != NULL, "maps open");
    char line[8192];
    int matches = 0;
    while (fgets(line, sizeof line, maps)) {
        uintptr_t low, high;
        unsigned dev_major, dev_minor;
        uintmax_t offset, inode;
        char perms[5];
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxMAX " %x:%x %" SCNuMAX,
                   &low, &high, perms, &offset, &dev_major, &dev_minor, &inode) != 7) continue;
        if ((uintptr_t)symbol < low || (uintptr_t)symbol >= high) continue;
        printf("%s owner %s, sealed device %x:%x inode %" PRIuMAX "\n  %s",
               label, info.dli_fname, major(expected.st_dev), minor(expected.st_dev),
               (uintmax_t)expected.st_ino, line);
        matches = perms[2] == 'x' && inode == expected.st_ino &&
                  dev_major == major(expected.st_dev) && dev_minor == minor(expected.st_dev);
        break;
    }
    fclose(maps);
    require(matches, "executable mapping identity");
}

int main(int argc, char** argv) {
    require(argc == 4, "expected absolute provider, dependency and competitor paths");
    int provider = sealed(argv[1]), dependency = sealed(argv[2]);
    void* competing = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    require(competing != NULL, "preload competing SONAME in base namespace");
    require(((Value)dlsym(competing, "g4_dependency"))() == 99, "competing value");
    require(rename(argv[3], argv[2]) == 0, "replace admitted dependency pathname");
    puts("replaced original dependency path after seal; competing SONAME mapped in base namespace");
    void* dep = load(LM_ID_NEWLM, dependency);
    Lmid_t ns;
    require(dlinfo(dep, RTLD_DI_LMID, &ns) == 0 && ns != LM_ID_BASE, "isolated namespace");
    void* main = load(ns, provider);
    struct link_map* link;
    require(dlinfo(main, RTLD_DI_LINKMAP, &link) == 0, "namespace map");
    while (link->l_prev) link = link->l_prev;
    unsigned count = 0;
    for (; link; link = link->l_next) {
        printf("namespace module %s\n", link->l_name);
        ++count;
    }
    require(count == 2, "exact namespace closure");
    Value value = (Value)dlsym(main, "g4_provider");
    DependencyAddress address = (DependencyAddress)dlsym(main, "g4_dependency_address");
    require(value && address, "required exports");
    mapped("provider", (void*)value, provider);
    mapped("dependency reached by provider", (void*)address(), dependency);
    void* fallback = dlsym(main, "g4_dependency");
    require(fallback && fallback == (void*)address(), "exact-handle lookup can return dependency symbol");
    mapped("exact-handle lookup for dependency-only export", fallback, dependency);
    int result = value();
    printf("result=%d\n", result);
    require(result == 42, "isolated provider result");
    require(!dlclose(main) && !dlclose(dep) && !dlclose(competing), "release modules");
    require(!close(provider) && !close(dependency), "release sealed fd");
}
