#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#endif
#include "vc_filesystem.h"
#include "vc_thread.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#endif

typedef struct VcFileHandle
{
    VcNativeMonitor *monitor;
    bool closed;
    bool seekable;
    bool append;
    int64_t append_start;
#if defined(_WIN32)
    HANDLE file;
#else
    int file;
#endif
} VcFileHandle;

bool vc_fs_windows(void)
{
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

#if defined(_WIN32)
static int32_t fs_error(DWORD value)
{
    switch (value)
    {
        case ERROR_FILE_NOT_FOUND: return 1;
        case ERROR_PATH_NOT_FOUND: case ERROR_DIRECTORY: return 2;
        case ERROR_ACCESS_DENIED: case ERROR_WRITE_PROTECT: return 3;
        case ERROR_ALREADY_EXISTS: case ERROR_FILE_EXISTS: return 4;
        case ERROR_INVALID_NAME: case ERROR_INVALID_PARAMETER: case ERROR_FILENAME_EXCED_RANGE: return 5;
        case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: return 6;
        case ERROR_NOT_SUPPORTED: case ERROR_INVALID_FUNCTION: return 7;
        case ERROR_INVALID_HANDLE: return 9;
        default: return 8;
    }
}
static WCHAR *fs_path(const char *path, int32_t *error)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count == 0) { *error = 5; return NULL; }
    WCHAR *wide = malloc((size_t)count * sizeof(*wide));
    if (wide == NULL) { *error = 6; return NULL; }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) == 0)
    { free(wide); *error = 5; return NULL; }
    return wide;
}
#else
_Static_assert(sizeof(off_t) >= sizeof(int64_t), "VOID filesystem requires 64-bit file offsets");
static int32_t fs_error(int value)
{
    switch (value)
    {
        case ENOENT: return 1;
        case ENOTDIR: return 2;
        case EACCES: case EPERM: case EROFS: case EISDIR: return 3;
        case EEXIST: return 4;
        case EINVAL: case ENAMETOOLONG: return 5;
        case ENOMEM: return 6;
        case ESPIPE: case ENOSYS: return 7;
        case EBADF: return 9;
        default: return 8;
    }
}
#endif

static _Thread_local char fs_owner_token;
static void *fs_owner(void)
{
    void *context = vc_native_thread_context_current();
    return context != NULL ? context : (void *)&fs_owner_token;
}
static bool fs_enter(VcFileHandle *handle)
{
    return handle != NULL && vc_native_monitor_enter(handle->monitor, fs_owner());
}
static void fs_exit(VcFileHandle *handle)
{
    (void)vc_native_monitor_exit(handle->monitor, fs_owner());
}

