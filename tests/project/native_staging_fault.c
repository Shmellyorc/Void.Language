#define _GNU_SOURCE
/* Focused syscall faults for the private native staging constructor only. */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *fault(void) {
    const char *value = getenv("VOID_STAGING_FAULT");
    return value ? value : "";
}

static int descriptor_path(int fd, char path[4096]) {
    char name[64];
    snprintf(name, sizeof name, "/proc/self/fd/%d", fd);
    ssize_t size = readlink(name, path, 4095);
    if (size < 0) return 0;
    path[size] = 0;
    return 1;
}

static int run_parent(int fd) {
    char path[4096];
    const char *root = getenv("VOID_STAGING_TEST_RUN");
    return root && descriptor_path(fd, path) && !strcmp(root, path);
}

static int stage_descriptor(int fd, int marker) {
    char path[4096];
    const char *root = getenv("VOID_STAGING_TEST_RUN");
    if (!root || !descriptor_path(fd, path)) return 0;
    size_t size = strlen(root);
    if (strncmp(root, path, size) || strncmp(path + size, "/native-", 8)) return 0;
    const char *tail = path + size + 8;
    if (strlen(tail) != (marker ? 23u : 16u)) return 0;
    return !marker || !strcmp(tail + 16, "/.owner");
}

int openat(int fd, const char *name, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
    }
    int (*next)(int, const char *, int, ...) = dlsym(RTLD_NEXT, "openat");
    if (run_parent(fd) && strlen(name) == 23 && !strncmp(name, "native-", 7) && (flags & O_DIRECTORY)) {
        if (!strcmp(fault(), "replace-open")) {
            if (renameat(fd, name, fd, "fault-created-parked") || mkdirat(fd, name, 0700)) _exit(91);
            int replacement = next(fd, name, O_RDONLY | O_DIRECTORY);
            int keep = next(replacement, "KEEP", O_WRONLY | O_CREAT | O_EXCL, 0600);
            if (replacement < 0 || keep < 0) _exit(92);
            close(keep);
            close(replacement);
            errno = EMFILE;
            return -1;
        }
        if (!strcmp(fault(), "open")) { errno = EMFILE; return -1; }
    }
    if (stage_descriptor(fd, 0) && (flags & O_CREAT)) {
        if (!strcmp(name, ".owner") && !strcmp(fault(), "marker-open")) { errno = EACCES; return -1; }
        if (strstr(name, ".so")) {
            if (!strcmp(fault(), "payload-permission")) { errno = EACCES; return -1; }
            if (!strcmp(fault(), "payload-quota")) { errno = EDQUOT; return -1; }
        }
    }
    return next(fd, name, flags, mode);
}

int fstat(int fd, struct stat *info) {
    int (*next)(int, struct stat *) = dlsym(RTLD_NEXT, "fstat");
    if (!strcmp(fault(), "inspect") && stage_descriptor(fd, 0)) { errno = EIO; return -1; }
    return next(fd, info);
}

ssize_t write(int fd, const void *data, size_t size) {
    ssize_t (*next)(int, const void *, size_t) = dlsym(RTLD_NEXT, "write");
    if (stage_descriptor(fd, 1)) {
        if (!strcmp(fault(), "marker-write")) { errno = EDQUOT; return -1; }
        if (!strcmp(fault(), "kill-marker")) {
            if (size < 3 || next(fd, data, 3) != 3) _exit(93);
            kill(getpid(), SIGKILL);
            _exit(94);
        }
        if (!strcmp(fault(), "short-marker") && size > 1) return next(fd, data, 1);
        static int interrupted;
        if (!strcmp(fault(), "eintr-marker") && !interrupted++) { errno = EINTR; return -1; }
    }
    return next(fd, data, size);
}
