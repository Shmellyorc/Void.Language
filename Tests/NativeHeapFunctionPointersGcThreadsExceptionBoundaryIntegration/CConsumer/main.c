#include "../Library/.void/Void259NativeHeapFunctionPointerIntegration.c"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

static atomic_int gate_ready;
static atomic_int gate_release;
static int32_t drive_function_result;
static int32_t drive_callback_result;
static int32_t drive_after_callback;

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

void voidc259_reset(void)
{
    atomic_store(&gate_ready, 0);
    atomic_store(&gate_release, 0);
    drive_function_result = -1;
    drive_callback_result = -1;
    drive_after_callback = 0;
}

int32_t voidc259_ready(void)
{
    return atomic_load(&gate_ready);
}

void voidc259_release(void)
{
    atomic_store(&gate_release, 1);
}

int32_t voidc259_add(int32_t left, int32_t right)
{
    return left + right;
}

int32_t voidc259_subtract(int32_t left, int32_t right)
{
    return left - right;
}

int32_t voidc259_block_sum(int32_t *values, int32_t count)
{
    atomic_store(&gate_ready, 1);
    while (atomic_load(&gate_release) == 0)
    {
        if (!vc_native_sleep_ms(1u))
            abort();
    }

    int32_t sum = 0;
    for (int32_t i = 0; i < count; i++)
        sum += values[i];
    return sum;
}

int32_t voidc259_drive(
    int32_t (*operation)(int32_t, int32_t),
    int32_t (*callback)(int32_t),
    int32_t tag)
{
    drive_function_result = operation == NULL ? -1000 : operation(tag, 2);
    drive_callback_result = callback == NULL ? -2000 : callback(tag);
    drive_after_callback++;
    return drive_function_result + drive_callback_result;
}

int32_t voidc259_gc_contains(void *memory)
{
    int32_t found = 0;
    vc_native_thread_runtime_heap_lock();
    for (VcGcAllocation *item = vc_gc_allocations; item != NULL; item = item->next)
    {
        if (item->memory == memory)
        {
            found = 1;
            break;
        }
    }
    vc_native_thread_runtime_heap_unlock();
    return found;
}

int32_t voidc259_drive_function_result(void)
{
    return drive_function_result;
}

int32_t voidc259_drive_callback_result(void)
{
    return drive_callback_result;
}

int32_t voidc259_drive_after_callback(void)
{
    return drive_after_callback;
}

int main(void)
{
    vc_library_init();

    VcRuntimeThreadContext context;
    check(vc_runtime_thread_attach(&context));

    check(void259_threaded_native_gc() == 1);
    check(void259_callback_gc() == 1);
    check(void259_callback_exception_cleanup() == 1);
    check(void259_thread_exception_cleanup() == 1);
    check(void259_post_failure_reuse() == 1);

    vc_gc_collect();
    check(vc_exception_in_flight == NULL);
    check(vc_native_pending_exception == NULL);
    check(!vc_native_pending_root_active);
    check(vc_native_call_depth == 0u);

    vc_runtime_cleanup();
    check(vc_runtime_thread_detach(&context));
    vc_type_initializer_runtime_shutdown();
    vc_native_thread_runtime_shutdown();
    return 0;
}
