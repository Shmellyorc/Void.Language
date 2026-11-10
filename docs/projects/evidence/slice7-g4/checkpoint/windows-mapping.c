#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

typedef int (*Value)(void);
typedef Value (*DependencyAddress)(void);

static void fail(const wchar_t* operation) {
    fwprintf(stderr, L"%ls failed with Windows error %lu\n", operation, GetLastError());
    ExitProcess(1);
}

static HANDLE hold(const wchar_t* path, int directory) {
    DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0);
    HANDLE file = CreateFileW(path, directory ? FILE_READ_ATTRIBUTES : GENERIC_READ,
                              FILE_SHARE_READ, NULL, OPEN_EXISTING, flags, NULL);
    if (file == INVALID_HANDLE_VALUE) fail(L"CreateFileW");
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(file, &info)) fail(L"GetFileInformationByHandle");
    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) fail(L"unexpected reparse point");
    if (!directory && info.nNumberOfLinks != 1) fail(L"unexpected hardlink");
    wprintf(L"held %ls id %08lx:%08lx%08lx\n", path,
            info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow);
    return file;
}

static int mapped(const wchar_t* label, const void* address, HANDLE file) {
    wchar_t expected[1024], actual[1024];
    MEMORY_BASIC_INFORMATION region;
    if (!VirtualQuery(address, &region, sizeof region)) fail(L"VirtualQuery");
    DWORD a = GetMappedFileNameW(GetCurrentProcess(), (void*)address, actual, 1024);
    DWORD b = GetFinalPathNameByHandleW(file, expected, 1024, VOLUME_NAME_NT);
    if (!a || a >= 1024 || !b || b >= 1024) fail(L"mapped and held paths");
    int match = region.Type == MEM_IMAGE && !wcscmp(expected, actual);
    wprintf(L"%ls MEM_IMAGE=%d match=%d\n  held: %ls\n  mapped: %ls\n",
            label, region.Type == MEM_IMAGE, match, expected, actual);
    return match;
}

struct Rename {
    DWORD flags;
    HANDLE directory;
    DWORD name_bytes;
    WCHAR name[1024];
};

static void replacement(const wchar_t* from, const wchar_t* to) {
    HANDLE file = CreateFileW(from, DELETE | FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) fail(L"open replacement");
    struct Rename request = {0};
    request.flags = 3; // FILE_RENAME_REPLACE_IF_EXISTS | FILE_RENAME_POSIX_SEMANTICS.
    wcscpy_s(request.name, 1024, to);
    request.name_bytes = (DWORD)(wcslen(to) * sizeof(WCHAR));
    BOOL ok = SetFileInformationByHandle(file, FileRenameInfoEx, &request,
        (DWORD)(offsetof(struct Rename, name) + request.name_bytes));
    DWORD error = ok ? 0 : GetLastError();
    wprintf(L"POSIX-style replacement while file and parent pinned: success=%d error=%lu\n", ok, error);
    CloseHandle(file);
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 5) return 2;
    const wchar_t* mode = argv[1];
    HANDLE directory = hold(argv[2], 1);
    HANDLE main_file = hold(argv[3], 0);
    wchar_t dependency[1024], alternate[1024];
    swprintf_s(dependency, 1024, L"%ls\\void_g4_probe_dep.dll", argv[2]);
    swprintf_s(alternate, 1024, L"%ls\\void_g4_probe_dep.dll", argv[4]);
    HANDLE dep_file = hold(dependency, 0);
    HMODULE competitor = NULL;
    if (!wcscmp(mode, L"collision")) {
        competitor = LoadLibraryExW(alternate, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!competitor) fail(L"competing LoadLibraryExW");
    }
    if (!wcscmp(mode, L"replace")) replacement(alternate, dependency);
    HMODULE provider = LoadLibraryExW(argv[3], NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!provider) fail(L"provider LoadLibraryExW");
    Value value = (Value)GetProcAddress(provider, "g4_provider");
    DependencyAddress dependency_address = (DependencyAddress)GetProcAddress(provider, "g4_dependency_address");
    if (!value || !dependency_address) fail(L"GetProcAddress");
    const int main_matches = mapped(L"provider", (const void*)value, main_file);
    const int dep_matches = mapped(L"dependency", (const void*)dependency_address(), dep_file);
    const int result = value();
    wprintf(L"result=%d\n", result);
    FreeLibrary(provider);
    if (competitor) FreeLibrary(competitor);
    CloseHandle(dep_file);
    CloseHandle(main_file);
    CloseHandle(directory);
    if (!wcscmp(mode, L"collision")) return main_matches && !dep_matches && result == 134 ? 0 : 3;
    return main_matches && dep_matches && result == 42 ? 0 : 3;
}
