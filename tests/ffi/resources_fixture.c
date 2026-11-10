#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>

typedef struct G3Box {
    int32_t value;
    int32_t release_status;
} G3Box;

/* Test observations only. Real ownership belongs to the Runtime ledger. */
static atomic_int live_count;
static atomic_int release_count;

const char* g3_fixture_identity(void) { return G3_FIXTURE_IDENTITY; }
G3Box* g3_create(int32_t value) {
    if (value == -1) return NULL;
    G3Box* box = malloc(sizeof *box);
    if (box) {
        box->value = value;
        box->release_status = value == -2 ? 17 : 0;
        atomic_fetch_add(&live_count, 1);
    }
    return box;
}
int32_t g3_read(const G3Box* box, int32_t control) {
    return control ? -19 : box->value;
}
int32_t g3_add(G3Box* box, int32_t delta) {
    const int64_t value = (int64_t)box->value + delta;
    if (value < INT32_MIN || value > INT32_MAX) return -23;
    box->value = (int32_t)value;
    return box->value;
}
int32_t g3_close(G3Box* box) {
    const int32_t status = box->release_status;
    free(box);
    atomic_fetch_add(&release_count, 1);
    atomic_fetch_sub(&live_count, 1);
    return status;
}
int32_t g3_live(void) { return atomic_load(&live_count); }
int32_t g3_releases(void) { return atomic_load(&release_count); }
