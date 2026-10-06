#include "vc_atomic.h"

#include <stdbool.h>
#include <stdlib.h>

static bool vc_atomic_order_valid(VcNativeMemoryOrder order)
{
    return order >= VC_NATIVE_MEMORY_ORDER_RELAXED && order <= VC_NATIVE_MEMORY_ORDER_SEQ_CST;
}

static void vc_atomic_require_location(const void *location)
{
    if (location == NULL)
        abort();
}

static void vc_atomic_require_order(VcNativeMemoryOrder order)
{
    if (!vc_atomic_order_valid(order))
        abort();
}

static void vc_atomic_require_load_order(VcNativeMemoryOrder order)
{
    vc_atomic_require_order(order);
    if (order == VC_NATIVE_MEMORY_ORDER_RELEASE || order == VC_NATIVE_MEMORY_ORDER_ACQ_REL)
        abort();
}

static void vc_atomic_require_store_order(VcNativeMemoryOrder order)
{
    vc_atomic_require_order(order);
    if (order == VC_NATIVE_MEMORY_ORDER_ACQUIRE || order == VC_NATIVE_MEMORY_ORDER_ACQ_REL)
        abort();
}

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static void vc_atomic_windows_order(VcNativeMemoryOrder order)
{
    vc_atomic_require_order(order);
}

int32_t vc_native_atomic_i32_load(const int32_t *location, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_load_order(order);
    return (int32_t)InterlockedCompareExchange((volatile LONG *)(uintptr_t)location, 0, 0);
}

void vc_native_atomic_i32_store(int32_t *location, int32_t value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_store_order(order);
    (void)InterlockedExchange((volatile LONG *)location, (LONG)value);
}

int32_t vc_native_atomic_i32_exchange(int32_t *location, int32_t value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return (int32_t)InterlockedExchange((volatile LONG *)location, (LONG)value);
}

int32_t vc_native_atomic_i32_compare_exchange(
    int32_t *location,
    int32_t value,
    int32_t comparand,
    VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return (int32_t)InterlockedCompareExchange(
        (volatile LONG *)location,
        (LONG)value,
        (LONG)comparand);
}

int32_t vc_native_atomic_i32_fetch_add(int32_t *location, int32_t value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return (int32_t)InterlockedExchangeAdd((volatile LONG *)location, (LONG)value);
}

int64_t vc_native_atomic_i64_load(const int64_t *location, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_load_order(order);
    return (int64_t)InterlockedCompareExchange64((volatile LONG64 *)(uintptr_t)location, 0, 0);
}

void vc_native_atomic_i64_store(int64_t *location, int64_t value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_store_order(order);
    (void)InterlockedExchange64((volatile LONG64 *)location, (LONG64)value);
}

int64_t vc_native_atomic_i64_exchange(int64_t *location, int64_t value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return (int64_t)InterlockedExchange64((volatile LONG64 *)location, (LONG64)value);
}

int64_t vc_native_atomic_i64_compare_exchange(
    int64_t *location,
    int64_t value,
    int64_t comparand,
    VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return (int64_t)InterlockedCompareExchange64(
        (volatile LONG64 *)location,
        (LONG64)value,
        (LONG64)comparand);
}

int64_t vc_native_atomic_i64_fetch_add(int64_t *location, int64_t value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return (int64_t)InterlockedExchangeAdd64((volatile LONG64 *)location, (LONG64)value);
}

intptr_t vc_native_atomic_iptr_load(const intptr_t *location, VcNativeMemoryOrder order)
{
#if defined(_WIN64)
    return (intptr_t)vc_native_atomic_i64_load((const int64_t *)location, order);
#else
    return (intptr_t)vc_native_atomic_i32_load((const int32_t *)location, order);
#endif
}

void vc_native_atomic_iptr_store(intptr_t *location, intptr_t value, VcNativeMemoryOrder order)
{
#if defined(_WIN64)
    vc_native_atomic_i64_store((int64_t *)location, (int64_t)value, order);
#else
    vc_native_atomic_i32_store((int32_t *)location, (int32_t)value, order);
#endif
}

intptr_t vc_native_atomic_iptr_exchange(intptr_t *location, intptr_t value, VcNativeMemoryOrder order)
{
#if defined(_WIN64)
    return (intptr_t)vc_native_atomic_i64_exchange((int64_t *)location, (int64_t)value, order);
#else
    return (intptr_t)vc_native_atomic_i32_exchange((int32_t *)location, (int32_t)value, order);
#endif
}

intptr_t vc_native_atomic_iptr_compare_exchange(
    intptr_t *location,
    intptr_t value,
    intptr_t comparand,
    VcNativeMemoryOrder order)
{
#if defined(_WIN64)
    return (intptr_t)vc_native_atomic_i64_compare_exchange(
        (int64_t *)location,
        (int64_t)value,
        (int64_t)comparand,
        order);
#else
    return (intptr_t)vc_native_atomic_i32_compare_exchange(
        (int32_t *)location,
        (int32_t)value,
        (int32_t)comparand,
        order);
#endif
}

intptr_t vc_native_atomic_iptr_fetch_add(intptr_t *location, intptr_t value, VcNativeMemoryOrder order)
{
#if defined(_WIN64)
    return (intptr_t)vc_native_atomic_i64_fetch_add((int64_t *)location, (int64_t)value, order);
#else
    return (intptr_t)vc_native_atomic_i32_fetch_add((int32_t *)location, (int32_t)value, order);
#endif
}

void *vc_native_atomic_ptr_load(void *const *location, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_load_order(order);
    return InterlockedCompareExchangePointer((void *volatile *)(uintptr_t)location, NULL, NULL);
}

