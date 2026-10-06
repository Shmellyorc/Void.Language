#include "../Library/.void/Void244FixedCleanup.c"

#include <stdbool.h>
#include <stdio.h>

static void check(bool condition)
{
    puts(condition ? "True" : "False");
}

static bool collect_to_baseline(size_t baseline)
{
    vc_gc_collect();
    return vc_gc_live_allocations == baseline;
}

int main(void)
{
    vc_library_init();
    VcRuntimeThreadContext context;
    check(vc_runtime_thread_attach(&context));

    /* Warm every exported path so static type initialization is outside the baseline. */
    check(void244_fixed_normal() == 2);
    check(void244_fixed_return() == 3);
    check(void244_fixed_break() == 5);
    check(void244_fixed_continue() == 2);
    check(void244_fixed_exception() == 7);
    check(void244_fixed_nested() == 19);
    vc_gc_collect();
    const size_t baseline = vc_gc_live_allocations;

    check(void244_fixed_normal() == 2 && collect_to_baseline(baseline));
    check(void244_fixed_return() == 3 && collect_to_baseline(baseline));
    check(void244_fixed_break() == 5 && collect_to_baseline(baseline));
    check(void244_fixed_continue() == 2 && collect_to_baseline(baseline));
    check(void244_fixed_exception() == 7 && collect_to_baseline(baseline));
    check(void244_fixed_nested() == 19 && collect_to_baseline(baseline));

    vc_runtime_cleanup();
    check(vc_runtime_thread_detach(&context));
    vc_type_initializer_runtime_shutdown();
    vc_native_thread_runtime_shutdown();
    return 0;
}
