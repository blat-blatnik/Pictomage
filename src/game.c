#include "basic.h"

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 900
#define FPS 60
#define DELTA_TIME (1.0f/FPS)
#define MAX_BULLETS 100
#define MAX_TURRETS 100

#define PLAYER_SPEED 400.0f
#define PLAYER_RADIUS 20.0f
#define BULLET_SPEED 1000.0f
#define BULLET_RADIUS 5.0f
#define TURRET_RADIUS 25.0f
#define TURRET_TURN_SPEED (25.0f*DEG2RAD)
#define TURRET_FIRE_RATE 1.5f
#define PLAYER_CAPTURE_CONE_HALF_ANGLE (40.0f*DEG2RAD)
#define PLAYER_CAPTURE_CONE_RADIUS 150.0f

typedef struct Player
{
	Vector2 pos;
	Vector2 vel;
	float lookAngle; // In radians.
	bool justCaptured; // True for the first game step when the player captures (for drawing the flash).
	bool hasCapture;
	bool isReleasingCapture; // True while the button is held down, before the capture is released.
	Vector2 releasePos;
} Player;

typedef struct Bullet
{
	Vector2 origin;
	Vector2 pos;
	Vector2 vel;
} Bullet;

typedef struct Turret
{
	Vector2 pos;
	float lookAngle;
	int framesUntilShoot;
} Turret;

bool paused = false;
bool devMode = true;
const Rectangle screenRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };

Player player;
Random rng;

int numBullets = 0;
Bullet bullets[MAX_BULLETS];
int numCapturedBullets = 0;
Bullet capturedBullets[MAX_BULLETS];

int numTurrets = 0;
Turret turrets[MAX_TURRETS];

Sound flashSound;
Sound longShotSound;

Vector2 GetMousePosWithInvertedY(void)
{
	Vector2 p = GetMousePosition();
	return Vec2(p.x, SCREEN_HEIGHT - p.y - 1);
}

int SpawnBullet(Vector2 pos, Vector2 vel)
{
	if (numBullets >= MAX_BULLETS)
		return -1;

	int index = numBullets++;
	bullets[index].origin = pos;
	bullets[index].pos = pos;
	bullets[index].vel = vel;
	return index;
}
int SpawnTurret(Vector2 pos, float lookAngleRadians)
{
	if (numTurrets >= MAX_TURRETS)
		return -1;

	int index = numTurrets++;
	turrets[index].pos = pos;
	turrets[index].lookAngle = lookAngleRadians;
	turrets[index].framesUntilShoot = 0;
	return index;
}
void RemoveBulletFromGlobalList(int index)
{
	ASSERT(index < numBullets);
	SwapMemory(bullets + index, bullets + numBullets - 1, sizeof bullets[0]);
	--numBullets;
}
void RemoveTurretFromGlobalList(int index)
{
	ASSERT(index < numTurrets);
	SwapMemory(turrets + index, turrets + numTurrets - 1, sizeof turrets[0]);
	--numTurrets;
}

