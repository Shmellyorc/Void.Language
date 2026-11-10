#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifdef G4_PROVIDER
#    define G4_API __declspec(dllexport)
#  else
#    define G4_API __declspec(dllimport)
#  endif
#else
#  define G4_API __attribute__((visibility("default")))
#endif

typedef struct ConventionalBox ConventionalBox;
typedef struct { int32_t tag; double value; } ConventionalSample;
G4_API int g4_conventional_add(int, int);
G4_API double g4_conventional_scale(double, float);
G4_API long g4_conventional_long(long);
G4_API bool g4_conventional_flip(bool);
G4_API ConventionalSample g4_conventional_sample(ConventionalSample);
G4_API int64_t g4_conventional_sum(const uint8_t*, size_t, size_t);
G4_API int64_t g4_conventional_fill(uint8_t*, size_t, size_t, uint8_t);
G4_API int64_t g4_conventional_shift(uint8_t*, size_t, size_t);
G4_API ConventionalBox* g4_conventional_acquire(int32_t);
G4_API int32_t g4_conventional_borrow(const ConventionalBox*);
G4_API int32_t g4_conventional_close(ConventionalBox*);
G4_API void g4_conventional_ping(void);
G4_API int32_t g4_conventional_pings(void);
G4_API ConventionalBox* g4_conventional_last(void);
