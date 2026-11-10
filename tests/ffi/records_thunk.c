#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "g2_descriptor.h"

#if !defined(__clang__) || __clang_major__ != 23 || __clang_minor__ != 1 || __clang_patchlevel__ != 4
#error Gate 2 needs the matching pinned Clang
#endif
#if defined(G2_LLP64)
#if !defined(_M_X64) || !defined(_WIN32) || !defined(_MSC_VER)
#error Expected x64 MSVC ABI
#endif
_Static_assert(sizeof(long) == 4, "LLP64");
#else
#if !defined(__linux__) || !defined(__x86_64__) || !defined(__GLIBC__)
#error Expected Linux x64 GNU ABI
#endif
_Static_assert(sizeof(long) == 8, "LP64");
#endif
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Expected little-endian
#endif
_Static_assert(CHAR_BIT == 8 && sizeof(void*) == 8 && SIZE_MAX == UINT64_MAX && sizeof(int) == 4, "x64 C widths");
_Static_assert(sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 &&
    FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_MAX_EXP == 128 && DBL_MAX_EXP == 1024, "float/double");

typedef struct G2Color { uint8_t r, g, b, a; } G2Color;
typedef struct G2Sample { int32_t tag; double reading; } G2Sample;
typedef struct G2Packet {
    G2Color color;
    uint16_t channels[3];
    uint8_t mode;
    G2Sample datum;
} G2Packet;

#define FIELD(record, field, type, offset) \
    _Static_assert(offsetof(record, field) == offset && _Generic(((record*)0)->field, type:1, default:0), #record "." #field)
_Static_assert(sizeof(G2Color) == 4 && _Alignof(G2Color) == 1, "color layout");
FIELD(G2Color, r, uint8_t, 0);
FIELD(G2Color, g, uint8_t, 1);
FIELD(G2Color, b, uint8_t, 2);
FIELD(G2Color, a, uint8_t, 3);
_Static_assert(sizeof(G2Sample) == 16 && _Alignof(G2Sample) == 8, "sample layout");
FIELD(G2Sample, tag, int32_t, 0);
FIELD(G2Sample, reading, double, 8);
_Static_assert(sizeof(G2Packet) == 32 && _Alignof(G2Packet) == 8, "packet layout");
FIELD(G2Packet, color, G2Color, 0);
FIELD(G2Packet, mode, uint8_t, 10);
FIELD(G2Packet, datum, G2Sample, 16);
_Static_assert(offsetof(G2Packet, channels) == 4 && sizeof(((G2Packet*)0)->channels) == 6 &&
    offsetof(G2Packet, channels[1]) == 6 && offsetof(G2Packet, channels[2]) == 8 &&
    _Generic(((G2Packet*)0)->channels[0], uint16_t:1, default:0), "packet array extent, stride and type");
#undef FIELD

extern const char* g2_fixture_identity(void);
extern G2Color g2_color(G2Color);
extern G2Sample g2_calibrate(G2Sample, float);
extern G2Packet g2_packet(G2Sample, G2Packet, uint32_t);
extern int64_t g2_sum(const uint8_t*, size_t);
extern int64_t g2_fill(uint8_t*, size_t, uint8_t);
extern int64_t g2_adjust(uint8_t*, size_t);
extern int64_t g2_move(const uint8_t*, size_t, uint8_t*, size_t);

static const void* record_at(uint64_t word, size_t alignment) {
    return word && word % alignment == 0 ? (const void*)(uintptr_t)word : NULL;
}
uint32_t vd_c_primitive_dispatch_v2(const char* descriptor, uint32_t size, uint32_t ordinal,
    const uint64_t* words, uint32_t count, uint64_t* out) {
    if (!descriptor || size != sizeof(g2_descriptor) - 1 || memcmp(descriptor, g2_descriptor, size) ||
        strcmp(g2_fixture_identity(), G2_FIXTURE_IDENTITY)) return 1;
    if (ordinal == UINT32_MAX) return words || count || out ? 2 : 0;
    static const uint8_t counts[] = {1,2,3,3,4,3,6};
    if (ordinal >= sizeof counts || count != counts[ordinal] || !words || !out || (uintptr_t)out % 8) return 2;
    switch (ordinal) {
    case 0: {
        G2Color input;
        const void* address = record_at(words[0], _Alignof(G2Color));
        if (!address) return 2;
        memcpy(&input, address, sizeof input);
        const G2Color result = g2_color(input);
        memcpy(out, &result, sizeof result);
        break;
    }
    case 1: {
        G2Sample input;
        const void* address = record_at(words[0], _Alignof(G2Sample));
        if (!address) return 2;
        memcpy(&input, address, sizeof input);
        const uint32_t bits = (uint32_t)words[1];
        float factor;
        memcpy(&factor, &bits, sizeof factor);
        const G2Sample result = g2_calibrate(input, factor);
        memcpy(out, &result, sizeof result);
        break;
    }
    case 2: {
        G2Sample base;
        G2Packet packet;
        const void* first = record_at(words[0], _Alignof(G2Sample));
        const void* second = record_at(words[1], _Alignof(G2Packet));
        if (!first || !second) return 2;
        memcpy(&base, first, sizeof base);
        memcpy(&packet, second, sizeof packet);
        const G2Packet result = g2_packet(base, packet, (uint32_t)words[2]);
        memcpy(out, &result, sizeof result);
        break;
    }
    case 3: *out = (uint64_t)g2_sum((const uint8_t*)(uintptr_t)words[0], words[2]); break;
    case 4: *out = (uint64_t)g2_fill((uint8_t*)(uintptr_t)words[0], words[2], (uint8_t)words[3]); break;
    case 5: *out = (uint64_t)g2_adjust((uint8_t*)(uintptr_t)words[0], words[2]); break;
    case 6: *out = (uint64_t)g2_move((const uint8_t*)(uintptr_t)words[0], words[2],
                                   (uint8_t*)(uintptr_t)words[3], words[5]); break;
    default: return 2;
    }
    return 0;
}
