#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct VcNativeStringView
{
    const char *data;
    size_t length;
} VcNativeStringView;

static int32_t release_count = 0;

void void300_reset(void)
{
    release_count = 0;
}

int32_t void300_borrowed_nul_length(const char *text)
{
    return text == NULL ? -1 : (int32_t)strlen(text);
}

int32_t void300_view_matches(const char *data, size_t length)
{
    static const char expected[] = {'A', '\0', 'B'};
    return data != NULL && length == sizeof(expected) && memcmp(data, expected, sizeof(expected)) == 0;
}

const char *void300_borrowed_nul(void)
{
    return "borrowed-300";
}

VcNativeStringView void300_static_view(void)
{
    static const char text[] = {'s','t','a','t','i','c','\0','v','i','e','w'};
    VcNativeStringView result = { text, sizeof(text) };
    return result;
}

const char *void300_owned_nul(void)
{
    static const char text[] = "owned-300";
    char *copy = (char *)malloc(sizeof(text));
    if (copy == NULL) abort();
    memcpy(copy, text, sizeof(text));
    return copy;
}

VcNativeStringView void300_owned_view(void)
{
    static const char text[] = {'o','w','n','e','d','\0','v','i','e','w'};
    char *copy = (char *)malloc(sizeof(text));
    if (copy == NULL) abort();
    memcpy(copy, text, sizeof(text));
    VcNativeStringView result = { copy, sizeof(text) };
    return result;
}

void void300_release(void *data)
{
    if (data != NULL)
    {
        free(data);
        release_count++;
    }
}

int32_t void300_release_count(void)
{
    return release_count;
}

int32_t void300_call_view(int32_t (*callback)(const char *, size_t))
{
    static const char text[] = {'c','a','l','l','b','a','c','k','\0','v','a','l','u','e'};
    return callback(text, sizeof(text));
}
