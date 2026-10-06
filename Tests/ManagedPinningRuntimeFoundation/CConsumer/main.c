#include "../Library/.void/Void243PinLib.c"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct PinProbeOwner
{
    void *child;
    int32_t value;
} PinProbeOwner;

static void trace_probe_owner(void *memory)
{
    PinProbeOwner *owner = (PinProbeOwner *)memory;
    vc_gc_mark(owner->child);
}

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

static size_t pin_count_for(void *owner)
{
    size_t count = SIZE_MAX;
    vc_native_thread_runtime_heap_lock();
    for (VcGcAllocation *item = vc_gc_allocations; item != NULL; item = item->next)
    {
        if (item->memory != owner)
            continue;
        count = item->pin_count;
        break;
    }
    vc_native_thread_runtime_heap_unlock();
    return count;
}

int main(void)
{
    vc_library_init();

    VcRuntimeThreadContext context;
    check(vc_runtime_thread_attach(&context));
    check(void243_ping() == 243);
    vc_gc_safepoint();

    const size_t baseline = vc_gc_live_allocations;

    VcGcPin null_pin = {0};
    check(vc_gc_pin_acquire(&null_pin, NULL));
    check(null_pin.active && null_pin.owner == NULL);
    check(vc_gc_pin_release(&null_pin));
    check(!null_pin.active && null_pin.owner == NULL);
    check(!vc_gc_pin_release(&null_pin));

    int unmanaged = 243;
    VcGcPin invalid_pin = {0};
    check(!vc_gc_pin_acquire(&invalid_pin, &unmanaged));
    check(!invalid_pin.active && invalid_pin.owner == NULL);

    int32_t *child = (int32_t *)vc_runtime_alloc(sizeof(*child), NULL);
    *child = 243;
    PinProbeOwner *owner = (PinProbeOwner *)vc_runtime_alloc(sizeof(*owner), trace_probe_owner);
    owner->child = child;
    owner->value = 486;

    check(vc_gc_live_allocations == baseline + 2u);
    check(pin_count_for(owner) == 0u);

    VcGcPin first = {0};
    VcGcPin second = {0};
    check(vc_gc_pin_acquire(&first, owner));
    check(first.active && first.owner == owner);
    check(pin_count_for(owner) == 1u);
    check(!vc_gc_pin_acquire(&first, owner));
    check(vc_gc_pin_acquire(&second, owner));
    check(pin_count_for(owner) == 2u);

    child = NULL;
    vc_gc_collect();
    check(vc_gc_live_allocations == baseline + 2u);
    check(owner->value == 486);
    check(*(int32_t *)owner->child == 243);

    check(vc_gc_pin_release(&first));
    check(pin_count_for(owner) == 1u);
    vc_gc_collect();
    check(vc_gc_live_allocations == baseline + 2u);
    check(*(int32_t *)owner->child == 243);

    check(vc_gc_pin_release(&second));
    check(pin_count_for(owner) == 0u);
    check(!vc_gc_pin_release(&second));

    owner = NULL;
    vc_gc_collect();
    check(vc_gc_live_allocations == baseline);

    vc_runtime_cleanup();
    check(vc_runtime_thread_detach(&context));
    vc_type_initializer_runtime_shutdown();
    vc_native_thread_runtime_shutdown();
    return 0;
}
