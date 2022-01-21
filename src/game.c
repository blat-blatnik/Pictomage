#include "basic.h"

Texture2D texture;

#include <time.h>
void GameInit(void)
{
    Random r = SeedRandom(time(NULL));
    float one_minus_epsilon = nextafterf(1, 0);
    u64 total = 0;
    u64 zero_count = 0;
    u64 one_count = 0;
    while (true)
    {
        ++total;
        float f = RandomFloat01(&r);
        ASSERT(f >= 0.0f && f < 1.0f);
        if (f == 0.0f)
        {
            zero_count++;
            printf("0 ~ every %g calls\n", total / (double)zero_count);
        }
        else if (f == one_minus_epsilon)
        {
            one_count++;
            printf("1 ~ every %g calls\n", total / (double)one_count);
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