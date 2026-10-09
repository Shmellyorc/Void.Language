/* Test-only entropy provider. Replaces the OS backend during linked C tests. */
#include "vc_entropy.h"
#include <stddef.h>
#ifndef TEST_ENTROPY_VALUE
#define TEST_ENTROPY_VALUE UINT64_C(0)
#endif
static int requests = 0;
bool vc_native_entropy_seed(uint64_t *seed)
{
    if (seed == NULL) return false;
    requests++;
    if (requests != 1) return false; /* no extra native reads during draws */
#ifdef TEST_ENTROPY_FAIL
    return false;
#else
    *seed = TEST_ENTROPY_VALUE;
    return true;
#endif
}
