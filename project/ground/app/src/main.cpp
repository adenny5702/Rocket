#include "raylib.h"
#include <iostream>

int main() {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenHeight, screenHeight, "window");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawText("window", 190, 200, 20, LIGHTGRAY);

        EndDrawing();
    }

    CloseWindow();

    std::cout << "Hello world" << std::endl;
    return 0;
}