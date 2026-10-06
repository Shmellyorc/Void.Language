#include <stddef.h>
#include <string.h>

typedef struct VcNativeStringView
{
    const char *data;
    size_t length;
} VcNativeStringView;

static char borrowed_nul[16] = "borrowed";
static char borrowed_view[] = {'A', '\0', 'B'};
static const char empty_data[] = "";
static const char static_view[] = {(char)0xc3, (char)0xa9, '\0', 'Z'};

const char *void296_borrowed_nul(void)
{
    memcpy(borrowed_nul, "borrowed", sizeof("borrowed"));
    return borrowed_nul;
}

const char *void296_static_nul(void)
{
    return "static";
}

VcNativeStringView void296_borrowed_view(void)
{
    borrowed_view[0] = 'A';
    borrowed_view[1] = '\0';
    borrowed_view[2] = 'B';
    return (VcNativeStringView){ .data = borrowed_view, .length = sizeof(borrowed_view) };
}

VcNativeStringView void296_static_view(void)
{
    return (VcNativeStringView){ .data = static_view, .length = sizeof(static_view) };
}

const char *void296_null_nul(void)
{
    return NULL;
}

VcNativeStringView void296_null_view(void)
{
    return (VcNativeStringView){ .data = NULL, .length = 0u };
}

VcNativeStringView void296_empty_view(void)
{
    return (VcNativeStringView){ .data = empty_data, .length = 0u };
}

const char *void296_legacy_nul(void)
{
    return "legacy";
}

VcNativeStringView void296_invalid_view(void)
{
    return (VcNativeStringView){ .data = NULL, .length = 3u };
}

void void296_mutate_borrowed(void)
{
    memcpy(borrowed_nul, "changed!", sizeof("changed!"));
    borrowed_view[0] = 'X';
    borrowed_view[1] = 'Y';
    borrowed_view[2] = 'Z';
}
