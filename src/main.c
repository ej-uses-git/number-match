#include <raylib.h>
#include <stdio.h>

int main(void) {
    InitWindow(800, 450, "Hello, Raylib!");

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(WHITE);
        }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
