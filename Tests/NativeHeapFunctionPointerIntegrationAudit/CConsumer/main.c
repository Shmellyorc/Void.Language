#include "../Library/.void/Void260NativeHeapFunctionPointerAudit.c"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

static atomic_int gate_ready;
static atomic_int gate_release;
static int32_t after_callback;

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

void voidc260_audit_reset(void)
{
    atomic_store(&gate_ready, 0);
    atomic_store(&gate_release, 0);
    after_callback = 0;
}

int32_t voidc260_audit_ready(void)
{
    return atomic_load(&gate_ready);
}

void voidc260_audit_release(void)
{
    atomic_store(&gate_release, 1);
}

int32_t voidc260_audit_add(int32_t left, int32_t right)
{
    return left + right;
}

int32_t voidc260_audit_block_sum(int32_t *values, int32_t count)
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

int32_t voidc260_audit_drive(
    int32_t (*operation)(int32_t, int32_t),
    int32_t (*callback)(int32_t),
    int32_t tag)
{
    const int32_t function_result = operation == NULL ? -1000 : operation(tag, 22);
    const int32_t callback_result = callback == NULL ? -2000 : callback(tag);
    after_callback++;
    return function_result + callback_result;
}

int32_t voidc260_audit_gc_contains(void *memory)
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

int32_t voidc260_audit_after_callback(void)
{
    return after_callback;
}

int main(void)
{
    vc_library_init();
    VcRuntimeThreadContext context;
    check(vc_runtime_thread_attach(&context));
    check(void260_native_heap_thread_gc() == 1);
    check(void260_callback_exception_cleanup() == 1);
    check(vc_exception_in_flight == NULL);
    check(vc_native_pending_exception == NULL);
    check(!vc_native_pending_root_active);
    check(vc_native_call_depth == 0u);
    check(void260_post_exception_reuse() == 1);
    check(vc_exception_in_flight == NULL);
    check(vc_native_pending_exception == NULL);
    check(!vc_native_pending_root_active);
    check(vc_native_call_depth == 0u);
    vc_gc_collect();
    vc_runtime_cleanup();
    check(vc_runtime_thread_detach(&context));
    vc_type_initializer_runtime_shutdown();
    vc_native_thread_runtime_shutdown();
    return 0;
}
