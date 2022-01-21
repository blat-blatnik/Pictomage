#include "basic.h"

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 600
#define FPS 60
#define DELTA_TIME (1.0f/FPS)

Sound snapSound;
Vector2 playerPos = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };

void GameInit(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Raylib Test");
    SetTargetFPS(FPS);
    InitAudioDevice();
    rlDisableBackfaceCulling();

    snapSound = LoadSound("res/snap.wav");
}
void GameLoopOneIteration(void)
{
    TempReset();

    Vector2 playerMove = Vec2Broadcast(0);
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        playerMove.y += 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
        playerMove.y -= 1;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
        playerMove.x -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        playerMove.x += 1;
    
    if (playerMove.x != 0 || playerMove.y != 0)
    {
        const float playerSpeed = 400;
        playerMove = Vector2Normalize(playerMove);
        playerMove = Vector2Scale(playerMove, playerSpeed * DELTA_TIME);
        playerPos = Vector2Add(playerPos, playerMove);
    }

    Vector2 mousePos = { GetMouseX(), GetMouseY() };
    Vector2 lookPos = { mousePos.x, SCREEN_HEIGHT - mousePos.y - 1 };
    Vector2 relativeLookPos = Vector2Subtract(lookPos, playerPos);
    float snapAngleRadians = atan2f(relativeLookPos.y, relativeLookPos.x);
    float snapAngleDegrees = RAD2DEG * -snapAngleRadians + 90;

    BeginDrawing();
    {
        ClearBackground(RAYWHITE);
        rlMatrixMode(RL_PROJECTION);

        rlLoadIdentity();
        rlOrtho(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, 0, 1);
        DrawRectangleVCentered(playerPos, Vec2(50, 50), BLACK);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            DrawCircleSector(playerPos, 160, snapAngleDegrees - 41, snapAngleDegrees + 41, 12, ColorAlpha(ORANGE, 0.9f));
            PlaySound(snapSound);
        }
        else
            DrawCircleSector(playerPos, 150, snapAngleDegrees - 40, snapAngleDegrees + 40, 12, ColorAlpha(GRAY, 0.1f));

        rlDrawRenderBatchActive();
        rlLoadIdentity();
        rlOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 1);
        DrawDebugText("%.1f, %.1f", lookPos.x, lookPos.y);
    }
    EndDrawing();
}