#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef ADDRESS_PROBE
#include <stddef.h>
#include <string.h>
struct Failure { const char *message; uint32_t count; const char *frames[32]; };
_Static_assert(offsetof(struct Failure, frames) == 16, "probe failure layout");
extern int32_t snapshot_case(int32_t, int32_t, struct Failure *);
static int tracking, event_count, events[4];
void record(int32_t event) {
    if (event_count < 4) events[event_count] = event;
    ++event_count;
}
#endif

/* Only the scalar adapter crosses the C ABI. LLVM array parameters/results
   travel between independently generated objects, never through C declarations. */
extern int32_t entry(int32_t seed, void *failure);
static unsigned fills, touches, checks;
void fill(void *storage, uint64_t count, int32_t seed) {
    uint32_t *values = storage;
    ++fills;
    for (uint64_t i = 0; i < count; ++i) values[i] = (uint32_t)seed + (uint32_t)i;
}
void touch(void *storage, uint64_t count) {
    uint32_t *values = storage;
    ++touches;
#ifdef ADDRESS_PROBE
    if (tracking) record(3);
#endif
    for (uint64_t i = 0; i < count; ++i) values[i] ^= UINT32_C(0xa55a5aa5);
}
int32_t check(void *storage, uint64_t count, int32_t seed) {
    const uint32_t *values = storage;
    ++checks;
#ifdef ADDRESS_PROBE
    if (tracking) record(4);
#endif
    for (uint64_t i = 0; i < count; ++i)
        if (values[i] != (((uint32_t)seed + (uint32_t)i) ^ UINT32_C(0xa55a5aa5))) return 1;
    return 0;
}
#ifdef ADDRESS_PROBE
int32_t plain(void *storage, uint64_t count, int32_t seed) {
    const uint32_t *values = storage;
    for (uint64_t i = 0; i < count; ++i)
        if (values[i] != (uint32_t)seed + (uint32_t)i) return 1;
    return 0;
}
int32_t later(void *storage, uint64_t count, int32_t mode, struct Failure *failure) {
    uint32_t *values = storage;
    record(2);
    for (uint64_t i = 0; i < count; ++i) values[i] = 99 + (uint32_t)i;
    if (mode == 2) failure->message = "probe later failure";
    return 99;
}
static int semantics(void) {
    tracking = 1;
    for (int32_t mode = 0; mode != 3; ++mode) {
        struct Failure failure = {0};
        fills = touches = checks = 0;
        event_count = 0;
        if (snapshot_case(17, mode, &failure) != 0 || fills != 1) return 1;
        int expected_events = mode == 0 ? 4 : mode == 1 ? 3 : 2;
        if (event_count != expected_events || touches != (mode == 2 ? 0u : 1u) ||
            checks != (mode == 0 ? 1u : 0u)) return 1;
        for (int i = 0; i < event_count; ++i) if (events[i] != i + 1) return 1;
        if (mode == 0) {
            if (failure.message || failure.count != 0) return 1;
        } else {
            const char *expected = mode == 1 ? "probe callee failure" : "probe later failure";
            if (!failure.message || strcmp(failure.message, expected) != 0 || failure.count != 1 ||
                !failure.frames[0] || strcmp(failure.frames[0], "probe caller") != 0) return 1;
        }
    }
    tracking = 0;
    return 0;
}
#endif
int main(void) {
    if (entry(17, NULL) != 0 || entry(-107, NULL) != 0 ||
        fills != 2 || touches != 2 || checks != 2) return EXIT_FAILURE;
#ifdef ADDRESS_PROBE
    if (semantics() != 0) return EXIT_FAILURE;
#endif
    puts("native array roundtrip passed");
    return EXIT_SUCCESS;
}
