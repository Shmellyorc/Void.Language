#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <wchar.h>

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) return 2;
    HANDLE parent = CreateFileW(argv[1], FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    HANDLE provider = CreateFileW(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    HANDLE dep = CreateFileW(argv[3], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (parent == INVALID_HANDLE_VALUE || provider == INVALID_HANDLE_VALUE || dep == INVALID_HANDLE_VALUE) {
        wprintf(L"pin failed: %lu\n", GetLastError());
        return 1;
    }
    wchar_t expected[1024], actual[1024];
    DWORD a = GetFinalPathNameByHandleW(provider, expected, 1024, VOLUME_NAME_NT);
    if (!a || a >= 1024) return 1;
    HMODULE module = LoadLibraryExW(argv[2], NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        wprintf(L"LoadLibraryExW failed: %lu\n", GetLastError());
        return 1;
    }
    DWORD b = GetMappedFileNameW(GetCurrentProcess(), module, actual, 1024);
    if (!b || b >= 1024) return 1;
    const volatile int* sentinel = (const volatile int*)GetProcAddress(module, "g4_init_marker");
    wprintf(L"held: %ls\nmapped: %ls\npath-match=%d initializer-marker=%d\n",
            expected, actual, !wcscmp(expected, actual), sentinel ? *sentinel : -1);
    FreeLibrary(module);
    CloseHandle(dep);
    CloseHandle(provider);
    CloseHandle(parent);
    return 0;
}
