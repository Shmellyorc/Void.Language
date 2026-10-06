#include "vc_memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct VcNativeMemoryHeader
{
    void *base;
    size_t size;
    size_t alignment;
} VcNativeMemoryHeader;

static bool vc_native_memory_alignment_valid(size_t alignment)
{
    return alignment != 0u && (alignment & (alignment - 1u)) == 0u;
}

static size_t vc_native_memory_default_alignment(void)
{
    return _Alignof(max_align_t);
}

static size_t vc_native_memory_effective_alignment(size_t alignment)
{
    const size_t minimum = _Alignof(VcNativeMemoryHeader);
    if (alignment < minimum)
        return minimum;
    return alignment;
}

static VcNativeMemoryHeader *vc_native_memory_header(void *memory)
{
    return (VcNativeMemoryHeader *)((unsigned char *)memory - sizeof(VcNativeMemoryHeader));
}

static bool vc_native_memory_allocate_impl(size_t size, size_t alignment, void **memory)
{
    if (memory == NULL)
        return false;

    *memory = NULL;
    if (!vc_native_memory_alignment_valid(alignment))
        return false;
    if (size == 0u)
        return true;

    const size_t effective_alignment = vc_native_memory_effective_alignment(alignment);
    const size_t header_size = sizeof(VcNativeMemoryHeader);
    if (effective_alignment - 1u > SIZE_MAX - header_size)
        return false;
    const size_t overhead = header_size + effective_alignment - 1u;
    if (size > SIZE_MAX - overhead)
        return false;

    void *base = malloc(size + overhead);
    if (base == NULL)
        return false;

    unsigned char *start = (unsigned char *)base + header_size;
    const uintptr_t address = (uintptr_t)start;
    const size_t mask = effective_alignment - 1u;
    const size_t adjustment = (size_t)((effective_alignment - (address & mask)) & mask);
    unsigned char *payload = start + adjustment;
    VcNativeMemoryHeader *header = (VcNativeMemoryHeader *)(payload - header_size);
    header->base = base;
    header->size = size;
    header->alignment = effective_alignment;
    *memory = payload;
    return true;
}

bool vc_native_memory_allocate(size_t size, void **memory)
{
    return vc_native_memory_allocate_impl(size, vc_native_memory_default_alignment(), memory);
}

bool vc_native_memory_allocate_aligned(size_t size, size_t alignment, void **memory)
{
    return vc_native_memory_allocate_impl(size, alignment, memory);
}

static bool vc_native_memory_reallocate_impl(void **memory, size_t size, size_t alignment)
{
    if (memory == NULL)
        return false;
    if (!vc_native_memory_alignment_valid(alignment))
        return false;

    void *original = *memory;
    if (size == 0u)
    {
        vc_native_memory_free(original);
        *memory = NULL;
        return true;
    }
    if (original == NULL)
        return vc_native_memory_allocate_impl(size, alignment, memory);

    VcNativeMemoryHeader *original_header = vc_native_memory_header(original);
    void *replacement = NULL;
    if (!vc_native_memory_allocate_impl(size, alignment, &replacement))
        return false;

    const size_t copy_size = original_header->size < size ? original_header->size : size;
    if (copy_size != 0u)
        memcpy(replacement, original, copy_size);
    vc_native_memory_free(original);
    *memory = replacement;
    return true;
}

bool vc_native_memory_reallocate(void **memory, size_t size)
{
    if (memory == NULL)
        return false;
    if (*memory == NULL)
        return vc_native_memory_reallocate_impl(memory, size, vc_native_memory_default_alignment());

    const size_t alignment = vc_native_memory_header(*memory)->alignment;
    return vc_native_memory_reallocate_impl(memory, size, alignment);
}

bool vc_native_memory_reallocate_aligned(void **memory, size_t size, size_t alignment)
{
    return vc_native_memory_reallocate_impl(memory, size, alignment);
}

void vc_native_memory_free(void *memory)
{
    if (memory == NULL)
        return;
    VcNativeMemoryHeader *header = vc_native_memory_header(memory);
    free(header->base);
}
