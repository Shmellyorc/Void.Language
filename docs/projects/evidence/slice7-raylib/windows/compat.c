#include "raylib.h"
#include <stdio.h>

int main(void) {
    InitWindow(480, 320, "raylib 6.0 Windows C control");
    if (!IsWindowReady()) return 2;
    SetTargetFPS(60);
    int frames = 0;
    while (!WindowShouldClose() && frames < 300) {
        BeginDrawing();
        ClearBackground(DARKBLUE);
        DrawRectangle(35 + frames % 300, 90, 90, 64, ORANGE);
        DrawCircle(300, 215, 36, LIME);
        EndDrawing();
        ++frames;
    }
    CloseWindow();
    printf("native C control rendered %d frames, window closed=%d\n", frames, !IsWindowReady());
    return frames > 2 && !IsWindowReady() ? 0 : 3;
}
