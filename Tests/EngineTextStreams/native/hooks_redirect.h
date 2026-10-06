#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include "vc_thread.h"
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
HANDLE WINAPI void_test_CreateFileW(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
BOOL WINAPI void_test_ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL WINAPI void_test_WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
#define CreateFileW void_test_CreateFileW
#define ReadFile void_test_ReadFile
#define WriteFile void_test_WriteFile
#else
#include <unistd.h>
#include <fcntl.h>
int void_test_open(const char *, int, ...);
ssize_t void_test_read(int, void *, size_t);
ssize_t void_test_write(int, const void *, size_t);
#define open void_test_open
#define read void_test_read
#define write void_test_write
#endif
void *void_test_malloc(size_t);
void *void_test_calloc(size_t, size_t);
void *void_test_realloc(void *, size_t);
void void_test_free(void *);
VcNativeMonitor *void_test_monitor_create(void);
#define malloc void_test_malloc
#define calloc void_test_calloc
#define realloc void_test_realloc
#define free void_test_free
#define vc_native_monitor_create void_test_monitor_create
