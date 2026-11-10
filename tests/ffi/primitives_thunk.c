#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "g1_descriptor.h"

#if !defined(__clang__) || __clang_major__ != 23 || __clang_minor__ != 1 || __clang_patchlevel__ != 4
#error Gate 1 C objects require the matching pinned Clang
#endif
#if defined(G1_LLP64)
#if !defined(_WIN32) || !defined(_M_X64) || !defined(_MSC_VER)
#error Gate 1 needs the x64 MSVC ABI on Windows
#endif
_Static_assert(sizeof(long) == 4 && LONG_MIN == INT32_MIN && LONG_MAX == INT32_MAX, "LLP64 long");
#else
#if !defined(__linux__) || !defined(__x86_64__) || !defined(__GLIBC__)
#error Gate 1 needs the x64 GNU SysV ABI on Linux
#endif
_Static_assert(sizeof(long) == 8 && LONG_MIN == INT64_MIN && LONG_MAX == INT64_MAX, "LP64 long");
#endif
_Static_assert(CHAR_BIT == 8 && sizeof(void*) == 8 && SIZE_MAX == UINT64_MAX, "64-bit size and octets");
_Static_assert(sizeof(int) == 4 && INT_MIN == INT32_MIN && INT_MAX == INT32_MAX, "32-bit signed C int");
_Static_assert(sizeof(bool) == 1, "target C bool byte");
_Static_assert(INT8_MIN == -128 && INT16_MIN == -32768 && INT64_MIN == -INT64_MAX - 1, "signed representation");
_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128, "binary32");
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, "binary64");
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Gate 1 requires little-endian x64 representation
#endif

/* The fixture is compiled separately without these declarations. */
extern const char* g1_fixture_identity(void);
extern int8_t g1_signed8(int8_t);
extern int16_t g1_signed16(int16_t);
extern int32_t g1_signed32(int32_t);
extern int64_t g1_signed64(int64_t);
extern uint8_t g1_unsigned8(uint8_t);
extern uint16_t g1_unsigned16(uint16_t);
extern uint32_t g1_unsigned32(uint32_t);
extern uint64_t g1_unsigned64(uint64_t);
extern int g1_cint(int);
extern long g1_clong(long);
extern size_t g1_csize(size_t);
extern bool g1_invert(bool);
extern float g1_single(float, float);
extern double g1_double(double, double);
extern bool g1_sign_zero(float, double);
extern uint64_t g1_mixed(int8_t, uint16_t, int32_t, float, uint64_t, double, int64_t,
                        float, uint32_t, double, uint8_t, int16_t, float, double);
extern uint32_t g1_sentinel(void);
extern void g1_no_result(uint32_t);

#define READ_BITS(name, value_type, bits_type)             \
    static value_type name(uint64_t word) {                \
        const bits_type bits = (bits_type)word;            \
        value_type result;                                \
        memcpy(&result, &bits, sizeof result);             \
        return result;                                    \
    }
READ_BITS(read_i8, int8_t, uint8_t)
READ_BITS(read_i16, int16_t, uint16_t)
READ_BITS(read_i32, int32_t, uint32_t)
READ_BITS(read_i64, int64_t, uint64_t)
READ_BITS(read_f32, float, uint32_t)
READ_BITS(read_f64, double, uint64_t)
#undef READ_BITS

uint32_t vd_c_primitive_dispatch_v1(const char* descriptor, uint32_t size, uint32_t ordinal,
                                    const uint64_t* args, uint32_t count, uint64_t* out) {
    if (!descriptor || size != sizeof(g1_descriptor) - 1 ||
        memcmp(descriptor, g1_descriptor, size) != 0 ||
        strcmp(g1_fixture_identity(), G1_FIXTURE_IDENTITY) != 0) return 1;
    if (ordinal == UINT32_MAX) return args || count || out ? 2 : 0;

    /* The generated descriptor pins this exact ordinal and word-count table. */
    static const uint8_t counts[] = {1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,14,0,1};
    if (ordinal >= sizeof counts || count != counts[ordinal] || (count != 0) != (args != NULL) ||
        (ordinal != 17) != (out != NULL)) return 2;
    switch (ordinal) {
    case 0: *out = (uint8_t)g1_signed8(read_i8(args[0])); break;
    case 1: *out = (uint16_t)g1_signed16(read_i16(args[0])); break;
    case 2: *out = (uint32_t)g1_signed32(read_i32(args[0])); break;
    case 3: *out = (uint64_t)g1_signed64(read_i64(args[0])); break;
    case 4: *out = g1_unsigned8((uint8_t)args[0]); break;
    case 5: *out = g1_unsigned16((uint16_t)args[0]); break;
    case 6: *out = g1_unsigned32((uint32_t)args[0]); break;
    case 7: *out = g1_unsigned64(args[0]); break;
    case 8: *out = (uint32_t)g1_cint(read_i32(args[0])); break;
    case 9:
#if defined(G1_LLP64)
        *out = (uint32_t)g1_clong(read_i32(args[0]));
#else
        *out = (uint64_t)g1_clong(read_i64(args[0]));
#endif
        break;
    case 10: *out = g1_csize((size_t)args[0]); break;
    case 11:
        if (args[0] > 1) return 2;
        *out = g1_invert(args[0] != 0);
        break;
    case 12: {
        const float value = g1_single(read_f32(args[0]), read_f32(args[1]));
        uint32_t bits;
        memcpy(&bits, &value, sizeof bits);
        *out = bits;
        break;
    }
    case 13: {
        const double value = g1_double(read_f64(args[0]), read_f64(args[1]));
        memcpy(out, &value, sizeof value);
        break;
    }
    case 14: *out = g1_sign_zero(read_f32(args[0]), read_f64(args[1])); break;
    case 15:
        *out = g1_mixed(read_i8(args[0]), (uint16_t)args[1], read_i32(args[2]), read_f32(args[3]), args[4],
                        read_f64(args[5]), read_i64(args[6]), read_f32(args[7]), (uint32_t)args[8],
                        read_f64(args[9]), (uint8_t)args[10], read_i16(args[11]), read_f32(args[12]), read_f64(args[13]));
        break;
    case 16: *out = g1_sentinel(); break;
    case 17: g1_no_result((uint32_t)args[0]); break;
    default: return 2;
    }
    return 0;
}
