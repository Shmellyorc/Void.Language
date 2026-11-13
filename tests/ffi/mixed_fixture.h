#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#define MIXED_EXPORT __declspec(dllexport)
#else
#define MIXED_EXPORT __attribute__((visibility("default")))
#endif
typedef struct MixedBox MixedBox;
typedef struct MixedPair { int32_t x; float y; } MixedPair;
typedef struct MixedBundle { MixedPair pairs[2]; char label[4]; } MixedBundle;
MIXED_EXPORT MixedBox* mixed_create(int32_t);
MIXED_EXPORT MixedBox* mixed_clone(const MixedBox*, MixedPair);
MIXED_EXPORT MixedBox* mixed_bytes(const uint8_t*, size_t, size_t);
MIXED_EXPORT MixedPair mixed_combine(const MixedBox*, const MixedBox*, MixedPair);
MIXED_EXPORT int64_t mixed_read(const MixedBox*, uint8_t*, size_t, size_t);
MIXED_EXPORT int32_t mixed_close(MixedBox*);
MIXED_EXPORT MixedBundle mixed_bundle(MixedBundle);
