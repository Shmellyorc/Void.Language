#include <stdint.h>

typedef int32_t *(*vc101_pointer_map)(int32_t *value);
typedef int32_t **(*vc101_nested_map)(int32_t **value);
typedef void *(*vc101_void_map)(void *value);
typedef void (*vc101_pointer_byref)(int32_t **current, int32_t **previous, int32_t * const *next);

int32_t *vc101_apply_pointer(vc101_pointer_map callback, int32_t *value)
{
    return callback(value);
}

int32_t **vc101_apply_nested(vc101_nested_map callback, int32_t **value)
{
    return callback(value);
}

void *vc101_apply_void(vc101_void_map callback, void *value)
{
    return callback(value);
}

void vc101_apply_byref(
    vc101_pointer_byref callback,
    int32_t **current,
    int32_t **previous,
    int32_t * const *next)
{
    callback(current, previous, next);
}
