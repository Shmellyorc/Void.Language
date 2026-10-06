#ifndef VC_MEMORY_H
#define VC_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * VOID native heap contract:
 * - allocation success/failure is reported separately from the pointer value;
 * - allocating zero bytes succeeds and produces the canonical empty value NULL;
 * - a failed nonzero allocation produces NULL and never transfers ownership;
 * - freeing NULL is always a no-op;
 * - nonzero reallocation failure leaves the original allocation unchanged;
 * - reallocation to zero bytes succeeds, frees the original allocation, and
 *   produces the canonical empty value NULL;
 * - explicit alignment is VOID-defined: alignment must be a nonzero power of
 *   two, zero-size aligned allocation still validates alignment, and backend
 *   library size/alignment quirks are not part of the public contract.
 */
bool vc_native_memory_allocate(size_t size, void **memory);
bool vc_native_memory_reallocate(void **memory, size_t size);
bool vc_native_memory_allocate_aligned(size_t size, size_t alignment, void **memory);
bool vc_native_memory_reallocate_aligned(void **memory, size_t size, size_t alignment);
void vc_native_memory_free(void *memory);

#ifdef __cplusplus
}
#endif

#endif
