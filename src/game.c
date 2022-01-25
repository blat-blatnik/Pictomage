#include "basic.h"

// *---===========---*
// |/   Constants   \|
// *---===========---*

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 900
#define FPS 60
#define DELTA_TIME (1.0f/FPS)
#define MAX_BULLETS 100
#define MAX_TURRETS 50
#define MAX_BOMBS 50
#define MAX_EXPLOSIONS 10
#define TILE_SIZE 30
#define MAX_TILES_X (SCREEN_WIDTH/TILE_SIZE)
#define MAX_TILES_Y (SCREEN_HEIGHT/TILE_SIZE)
#define MAX_DOORS_PER_ROOM 4
#define MAX_ROOM_NAME 64

#define PLAYER_SPEED 10.0f
#define PLAYER_RADIUS 0.5f // Must be < 1 tile otherwise collision detection wont work!
#define BULLET_SPEED 30.0f
#define BULLET_RADIUS 0.2f
#define TURRET_RADIUS 1.0f
#define TURRET_TURN_SPEED (30.0f*DEG2RAD)
#define TURRET_FIRE_RATE 1.5f
#define TURRET_FRICTION 0.85f
#define BOMB_RADIUS 0.4f
#define BOMB_IDLE_SPEED 1.0f
#define BOMB_SPEED 2.5f
#define BOMB_SPEED_CLOSE 8.0f
#define BOMB_CLOSE_THRESHOLD 6.0f
#define BOMB_EXPLOSION_RADIUS 4.0f
#define BOMB_EXPLOSION_DURATION 0.5f
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
	TILE_ENTRANCE,
	TILE_EXIT,
} Tile;

typedef enum EditorSelectionKind
{
	EDITOR_SELECTION_KIND_NONE,
	EDITOR_SELECTION_KIND_TILE,
	EDITOR_SELECTION_KIND_PLAYER,
	EDITOR_SELECTION_KIND_TURRET,
	EDITOR_SELECTION_KIND_BOMB,
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
	Vector2 flingVelocity;
	Vector2 lastKnownPlayerPos; //@HACK: x == 0 && y == 0 means the player was never seen.
} Turret;

typedef struct Bomb
{
	Vector2 pos;
	Vector2 lastKnownPlayerPos; //@HACK: x == 0 && y == 0 means the player was never seen.
	Vector2 idleMoveVel;
	bool wasFlung;
	Vector2 flungOrigin;
	Vector2 flungVel;
	int framesUntilIdleMove;
	int framesToIdleMove;
} Bomb;

typedef struct Room
{
	char name[MAX_ROOM_NAME];
	char prev[MAX_ROOM_NAME];
	char next[MAX_ROOM_NAME];

	int numTilesX;
	int numTilesY;
	Tile tiles[MAX_TILES_Y][MAX_TILES_X];
	
	Vector2 playerDefaultPos;
	
	int numTurrets;
	Vector2 turretPos[MAX_TURRETS];
	float turretLookAngle[MAX_TURRETS];

	int numBombs;
	Vector2 bombPos[MAX_BOMBS];
} Room;

typedef struct Explosion
{
	Vector2 pos;
	float radius;
	int frame;
	int durationFrames;
} Explosion;

