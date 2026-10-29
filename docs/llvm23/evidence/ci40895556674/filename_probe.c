/* Native observation only. Run on the same filesystem as the CTest fixture.
 * clang filename_probe.c -o /tmp/void-filename-probe
 * /tmp/void-filename-probe build/Debug
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/mount.h>
#endif

int main(int argc, char **argv) {
    char directory[4096], path[4096];
    if (argc != 2 || snprintf(directory, sizeof(directory), "%s/void-name-XXXXXX", argv[1]) >= (int)sizeof(directory))
        return 2;
    if (!mkdtemp(directory)) { perror("mkdtemp"); return 2; }
    if (snprintf(path, sizeof(path), "%s/invalid-\xff", directory) >= (int)sizeof(path))
        return 2;
#ifdef __APPLE__
    struct statfs volume;
    if (statfs(directory, &volume) != 0) { perror("statfs"); return 2; }
    printf("filesystem=%s\n", volume.f_fstypename);
#endif
    errno = 0;
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    int creation_error = fd < 0 ? errno : 0;
    printf("open_status=%d open_errno=%d\n", fd, creation_error);
    if (fd >= 0 && close(fd) != 0) { perror("close"); return 2; }
    struct stat info;
    errno = 0;
    int lookup_status = lstat(path, &info);
    int lookup_error = errno;
    printf("lstat_status=%d lstat_errno=%d\n", lookup_status, lookup_error);
    DIR *stream = opendir(directory);
    if (!stream) { perror("opendir"); return 2; }
    unsigned entries = 0, exact_name = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(stream);
        if (!entry) {
            if (errno) { perror("readdir"); return 2; }
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        ++entries;
        exact_name += !strcmp(entry->d_name, "invalid-\xff");
    }
    if (closedir(stream) != 0) { perror("closedir"); return 2; }
    printf("directory_entries=%u exact_invalid_name=%u\n", entries, exact_name);
    if (fd >= 0 && unlink(path) != 0) { perror("unlink"); return 2; }
    if (rmdir(directory) != 0) { perror("rmdir"); return 2; }
    return 0;
}
