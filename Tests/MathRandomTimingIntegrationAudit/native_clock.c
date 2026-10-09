/* Execute the real POSIX conversion with an injected host clock. The pure
   fraction helper is also tested here; this is not Windows API execution. */
#define clock_gettime audit_clock_gettime
#include "../../Runtime/src/vc_thread.c"
#undef clock_gettime
#include <stdio.h>
#include "clock_vectors.h"

static struct timespec next_time;
static int fail_clock;
int audit_clock_gettime(clockid_t id, struct timespec *out)
{
    if (id != CLOCK_MONOTONIC || fail_clock) return -1;
    *out = next_time;
    return 0;
}

int main(void)
{
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++)
        if (vc_native_clock_fraction_ns(vectors[i][0], vectors[i][1]) != vectors[i][2])
            return 1;
    uint64_t value = 42;
    if (vc_native_monotonic_time_ns(NULL)) return 2;
    fail_clock = 1;
    if (vc_native_monotonic_time_ns(&value) || value != 42) return 3;
    fail_clock = 0;
    next_time.tv_sec = -1;
    if (vc_native_monotonic_time_ns(&value) || value != 42) return 4;
    next_time.tv_sec = 0;
    next_time.tv_nsec = -1;
    if (vc_native_monotonic_time_ns(&value) || value != 42) return 5;
    next_time.tv_nsec = 1000000000L;
    if (vc_native_monotonic_time_ns(&value) || value != 42) return 6;
    next_time.tv_sec = 123;
    next_time.tv_nsec = 456;
    if (!vc_native_monotonic_time_ns(&value) || value != UINT64_C(123000000456)) return 7;
    if (sizeof(time_t) >= 8)
    {
        next_time.tv_sec = (time_t)(UINT64_MAX / UINT64_C(1000000000));
        next_time.tv_nsec = (long)(UINT64_MAX % UINT64_C(1000000000));
        if (!vc_native_monotonic_time_ns(&value) || value != UINT64_MAX) return 8;
        next_time.tv_nsec++;
        value = 42;
        if (vc_native_monotonic_time_ns(&value) || value != 42) return 9;
        next_time.tv_sec++;
        next_time.tv_nsec = 0;
        if (vc_native_monotonic_time_ns(&value) || value != 42) return 10;
    }
    puts("True");
    return 0;
}
