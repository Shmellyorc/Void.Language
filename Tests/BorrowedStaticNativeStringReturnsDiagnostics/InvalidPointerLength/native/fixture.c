#include <stddef.h>

typedef struct VcNativeStringView
{
    const char *data;
    size_t length;
} VcNativeStringView;

VcNativeStringView void296_invalid_view(void)
{
    return (VcNativeStringView){ .data = NULL, .length = 3u };
}
