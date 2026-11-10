#include "dynamic_fixture.h"
#include "g4_descriptor.h"

#include <stdatomic.h>
#include <limits.h>

#if !defined(__clang__) || __clang_major__ != 23 || __clang_minor__ != 1 || __clang_patchlevel__ != 4 || \
    !defined(__linux__) || !defined(__x86_64__)
#error G4 fixture requires the pinned Linux x64 target compiler
#endif
_Static_assert(CHAR_BIT == 8 && sizeof(long) == 8 && sizeof(int) == 4 &&
    UINTPTR_MAX == UINT64_MAX && sizeof(G4Sample) == 16 && _Alignof(G4Sample) == 8 &&
    offsetof(G4Sample, reading) == 8, "SysV carrier geometry");

static _Atomic(G4Box*) last;
static int32_t i32(uint64_t bits) {
    union { uint32_t raw; int32_t value; } word = { (uint32_t)bits };
    return word.value;
}
G4_EXPORT uint32_t vd_c_primitive_dispatch_v3(const char* descriptor, uint32_t size, uint32_t ordinal,
    const uint64_t* words, uint32_t count, uint64_t* result) {
    if (!descriptor || size != sizeof(g4_descriptor) - 1) return 1;
    for (uint32_t n = 0; n < size; ++n)
        if (descriptor[n] != g4_descriptor[n]) return 1;
    if (ordinal == UINT32_MAX) return words || count || result ? 1 : 0;
    static const uint8_t arity[] = {2,1,4,1,2,1,0,0,1};
    if (ordinal >= sizeof arity || count != arity[ordinal] || (count && !words) || !result) return 2;
    switch (ordinal) {
        case 0: *result = (uint32_t)g4_add(i32(words[0]), i32(words[1])); break;
        case 1: {
            if (!words[0]) return 2;
            const G4Sample* input = (const G4Sample*)(uintptr_t)words[0];
            G4Sample* output = (G4Sample*)result;
            *output = g4_sample(*input);
            break;
        }
        case 2:
            if (words[2] > 64 || (words[2] && !words[0])) return 2;
            *result = (uint64_t)g4_fill((uint8_t*)(uintptr_t)words[0], words[1], words[2], (uint8_t)words[3]);
            break;
        case 3: {
            int32_t input = i32(words[0]);
            if (input == -3) {
                // A refusal deliberately leaves a word that the caller does not own.
                *result = (uintptr_t)atomic_load(&last);
                return 2;
            }
            G4Box* box = g4_create(input);
            if (box) atomic_store(&last, box);
            *result = (uintptr_t)box;
            break;
        }
        case 4:
            if (!words[0]) return 2;
            *result = (uint32_t)g4_read((const G4Box*)(uintptr_t)words[0], i32(words[1]));
            break;
        case 5: {
            if (!words[0]) return 2;
            G4Box* box = (G4Box*)(uintptr_t)words[0];
            G4Box* expected = box;
            atomic_compare_exchange_strong(&last, &expected, NULL);
            *result = (uint32_t)g4_close(box);
            break;
        }
        case 6: *result = (uint32_t)g4_live(); break;
        case 7: *result = (uint32_t)g4_releases(); break;
        case 8: *result = (uint32_t)g4_gate(i32(words[0])); break;
        default: return 2;
    }
    return 0;
}