int32_t vc_fs_open(const char *path, int32_t mode, int32_t access, void **result)
{
    if (result == NULL) return 5;
    *result = NULL;
    if (path == NULL || path[0] == '\0' || mode < 1 || mode > 6 || access < 1 || access > 3 ||
        ((mode == 1 || mode == 2 || mode == 5 || mode == 6) && access == 1) ||
        (mode == 6 && access != 2)) return 5;
    VcFileHandle *handle = calloc(1u, sizeof(*handle));
    if (handle == NULL) return 6;
    handle->monitor = vc_native_monitor_create();
    if (handle->monitor == NULL) { free(handle); return 6; }
    int32_t status = 0;
#if defined(_WIN32)
    WCHAR *wide = fs_path(path, &status);
    if (wide == NULL) goto failed;
    DWORD desired = access == 1 ? GENERIC_READ : access == 2 ? GENERIC_WRITE : GENERIC_READ | GENERIC_WRITE;
    DWORD creation = mode == 1 ? CREATE_NEW : mode == 2 ? CREATE_ALWAYS : mode == 3 ? OPEN_EXISTING :
        mode == 5 ? TRUNCATE_EXISTING : OPEN_ALWAYS;
    handle->file = CreateFileW(wide, desired, 0,
        NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD open_failure = handle->file == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    free(wide);
    if (handle->file == INVALID_HANDLE_VALUE) { status = fs_error(open_failure); goto failed; }
    handle->seekable = GetFileType(handle->file) == FILE_TYPE_DISK;
    if (mode == 6)
    {
        LARGE_INTEGER zero; zero.QuadPart = 0;
        LARGE_INTEGER position;
        if (!SetFilePointerEx(handle->file, zero, &position, FILE_END))
        { status = fs_error(GetLastError()); (void)CloseHandle(handle->file); goto failed; }
        handle->append_start = position.QuadPart;
    }
#else
    int flags = access == 1 ? O_RDONLY : access == 2 ? O_WRONLY : O_RDWR;
    if (mode == 1) flags |= O_CREAT | O_EXCL;
    if (mode == 2) flags |= O_CREAT;
    if (mode == 4 || mode == 6) flags |= O_CREAT;
    flags |= O_CLOEXEC;
    do { handle->file = open(path, flags, 0666); } while (handle->file < 0 && errno == EINTR);
    if (handle->file < 0) { status = fs_error(errno); goto failed; }
    struct stat metadata;
    if (fstat(handle->file, &metadata) != 0)
    { status = fs_error(errno); (void)close(handle->file); goto failed; }
    if (S_ISDIR(metadata.st_mode))
    { status = 3; (void)close(handle->file); goto failed; }
    if (flock(handle->file, LOCK_EX | LOCK_NB) != 0)
    { status = fs_error(errno); (void)close(handle->file); goto failed; }
    if ((mode == 2 || mode == 5) && ftruncate(handle->file, 0) != 0)
    { status = fs_error(errno); (void)close(handle->file); goto failed; }
    handle->seekable = S_ISREG(metadata.st_mode);
    if (mode == 6)
    {
        off_t position = lseek(handle->file, 0, SEEK_END);
        if (position < 0) { status = fs_error(errno); (void)close(handle->file); goto failed; }
        handle->append_start = (int64_t)position;
    }
#endif
    handle->append = mode == 6;
    *result = handle;
    return 0;
failed:
    (void)vc_native_monitor_destroy(handle->monitor);
    free(handle);
    return status;
}

int32_t vc_fs_close(void *resource)
{
    VcFileHandle *handle = resource;
    if (!fs_enter(handle)) return 8;
    int32_t status = 0;
    if (!handle->closed)
    {
        handle->closed = true;
#if defined(_WIN32)
        if (!CloseHandle(handle->file)) status = fs_error(GetLastError());
#else
        /* Never retry close after EINTR: the descriptor may already be released. */
        if (close(handle->file) != 0) status = fs_error(errno);
#endif
    }
    fs_exit(handle);
    return status;
}
void vc_fs_release(void *resource)
{
    VcFileHandle *handle = resource;
    if (handle == NULL) return;
    (void)vc_fs_close(handle);
    (void)vc_native_monitor_destroy(handle->monitor);
    free(handle);
}

bool vc_fs_can_seek(void *resource)
{
    VcFileHandle *handle = resource;
    return handle != NULL && handle->seekable;
}

int32_t vc_fs_read(void *resource, uint8_t *buffer, int32_t count)
{
    VcFileHandle *handle = resource;
    if (count < 0 || (count != 0 && buffer == NULL)) return -5;
    if (!fs_enter(handle)) return -8;
    int32_t result = -9;
    if (!handle->closed)
    {
#if defined(_WIN32)
        DWORD actual = 0;
        result = ReadFile(handle->file, buffer, (DWORD)count, &actual, NULL) ? (int32_t)actual : -fs_error(GetLastError());
#else
        ssize_t actual;
        do { actual = read(handle->file, buffer, (size_t)count); } while (actual < 0 && errno == EINTR);
        result = actual < 0 ? -fs_error(errno) : (int32_t)actual;
#endif
    }
    fs_exit(handle);
    return result;
}
int32_t vc_fs_write(void *resource, const uint8_t *buffer, int32_t count)
{
    VcFileHandle *handle = resource;
    if (count < 0 || (count != 0 && buffer == NULL)) return -5;
    if (!fs_enter(handle)) return -8;
    int32_t result = -9;
    if (!handle->closed)
    {
#if defined(_WIN32)
        DWORD actual = 0;
        result = WriteFile(handle->file, buffer, (DWORD)count, &actual, NULL) ? (int32_t)actual : -fs_error(GetLastError());
#else
        ssize_t actual;
        do { actual = write(handle->file, buffer, (size_t)count); } while (actual < 0 && errno == EINTR);
        result = actual < 0 ? -fs_error(errno) : (int32_t)actual;
#endif
    }
    fs_exit(handle);
    return result;
}
int64_t vc_fs_seek(void *resource, int64_t offset, int32_t origin)
{
    VcFileHandle *handle = resource;
    if (origin < 0 || origin > 2) return -5;
    if (!fs_enter(handle)) return -8;
    int64_t result = -9;
    if (!handle->closed)
    {
#if defined(_WIN32)
        LARGE_INTEGER zero; zero.QuadPart = 0;
        LARGE_INTEGER previous;
        if (!SetFilePointerEx(handle->file, zero, &previous, FILE_CURRENT))
        { result = -fs_error(GetLastError()); fs_exit(handle); return result; }
        LARGE_INTEGER distance; distance.QuadPart = offset;
        LARGE_INTEGER position;
        DWORD method = origin == 0 ? FILE_BEGIN : origin == 1 ? FILE_CURRENT : FILE_END;
        result = SetFilePointerEx(handle->file, distance, &position, method) ? position.QuadPart : -fs_error(GetLastError());
#else
        off_t previous = lseek(handle->file, 0, SEEK_CUR);
        if (previous < 0) { result = -fs_error(errno); fs_exit(handle); return result; }
        off_t position = lseek(handle->file, (off_t)offset, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END);
        result = position < 0 ? (errno == EINVAL ? -8 : -fs_error(errno)) : (int64_t)position;
#endif
        if (result >= 0 && handle->append && result < handle->append_start)
        {
#if defined(_WIN32)
            (void)SetFilePointerEx(handle->file, previous, NULL, FILE_BEGIN);
#else
            (void)lseek(handle->file, previous, SEEK_SET);
#endif
            result = -8;
        }
    }
    fs_exit(handle);
    return result;
}
int64_t vc_fs_length(void *resource)
{
    VcFileHandle *handle = resource;
    if (!fs_enter(handle)) return -8;
    int64_t result = -9;
    if (!handle->closed)
    {
#if defined(_WIN32)
        LARGE_INTEGER length;
        result = GetFileSizeEx(handle->file, &length) ? length.QuadPart : -fs_error(GetLastError());
#else
        struct stat metadata;
        result = fstat(handle->file, &metadata) == 0 ? (int64_t)metadata.st_size : -fs_error(errno);
#endif
    }
    fs_exit(handle);
    return result;
}
int32_t vc_fs_set_length(void *resource, int64_t length)
{
    VcFileHandle *handle = resource;
    if (length < 0) return 5;
    if (!fs_enter(handle)) return 8;
    int32_t result = handle->closed ? 9 : 0;
    if (!handle->closed)
    {
        if (handle->append && length < handle->append_start) result = 8;
        else
        {
#if defined(_WIN32)
            LARGE_INTEGER zero; zero.QuadPart = 0;
            LARGE_INTEGER original;
            LARGE_INTEGER target; target.QuadPart = length;
            if (!SetFilePointerEx(handle->file, zero, &original, FILE_CURRENT)) result = fs_error(GetLastError());
            else
            {
                if (!SetFilePointerEx(handle->file, target, NULL, FILE_BEGIN) || !SetEndOfFile(handle->file)) result = fs_error(GetLastError());
                if (result == 0 && original.QuadPart > length) original.QuadPart = length;
                if (!SetFilePointerEx(handle->file, original, NULL, FILE_BEGIN) && result == 0) result = fs_error(GetLastError());
            }
#else
            if (ftruncate(handle->file, (off_t)length) != 0) result = fs_error(errno);
            else
            {
                off_t position = lseek(handle->file, 0, SEEK_CUR);
                if (position > (off_t)length && lseek(handle->file, (off_t)length, SEEK_SET) < 0) result = fs_error(errno);
            }
#endif
        }
    }
    fs_exit(handle);
    return result;
}
int32_t vc_fs_flush(void *resource)
{
    VcFileHandle *handle = resource;
    if (!fs_enter(handle)) return 8;
    int32_t result = handle->closed ? 9 : 0;
    if (!handle->closed)
    {
#if defined(_WIN32)
        if (!FlushFileBuffers(handle->file)) result = fs_error(GetLastError());
#else
        int flushed;
        do { flushed = fsync(handle->file); } while (flushed != 0 && errno == EINTR);
        if (flushed != 0) result = fs_error(errno);
#endif
    }
    fs_exit(handle);
    return result;
}

int32_t vc_fs_kind(const char *path)
{
    if (path == NULL || path[0] == '\0') return -5;
#if defined(_WIN32)
    int32_t error = 0; WCHAR *wide = fs_path(path, &error);
    if (wide == NULL) return -error;
    DWORD attributes = GetFileAttributesW(wide);
    DWORD failure = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    free(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) return -fs_error(failure);
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 2 : 1;
#else
    struct stat metadata;
    if (stat(path, &metadata) != 0) return -fs_error(errno);
    return S_ISDIR(metadata.st_mode) ? 2 : 1;
#endif
}
int32_t vc_fs_mkdir(const char *path)
{
#if defined(_WIN32)
    int32_t error = 0; WCHAR *wide = fs_path(path, &error);
    if (wide == NULL) return error;
    bool created = CreateDirectoryW(wide, NULL) != 0;
    DWORD failure = created ? ERROR_SUCCESS : GetLastError(); free(wide);
    if (created) return 0;
    if (failure == ERROR_ALREADY_EXISTS && vc_fs_kind(path) == 2) return 0;
    return fs_error(failure);
#else
    if (mkdir(path, 0777) == 0) return 0;
    int error = errno;
    if (error == EEXIST && vc_fs_kind(path) == 2) return 0;
    return fs_error(error);
#endif
}
int32_t vc_fs_delete(const char *path, bool directory)
{
#if defined(_WIN32)
    int32_t error = 0; WCHAR *wide = fs_path(path, &error);
    if (wide == NULL) return error;
    bool removed = directory ? RemoveDirectoryW(wide) != 0 : DeleteFileW(wide) != 0;
    DWORD failure = removed ? ERROR_SUCCESS : GetLastError(); free(wide);
    if (removed) return 0;
    return fs_error(failure);
#else
    if (directory)
    {
        struct stat metadata;
        if (lstat(path, &metadata) == 0 && S_ISLNK(metadata.st_mode))
            return unlink(path) == 0 ? 0 : fs_error(errno);
    }
    if ((directory ? rmdir(path) : unlink(path)) == 0) return 0;
    return fs_error(errno);
#endif
}

/* Root resolution is an OS capability; lexical component normalization belongs
   to editable Path source. -10 requests a larger output buffer. */
int32_t vc_fs_resolve_absolute(const char *path, uint8_t *buffer, int32_t count)
{
    if (path == NULL || buffer == NULL || count <= 0) return -5;
#if defined(_WIN32)
    int32_t error = 0;
    WCHAR *wide = fs_path(path, &error);
    if (wide == NULL) return -error;
    DWORD needed = GetFullPathNameW(wide, 0, NULL, NULL);
    if (needed == 0 || needed > (DWORD)INT_MAX)
    { DWORD failure = GetLastError(); free(wide); return needed == 0 ? -fs_error(failure) : -5; }
    WCHAR *absolute = malloc((size_t)needed * sizeof(*absolute));
    if (absolute == NULL) { free(wide); return -6; }
    DWORD filled = GetFullPathNameW(wide, needed, absolute, NULL);
    DWORD resolve_failure = filled == 0 ? GetLastError() : ERROR_SUCCESS;
    free(wide);
    if (filled == 0 || filled >= needed)
    { free(absolute); return filled == 0 ? -fs_error(resolve_failure) : -10; }
    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, absolute, -1, NULL, 0, NULL, NULL);
    if (bytes == 0) { free(absolute); return -5; }
    if (bytes > count) { free(absolute); return -10; }
    int converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, absolute, -1, (char *)buffer, count, NULL, NULL);
    free(absolute);
    return converted == 0 ? -5 : converted - 1;
#else
    if (path[0] == '/')
    {
        size_t length = strlen(path);
        if (length >= (size_t)count) return -10;
        memcpy(buffer, path, length + 1u); return (int32_t)length;
    }
    if (getcwd((char *)buffer, (size_t)count) == NULL) return errno == ERANGE ? -10 : -fs_error(errno);
    size_t base = strlen((char *)buffer);
    size_t suffix = strlen(path);
    bool separator = base != 0u && buffer[base - 1u] != '/';
    size_t extra = separator ? 1u : 0u;
    if (suffix > (size_t)count - base - 1u || extra > (size_t)count - base - suffix - 1u) return -10;
    if (separator) buffer[base++] = '/';
    memcpy(buffer + base, path, suffix + 1u);
    return (int32_t)(base + suffix);
#endif
}
