/* Test-only fault injection around the generated, signature-checked adapter. */
#define vd_c_primitive_dispatch_v3 conventional_generated_dispatch
#include "adapter.c"
#undef vd_c_primitive_dispatch_v3

#ifdef _WIN32
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
uint32_t vd_c_primitive_dispatch_v3(const char* descriptor, uint32_t size, uint32_t ordinal,
    const uint64_t* words, uint32_t count, uint64_t* result) {
    if (ordinal == 8 && count == 1 && words[0] == UINT32_MAX - 2 && result) {
        // A rejected acquisition may leave a non-owning scratch value.
        *result = (uintptr_t)g4_conventional_last();
        return 2;
    }
    return conventional_generated_dispatch(descriptor, size, ordinal, words, count, result);
}
