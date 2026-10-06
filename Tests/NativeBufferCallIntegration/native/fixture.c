#include <stdint.h>
#include <stddef.h>

typedef struct Voidc248Pair
{
    int32_t a;
    int32_t b;
} Voidc248Pair;

int32_t voidc248_sum_i32(const int32_t *values, int32_t length)
{
    int32_t sum = 0;
    for (int32_t i = 0; i < length; i++)
        sum += values[i];
    return sum;
}

void voidc248_add_i32(int32_t *values, int32_t length, int32_t delta)
{
    for (int32_t i = 0; i < length; i++)
        values[i] += delta;
}

int32_t voidc248_dot_i32(
    const int32_t *left, int32_t left_length,
    const int32_t *right, int32_t right_length)
{
    if (left_length != right_length)
        return -1;
    int32_t sum = 0;
    for (int32_t i = 0; i < left_length; i++)
        sum += left[i] * right[i];
    return sum;
}

int32_t voidc248_empty_readonly(const int32_t *values, int32_t length)
{
    return values == NULL && length == 0 ? 1 : 0;
}

int32_t voidc248_empty_writable(int32_t *values, int32_t length)
{
    return values == NULL && length == 0 ? 1 : 0;
}

int32_t voidc248_sum_pairs(const Voidc248Pair *values, int32_t length)
{
    int32_t sum = 0;
    for (int32_t i = 0; i < length; i++)
        sum += values[i].a + values[i].b;
    return sum;
}

void voidc248_shift_pairs(Voidc248Pair *values, int32_t length, int32_t delta)
{
    for (int32_t i = 0; i < length; i++)
    {
        values[i].a += delta;
        values[i].b += delta;
    }
}
