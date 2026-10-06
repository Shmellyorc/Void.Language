#include "vc_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check(int condition)
{
    puts(condition ? "True" : "False");
}

static int aligned_to(const void *memory, size_t alignment)
{
    return memory != NULL && ((uintptr_t)memory % alignment) == 0u;
}

int main(void)
{
    void *memory = (void *)(uintptr_t)1u;
    check(vc_native_memory_allocate(0u, &memory) && memory == NULL);
    check(vc_native_memory_allocate_aligned(0u, 64u, &memory) && memory == NULL);
    check(!vc_native_memory_allocate_aligned(0u, 3u, &memory) && memory == NULL);

    check(vc_native_memory_allocate(16u, &memory) && memory != NULL);
    memset(memory, 0x5a, 16u);
    void *original = memory;
    check(!vc_native_memory_reallocate(&memory, SIZE_MAX));
    check(memory == original && ((unsigned char *)memory)[0] == 0x5a);
    check(vc_native_memory_reallocate(&memory, 32u) && memory != NULL);
    check(((unsigned char *)memory)[0] == 0x5a && ((unsigned char *)memory)[15] == 0x5a);
    check(vc_native_memory_reallocate(&memory, 0u) && memory == NULL);

    check(vc_native_memory_allocate_aligned(33u, 64u, &memory) && aligned_to(memory, 64u));
    memset(memory, 0x33, 33u);
    original = memory;
    check(!vc_native_memory_reallocate_aligned(&memory, 48u, 6u));
    check(memory == original && ((unsigned char *)memory)[0] == 0x33);
    check(vc_native_memory_reallocate_aligned(&memory, 80u, 128u) && aligned_to(memory, 128u));
    check(((unsigned char *)memory)[0] == 0x33 && ((unsigned char *)memory)[32] == 0x33);
    check(vc_native_memory_reallocate_aligned(&memory, 0u, 128u) && memory == NULL);
    vc_native_memory_free(NULL);
    check(1);
    return 0;
}
