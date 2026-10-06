#include "../Library/.void/Void249PinInteropIntegration.c"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

static atomic_int gate_ready;
static atomic_int gate_release;

static int32_t block_pin_count;
static int32_t roundtrip_pin_before;
static int32_t roundtrip_pin_after;
static int32_t roundtrip_callback_result;
static int32_t two_buffer_pin_before;
static int32_t two_buffer_pin_after;
static int32_t observed_nested_pin_count;

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

void voidc249_reset(void)
{
    atomic_store(&gate_ready, 0);
    atomic_store(&gate_release, 0);
    block_pin_count = -1;
    roundtrip_pin_before = -1;
    roundtrip_pin_after = -1;
    roundtrip_callback_result = -1;
    two_buffer_pin_before = -1;
    two_buffer_pin_after = -1;
    observed_nested_pin_count = -1;
}

int32_t voidc249_ready(void)
{
    return atomic_load(&gate_ready);
}

void voidc249_release(void)
{
    atomic_store(&gate_release, 1);
}

int32_t voidc249_block_until_release(int32_t *values, int32_t length)
{
    block_pin_count = (int32_t)pin_count_for_data(values);
    atomic_store(&gate_ready, 1);
    while (atomic_load(&gate_release) == 0)
    {
        if (!vc_native_sleep_ms(1u))
            abort();
    }

    int32_t sum = 0;
    for (int32_t i = 0; i < length; i++)
    {
        sum += values[i];
        values[i] += 1;
    }
    return sum;
}

int32_t voidc249_roundtrip(
    int32_t *values,
    int32_t length,
    int32_t (*callback)(int32_t),
    int32_t tag)
{
    roundtrip_pin_before = (int32_t)pin_count_for_data(values);
    const int32_t callback_result = callback(tag);
    roundtrip_callback_result = callback_result;
    roundtrip_pin_after = (int32_t)pin_count_for_data(values);

    int32_t sum = 0;
    for (int32_t i = 0; i < length; i++)
    {
        sum += values[i];
        values[i] += 10;
    }
    return callback_result + sum;
}

int32_t voidc249_two_buffers(
    int32_t *writable,
    int32_t writable_length,
    const int32_t *readable,
    int32_t readable_length,
    int32_t (*callback)(int32_t),
    int32_t tag)
{
    two_buffer_pin_before = (int32_t)pin_count_for_data(writable);
    const int32_t callback_result = callback(tag);
    two_buffer_pin_after = (int32_t)pin_count_for_data(readable);

    int32_t sum = callback_result;
    for (int32_t i = 0; i < readable_length; i++)
        sum += readable[i];
    for (int32_t i = 0; i < writable_length; i++)
        writable[i] += 1;
    return sum;
}

int32_t voidc249_observe_pin(int32_t *values, int32_t length)
{
    (void)length;
    observed_nested_pin_count = (int32_t)pin_count_for_data(values);
    return observed_nested_pin_count;
}

int32_t voidc249_pin_count(int32_t *data)
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

    check(void249_concurrent_gc() == 1);
    check(block_pin_count == 1);
    check(all_pin_counts_zero());

    check(void249_callback_gc() == 1);
    check(roundtrip_pin_before == 1);
    check(roundtrip_pin_after == 1);
    check(roundtrip_callback_result == 41);
    check(all_pin_counts_zero());

    check(void249_callback_exception() == 1);
    check(roundtrip_pin_before == 1);
    check(roundtrip_pin_after == 1);
    check(roundtrip_callback_result == 0);
    check(all_pin_counts_zero());

    check(void249_fixed_nested_native() == 1);
    check(observed_nested_pin_count == 2);
    check(all_pin_counts_zero());

    check(void249_two_buffer_memory() == 1);
    check(two_buffer_pin_before == 2);
    check(two_buffer_pin_after == 2);
    check(all_pin_counts_zero());

    check(void249_ownerless_stack() == 1);
    check(roundtrip_pin_before == (int32_t)SIZE_MAX);
    check(roundtrip_pin_after == (int32_t)SIZE_MAX);
    check(all_pin_counts_zero());

    check(void249_post_failure_reuse() == 1);
    check(roundtrip_pin_before == 1);
    check(roundtrip_pin_after == 1);
    check(roundtrip_callback_result == 6);
    check(all_pin_counts_zero());

    vc_gc_collect();
    check(all_pin_counts_zero());

    vc_runtime_cleanup();
    check(vc_runtime_thread_detach(&context));
    vc_type_initializer_runtime_shutdown();
    vc_native_thread_runtime_shutdown();
    return 0;
}
