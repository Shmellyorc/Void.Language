#include "../Library/.void/Void250UnmanagedMemoryNativeBufferAudit.c"

#include <stdint.h>
#include <stdio.h>

static int32_t audit_pin_before_writable;
static int32_t audit_pin_after_readable;
static int32_t audit_callback_result;
static int32_t observed_pin_count;

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

static size_t pin_count_for_data(const int32_t *data)
{
    const uintptr_t probe = (uintptr_t)data;
    size_t count = SIZE_MAX;

    vc_native_thread_runtime_heap_lock();
    for (VcGcAllocation *item = vc_gc_allocations; item != NULL; item = item->next)
    {
        if (item->trace != vc_array_trace)
            continue;
        VcArray *array = (VcArray *)item->memory;
        const uintptr_t begin = (uintptr_t)array->data;
        const size_t bytes = (size_t)array->length * array->element_size;
        const uintptr_t end = begin + bytes;
        if ((bytes == 0u && probe == begin) || (bytes != 0u && probe >= begin && probe < end))
        {
            count = item->pin_count;
            break;
        }
    }
    vc_native_thread_runtime_heap_unlock();
    return count;
}

static bool all_pin_counts_zero(void)
{
    bool zero = true;
    vc_native_thread_runtime_heap_lock();
    for (VcGcAllocation *item = vc_gc_allocations; item != NULL; item = item->next)
    {
        if (item->pin_count != 0u)
        {
            zero = false;
            break;
        }
    }
    vc_native_thread_runtime_heap_unlock();
    return zero;
}

static void reset_observations(void)
{
    audit_pin_before_writable = -1;
    audit_pin_after_readable = -1;
    audit_callback_result = -1;
    observed_pin_count = -1;
}

int32_t voidc250_audit_buffers(
    int32_t *writable,
    int32_t writable_length,
    const int32_t *readable,
    int32_t readable_length,
    int32_t (*callback)(int32_t),
    int32_t tag)
{
    audit_pin_before_writable = (int32_t)pin_count_for_data(writable);
    audit_callback_result = callback(tag);
    audit_pin_after_readable = (int32_t)pin_count_for_data(readable);

    int32_t sum = audit_callback_result;
    for (int32_t i = 0; i < readable_length; i++)
        sum += readable[i];
    for (int32_t i = 0; i < writable_length; i++)
        writable[i] += 1;
    return sum;
}

int32_t voidc250_observe_pin(int32_t *values, int32_t length)
{
    (void)length;
    observed_pin_count = (int32_t)pin_count_for_data(values);
    return observed_pin_count;
}

int32_t voidc250_pin_count(int32_t *data)
{
    const size_t count = pin_count_for_data(data);
    return count == SIZE_MAX ? -1 : (int32_t)count;
}

int main(void)
{
    vc_library_init();

    VcRuntimeThreadContext context;
    check(vc_runtime_thread_attach(&context));
    check(all_pin_counts_zero());

    reset_observations();
    check(void250_managed_memory_native() == 1);
    check(audit_pin_before_writable == 2);
    check(audit_pin_after_readable == 2);
    check(audit_callback_result == 51);
    check(all_pin_counts_zero());

    reset_observations();
    check(void250_nested_array_fixed_native() == 1);
    check(observed_pin_count == 2);
    check(all_pin_counts_zero());

    reset_observations();
    check(void250_callback_exception_thread() == 1);
    check(audit_pin_before_writable == 2);
    check(audit_pin_after_readable == 2);
    check(audit_callback_result == 0);
    check(all_pin_counts_zero());

    reset_observations();
    check(void250_ownerless_native() == 1);
    check(audit_pin_before_writable == (int32_t)SIZE_MAX);
    check(audit_pin_after_readable == (int32_t)SIZE_MAX);
    check(audit_callback_result == 11);
    check(all_pin_counts_zero());

    reset_observations();
    check(void250_readonly_separate() == 1);
    check(audit_pin_before_writable == 1);
    check(audit_pin_after_readable == 1);
    check(audit_callback_result == 21);
    check(all_pin_counts_zero());

    vc_gc_collect();
    check(all_pin_counts_zero());

    vc_runtime_cleanup();
    check(vc_runtime_thread_detach(&context));
    vc_type_initializer_runtime_shutdown();
    vc_native_thread_runtime_shutdown();
    return 0;
}
