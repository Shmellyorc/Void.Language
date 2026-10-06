#include <stdint.h>

int32_t voidc260_add(int32_t left, int32_t right)
{
    return left + right;
}

int32_t voidc260_subtract(int32_t left, int32_t right)
{
    return left - right;
}

int32_t voidc260_apply(int32_t (*callback)(int32_t, int32_t), int32_t left, int32_t right)
{
    return callback == 0 ? -1 : callback(left, right);
}

int32_t (*voidc260_select(int32_t mode))(int32_t, int32_t)
{
    return mode == 0 ? voidc260_add : voidc260_subtract;
}
