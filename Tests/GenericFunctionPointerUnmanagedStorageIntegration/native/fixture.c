#include <stdint.h>

typedef int32_t (*Voidc258BinaryFn)(int32_t, int32_t);

int32_t voidc258_add(int32_t left, int32_t right)
{
    return left + right;
}

int32_t voidc258_subtract(int32_t left, int32_t right)
{
    return left - right;
}

int32_t voidc258_apply(Voidc258BinaryFn callback, int32_t left, int32_t right)
{
    return callback == 0 ? -999 : callback(left, right);
}

Voidc258BinaryFn voidc258_select(int32_t mode)
{
    return mode == 0 ? &voidc258_add : &voidc258_subtract;
}
