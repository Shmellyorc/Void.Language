#include "dynamic_fixture.h"
#include <stdatomic.h>

struct G4Box {
    _Atomic unsigned used;
    int32_t value, status;
};
static G4Box boxes[4];
static _Atomic int32_t releases;
static _Atomic int32_t gate;

int32_t g4_add(int32_t left, int32_t right) {
    if (left == 200 && right == 0) {
        int32_t ready = 0;
        atomic_compare_exchange_strong(&gate, &ready, 1);
        while (atomic_load(&gate) == 1) __builtin_ia32_pause();
        return 200;
    }
    int64_t total = (int64_t)left + right + g4_bias();
    return total < INT32_MIN || total > INT32_MAX ? -19 : (int32_t)total;
}
G4Sample g4_sample(G4Sample input) {
    input.tag = g4_add(input.tag, 0);
    input.reading *= 2.0;
    return input;
}
int64_t g4_fill(uint8_t* bytes, size_t offset, size_t length, uint8_t start) {
    (void)offset; // The bridge already clips the native view.
    if (start == 255) return -7;
    for (size_t n = 0; n < length; ++n) bytes[n] = (uint8_t)(start + n);
    return (int64_t)length;
}
G4Box* g4_create(int32_t value) {
    if (value == -1) return NULL;
    for (size_t n = 0; n < sizeof boxes / sizeof *boxes; ++n) {
        unsigned free = 0;
        if (atomic_compare_exchange_strong(&boxes[n].used, &free, 1)) {
            boxes[n].value = value;
            boxes[n].status = value == -2 ? 17 : 0;
            return &boxes[n];
        }
    }
    return NULL;
}
int32_t g4_read(const G4Box* box, int32_t control) {
    return control ? -13 : box->value;
}
int32_t g4_close(G4Box* box) {
    int32_t status = box->status;
    atomic_fetch_add(&releases, 1);
    atomic_store(&box->used, 0);
    return status;
}
int32_t g4_live(void) {
    int32_t count = 0;
    for (size_t n = 0; n < sizeof boxes / sizeof *boxes; ++n) count += atomic_load(&boxes[n].used) != 0;
    return count;
}
int32_t g4_releases(void) { return atomic_load(&releases); }
int32_t g4_gate(int32_t command) {
    if (command == 2) atomic_store(&gate, 2);
    return atomic_load(&gate);
}
