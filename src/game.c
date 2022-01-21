#include "basic.h"

#define WIDTH 900
#define HEIGHT 600

Texture2D texture;
Sound sound;
Music music;

void GameInit(void)
{
    InitAudioDevice();

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "Raylib Test");

    texture = LoadTexture("res/test.png");
    sound = LoadSound("res/test.wav");
    music = LoadMusicStream("res/test.ogg");

    PlayMusicStream(music);
}
void GameLoopOneIteration(void)
{
    UpdateMusicStream(music);

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

        if (GuiButton((Rectangle) { 500, 200, 200, 40 }, "Play Sound"))
            PlaySound(sound);

        static bool active = false;
        active = GuiToggle((Rectangle) { 500, 250, 200, 40 }, active ? "On" : "Off", active);
        if (active)
            DrawText("Hi There", 400, 10, 24, DARKGRAY);

        DrawTexture(texture, 100, 400, WHITE);
        DrawFPS(10, 10);
    }
    EndDrawing();
}