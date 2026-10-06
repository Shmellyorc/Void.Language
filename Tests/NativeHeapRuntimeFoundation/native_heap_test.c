#include "vc_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void check(bool value)
{
    puts(value ? "True" : "False");
}

int main(void)
{
    void *empty = (void *)(uintptr_t)1u;
    check(vc_native_memory_allocate(0u, &empty));
    check(empty == NULL);

    check(!vc_native_memory_allocate(1u, NULL));

    void *one = NULL;
    check(vc_native_memory_allocate(1u, &one));
    check(one != NULL);
    if (one != NULL)
        ((unsigned char *)one)[0] = 0x5au;
    check(one != NULL && ((unsigned char *)one)[0] == 0x5au);

    void *block = NULL;
    check(vc_native_memory_allocate(257u, &block));
    check(block != NULL);
    if (block != NULL)
    {
        unsigned char *bytes = block;
        for (size_t i = 0u; i < 257u; i++)
            bytes[i] = (unsigned char)(i & 0xffu);
    }
    bool pattern_ok = block != NULL;
    if (block != NULL)
    {
        const unsigned char *bytes = block;
        for (size_t i = 0u; i < 257u; i++)
            pattern_ok = pattern_ok && bytes[i] == (unsigned char)(i & 0xffu);
    }
    check(pattern_ok);
    check(one == NULL || block == NULL || one != block);

    vc_native_memory_free(NULL);
    check(true);

    vc_native_memory_free(one);
    vc_native_memory_free(block);
    check(true);

    bool repeated_ok = true;
    for (size_t i = 1u; i <= 64u; i++)
    {
        void *item = NULL;
        if (!vc_native_memory_allocate(i, &item) || item == NULL)
        {
            repeated_ok = false;
            break;
        }
        ((unsigned char *)item)[i - 1u] = (unsigned char)i;
        repeated_ok = repeated_ok && ((unsigned char *)item)[i - 1u] == (unsigned char)i;
        vc_native_memory_free(item);
    }
    check(repeated_ok);

    return 0;
}
