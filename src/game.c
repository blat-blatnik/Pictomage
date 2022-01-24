#include "basic.h"

// *---===========---*
// |/   Constants   \|
// *---===========---*

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 900
#define FPS 60
#define DELTA_TIME (1.0f/FPS)
#define MAX_BULLETS 100
#define MAX_TURRETS 100
#define TILE_SIZE 30
#define MAX_TILES_X (SCREEN_WIDTH/TILE_SIZE)
#define MAX_TILES_Y (SCREEN_HEIGHT/TILE_SIZE)
#define MAX_DOORS_PER_ROOM 4
#define MAX_ROOM_FILE_NAME 64

#define PLAYER_SPEED 10.0f
#define PLAYER_RADIUS 0.5f
#define BULLET_SPEED 30.0f
#define BULLET_RADIUS 0.2f
#define TURRET_RADIUS 1.0f
#define TURRET_TURN_SPEED (30.0f*DEG2RAD)
#define TURRET_FIRE_RATE 1.5f
#define PLAYER_CAPTURE_CONE_HALF_ANGLE (40.0f*DEG2RAD)
#define PLAYER_CAPTURE_CONE_RADIUS 3.0f

// *---=======---*
// |/   Types   \|
// *---=======---*

typedef enum GameState
{
	GAME_STATE_START_MENU,
	GAME_STATE_PLAYING,
	GAME_STATE_LEVEL_EDITOR,
	GAME_STATE_PAUSED,
	GAME_STATE_GAME_OVER,
	GAME_STATE_SCORE,
	GAME_STATE_CREDITS,
} GameState;

typedef enum Direction
{
	DIRECTION_UP,
	DIRECTION_DOWN,
	DIRECTION_LEFT,
	DIRECTION_RIGHT
} Direction;

typedef enum Tile
{
	TILE_WALL,
	TILE_FLOOR,
} Tile;

typedef enum EditorSelectionKind
{
	EDITOR_SELECTION_KIND_NONE,
	EDITOR_SELECTION_KIND_CONSOLE,
	EDITOR_SELECTION_KIND_TILE,
	EDITOR_SELECTION_KIND_PLAYER,
	EDITOR_SELECTION_KIND_TURRET,
} EditorSelectionKind;

