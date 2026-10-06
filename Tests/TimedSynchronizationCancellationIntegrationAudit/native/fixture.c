#include <stdint.h>

static int32_t calls;

void voidc190_reset(void)
{
    calls = 0;
}

int32_t voidc190_mark(int32_t value)
{
    calls++;
    return value + 10;
}

int32_t voidc190_calls(void)
{
    return calls;
}
