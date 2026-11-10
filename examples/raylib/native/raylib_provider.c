#include "raylib_provider.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(RAYLIB_VERSION_MAJOR == 6 && RAYLIB_VERSION_MINOR == 0 && RAYLIB_VERSION_PATCH == 0,
               "Use the qualified raylib 6.0 headers");
_Static_assert(sizeof(Color) == 4 && _Alignof(Color) == 1 && offsetof(Color, a) == 3, "raylib Color");
_Static_assert(sizeof(Vector2) == 8 && _Alignof(Vector2) == 4 && offsetof(Vector2, y) == 4, "raylib Vector2");
_Static_assert(sizeof(Image) == 24 && _Alignof(Image) == 8 && offsetof(Image, width) == 8 &&
               offsetof(Image, format) == 20, "raylib Image");
_Static_assert(sizeof(Texture2D) == 20 && _Alignof(Texture2D) == 4 &&
               offsetof(Texture2D, width) == 4 && offsetof(Texture2D, format) == 16, "raylib Texture2D");

struct VoidRaylibWindow { Texture2D sprite; };

enum { title_capacity = 256 };
static _Thread_local char pending_title[title_capacity];
static atomic_flag window_open = ATOMIC_FLAG_INIT;
// raylib stores the pointer in its global window state, even on failed init.
static char window_title[title_capacity];

struct TraceCall {
    VoidRaylibTrace callback;
    void* context;
    int delivered;
};
static _Thread_local struct TraceCall* trace_call;
static atomic_flag trace_active = ATOMIC_FLAG_INIT;

static void raylib_trace(int level, const char* format, va_list args) {
    struct TraceCall* call = trace_call;
    if (!call || call->delivered) return;
    char message[128];
    va_list copied;
    va_copy(copied, args);
    const int count = vsnprintf(message, sizeof message, format, copied);
    va_end(copied);
    if (count < 0 || (size_t)count >= sizeof message) {
        call->delivered = -1;
        return;
    }
    uint32_t digest = UINT32_C(2166136261);
    for (int i = 0; i < count; ++i) {
        digest ^= (unsigned char)message[i];
        digest *= UINT32_C(16777619);
    }
    call->delivered = 1;
    call->callback(call->context, level, (uint32_t)count, digest);
}

int vd_raylib_trace(VoidRaylibTrace callback, void* context) {
    // Gallery startup owns all raylib calls and has no window/audio workers.
    // raylib's global setter is used only within this synchronous scope.
    if (atomic_flag_test_and_set(&trace_active)) return -1;
    struct TraceCall call = {callback, context, 0};
    trace_call = &call;
    SetTraceLogCallback(raylib_trace);
    TraceLog(LOG_WARNING, "VOID G5 trace %d %s", 7, "raylib");
    SetTraceLogCallback(NULL);
    trace_call = NULL;
    atomic_flag_clear(&trace_active);
    return call.delivered == 1 ? 0 : -1;
}

Color vd_raylib_tint(Color color, float factor) {
    return ColorBrightness(color, factor);
}

static bool valid_title(const uint8_t* bytes, size_t size) {
    for (size_t i = 0; i < size;) {
        uint8_t lead = bytes[i++];
        if (!lead) return false;
        if (lead < 0x80) continue;
        if (lead < 0xc2 || lead > 0xf4) return false;
        unsigned more = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
        if (more > size - i) return false;
        uint8_t second = bytes[i];
        if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second >= 0xa0) ||
            (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second >= 0x90)) return false;
        while (more--) if ((bytes[i++] & 0xc0) != 0x80) return false;
    }
    return true;
}

int64_t vd_raylib_title(const uint8_t* bytes, size_t offset, size_t size) {
    (void)offset;
    pending_title[0] = '\0';
    if (!bytes || !size || size >= title_capacity || !valid_title(bytes, size)) return -1;
    memcpy(pending_title, bytes, size);
    pending_title[size] = '\0';
    return (int64_t)size;
}

VoidRaylibWindow* vd_raylib_open(int width, int height) {
    if (!pending_title[0] || width < 320 || height < 240 || width > 1600 || height > 1200) return NULL;
    if (atomic_flag_test_and_set(&window_open)) return NULL;
    memcpy(window_title, pending_title, strlen(pending_title) + 1);
    pending_title[0] = '\0';
    VoidRaylibWindow* window = malloc(sizeof *window);
    if (window) {
        InitWindow(width, height, window_title);
        if (IsWindowReady()) {
            Image image = GenImageColor(24, 24, SKYBLUE);
            window->sprite = LoadTextureFromImage(image);
            UnloadImage(image);
            if (IsTextureValid(window->sprite)) {
                SetTargetFPS(60);
                return window;
            }
            CloseWindow();
        }
        free(window);
    }
    atomic_flag_clear(&window_open);
    return NULL;
}

bool vd_raylib_should_close(const VoidRaylibWindow* window) {
    (void)window;
    return WindowShouldClose();
}

int vd_raylib_direction(const VoidRaylibWindow* window) {
    (void)window;
    return (int)IsKeyDown(KEY_RIGHT) - (int)IsKeyDown(KEY_LEFT);
}

int vd_raylib_draw(VoidRaylibWindow* window, int x, int frame, uint8_t r, uint8_t g, uint8_t b) {
    BeginDrawing();
    ClearBackground((Color){r, g, b, 255});
    DrawRectangle(20, 75, 600, 205, (Color){34, 48, 74, 255});
    DrawRectangle(x, 153, 62, 43, ORANGE);
    DrawCircle(60 + frame % 500, 229, 17, LIME);
    DrawTextureV(window->sprite, (Vector2){(float)x + 19, 162}, WHITE);
    DrawCircleV(GetMousePosition(), 8, PINK);
    DrawText("VOID / raylib 6.0", 22, 23, 24, RAYWHITE);
    DrawText("Left / right to move. Escape or close to quit.", 22, 310, 18, RAYWHITE);
    EndDrawing();
    return 0;
}

int32_t vd_raylib_close(VoidRaylibWindow* window) {
    UnloadTexture(window->sprite);
    CloseWindow();
    const bool closed = !IsWindowReady();
    free(window);
    atomic_flag_clear(&window_open);
    if (closed) {
        puts("RAYLIB owner released texture and window");
        fflush(stdout);
    }
    return closed ? 0 : -1;
}
