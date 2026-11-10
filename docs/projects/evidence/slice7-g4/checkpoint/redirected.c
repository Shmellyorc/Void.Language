#include <windows.h>
__declspec(dllexport) volatile int g4_init_marker;
BOOL WINAPI checkpoint_attach(HINSTANCE module, DWORD reason, LPVOID reserved) {
    (void)module;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) g4_init_marker = 1;
    return TRUE;
}
