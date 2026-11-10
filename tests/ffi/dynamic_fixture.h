#pragma once
#include <stddef.h>
#include <stdint.h>

#define G4_EXPORT __attribute__((visibility("default")))

typedef struct G4Sample {
    int32_t tag;
    double reading;
} G4Sample;
typedef struct G4Box G4Box;

G4_EXPORT int32_t g4_bias(void);
G4_EXPORT int32_t g4_add(int32_t, int32_t);
G4_EXPORT G4Sample g4_sample(G4Sample);
G4_EXPORT int64_t g4_fill(uint8_t*, size_t, size_t, uint8_t);
G4_EXPORT G4Box* g4_create(int32_t);
G4_EXPORT int32_t g4_read(const G4Box*, int32_t);
G4_EXPORT int32_t g4_close(G4Box*);
G4_EXPORT int32_t g4_live(void);
G4_EXPORT int32_t g4_releases(void);
G4_EXPORT int32_t g4_gate(int32_t);