typedef struct Player
{
	Vector2 pos;
	Vector2 vel;
	float lookAngle; // In radians.
	bool justSnapped; // True for the first game step when the player captures (for drawing the flash).
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

typedef struct Room
{
	int numTilesX;
	int numTilesY;
	Tile tiles[MAX_TILES_Y][MAX_TILES_X];

	int numDoors;
	int doorX[MAX_DOORS_PER_ROOM];
	int doorY[MAX_DOORS_PER_ROOM];
	char connections[MAX_DOORS_PER_ROOM][MAX_ROOM_FILE_NAME];
	
	Vector2 playerDefaultPos;
	
	int numTurrets;
	Vector2 turretPos[MAX_TURRETS];
	float turretLookAngle[MAX_TURRETS];
} Room;

typedef struct EditorSelection
{
	EditorSelectionKind kind;
	union
	{
		Tile tile;
		Player player;
		Turret turret;
	};
} EditorSelection;

// *---========---*
// |/   Camera   \|
// *---========---*

float cameraZoom = 1;
Vector2 cameraPos; // In tiles.

void ZoomInToPoint(Vector2 screenPoint, float newZoom)
{
	float s = newZoom / cameraZoom;
	Vector2 p = screenPoint;
	p.y = SCREEN_HEIGHT - p.y - 1;
	p = Vector2Multiply(p, Vec2((float)MAX_TILES_X / SCREEN_WIDTH, (float)MAX_TILES_Y / SCREEN_HEIGHT));
	Vector2 d = Vector2Scale(Vector2Subtract(p, cameraPos), 1 - s);
	cameraPos = Vector2Add(cameraPos, d);
	cameraZoom = newZoom;
}

// *---=========---*
// |/   Globals   \|
// *---=========---*

const bool devMode = true; //@TODO: Disable this for release.
const Vector2 screenCenter = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
const Rectangle screenRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
const Rectangle screenRectTiles = { 0, 0, MAX_TILES_X, MAX_TILES_Y };
GameState gameState = GAME_STATE_START_MENU;
Random rng;
Sound flashSound;
Sound longShotSound;
Player player;
int numBullets;
int numCapturedBullets;
Bullet bullets[MAX_BULLETS];
Bullet capturedBullets[MAX_BULLETS];
int numTurrets;
Turret turrets[MAX_TURRETS];

float PixelsToTiles(float pixels)
{
	return pixels / TILE_SIZE;
}
Vector2 PixelsToTiles2(float pixelsX, float pixelsY)
{
	Vector2 result = {
		PixelsToTiles(pixelsX),
		PixelsToTiles(pixelsY),
	};
	return result;
}
Vector2 ScreenToTile(Vector2 screenPos)
{
	screenPos.y = SCREEN_HEIGHT - screenPos.y - 1;
	Vector2 p = Vector2Multiply(screenPos, Vec2((float)MAX_TILES_X / SCREEN_WIDTH, (float)MAX_TILES_Y / SCREEN_HEIGHT));
	p = Vector2Subtract(p, cameraPos);
	return Vector2Scale(p, 1 / cameraZoom);
}
Vector2 TileToScreen(Vector2 tilePos)
{
	Vector2 p = Vector2Scale(tilePos, cameraZoom);
	p = Vector2Add(p, cameraPos);
	p = Vector2Multiply(p, Vec2(SCREEN_WIDTH / (float)MAX_TILES_X, SCREEN_HEIGHT / (float)MAX_TILES_Y));
	p.y = SCREEN_HEIGHT - p.y - 1;
	return p;
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

// *---=======---*
// |/   Level   \|
// *---=======---*

Room currentRoom;

bool LoadRoom(Room *room, const char *filename)
{
	FILE *file = fopen(filename, "rb");
	if (!file)
	{
		TraceLog(LOG_ERROR, "Unable to load room from file '%s'. The file might not exist.", filename);
		return false;
	}

	u64 flags = 0;
	u8 numTilesX = 1;
	u8 numTilesY = 1;
	u8 tiles[MAX_TILES_Y][MAX_TILES_X] = { { TILE_FLOOR } };
	Vector2 playerDefaultPos = Vec2(0.5f, 0.5f);
	u8 numDoors = 0;
	char connections[MAX_DOORS_PER_ROOM][MAX_ROOM_FILE_NAME] = { 0 };
	u8 doorX[MAX_DOORS_PER_ROOM] = { 0 };
	u8 doorY[MAX_DOORS_PER_ROOM] = { 0 };
	u8 numTurrets = 0;
	Vector2 turretPos[MAX_TURRETS] = { 0 };
	float turretLookAngle[MAX_TURRETS] = { 0 };

	fread(&flags, sizeof flags, 1, file);
	fread(&numTilesX, sizeof numTilesX, 1, file);
	fread(&numTilesY, sizeof numTilesY, 1, file);
	for (u8 y = 0; y < numTilesY; ++y)
		fread(tiles[y], sizeof tiles[0][0], numTilesX, file);
	fread(&playerDefaultPos, sizeof playerDefaultPos, 1, file);
	fread(&numDoors, sizeof numDoors, 1, file);
	fread(connections, sizeof connections[0], numDoors, file);
	fread(doorX, sizeof doorX[0], numDoors, file);
	fread(doorY, sizeof doorY[0], numDoors, file);
	fread(&numTurrets, sizeof numTurrets, 1, file);
	fread(turretPos, sizeof turretPos[0], numTurrets, file);
	fread(turretLookAngle, sizeof turretLookAngle[0], numTurrets, file);

	room->numTilesX = (int)numTilesX;
	room->numTilesY = (int)numTilesY;
	for (u8 y = 0; y < numTilesY; ++y)
		for (u8 x = 0; x < numTilesX; ++x)
			room->tiles[y][x] = (Tile)tiles[y][x];
	room->playerDefaultPos = playerDefaultPos;
	room->numDoors = (int)numDoors;
	memcpy(room->connections, connections, sizeof connections);
	for (u8 i = 0; i < numDoors; ++i)
	{
		room->doorX[i] = (int)doorX[i];
		room->doorY[i] = (int)doorY[i];
	}
	room->numTurrets = (int)numTurrets;
	memcpy(room->turretPos, turretPos, sizeof turretPos);
	memcpy(room->turretLookAngle, turretLookAngle, sizeof turretLookAngle);

	return true;
}
void SaveRoom(const Room *room, const char *filename)
{
	FILE *file = fopen(filename, "wb");
	if (!file)
	{
		TraceLog(LOG_ERROR, "Failed to save room because couldn't open file '%s'.", filename);
		return;
	}

	u64 flags = 0;
	fwrite(&flags, sizeof flags, 1, file);

	u8 numTilesX = (u8)room->numTilesX;
	u8 numTilesY = (u8)room->numTilesY;
	fwrite(&numTilesX, sizeof numTilesX, 1, file);
	fwrite(&numTilesY, sizeof numTilesY, 1, file);

	for (u8 y = 0; y < numTilesY; ++y)
	{
		for (u8 x = 0; x < numTilesX; ++x)
		{
			u8 tile = (u8)room->tiles[y][x];
			fwrite(&tile, sizeof tile, 1, file);
		}
	}

	fwrite(&room->playerDefaultPos, sizeof room->playerDefaultPos, 1, file);

	u8 numDoors = room->numDoors;
	fwrite(&numDoors, sizeof numDoors, 1, file);
	fwrite(room->connections, sizeof room->connections[0], numDoors, file);
	for (u8 i = 0; i < numDoors; ++i)
	{
		u8 doorX = (u8)room->doorX[i];
		fwrite(&doorX, sizeof doorX, 1, file);
	}
	for (u8 i = 0; i < numDoors; ++i)
	{
		u8 doorY = (u8)room->doorY[i];
		fwrite(&doorY, sizeof doorY, 1, file);
	}

	u8 numTurrets = room->numTurrets;
	fwrite(&numTurrets, sizeof numTurrets, 1, file);
	fwrite(room->turretPos, sizeof room->turretPos[0], numTurrets, file);
}

// *---======---*
// |/   Game   \|
// *---======---*

void Playing_Init(GameState oldState)
{
	cameraZoom = 1;
	cameraPos = Vector2Zero();
}
GameState Playing_Update(void)
{
	if (IsKeyPressed(KEY_GRAVE))
		return GAME_STATE_LEVEL_EDITOR;
	if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
		return GAME_STATE_PAUSED;

	Vector2 mousePos = ScreenToTile(GetMousePosition());

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

		player.lookAngle = AngleBetween(player.pos, mousePos);
		player.justSnapped = false; // Reset from previous frame.

		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			if (!player.hasCapture)
			{
				player.justSnapped = true;
				PlaySound(flashSound);

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
				}
			}
			else
			{
				player.releasePos = mousePos;
				player.isReleasingCapture = true;
			}
		}

		if (player.isReleasingCapture && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
		{
			player.isReleasingCapture = false;
			player.hasCapture = false;

			Vector2 releaseDir = Vector2Subtract(mousePos, player.releasePos);
			if (releaseDir.x == 0 || releaseDir.y == 0)
			{
				// If we don't have a direction we just have to pick one, otherwise 
				// everything will just hang in the air indefinitely and never despawn.
				// First try to send everything away from the player, 
				// if that doesn't work either just pick a random direction.
				releaseDir = Vector2Subtract(player.releasePos, player.pos);
				while (releaseDir.x == 0 || releaseDir.y == 0)
				{
					releaseDir.x = RandomFloat(&rng, -1, +1);
					releaseDir.y = RandomFloat(&rng, -1, +1);
				}
			}

			float releaseSpeed = Vector2Length(releaseDir);
			releaseDir = Vector2Scale(releaseDir, 1 / releaseSpeed);

			Vector2 bulletReleaseVel = Vector2Scale(releaseDir, fmaxf(5 * releaseSpeed, 30));
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
		if (!CheckCollisionCircleRec(b->pos, BULLET_RADIUS, ExpandRectangle(screenRectTiles, 30)))
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
					t->pos.x + (TURRET_RADIUS + PixelsToTiles(15)) * c,
					t->pos.y + (TURRET_RADIUS + PixelsToTiles(15)) * s
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

	return GAME_STATE_PLAYING;
}
void Playing_Draw(void)
{
	ClearBackground(BLACK);

	rlMatrixMode(RL_PROJECTION);
	rlLoadIdentity();
	rlOrtho(0, MAX_TILES_X, 0, MAX_TILES_Y, 0, 1);
	rlTranslatef(cameraPos.x, cameraPos.y, 0);
	rlScalef(cameraZoom, cameraZoom, 0);
	rlGetMatrixProjection();
	{
		for (int y = 0; y < MAX_TILES_Y; ++y)
			for (int x = 0; x < MAX_TILES_X; ++x)
				DrawRectangle(x, y, 1, 1, (x + y) % 2 ? FloatRGBA(0.95f, 0.95f, 0.95f, 1) : FloatRGBA(0.9f, 0.9f, 0.9f, 1));

		DrawCircleV(player.pos, PLAYER_RADIUS, DARKGREEN);

		for (int i = 0; i < numTurrets; ++i)
		{
			Turret t = turrets[i];
			DrawCircleV(t.pos, TURRET_RADIUS, BLACK);
			DrawCircleV(t.pos, TURRET_RADIUS - PixelsToTiles(5), DARKGRAY);
			float lookAngleDegrees = RAD2DEG * t.lookAngle;
			Rectangle gunBarrel = { t.pos.x, t.pos.y, TURRET_RADIUS + PixelsToTiles(10), PixelsToTiles(12) };
			DrawRectanglePro(gunBarrel, PixelsToTiles2(-5, +6), lookAngleDegrees, MAROON);
			DrawCircleV(Vec2(gunBarrel.x, gunBarrel.y), PixelsToTiles(2), ORANGE);
		}

		Color bulletTrail0 = ColorAlpha(DARKGRAY, 0);
		Color bulletTrail1 = ColorAlpha(DARKGRAY, 0.2);
		for (int i = 0; i < numBullets; ++i)
		{
			Bullet b = bullets[i];
			Vector2 perp1 = Vector2Scale(Vector2Normalize(Vec2(-b.vel.y, +b.vel.x)), BULLET_RADIUS);
			Vector2 perp2 = Vector2Scale(Vector2Normalize(Vec2(+b.vel.y, -b.vel.x)), BULLET_RADIUS);
			Vector2 toOrigin = Vector2Subtract(b.origin, b.pos);
			Vector2 perp3 = Vector2Scale(Vector2Normalize(toOrigin), PixelsToTiles(400));
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
			if (player.justSnapped)
			{
				DrawCircleSector(player.pos,
					PLAYER_CAPTURE_CONE_RADIUS + PixelsToTiles(10),
					lookAngleDegrees - RAD2DEG * PLAYER_CAPTURE_CONE_HALF_ANGLE - 1,
					lookAngleDegrees + RAD2DEG * PLAYER_CAPTURE_CONE_HALF_ANGLE + 1,
					12,
					ColorAlpha(ORANGE, 0.9f));
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

			Vector2 mousePos = ScreenToTile(GetMousePosition());
			Vector2 arrowDir = Vector2Normalize(Vector2Subtract(mousePos, player.releasePos));
			Vector2 arrowPos0 = Vector2Add(player.releasePos, Vector2Scale(arrowDir, BULLET_RADIUS + PixelsToTiles(10)));
			Vector2 arrowPos1 = mousePos;
			float arrowLength = Vector2Distance(arrowPos0, arrowPos1);
			float arrowWidth0 = PixelsToTiles(5);
			float arrowWidth1 = Clamp(0.2f * arrowLength, PixelsToTiles(5), PixelsToTiles(30));
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

	rlMatrixMode(RL_PROJECTION);
	rlLoadIdentity();
	rlOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 1);
	{
		Vector2 mp = ScreenToTile(GetMousePosition());
		DrawDebugText("[%.1f %.1f] [%.1f %.1f]", mp.x, mp.y, cameraPos.x, cameraPos.y);
	}
}

void Paused_Init(GameState oldState)
{

}
GameState Paused_Update(void)
{
	if (IsKeyPressed(KEY_GRAVE))
		return GAME_STATE_LEVEL_EDITOR;
	if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))
		return GAME_STATE_PLAYING;
	if (IsKeyPressed(KEY_ESCAPE))
		exit(EXIT_SUCCESS);

	return GAME_STATE_PAUSED;
}
void Paused_Draw(void)
{
	Playing_Draw();
	DrawText("[PAUSED]", 50, 50, 20, BLACK);
}

EditorSelection selection;
char consoleInputBuffer[256];
const Rectangle consoleWindowRect = { 0, 0, SCREEN_WIDTH, 50 };
const Rectangle consoleInputRect = { 0, 24, SCREEN_WIDTH, 25 };
const Rectangle objectsWindowRect = { 0, SCREEN_HEIGHT - 200, SCREEN_WIDTH, 200 };
const Rectangle propertiesWindowRect = { SCREEN_WIDTH - 200, SCREEN_HEIGHT - 800, 200, 550 };
const Rectangle tilesWindowRect = { 0, SCREEN_HEIGHT - 800, 200, 550 };

void LevelEditor_Init(GameState oldState)
{
	cameraPos = Vector2Zero();
	cameraZoom = 1;
	ZoomInToPoint(screenCenter, powf(1.1f, -7));
}
GameState LevelEditor_Update(void)
{
	Vector2 mousePos = GetMousePosition();

	if (IsKeyPressed(KEY_GRAVE))
	{
		cameraZoom = 1;
		return GAME_STATE_PLAYING;
	}

	// Zoom
	float wheelMove = GetMouseWheelMove();
	if (wheelMove > 0)
		ZoomInToPoint(mousePos, cameraZoom * 1.1f);
	else if (wheelMove < 0)
		ZoomInToPoint(mousePos, cameraZoom / 1.1f);

	if (selection.kind == EDITOR_SELECTION_KIND_CONSOLE && IsKeyPressed(KEY_ENTER))
	{
		selection.kind = EDITOR_SELECTION_KIND_NONE;
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		if (!CheckCollisionPointRec(mousePos, consoleWindowRect) &&
			!CheckCollisionPointRec(mousePos, objectsWindowRect) &&
			!CheckCollisionPointRec(mousePos, propertiesWindowRect) &&
			!CheckCollisionPointRec(mousePos, tilesWindowRect))
		{
			// @TODO: Check for collisions with stuff in game.
			selection.kind = EDITOR_SELECTION_KIND_NONE;
		}
	}

	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		Vector2 mouseDelta = GetMouseDelta();
		mouseDelta = Vector2Divide(mouseDelta, Vec2(SCREEN_WIDTH, -SCREEN_HEIGHT));
		mouseDelta = Vector2Multiply(mouseDelta, Vec2(MAX_TILES_X, MAX_TILES_Y));
		cameraPos.x += mouseDelta.x;
		cameraPos.y += mouseDelta.y;
	}
	if (IsKeyPressed(KEY_C))
	{
		cameraPos = Vector2Zero();
		cameraZoom = 1;
		ZoomInToPoint(screenCenter, powf(1.1f, -7));
	}

	return GAME_STATE_LEVEL_EDITOR;
}
void LevelEditor_Draw(void)
{
	Playing_Draw();

	GuiWindowBox(consoleWindowRect, "Console");
	{
		
		if (GuiTextBox(consoleInputRect, consoleInputBuffer, sizeof consoleInputBuffer, selection.kind == EDITOR_SELECTION_KIND_CONSOLE))
			selection.kind = EDITOR_SELECTION_KIND_CONSOLE;
	}
	
	GuiWindowBox(objectsWindowRect, "Objects");
	{
		float x0 = objectsWindowRect.x + 5;
		float y0 = objectsWindowRect.y + 30;
		float x = x0;
		float y = y0;
		if (GuiButton(Rect(x, y, 60, 60), "Turret"))
		{
			selection.kind = EDITOR_SELECTION_KIND_TURRET;
		}
		DrawRectangleRec(Rect(x + 10, y + 10, 40, 40), ColorAlpha(BLACK, 0.2f));
	}

	GuiWindowBox(tilesWindowRect, "Tiles");
	{
		float x0 = tilesWindowRect.x + 5;
		float y0 = tilesWindowRect.y + 30;
		float x = x0;
		float y = y0;
		if (GuiButton(Rect(x, y, 50, 50), "Floor"))
		{
			selection.kind = EDITOR_SELECTION_KIND_TILE;
			selection.tile = TILE_FLOOR;
		}
		DrawRectangleRec(Rect(x + 10, y + 10, 30, 30), ColorAlpha(BLACK, 0.2f));
		x += 55;
		if (GuiButton(Rect(x, y, 50, 50), "Wall"))
		{
			selection.kind = EDITOR_SELECTION_KIND_TILE;
			selection.tile = TILE_WALL;
		}
		DrawRectangleRec(Rect(x + 10, y + 10, 30, 30), ColorAlpha(BLACK, 0.2f));
	}

	const char *propertiesTitle = NULL;
	switch (selection.kind)
	{
		case EDITOR_SELECTION_KIND_PLAYER: propertiesTitle = "Player properties"; break;
		case EDITOR_SELECTION_KIND_TURRET: propertiesTitle = "Turret properties"; break;
		default: propertiesTitle = "Level properties"; break;
	}
	GuiWindowBox(propertiesWindowRect, propertiesTitle);
	{
		float x = propertiesWindowRect.x + 10;
		float y = propertiesWindowRect.y + 30;
		switch (selection.kind)
		{
			case EDITOR_SELECTION_KIND_PLAYER:
			{

			} break;

			case EDITOR_SELECTION_KIND_TURRET:
			{
				GuiSlider(Rect(x, y, 100, 20), "", "", 0, 0, 100);
			} break;

			default:
			{

			} break;
		}
	}
}

void GameInit(void)
{
	if (devMode)
		gameState = GAME_STATE_PLAYING;

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

	player.pos.x = 15;
	player.pos.y = 15;
	SpawnTurret(Vec2(2, 28), 0);
	SpawnTurret(Vec2(28, 28), 0);
	SpawnTurret(Vec2(15, 28), 0);
}
void GameLoopOneIteration(void)
{
	TempReset();

	// Do update and draw in 2 completely separate steps because state could change in the update function.
	GameState oldState = gameState;
	switch (gameState)
	{
		case GAME_STATE_START_MENU:   assert(false);                    break;
		case GAME_STATE_PLAYING:      gameState = Playing_Update();     break;
		case GAME_STATE_PAUSED:       gameState = Paused_Update();      break;
		case GAME_STATE_GAME_OVER:    assert(false);                    break;
		case GAME_STATE_LEVEL_EDITOR: gameState = LevelEditor_Update(); break;
		case GAME_STATE_SCORE:        assert(false);                    break;
		case GAME_STATE_CREDITS:      assert(false);                    break;
	}

	if (gameState != oldState)
	{
		switch (gameState)
		{
			case GAME_STATE_START_MENU:   assert(false);              break;
			case GAME_STATE_PLAYING:      Playing_Init(oldState);     break;
			case GAME_STATE_PAUSED:       Paused_Init(oldState);      break;
			case GAME_STATE_GAME_OVER:    assert(false);              break;
			case GAME_STATE_LEVEL_EDITOR: LevelEditor_Init(oldState); break;
			case GAME_STATE_SCORE:        assert(false);              break;
			case GAME_STATE_CREDITS:      assert(false);              break;
		}
	}
	
	BeginDrawing();
	{
		switch (gameState)
		{
			case GAME_STATE_START_MENU:   assert(false);      break;
			case GAME_STATE_PLAYING:      Playing_Draw();     break;
			case GAME_STATE_PAUSED:       Paused_Draw();      break;
			case GAME_STATE_GAME_OVER:    assert(false);      break;
			case GAME_STATE_LEVEL_EDITOR: LevelEditor_Draw(); break;
			case GAME_STATE_SCORE:        assert(false);      break;
			case GAME_STATE_CREDITS:      assert(false);      break;
		}
	}
	EndDrawing();
}