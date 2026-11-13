#include "mixed_fixture.h"
#include <stdlib.h>
struct MixedBox { int32_t value; };
MixedBox* mixed_create(int32_t value) {
    MixedBox* box = malloc(sizeof *box);
    if (box) box->value = value;
    return box;
}
MixedBox* mixed_clone(const MixedBox* box, MixedPair pair) { return mixed_create(box->value + pair.x); }
MixedBox* mixed_bytes(const uint8_t* bytes, size_t offset, size_t length) {
    (void)offset;
    return length ? mixed_create(bytes[0]) : NULL;
}
MixedPair mixed_combine(const MixedBox* a, const MixedBox* b, MixedPair pair) {
    return (MixedPair){a->value + b->value + pair.x, pair.y * 2};
}
int64_t mixed_read(const MixedBox* box, uint8_t* bytes, size_t offset, size_t length) {
    (void)offset;
    if (!length) return -1;
    bytes[0] = (uint8_t)box->value;
    return 1;
}
int32_t mixed_close(MixedBox* box) { free(box); return 0; }
MixedBundle mixed_bundle(MixedBundle bundle) {
    MixedPair first=bundle.pairs[0];bundle.pairs[0]=bundle.pairs[1];bundle.pairs[1]=first;
    bundle.label[1]='Z';return bundle;
}
