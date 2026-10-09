/* Enable OS declarations before any libc header is parsed. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#include "vc_entropy.h"
#include <stddef.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#include <errno.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#else
#error "VOID automatic Random seeding requires an implemented secure OS entropy source"
#endif

bool vc_native_entropy_seed(uint64_t *seed)
{
    if (seed == NULL) return false;
    uint64_t bits = 0;
#if defined(_WIN32)
    /* OS-managed system RNG; BCryptGenRandom links against bcrypt. */
    if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, (PUCHAR)&bits,
                        (ULONG)sizeof(bits), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return false;
    *seed = bits;
    return true;
#elif defined(__linux__)
    unsigned char *bytes = (unsigned char *)&bits;
    size_t received = 0;
    while (received < sizeof(bits))
    {
        ssize_t amount = getrandom(bytes + received, sizeof(bits) - received, 0);
        if (amount < 0)
        {
            if (errno == EINTR) continue;
            return false;
        }
        if (amount == 0) return false;
        received += (size_t)amount;
    }
    *seed = bits;
    return true;
#else
    arc4random_buf(&bits, sizeof(bits));
    *seed = bits;
    return true;
#endif
}
