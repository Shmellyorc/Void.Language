#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(dllimport) int g4_dependency(void);

int main(void) {
    // This is the first observation in the child. Its static application
    // import has already been resolved and DLL_PROCESS_ATTACH has run.
    HMODULE dependency;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)g4_dependency, &dependency)) return 2;
    const volatile int* marker = (const volatile int*)GetProcAddress(dependency, "g4_init_marker");
    if (marker && *marker == 1) return 121;
    return g4_dependency() == 7 ? 42 : 3;
}
