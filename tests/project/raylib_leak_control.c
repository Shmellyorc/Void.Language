#include <raylib.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--no-window") == 0) return 0;
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(160, 120, "G24 native C allocation control");
    if (!IsWindowReady()) return 2;
    for (int i = 0; i < 3; ++i) {
        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return 3;
    char line[4096];
    while (fgets(line, sizeof line, maps))
        if (strstr(line, "gallium") || strstr(line, "raylib")) fputs(line, stderr);
    fclose(maps);
    CloseWindow();
    fputs("native control: CloseWindow completed\n", stderr);
    return 0;
}
