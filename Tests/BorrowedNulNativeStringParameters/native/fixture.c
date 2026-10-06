#include <stdint.h>
#include <stddef.h>
#include <string.h>

int32_t void294_is_null(const char *text)
{
    return text == NULL ? 1 : 0;
}

int32_t void294_length(const char *text)
{
    return text == NULL ? -1 : (int32_t)strlen(text);
}

int32_t void294_length_legacy(const char *text)
{
    return text == NULL ? -1 : (int32_t)strlen(text);
}
