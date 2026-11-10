#define wmain earlier_mapping_probe
#include "windows-mapping.c"
#undef wmain

typedef LONG (NTAPI *CompareMapping)(void*, void*);
static LONG compare(void* image, HANDLE file) {
    CompareMapping same = (CompareMapping)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtAreMappedFilesTheSame");
    if (!same) fail(L"NtAreMappedFilesTheSame export");
    HANDLE section = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!section) fail(L"CreateFileMappingW");
    void* data = MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0);
    if (!data) fail(L"MapViewOfFile");
    LONG result = same(image, data);
    UnmapViewOfFile(data);
    CloseHandle(section);
    return result;
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 5 && argc != 6) return 2;
    const wchar_t* name = L"void_g4_probe_dep.dll";
    HANDLE directory = hold(argv[1], 1);
    HANDLE main_file = hold(argv[2], 0);
    HANDLE dep_file = hold(argv[3], 0);
    if (GetModuleHandleW(name)) return 2;
    wprintf(L"dependency loaded before activation=0\n");
    ACTCTXW options = {0};
    options.cbSize = sizeof options;
    options.lpSource = argv[4];
    HANDLE context = CreateActCtxW(&options);
    ULONG_PTR cookie, guard;
    if (context == INVALID_HANDLE_VALUE || !ActivateActCtx(context, &cookie)) fail(L"activation");
    if (argc == 6 && !ActivateActCtx(NULL, &guard)) fail(L"default activation");
    ACTCTX_SECTION_KEYED_DATA redirect = {0};
    redirect.cbSize = sizeof redirect;
    BOOL listed = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, name, &redirect);
    wprintf(L"active context redirects dependency=%d\n", listed);
    HMODULE main = LoadLibraryExW(argv[2], NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!main) fail(L"provider LoadLibraryExW");
    DependencyAddress address = (DependencyAddress)GetProcAddress(main, "g4_dependency_address");
    if (!address) fail(L"GetProcAddress");
    HMODULE dep;
    void* actual = (void*)address();
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)actual, &dep)) fail(L"owner of resolved import");
    const volatile int* marker = (const volatile int*)GetProcAddress(dep, "g4_init_marker");
    int dep_matches = mapped(L"dependency", actual, dep_file);
    LONG main_id = compare(main, main_file), dep_id = compare(dep, dep_file);
    wprintf(L"initializer-marker=%d kernel-oracle-provider=%08lx kernel-oracle-dependency=%08lx\n",
            marker ? *marker : -1, (unsigned long)main_id, (unsigned long)dep_id);
    FreeLibrary(main);
    if (argc == 6 && !DeactivateActCtx(0, guard)) fail(L"deactivate default");
    if (!DeactivateActCtx(0, cookie)) fail(L"deactivate override");
    ReleaseActCtx(context);
    CloseHandle(dep_file);
    CloseHandle(main_file);
    CloseHandle(directory);
    return main_id != 0 || (argc == 6 ? (!dep_matches || dep_id != 0 || marker != NULL || listed) :
                             (dep_matches || dep_id == 0 || !marker || !listed));
}
