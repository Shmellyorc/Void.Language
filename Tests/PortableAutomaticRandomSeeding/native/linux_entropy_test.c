/* Test Linux short reads, EINTR, zero-valued entropy, and failure forwarding. */
#if defined(__linux__)
#define getrandom vc377_mock_getrandom
#include "../../../Runtime/src/vc_entropy.c"
#undef getrandom
#include <stdio.h>
#include <string.h>

static int mode;
static int calls;
ssize_t vc377_mock_getrandom(void *buffer, size_t size, unsigned int flags)
{
    (void)flags;
    calls++;
    if (mode == 1) { errno = EIO; return -1; }
    if (calls == 1) { errno = EINTR; return -1; }
    if (size > 2) size = 2;
    memset(buffer, 0, size);
    return (ssize_t)size;
}
int main(void)
{
    uint64_t result = UINT64_C(123456);
    mode = 0; calls = 0;
    if (!vc_native_entropy_seed(&result) || result != 0 || calls != 5) return 1;
    mode = 1; calls = 0; result = UINT64_C(123456);
    if (vc_native_entropy_seed(&result) || result != UINT64_C(123456) || calls != 1) return 2;
    if (vc_native_entropy_seed(NULL)) return 3;
    puts("True");
    return 0;
}
#else
int main(void) { return 0; }
#endif
