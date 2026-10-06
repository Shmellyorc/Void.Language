#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct VcNativeStringView
{
    const char *data;
    size_t length;
} VcNativeStringView;

const char *void301_valid_nul(void)
{
    static const unsigned char text[] = {'h', 0xc3u, 0xa9u, 'l', 'l', 'o', 0u};
    return (const char *)text;
}

VcNativeStringView void301_valid_view(void)
{
    static const unsigned char text[] = {'A', 0u, 0xf0u, 0x9fu, 0x98u, 0x80u, 'B'};
    VcNativeStringView result = { (const char *)text, sizeof(text) };
    return result;
}

int32_t void301_view_matches(const char *data, size_t length)
{
    static const unsigned char expected[] = {'A', 0u, 0xf0u, 0x9fu, 0x98u, 0x80u, 'B'};
    return data != NULL && length == sizeof(expected) &&
        memcmp(data, expected, sizeof(expected)) == 0;
}

int32_t void301_call_valid_view(int32_t (*callback)(const char *, size_t))
{
    static const unsigned char text[] = {'c','a','l','l','b','a','c','k',0u,0xf0u,0x9fu,0x98u,0x80u};
    return callback((const char *)text, sizeof(text));
}

const char *void301_invalid_nul(void)
{
    static const unsigned char text[] = {0xc3u, 0x28u, 0u};
    return (const char *)text;
}

VcNativeStringView void301_invalid_overlong(void)
{
    static const unsigned char text[] = {0xc0u, 0xafu};
    VcNativeStringView result = { (const char *)text, sizeof(text) };
    return result;
}

VcNativeStringView void301_invalid_surrogate(void)
{
    static const unsigned char text[] = {0xedu, 0xa0u, 0x80u};
    VcNativeStringView result = { (const char *)text, sizeof(text) };
    return result;
}

int32_t void301_call_invalid_view(int32_t (*callback)(const char *, size_t))
{
    static const unsigned char text[] = {0xf4u, 0x90u, 0x80u, 0x80u};
    return callback((const char *)text, sizeof(text));
}

VcNativeStringView void301_owned_invalid(void)
{
    unsigned char *data = (unsigned char *)malloc(2u);
    if (data == NULL) abort();
    data[0] = 0xc0u;
    data[1] = 0xafu;
    VcNativeStringView result = { (const char *)data, 2u };
    return result;
}

void void301_release_invalid(void *data)
{
    if (data == NULL) return;
    free(data);
    FILE *marker = fopen("Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn/release.marker", "wb");
    if (marker != NULL)
    {
        fputs("released", marker);
        fclose(marker);
    }
}
