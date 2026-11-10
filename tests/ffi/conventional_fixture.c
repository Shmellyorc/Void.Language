#define G4_PROVIDER
#include "conventional_fixture.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
__declspec(dllimport)
#endif
int g4_conventional_bias(void);
#ifdef _WIN32
__declspec(dllimport)
#endif
void g4_conventional_delay(void);

struct ConventionalBox { int32_t value; };
static ConventionalBox* last;
static int32_t pings;

int g4_conventional_add(int x, int y) { return x + y + g4_conventional_bias(); }
double g4_conventional_scale(double x, float y) { return x * y; }
long g4_conventional_long(long x) { return x + g4_conventional_bias(); }
bool g4_conventional_flip(bool x) { return !x; }
ConventionalSample g4_conventional_sample(ConventionalSample s) {
    return (ConventionalSample){s.tag + g4_conventional_bias(), s.value * 2};
}
int64_t g4_conventional_sum(const uint8_t* data, size_t offset, size_t length) {
    (void)offset;
    int64_t total = 0;
    for (size_t i = 0; i < length; ++i) total += data[i];
    return total;
}
int64_t g4_conventional_fill(uint8_t* data, size_t offset, size_t length, uint8_t start) {
    (void)offset;
    for (size_t i = 0; i < length; ++i) data[i] = (uint8_t)(start + i);
    return start == 255 ? -7 : (int64_t)length;
}
int64_t g4_conventional_shift(uint8_t* data, size_t offset, size_t length) {
    (void)offset;
    for (size_t i = 0; i < length; ++i) data[i] += 2;
    return (int64_t)length;
}
ConventionalBox* g4_conventional_acquire(int32_t value) {
    if (value == -1) return NULL;
    ConventionalBox* box = malloc(sizeof *box);
    if (box) {
        box->value = value;
        last = box;
    }
    return box;
}
int32_t g4_conventional_borrow(const ConventionalBox* box) { return box->value; }
int32_t g4_conventional_close(ConventionalBox* box) {
    int32_t status = box->value == -2 ? 17 : 0;
    FILE* log = fopen("native-release.log", "a");
    if (log) {
        fprintf(log, "%d\n", (int)box->value);
        fclose(log);
    }
    if (last == box) last = NULL;
    free(box);
    return status;
}
void g4_conventional_ping(void) {
    g4_conventional_delay();
    ++pings;
}
int32_t g4_conventional_pings(void) { return pings; }
ConventionalBox* g4_conventional_last(void) { return last; }
