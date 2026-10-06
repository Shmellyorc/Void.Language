#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

int32_t void295_pair_kind(const char *text, size_t length)
{
    if (text == NULL)
        return length == 0u ? 1 : -1;
    return length == 0u ? 2 : 3;
}

int32_t void295_length(const char *text, size_t length)
{
    if (text == NULL && length != 0u)
        return -2;
    if (length > (size_t)INT32_MAX)
        return -3;
    return (int32_t)length;
}

int32_t void295_embedded(const char *text, size_t length)
{
    if (text == NULL || length != 5u)
        return 0;
    return text[0] == 'a' && text[1] == 'b' && text[2] == '\0' &&
        text[3] == 'c' && text[4] == 'd' ? 1 : 0;
}

int32_t void295_mix(
    int32_t prefix,
    const char *left,
    size_t left_length,
    int32_t middle,
    const char *right,
    size_t right_length,
    int32_t suffix)
{
    static const unsigned char expected_right[] = {0xc3u, 0xa9u};
    if (prefix != 1 || middle != 2 || suffix != 3)
        return 0;
    if (left == NULL || left_length != 3u || left[0] != 'A' || left[1] != '\0' || left[2] != 'B')
        return 0;
    if (right == NULL || right_length != sizeof(expected_right))
        return 0;
    return (unsigned char)right[0] == expected_right[0] &&
        (unsigned char)right[1] == expected_right[1] ? 1 : 0;
}

int32_t void295_borrowed_nul_length(const char *text)
{
    return text == NULL ? -1 : (int32_t)strlen(text);
}

int32_t void295_legacy_length(const char *text)
{
    return text == NULL ? -1 : (int32_t)strlen(text);
}
