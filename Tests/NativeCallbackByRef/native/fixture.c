#include <stdint.h>

typedef struct vc_s_0
{
    int32_t a;
    int32_t b;
} vc_s_0;

int32_t voidc098_callback_ref_int(int32_t value, void (*callback)(int32_t *))
{
    callback(&value);
    return value;
}

int32_t voidc098_callback_out_int(void (*callback)(int32_t *))
{
    int32_t value = 0;
    callback(&value);
    return value;
}

int32_t voidc098_callback_in_int(int32_t value, int32_t (*callback)(const int32_t *))
{
    return callback(&value);
}

int32_t voidc098_callback_ref_pair(int32_t a, int32_t b, void (*callback)(vc_s_0 *))
{
    vc_s_0 value = { a, b };
    callback(&value);
    return value.a + value.b;
}

int32_t voidc098_callback_in_pair(int32_t a, int32_t b, int32_t (*callback)(const vc_s_0 *))
{
    vc_s_0 value = { a, b };
    return callback(&value);
}

int32_t voidc098_callback_mixed(
    int32_t left,
    int32_t addend,
    void (*callback)(int32_t *, int32_t *, const int32_t *))
{
    int32_t output = 0;
    callback(&left, &output, &addend);
    return left + output;
}

int32_t voidc098_callback_out_pair(void (*callback)(vc_s_0 *))
{
    vc_s_0 value = { 0, 0 };
    callback(&value);
    return value.a * 10 + value.b;
}
