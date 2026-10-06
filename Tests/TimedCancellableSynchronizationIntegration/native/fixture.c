#include <stdint.h>

static int32_t calls;

void voidc189_reset(void)
{
    calls = 0;
}

int32_t voidc189_mark(int32_t value)
{
    calls++;
    return value + 1;
}

int32_t voidc189_calls(void)
{
    return calls;
}
