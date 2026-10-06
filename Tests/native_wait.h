#ifndef VOID_TEST_NATIVE_WAIT_H
#define VOID_TEST_NATIVE_WAIT_H

#include "vc_thread.h"
#include <stdatomic.h>

/* Iterations do not measure elapsed time. Yield so newly created workers can
 * reach the asserted readiness state even while parallel builds occupy CPUs. */
static bool void_test_wait_for_count(atomic_int *value, int expected)
{
    uint64_t started;
    if (!vc_native_monotonic_time_ns(&started))
        return false;
    for (;;)
    {
        if (atomic_load(value) >= expected)
            return true;
        uint64_t now;
        if (!vc_native_monotonic_time_ns(&now)
            || now - started >= UINT64_C(10000000000)
            || !vc_native_sleep_ms(1u))
            return false;
    }
}

#endif