void GameInit(void)
{
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Snapper");
	SetTargetFPS(FPS);
	InitAudioDevice();
	rlDisableBackfaceCulling(); // It's a 2D game we don't need this..
	rlDisableDepthTest();

	rng = SeedRandom(time(NULL));

	flashSound = LoadSound("res/snap.wav");
	longShotSound = LoadSound("res/long-shot.wav");
	SetSoundVolume(longShotSound, 0.1f);

	player.pos.x = SCREEN_WIDTH / 2.f;
	player.pos.y = SCREEN_HEIGHT / 2.f;
	SpawnTurret(Vec2(50, SCREEN_HEIGHT - 50), 0);
	SpawnTurret(Vec2(SCREEN_WIDTH - 50, SCREEN_HEIGHT - 50), 0);
	SpawnTurret(Vec2(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 100), 0);
}
void Update(void)
{
	// Update player
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
			player.vel = Vector2Normalize(playerMove);
			player.vel = Vector2Scale(player.vel, PLAYER_SPEED);
			player.pos = Vector2Add(player.pos, Vector2Scale(player.vel, DELTA_TIME));
		}

		Vector2 mousePos = GetMousePosWithInvertedY();
		player.lookAngle = AngleBetween(player.pos, mousePos);
		player.justCaptured = false; // Reset from previous frame.

		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			if (!player.hasCapture)
			{
				for (int i = 0; i < numBullets; ++i)
				{
					Bullet *b = &bullets[i];
					if (CheckCollisionConeCircle(player.pos,
						PLAYER_CAPTURE_CONE_RADIUS,
						player.lookAngle - PLAYER_CAPTURE_CONE_HALF_ANGLE,
						player.lookAngle + PLAYER_CAPTURE_CONE_HALF_ANGLE,
						b->pos, BULLET_RADIUS))
					{
						int index = numCapturedBullets++;
						capturedBullets[index] = *b;
						RemoveBulletFromGlobalList(i);
						--i;
					}
				}

				int numCapturedItems = numCapturedBullets;
				if (numCapturedItems != 0)
				{
					Vector2 captureCenter = Vector2Zero();
					for (int i = 0; i < numCapturedBullets; ++i)
					{
						captureCenter = Vector2Add(captureCenter, capturedBullets[i].pos);
					}
					captureCenter = Vector2Scale(captureCenter, 1.0f / numCapturedItems);

					for (int i = 0; i < numCapturedBullets; ++i)
						capturedBullets[i].pos = Vector2Subtract(capturedBullets[i].pos, captureCenter);

					player.hasCapture = true;
					player.justCaptured = true;
				}
			}
			else
			{
				player.releasePos = GetMousePosWithInvertedY();
				player.isReleasingCapture = true;
			}
		}

		if (player.isReleasingCapture && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
		{
			player.isReleasingCapture = false;
			player.hasCapture = false;

			Vector2 mousePos = GetMousePosWithInvertedY();
			Vector2 releaseDir = Vector2Subtract(mousePos, player.releasePos);
			float releaseSpeed = Vector2Length(releaseDir);
			releaseDir = Vector2Scale(releaseDir, 1 / releaseSpeed);

			Vector2 bulletReleaseVel = Vector2Scale(releaseDir, fmaxf(5 * releaseSpeed, 200));
			for (int i = 0; i < numCapturedBullets; ++i)
			{
				Bullet b = capturedBullets[i];
				Vector2 pos = Vector2Add(b.pos, player.releasePos);
				SpawnBullet(pos, bulletReleaseVel);
			}
			numCapturedBullets = 0;
		}
	}

	for (int i = 0; i < numBullets; ++i)
	{
		Bullet *b = &bullets[i];
		b->pos = Vector2Add(b->pos, Vector2Scale(b->vel, DELTA_TIME));
		if (!CheckCollisionCircleRec(b->pos, BULLET_RADIUS, ExpandRectangle(screenRect, 1000)))
		{
			RemoveBulletFromGlobalList(i);
			--i;
		}
		else
		{
			for (int j = 0; j < numTurrets; ++j)
			{
				Turret *t = &turrets[j];
				if (CheckCollisionCircles(b->pos, BULLET_RADIUS, t->pos, TURRET_RADIUS))
				{
					RemoveBulletFromGlobalList(i);
					RemoveTurretFromGlobalList(j);
					--i;
					break;
				}
			}
		}
	}

	for (int i = 0; i < numTurrets; ++i)
	{
		Turret *t = &turrets[i];
		float angleToPlayer = AngleBetween(t->pos, player.pos);
		float dAngle = WrapMinMax(angleToPlayer - t->lookAngle, -PI, +PI);

		if (fabsf(dAngle) < 15 * DEG2RAD)
		{
			t->framesUntilShoot--;
			if (t->framesUntilShoot <= 0)
			{
				t->framesUntilShoot = (int)(FPS / TURRET_FIRE_RATE); // (Frame/sec) / (Shots/sec) = Frame/Shot
				float s = sinf(t->lookAngle);
				float c = cosf(t->lookAngle);
				Vector2 bulletPos = {
					t->pos.x + (TURRET_RADIUS + 15) * c,
					t->pos.y + (TURRET_RADIUS + 15) * s
				};
				Vector2 bulletVel = {
					BULLET_SPEED * c,
					BULLET_SPEED * s,
				};
				SpawnBullet(bulletPos, bulletVel);
				PlaySound(longShotSound);
				SetSoundPitch(longShotSound, RandomFloat(&rng, 0.95f, 1.2f));
			}
		}
		else
			t->framesUntilShoot = (int)(FPS / TURRET_FIRE_RATE); // (Frame/sec) / (Shots/sec) = Frame/Shot

		float turnSpeed = TURRET_TURN_SPEED;
		if (dAngle < 0)
			turnSpeed *= -1;
		t->lookAngle += turnSpeed * DELTA_TIME;
	}
}
void Draw(void)
{
	BeginDrawing();
	{
		ClearBackground(RAYWHITE);
		rlMatrixMode(RL_PROJECTION);

		rlLoadIdentity();
		rlOrtho(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, 0, 1);
		{
			DrawCircleV(player.pos, PLAYER_RADIUS, DARKGREEN);

			for (int i = 0; i < numTurrets; ++i)
			{
				Turret t = turrets[i];
				DrawCircleV(t.pos, TURRET_RADIUS, BLACK);
				DrawCircleV(t.pos, TURRET_RADIUS - 5, DARKGRAY);
				float lookAngleDegrees = RAD2DEG * t.lookAngle;
				Rectangle gunBarrel = { t.pos.x, t.pos.y, TURRET_RADIUS + 10, 12 };
				DrawRectanglePro(gunBarrel, Vec2(-5, +6), lookAngleDegrees, MAROON);
				DrawCircleV(Vec2(gunBarrel.x, gunBarrel.y), 2, ORANGE);
			}

			Color bulletTrail0 = ColorAlpha(DARKGRAY, 0);
			Color bulletTrail1 = ColorAlpha(DARKGRAY, 0.2);
			for (int i = 0; i < numBullets; ++i)
			{
				Bullet b = bullets[i];
				Vector2 perp1 = Vector2Scale(Vector2Normalize(Vec2(-b.vel.y, +b.vel.x)), BULLET_RADIUS);
				Vector2 perp2 = Vector2Scale(Vector2Normalize(Vec2(+b.vel.y, -b.vel.x)), BULLET_RADIUS);
				Vector2 toOrigin = Vector2Subtract(b.origin, b.pos);
				Vector2 perp3 = Vector2Scale(Vector2Normalize(toOrigin), 400);
				if (Vector2LengthSqr(perp3) > Vector2LengthSqr(toOrigin))
					perp3 = toOrigin;
				Vector2 trail1 = Vector2Add(b.pos, perp1);
				Vector2 trail2 = Vector2Add(b.pos, perp2);
				Vector2 trail3 = Vector2Add(b.pos, perp3);
				rlBegin(RL_TRIANGLES);
				{
					rlColor(bulletTrail1);
					rlVertex2fv(trail1);
					rlVertex2fv(trail2);
					rlColor(bulletTrail0);
					rlVertex2fv(trail3);
				}
				rlEnd();
				DrawCircleV(b.pos, BULLET_RADIUS, DARKGRAY);
			}

			// Draw Player
			{
				float lookAngleDegrees = (RAD2DEG * (-player.lookAngle)) + 90; // No idea why RAD2DEG*radians isn't enough here.. whatever.
				if (player.justCaptured)
				{
					DrawCircleSector(player.pos, 
						PLAYER_CAPTURE_CONE_RADIUS + 10, 
						lookAngleDegrees - RAD2DEG * PLAYER_CAPTURE_CONE_HALF_ANGLE - 1, 
						lookAngleDegrees + RAD2DEG * PLAYER_CAPTURE_CONE_HALF_ANGLE + 1, 
						12, 
						ColorAlpha(ORANGE, 0.9f));
					PlaySound(flashSound);
				}
				else if (!player.hasCapture)
				{
					DrawCircleSector(player.pos,
						PLAYER_CAPTURE_CONE_RADIUS,
						lookAngleDegrees - RAD2DEG * PLAYER_CAPTURE_CONE_HALF_ANGLE,
						lookAngleDegrees + RAD2DEG * PLAYER_CAPTURE_CONE_HALF_ANGLE,
						12,
						ColorAlpha(GRAY, 0.1f));
				}
			}

			if (player.isReleasingCapture)
			{
				for (int i = 0; i < numCapturedBullets; ++i)
				{
					Bullet b = capturedBullets[i];
					Vector2 pos = Vector2Add(b.pos, player.releasePos);
					DrawCircleV(pos, BULLET_RADIUS, ColorAlpha(BLUE, 0.5f));
				}

				Vector2 mousePos = GetMousePosWithInvertedY();
				Vector2 arrowDir = Vector2Normalize(Vector2Subtract(mousePos, player.releasePos));
				Vector2 arrowPos0 = Vector2Add(player.releasePos, Vector2Scale(arrowDir, BULLET_RADIUS + 10));
				Vector2 arrowPos1 = mousePos;
				float arrowLength = Vector2Distance(arrowPos0, arrowPos1);
				float arrowWidth0 = 5;
				float arrowWidth1 = Clamp(0.2f * arrowLength, 5, 30);
				Vector2 dir1 = { -arrowDir.y, +arrowDir.x };
				Vector2 dir2 = { +arrowDir.y, -arrowDir.x };
				Color arrowColor = ColorAlpha(BLUE, 0.3f);
				rlBegin(RL_QUADS);
				{
					rlColor(arrowColor);
					rlVertex2fv(Vector2Add(arrowPos0, Vector2Scale(dir1, arrowWidth0)));
					rlVertex2fv(Vector2Add(arrowPos0, Vector2Scale(dir2, arrowWidth0)));
					rlVertex2fv(Vector2Add(arrowPos1, Vector2Scale(dir2, arrowWidth1)));
					rlVertex2fv(Vector2Add(arrowPos1, Vector2Scale(dir1, arrowWidth1)));
				}
				rlEnd();
				float arrowheadWidth = 2 * arrowWidth1;
				float arrowheadLength = 1.5f * arrowWidth1;
				DrawTriangle(
					Vector2Add(arrowPos1, Vector2Scale(dir1, arrowheadWidth)),
					Vector2Add(arrowPos1, Vector2Scale(dir2, arrowheadWidth)),
					Vector2Add(arrowPos1, Vector2Scale(arrowDir, arrowheadLength)),
					arrowColor);
			}
		}
		rlDrawRenderBatchActive();

		rlLoadIdentity();
		rlOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 1);
		{
			//DrawDebugText("%.2f", turrets[0].lookAngle / (2 * PI));
			//DrawDebugText("%.2f", player.lookAngle / (2*PI));
			//if (devMode)
			//{
			//	float configWidth = 300;
			//	float configHeight = 200;
			//	float configX = SCREEN_WIDTH - configWidth;
			//	float configY = SCREEN_HEIGHT - configHeight;
			//	if (GuiWindowBox(Rect(configX, configY, configWidth, configHeight), "Config"))
			//		devMode = false;
			//
			//	config.playerRadius = GuiSlider(
			//		Rect(configX + 20, configY + 40, 150, 15), "", "Player radius", config.playerRadius, 5, 50);
			//	config.playerSpeed = GuiSlider(
			//		Rect(configX + 20, configY + 60, 150, 15), "", "Player speed", config.playerSpeed, 100, 1000);
			//	config.bulletRadius = GuiSlider(
			//		Rect(configX + 20, configY + 80, 150, 15), "", "Bullet radius", config.bulletRadius, 1, 20);
			//	config.bulletSpeed = GuiSlider(
			//		Rect(configX + 20, configY + 100, 150, 15), "", "Bullet speed", config.bulletSpeed, 100, 1000);
			//}
		}
		rlDrawRenderBatchActive();
	}
	EndDrawing();
}
void GameLoopOneIteration(void)
{
	TempReset();

	if (IsKeyPressed(KEY_GRAVE))
		devMode = !devMode;
	if (IsKeyPressed(KEY_SPACE))
		paused = !paused;

	if (paused)
	{
		if (IsKeyPressed(KEY_PERIOD))
			Update();
	}
	else Update();

	Draw();
}