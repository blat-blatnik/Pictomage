#include "basic.h"

Texture2D texture;

void GameInit(void)
{
    InitWindow(900, 450, "Raylib Test");
    texture = LoadTexture("res/test.png");
}
void GameLoopOneIteration(void)
{
    Vector3 x = { 0 };
    Vector3Barycenter(x, x, x, x);

    BeginDrawing();
    {
        ClearBackground(RAYWHITE);

        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
        static bool active = false;
        active = GuiToggle((Rectangle) { .x = 50, .y = 50, .width = 100, .height = 30 }, "Test Button", active);
        if (active)
            DrawText("You are pushing the button!", 190, 250, 20, LIGHTGRAY);

        DrawTexture(texture, 0, 300, WHITE);
    }
    EndDrawing();
}