#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef int32_t (*Voidc257BinaryFn)(int32_t, int32_t);

int32_t voidc257_add(int32_t left, int32_t right)
{
    return left + right;
}

int32_t voidc257_subtract(int32_t left, int32_t right)
{
    return left - right;
}

int32_t voidc257_apply(Voidc257BinaryFn callback, int32_t left, int32_t right)
{
    return callback == NULL ? -1 : callback(left, right);
}

Voidc257BinaryFn voidc257_select(int32_t operation)
{
    return operation == 0 ? &voidc257_add : &voidc257_subtract;
}

Voidc257BinaryFn voidc257_passthrough(Voidc257BinaryFn callback)
{
    return callback;
}

bool voidc257_is_null(Voidc257BinaryFn callback)
{
    return callback == NULL;
}
