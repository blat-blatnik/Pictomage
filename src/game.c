#include "basic.h"

Texture2D texture;

void GameInit(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(900, 600, "Raylib Test");
    texture = LoadTexture("res/test.png");
}
void GameLoopOneIteration(void)
{
    BeginDrawing();
    {
        ClearBackground(RAYWHITE);

        rlBegin(RL_TRIANGLES);
        {
            rlColor3f(1, 0, 0); rlVertex2f(+100.f, +100.f);
            rlColor3f(0, 0, 1); rlVertex2f(+300.f, +300.f);
            rlColor3f(0, 1, 0); rlVertex2f(+500.f, +100.f);
        }
        rlEnd();

        static bool active = false;
        active = GuiToggle((Rectangle) { 500, 300, 200, 50 }, active ? "On" : "Off", active);
        if (active)
            DrawText("Hi There", 400, 10, 24, DARKGRAY);

        DrawTexture(texture, 100, 400, WHITE);
        DrawFPS(10, 10);
    }
    EndDrawing();
}