#include <stdint.h>

typedef void (*void100_mixed_callback)(int32_t *left, int32_t *right, const int32_t *addend);

int32_t void100_callback_mixed(int32_t left, int32_t addend, void100_mixed_callback callback)
{
    int32_t right = 0;
    callback(&left, &right, &addend);
    return left + right;
}
