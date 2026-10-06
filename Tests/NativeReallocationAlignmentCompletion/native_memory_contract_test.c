#include "vc_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int aligned_to(const void *memory, size_t alignment)
{
    return memory != NULL && ((uintptr_t)memory % alignment) == 0u;
}

int main(void)
{
    void *memory = NULL;
    if (!vc_native_memory_allocate(16u, &memory) || memory == NULL)
        return 1;
    memset(memory, 0x5a, 16u);

    void *original = memory;
    if (vc_native_memory_reallocate(&memory, SIZE_MAX))
        return 2;
    if (memory != original || ((unsigned char *)memory)[0] != 0x5a)
        return 3;

    if (!vc_native_memory_reallocate(&memory, 32u) || memory == NULL)
        return 4;
    for (size_t i = 0u; i < 16u; i++)
        if (((unsigned char *)memory)[i] != 0x5a)
            return 5;

    if (!vc_native_memory_reallocate(&memory, 0u) || memory != NULL)
        return 6;
    if (!vc_native_memory_reallocate(&memory, 8u) || memory == NULL)
        return 7;
    vc_native_memory_free(memory);

    const size_t alignments[] = {1u, 8u, 16u, 64u, 256u};
    for (size_t i = 0u; i < sizeof(alignments) / sizeof(alignments[0]); i++)
    {
        memory = NULL;
        if (!vc_native_memory_allocate_aligned(33u, alignments[i], &memory))
            return 8;
        if (!aligned_to(memory, alignments[i]))
            return 9;
        vc_native_memory_free(memory);
    }

    memory = (void *)(uintptr_t)1u;
    if (!vc_native_memory_allocate_aligned(0u, 64u, &memory) || memory != NULL)
        return 10;
    if (vc_native_memory_allocate_aligned(0u, 3u, &memory) || memory != NULL)
        return 11;

    if (!vc_native_memory_allocate_aligned(24u, 64u, &memory) || !aligned_to(memory, 64u))
        return 12;
    memset(memory, 0x33, 24u);
    original = memory;
    if (vc_native_memory_reallocate_aligned(&memory, 48u, 3u))
        return 13;
    if (memory != original || ((unsigned char *)memory)[0] != 0x33)
        return 14;
    if (!vc_native_memory_reallocate_aligned(&memory, 48u, 128u) || !aligned_to(memory, 128u))
        return 15;
    for (size_t i = 0u; i < 24u; i++)
        if (((unsigned char *)memory)[i] != 0x33)
            return 16;
    if (!vc_native_memory_reallocate_aligned(&memory, 0u, 128u) || memory != NULL)
        return 17;

    if (vc_native_memory_allocate_aligned(8u, 0u, &memory))
        return 18;
    if (vc_native_memory_allocate_aligned(8u, 6u, &memory))
        return 19;
    if (vc_native_memory_reallocate(NULL, 8u))
        return 20;
    if (vc_native_memory_reallocate_aligned(NULL, 8u, 16u))
        return 21;

    puts("True");
    puts("True");
    puts("True");
    puts("True");
    puts("True");
    puts("True");
    puts("True");
    puts("True");
    return 0;
}
