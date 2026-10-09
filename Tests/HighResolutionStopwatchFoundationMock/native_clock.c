#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

static size_t index_clock = 0;
static const uint64_t times[] = {
    UINT64_C(100), UINT64_C(1000000100), UINT64_C(2000000100),
    UINT64_C(3000000100), UINT64_C(4000000100), UINT64_C(5000000100),
    UINT64_C(6000000100), UINT64_C(6001000100), UINT64_C(6002000100),
    UINT64_C(6003000100), UINT64_C(0), UINT64_MAX,
    UINT64_C(1000), UINT64_C(500), UINT64_C(800),
    UINT64_C(0), (uint64_t)INT64_MAX - UINT64_C(1),
    (uint64_t)INT64_MAX - UINT64_C(1), (uint64_t)INT64_MAX,
    UINT64_C(0), UINT64_C(1)
};

bool vc_stopwatch_test_clock_ns(uint64_t *out)
{
    if (out == NULL || index_clock >= sizeof(times) / sizeof(times[0]))
        return false;
    const size_t i = index_clock++;
    if (i == 10u)
        return false;
    *out = times[i];
    return true;
}
