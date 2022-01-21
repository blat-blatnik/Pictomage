#include "basic.h"

Texture2D texture;

#include <time.h>
void GameInit(void)
{
    Random r = SeedRandom(time(NULL));
    u64 bit_counts[20] = {0};
    u64 total_count = 0;
    for (u64 i = 0;; ++i)
    {
        u32 r = (u32)RandomInt(&r, 0, 1 << 20);
        total_count++;
        u32 bit = 1;
        for (int j = 0; j < 20; ++j)
        {
            if (r & bit)
                bit_counts[j]++;
            bit <<= 1;
        }
        if (i % 1000000000 == 0)
        {
            for (int i = 19; i >= 0; --i)
                printf("[%4d] ", i);
            printf("\n");
            for (int i = 19; i >= 0; --i)
                printf("%.4f ", bit_counts[i] / (double)total_count);
            printf("\n\n");
        }
    }

    TraceLog(LOG_INFO, "Done");
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