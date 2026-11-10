#include "explicit_abi.h"

// MSVC ABI marker for freestanding floating-point code. No CRT is linked.
int _fltused = 0;

struct Token { int value; int active; };
static Token owned;

__declspec(dllexport) Sample sample(const Table* table, Sample in) {
    in.tag += table->bias();
    in.value *= 2.0;
    return in;
}

__declspec(dllexport) size_t fill(const Table* table, unsigned char* data, size_t size) {
    for (size_t n = 0; n < size; ++n) data[n] = (unsigned char)(table->bias() + n);
    return size;
}

__declspec(dllexport) Token* acquire(const Table* table, int value) {
    if (owned.active) return 0;
    owned.value = value + table->bias();
    owned.active = 1;
    return &owned;
}

__declspec(dllexport) int inspect(Token* token) {
    return token == &owned && owned.active ? token->value : -1;
}

__declspec(dllexport) int release(Token* token) {
    if (token != &owned || !owned.active) return 1;
    owned.active = 0;
    return 0;
}
