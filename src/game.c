#include "basic.h"

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 600
#define FPS 60
#define DELTA_TIME (1.0f/FPS)

typedef struct Player
{
    Vector2 pos;
} Player;

Player player = {
    .pos = { SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f }
};

void GameInit(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Raylib Test");
    SetTargetFPS(FPS);
    InitAudioDevice();
}
void GameLoopOneIteration(void)
{
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
        const float playerSpeed = 1000;
        playerMove = Vector2Normalize(playerMove);
        playerMove = Vector2Scale(playerMove, playerSpeed * DELTA_TIME);
        player.pos = Vector2Add(player.pos, playerMove);
    }

    BeginDrawing();
    {
        ClearBackground(RAYWHITE);
        rlPushMatrix();
        {
            Color c = RAYWHITE;
            rlDisableBackfaceCulling();
            rlMatrixMode(RL_PROJECTION);
            rlLoadIdentity();
            Matrix m = rlGetMatrixProjection();
            rlOrtho(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, 0.001, 1000);
            m = rlGetMatrixProjection();
            DrawRectangleV(Vec2(-0.5, -0.5), Vec2(+1, +1), BLACK);
            m = rlGetMatrixProjection();
            int x = 123;
        }
        rlPopMatrix();
    }
    EndDrawing();
}