void vc_native_atomic_ptr_store(void **location, void *value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_store_order(order);
    (void)InterlockedExchangePointer((void *volatile *)location, value);
}

void *vc_native_atomic_ptr_exchange(void **location, void *value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return InterlockedExchangePointer((void *volatile *)location, value);
}

void *vc_native_atomic_ptr_compare_exchange(
    void **location,
    void *value,
    void *comparand,
    VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_windows_order(order);
    return InterlockedCompareExchangePointer((void *volatile *)location, value, comparand);
}

#elif defined(__GNUC__) || defined(__clang__)

static int vc_atomic_gnu_order(VcNativeMemoryOrder order)
{
    vc_atomic_require_order(order);
    switch (order)
    {
        case VC_NATIVE_MEMORY_ORDER_RELAXED: return __ATOMIC_RELAXED;
        case VC_NATIVE_MEMORY_ORDER_ACQUIRE: return __ATOMIC_ACQUIRE;
        case VC_NATIVE_MEMORY_ORDER_RELEASE: return __ATOMIC_RELEASE;
        case VC_NATIVE_MEMORY_ORDER_ACQ_REL: return __ATOMIC_ACQ_REL;
        case VC_NATIVE_MEMORY_ORDER_SEQ_CST: return __ATOMIC_SEQ_CST;
    }
    abort();
}

static int vc_atomic_gnu_failure_order(VcNativeMemoryOrder order)
{
    switch (order)
    {
        case VC_NATIVE_MEMORY_ORDER_RELAXED:
        case VC_NATIVE_MEMORY_ORDER_RELEASE:
            return __ATOMIC_RELAXED;
        case VC_NATIVE_MEMORY_ORDER_ACQUIRE:
        case VC_NATIVE_MEMORY_ORDER_ACQ_REL:
            return __ATOMIC_ACQUIRE;
        case VC_NATIVE_MEMORY_ORDER_SEQ_CST:
            return __ATOMIC_SEQ_CST;
    }
    abort();
}

#define VC_DEFINE_GNU_INTEGER_ATOMICS(SUFFIX, TYPE) \
TYPE vc_native_atomic_##SUFFIX##_load(const TYPE *location, VcNativeMemoryOrder order) \
{ \
    vc_atomic_require_location(location); \
    vc_atomic_require_load_order(order); \
    return __atomic_load_n(location, vc_atomic_gnu_order(order)); \
} \
void vc_native_atomic_##SUFFIX##_store(TYPE *location, TYPE value, VcNativeMemoryOrder order) \
{ \
    vc_atomic_require_location(location); \
    vc_atomic_require_store_order(order); \
    __atomic_store_n(location, value, vc_atomic_gnu_order(order)); \
} \
TYPE vc_native_atomic_##SUFFIX##_exchange(TYPE *location, TYPE value, VcNativeMemoryOrder order) \
{ \
    vc_atomic_require_location(location); \
    return __atomic_exchange_n(location, value, vc_atomic_gnu_order(order)); \
} \
TYPE vc_native_atomic_##SUFFIX##_compare_exchange( \
    TYPE *location, TYPE value, TYPE comparand, VcNativeMemoryOrder order) \
{ \
    vc_atomic_require_location(location); \
    TYPE expected = comparand; \
    (void)__atomic_compare_exchange_n( \
        location, &expected, value, false, \
        vc_atomic_gnu_order(order), vc_atomic_gnu_failure_order(order)); \
    return expected; \
} \
TYPE vc_native_atomic_##SUFFIX##_fetch_add(TYPE *location, TYPE value, VcNativeMemoryOrder order) \
{ \
    vc_atomic_require_location(location); \
    return __atomic_fetch_add(location, value, vc_atomic_gnu_order(order)); \
}

VC_DEFINE_GNU_INTEGER_ATOMICS(i32, int32_t)
VC_DEFINE_GNU_INTEGER_ATOMICS(i64, int64_t)
VC_DEFINE_GNU_INTEGER_ATOMICS(iptr, intptr_t)

void *vc_native_atomic_ptr_load(void *const *location, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_load_order(order);
    return __atomic_load_n(location, vc_atomic_gnu_order(order));
}

void vc_native_atomic_ptr_store(void **location, void *value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    vc_atomic_require_store_order(order);
    __atomic_store_n(location, value, vc_atomic_gnu_order(order));
}

void *vc_native_atomic_ptr_exchange(void **location, void *value, VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    return __atomic_exchange_n(location, value, vc_atomic_gnu_order(order));
}

void *vc_native_atomic_ptr_compare_exchange(
    void **location,
    void *value,
    void *comparand,
    VcNativeMemoryOrder order)
{
    vc_atomic_require_location(location);
    void *expected = comparand;
    (void)__atomic_compare_exchange_n(
        location,
        &expected,
        value,
        false,
        vc_atomic_gnu_order(order),
        vc_atomic_gnu_failure_order(order));
    return expected;
}

#else
#error "VOID atomic runtime requires Win32 Interlocked or GCC/Clang __atomic support"
#endif

int32_t vc_native_atomic_i32_add(int32_t *location, int32_t value, VcNativeMemoryOrder order)
{
    const int32_t previous = vc_native_atomic_i32_fetch_add(location, value, order);
    return (int32_t)((uint32_t)previous + (uint32_t)value);
}

int64_t vc_native_atomic_i64_add(int64_t *location, int64_t value, VcNativeMemoryOrder order)
{
    const int64_t previous = vc_native_atomic_i64_fetch_add(location, value, order);
    return (int64_t)((uint64_t)previous + (uint64_t)value);
}