typedef struct EditorSelection
{
	EditorSelectionKind kind;
	union
	{
		Tile tile;
		Player *player;
		Turret *turret;
		Bomb *bomb;
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

bool godMode = true; //@TODO: Disable this for release.
const bool devMode = true; //@TODO: Disable this for release.
const Vector2 screenCenter = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
const Rectangle screenRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
const Rectangle screenRectTiles = { 0, 0, MAX_TILES_X, MAX_TILES_Y };
char roomName[MAX_ROOM_NAME];
char previousRoomName[MAX_ROOM_NAME];
char nextRoomName[MAX_ROOM_NAME];
int numTilesX = MAX_TILES_X;
int numTilesY = MAX_TILES_Y;
Tile tiles[MAX_TILES_Y][MAX_TILES_X];
GameState gameState = GAME_STATE_START_MENU;
Random rng;
Sound flashSound;
Sound longShotSound;
Sound explosionSound;
Sound turretDestroySound;
Player player;
int numBullets;
int numCapturedBullets;
Bullet bullets[MAX_BULLETS];
Bullet capturedBullets[MAX_BULLETS];
int numTurrets;
int numCapturedTurrets;
Turret turrets[MAX_TURRETS];
Turret capturedTurrets[MAX_TURRETS];
int numBombs;
int numCapturedBombs;
Bomb bombs[MAX_BOMBS];
Bomb capturedBombs[MAX_BOMBS];
int numExplosions;
Explosion explosions[MAX_EXPLOSIONS];

// *---=======---*
// |/   Tiles   \|
// *---=======---*

int NumRemainingEnemies(void)
{
	return numTurrets + numCapturedTurrets + numBombs + numCapturedBombs;
}
const char *GetTileName(Tile tile)
{
	switch (tile)
	{
		case TILE_NONE:     return "None";
		case TILE_FLOOR:    return "Floor";
		case TILE_WALL:     return "Wall";
		case TILE_ENTRANCE: return "Entrance";
		case TILE_EXIT:     return "Exit";
		default:            return "[NULL]";
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
		case TILE_ENTRANCE:
		default:
			return false;

		case TILE_EXIT:
			if (NumRemainingEnemies() > 0)
				return false;
			else
				return true;
	}
}
bool TileIsOpaque(Tile tile)
{
	switch (tile)
	{
		case TILE_WALL:
			return true;
		case TILE_NONE:
		case TILE_FLOOR:
		case TILE_ENTRANCE:
		default:
			return false;

		case TILE_EXIT:
			if (NumRemainingEnemies() > 0)
				return false;
			else
				return true;
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
	for (int i = 0; i < numBombs; ++i)
		if (CheckCollisionLineCircle(pos, target, bombs[i].pos, BOMB_RADIUS))
			return true;
	return false;
}
Vector2 ResolveCollisionsCircleRoom(Vector2 center, float radius, Vector2 velocity)
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

	// Impassable tiles.
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
				if (overlap > 0 && len > 0)
					p1f = Vector2Subtract(p1f, Vector2Scale(toNearest, overlap / len));
			}
		}
	}

	// Room boundaries
	if (p1f.x - radius < 0)
		p1f.x = radius;
	if (p1f.y - radius < 0)
		p1f.y = radius;
	if (p1f.x + radius > numTilesX)
		p1f.x = (float)numTilesX - radius;
	if (p1f.y + radius > numTilesY)
		p1f.y = (float)numTilesY - radius;

	return p1f;
}
Vector2 ResolveCollisionCircles(Vector2 center, float radius, Vector2 obstacleCenter, float obstacleRadius)
{
	Vector2 normal = Vector2Subtract(center, obstacleCenter);
	float length = Vector2Length(normal);
	float penetration = (radius + obstacleRadius) - length;
	if (penetration > 0 && length > 0)
		center = Vector2Add(center, Vector2Scale(normal, penetration / length));
	return center;
}

