#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <raylib.h>

#ifdef _WIN32
#define VOID_RAYLIB_EXPORT __declspec(dllexport)
#else
#define VOID_RAYLIB_EXPORT __attribute__((visibility("default")))
#endif

typedef struct VoidRaylibWindow VoidRaylibWindow;
typedef void (*VoidRaylibTrace)(void*, int32_t, uint32_t, uint32_t);
VOID_RAYLIB_EXPORT int vd_raylib_trace(VoidRaylibTrace, void*);
VOID_RAYLIB_EXPORT Color vd_raylib_tint(Color, float);
VOID_RAYLIB_EXPORT int64_t vd_raylib_title(const uint8_t*, size_t, size_t);
VOID_RAYLIB_EXPORT VoidRaylibWindow* vd_raylib_open(int, int);
VOID_RAYLIB_EXPORT bool vd_raylib_should_close(const VoidRaylibWindow*);
VOID_RAYLIB_EXPORT int vd_raylib_direction(const VoidRaylibWindow*);
VOID_RAYLIB_EXPORT int vd_raylib_draw(VoidRaylibWindow*, int, int, uint8_t, uint8_t, uint8_t);
VOID_RAYLIB_EXPORT int32_t vd_raylib_close(VoidRaylibWindow*);

#ifdef VOID_RAYLIB_API_CALLBACKS
bool vd_raylib_scoped_trace_active(void);
VOID_RAYLIB_EXPORT bool vd_raylib_api_trace_registered(void);
#endif
