#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Intercept the final replacement of an existing lock. The staged file has
// already been flushed, and the parent process can kill the writer here.
int renameat(int from_dir, const char *from, int to_dir, const char *to) {
    const char *marker = getenv("VOID_TEST_LOCK_MARKER");
    if (strcmp(to, "void.lock") == 0 && marker) {
        int fd = open(marker, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) _exit(91);
        close(fd);
        while (1) pause();
    }
    if (strcmp(to, "void.lock") == 0 && getenv("VOID_TEST_FAIL_LOCK_RENAME")) {
        errno = EIO;
        return -1;
    }
    // All tests run in a private workspace. An unrelated libc rename is a
    // fixture failure so the hook cannot accidentally mask it.
    (void)from_dir;
    (void)from;
    (void)to_dir;
    _exit(92);
}
