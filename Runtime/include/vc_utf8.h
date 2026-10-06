#ifndef VC_UTF8_H
#define VC_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool vc_utf8_scalar_is_valid(uint32_t scalar)
{
    return scalar <= UINT32_C(0x10ffff) &&
        !(scalar >= UINT32_C(0xd800) && scalar <= UINT32_C(0xdfff));
}

static inline bool vc_utf8_decode_one(
    const char *data,
    size_t length,
    size_t *offset,
    uint32_t *scalar)
{
    if (data == NULL || offset == NULL || scalar == NULL || *offset >= length)
        return false;

    const size_t start = *offset;
    const uint8_t first = (uint8_t)data[start];
    uint32_t value = 0u;
    size_t width = 0u;

    if (first <= UINT8_C(0x7f))
    {
        value = first;
        width = 1u;
    }
    else if (first >= UINT8_C(0xc2) && first <= UINT8_C(0xdf))
    {
        if (length - start < 2u)
            return false;
        const uint8_t second = (uint8_t)data[start + 1u];
        if ((second & UINT8_C(0xc0)) != UINT8_C(0x80))
            return false;
        value = ((uint32_t)(first & UINT8_C(0x1f)) << 6) |
            (uint32_t)(second & UINT8_C(0x3f));
        width = 2u;
    }
    else if (first >= UINT8_C(0xe0) && first <= UINT8_C(0xef))
    {
        if (length - start < 3u)
            return false;
        const uint8_t second = (uint8_t)data[start + 1u];
        const uint8_t third = (uint8_t)data[start + 2u];
        if ((second & UINT8_C(0xc0)) != UINT8_C(0x80) ||
            (third & UINT8_C(0xc0)) != UINT8_C(0x80))
            return false;
        if ((first == UINT8_C(0xe0) && second < UINT8_C(0xa0)) ||
            (first == UINT8_C(0xed) && second > UINT8_C(0x9f)))
            return false;
        value = ((uint32_t)(first & UINT8_C(0x0f)) << 12) |
            ((uint32_t)(second & UINT8_C(0x3f)) << 6) |
            (uint32_t)(third & UINT8_C(0x3f));
        width = 3u;
    }
    else if (first >= UINT8_C(0xf0) && first <= UINT8_C(0xf4))
    {
        if (length - start < 4u)
            return false;
        const uint8_t second = (uint8_t)data[start + 1u];
        const uint8_t third = (uint8_t)data[start + 2u];
        const uint8_t fourth = (uint8_t)data[start + 3u];
        if ((second & UINT8_C(0xc0)) != UINT8_C(0x80) ||
            (third & UINT8_C(0xc0)) != UINT8_C(0x80) ||
            (fourth & UINT8_C(0xc0)) != UINT8_C(0x80))
            return false;
        if ((first == UINT8_C(0xf0) && second < UINT8_C(0x90)) ||
            (first == UINT8_C(0xf4) && second > UINT8_C(0x8f)))
            return false;
        value = ((uint32_t)(first & UINT8_C(0x07)) << 18) |
            ((uint32_t)(second & UINT8_C(0x3f)) << 12) |
            ((uint32_t)(third & UINT8_C(0x3f)) << 6) |
            (uint32_t)(fourth & UINT8_C(0x3f));
        width = 4u;
    }
    else
    {
        return false;
    }

    if (!vc_utf8_scalar_is_valid(value))
        return false;

    *offset = start + width;
    *scalar = value;
    return true;
}

static inline bool vc_utf8_scalar_at(
    const char *data,
    size_t length,
    size_t scalar_index,
    uint32_t *scalar)
{
    if (data == NULL || scalar == NULL)
        return false;

    size_t offset = 0u;
    size_t current = 0u;
    while (offset < length)
    {
        uint32_t value = 0u;
        if (!vc_utf8_decode_one(data, length, &offset, &value))
            return false;
        if (current == scalar_index)
        {
            *scalar = value;
            return true;
        }
        current++;
    }
    return false;
}

static inline bool vc_utf8_byte_offset(
    const char *data,
    size_t length,
    size_t scalar_index,
    size_t *byte_offset)
{
    if (byte_offset == NULL || (data == NULL && length != 0u))
        return false;

    size_t offset = 0u;
    size_t current = 0u;
    while (current < scalar_index)
    {
        uint32_t scalar = 0u;
        if (!vc_utf8_decode_one(data, length, &offset, &scalar))
            return false;
        current++;
    }
    *byte_offset = offset;
    return true;
}

static inline size_t vc_utf8_encode_scalar(uint32_t scalar, char out[4])
{
    if (out == NULL || !vc_utf8_scalar_is_valid(scalar))
        return 0u;
    if (scalar <= UINT32_C(0x7f))
    {
        out[0] = (char)scalar;
        return 1u;
    }
    if (scalar <= UINT32_C(0x7ff))
    {
        out[0] = (char)(UINT32_C(0xc0) | (scalar >> 6));
        out[1] = (char)(UINT32_C(0x80) | (scalar & UINT32_C(0x3f)));
        return 2u;
    }
    if (scalar <= UINT32_C(0xffff))
    {
        out[0] = (char)(UINT32_C(0xe0) | (scalar >> 12));
        out[1] = (char)(UINT32_C(0x80) | ((scalar >> 6) & UINT32_C(0x3f)));
        out[2] = (char)(UINT32_C(0x80) | (scalar & UINT32_C(0x3f)));
        return 3u;
    }
    out[0] = (char)(UINT32_C(0xf0) | (scalar >> 18));
    out[1] = (char)(UINT32_C(0x80) | ((scalar >> 12) & UINT32_C(0x3f)));
    out[2] = (char)(UINT32_C(0x80) | ((scalar >> 6) & UINT32_C(0x3f)));
    out[3] = (char)(UINT32_C(0x80) | (scalar & UINT32_C(0x3f)));
    return 4u;
}

static inline bool vc_utf8_validate_and_count(
    const char *data,
    size_t length,
    size_t *scalar_count)
{
    if (scalar_count == NULL)
        return false;
    *scalar_count = 0u;
    if (data == NULL)
        return length == 0u;

    size_t offset = 0u;
    while (offset < length)
    {
        uint32_t scalar = 0u;
        if (!vc_utf8_decode_one(data, length, &offset, &scalar))
            return false;
        (*scalar_count)++;
    }
    return true;
}

static inline bool vc_utf8_validate(const char *data, size_t length)
{
    size_t scalar_count = 0u;
    return vc_utf8_validate_and_count(data, length, &scalar_count);
}

#endif
