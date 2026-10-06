#ifndef VC_ATOMIC_H
#define VC_ATOMIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VcNativeMemoryOrder
{
    VC_NATIVE_MEMORY_ORDER_RELAXED = 0,
    VC_NATIVE_MEMORY_ORDER_ACQUIRE = 1,
    VC_NATIVE_MEMORY_ORDER_RELEASE = 2,
    VC_NATIVE_MEMORY_ORDER_ACQ_REL = 3,
    VC_NATIVE_MEMORY_ORDER_SEQ_CST = 4
} VcNativeMemoryOrder;

int32_t vc_native_atomic_i32_load(const int32_t *location, VcNativeMemoryOrder order);
void vc_native_atomic_i32_store(int32_t *location, int32_t value, VcNativeMemoryOrder order);
int32_t vc_native_atomic_i32_exchange(int32_t *location, int32_t value, VcNativeMemoryOrder order);
int32_t vc_native_atomic_i32_compare_exchange(
    int32_t *location,
    int32_t value,
    int32_t comparand,
    VcNativeMemoryOrder order);
int32_t vc_native_atomic_i32_fetch_add(int32_t *location, int32_t value, VcNativeMemoryOrder order);
int32_t vc_native_atomic_i32_add(int32_t *location, int32_t value, VcNativeMemoryOrder order);

int64_t vc_native_atomic_i64_load(const int64_t *location, VcNativeMemoryOrder order);
void vc_native_atomic_i64_store(int64_t *location, int64_t value, VcNativeMemoryOrder order);
int64_t vc_native_atomic_i64_exchange(int64_t *location, int64_t value, VcNativeMemoryOrder order);
int64_t vc_native_atomic_i64_compare_exchange(
    int64_t *location,
    int64_t value,
    int64_t comparand,
    VcNativeMemoryOrder order);
int64_t vc_native_atomic_i64_fetch_add(int64_t *location, int64_t value, VcNativeMemoryOrder order);
int64_t vc_native_atomic_i64_add(int64_t *location, int64_t value, VcNativeMemoryOrder order);

intptr_t vc_native_atomic_iptr_load(const intptr_t *location, VcNativeMemoryOrder order);
void vc_native_atomic_iptr_store(intptr_t *location, intptr_t value, VcNativeMemoryOrder order);
intptr_t vc_native_atomic_iptr_exchange(intptr_t *location, intptr_t value, VcNativeMemoryOrder order);
intptr_t vc_native_atomic_iptr_compare_exchange(
    intptr_t *location,
    intptr_t value,
    intptr_t comparand,
    VcNativeMemoryOrder order);
intptr_t vc_native_atomic_iptr_fetch_add(intptr_t *location, intptr_t value, VcNativeMemoryOrder order);

void *vc_native_atomic_ptr_load(void *const *location, VcNativeMemoryOrder order);
void vc_native_atomic_ptr_store(void **location, void *value, VcNativeMemoryOrder order);
void *vc_native_atomic_ptr_exchange(void **location, void *value, VcNativeMemoryOrder order);
void *vc_native_atomic_ptr_compare_exchange(
    void **location,
    void *value,
    void *comparand,
    VcNativeMemoryOrder order);

#ifdef __cplusplus
}
#endif

#endif
