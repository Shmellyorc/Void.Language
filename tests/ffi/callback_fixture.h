#pragma once
#include <stdint.h>

#if defined(_WIN32)
# if defined(G5_FIXTURE_BUILD)
#  define G5_API __declspec(dllexport)
# else
#  define G5_API __declspec(dllimport)
# endif
#else
# define G5_API __attribute__((visibility("default")))
#endif

typedef int32_t (*G5Int)(void*, int32_t);
typedef double (*G5Float)(void*, double, float);
typedef void (*G5Notify)(void*, int32_t);
typedef struct G5Registration G5Registration;

G5_API int32_t g5_twice(int32_t, G5Int, void*);
G5_API double g5_mix(double, float, G5Float, void*);
G5_API void g5_notify(int32_t, G5Notify, void*);
G5_API G5Registration* g5_register(G5Int, void*);
G5_API int32_t g5_fire(const G5Registration*, int32_t, int32_t);
G5_API int32_t g5_queue(G5Registration*, int32_t);
G5_API int32_t g5_wait(void);
G5_API int32_t g5_unregister(G5Registration*);
G5_API int32_t g5_late(void);
