#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "g3_descriptor.h"

#if !defined(__clang__) || __clang_major__ != 23 || __clang_minor__ != 1 || __clang_patchlevel__ != 4
#error Gate 3 needs the matching pinned Clang
#endif
#if defined(G3_LLP64)
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
_Static_assert(CHAR_BIT == 8 && sizeof(void*) == 8 && UINTPTR_MAX == UINT64_MAX &&
    sizeof(int) == 4 && INT32_MIN == -INT32_MAX - 1, "x64 C representation");

typedef struct G3Box G3Box;
extern const char* g3_fixture_identity(void);
extern G3Box* g3_create(int32_t);
extern int32_t g3_read(const G3Box*, int32_t);
extern int32_t g3_add(G3Box*, int32_t);
extern int32_t g3_close(G3Box*);
extern int32_t g3_live(void);

static int32_t signed_word(uint64_t word) {
    uint32_t raw = (uint32_t)word;
    int32_t result;
    memcpy(&result, &raw, sizeof result);
    return result;
}
uint32_t vd_c_primitive_dispatch_v3(const char* descriptor, uint32_t size, uint32_t ordinal,
    const uint64_t* words, uint32_t count, uint64_t* out) {
    if (!descriptor || size != sizeof(g3_descriptor) - 1 || memcmp(descriptor, g3_descriptor, size) ||
        strcmp(g3_fixture_identity(), G3_FIXTURE_IDENTITY)) return 1;
    if (ordinal == UINT32_MAX) return words || count || out ? 2 : 0;
    static const uint8_t counts[] = {1, 2, 2, 1, 0};
    if (ordinal >= sizeof counts || count != counts[ordinal] || (count && !words) || !out ||
        (ordinal >= 1 && ordinal <= 3 && !words[0])) return 2;
    switch (ordinal) {
    case 0: *out = (uintptr_t)g3_create(signed_word(words[0])); break;
    case 1: *out = (uint32_t)g3_read((const G3Box*)(uintptr_t)words[0], signed_word(words[1])); break;
    case 2: *out = (uint32_t)g3_add((G3Box*)(uintptr_t)words[0], signed_word(words[1])); break;
    case 3: *out = (uint32_t)g3_close((G3Box*)(uintptr_t)words[0]); break;
    case 4: *out = (uint32_t)g3_live(); break;
    default: return 2;
    }
    return 0;
}
