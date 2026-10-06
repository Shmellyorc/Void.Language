#include <stdint.h>
#include <stddef.h>

typedef struct Voidc040Pair
{
    int32_t a;
    int32_t b;
} Voidc040Pair;

int32_t voidc040_add(int32_t left, int32_t right)
{
    return left + right;
}

void voidc040_make_pair(int32_t a, int32_t b, Voidc040Pair *value)
{
    value->a = a;
    value->b = b;
}

int32_t voidc040_sum_pair(const Voidc040Pair *value)
{
    return value->a + value->b;
}

void voidc040_shift_pair(Voidc040Pair *value, int32_t da, int32_t db)
{
    value->a += da;
    value->b += db;
}

int32_t voidc040_read_int(const int32_t *value)
{
    return *value;
}

void voidc040_increment(int32_t *value)
{
    (*value)++;
}

void voidc040_write_int(int32_t *value)
{
    *value = 1234;
}

int32_t voidc040_apply(int32_t value, int32_t (*callback)(int32_t))
{
    return callback(value);
}

void voidc040_fill(int32_t *values, int32_t count)
{
    for (int32_t i = 0; i < count; i++)
        values[i] = (i + 1) * 3;
}

int32_t voidc040_pair_size_matches(int32_t bytes)
{
    return bytes == (int32_t)sizeof(Voidc040Pair);
}
