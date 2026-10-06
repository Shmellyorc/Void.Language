#include <stdint.h>

int32_t voidc256_add(int32_t left, int32_t right)
{
    return left + right;
}

int32_t voidc256_answer(void)
{
    return 42;
}

void voidc256_scale(int32_t *values, int32_t count, int32_t factor)
{
    if (values == 0 || count < 0)
        return;
    for (int32_t i = 0; i < count; i++)
        values[i] *= factor;
}

int32_t *voidc256_identity(int32_t *value)
{
    return value;
}
