#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Stop after the private staging directory exists, before any snapshot is visible.
int mkdirat(int directory, const char *name, mode_t mode) {
    int (*real_mkdirat)(int, const char *, mode_t) = dlsym(RTLD_NEXT, "mkdirat");
    if (!real_mkdirat) _exit(91);
    int result = real_mkdirat(directory, name, mode);
    const char *marker = getenv("VOID_TEST_GIT_MARKER");
    if (!result && marker && strncmp(name, "acquire-", 8) == 0) {
        int fd = open(marker, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) _exit(92);
        close(fd);
        while (1) pause();
    }
    return result;
}
