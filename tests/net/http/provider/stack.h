#include <stdio.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
static int provider_stack(void) {
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)GetModuleHandleW(NULL);
    if(!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)return 1;
    const IMAGE_NT_HEADERS* pe = (const IMAGE_NT_HEADERS*)((const unsigned char*)dos + dos->e_lfanew);
    if(pe->Signature != IMAGE_NT_SIGNATURE)return 1;
    printf("{\"stack_soft\":%llu,\"stack_hard\":%llu}\n",
        (unsigned long long)pe->OptionalHeader.SizeOfStackReserve,
        (unsigned long long)pe->OptionalHeader.SizeOfStackReserve);
    return 0;
}
#else
#include <sys/resource.h>
static int provider_stack(void) {
    struct rlimit stack;
    if(getrlimit(RLIMIT_STACK,&stack))return 1;
    printf("{\"stack_soft\":%llu,\"stack_hard\":%llu}\n",
        (unsigned long long)stack.rlim_cur,(unsigned long long)stack.rlim_max);
    return 0;
}
#endif