Bullet *SpawnBullet(Vector2 pos, Vector2 vel)
{
	if (numBullets >= MAX_BULLETS)
		return NULL;

	Tile t = TileAtVec(pos);
	if (t == TILE_WALL)
		return NULL;

	Bullet *bullet = &bullets[numBullets++];
	bullet->origin = pos;
	bullet->pos = pos;
	bullet->vel = vel;
	return bullet;
}
Turret *SpawnTurret(Vector2 pos, float lookAngleRadians)
{
	if (numTurrets >= MAX_TURRETS)
		return NULL;

	Turret *turret = &turrets[numTurrets++];
	turret->pos = pos;
	turret->lookAngle = lookAngleRadians;
	turret->framesUntilShoot = (int)(FPS / TURRET_FIRE_RATE);
	turret->lastKnownPlayerPos = Vector2Zero();
	turret->flingVelocity = Vector2Zero();
	return turret;
}
Bomb *SpawnBomb(Vector2 pos)
{
	if (numBombs >= MAX_BOMBS)
		return NULL;

	Bomb *bomb = &bombs[numBombs++];
	bomb->pos = pos;
	bomb->lastKnownPlayerPos.x = 0;
	bomb->lastKnownPlayerPos.y = 0;
	bomb->framesUntilIdleMove = RandomInt(&rng, 60, 120);
	bomb->framesToIdleMove = 0;
	bomb->idleMoveVel = Vector2Zero();
	bomb->wasFlung = false;
	bomb->flungOrigin = Vector2Zero();
	bomb->flungVel = Vector2Zero();
	return bomb;
}
Explosion *SpawnExplosion(Vector2 pos, float durationSeconds, float radius)
{
	if (numExplosions >= MAX_EXPLOSIONS)
		return NULL;

	Explosion *explosion = &explosions[numExplosions++];
	explosion->durationFrames = (int)(durationSeconds * FPS);
	explosion->frame = 0;
	explosion->pos = pos;
	explosion->radius = radius;
	return explosion;
}
void RemoveBulletFromGlobalList(int index)
{
	ASSERT(index >= 0 && index < numBullets);
	SwapMemory(bullets + index, bullets + numBullets - 1, sizeof bullets[0]);
	--numBullets;
}
void RemoveTurretFromGlobalList(int index)
{
	ASSERT(index >= 0 && index < numTurrets);
	SwapMemory(turrets + index, turrets + numTurrets - 1, sizeof turrets[0]);
	--numTurrets;
}
void RemoveBombFromGlobalList(int index)
{
	ASSERT(index >= 0 && index < numBombs);
	SwapMemory(bombs + index, bombs + numBombs - 1, sizeof bombs[0]);
	--numBombs;
}
void RemoveExplosionsFromGlobalList(int index)
{
	ASSERT(index >= 0 && index < numExplosions);
	SwapMemory(explosions + index, explosions + numExplosions - 1, sizeof explosions[0]);
	--numExplosions;
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
	for (int i = 0; i < numBombs; ++i)
	{
		bombs[i].pos.x += dx;
		bombs[i].pos.y += dy;
	}
}
void ExplodeBomb(int index)
{
	assert(index >= 0 && index < numBombs);
	Bomb *bomb = &bombs[index];
	SpawnExplosion(bomb->pos, BOMB_EXPLOSION_DURATION, BOMB_EXPLOSION_RADIUS);
	PlaySound(explosionSound);
	RemoveBombFromGlobalList(index);
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
	char prev[MAX_ROOM_NAME];
	char next[MAX_ROOM_NAME];
	u8 numTilesX = 1;
	u8 numTilesY = 1;
	u8 tiles[MAX_TILES_Y][MAX_TILES_X] = { { TILE_FLOOR } };
	Vector2 playerDefaultPos = Vec2(0.5f, 0.5f);
	u8 numTurrets = 0;
	Vector2 turretPos[MAX_TURRETS] = { 0 };
	float turretLookAngle[MAX_TURRETS] = { 0 };
	u8 numBombs = 0;
	Vector2 bombPos[MAX_BOMBS] = { 0 };

	fread(&flags, sizeof flags, 1, file);
	fread(prev, sizeof prev, 1, file);
	fread(next, sizeof next, 1, file);
	fread(&numTilesX, sizeof numTilesX, 1, file);
	fread(&numTilesY, sizeof numTilesY, 1, file);
	for (u8 y = 0; y < numTilesY; ++y)
		fread(tiles[y], sizeof tiles[0][0], numTilesX, file);
	fread(&playerDefaultPos, sizeof playerDefaultPos, 1, file);
	fread(&numTurrets, sizeof numTurrets, 1, file);
	fread(turretPos, sizeof turretPos[0], numTurrets, file);
	fread(turretLookAngle, sizeof turretLookAngle[0], numTurrets, file);
	fread(&numBombs, sizeof numBombs, 1, file);
	fread(bombPos, sizeof bombPos[0], numBombs, file);
	fclose(file);

	char name[sizeof room->name];
	snprintf(name, sizeof name, "%s", filename);
	memset(room, 0, sizeof room[0]);
	memcpy(room->name, name, sizeof room->name);
	memcpy(room->prev, prev, sizeof room->prev);
	memcpy(room->next, next, sizeof room->next);
	room->numTilesX = (int)numTilesX;
	room->numTilesY = (int)numTilesY;
	for (u8 y = 0; y < numTilesY; ++y)
		for (u8 x = 0; x < numTilesX; ++x)
			room->tiles[y][x] = (Tile)tiles[y][x];
	room->playerDefaultPos = playerDefaultPos;
	room->numTurrets = (int)numTurrets;
	memcpy(room->turretPos, turretPos, sizeof turretPos);
	memcpy(room->turretLookAngle, turretLookAngle, sizeof turretLookAngle);
	room->numBombs = (int)numBombs;
	memcpy(room->bombPos, bombPos, sizeof bombPos);

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
	fwrite(room->prev, sizeof room->prev, 1, file);
	fwrite(room->next, sizeof room->next, 1, file);

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

	u8 numTurrets = (u8)room->numTurrets;
	fwrite(&numTurrets, sizeof numTurrets, 1, file);
	fwrite(room->turretPos, sizeof room->turretPos[0], numTurrets, file);
	fwrite(room->turretLookAngle, sizeof room->turretLookAngle[0], numTurrets, file);

	u8 numBombs = (u8)room->numBombs;
	fwrite(&numBombs, sizeof numBombs, 1, file);
	fwrite(room->bombPos, sizeof room->bombPos[0], numBombs, file);

	fclose(file);
	TraceLog(LOG_INFO, "Saved room '%s'.", filepath);
}
void CopyRoomToGame(Room *room)
{
	memcpy(roomName, room->name, sizeof roomName);
	memcpy(previousRoomName, room->prev, sizeof previousRoomName);
	memcpy(nextRoomName, room->next, sizeof nextRoomName);

	numBullets = 0;
	numTurrets = 0;
	numBombs = 0;
	numCapturedBullets = 0;
	numCapturedTurrets = 0;
	numCapturedBombs = 0;
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
	for (int i = 0; i < room->numBombs; ++i)
		SpawnBomb(room->bombPos[i]);
}
void CopyGameToRoom(Room *room)
{
	char name[sizeof room->name];
	memcpy(name, room->name, sizeof name);
	memset(room, 0, sizeof room[0]);
	memcpy(room->name, name, sizeof name);
	memcpy(room->prev, previousRoomName, sizeof room->prev);
	memcpy(room->next, nextRoomName, sizeof room->next);
	room->numTilesX = numTilesX;
	room->numTilesY = numTilesY;
	for (int y = 0; y < numTilesY; ++y)
		memcpy(room->tiles[y], tiles[y], numTilesX * sizeof tiles[y][0]);
	room->playerDefaultPos = player.pos;
	room->numTurrets = numTurrets;
	for (int i = 0; i < numTurrets; ++i)
	{
		Turret t = turrets[i];
		room->turretPos[i] = t.pos;
		room->turretLookAngle[i] = t.lookAngle;
	}
	room->numBombs = numBombs;
	for (int i = 0; i < numBombs; ++i)
	{
		Bomb b = bombs[i];
		room->bombPos[i] = b.pos;
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

	for (int i = 0; i < numCapturedTurrets; ++i)
	{
		Color color = ColorAlpha(DARKBLUE, 0.5f);
		Turret t = capturedTurrets[i];
		Vector2 pos = Vector2Add(t.pos, player.releasePos);
		DrawCircleV(pos, TURRET_RADIUS, color);
		DrawCircleV(pos, TURRET_RADIUS - PixelsToTiles(5), color);
		float lookAngleDegrees = RAD2DEG * t.lookAngle;
		Rectangle gunBarrel = { pos.x, pos.y, TURRET_RADIUS + PixelsToTiles(10), PixelsToTiles(12) };
		DrawRectanglePro(gunBarrel, PixelsToTiles2(-5, +6), lookAngleDegrees, color);
		DrawCircleV(Vec2(gunBarrel.x, gunBarrel.y), PixelsToTiles(2), color);
	}

	for (int i = 0; i < numCapturedBombs; ++i)
	{
		Bomb b = capturedBombs[i];
		Vector2 pos = Vector2Add(b.pos, player.releasePos);
		DrawCircleV(pos, BOMB_RADIUS, ColorAlpha(DARKBLUE, 0.5f));
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
				} break;
				case TILE_WALL:
				{
					DrawRectangle(x, y, 1, 1, FloatRGBA(0.5f, 0.5f, 0.5f, 1));
				} break;
				case TILE_ENTRANCE:
				{
					DrawRectangle(x, y, 1, 1, BLUE);
				} break;
				case TILE_EXIT:
				{
					if (NumRemainingEnemies() == 0)
						DrawRectangle(x, y, 1, 1, FloatRGBA(0, 1, 0, 1));
					else
						DrawRectangle(x, y, 1, 1, FloatRGBA(1, 0, 0, 1));
				} break;
				default:
				{
					DrawRectangle(x, y, 1, 1, FloatRGBA(1, 0, 1, 1));
				} break;
			}
		}
	}
}
void DrawBullets(void)
{
	const Color trailColor0 = ColorAlpha(DARKGRAY, 0);
	const Color trailColor1 = ColorAlpha(DARKGRAY, 0.2);
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
			rlColor(trailColor1);
			rlVertex2fv(trail1);
			rlVertex2fv(trail2);
			rlColor(trailColor0);
			rlVertex2fv(trail3);
		}
		rlEnd();
		DrawCircleV(b.pos, BULLET_RADIUS, DARKGRAY);
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
void DrawBombs(void)
{
	const Color trailColor0 = ColorAlpha(DARKGRAY, 0);
	const Color trailColor1 = ColorAlpha(DARKGRAY, 0.2);
	for (int i = 0; i < numBombs; ++i)
	{
		Bomb b = bombs[i];
		if (b.wasFlung)
		{
			Vector2 perp1 = Vector2Scale(Vector2Normalize(Vec2(-b.flungVel.y, +b.flungVel.x)), BULLET_RADIUS);
			Vector2 perp2 = Vector2Scale(Vector2Normalize(Vec2(+b.flungVel.y, -b.flungVel.x)), BULLET_RADIUS);
			Vector2 toOrigin = Vector2Subtract(b.flungOrigin, b.pos);
			Vector2 perp3 = Vector2Scale(Vector2Normalize(toOrigin), PixelsToTiles(400));
			if (Vector2LengthSqr(perp3) > Vector2LengthSqr(toOrigin))
				perp3 = toOrigin;
			Vector2 trail1 = Vector2Add(b.pos, perp1);
			Vector2 trail2 = Vector2Add(b.pos, perp2);
			Vector2 trail3 = Vector2Add(b.pos, perp3);
			rlBegin(RL_TRIANGLES);
			{
				rlColor(trailColor1);
				rlVertex2fv(trail1);
				rlVertex2fv(trail2);
				rlColor(trailColor0);
				rlVertex2fv(trail3);
			}
			rlEnd();
		}
		DrawCircleV(b.pos, BOMB_RADIUS, BLACK);
	}
}
void DrawExplosions(void)
{
	for (int i = 0; i < numExplosions; ++i)
	{
		Explosion *e = &explosions[i];
		float t = 2 * Clamp(e->frame / (float)e->durationFrames, 0, 1);
		float x = 1 - t;
		float r = (1 - x * x * x * x * x * x) * e->radius;
		DrawCircleV(e->pos, r, ColorAlpha(RED, 0.5f));
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
		player.pos = ResolveCollisionsCircleRoom(player.pos, PLAYER_RADIUS, player.vel);
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

			for (int i = 0; i < numBombs; ++i)
			{
				Bomb *b = &bombs[i];
				if (CheckCollisionConeCircle(player.pos,
					PLAYER_CAPTURE_CONE_RADIUS,
					player.lookAngle - PLAYER_CAPTURE_CONE_HALF_ANGLE,
					player.lookAngle + PLAYER_CAPTURE_CONE_HALF_ANGLE,
					b->pos, BOMB_RADIUS))
				{
					int index = numCapturedBombs++;
					capturedBombs[index] = *b;
					RemoveBombFromGlobalList(i);
					--i;
				}
			}

			for (int i = 0; i < numTurrets; ++i)
			{
				Turret *t = &turrets[i];
				if (CheckCollisionConeCircle(player.pos,
					PLAYER_CAPTURE_CONE_RADIUS,
					player.lookAngle - PLAYER_CAPTURE_CONE_HALF_ANGLE,
					player.lookAngle + PLAYER_CAPTURE_CONE_HALF_ANGLE,
					t->pos, TURRET_RADIUS))
				{
					int index = numCapturedTurrets++;
					capturedTurrets[index] = *t;
					RemoveTurretFromGlobalList(i);
					--i;
				}
			}

			int numCapturedItems = numCapturedBullets + numCapturedTurrets + numCapturedBombs;
			if (numCapturedItems != 0)
			{
				Vector2 captureCenter = Vector2Zero();
				for (int i = 0; i < numCapturedBullets; ++i)
					captureCenter = Vector2Add(captureCenter, capturedBullets[i].pos);
				for (int i = 0; i < numCapturedTurrets; ++i)
					captureCenter = Vector2Add(captureCenter, capturedTurrets[i].pos);
				for (int i = 0; i < numCapturedBombs; ++i)
					captureCenter = Vector2Add(captureCenter, capturedBombs[i].pos);
				captureCenter = Vector2Scale(captureCenter, 1.0f / numCapturedItems);

				for (int i = 0; i < numCapturedBullets; ++i)
					capturedBullets[i].pos = Vector2Subtract(capturedBullets[i].pos, captureCenter);
				for (int i = 0; i < numCapturedTurrets; ++i)
					capturedTurrets[i].pos = Vector2Subtract(capturedTurrets[i].pos, captureCenter);
				for (int i = 0; i < numCapturedBombs; ++i)
					capturedBombs[i].pos = Vector2Subtract(capturedBombs[i].pos, captureCenter);

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

		Vector2 releaseVel = Vector2Scale(releaseDir, fmaxf(5 * releaseSpeed, 30));
		for (int i = 0; i < numCapturedBullets; ++i)
		{
			Bullet b = capturedBullets[i];
			Vector2 pos = Vector2Add(b.pos, player.releasePos);
			SpawnBullet(pos, releaseVel);
		}
		for (int i = 0; i < numCapturedTurrets; ++i)
		{
			Turret t = capturedTurrets[i];
			Vector2 pos = Vector2Add(t.pos, player.releasePos);
			Turret *turret = SpawnTurret(pos, t.lookAngle);
			if (turret)
			{
				turret->flingVelocity = releaseVel;
			}
		}
		for (int i = 0; i < numCapturedBombs; ++i)
		{
			Bomb b = capturedBombs[i];
			Vector2 pos = Vector2Add(b.pos, player.releasePos);
			Bomb *bomb = SpawnBomb(pos);
			if (bomb)
			{
				bomb->wasFlung = true;
				bomb->flungOrigin = pos;
				bomb->flungVel = releaseVel;
			}
		}
		numCapturedBullets = 0;
		numCapturedTurrets = 0;
		numCapturedBombs = 0;
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
			bool collided = false;
			for (int j = 0; j < numTurrets; ++j)
			{
				Turret *t = &turrets[j];
				if (CheckCollisionCircles(b->pos, BULLET_RADIUS, t->pos, TURRET_RADIUS))
				{
					RemoveBulletFromGlobalList(i);
					RemoveTurretFromGlobalList(j);
					PlaySound(turretDestroySound);
					--i;
					collided = true;
					break;
				}
			}
			if (collided)
				continue;

			for (int j = 0; j < numBombs; ++j)
			{
				Bomb *bomb = &bombs[j];
				if (CheckCollisionCircles(b->pos, BULLET_RADIUS, bomb->pos, BOMB_RADIUS))
				{
					RemoveBulletFromGlobalList(i);
					ExplodeBomb(j);
					--i;
					collided = true;
					break;
				}
			}
			if (collided)
				continue;
			
			if (CheckCollisionCircles(b->pos, BULLET_RADIUS, player.pos, PLAYER_RADIUS))
			{
				RemoveBulletFromGlobalList(i);
				--i;
				continue;
			}
		}
	}
}
void UpdateTurrets(void)
{
	for (int i = 0; i < numTurrets; ++i)
	{
		Turret *t = &turrets[i];
		Vector2 dpos = Vector2Scale(t->flingVelocity, DELTA_TIME);
		if (dpos.x > 0.00001f || dpos.y > 0.00001f)
		{
			t->pos = ResolveCollisionsCircleRoom(t->pos, TURRET_RADIUS, t->flingVelocity);
			for (int j = 0; j < numTurrets; ++j)
				if (i != j)
					t->pos = ResolveCollisionCircles(t->pos, TURRET_RADIUS, turrets[j].pos, TURRET_RADIUS);

			float velMagnitude = Vector2Length(t->flingVelocity);
			if (velMagnitude > 0.5f)
			{
				bool exploded = false;
				for (int j = 0; j < numBombs; ++j)
				{
					if (CheckCollisionCircles(t->pos, TURRET_RADIUS, bombs[j].pos, BOMB_RADIUS))
					{
						exploded = true;
						ExplodeBomb(j);
						break;
					}
				}
				if (exploded)
				{
					RemoveTurretFromGlobalList(i);
					--i;
					continue;
				}
			}

			t->flingVelocity = Vector2Scale(t->flingVelocity, TURRET_FRICTION);
		}

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
void UpdateBombs(void)
{
	for (int i = 0; i < numBombs; ++i)
	{
		Bomb *b = &bombs[i];
		if (b->wasFlung)
		{
			Vector2 vel = b->flungVel;
			Vector2 expectedPos = Vector2Add(b->pos, Vector2Scale(vel, DELTA_TIME));
			if (vel.x != 0 || vel.y != 0)
			{
				b->pos = ResolveCollisionsCircleRoom(b->pos, BOMB_RADIUS, vel);
				for (int j = 0; j < numTurrets; ++j)
					b->pos = ResolveCollisionCircles(b->pos, BOMB_RADIUS, turrets[j].pos, TURRET_RADIUS);
			}

			bool collidedWithPlayer = CheckCollisionCircles(b->pos, BOMB_RADIUS, player.pos, PLAYER_RADIUS);
			if (collidedWithPlayer || !Vector2Equal(b->pos, expectedPos))
			{
				ExplodeBomb(i);
				--i;
			}
		}
		else
		{
			bool playerIsVisible = IsPointVisibleFrom(b->pos, player.pos);
			if (playerIsVisible)
				b->lastKnownPlayerPos = player.pos;

			Vector2 vel = Vector2Zero();
			bool chasedPlayer = false;

			if (b->lastKnownPlayerPos.x != 0 || b->lastKnownPlayerPos.y != 0)
			{
				Vector2 toPlayer = Vector2Subtract(b->lastKnownPlayerPos, b->pos);
				float length = Vector2Length(toPlayer);
				float speed = Lerp(BOMB_SPEED, BOMB_SPEED_CLOSE, Smoothstep(1.5f * BOMB_CLOSE_THRESHOLD, 0.5f * BOMB_CLOSE_THRESHOLD, length));
				if (speed * DELTA_TIME > length)
					speed = length / DELTA_TIME;
				if (speed > 0.001f)
				{
					chasedPlayer = true;
					b->framesUntilIdleMove = RandomInt(&rng, 60, 120);
					b->framesToIdleMove = 0;
					vel = Vector2Scale(toPlayer, speed / length);
				}
			}

			if (!chasedPlayer)
			{
				b->framesUntilIdleMove--;
				b->framesToIdleMove--;
				if (b->framesUntilIdleMove <= 0)
				{
					b->framesToIdleMove = RandomInt(&rng, 30, 120);
					b->framesUntilIdleMove = b->framesToIdleMove + RandomInt(&rng, 0, 60);
					b->idleMoveVel.x = (RandomProbability(&rng, 0.5f) ? -1 : +1) * RandomFloat(&rng, 0.8f * BOMB_IDLE_SPEED, 1.2f * BOMB_IDLE_SPEED);
					b->idleMoveVel.y = (RandomProbability(&rng, 0.5f) ? -1 : +1) * RandomFloat(&rng, 0.8f * BOMB_IDLE_SPEED, 1.2f * BOMB_IDLE_SPEED);
				}
				if (b->framesToIdleMove > 0)
					vel = b->idleMoveVel;
			}

			if (vel.x != 0 || vel.y != 0)
			{
				b->pos = ResolveCollisionsCircleRoom(b->pos, BOMB_RADIUS, vel);
				for (int j = 0; j < numTurrets; ++j)
					b->pos = ResolveCollisionCircles(b->pos, BOMB_RADIUS, turrets[j].pos, TURRET_RADIUS);
			}

			if (CheckCollisionCircles(b->pos, BOMB_RADIUS, player.pos, PLAYER_RADIUS))
			{
				ExplodeBomb(i);
				--i;
			}
		}
	}
}
void UpdateExplosions(void)
{
	for (int i = 0; i < numExplosions; ++i)
	{
		Explosion *e = &explosions[i];
		e->frame++;
		if (e->frame > e->durationFrames)
		{
			RemoveExplosionsFromGlobalList(i);
			--i;
		}
		else
		{
			float t = 2 * Clamp(e->frame / (float)e->durationFrames, 0, 1);
			if (t <= 1)
			{
				float x = 1 - t;
				float r = (1 - x * x * x * x * x * x) * e->radius;

				for (int j = 0; j < numTurrets; ++j)
				{
					Turret *t = &turrets[j];
					if (CheckCollisionCircles(t->pos, TURRET_RADIUS, e->pos, r))
					{
						RemoveTurretFromGlobalList(j);
						--j;
					}
				}
				for (int j = 0; j < numBombs; ++j)
				{
					Bomb *b = &bombs[j];
					if (CheckCollisionCircles(b->pos, BOMB_RADIUS, e->pos, r))
					{
						ExplodeBomb(j);
						--j;
					}
				}

				// @TODO: Collision check with player!
			}
		}
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
	UpdateBombs();
	UpdateExplosions();

	Tile playerTile = TileAtVec(player.pos);
	if (playerTile == TILE_EXIT && NumRemainingEnemies() == 0)
	{
		bool loadedNextLevel = LoadRoom(&currentRoom, nextRoomName);
		if (loadedNextLevel)
			CopyRoomToGame(&currentRoom);
	}

	return GAME_STATE_PLAYING;
}
void Playing_Draw(void)
{
	SetupTileCoordinateDrawing();
	{
		DrawTiles();
		DrawTurrets();
		DrawBombs();
		DrawPlayer();
		DrawBullets();
		DrawExplosions();
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
const Rectangle objectsWindowRect = { 0, SCREEN_HEIGHT - 100, SCREEN_WIDTH, 100 };
const Rectangle propertiesWindowRect = { SCREEN_WIDTH - 200, SCREEN_HEIGHT - 800, 200, 550 };
const Rectangle tilesWindowRect = { 0, SCREEN_HEIGHT - 800, 120, 550 };

void DoTileButton(Tile tile, float x, float y)
{
	int state = GuiGetState();
	if (selection.kind == EDITOR_SELECTION_KIND_TILE && selection.tile == tile)
		GuiSetState(GUI_STATE_PRESSED);
	if (GuiButton(Rect(x, y, 50, 50), GetTileName(tile)))
	{
		selection.kind = EDITOR_SELECTION_KIND_TILE;
		selection.tile = tile;
	}
	DrawRectangleRec(Rect(x + 10, y + 10, 30, 30), ColorAlpha(BLACK, 0.2f));
	GuiSetState(state);
}

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

	if (selection.kind == EDITOR_SELECTION_KIND_TILE && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		int tileX = (int)mousePosTiles.x;
		int tileY = (int)mousePosTiles.y;
		if (tileX >= 0 && tileX < numTilesX && tileY >= 0 && tileY < numTilesY)
			tiles[tileY][tileX] = selection.tile;
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		lastMouseClickPos = mousePos;
		if (selection.kind == EDITOR_SELECTION_KIND_TILE)
		{
			int tileX = (int)mousePosTiles.x;
			int tileY = (int)mousePosTiles.y;
			if (tileX >= 0 && tileX < numTilesX && tileY >= 0 && tileY < numTilesY)
				tiles[tileY][tileX] = selection.tile;
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

				for (int i = 0; i < numBombs; ++i)
				{
					if (CheckCollisionPointCircle(mousePosTiles, bombs[i].pos, BOMB_RADIUS))
					{
						selection.kind = EDITOR_SELECTION_KIND_BOMB;
						selection.bomb = &bombs[i];
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
			case EDITOR_SELECTION_KIND_BOMB:
			{
				Vector2 newPos = Vector2Add(selection.bomb->pos, mouseDeltaTiles);
				if (CheckCollisionPointCircle(mousePosTiles, newPos, BOMB_RADIUS))
					selection.bomb->pos = newPos;
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
		DrawBombs();
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

			memset(consoleInputBuffer, 0, sizeof consoleInputBuffer);
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
			Turret *turret = SpawnTurret(Vector2Zero(), 0);
			if (turret)
			{
				selection.kind = EDITOR_SELECTION_KIND_TURRET;
				selection.turret = turret;
			}
		}
		DrawRectangleRec(Rect(x + 10, y + 10, 40, 40), ColorAlpha(BLACK, 0.2f));
		x += 70;

		if (GuiButton(Rect(x, y, 60, 60), "Bomb"))
		{
			Bomb *bomb = SpawnBomb(Vector2Zero());
			if (bomb)
			{
				selection.kind = EDITOR_SELECTION_KIND_BOMB;
				selection.bomb = bomb;
			}
		}
		DrawRectangleRec(Rect(x + 10, y + 10, 40, 40), ColorAlpha(BLACK, 0.2f));
		x += 70;
	}

	GuiWindowBox(tilesWindowRect, "Tiles");
	{
		float x0 = tilesWindowRect.x + 5;
		float y0 = tilesWindowRect.y + 30;
		float x = x0;
		float y = y0;
		int state;

		x = x0;
		DoTileButton(TILE_FLOOR, x, y);
		x += 55;
		DoTileButton(TILE_WALL, x, y);
		y += 55;

		x = x0;
		DoTileButton(TILE_ENTRANCE, x, y);
		x += 55;
		DoTileButton(TILE_EXIT, x, y);
		y += 55;
	}

	const char *propertiesTitle = NULL;
	switch (selection.kind)
	{
		case EDITOR_SELECTION_KIND_PLAYER: propertiesTitle = "Player properties"; break;
		case EDITOR_SELECTION_KIND_TURRET: propertiesTitle = "Turret properties"; break;
		case EDITOR_SELECTION_KIND_BOMB: propertiesTitle = "Bomb properties"; break;
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

			case EDITOR_SELECTION_KIND_BOMB:
			{
				GuiText(Rect(x, y, 100, 20), "X: %.2f", selection.bomb->pos.x);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Y: %.2f", selection.bomb->pos.y);
				y += 20;
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

				Rectangle prevNameRect = Rect(x, y, 140, 20);
				isFocused = CheckCollisionPointRec(lastMouseClickPos, prevNameRect);
				GuiTextBox(prevNameRect, previousRoomName, sizeof previousRoomName, isFocused);
				GuiLabel(Rect(x + 145, y, 20, 20), "Prev");
				y += 25;

				Rectangle nextNameRect = Rect(x, y, 140, 20);
				isFocused = CheckCollisionPointRec(lastMouseClickPos, nextNameRect);
				GuiTextBox(nextNameRect, nextRoomName, sizeof nextRoomName, isFocused);
				GuiLabel(Rect(x + 145, y, 20, 20), "Next");
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
	explosionSound = LoadSound("res/explosion.wav");
	turretDestroySound = LoadSound("res/turret-destroy.wav");
	SetSoundVolume(turretDestroySound, 0.3f);

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