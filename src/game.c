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
#define PLAYER_RADIUS 0.5f // Must be < 1 tile otherwise collision detection wont work!
#define BULLET_SPEED 30.0f
#define BULLET_RADIUS 0.2f
#define TURRET_RADIUS 1.0f
#define TURRET_TURN_SPEED (30.0f*DEG2RAD)
#define TURRET_FIRE_RATE 1.5f
#define PLAYER_CAPTURE_CONE_HALF_ANGLE (40.0f*DEG2RAD)
#define PLAYER_CAPTURE_CONE_RADIUS 3.5f

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
	TILE_NONE,
	TILE_FLOOR,
	TILE_WALL,
} Tile;

typedef enum EditorSelectionKind
{
	EDITOR_SELECTION_KIND_NONE,
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
	Vector2 lastKnownPlayerPos; //@HACK: x == 0 && y == 0 means the player was never seen.
} Turret;

typedef struct Room
{
	char name[MAX_ROOM_FILE_NAME];

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
		Player *player;
		Turret *turret;
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
void ShiftCamera(float dx, float dy)
{
	cameraPos.x += dx * cameraZoom;
	cameraPos.y += dy * cameraZoom;
}
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
Vector2 ScreenDeltaToTileDelta(Vector2 screenDelta)
{
	Vector2 p = Vector2Multiply(screenDelta, Vec2((float)MAX_TILES_X / SCREEN_WIDTH, -(float)MAX_TILES_Y / SCREEN_HEIGHT));
	return Vector2Scale(p, 1 / cameraZoom);
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

// *---=========---*
// |/   Globals   \|
// *---=========---*

const bool devMode = true; //@TODO: Disable this for release.
const Vector2 screenCenter = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
const Rectangle screenRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
const Rectangle screenRectTiles = { 0, 0, MAX_TILES_X, MAX_TILES_Y };
int numTilesX = MAX_TILES_X;
int numTilesY = MAX_TILES_Y;
Tile tiles[MAX_TILES_Y][MAX_TILES_X];
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

// *---=======---*
// |/   Tiles   \|
// *---=======---*

const char *GetTileName(Tile tile)
{
	switch (tile)
	{
		case TILE_NONE:  return "None";
		case TILE_FLOOR: return "Floor";
		case TILE_WALL:  return "Wall";
		default:         return "[NULL]";
	}
}
bool TileIsPassable(Tile tile)
{
	switch (tile)
	{
		case TILE_NONE:
		case TILE_FLOOR:
			return true;
		case TILE_WALL:
		default:
			return false;
	}
}
Tile TileAt(int x, int y)
{
	if (x < 0 || x >= numTilesX || y < 0 || y >= numTilesY)
		return TILE_NONE;
	return tiles[y][x];
}
Tile TileAtVec(Vector2 xy)
{
	return TileAt((int)xy.x, (int)xy.y);
}

Rectangle GetRoomRect(void)
{
	return (Rectangle) { 0, 0, numTilesX, numTilesY };
}
bool IsPointVisibleFrom(Vector2 pos, Vector2 target)
{
	// @EXTRATIME
	// This doesnt actually visit every tile in between the points.
	// That basically limits the level design, we cant have empty corners now.
	// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

	int x0 = (int)pos.x;
	int y0 = (int)pos.y;
	int x1 = (int)target.x;
	int y1 = (int)target.y;
	int dx = +abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	while (true)
	{
		if (!TileIsPassable(TileAt(x0, y0)))
			return false;
		if (x0 == x1 && y0 == y1) 
			break;
		int e2 = 2 * err;
		if (e2 >= dy)
		{
			if (x0 == x1) break;
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx)
		{
			if (y0 == y1) break;
			err += dx;
			y0 += sy;
		}
	}
	return true;
}
bool IsPointBlockedByEnemiesFrom(Vector2 pos, Vector2 target)
{
	for (int i = 0; i < numTurrets; ++i)
		if (CheckCollisionLineCircle(pos, target, turrets[i].pos, TURRET_RADIUS))
			return true;
	return false;
}
Vector2 ResolveCollisionsCircleTiles(Vector2 center, float radius, Vector2 velocity)
{
	// https://www.youtube.com/watch?v=D2a5fHX-Qrs

	Vector2 p0f = center;
	Vector2 p1f = Vector2Add(center, Vector2Scale(velocity, DELTA_TIME));
	int minx = (int)floorf(fminf(p0f.x, p1f.x) - radius) - 1;
	int miny = (int)floorf(fminf(p0f.y, p1f.y) - radius) - 1;
	int maxx = (int)ceilf(fmaxf(p0f.x, p1f.x) + radius) + 1;
	int maxy = (int)ceilf(fmaxf(p0f.y, p1f.y) + radius) + 1;
	if (minx < 0)
		minx = 0;
	if (miny < 0)
		miny = 0;
	if (maxx >= numTilesX)
		maxx = numTilesX - 1;
	if (maxy >= numTilesY)
		maxy = numTilesY - 1;

	for (int ty = miny; ty <= maxy; ++ty)
	{
		for (int tx = minx; tx <= maxx; ++tx)
		{
			Tile tile = TileAt(tx, ty);
			if (!TileIsPassable(tile))
			{
				Vector2 nearestPoint;
				nearestPoint.x = Clamp(p1f.x, tx, tx + 1);
				nearestPoint.y = Clamp(p1f.y, ty, ty + 1);
				
				Vector2 toNearest = Vector2Subtract(nearestPoint, p1f);
				float len = Vector2Length(toNearest);
				float overlap = radius - len;
				if (overlap > 0)
					p1f = Vector2Subtract(p1f, Vector2Scale(toNearest, overlap / len));
			}
		}
	}

	return p1f;
}
Vector2 ResolveCollisionCircles(Vector2 center, float radius, Vector2 obstacleCenter, float obstacleRadius)
{
	Vector2 normal = Vector2Subtract(center, obstacleCenter);
	float length = Vector2Length(normal);
	float penetration = (radius + obstacleRadius) - length;
	if (penetration > 0)
		center = Vector2Add(center, Vector2Scale(normal, penetration / length));
	return center;
}

int SpawnBullet(Vector2 pos, Vector2 vel)
{
	if (numBullets >= MAX_BULLETS)
		return -1;

	Tile t = TileAtVec(pos);
	if (t == TILE_WALL)
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
	turrets[index].framesUntilShoot = (int)(FPS / TURRET_FIRE_RATE);
	turrets[index].lastKnownPlayerPos.x = 0;
	turrets[index].lastKnownPlayerPos.y = 0;
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
void ShiftAllObjectsBy(float dx, float dy)
{
	player.pos.x += dx;
	player.pos.y += dy;
	for (int i = 0; i < numTurrets; ++i)
	{
		turrets[i].pos.x += dx;
		turrets[i].pos.y += dy;
	}
}

// *---=========---*
// |/   Drawing   \|
// *---=========---*

void SetupTileCoordinateDrawing(void)
{
	rlDrawRenderBatchActive();
	rlMatrixMode(RL_PROJECTION);
	rlLoadIdentity();
	rlOrtho(0, MAX_TILES_X, 0, MAX_TILES_Y, 0, 1);
	rlTranslatef(cameraPos.x, cameraPos.y, 0);
	rlScalef(cameraZoom, cameraZoom, 0);
}
void SetupScreenCoordinateDrawing(void)
{
	rlDrawRenderBatchActive();
	rlMatrixMode(RL_PROJECTION);
	rlLoadIdentity();
	rlOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 1);
}

void DrawPlayer(void)
{
	DrawCircleV(player.pos, PLAYER_RADIUS, DARKGREEN);
}
void DrawPlayerCaptureCone(void)
{
	// No idea why RAD2DEG*radians isn't enough here.. whatever.
	float lookAngleDegrees = (RAD2DEG * (-player.lookAngle)) + 90;
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

	//Vector2 p0 = ScreenToTile(GetMousePosition());
	//Vector2 p1 = player.pos;
	//Color lineColor = IsPointBlockedByEnemiesFrom(p0, p1) ? GREEN : RED;
	//DrawLineV(p0, p1, lineColor);
}
void DrawPlayerRelease(void)
{
	if (!player.isReleasingCapture)
		return;

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
void DrawTiles(void)
{
	for (int y = 0; y < numTilesY; ++y)
	{
		for (int x = 0; x < numTilesX; ++x)
		{
			Tile t = tiles[y][x];
			switch (t)
			{
				case TILE_FLOOR:
				{
					DrawRectangle(x, y, 1, 1, (x + y) % 2 ? FloatRGBA(0.95f, 0.95f, 0.95f, 1) : FloatRGBA(0.9f, 0.9f, 0.9f, 1));
					break;
				}
				case TILE_WALL:
				{
					DrawRectangle(x, y, 1, 1, FloatRGBA(0.5f, 0.5f, 0.5f, 1));
					break;
				}
			}

		}
	}
}
void DrawTurrets(void)
{
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
}
void DrawBullets(void)
{
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
}

void UpdatePlayer(void)
{
	Vector2 mousePos = ScreenToTile(GetMousePosition());

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
		playerMove = Vector2Normalize(playerMove);
		player.vel = Vector2Scale(playerMove, PLAYER_SPEED);
		player.pos = ResolveCollisionsCircleTiles(player.pos, PLAYER_RADIUS, player.vel);
		for (int i = 0; i < numTurrets; ++i)
			player.pos = ResolveCollisionCircles(player.pos, PLAYER_RADIUS, turrets[i].pos, TURRET_RADIUS);
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
void UpdateBullets(void)
{
	for (int i = 0; i < numBullets; ++i)
	{
		Bullet *b = &bullets[i];
		b->pos = Vector2Add(b->pos, Vector2Scale(b->vel, DELTA_TIME));
		if (!CheckCollisionCircleRec(b->pos, BULLET_RADIUS, ExpandRectangle(screenRectTiles, 20)))
		{
			RemoveBulletFromGlobalList(i);
			--i;
		}
		else if (TileAtVec(b->pos) == TILE_WALL) // @TODO: This is treating bullets as mere points even though they have a radius..
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
}
void UpdateTurrets(void)
{
	for (int i = 0; i < numTurrets; ++i)
	{
		Turret *t = &turrets[i];
		bool playerIsVisible = IsPointVisibleFrom(t->pos, player.pos);
		if (playerIsVisible)
			t->lastKnownPlayerPos = player.pos;

		bool triedToShoot = false;
		if (t->lastKnownPlayerPos.x != 0 || t->lastKnownPlayerPos.y != 0)
		{
			float angleToPlayer = AngleBetween(t->pos, t->lastKnownPlayerPos);
			float dAngle = WrapMinMax(angleToPlayer - t->lookAngle, -PI, +PI);

			if (playerIsVisible && fabsf(dAngle) < 15 * DEG2RAD)
			{
				float s = sinf(t->lookAngle);
				float c = cosf(t->lookAngle);
				Vector2 bulletPos = {
					t->pos.x + (TURRET_RADIUS + PixelsToTiles(15)) * c,
					t->pos.y + (TURRET_RADIUS + PixelsToTiles(15)) * s
				};
				if (!IsPointBlockedByEnemiesFrom(bulletPos, player.pos))
				{
					triedToShoot = true;
					t->framesUntilShoot--;
					if (t->framesUntilShoot <= 0)
					{
						t->framesUntilShoot = (int)(FPS / TURRET_FIRE_RATE); // (Frame/sec) / (Shots/sec) = Frame/Shot
						Vector2 bulletVel = {
							BULLET_SPEED * c,
							BULLET_SPEED * s,
						};
						SpawnBullet(bulletPos, bulletVel);
						PlaySound(longShotSound);
						SetSoundPitch(longShotSound, RandomFloat(&rng, 0.95f, 1.2f));
					}
				}
			}

			float turnSpeed = TURRET_TURN_SPEED;
			if (dAngle < 0)
				turnSpeed *= -1;
			t->lookAngle += turnSpeed * DELTA_TIME;
		}

		if (!triedToShoot)
			t->framesUntilShoot = (int)(FPS / TURRET_FIRE_RATE); // (Frame/sec) / (Shots/sec) = Frame/Shot
	}
}

// *---=======---*
// |/   Level   \|
// *---=======---*

Room currentRoom;
bool LoadRoom(Room *room, const char *filename)
{
	char *filepath = TempPrint("res/%s.bin", filename);
	FILE *file = fopen(filepath, "rb");
	if (!file)
	{
		TraceLog(LOG_ERROR, "Unable to load room from file '%s'. The file might not exist.", filepath);
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
	fclose(file);

	char name[sizeof room->name];
	snprintf(name, sizeof name, "%s", filename);
	memset(room, 0, sizeof room[0]);
	memcpy(room->name, name, sizeof name);
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

	TraceLog(LOG_INFO, "Loaded room '%s'.", filepath);
	return true;
}
void SaveRoom(const Room *room)
{
	char *filepath = TempPrint("res/%s.bin", room->name);
	FILE *file = fopen(filepath, "wb");
	if (!file)
	{
		TraceLog(LOG_ERROR, "Failed to save room because couldn't open file '%s'.", filepath);
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
	fwrite(room->turretLookAngle, sizeof room->turretLookAngle[0], numTurrets, file);

	fclose(file);
	TraceLog(LOG_INFO, "Saved room '%s'.", filepath);
}
void CopyRoomToGame(Room *room)
{
	numBullets = 0;
	numTurrets = 0;
	numCapturedBullets = 0;
	player.hasCapture = false;
	player.justSnapped = false;
	player.isReleasingCapture = false;

	numTilesX = room->numTilesX;
	numTilesY = room->numTilesY;
	for (int y = 0; y < numTilesY; ++y)
		memcpy(tiles[y], room->tiles[y], sizeof tiles[0]);

	player.pos = room->playerDefaultPos;
	for (int i = 0; i < room->numTurrets; ++i)
		SpawnTurret(room->turretPos[i], room->turretLookAngle[i]);
}
void CopyGameToRoom(Room *room)
{
	char name[sizeof room->name];
	memcpy(name, room->name, sizeof name);
	memset(room, 0, sizeof room[0]);
	memcpy(room->name, name, sizeof name);
	room->numTilesX = numTilesX;
	room->numTilesY = numTilesY;
	for (int y = 0; y < numTilesY; ++y)
		memcpy(room->tiles[y], tiles[y], numTilesX * sizeof tiles[y][0]);
	room->numTurrets = numTurrets;
	room->playerDefaultPos = player.pos;
	for (int i = 0; i < numTurrets; ++i)
	{
		Turret t = turrets[i];
		room->turretPos[i] = t.pos;
		room->turretLookAngle[i] = t.lookAngle;
	}
}

// *---=========---*
// |/   Playing   \|
// *---=========---*

void Playing_Init(GameState oldState)
{
	cameraZoom = 1;
	cameraPos = Vector2Zero();
	ShiftCamera(-(numTilesX - MAX_TILES_X) / 2.0f, -(numTilesY - MAX_TILES_Y) / 2.0f);
}
GameState Playing_Update(void)
{
	if (IsKeyPressed(KEY_GRAVE))
		return GAME_STATE_LEVEL_EDITOR;
	if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
		return GAME_STATE_PAUSED;

	UpdatePlayer();
	UpdateBullets();
	UpdateTurrets();
	return GAME_STATE_PLAYING;
}
void Playing_Draw(void)
{
	SetupTileCoordinateDrawing();
	{
		DrawTiles();
		DrawTurrets();
		DrawPlayer();
		DrawBullets();
		DrawPlayerCaptureCone();
		DrawPlayerRelease();
	}
	rlDrawRenderBatchActive();

	SetupScreenCoordinateDrawing();
	{
		//DrawFPS(100, 100);
		//Vector2 mp = ScreenToTile(GetMousePosition());
		//DrawDebugText("[%.1f %.1f] [%.1f %.1f]", mp.x, mp.y, cameraPos.x, cameraPos.y);
	}
}

// *---========---*
// |/   Paused   \|
// *---========---*

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

	Color edgeColor = ColorAlpha(BLACK, 0.9f);
	Color centerColor = ColorAlpha(BLACK, 0.1f);
	DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2, edgeColor, centerColor);
	DrawRectangleGradientV(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2, centerColor, edgeColor);
	DrawTextCentered("[PAUSED]", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 40, BLACK);
}

// *---==============---*
// |/   Level Editor   \|
// *---==============---*

EditorSelection selection;
Vector2 lastMouseClickPos;
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
	CopyRoomToGame(&currentRoom);
	ShiftCamera(-(numTilesX - MAX_TILES_X) / 2.0f, -(numTilesY - MAX_TILES_Y) / 2.0f);
	ZoomInToPoint(screenCenter, powf(1.1f, -7));
}
GameState LevelEditor_Update(void)
{
	Vector2 mousePos = GetMousePosition();
	Vector2 mouseDelta = GetMouseDelta();
	Vector2 mousePosTiles = ScreenToTile(mousePos);
	Vector2 mouseDeltaTiles = ScreenDeltaToTileDelta(mouseDelta);

	if (IsKeyPressed(KEY_GRAVE))
	{
		CopyGameToRoom(&currentRoom);
		return GAME_STATE_PLAYING;
	}

	bool altIsDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
	if (IsKeyPressed(KEY_RIGHT))
	{
		if (altIsDown)
		{
			if (numTilesX > 1)
			{
				for (int y = 0; y < numTilesY; ++y)
					memmove(&tiles[y][0], &tiles[y][1], (numTilesX - 1) * sizeof tiles[0][0]);
				--numTilesX;
				ShiftAllObjectsBy(-1, 0);
				ShiftCamera(+1, 0);
			}
		}
		else
		{
			if (numTilesX < MAX_TILES_X)
			{
				int x = numTilesX++;
				for (int y = 0; y < numTilesY; ++y)
					tiles[y][x] = TILE_FLOOR;
			}
		}
	}
	if (IsKeyPressed(KEY_LEFT))
	{
		if (altIsDown)
		{
			if (numTilesX > 1)
				--numTilesX;
		}
		else
		{
			if (numTilesX < MAX_TILES_X)
			{
				for (int y = 0; y < numTilesY; ++y)
				{
					memmove(&tiles[y][1], &tiles[y][0], numTilesX * sizeof tiles[0][0]);
					tiles[y][0] = TILE_FLOOR;
				}
				++numTilesX;
				ShiftAllObjectsBy(+1, 0);
				ShiftCamera(-1, 0);
			}
		}
	}
	if (IsKeyPressed(KEY_UP)) // For some reason up and down seem to be switched here.. not sure why.
	{
		if (altIsDown)
		{
			if (numTilesY > 1)
			{
				for (int y = 0; y < numTilesY - 1; ++y)
					memcpy(tiles[y], tiles[y + 1], numTilesX * sizeof tiles[0][0]);
				--numTilesY;
				ShiftAllObjectsBy(0, -1);
				ShiftCamera(0, +1);
			}
		}
		else
		{
			if (numTilesY < MAX_TILES_Y)
			{
				int y = numTilesY++;
				for (int x = 0; x < numTilesX; ++x)
					tiles[y][x] = TILE_FLOOR;
			}
		}
	}
	if (IsKeyPressed(KEY_DOWN))
	{
		if (altIsDown)
		{
			if (numTilesY > 1)
				--numTilesY;
		}
		else
		{
			if (numTilesY < MAX_TILES_Y)
			{
				for (int y = numTilesY - 1; y >= 0; --y)
					memcpy(tiles[y + 1], tiles[y], numTilesX * sizeof tiles[0][0]);
				for (int x = 0; x < numTilesX; ++x)
					tiles[0][x] = TILE_FLOOR;
				++numTilesY;
				ShiftAllObjectsBy(0, +1);
				ShiftCamera(0, -1);
			}
		}
	}

	// Zoom
	float wheelMove = GetMouseWheelMove();
	if (wheelMove > 0)
		ZoomInToPoint(mousePos, cameraZoom * 1.1f);
	else if (wheelMove < 0)
		ZoomInToPoint(mousePos, cameraZoom / 1.1f);

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		lastMouseClickPos = mousePos;
		if (selection.kind == EDITOR_SELECTION_KIND_TILE)
		{
			int tileX = (int)mousePosTiles.x;
			int tileY = (int)mousePosTiles.y;
			if (tileX >= 0 && tileX < numTilesX && tileY >= 0 && tileY < numTilesY)
			{
				tiles[tileY][tileX] = selection.tile;
			}
			else selection.kind = EDITOR_SELECTION_KIND_NONE;
		}

		if (selection.kind != EDITOR_SELECTION_KIND_TILE) // Checking this again because it might change above.
		{
			if (!CheckCollisionPointRec(mousePos, consoleWindowRect) &&
				!CheckCollisionPointRec(mousePos, objectsWindowRect) &&
				!CheckCollisionPointRec(mousePos, propertiesWindowRect) &&
				!CheckCollisionPointRec(mousePos, tilesWindowRect))
			{
				selection.kind = EDITOR_SELECTION_KIND_NONE;

				for (int i = 0; i < numTurrets; ++i)
				{
					if (CheckCollisionPointCircle(mousePosTiles, turrets[i].pos, TURRET_RADIUS))
					{
						selection.kind = EDITOR_SELECTION_KIND_TURRET;
						selection.turret = &turrets[i];
					}
				}

				if (CheckCollisionPointCircle(mousePosTiles, player.pos, PLAYER_RADIUS))
				{
					selection.kind = EDITOR_SELECTION_KIND_PLAYER;
					selection.player = &player;
				}
			}
		}
	}

	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		Vector2 d = Vector2Divide(mouseDelta, Vec2(SCREEN_WIDTH, -SCREEN_HEIGHT));
		d = Vector2Multiply(d, Vec2(MAX_TILES_X, MAX_TILES_Y));
		cameraPos.x += d.x;
		cameraPos.y += d.y;
	}
	if (IsKeyPressed(KEY_C))
	{
		cameraPos = Vector2Zero();
		cameraZoom = 1;
		ShiftCamera(-(numTilesX - MAX_TILES_X) / 2.0f, -(numTilesY - MAX_TILES_Y) / 2.0f);
		ZoomInToPoint(screenCenter, powf(1.1f, -7));
	}
	if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S))
	{
		CopyGameToRoom(&currentRoom);
		SaveRoom(&currentRoom);
	}
	if (IsKeyDown(KEY_DELETE))
	{
		switch (selection.kind)
		{
			case EDITOR_SELECTION_KIND_TURRET:
			{
				int index = (int)(selection.turret - turrets);
				RemoveTurretFromGlobalList(index);
				selection.kind = EDITOR_SELECTION_KIND_NONE;
				break;
			}
		}
	}

	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		switch (selection.kind)
		{
			case EDITOR_SELECTION_KIND_TURRET:
			{
				Vector2 newPos = Vector2Add(selection.turret->pos, mouseDeltaTiles);
				if (CheckCollisionPointCircle(mousePosTiles, newPos, TURRET_RADIUS))
					selection.turret->pos = newPos;
				break;
			}
			case EDITOR_SELECTION_KIND_PLAYER:
			{
				Vector2 newPos = Vector2Add(selection.player->pos, mouseDeltaTiles);
				if (CheckCollisionPointCircle(mousePosTiles, newPos, PLAYER_RADIUS))
					selection.player->pos = newPos;
				break;
			}
		}
	}

	return GAME_STATE_LEVEL_EDITOR;
}
void LevelEditor_Draw(void)
{
	SetupTileCoordinateDrawing();
	{
		DrawTiles();
		DrawTurrets();
		DrawPlayer();
	}
	SetupScreenCoordinateDrawing();

	Vector2 mousePos = GetMousePosition();
	Vector2 mousePosTiles = ScreenToTile(mousePos);

	GuiWindowBox(consoleWindowRect, "Console");
	{
		bool isFocused = CheckCollisionPointRec(lastMouseClickPos, consoleInputRect);
		GuiTextBox(consoleInputRect, consoleInputBuffer, sizeof consoleInputBuffer, isFocused);

		if (isFocused && IsKeyPressed(KEY_ENTER))
		{
			int splitCount;
			const char **split = TextSplit(consoleInputBuffer, ' ', &splitCount);
			if (splitCount >= 1)
			{
				int argCount = splitCount - 1;
				const char *command = split[0];
				const char **args = &split[1];
				if (TextIsEqual(command, "lr") || TextIsEqual(command, "loadroom"))
				{
					if (splitCount == 2)
					{
						if (LoadRoom(&currentRoom, args[0]))
							CopyRoomToGame(&currentRoom);
					}
					else if (splitCount < 2) TraceLog(LOG_ERROR, "Command '%s' requires the name of the room to load.", command);
					else if (splitCount > 2) TraceLog(LOG_ERROR, "Command '%s' requires only 1 argument, but %d were given.", command, argCount);
				}
			}
		}
	}
	
	GuiWindowBox(objectsWindowRect, "Objects");
	{
		float x0 = objectsWindowRect.x + 5;
		float y0 = objectsWindowRect.y + 30;
		float x = x0;
		float y = y0;
		if (GuiButton(Rect(x, y, 60, 60), "Turret"))
		{
			int index = SpawnTurret(Vector2Zero(), 0);
			selection.kind = EDITOR_SELECTION_KIND_TURRET;
			selection.turret = &turrets[index];
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
		case EDITOR_SELECTION_KIND_TILE: propertiesTitle = "Tile properties"; break;
		default: propertiesTitle = "Room properties"; break;
	}
	GuiWindowBox(propertiesWindowRect, propertiesTitle);
	{
		float x = propertiesWindowRect.x + 10;
		float y = propertiesWindowRect.y + 30;
		switch (selection.kind)
		{
			case EDITOR_SELECTION_KIND_PLAYER:
			{
				GuiText(Rect(x, y, 100, 20), "X: %.2f", selection.player->pos.x);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Y: %.2f", selection.player->pos.y);
				y += 20;
			} break;

			case EDITOR_SELECTION_KIND_TURRET:
			{
				GuiText(Rect(x, y, 100, 20), "X: %.2f", selection.turret->pos.x);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Y: %.2f", selection.turret->pos.y);
				y += 20;
				selection.turret->lookAngle = GuiSlider(Rect(x, y, 100, 20), "", "Look angle", selection.turret->lookAngle, -PI, +PI);
			} break;

			case EDITOR_SELECTION_KIND_TILE:
			{

			} break;

			default: // Room properties
			{
				Rectangle roomNameRect = Rect(x, y, 140, 20);
				bool isFocused = CheckCollisionPointRec(lastMouseClickPos, roomNameRect);
				GuiTextBox(roomNameRect, currentRoom.name, sizeof currentRoom.name, isFocused);
				GuiLabel(Rect(x + 145, y, 20, 20), "Name");
				y += 25;
				if (GuiButton(Rect(x, y, 65, 20), "Save"))
				{
					CopyGameToRoom(&currentRoom);
					SaveRoom(&currentRoom);
				}
				if (GuiButton(Rect(x + 75, y, 65, 20), "Reload"))
				{
					LoadRoom(&currentRoom, currentRoom.name);
					CopyRoomToGame(&currentRoom);
				}
				y += 25;
				GuiText(Rect(x, y, 100, 20), "Tiles X: %d", numTilesX);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Tiles Y: %d", numTilesY);
				y += 20;
			} break;
		}

		y = propertiesWindowRect.y + propertiesWindowRect.height - 60;
		GuiText(Rect(x, y, 100, 20), "%s", GetTileName(TileAtVec(mousePosTiles)));
		y += 20;
		GuiText(Rect(x, y, 100, 20), "Mouse X: %.2f", mousePosTiles.x);
		y += 20;
		GuiText(Rect(x, y, 100, 20), "Mouse Y: %.2f", mousePosTiles.y);
	}
}

// *---======---*
// |/   Game   \|
// *---======---*

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

	if (devMode)
		gameState = GAME_STATE_PLAYING;
	if (gameState == GAME_STATE_PLAYING)
	{
		LoadRoom(&currentRoom, "room0");
		CopyRoomToGame(&currentRoom);
		Playing_Init(GAME_STATE_PLAYING);
	}
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
	ClearBackground(BLACK);
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