#define WIN32_LEAN_AND_MEAN
#include <windows.h>

__declspec(dllexport) int g4_dependency(void) { return 99; }

BOOL WINAPI sentinel_entry(HINSTANCE image, DWORD reason, LPVOID reserved) {
    (void)image;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) ExitProcess(W2_SENTINEL);
    return TRUE;
}
