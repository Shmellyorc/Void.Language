#define _POSIX_C_SOURCE 200809L
#include "vc_thread.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
static int live;
static bool registered;
static void check_released(void)
{
    if (live != 0) { fputs("native resource allocation leaked\n", stderr); _Exit(97); }
}
static void register_leak_check(void)
{
    if (!registered) { registered = true; if (atexit(check_released) != 0) abort(); }
}
void *void_test_malloc(size_t size)
{
    register_leak_check();
    void *result = malloc(size);
    if (result != NULL) live++;
    return result;
}
void *void_test_calloc(size_t count, size_t size)
{
    register_leak_check();
    if (getenv("VOID_TEST_STATE_OOM") != NULL) return NULL;
    void *result = calloc(count, size);
    if (result != NULL) live++;
    return result;
}
void *void_test_realloc(void *memory, size_t size)
{
    register_leak_check();
    const bool had_allocation = memory != NULL;
    void *result = realloc(memory, size);
    if (result != NULL && !had_allocation) live++;
    /* The native CRT/glibc zero-size case can release the old allocation and
     * return NULL. A nonzero failed resize leaves the old allocation live;
     * a successful resize (including a non-NULL zero-size result) keeps one. */
    if (result == NULL && had_allocation && size == 0) live--;
    return result;
}
void void_test_free(void *memory)
{
    if (memory != NULL) live--;
    free(memory);
}
VcNativeMonitor *void_test_monitor_create(void)
{
    return getenv("VOID_TEST_MONITOR_OOM") != NULL ? NULL : vc_native_monitor_create();
}
#ifdef _WIN32
HANDLE WINAPI void_test_CreateFileW(LPCWSTR path, DWORD access, DWORD sharing,
    LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD attributes, HANDLE template_file)
{
    if (getenv("VOID_TEST_DENIED") != NULL) {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    return CreateFileW(path, access, sharing, security, creation, attributes, template_file);
}
BOOL WINAPI void_test_ReadFile(HANDLE file, LPVOID buffer, DWORD count,
    LPDWORD actual, LPOVERLAPPED overlapped)
{
    return ReadFile(file, buffer, count > 2u ? 2u : count, actual, overlapped);
}
BOOL WINAPI void_test_WriteFile(HANDLE file, LPCVOID buffer, DWORD count,
    LPDWORD actual, LPOVERLAPPED overlapped)
{
    return WriteFile(file, buffer, count > 3u ? 3u : count, actual, overlapped);
}
#else
int void_test_open(const char *path, int flags, ...)
{
    va_list arguments; va_start(arguments, flags);
    int mode = va_arg(arguments, int); va_end(arguments);
    if (getenv("VOID_TEST_DENIED") != NULL) { errno = EACCES; return -1; }
    return open(path, flags, mode);
}
ssize_t void_test_read(int file, void *buffer, size_t count)
{
    return read(file, buffer, count > 2u ? 2u : count);
}
ssize_t void_test_write(int file, const void *buffer, size_t count)
{
    return write(file, buffer, count > 3u ? 3u : count);
}
#endif
