#include "lib/raylib.h"
#include "lib/raygui.h"

void game_init(void)
{
    InitWindow(900, 450, "Raylib Test");
}
void game_loop_one_iteration(void)
{
    BeginDrawing();
    {
        ClearBackground(RAYWHITE);
        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
        static bool active = false;
        if ((active = GuiToggle((Rectangle) { .x = 50, .y = 50, .width = 100, .height = 30 }, "Test Button", active)))
            DrawText("You are pushing the button!", 190, 250, 20, LIGHTGRAY);
    }
    EndDrawing();
}