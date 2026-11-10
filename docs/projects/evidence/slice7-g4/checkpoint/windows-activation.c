#define wmain checkpoint_map
#include "windows-redirection.c"
#undef wmain

int wmain(int argc, wchar_t** argv) {
    if (argc != 5 && argc != 6) return 2;
    const wchar_t* name = L"void_g4_probe_main.dll";
    HMODULE existing = GetModuleHandleW(name);
    wprintf(L"provider loaded before activation=%d\n", existing != NULL);
    if (existing) return 1;
    ACTCTXW options = {0};
    options.cbSize = sizeof options;
    options.lpSource = argv[4];
    HANDLE context = CreateActCtxW(&options);
    ULONG_PTR cookie;
    if (context == INVALID_HANDLE_VALUE || !ActivateActCtx(context, &cookie)) {
        wprintf(L"activation failed: %lu\n", GetLastError());
        return 1;
    }
    ULONG_PTR guard;
    if (argc == 6) {
        if (!ActivateActCtx(NULL, &guard)) {
            wprintf(L"default activation failed: %lu\n", GetLastError());
            return 1;
        }
        wprintf(L"pushed default activation context\n");
    }
    int result = checkpoint_map(4, argv);
    if (argc == 6 && !DeactivateActCtx(0, guard)) return 1;
    if (!DeactivateActCtx(0, cookie)) return 1;
    ReleaseActCtx(context);
    return result;
}
