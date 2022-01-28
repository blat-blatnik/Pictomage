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
#define MAX_GLASS_BOXES 50
#define MAX_POPUP_MESSAGE_LENGTH 128
#define MAX_TRIGGER_MESSAGES 10
#define MAX_TEXTURE_VARIANTS 32
#define MAX_SPARKS 100
#define MAX_SHARDS 500
#define MAX_DECALS 30

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
#define RESTARTING_DURATION 0.8f
#define GRACE_PERIOD 2.0f
#define POPUP_ANIMATION_TIME 0.6f

// *---=======---*
// |/   Types   \|
// *---=======---*

typedef enum GameState
{
	GAME_STATE_MAIN_MENU,
	GAME_STATE_FADE_IN,
	GAME_STATE_PLAYING,
	GAME_STATE_LEVEL_TRANSITION,
	GAME_STATE_LEVEL_EDITOR,
	GAME_STATE_PAUSED,
	GAME_STATE_GAME_OVER,
	GAME_STATE_CREDITS,
	GAME_STATE_RESTARTING,
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
	// ----------------
	TILE_ENUM_COUNT
} Tile;

typedef enum EditorSelectionKind
{
	EDITOR_SELECTION_KIND_NONE,
	EDITOR_SELECTION_KIND_TILE,
	EDITOR_SELECTION_KIND_PLAYER,
	EDITOR_SELECTION_KIND_TURRET,
	EDITOR_SELECTION_KIND_BOMB,
	EDITOR_SELECTION_KIND_GLASS_BOX,
	EDITOR_SELECTION_KIND_TRIGGER_MESSAGE,
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
	bool isAlive;
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
	int framesUntilIdleMove;
	int framesToIdleMove;
	float idleAngularVel;
	bool isDestroyed;
	u8 variant;
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
	u8 variant;
} Bomb;

typedef struct GlassBox
{
	Rectangle rect;
} GlassBox;

typedef struct TriggerMessage
{
	Rectangle rect;
	bool once;
	bool isTriggered;
	bool wasTriggerred;
	double enterTime; // For animations.
	double leaveTime;
	char message[MAX_POPUP_MESSAGE_LENGTH];
} TriggerMessage;

typedef struct Room
{
	char name[MAX_ROOM_NAME];
	char next[MAX_ROOM_NAME];

	int numTilesX;
	int numTilesY;
	Tile tiles[MAX_TILES_Y][MAX_TILES_X];
	u8 tileVariants[MAX_TILES_Y][MAX_TILES_X];

	Color evenTileTint0;
	Color evenTileTint1;
	Color oddTileTint0;
	Color oddTileTint1;

	Vector2 playerDefaultPos;
	
	int numTurrets;
	Vector2 turretPos[MAX_TURRETS];
	float turretLookAngle[MAX_TURRETS];
	bool turretIsDestroyed[MAX_TURRETS];
	u8 turretVariants[MAX_TURRETS];

	int numBombs;
	Vector2 bombPos[MAX_BOMBS];
	u8 bombVariants[MAX_BOMBS];

	int numGlassBoxes;
	Rectangle glassBoxRects[MAX_GLASS_BOXES];

	int numTriggerMessages;
	Rectangle triggerMessagesRects[MAX_TRIGGER_MESSAGES];
	bool triggerMessagesOnce[MAX_TRIGGER_MESSAGES];
	char triggerMessages[MAX_TRIGGER_MESSAGES][MAX_POPUP_MESSAGE_LENGTH];
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
		struct { Tile tile; u8 tileVariant; };
		Player *player;
		Turret *turret;
		Bomb *bomb;
		GlassBox *glassBox;
		TriggerMessage *triggerMessage;
	};
} EditorSelection;

typedef struct TextureVariants
{
	int numVariants;
	Texture variants[MAX_TEXTURE_VARIANTS];
} TextureVariants;

typedef struct Spark
{
	double spawnTime;
	float lifetime;
	Vector2 pos;
	Vector2 vel;
} Spark;

typedef struct Shard
{
	Vector2 pos;
	Vector2 vel;
	Vector2 size;
	float friction;
	Color color;
} Shard;

typedef struct Decal
{
	Texture *texture; // @TODO: This is unused right now.
	Vector2 pos;
	Vector2 size;
	float rotation;
} Decal;

// *---========---*
// |/   Camera   \|
// *---========---*

Random rng;
float cameraZoom = 1;
Vector2 cameraPos; // In tiles.
float screenShakeDuration;
float screenShakeIntensity;
float screenShakeDamping;

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
float TilesToPixels(float tiles)
{
	return tiles * TILE_SIZE;
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
void ScreenShake(float intensity, float duration, float damping)
{
	screenShakeDuration = duration;
	screenShakeIntensity = intensity;
	screenShakeDamping = damping;
}
void DoScreenShake(void)
{
	if (screenShakeDuration > 0)
	{
		Vector2 cameraOffset = RandomVector(&rng, screenShakeIntensity);
		screenShakeDuration -= DELTA_TIME;
		screenShakeIntensity *= screenShakeDamping;
		rlTranslatef(cameraOffset.x, cameraOffset.y, 0);
	}
}

// *---=========---*
// |/   Globals   \|
// *---=========---*

bool godMode = true; //@TODO: Disable this for release.
bool devMode = false; //@TODO: Disable this for release.
const char *devModeStartRoom = "room0";
double timeAtStartOfFrame;
const Vector2 screenCenter = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
const Rectangle screenRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
const Rectangle screenRectTiles = { 0, 0, MAX_TILES_X, MAX_TILES_Y };
// Color evenTileTint0; //@TODO: These are currently unused.
// Color evenTileTint1;
// Color oddTileTint0;
// Color oddTileTint1;
char roomName[MAX_ROOM_NAME];
char nextRoomName[MAX_ROOM_NAME];
int numTilesX = MAX_TILES_X;
int numTilesY = MAX_TILES_Y;
Tile tiles[MAX_TILES_Y][MAX_TILES_X];
u8 tileVariants[MAX_TILES_X][MAX_TILES_Y];
GameState gameState = GAME_STATE_MAIN_MENU;
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
int numGlassBoxes;
GlassBox glassBoxes[MAX_GLASS_BOXES];
int numTriggerMessages;
TriggerMessage triggerMessages[MAX_TRIGGER_MESSAGES];
int numSparks;
Spark sparks[MAX_SPARKS];
int numShards;
int shardCursor;
Shard shards[MAX_SHARDS];
int numDecals;
int decalCursor;
Decal decals[MAX_DECALS];

// *---========---*
// |/   Assets   \|
// *---========---*

Sound flashSound;
Sound longShotSound;
Sound explosionSound;
Sound turretDestroySound;
Sound shutterSound;
Sound glassShatterSound;
Sound bulletHitWallSound;
Sound ringingSound;
Sound shutterEchoSound;
Sound teleportSound;

void LoadAllSounds(void)
{
	flashSound = LoadSound("res/snap.wav");
	longShotSound = LoadSound("res/long-shot.wav");
	SetSoundVolume(longShotSound, 0.1f);
	explosionSound = LoadSound("res/explosion.wav");
	SetSoundVolume(explosionSound, 0.5f);
	turretDestroySound = LoadSound("res/turret-destroy.wav");
	SetSoundVolume(turretDestroySound, 0.3f);
	shutterSound = LoadSound("res/shutter1.wav");
	glassShatterSound = LoadSound("res/shatter.wav");
	bulletHitWallSound = LoadSound("res/bullet-wall.wav");
	ringingSound = LoadSound("res/ringing1.wav");
	SetSoundVolume(ringingSound, 0.5f);
	shutterEchoSound = LoadSound("res/shutter-echo.wav");
	teleportSound = LoadSound("res/teleport.wav");
}
void StopAllLevelSounds(void)
{
	StopSound(flashSound);
	StopSound(longShotSound);
	StopSound(explosionSound);
	StopSound(turretDestroySound);
	StopSound(glassShatterSound);
}

Texture missingTexture;
Texture playerTexture;
Texture creditsTexture;
TextureVariants tileTextureVariants[TILE_ENUM_COUNT];
TextureVariants tileExitOpenVariants;
TextureVariants turretBaseVariants;
TextureVariants turretTopVariants;
TextureVariants destroyedTurretBaseVariants;
TextureVariants destroyedTurretTopVariants;
TextureVariants bombVariants;

void LoadTextureVariants(TextureVariants *tv, const char *baseName)
{
	for (int i = 0; i < MAX_TEXTURE_VARIANTS; ++i)
		tv->variants[i] = missingTexture;

	tv->numVariants = 0;
	for (int i = 0;; ++i)
	{
		const char *path = TempPrint("res/%s%d.png", baseName, i);
		if (!FileExists(path))
			break;

		tv->variants[tv->numVariants++] = LoadTexture(path);
	}
}
void LoadAllTextures(void)
{
	missingTexture = LoadTexture("res/missing.png");
	LoadTextureVariants(&tileTextureVariants[TILE_FLOOR], "floor");
	LoadTextureVariants(&tileTextureVariants[TILE_WALL], "wall");
	LoadTextureVariants(&tileTextureVariants[TILE_ENTRANCE], "entrance");
	LoadTextureVariants(&tileTextureVariants[TILE_EXIT], "exit-closed");
	LoadTextureVariants(&tileExitOpenVariants, "exit-open");

	LoadTextureVariants(&turretBaseVariants, "turret-base");
	LoadTextureVariants(&turretTopVariants, "turret-top");
	LoadTextureVariants(&bombVariants, "bomb");
	LoadTextureVariants(&destroyedTurretBaseVariants, "turret-base-destroyed");
	LoadTextureVariants(&destroyedTurretTopVariants, "turret-top-destroyed");

	playerTexture = LoadTexture("res/player-evil.png");
	creditsTexture = LoadTexture("res/credits.png");
}

// *---=======---*
// |/   Tiles   \|
// *---=======---*

int NumRemainingEnemies(void)
{
	int result = numBombs + numCapturedBombs;
	for (int i = 0; i < numTurrets; ++i)
		if (!turrets[i].isDestroyed)
			++result;
	for (int i = 0; i < numCapturedTurrets; ++i)
		if (!capturedTurrets[i].isDestroyed)
			++result;
	return result;
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
		case TILE_EXIT:
		case TILE_ENTRANCE:
		case TILE_FLOOR:
			return true;
		case TILE_WALL:
		case TILE_NONE:
		default:
			return false;
	}
}
bool TileIsOpaque(Tile tile)
{
	switch (tile)
	{
		case TILE_WALL:
		default:
			return true;
		case TILE_NONE:
		case TILE_FLOOR:
		case TILE_ENTRANCE:
		case TILE_EXIT:
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
	return TileAt((int)floorf(xy.x), (int)floorf(xy.y));
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
	Vector2 p0f = center;
	Vector2 p1f = Vector2Add(center, Vector2Scale(velocity, DELTA_TIME));

	// Room boundaries
	if (p1f.x - radius < 0)
		p1f.x = radius;
	if (p1f.y - radius < 0)
		p1f.y = radius;
	if (p1f.x + radius > numTilesX)
		p1f.x = (float)numTilesX - radius;
	if (p1f.y + radius > numTilesY)
		p1f.y = (float)numTilesY - radius;

	// Glass Boxes
	for (int i = 0; i < numGlassBoxes; ++i)
	{
		GlassBox box = glassBoxes[i];
		p1f = ResolveCollisionCircleRec(p1f, radius, box.rect);
	}

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
				p1f = ResolveCollisionCircleRec(p1f, radius, Rect(tx, ty, 1, 1));
		}
	}

	return p1f;
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
	turret->framesUntilIdleMove = RandomInt(&rng, 30, 60);
	turret->framesToIdleMove = 0;
	turret->idleAngularVel = 0;
	turret->isDestroyed = false;
	turret->variant = 0;
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
	bomb->variant = 0;
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
GlassBox *SpawnGlassBox(Rectangle rect)
{
	if (numGlassBoxes >= MAX_GLASS_BOXES)
		return NULL;

	GlassBox *box = &glassBoxes[numGlassBoxes++];
	box->rect = rect;
	return box;
}
TriggerMessage *SpawnTriggerMessage(Rectangle rect, bool once, const char *message)
{
	if (numTriggerMessages >= MAX_TRIGGER_MESSAGES)
		return NULL;

	TriggerMessage *tm = &triggerMessages[numTriggerMessages++];
	tm->rect = rect;
	tm->once = once;
	tm->isTriggered = false;
	tm->wasTriggerred = false;
	tm->enterTime = 0;
	tm->leaveTime = INFINITY;
	memset(tm->message, 0, sizeof tm->message);
	snprintf(tm->message, sizeof tm->message, "%s", message);
	return tm;
}
Spark *SpawnSpark(Vector2 pos, Vector2 vel, float lifetime)
{
	if (numSparks >= MAX_SPARKS)
		return NULL;

	Spark *spark = &sparks[numSparks++];
	spark->pos = pos;
	spark->vel = vel;
	spark->spawnTime = timeAtStartOfFrame;
	spark->lifetime = lifetime;
	return spark;
}
Shard *SpawnShard(Vector2 pos, Vector2 vel, Vector2 size, Color color, float friction)
{
	Shard *shard = &shards[shardCursor++];
	++numShards;
	if (numShards > MAX_SHARDS)
		numShards = MAX_SHARDS;
	if (shardCursor >= MAX_SHARDS)
		shardCursor = 0;

	shard->pos = pos;
	shard->vel = vel;
	shard->size = size;
	shard->color = color;
	shard->friction = friction;
	return shard;
}
Decal *SpawnDecal(Vector2 pos, Vector2 size, float angleRadians)
{
	Decal *decal = &decals[decalCursor++];
	++numDecals;
	if (numDecals > MAX_DECALS)
		numDecals = MAX_DECALS;
	if (decalCursor >= MAX_DECALS)
		decalCursor = 0;

	decal->pos = pos;
	decal->size = size;
	decal->rotation = angleRadians;
	decal->texture = NULL;
	return decal;
}

void DespawnBullet(int index)
{
	ASSERT(index >= 0 && index < numBullets);
	SwapMemory(bullets + index, bullets + numBullets - 1, sizeof bullets[0]);
	--numBullets;
}
void DespawnTurret(int index)
{
	ASSERT(index >= 0 && index < numTurrets);
	SwapMemory(turrets + index, turrets + numTurrets - 1, sizeof turrets[0]);
	--numTurrets;
}
void DespawnBomb(int index)
{
	ASSERT(index >= 0 && index < numBombs);
	SwapMemory(bombs + index, bombs + numBombs - 1, sizeof bombs[0]);
	--numBombs;
}
void DespawnExplosion(int index)
{
	ASSERT(index >= 0 && index < numExplosions);
	SwapMemory(explosions + index, explosions + numExplosions - 1, sizeof explosions[0]);
	--numExplosions;
}
void DespawnGlassBox(int index)
{
	ASSERT(index >= 0 && index < numGlassBoxes);
	SwapMemory(glassBoxes + index, glassBoxes + numGlassBoxes - 1, sizeof glassBoxes[0]);
	--numGlassBoxes;
}
void DespawnTriggerMessage(int index)
{
	ASSERT(index >= 0 && index < numTriggerMessages);
	SwapMemory(triggerMessages + index, triggerMessages + numTriggerMessages - 1, sizeof triggerMessages[0]);
	--numTriggerMessages;
}
void DespawnSpark(int index)
{
	ASSERT(index >= 0 && index < numSparks);
	SwapMemory(sparks + index, sparks + numSparks - 1, sizeof sparks[0]);
	--numSparks;
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
	for (int i = 0; i < numGlassBoxes; ++i)
	{
		glassBoxes[i].rect.x += dx;
		glassBoxes[i].rect.y += dy;
	}
}
void ExplodeBomb(int index)
{
	assert(index >= 0 && index < numBombs);
	Bomb *bomb = &bombs[index];
	SpawnExplosion(bomb->pos, BOMB_EXPLOSION_DURATION, BOMB_EXPLOSION_RADIUS);

	int debrisCount = RandomInt(&rng, 50, 100);
	for (int i = 0; i < debrisCount; ++i)
	{
		float speed = RandomFloat(&rng, 15, 25);
		Vector2 dir = RandomVector(&rng, speed);
		Vector2 size = {
			RandomFloat(&rng, 0.1f, 0.2f),
			RandomFloat(&rng, 0.1f, 0.2f),
		};

		Color accent = RandomProbability(&rng, 0.5f) ? RED : GRAY;
		Color color = LerpColor(BLACK, accent, RandomFloat(&rng, 0, 0.25f));
		SpawnShard(bomb->pos, dir, size, color, 0.85f);
	}
	SpawnDecal(bomb->pos, Vec2Broadcast(16 * BOMB_RADIUS), 0);

	PlaySound(explosionSound);
	SetSoundPitch(explosionSound, RandomFloat(&rng, 0.8f, 1.2f));
	ScreenShake(1, 0.3f, 0);
	DespawnBomb(index);
}
void KillPlayer(Vector2 direction, float intensity)
{
	//@TODO: Spawn some gore?
	if (!godMode)
	{
		direction = Vector2Normalize(direction);
		player.isAlive = false;
		ScreenShake(0.5f, 0.05f, 0);
	}
}
void DoCollisionSparks(Vector2 incident, Vector2 normal, Vector2 pos, int minSparksNorm, int maxSparksNorm, int minSparksRefl, int maxSparksRefl)
{
	incident = Vector2Normalize(incident);
	normal = Vector2Normalize(normal);
	Vector2 reflection = Vector2Reflect(incident, normal);
	float dot = Clamp(-Vector2DotProduct(incident, normal), 0.2f, 1);

	float normAngle = Vec2Angle(normal);
	int numSparks1 = RandomInt(&rng, minSparksNorm, maxSparksNorm);
	for (int i = 0; i < numSparks1; ++i)
	{
		float r = Clamp(RandomNormal(&rng, 0, dot).x, -PI / 2, +PI / 2);
		Vector2 vel = Vec2FromPolar(RandomFloat(&rng, 4, 8), normAngle + r);
		SpawnSpark(pos, vel, RandomFloat(&rng, 0.3f, 0.6f));
	}

	float reflAngle = Vec2Angle(reflection);
	int numSparks2 = RandomInt(&rng, minSparksRefl, maxSparksRefl);
	for (int i = 0; i < numSparks2; ++i)
	{
		float r = Clamp(RandomNormal(&rng, 0, dot).x, -PI / 2, +PI / 2);
		Vector2 vel = Vec2FromPolar(RandomFloat(&rng, 4, 8), reflAngle + r);
		SpawnSpark(pos, vel, RandomFloat(&rng, 0.3f, 0.6f));
	}
}
void ShatterGlassBox(int index, Vector2 pos, Vector2 incident, float alignedForce, float unalignedForce)
{
	incident = Vector2Normalize(incident);

	GlassBox *box = &glassBoxes[index];
	Rectangle rect = box->rect;
	float area = rect.width * rect.height;
	float pitch = Clamp(Lerp(1, 0.5f, area / 10), 0.6f, 1.0f) + RandomFloat(&rng, -0.1f, 0.1f);
	SetSoundPitch(glassShatterSound, pitch);
	PlaySound(glassShatterSound);
	ScreenShake(0.1f, 0.1f, 0);

	const Color color1 = ColorAlpha(DARKBLUE, 0.3f);
	const Color color2 = ColorAlpha(SKYBLUE, 0.3f);

	Vector2 center = RectangleCenter(rect);
	int count = (int)Lerp(20, 200, Clamp(area / 10, 0, 1));
	for (int i = 0; i < count; ++i)
	{
		Vector2 debrisPos = {
			RandomFloat(&rng, rect.x, rect.x + rect.width),
			RandomFloat(&rng, rect.y, rect.y + rect.height)
		};

		Vector2 offset = Vector2Normalize(Vector2Subtract(debrisPos, center));
		float dot = Clamp(Vector2DotProduct(offset, incident), 0, 1);
		float force = 20 * Lerp(unalignedForce, alignedForce, dot);
		Vector2 vel = Vector2Scale(offset, force * RandomFloat(&rng, 0.1f, +1.1f));
		Color color = LerpColor(color1, color2, RandomFloat01(&rng));
		SpawnShard(debrisPos, vel, Vec2(RandomFloat(&rng, 0.15f, 0.25f), RandomFloat(&rng, 0.15f, 0.25f)), color, 0.95f);
	}

	DespawnGlassBox(index);
}

void CenterCameraOnLevel(void)
{
	cameraZoom = 1;
	cameraPos = Vector2Zero();
	ShiftCamera(-(numTilesX - MAX_TILES_X) / 2.0f, -(numTilesY - MAX_TILES_Y) / 2.0f);
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
	char next[MAX_ROOM_NAME];
	u8 numTilesX = 1;
	u8 numTilesY = 1;
	u8 tiles[MAX_TILES_Y][MAX_TILES_X] = { 0 };
	u8 tileVariants[MAX_TILES_Y][MAX_TILES_X] = { 0 };
	Vector2 playerDefaultPos = Vec2(0.5f, 0.5f);
	u8 numTurrets = 0;
	Vector2 turretPos[MAX_TURRETS] = { 0 };
	float turretLookAngle[MAX_TURRETS] = { 0 };
	bool turretIsDestroyed[MAX_TURRETS] = { 0 };
	u8 turretVariants[MAX_TURRETS] = { 0 };
	u8 numBombs = 0;
	Vector2 bombPos[MAX_BOMBS] = { 0 };
	u8 bombVariants[MAX_BOMBS] = { 0 };
	u8 numGlassBoxes = 0;
	Rectangle glassBoxRects[MAX_GLASS_BOXES] = { 0 };
	u8 numTriggerMessages = 0;
	Rectangle triggerMessagesRects[MAX_TRIGGER_MESSAGES] = { 0 };
	bool triggerMessagesOnce[MAX_TRIGGER_MESSAGES] = { 0 };
	char triggerMessages[MAX_TRIGGER_MESSAGES][MAX_POPUP_MESSAGE_LENGTH] = { 0 };

	fread(&flags, sizeof flags, 1, file);
	fread(next, sizeof next, 1, file);
	fread(&numTilesX, sizeof numTilesX, 1, file);
	fread(&numTilesY, sizeof numTilesY, 1, file);
	for (u8 y = 0; y < numTilesY; ++y)
		fread(tiles[y], sizeof tiles[0][0], numTilesX, file);
	for (u8 y = 0; y < numTilesY; ++y)
		fread(tileVariants[y], sizeof tileVariants[0][0], numTilesX, file);
	fread(&playerDefaultPos, sizeof playerDefaultPos, 1, file);
	fread(&numTurrets, sizeof numTurrets, 1, file);
	fread(turretPos, sizeof turretPos[0], numTurrets, file);
	fread(turretLookAngle, sizeof turretLookAngle[0], numTurrets, file);
	fread(turretIsDestroyed, sizeof turretIsDestroyed[0], numTurrets, file);
	fread(turretVariants, sizeof turretVariants[0], numTurrets, file);
	fread(&numBombs, sizeof numBombs, 1, file);
	fread(bombPos, sizeof bombPos[0], numBombs, file);
	fread(bombVariants, sizeof bombVariants[0], numBombs, file);
	fread(&numGlassBoxes, sizeof numGlassBoxes, 1, file);
	fread(glassBoxRects, sizeof glassBoxRects[0], numGlassBoxes, file);
	fread(&numTriggerMessages, sizeof numTriggerMessages, 1, file);
	fread(triggerMessagesRects, sizeof triggerMessagesRects[0], numTriggerMessages, file);
	fread(triggerMessagesOnce, sizeof triggerMessagesOnce[0], numTriggerMessages, file);
	fread(triggerMessages, sizeof triggerMessages[0], numTriggerMessages, file);
	fclose(file);

	char name[sizeof room->name];
	snprintf(name, sizeof name, "%s", filename);
	memset(room, 0, sizeof room[0]);
	memcpy(room->name, name, sizeof room->name);
	memcpy(room->next, next, sizeof room->next);
	room->numTilesX = (int)numTilesX;
	room->numTilesY = (int)numTilesY;
	for (u8 y = 0; y < numTilesY; ++y)
		for (u8 x = 0; x < numTilesX; ++x)
			room->tiles[y][x] = (Tile)tiles[y][x];
	memcpy(room->tileVariants, tileVariants, sizeof tileVariants);
	room->playerDefaultPos = playerDefaultPos;
	room->numTurrets = (int)numTurrets;
	memcpy(room->turretPos, turretPos, sizeof turretPos);
	memcpy(room->turretLookAngle, turretLookAngle, sizeof turretLookAngle);
	memcpy(room->turretIsDestroyed, turretIsDestroyed, sizeof turretIsDestroyed);
	memcpy(room->turretVariants, turretVariants, sizeof turretVariants);
	room->numBombs = (int)numBombs;
	memcpy(room->bombPos, bombPos, sizeof bombPos);
	memcpy(room->bombVariants, bombVariants, sizeof bombVariants);
	room->numGlassBoxes = (int)numGlassBoxes;
	memcpy(room->glassBoxRects, glassBoxRects, sizeof glassBoxRects);
	room->numTriggerMessages = numTriggerMessages;
	memcpy(room->triggerMessagesRects, triggerMessagesRects, sizeof triggerMessagesRects);
	memcpy(room->triggerMessagesOnce, triggerMessagesOnce, sizeof triggerMessagesOnce);
	memcpy(room->triggerMessages, triggerMessages, sizeof triggerMessages);
	
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
	for (u8 y = 0; y < numTilesY; ++y)
		fwrite(tileVariants[y], sizeof tileVariants[0][0], numTilesX, file);

	fwrite(&room->playerDefaultPos, sizeof room->playerDefaultPos, 1, file);

	u8 numTurrets = (u8)room->numTurrets;
	fwrite(&numTurrets, sizeof numTurrets, 1, file);
	fwrite(room->turretPos, sizeof room->turretPos[0], numTurrets, file);
	fwrite(room->turretLookAngle, sizeof room->turretLookAngle[0], numTurrets, file);
	fwrite(room->turretIsDestroyed, sizeof room->turretIsDestroyed[0], numTurrets, file);
	fwrite(room->turretVariants, sizeof room->turretVariants[0], numTurrets, file);

	u8 numBombs = (u8)room->numBombs;
	fwrite(&numBombs, sizeof numBombs, 1, file);
	fwrite(room->bombPos, sizeof room->bombPos[0], numBombs, file);
	fwrite(room->bombVariants, sizeof room->bombVariants[0], numBombs, file);

	u8 numGlassBoxes = (u8)room->numGlassBoxes;
	fwrite(&numGlassBoxes, sizeof numGlassBoxes, 1, file);
	fwrite(room->glassBoxRects, sizeof room->glassBoxRects[0], numGlassBoxes, file);

	u8 numTriggerMessages = (u8)room->numTriggerMessages;
	fwrite(&numTriggerMessages, sizeof numTriggerMessages, 1, file);
	fwrite(room->triggerMessagesRects, sizeof room->triggerMessagesRects[0], numTriggerMessages, file);
	fwrite(room->triggerMessagesOnce, sizeof room->triggerMessagesOnce[0], numTriggerMessages, file);
	fwrite(room->triggerMessages, sizeof room->triggerMessages[0], numTriggerMessages, file);

	fclose(file);
	TraceLog(LOG_INFO, "Saved room '%s'.", filepath);
}
void CopyRoomToGame(Room *room)
{
	memcpy(roomName, room->name, sizeof roomName);
	memcpy(nextRoomName, room->next, sizeof nextRoomName);

	StopAllLevelSounds();

	numBullets = 0;
	numTurrets = 0;
	numBombs = 0;
	numGlassBoxes = 0;
	numTriggerMessages = 0;
	numCapturedBullets = 0;
	numCapturedTurrets = 0;
	numCapturedBombs = 0;
	numExplosions = 0;
	numSparks = 0;
	numShards = 0;
	shardCursor = 0;
	numDecals = 0;
	decalCursor = 0;
	player.hasCapture = false;
	player.justSnapped = false;
	player.isReleasingCapture = false;
	player.vel = Vector2Zero();
	player.releasePos = Vector2Zero();
	player.isAlive = true;
	screenShakeDuration = 0;
	screenShakeIntensity = 0;
	screenShakeDamping = 0;

	numTilesX = room->numTilesX;
	numTilesY = room->numTilesY;
	for (int y = 0; y < numTilesY; ++y)
	{
		memcpy(tiles[y], room->tiles[y], sizeof tiles[0]);
		memcpy(tileVariants[y], room->tileVariants[y], sizeof tileVariants[0]);
	}

	player.pos = room->playerDefaultPos;
	for (int i = 0; i < room->numTurrets; ++i)
	{
		Turret *turret = SpawnTurret(room->turretPos[i], room->turretLookAngle[i]);
		if (turret)
		{
			turret->variant = room->turretVariants[i];
			turret->isDestroyed = room->turretIsDestroyed[i];
		}
	}
	for (int i = 0; i < room->numBombs; ++i)
	{
		Bomb *bomb = SpawnBomb(room->bombPos[i]);
		if (bomb)
		{
			bomb->variant = room->bombVariants[i];
		}
	}
	for (int i = 0; i < room->numGlassBoxes; ++i)
		SpawnGlassBox(room->glassBoxRects[i]);
	for (int i = 0; i < room->numTriggerMessages; ++i)
		SpawnTriggerMessage(room->triggerMessagesRects[i], room->triggerMessagesOnce[i], room->triggerMessages[i]);
}
void CopyGameToRoom(Room *room)
{
	char name[sizeof room->name];
	memcpy(name, room->name, sizeof name);
	memset(room, 0, sizeof room[0]);
	memcpy(room->name, name, sizeof name);
	memcpy(room->next, nextRoomName, sizeof room->next);
	room->numTilesX = numTilesX;
	room->numTilesY = numTilesY;
	for (int y = 0; y < numTilesY; ++y)
	{
		memcpy(room->tiles[y], tiles[y], numTilesX * sizeof tiles[y][0]);
		memcpy(room->tileVariants[y], tileVariants[y], numTilesX * sizeof tileVariants[y][0]);
	}
	room->playerDefaultPos = player.pos;
	room->numTurrets = numTurrets;
	for (int i = 0; i < numTurrets; ++i)
	{
		Turret t = turrets[i];
		room->turretPos[i] = t.pos;
		room->turretLookAngle[i] = t.lookAngle;
		room->turretIsDestroyed[i] = t.isDestroyed;
		room->turretVariants[i] = t.variant;
	}
	room->numBombs = numBombs;
	for (int i = 0; i < numBombs; ++i)
	{
		Bomb b = bombs[i];
		room->bombPos[i] = b.pos;
		room->bombVariants[i] = b.variant;
	}
	room->numGlassBoxes = numGlassBoxes;
	for (int i = 0; i < numGlassBoxes; ++i)
	{
		GlassBox box = glassBoxes[i];
		room->glassBoxRects[i] = box.rect;
	}
	room->numTriggerMessages = numTriggerMessages;
	for (int i = 0; i < numTriggerMessages; ++i)
	{
		TriggerMessage tm = triggerMessages[i];
		room->triggerMessagesRects[i] = tm.rect;
		room->triggerMessagesOnce[i] = tm.once;
		memcpy(&room->triggerMessages[i], tm.message, sizeof tm.message);
	}
}

// *---========---*
// |/   Update   \|
// *---========---*

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
					DespawnBullet(i);
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
					DespawnBomb(i);
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
					DespawnTurret(i);
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
				turret->isDestroyed = t.isDestroyed;
				turret->flingVelocity = releaseVel;
				turret->variant = t.variant;
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
				bomb->variant = b.variant;
			}
		}
		numCapturedBullets = 0;
		numCapturedTurrets = 0;
		numCapturedBombs = 0;
	}
}
void UpdateBullets(void)
{
	// Update each bullet multiple times for increased collision precision.
	// Otherwise bullets phase through 1 tile thin walls.
	const int numIterations = 4; // @SPEED: Maybe 2 or 3 iterations is enough. Although we never have too many bullets.
	for (int iter = 0; iter < numIterations; ++iter)
	{
		for (int i = 0; i < numBullets; ++i)
		{
			Bullet *b = &bullets[i];
			b->pos = Vector2Add(b->pos, Vector2Scale(b->vel, DELTA_TIME / numIterations));
			if (!CheckCollisionCircleRec(b->pos, BULLET_RADIUS, ExpandRectangle(screenRectTiles, 20)))
			{
				DespawnBullet(i);
				--i;
			}
			else if (!TileIsPassable(TileAtVec(b->pos))) // @TODO: This is treating bullets as points even though they have a radius..
			{
				int tx = (int)b->pos.x;
				int ty = (int)b->pos.y;
				RayCollision collision = GetProjectileCollisionWithRect(b->origin, b->vel, Rect(tx, ty, 1, 1));
				Vector2 normal = { collision.normal.x, collision.normal.y };
				Vector2 pos = { collision.point.x, collision.point.y };
				DoCollisionSparks(b->vel, normal, pos, 10, 20, 0, 1);

				int shardCount = RandomInt(&rng, 2, 5);
				float normalAngle = Vec2Angle(normal);
				float bulletSpeed = Vector2Length(b->vel);
				for (int j = 0; j < shardCount; ++j)
				{
					float angle = Clamp(RandomNormal(&rng, 0, PI / 6).x, -PI / 2, +PI / 2);
					angle += normalAngle;
					float speed = RandomFloat(&rng, 0.1f * bulletSpeed, 0.4f * bulletSpeed);
					Vector2 vel = Vec2FromPolar(speed, angle);
					Vector2 size = {
						RandomFloat(&rng, 0.05f, 0.10f),
						RandomFloat(&rng, 0.05f, 0.10f),
					};
					SpawnShard(pos, vel, size, BLACK, 0.9f); //@TODO: Wall color.
				}

				SetSoundPitch(bulletHitWallSound, RandomFloat(&rng, 0.8f, 1.3f));
				PlaySound(bulletHitWallSound);
				DespawnBullet(i);
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
						Vector2 pos = ResolveCollisionCircles(b->pos, 0.01f, t->pos, TURRET_RADIUS);
						Vector2 normal = Vector2Normalize(Vector2Subtract(pos, t->pos));
						Vector2 incident = Vector2Normalize(b->vel);
						DoCollisionSparks(incident, normal, pos, 10, 20, 10, 20);

						float hitSpeed = Vector2Length(b->vel);
						float hitAngle = Vec2Angle(incident);
						int debrisCount = RandomInt(&rng, 10, 20);
						Vector2 debrisPos = Vector2Scale(Vector2Add(pos, t->pos), 0.5f);
						for (int k = 0; k < debrisCount; ++k)
						{
							float randAngle = Clamp(RandomNormal(&rng, 0, PI / 8).x, -PI / 2, +PI / 2);
							float angle = hitAngle + randAngle;
							float speed = RandomFloat(&rng, 0.5f * hitSpeed, 1.0f * hitSpeed);
							Vector2 vel = Vec2FromPolar(speed, angle);
							Vector2 size = {
								RandomFloat(&rng, 0.05f, 0.10f),
								RandomFloat(&rng, 0.05f, 0.10f),
							};
							const Color colors[] = {
								BLACK,
								BLACK,
								{ 80, 80, 80, 0xFF },
								{ 226, 189, 0, 0xFF },
								{ 190, 33, 55, 0xFF },
							};
							int index1 = RandomInt(&rng, 0, COUNTOF(colors));
							int index2 = RandomInt(&rng, 0, COUNTOF(colors));
							Color color1 = colors[index1];
							Color color2 = colors[index2];
							Color color = LerpColor(color1, color2, RandomFloat01(&rng));
							SpawnShard(debrisPos, vel, size, color, 0.8f);
						}

						if (t->isDestroyed)
						{
							ScreenShake(0.05f, 0.05f, 0);
							SetSoundVolume(turretDestroySound, 0.1f);
							PlaySound(turretDestroySound);
						}
						else
						{
							t->isDestroyed = true;
							ScreenShake(0.2f, 0.1f, 0);
							SetSoundVolume(turretDestroySound, 0.25f);
							PlaySound(turretDestroySound);
						}

						DespawnBullet(i);
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
						ExplodeBomb(j);
						DespawnBullet(i);
						--i;
						collided = true;
						break;
					}
				}
				if (collided)
					continue;

				for (int j = 0; j < numGlassBoxes; ++j)
				{
					GlassBox *box = &glassBoxes[j];

					//@BUG: CheckCollisionCircleRec is pretty bugged.
					//if (CheckCollisionCircleRec(b->pos, BULLET_RADIUS, box->rect))
					if (CheckCollisionPointRec(b->pos, box->rect))
					{
						ShatterGlassBox(j, b->pos, b->vel, 1, 0.1f);
						--j;
					}
				}

				if (CheckCollisionCircles(b->pos, BULLET_RADIUS, player.pos, PLAYER_RADIUS))
				{
					Vector2 toPlayer = Vector2Subtract(player.pos, b->pos);
					DespawnBullet(i);
					--i;
					KillPlayer(toPlayer, 0.1f);
					continue;
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
		if (Vector2LengthSqr(t->flingVelocity) > 1e-9f)
		{
			Vector2 newPos = Vector2Add(t->pos, Vector2Scale(t->flingVelocity, DELTA_TIME));
			for (int j = 0; j < numGlassBoxes; ++j)
			{
				GlassBox *box = &glassBoxes[j];
				if (CheckCollisionCircleRec(newPos, TURRET_RADIUS, box->rect))
				{
					ShatterGlassBox(j, t->pos, t->flingVelocity, 1, 1.0f);
					--j;
				}
			}

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
					DespawnTurret(i);
					--i;
					continue;
				}
			}

			t->flingVelocity = Vector2Scale(t->flingVelocity, TURRET_FRICTION);
		}

		if (t->isDestroyed)
			continue;

		bool playerIsVisible = player.isAlive && IsPointVisibleFrom(t->pos, player.pos);
		if (playerIsVisible)
			t->lastKnownPlayerPos = player.pos;

		bool triedToShoot = false;
		if (player.isAlive && (t->lastKnownPlayerPos.x != 0 || t->lastKnownPlayerPos.y != 0))
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

						int sparkCount = RandomInt(&rng, 10, 20);
						for (int k = 0; k < sparkCount; ++k)
						{
							float speed = RandomFloat(&rng, 5, 8);
							float angle = RandomNormal(&rng, 0, PI / 4).x;
							angle = t->lookAngle + Clamp(angle, -PI / 2, +PI / 2);
							Vector2 pos = bulletPos;
							Vector2 vel = Vec2FromPolar(speed, angle);
							SpawnSpark(pos, vel, 0.2f);
						}

						//@TODO: Maybe remove?
						ScreenShake(0.02f, 0.05f, 0);
					}
				}
			}

			float turnSpeed = TURRET_TURN_SPEED;
			if (dAngle < 0)
				turnSpeed *= -1;
			t->lookAngle += turnSpeed * DELTA_TIME;
		}
		else
		{
			t->framesUntilIdleMove--;
			t->framesToIdleMove--;
			if (t->framesUntilIdleMove <= 0)
			{
				t->framesToIdleMove = RandomInt(&rng, 30, 120);
				t->framesUntilIdleMove = t->framesToIdleMove + RandomInt(&rng, 0, 60);
				t->idleAngularVel = (RandomProbability(&rng, 0.5f) ? -1 : +1) * TURRET_TURN_SPEED / 3;
			}
			t->lookAngle += t->idleAngularVel * DELTA_TIME;
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
			// Just like bullets, we need to break up this update otherwise the bomb
			// could phase through 1 tile thick walls.
			// @SPEED: Maybe 2 or 3 iteration is enough?
			const int numIters = 4;
			for (int iter = 0; iter < numIters; ++iter)
			{
				Vector2 vel = Vector2Scale(b->flungVel, 1.0f / numIters);
				Vector2 expectedPos = Vector2Add(b->pos, Vector2Scale(vel, DELTA_TIME));

				bool exploded = false;
				b->pos = ResolveCollisionsCircleRoom(b->pos, BOMB_RADIUS, vel);
				if (!Vector2Equal(b->pos, expectedPos))
					exploded = true;

				for (int j = 0; j < numTurrets; ++j)
					if (CheckCollisionCircles(b->pos, BOMB_RADIUS, turrets[j].pos, TURRET_RADIUS))
						exploded = true;
				for (int j = 0; j < numBombs; ++j)
					if (j != i)
						if (CheckCollisionCircles(b->pos, BOMB_RADIUS, bombs[j].pos, BOMB_RADIUS))
							exploded = true;
				for (int j = 0; j < numGlassBoxes; ++j)
					if (CheckCollisionCircleRec(b->pos, BOMB_RADIUS, glassBoxes[j].rect))
						exploded = true;
				if (CheckCollisionCircles(b->pos, BOMB_RADIUS, player.pos, PLAYER_RADIUS))
					exploded = true;

				if (exploded)
				{
					ExplodeBomb(i);
					--i;
					break;
				}
			}
		}
		else
		{
			bool playerIsVisible = player.isAlive && IsPointVisibleFrom(b->pos, player.pos);
			if (playerIsVisible)
				b->lastKnownPlayerPos = player.pos;

			Vector2 vel = Vector2Zero();
			bool chasedPlayer = false;

			if (player.isAlive && (b->lastKnownPlayerPos.x != 0 || b->lastKnownPlayerPos.y != 0))
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
				for (int j = 0; j < numBombs; ++j)
					if (i != j)
						b->pos = ResolveCollisionCircles(b->pos, BOMB_RADIUS, bombs[j].pos, BOMB_RADIUS);
			}

			if (player.isAlive && CheckCollisionCircles(b->pos, BOMB_RADIUS, player.pos, PLAYER_RADIUS))
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
			DespawnExplosion(i);
			--i;
		}
		else
		{
			float time = 2 * Clamp(e->frame / (float)e->durationFrames, 0, 1);
			if (time <= 1)
			{
				float x = 1 - time;
				float r = (1 - x * x * x * x * x * x) * e->radius;

				for (int j = 0; j < numTurrets; ++j)
				{
					Turret *t = &turrets[j];
					if (CheckCollisionCircles(t->pos, TURRET_RADIUS, e->pos, r))
					{
						int debrisCount = RandomInt(&rng, 40, 80);
						for (int k = 0; k < debrisCount; ++k)
						{
							float speed = RandomFloat(&rng, 15, 25);
							Vector2 dir = RandomVector(&rng, speed);
							Vector2 size = {
								RandomFloat(&rng, 0.1f, 0.2f),
								RandomFloat(&rng, 0.1f, 0.2f),
							};

							Color accent = RandomProbability(&rng, 0.5f) ? RED : GRAY;
							Color color = LerpColor(BLACK, accent, RandomFloat(&rng, 0, 0.25f));
							SpawnShard(t->pos, dir, size, color, 0.85f);
						}
						SpawnDecal(t->pos, Vec2Broadcast(4 * TURRET_RADIUS), 0);
						DespawnTurret(j);
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
				for (int j = 0; j < numGlassBoxes; ++j)
				{
					GlassBox *box = &glassBoxes[j];
					if (CheckCollisionCircleRec(e->pos, r, box->rect))
					{
						ShatterGlassBox(j, e->pos, Vector2Subtract(RectangleCenter(box->rect), e->pos), 1, 0.5f);
						--j;
					}
				}

				//@SPEED: We dont really need to do this.. we could also stagger it
				if (time < 0.1f)
				{
					float px = e->pos.x;
					float py = e->pos.y;
					for (int j = 0; j < numShards; ++j)
					{
						Shard *shard = &shards[j];
						float cx = shard->pos.x + 0.5f * shard->size.x;
						float cy = shard->pos.y + 0.5f * shard->size.y;
						float dx = cx - px;
						float dy = cy - py;
						float d2 = dx * dx + dy * dy;
						float id2 = 1 / (1 + d2);
						shard->vel.x += (30 * dx) * id2;
						shard->vel.y += (30 * dy) * id2;
					}
				}

				if (CheckCollisionCircles(player.pos, PLAYER_RADIUS, e->pos, r))
				{
					Vector2 toPlayer = Vector2Subtract(player.pos, e->pos);
					float distance = Vector2Length(toPlayer);
					KillPlayer(toPlayer, 1 / (1 + distance));
				}
			}
		}
	}
}
void UpdateTriggerredMessages(void)
{
	for (int i = 0; i < numTriggerMessages; ++i)
	{
		TriggerMessage *tm = &triggerMessages[i];
		bool innerOverlap = CheckCollisionPointRec(player.pos, tm->rect);
		//bool outerOverlap = CheckCollisionCircleRec(player.pos, PLAYER_RADIUS, tm->rect);
		if (!tm->isTriggered && innerOverlap)
		{
			if (!tm->once || !tm->wasTriggerred)
			{
				tm->enterTime = timeAtStartOfFrame;
				tm->isTriggered = true;
				tm->wasTriggerred = true;
			}
		}
		else if (tm->isTriggered && !innerOverlap)
		{
			tm->isTriggered = false;
			tm->leaveTime = timeAtStartOfFrame;
		}
	}
}
void UpdateSparks(void)
{
	for (int i = 0; i < numSparks; ++i)
	{
		Spark *spark = &sparks[i];
		float lifetime = (float)(timeAtStartOfFrame - spark->spawnTime);
		if (lifetime >= spark->lifetime)
		{
			DespawnSpark(i);
			--i;
		}
	}
}
void UpdateShards(void)
{
	float px = player.pos.x;
	float py = player.pos.y;
	float vx = player.vel.x;
	float vy = player.vel.y;
	for (int i = 0; i < numShards; ++i)
	{
		Shard *shard = &shards[i];
		float cx = shard->pos.x + 0.5f * shard->size.x;
		float cy = shard->pos.y + 0.5f * shard->size.y;
		float dx = cx - px;
		float dy = cy - py;
		float d2 = dx * dx + dy * dy;
		if (d2 <= PLAYER_RADIUS * PLAYER_RADIUS)
		{
			float id2 = 1 / (1 + d2);
			shard->vel.x += (4.0f * dx + 0.1f * vx) * id2;
			shard->vel.y += (4.0f * dy + 0.1f * vy) * id2;
		}
		shard->pos.x += shard->vel.x * DELTA_TIME;
		shard->pos.y += shard->vel.y * DELTA_TIME;
		shard->vel.x *= shard->friction;
		shard->vel.y *= shard->friction;
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

void DrawPopupMessage(float enterTime, float leaveTime, float centerX, float centerY, const char *message)
{
	const float flyInOutTime = 0.2f;
	float t0Enter = Clamp((enterTime) / flyInOutTime, 0, 1);
	float t0Leave = Clamp((leaveTime - 1 + flyInOutTime) / flyInOutTime, 0, 1);
	float t1Enter = Clamp((enterTime - flyInOutTime) / (1 - flyInOutTime), 0, 1);
	float t1Leave = Clamp((leaveTime) / (1 - flyInOutTime), 0, 1);

	char text[MAX_POPUP_MESSAGE_LENGTH];
	uptr len = strlen(message);
	if (len + 1 > MAX_POPUP_MESSAGE_LENGTH)
		len = MAX_POPUP_MESSAGE_LENGTH - 1;
	memcpy(text, message, len);
	text[len] = 0;
	int textLength = (int)len;

	const float fontSize = 32;
	Font font = GetFontDefault();
	Vector2 textSize = MeasureTextEx(font, text, fontSize, 1);

	const float tapePadX = 30;
	const float tapePadY = 10;

	Rectangle tape = {
		centerX - 0.5f * textSize.x - tapePadX,
		centerY - 0.5f * fontSize - tapePadY,
		textSize.x + 2 * tapePadX,
		textSize.y + 2 * tapePadY
	};

	float tapeX0 = tape.x;
	float tapeX1 = tape.x + tape.width;
	float tapeY0 = -tape.height - 10;
	float tapeY1 = tape.y;
	tape.y = Lerp(tapeY0, tapeY1, Clamp(t0Enter - t0Leave, 0, 1));
	tape.x = Lerp(tapeX0, tapeX1, t1Leave);
	tape.width = Lerp(0, tape.width, Clamp(t1Enter - t1Leave, 0, 1));

	Rectangle reel = {
		tape.x + tape.width,
		tape.y - 2,
		30,
		tape.height + 4
	};
	Rectangle reelTop = {
		reel.x - 3,
		reel.y + reel.height,
		reel.width + 6,
		3
	};
	Rectangle reelBottom = {
		reel.x - 3,
		reel.y - 3,
		reel.width + 6,
		3
	};

	float textOffsetX = tape.x - tapeX0;
	float textMaxWidth = tape.width - tapePadX;
	for (int j = 0; j < textLength; ++j)
	{
		char backup = text[j];
		text[j] = 0;
		Vector2 s = MeasureTextEx(font, text, fontSize, 1);
		if (s.x >= textMaxWidth)
			break;
		text[j] = backup;
	}

	DrawRectangleRec(tape, Grayscale(0.15f));
	DrawTextEx(font, text, Vec2(centerX - 0.5f * textSize.x + textOffsetX, centerY - 0.5f * textSize.y), fontSize, 1, RAYWHITE);

	DrawRectangleRec(reel, Grayscale(0.4f));
	DrawRectangleRec(ExpandRectangleEx(reel, +3, +3, -3.5f, -3.5f), Grayscale(0.1f));
	DrawRectangleRec(reelTop, Grayscale(0.3f));
	DrawRectangleRec(reelBottom, Grayscale(0.3f));
}
void DrawPlayer(float alpha)
{
	float ew = PixelsToTiles(playerTexture.width) / 2;
	float eh = PixelsToTiles(playerTexture.height) / 2;
	DrawTexRotated(playerTexture, player.pos, Vec2(ew, eh), ColorAlpha(WHITE, alpha), player.lookAngle + PI / 2);
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
void DrawTiles(bool passable)
{
	rlBegin(RL_QUADS);
	for (int y = 0; y < numTilesY; ++y)
	{
		for (int x = 0; x < numTilesX; ++x)
		{
			Tile tile = tiles[y][x];
			if (TileIsPassable(tile) == passable)
			{
				u8 variant = tileVariants[y][x];
				Texture2D texture;
				if (tile == TILE_EXIT && NumRemainingEnemies() == 0)
					texture = tileExitOpenVariants.variants[variant];
				else
					texture = tileTextureVariants[tile].variants[variant];

				Color tint = ((x + y) % 2 == 0) ? Grayscale(1) : Grayscale(0.95f);

				rlColor(tint);
				rlSetTexture(texture.id);
				rlTexCoord2f(0, 0); rlVertex2f(x + 0, y + 0);
				rlTexCoord2f(1, 0); rlVertex2f(x + 1, y + 0);
				rlTexCoord2f(1, 1); rlVertex2f(x + 1, y + 1);
				rlTexCoord2f(0, 1); rlVertex2f(x + 0, y + 1);
			}
		}
	}
	rlEnd();
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
		Texture topTex;
		Texture botTex;
		if (t.isDestroyed)
		{
			topTex = destroyedTurretTopVariants.variants[t.variant];
			botTex = destroyedTurretBaseVariants.variants[t.variant];
		}
		else
		{
			topTex = turretTopVariants.variants[t.variant];
			botTex = turretBaseVariants.variants[t.variant];
		}
		DrawTex(botTex, t.pos, Vec2Broadcast(TURRET_RADIUS), WHITE);
		DrawTexRotated(topTex, t.pos, Vec2Broadcast(1.5f * TURRET_RADIUS), WHITE, t.lookAngle);
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

		Texture tex = bombVariants.variants[b.variant];
		DrawTex(tex, b.pos, Vec2Broadcast(BOMB_RADIUS), WHITE);
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
void DrawShutter(float t, Color innerColor, Color outerColor, float edgeThickness) // t = 0 (fully open), t = 1 (fully closed)
{
	float angle = 0;
	for (int i = 0; i < 6; ++i)
	{
		float angle1 = angle;
		float angle2 = angle + 60 * DEG2RAD;
		const float size = 1.6f * fmaxf(SCREEN_HEIGHT, SCREEN_WIDTH);

		Vector2 v[3] = {
			screenCenter,
			Vector2Add(screenCenter, Vec2(size * cos(angle1), size * sin(angle1))),
			Vector2Add(screenCenter, Vec2(size * cos(angle2), size * sin(angle2))),
		};

		Vector2 direction = Vector2Normalize(Vector2Subtract(v[2], v[1]));
		for (int j = 0; j < 3; ++j)
			v[j] = Vector2Add(v[j], Vector2Scale(direction, (1 - t) * size));

		DrawTriangle(v[0], v[1], v[2], outerColor);

		Vector2 center = Vector2Zero();
		for (int j = 0; j < 3; ++j)
			center = Vector2Add(center, v[j]);
		center = Vector2Scale(center, 1 / 3.0f);

		for (int j = 0; j < 3; ++j)
		{
			Vector2 toCenter = Vector2Normalize(Vector2Subtract(center, v[j]));
			v[j] = Vector2Add(v[j], Vector2Scale(toCenter, edgeThickness));
		}

		DrawTriangle(v[0], v[1], v[2], innerColor);
		angle = angle2;
	}
}
void DrawGlassBoxes(void)
{
	for (int i = 0; i < numGlassBoxes; ++i)
	{
		GlassBox box = glassBoxes[i];
		//DrawRectanglePro(box.rect, RectangleCenter(box.rect), RAD2DEG * box.rotation, ColorAlpha(BLUE, 0.2f));
		DrawRectangleRec(box.rect, ColorAlpha(BLUE, 0.3f));
	}
}
void DrawTriggerMessages(bool debug)
{
	double timeNow = timeAtStartOfFrame;
	for (int i = 0; i < numTriggerMessages; ++i)
	{
		TriggerMessage tm = triggerMessages[i];
		if (debug)
			DrawRectangleRec(tm.rect, ColorAlpha(ORANGE, 0.3f));
		else
		{
			float enterTime = Clamp((timeNow - tm.enterTime) / POPUP_ANIMATION_TIME, 0, 1);
			float leaveTime = Clamp((timeNow - tm.leaveTime) / POPUP_ANIMATION_TIME, -0.001f, 1); // @HACK: Otherwise message invisible for 1 frame.
			if (tm.leaveTime < tm.enterTime)
				leaveTime = 0;
			if (tm.isTriggered || (leaveTime >= 0 && leaveTime < 1))
				DrawPopupMessage(enterTime, leaveTime, screenCenter.x, 200, tm.message);
		}
	}
}
void DrawSparks(void)
{
	Color color0 = YELLOW;
	Color color1 = ColorAlpha(RED, 0);
	for (int i = 0; i < numSparks; ++i)
	{
		Spark *spark = &sparks[i];
		float dt = (float)(timeAtStartOfFrame - spark->spawnTime);
		Vector2 pos = Vector2Add(spark->pos, Vector2Scale(spark->vel, dt));
		
		float t = Clamp(dt / spark->lifetime, 0, 1);
		Color color = LerpColor(color0, color1, t);
		DrawRectangleRec(RectVec(pos, Vec2Broadcast(0.1f)), color);
	}
}
void DrawShards(void)
{
	for (int i = 0; i < numShards; ++i)
	{
		// @SPEED: We don't really need to do this. It looks a tiny bit better but not much.
		Shard shard = shards[i];
		float x = shard.pos.x;
		float y = shard.pos.y;
		float w = shard.size.x;
		float h = shard.size.y;
		float cx = shard.pos.x + 0.5f * w;
		float cy = shard.pos.y + 0.5f * h;
		int tx = (int)floorf(cx); // Need floor otherwise we round to 0, and -0.5 becomes 0.
		int ty = (int)floorf(cy);
		if (tx >= 0 && tx < numTilesX && ty >= 0 && ty < numTilesY)
		{
			Tile tile = tiles[ty][tx];
			if (TileIsPassable(tile))
				DrawRectangleRec((Rectangle) { x, y, w, h }, shard.color);
		}
	}
}
void DrawDecals(void)
{
	for (int i = 0; i < numDecals; ++i)
	{
		Decal *decal = &decals[i];
		Vector2 center = decal->pos;
		float ew = decal->size.x / 2;
		float eh = decal->size.y / 2;

		u64 hash = HashBytes(&decal->pos, sizeof decal->pos);
		Random rand = SeedRandom(hash);

		float angles[16];
		angles[0] = 0;
		for (int j = 1; j < COUNTOF(angles) - 1; ++j)
			angles[j] = ((float)j / COUNTOF(angles)) * 2 * PI;
		angles[COUNTOF(angles) - 1] = 2 * PI;
		
		//float offset = RandomFloat(&rand, 0, PI);
		//for (int j = 0; j < COUNTOF(angles); ++j)
		//	angles[j] += offset;

		Vector2 points[COUNTOF(angles)];
		for (int j = 0; j < COUNTOF(points); ++j)
		{
			float angle = angles[j];
			float s = sinf(angle);
			float c = cosf(angle);
			float r = RandomFloat(&rand, 0.9f, 1.1f);
			points[j].x = center.x + (c * r) * ew;
			points[j].y = center.y + (s * r) * eh;
		}

		rlBegin(RL_TRIANGLES);
		{
			for (int j = 0; j < COUNTOF(points) - 1; ++j)
			{
				rlColor4f(0, 0, 0, 1); 
				rlVertex2fv(center);
				rlColor4f(0, 0, 0, 0);
				rlVertex2f(points[j + 0].x, points[j + 0].y);
				rlColor4f(0, 0, 0, 0);
				rlVertex2f(points[j + 1].x, points[j + 1].y);
			}
		}
		rlEnd();
	}
}

// *---===========---*
// |/   Main Menu   \|
// *---===========---*

void DrawControls(float alpha, float fillAlpha)
{
	const Color white = ColorAlpha(WHITE, alpha);

	Rectangle rW = Rect(100 + 00.00f, 750 + 00.00f, 25, 25);
	Rectangle rA = Rect(100 - 28.00f, 750 + 28.00f, 25, 25);
	Rectangle rS = Rect(100 + 00.00f, 750 + 28.00f, 25, 25);
	Rectangle rD = Rect(100 + 28.00f, 750 + 28.00f, 25, 25);
	DrawRectangleLinesEx(rW, 2, white);
	DrawRectangleLinesEx(rA, 2, white);
	DrawRectangleLinesEx(rS, 2, white);
	DrawRectangleLinesEx(rD, 2, white);
	Vector2 cW = RectangleCenter(rW);
	Vector2 cA = RectangleCenter(rA);
	Vector2 cS = RectangleCenter(rS);
	Vector2 cD = RectangleCenter(rD);
	DrawTextCentered("W", cW.x, cW.y, 12, white);
	DrawTextCentered("A", cA.x, cA.y, 12, white);
	DrawTextCentered("S", cS.x, cS.y, 12, white);
	DrawTextCentered("D", cD.x, cD.y, 12, white);

	Rectangle rMM = Rect(100 - 28.00f, 830 + 00.00f, 30, 40);
	DrawRectangleRoundedLines(rMM, 0.3f, 8, 2, white);
	DrawLineEx(Vec2(rMM.x, rMM.y + 20), Vec2(rMM.x + rMM.width, rMM.y + 20), 2, white);
	DrawLineEx(Vec2(rMM.x + rMM.width / 2, rMM.y), Vec2(rMM.x + rMM.width / 2, rMM.y + 20), 2, white);
	DrawRectangleRounded(Rect(rMM.x + rMM.width / 2 - 2, rMM.y + 5, 4, 10), 0.3f, 8, white);

	Rectangle rMC = Rect(100 + 24.00f, 830 + 00.00f, 30, 40);
	{
		// We need to make this one transparent when the main menu flash happens.
		DrawRectangle(rMC.x, rMC.y, rMC.width / 2, rMC.height / 2, ColorAlpha(GRAY, alpha * fillAlpha));
	}
	DrawRectangleRoundedLines(rMC, 0.3f, 8, 2, white);
	DrawLineEx(Vec2(rMC.x, rMC.y + 20), Vec2(rMC.x + rMC.width, rMC.y + 20), 2, white);
	DrawLineEx(Vec2(rMC.x + rMC.width / 2, rMC.y), Vec2(rMC.x + rMC.width / 2, rMC.y + 20), 2, white);
	DrawRectangleRounded(Rect(rMC.x + rMC.width / 2 - 2, rMC.y + 5, 4, 10), 0.3f, 8, white);

	DrawText("Move", 170, 770, 20, white);
	DrawText("Aim / Shoot", 170, 840, 20, white);

	//@TODO: Maybe we don't need these?
	const int iconMove = 67; // RICON_TARGET_MOVE_FILL
	const int iconSnap = 184; // RICON_PHOTO_CAMERA_FLASH
	const int iconRestart = 72; // RICON_UNDO_FILL
	const int iconEditor = 166; // RICON_CUBE_FACE_BOTTOM
	GuiDrawIcon(iconMove, 225, 760, 2, white);
	GuiDrawIcon(iconSnap, 300, 830, 2, white);
	GuiDrawIcon(iconRestart, 620, 760, 2, white);
	GuiDrawIcon(iconEditor, 580, 830, 2, white);

	Rectangle rR = Rect(750 + 00.00f, 765 + 00.00f, 25, 25);
	Rectangle rT = Rect(750 + 00.00f, 765 + 70.00f, 25, 25);
	DrawRectangleLinesEx(rR, 2, white);
	DrawRectangleLinesEx(rT, 2, white);
	Vector2 cR = RectangleCenter(rR);
	Vector2 cT = RectangleCenter(rT);
	DrawTextCentered("R", cR.x, cR.y, 12, white);
	DrawTextCentered("`", cT.x, cT.y, 12, white);

	DrawTextRightAligned("Restart", 730, 770, 20, white);
	DrawTextRightAligned("Level editor", 730, 840, 20, white);
}

#define TITLE_DROP_DURATION 2.0f
#define MAIN_MENU_FADE_DURATION 2.0f

float mainMenuTime;
float mainMenuFollowerAngle;
float mainMenuFollowerExtension;
float mainMenuFadeTime;
bool mainMenuPlayedSnap;
bool mainMenuPlayedShutterEcho;
bool mainMenuPlayedRinging;
void MainMenu_Init(GameState oldState)
{
	mainMenuTime = 0;
	mainMenuPlayedSnap = false;
	mainMenuFollowerExtension = 0;
	mainMenuFollowerAngle = PI;
	mainMenuFadeTime = -1;
	mainMenuPlayedRinging = false;
	mainMenuPlayedShutterEcho = false;
}
GameState MainMenu_Update(void)
{
	mainMenuTime += DELTA_TIME;
	if (mainMenuFadeTime >= 0)
		mainMenuFadeTime += DELTA_TIME;
	if (mainMenuFadeTime > MAIN_MENU_FADE_DURATION)
	{
		LoadRoom(&currentRoom, "room0");
		CopyRoomToGame(&currentRoom);
		return GAME_STATE_FADE_IN;
	}

	bool anyKeyPressed =
		IsKeyPressed(KEY_SPACE) ||
		IsKeyPressed(KEY_ENTER) ||
		IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
		IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
		false;

	if (anyKeyPressed)
	{
		if (mainMenuTime < TITLE_DROP_DURATION)
		{
			mainMenuTime = TITLE_DROP_DURATION;
			mainMenuPlayedSnap = true;
		}
		else if (mainMenuFadeTime < 0)
		{
			mainMenuFadeTime = 0;
		}
	}

	return GAME_STATE_MAIN_MENU;
}
void MainMenu_Draw(void)
{
	DoScreenShake();

	const float maxExtension = 60;
	const float ringTime = 1;
	const float shutterTime = 1;
	float time = mainMenuTime;

	if (time > 1.4f && !mainMenuPlayedSnap)
	{
		mainMenuPlayedSnap = true;
		PlaySound(shutterSound);
	}

	if (time >= ringTime + shutterTime / 2)
	{
		//@TODO: Maybe remove these?
		DrawCircle(570, 380, 60, ColorAlpha(SKYBLUE, 0.3f));
		DrawCircle(475, 440, 25, ColorAlpha(BLUE, 0.3f));
	}

	if (time >= ringTime + shutterTime / 2)
	{
		DrawTextCentered("Pictomage", screenCenter.x, screenCenter.y - 100, 50, WHITE);
		float timeCos = (1 + cosf(10 * GetTime())) / 2;
		DrawTextCentered("press any key", screenCenter.x, screenCenter.y + 100, 40, Grayscale(Lerp(0.5f, 1.0f, timeCos)));
		
		DrawTriangle(Vec2(325, 340), Vec2(325, 360), Vec2(265, 380), WHITE);
		DrawTriangle(Vec2(575, 340), Vec2(575, 360), Vec2(635, 380), WHITE);
	}

	float startAngle = 360;
	float endAngle = 0;
	if (time < ringTime)
	{
		float t = Smoothstep(0, ringTime, time);
		endAngle = 180 - t * 180;
		startAngle = 360 - endAngle;
	}
	
	float shutterT = Clamp((time - ringTime) / shutterTime, 0, 1);
	if (time >= ringTime && time < ringTime + shutterTime)
	{
		float t = 0.5f + (1 - fabsf(2 * (shutterT - 0.5f))) / 2;
		DrawShutter(t, Grayscale(0.05f), Grayscale(0.1f), 5);
	}

	DrawRing(screenCenter, 230, 900, 0, 360, 300, BLACK);

	float followerAngle = WrapMinMax(mainMenuFollowerAngle * RAD2DEG, 0, 360);
	float followerExtent = Remap(mainMenuFollowerExtension, 0, maxExtension, 20, 16.5f);
	float followerMin = followerAngle - followerExtent;
	float followerMax = followerAngle + followerExtent;
	if (time < ringTime)
	{
		followerMin = Clamp(followerMin, endAngle, startAngle);
		followerMax = Clamp(followerMax, endAngle, startAngle);
	}
	DrawRing(screenCenter, 262, 280 + mainMenuFollowerExtension, followerMin, followerMax, 20, Grayscale(0.1f));

	if (time > ringTime / 2)
	{
		Vector2 mousePos = GetMousePosition();
		float mouseAngle = PI / 2 - AngleBetween(screenCenter, mousePos);
		float dAngle = WrapMinMax(mouseAngle - mainMenuFollowerAngle, -PI, +PI);
		mainMenuFollowerAngle += 0.8f * Clamp(dAngle, -PI / 4, +PI / 4) * DELTA_TIME;
		mainMenuFollowerAngle = WrapMinMax(mainMenuFollowerAngle, -PI, +PI);

		float s = sinf(PI / 2 - mainMenuFollowerAngle);
		float c = cosf(PI / 2 - mainMenuFollowerAngle);
		Vector2 center = {
			screenCenter.x + c * 320,
			screenCenter.y + s * 320
		};

		if (CheckCollisionPointCircle(mousePos, center, 80))
			mainMenuFollowerExtension += 100 * DELTA_TIME;
		else
			mainMenuFollowerExtension -= 100 * DELTA_TIME;
		mainMenuFollowerExtension = Clamp(mainMenuFollowerExtension, 0, maxExtension);
	}

	rlDrawRenderBatchActive();
	rlBegin(RL_QUADS);
	{
		float text = mainMenuFollowerExtension / maxExtension;
		float cx = screenCenter.x;
		float cy = screenCenter.y;
		float ew = creditsTexture.width / 2.0f;
		float eh = creditsTexture.height / 2.0f;
		Vector2 v0 = { 260 + 0, -ew };
		Vector2 v1 = { 260 + 0, +ew };
		Vector2 v2 = { 260 + text * 2 * eh, +ew };
		Vector2 v3 = { 260 + text * 2 * eh, -ew };
		Vector2 v[4] = { v0, v1, v2, v3 };
		float s = sinf(PI / 2 - mainMenuFollowerAngle);
		float c = cosf(PI / 2 - mainMenuFollowerAngle);
		for (int i = 0; i < 4; ++i)
		{
			float rx = v[i].x * c - v[i].y * s;
			float ry = v[i].x * s + v[i].y * c;
			v[i].x = cx + rx;
			v[i].y = cy + ry;
		}
		rlSetTexture(creditsTexture.id);
		rlColor(WHITE);
		rlTexCoord2f(0, text); rlVertex2fv(v[0]);
		rlTexCoord2f(1, text); rlVertex2fv(v[1]);
		rlTexCoord2f(1, 0); rlVertex2fv(v[2]);
		rlTexCoord2f(0, 0); rlVertex2fv(v[3]);
	}
	rlEnd();
	rlDrawRenderBatchActive();

	DrawRing(screenCenter, 230, 260, startAngle, endAngle, 300, Grayscale(0.2f));
	DrawRing(screenCenter, 228, 232, startAngle, endAngle, 300, Grayscale(0.15f));
	DrawRing(screenCenter, 258, 262, startAngle, endAngle, 300, Grayscale(0.15f));

	const float flash0Duration = MAIN_MENU_FADE_DURATION / 2;
	const float flash1Duration = MAIN_MENU_FADE_DURATION - flash0Duration;
	if (mainMenuFadeTime >= 0)
	{
		if (mainMenuFadeTime < flash0Duration)
		{
			float t0 = mainMenuFadeTime / flash0Duration;
			float t = powf(sinf(PI * t0), 16);
			
			Color color0 = FloatRGBA(0.2f, 0.2f, 0.2f, t);
			Color color1 = FloatRGBA(1, 1, 1, t);
			DrawRectangleRounded(Rect(340, 40, 220, 120), 0.5f, 20, color0);
			DrawRectangleRounded(Rect(350, 50, 200, 100), 0.5f, 20, color1);

			if (t0 > 0.6f && !mainMenuPlayedShutterEcho)
			{
				ScreenShake(10, 0.7f, 0.96f);
				PlaySound(shutterEchoSound);
				mainMenuPlayedShutterEcho = true;
			}
		}
		else
		{
			float t = Smoothstep(flash0Duration, MAIN_MENU_FADE_DURATION, mainMenuFadeTime);
			Color color = FloatRGBA(1, 1, 1, t);
			DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color);
			if (!mainMenuPlayedRinging)
			{
				PlaySound(ringingSound);
				mainMenuPlayedRinging = true;
			}
		}
	}

	float baseAlpha = Clamp(Remap(shutterT, 0.5f, 1, 0, 1), 0, 1);
	float fillAlpha = 1;
	if (mainMenuFadeTime > flash0Duration)
		fillAlpha = 1 - (mainMenuFadeTime - flash0Duration) / flash1Duration;
	DrawControls(baseAlpha, Clamp(fillAlpha, 0, 1));
}

// *---=========---*
// |/   Playing   \|
// *---=========---*

double gracePeriodStartTime;
bool hasGracePeriod;
void Playing_Init(GameState oldState)
{
	if (oldState != GAME_STATE_PAUSED)
	{
		gracePeriodStartTime = timeAtStartOfFrame;
		hasGracePeriod = true;
	}
	CenterCameraOnLevel();
}
GameState Playing_Update(void)
{
	if (IsKeyPressed(KEY_GRAVE))
		return GAME_STATE_LEVEL_EDITOR;
	if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
		return GAME_STATE_PAUSED;
	if (IsKeyPressed(KEY_R))
		return GAME_STATE_RESTARTING;

	double gracePeriodTime = timeAtStartOfFrame - gracePeriodStartTime;
	if (gracePeriodTime > GRACE_PERIOD)
		hasGracePeriod = false;

	Vector2 initialPos = player.pos;
	UpdatePlayer();
	if (!Vector2Equal(initialPos, player.pos))
		hasGracePeriod = false;

	if (!hasGracePeriod)
	{
		UpdateBullets();
		UpdateTurrets();
		UpdateBombs();
		UpdateSparks();
		UpdateShards();
		UpdateExplosions();
		UpdateTriggerredMessages();
	}
	
	Tile playerTile = TileAtVec(player.pos);
	if (playerTile == TILE_EXIT && NumRemainingEnemies() == 0)
		return GAME_STATE_LEVEL_TRANSITION;
	if (!player.isAlive)
		return GAME_STATE_GAME_OVER;
	return GAME_STATE_PLAYING;
}
void Playing_Draw(void)
{
	SetupTileCoordinateDrawing();
	{
		DoScreenShake();

		DrawTiles(true);
		DrawDecals();
		DrawShards();
		DrawTiles(false);
		DrawGlassBoxes();
		DrawTurrets();
		DrawBombs();
		DrawPlayer(1);
		DrawBullets();
		DrawSparks();
		DrawExplosions();
		DrawPlayerCaptureCone();
		DrawPlayerRelease();
	}
	rlDrawRenderBatchActive();

	SetupScreenCoordinateDrawing();
	{
		DrawTriggerMessages(false);
		//float time = 2 * fmod(GetTime(), 1);
		//float t = fabsf(time - 1);
		//DrawShutter(t);
		//DrawShutter(1);
		//DrawDebugText("%.2f", GetTime());
		//DrawFPS(100, 100);
		//Vector2 mp = ScreenToTile(GetMousePosition());
		//DrawDebugText("[%.1f %.1f] [%.1f %.1f]", mp.x, mp.y, cameraPos.x, cameraPos.y);
	}
}

// *---==================---*
// |/   Level Transition   \|
// *---==================---*

bool levelTransitionLoadedNextLevel;
bool levelTransitionPlayedTeleportSound;
float levelTransitionTime;
Vector2 levelTransitionOutCenter;
void LevelTransition_Init(GameState oldState)
{
	levelTransitionTime = 0;
	levelTransitionLoadedNextLevel = false;
	levelTransitionPlayedTeleportSound = false;

	levelTransitionOutCenter = Vector2Zero();
	int ptx = (int)floorf(player.pos.x);
	int pty = (int)floorf(player.pos.y);
	float exitCount = 0;
	for (int ty = pty - 1; ty <= pty + 1; ++ty)
	{
		for (int tx = ptx - 1; tx <= ptx + 1; ++tx)
		{
			if (TileAt(tx, ty) == TILE_EXIT)
			{
				++exitCount;
				levelTransitionOutCenter.x += tx + 0.5f;
				levelTransitionOutCenter.y += ty + 0.5f;
			}
		}
	}

	ASSERT(exitCount > 0);
	levelTransitionOutCenter.x /= exitCount;
	levelTransitionOutCenter.y /= exitCount;
}
GameState LevelTransition_Update(void)
{
	const float phase1Duration = 0.5f; // Player walk in.
	const float phase2Duration = 1.0f; // Beam out.
	const float phase3Duration = 1.0f; // Beam in.
	const float phase1Start = 0;
	const float phase1End = phase1Duration;
	const float phase2Start = phase1End;
	const float phase2End = phase2Start + phase2Duration;
	const float phase3Start = phase2End;
	const float phase3End = phase3Start + phase3Duration;

	levelTransitionTime += DELTA_TIME;

	if (levelTransitionTime >= phase3Start && !levelTransitionLoadedNextLevel)
	{
		levelTransitionLoadedNextLevel = true;
		bool loadedNextLevel = LoadRoom(&currentRoom, nextRoomName);
		if (loadedNextLevel)
		{
			CopyRoomToGame(&currentRoom);
			CenterCameraOnLevel();
		}
		else return GAME_STATE_CREDITS;
	}

	if (levelTransitionTime > phase3End)
		return GAME_STATE_PLAYING;
	else
	{
		UpdateSparks();
		UpdateShards();
		return GAME_STATE_LEVEL_TRANSITION;
	}
}
void LevelTransition_Draw(void)
{
	const float phase1Duration = 0.5f; // Player walk in.
	const float phase2Duration = 1.0f; // Beam out.
	const float phase3Duration = 1.0f; // Beam in.
	const float phase1Start = 0;
	const float phase1End = phase1Duration;
	const float phase2Start = phase1End;
	const float phase2End = phase2Start + phase2Duration;
	const float phase3Start = phase2End;
	const float phase3End = phase3Start + phase3Duration;
	const float time = levelTransitionTime;

	SetupTileCoordinateDrawing();
	{
		DoScreenShake();

		DrawTiles(true);
		DrawDecals();
		DrawShards();
		DrawTiles(false);
		DrawGlassBoxes();
		DrawTurrets();
		DrawBombs();
		DrawSparks();

		// Slowly move the player towards the center of the teleporter.
		Vector2 backupPos = player.pos;
		{
			if (time < phase3Start)
			{
				float t = Clamp(time / phase1Duration, 0, 1);
				player.pos.x = Lerp(player.pos.x, levelTransitionOutCenter.x, t);
				player.pos.y = Lerp(player.pos.y, levelTransitionOutCenter.y, t);
			}

			float alpha = 1;
			if (time >= phase2Start && time < phase3Start)
			{
				alpha = 1 - Clamp((time - phase2Start) / phase2Duration, 0, 1);
			}
			else if (time >= phase3Start)
			{
				alpha = Clamp((time - phase3Start) / phase3Duration, 0, 1);
			}

			DrawPlayer(alpha);
		}
		player.pos = backupPos;

		// Beams
		if (time >= phase2Start)
		{
			if (!levelTransitionPlayedTeleportSound)
			{
				PlaySound(teleportSound);
				levelTransitionPlayedTeleportSound = true;
			}

			float t;
			Vector2 c;
			if (time < phase3Start)
			{
				t = Clamp((time - phase2Start) / phase2Duration, 0, 1);
				c = levelTransitionOutCenter;
			}
			else
			{
				t = 1 - Clamp((time - phase3Start) / phase3Duration, 0, 1);
				c = player.pos;
			}

			Color beamColor1 = RGBA8(84, 190, 191, 255);
			Color beamColor0 = ColorAlpha(beamColor1, 0);
			t = powf(sinf(PI * t), 16);
			DrawCircleGradientV(c, Vec2Broadcast(2.5f * t), beamColor1, beamColor0);
		}
	}
	rlDrawRenderBatchActive();

	//SetupScreenCoordinateDrawing();
	{
		//DrawTriggerMessages(false);
		//DrawRectangle(0, 0, (int)(levelTransitionTime * 400), 100, WHITE);
	}
}

// *---=========---*
// |/   Fade In   \|
// *---=========---*

#define FADE_IN_DURATION 1.0f

float fadeInTime;
void FadeIn_Init(GameState oldState)
{
	CenterCameraOnLevel();
	fadeInTime = 0;
}
GameState FadeIn_Update(void)
{
	fadeInTime += DELTA_TIME;
	if (fadeInTime > FADE_IN_DURATION)
		return GAME_STATE_PLAYING;
	else
		return GAME_STATE_FADE_IN;
}
void FadeIn_Draw(void)
{
	//@TODO: Probably customize this.
	Playing_Draw();

	float t = 1 - Smoothstep(0, FADE_IN_DURATION, fadeInTime);
	Color color = FloatRGBA(1, 1, 1, t);
	DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, color);
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

// *---===========---*
// |/   Game Over   \|
// *---===========---*

double gameOverStartTime;
void GameOver_Init(GameState oldState)
{
	gameOverStartTime = timeAtStartOfFrame;
}
GameState GameOver_Update(void)
{
	if (IsKeyPressed(KEY_GRAVE))
		return GAME_STATE_LEVEL_EDITOR;
	if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))
		return GAME_STATE_RESTARTING;

	UpdateBullets();
	UpdateTurrets();
	UpdateBombs();
	UpdateExplosions();

	return GAME_STATE_GAME_OVER;
}
void GameOver_Draw(void)
{
	Playing_Draw();

	Color edgeColor = ColorAlpha(BLACK, 0.9f);
	Color centerColor = ColorAlpha(BLACK, 0.1f);
	DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT / 2, edgeColor, centerColor);
	DrawRectangleGradientV(0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2, centerColor, edgeColor);
	DrawTextCentered("- YOU DIED -", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 40, BLACK);
}

// *---============---*
// |/   Restarting   \|
// *---============---*

double restartingStartTime;
bool restartingDone;
bool restartingPlayedShutterSound;
void Restarting_Init(GameState oldState)
{
	restartingStartTime = timeAtStartOfFrame;
	restartingDone = false;
	restartingPlayedShutterSound = false;
	StopAllLevelSounds();
}
GameState Restarting_Update(void)
{
	double time = timeAtStartOfFrame - restartingStartTime;
	if (time > 0.5f * RESTARTING_DURATION && !restartingDone)
	{
		CopyRoomToGame(&currentRoom);
		restartingDone = true;
	}
	if (time > 0.3f * RESTARTING_DURATION && !restartingPlayedShutterSound)
	{
		PlaySound(shutterSound);
		restartingPlayedShutterSound = true;
	}

	if (time > RESTARTING_DURATION)
		return GAME_STATE_PLAYING;
	else
		return GAME_STATE_RESTARTING;
}
void Restarting_Draw(void)
{
	Playing_Draw();
	float time = (float)((timeAtStartOfFrame - restartingStartTime) / (0.5f * RESTARTING_DURATION));
	float t = 1 - fabsf(time - 1);
	DrawShutter(Clamp(t, 0, 1), Grayscale(0.1f), Grayscale(0.3f), 10);
}

// *---==============---*
// |/   Level Editor   \|
// *---==============---*

bool isFirstLevelEditorFrame; // @HACK: On the first frame, the console is focused and will receive the '`' that switched to the editor.
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
		selection.tileVariant = 0;
	}
	DrawRectangleRec(Rect(x + 10, y + 10, 30, 30), ColorAlpha(BLACK, 0.2f));
	GuiSetState(state);
}
void FillAllTilesBetween(Vector2 p0, Vector2 p1, Tile tile, u8 variant)
{
	// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

	int x0 = ClampInt((int)p0.x, 0, numTilesX - 1);
	int y0 = ClampInt((int)p0.y, 0, numTilesY - 1);
	int x1 = ClampInt((int)p1.x, 0, numTilesX - 1);
	int y1 = ClampInt((int)p1.y, 0, numTilesY - 1);
	int dx = +abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	while (true)
	{
		tiles[y0][x0] = tile;
		tileVariants[y0][x0] = variant;
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
}

void LevelEditor_Init(GameState oldState)
{
	isFirstLevelEditorFrame = true;
	cameraPos = Vector2Zero();
	cameraZoom = 1;
	CopyRoomToGame(&currentRoom);
	ShiftCamera(-(numTilesX - MAX_TILES_X) / 2.0f, -(numTilesY - MAX_TILES_Y) / 2.0f);
	ZoomInToPoint(screenCenter, powf(1.1f, -7));
	
	// Focus on the console when you first pull up the editor.
	lastMouseClickPos.x = consoleInputRect.x + 1;
	lastMouseClickPos.y = consoleInputRect.y + 1;
}
GameState LevelEditor_Update(void)
{
	isFirstLevelEditorFrame = false;
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
		{
			Vector2 p1 = mousePosTiles;
			Vector2 p0 = Vector2Subtract(mousePosTiles, mouseDeltaTiles);
			FillAllTilesBetween(p0, p1, selection.tile, selection.tileVariant);
		}
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		lastMouseClickPos = mousePos;
		if (selection.kind == EDITOR_SELECTION_KIND_TILE)
		{
			if (!CheckCollisionPointRec(mousePos, propertiesWindowRect))
			{
				int tileX = (int)mousePosTiles.x;
				int tileY = (int)mousePosTiles.y;
				if (tileX >= 0 && tileX < numTilesX && tileY >= 0 && tileY < numTilesY)
				{
					tiles[tileY][tileX] = selection.tile;
					tileVariants[tileY][tileX] = selection.tileVariant;
				}
				else selection.kind = EDITOR_SELECTION_KIND_NONE;
			}
		}

		if (selection.kind != EDITOR_SELECTION_KIND_TILE) // Checking this again because it might change above.
		{
			if (!CheckCollisionPointRec(mousePos, consoleWindowRect) &&
				!CheckCollisionPointRec(mousePos, objectsWindowRect) &&
				!CheckCollisionPointRec(mousePos, propertiesWindowRect) &&
				!CheckCollisionPointRec(mousePos, tilesWindowRect))
			{
				selection.kind = EDITOR_SELECTION_KIND_NONE;

				for (int i = 0; i < numTriggerMessages; ++i)
				{
					if (CheckCollisionPointRec(mousePosTiles, triggerMessages[i].rect))
					{
						selection.kind = EDITOR_SELECTION_KIND_TRIGGER_MESSAGE;
						selection.triggerMessage = &triggerMessages[i];
					}
				}

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

				for (int i = 0; i < numGlassBoxes; ++i)
				{
					if (CheckCollisionPointRec(mousePosTiles, glassBoxes[i].rect))
					{
						selection.kind = EDITOR_SELECTION_KIND_GLASS_BOX;
						selection.glassBox = &glassBoxes[i];
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
				DespawnTurret(index);
				selection.kind = EDITOR_SELECTION_KIND_NONE;
			} break;
			case EDITOR_SELECTION_KIND_BOMB:
			{
				int index = (int)(selection.bomb - bombs);
				DespawnBomb(index);
				selection.kind = EDITOR_SELECTION_KIND_NONE;
			} break;
			case EDITOR_SELECTION_KIND_GLASS_BOX:
			{
				int index = (int)(selection.glassBox - glassBoxes);
				DespawnGlassBox(index);
				selection.kind = EDITOR_SELECTION_KIND_NONE;
			} break;
			case EDITOR_SELECTION_KIND_TRIGGER_MESSAGE:
			{
				int index = (int)(selection.triggerMessage - triggerMessages);
				DespawnTriggerMessage(index);
				selection.kind = EDITOR_SELECTION_KIND_NONE;
			} break;
		}
	}

	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && (mouseDeltaTiles.x != 0 || mouseDeltaTiles.y != 0))
	{
		switch (selection.kind)
		{
			case EDITOR_SELECTION_KIND_TURRET:
			{
				Vector2 newPos = Vector2Add(selection.turret->pos, mouseDeltaTiles);
				if (CheckCollisionPointCircle(mousePosTiles, newPos, TURRET_RADIUS))
					selection.turret->pos = newPos;
			} break;
			case EDITOR_SELECTION_KIND_BOMB:
			{
				Vector2 newPos = Vector2Add(selection.bomb->pos, mouseDeltaTiles);
				if (CheckCollisionPointCircle(mousePosTiles, newPos, BOMB_RADIUS))
					selection.bomb->pos = newPos;
			} break;
			case EDITOR_SELECTION_KIND_GLASS_BOX:
			{
				Rectangle newRect = selection.glassBox->rect;
				newRect.x += mouseDeltaTiles.x;
				newRect.y += mouseDeltaTiles.y;
				if (CheckCollisionPointRec(mousePosTiles, newRect))
					selection.glassBox->rect = newRect;
			} break;
			case EDITOR_SELECTION_KIND_TRIGGER_MESSAGE:
			{
				Rectangle newRect = selection.triggerMessage->rect;
				newRect.x += mouseDeltaTiles.x;
				newRect.y += mouseDeltaTiles.y;
				if (CheckCollisionPointRec(mousePosTiles, newRect))
					selection.triggerMessage->rect = newRect;
			} break;
			case EDITOR_SELECTION_KIND_PLAYER:
			{
				Vector2 newPos = Vector2Add(selection.player->pos, mouseDeltaTiles);
				if (CheckCollisionPointCircle(mousePosTiles, newPos, PLAYER_RADIUS))
					selection.player->pos = newPos;
			} break;
		}
	}

	return GAME_STATE_LEVEL_EDITOR;
}
void LevelEditor_Draw(void)
{
	SetupTileCoordinateDrawing();
	{
		DrawTiles(true);
		DrawTiles(false);
		DrawGlassBoxes();
		DrawTurrets();
		DrawBombs();
		DrawPlayer(1);
		DrawTriggerMessages(true);
	}
	SetupScreenCoordinateDrawing();

	Vector2 mousePos = GetMousePosition();
	Vector2 mousePosTiles = ScreenToTile(mousePos);

	GuiWindowBox(consoleWindowRect, "Console");
	{
		bool isFocused = !isFirstLevelEditorFrame && CheckCollisionPointRec(lastMouseClickPos, consoleInputRect);
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
					if (argCount == 1)
					{
						if (LoadRoom(&currentRoom, args[0]))
							CopyRoomToGame(&currentRoom);
					}
					else if (argCount < 2) TraceLog(LOG_ERROR, "Command '%s' requires the name of the room to load.", command);
					else if (argCount > 2) TraceLog(LOG_ERROR, "Command '%s' requires only 1 argument, but %d were given.", command, argCount);
				}
				else if (TextIsEqual(command, "gm") || TextIsEqual(command, "godmode"))
				{
					if (argCount == 0)
						godMode = !godMode;
					else if (argCount == 1)
					{
						if (TextIsEqual(args[0], "1"))
							godMode = true;
						else if (TextIsEqual(args[0], "0"))
							godMode = false;
						else
							TraceLog(LOG_ERROR, "Command '%s' accepts either '1' or '0', but '%s' was given.", command, args[0]);
					}
					else if (argCount > 1) TraceLog(LOG_ERROR, "Command '%s' requires 1 or 0 arguments, but %d were given.", command, argCount);
				}
				else if (TextIsEqual(command, "dm") || TextIsEqual(command, "devmode"))
				{
					if (argCount == 0)
						devMode = !devMode;
					else if (argCount == 1)
					{
						if (TextIsEqual(args[0], "1"))
							devMode = true;
						else if (TextIsEqual(args[0], "0"))
							devMode = false;
						else
							TraceLog(LOG_ERROR, "Command '%s' accepts either '1' or '0', but '%s' was given.", command, args[0]);
					}
					else if (argCount > 1) TraceLog(LOG_ERROR, "Command '%s' requires 1 or 0 arguments, but %d were given.", command, argCount);
				}
				else
				{
					TraceLog(LOG_ERROR, "Unknown command '%s'.", command);
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

		if (GuiButton(Rect(x, y, 60, 60), "Glass"))
		{
			GlassBox *box = SpawnGlassBox(Rect(0, 0, 1, 1));
			if (box)
			{
				selection.kind = EDITOR_SELECTION_KIND_GLASS_BOX;
				selection.glassBox = box;
			}
		}
		DrawRectangleRec(Rect(x + 10, y + 10, 40, 40), ColorAlpha(BLACK, 0.2f));
		x += 70;

		if (GuiButton(Rect(x, y, 60, 60), "Message"))
		{
			TriggerMessage *tm = SpawnTriggerMessage(Rect(0, 0, 1, 1), false, "");
			if (tm)
			{
				selection.kind = EDITOR_SELECTION_KIND_TRIGGER_MESSAGE;
				selection.triggerMessage = tm;
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
		case EDITOR_SELECTION_KIND_GLASS_BOX: propertiesTitle = "Glass properties"; break;
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
				y += 25;
				selection.turret->isDestroyed = GuiCheckBox(Rect(x, y, 20, 20), "Destroyed", selection.turret->isDestroyed);
				y += 25;

				int numVariants1 = turretBaseVariants.numVariants;
				int numVariants2 = turretTopVariants.numVariants;
				int numVariants3 = destroyedTurretBaseVariants.numVariants;
				int numVariants4 = destroyedTurretTopVariants.numVariants;
				int minVariants = numVariants1;
				if (minVariants > numVariants2) minVariants = numVariants2;
				if (minVariants > numVariants3) minVariants = numVariants3;
				if (minVariants > numVariants4) minVariants = numVariants4;

				if (numVariants1 != minVariants || numVariants2 != minVariants || numVariants3 != minVariants || numVariants4 != minVariants)
				{
					GuiText(Rect(x, y, 180, 20), "!!! WARNING !!!");
					y += 20;
					GuiText(Rect(x, y, 180, 20), "Missmatched number of variants");
					y += 20;
					GuiText(Rect(x, y, 180, 20), "------------------------------");
					y += 20;
					GuiText(Rect(x, y, 180, 20), "%d base variants", numVariants1);
					y += 20;
					GuiText(Rect(x, y, 180, 20), "%d top variants", numVariants2);
					y += 20;
					GuiText(Rect(x, y, 180, 20), "%d destroyed base variants", numVariants3);
					y += 20;
					GuiText(Rect(x, y, 180, 20), "%d destroyed top variants", numVariants4);
					y += 20;
				}

				Rectangle spinnerRect = Rect(x, y, 80, 20);
				bool isFocused = CheckCollisionPointRec(lastMouseClickPos, spinnerRect);
				int variant = selection.turret->variant;
				isFocused = GuiSpinner(spinnerRect, "", &variant, 0, minVariants - 1, isFocused);
				selection.turret->variant = (u8)ClampInt(variant, 0, minVariants - 1);
				GuiLabel(Rect(x + 90, y, 80, 20), "Variant");
				y += 25;

				if (GuiButton(Rect(x, y, 100, 20), "Fill"))
				{
					for (int i = 0; i < numTurrets; ++i)
						turrets[i].variant = (u8)selection.turret->variant;
				}
				y += 25;

			} break;

			case EDITOR_SELECTION_KIND_BOMB:
			{
				GuiText(Rect(x, y, 100, 20), "X: %.2f", selection.bomb->pos.x);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Y: %.2f", selection.bomb->pos.y);
				y += 20;

				Rectangle spinnerRect = Rect(x, y, 80, 20);
				bool isFocused = CheckCollisionPointRec(lastMouseClickPos, spinnerRect);
				int variant = selection.bomb->variant;
				int numVariants = bombVariants.numVariants;
				isFocused = GuiSpinner(spinnerRect, "", &variant, 0, numVariants - 1, isFocused);
				selection.bomb->variant = (u8)ClampInt(variant, 0, numVariants - 1);
				GuiLabel(Rect(x + 90, y, 80, 20), "Variant");
				y += 25;

				if (GuiButton(Rect(x, y, 100, 20), "Fill"))
				{
					for (int i = 0; i < numBombs; ++i)
						bombs[i].variant = (u8)selection.bomb->variant;
				}
				y += 25;
			} break;

			case EDITOR_SELECTION_KIND_GLASS_BOX:
			{
				GuiText(Rect(x, y, 100, 20), "X: %.2f", selection.glassBox->rect.x);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Y: %.2f", selection.glassBox->rect.y);
				y += 20;
				selection.glassBox->rect.width = GuiSlider(Rect(x, y, 100, 20), "", "Width", selection.glassBox->rect.width, 0.5f, 30);
				y += 25;
				selection.glassBox->rect.height = GuiSlider(Rect(x, y, 100, 20), "", "Height", selection.glassBox->rect.height, 0.5f, 30);
				y += 25;
			} break;

			case EDITOR_SELECTION_KIND_TRIGGER_MESSAGE:
			{
				Rectangle messageRect = Rect(x, y, 180, 60);
				bool isFocused = CheckCollisionPointRec(lastMouseClickPos, messageRect);
				isFocused = GuiTextBoxMulti(messageRect, selection.triggerMessage->message, sizeof selection.triggerMessage->message, isFocused);
				y += 60;

				GuiText(Rect(x, y, 100, 20), "X: %.2f", selection.triggerMessage->rect.x);
				y += 20;
				GuiText(Rect(x, y, 100, 20), "Y: %.2f", selection.triggerMessage->rect.y);
				y += 20;
				selection.triggerMessage->rect.width = GuiSlider(Rect(x, y, 100, 20), "", "Width", selection.triggerMessage->rect.width, 1, 30);
				y += 25;
				selection.triggerMessage->rect.height = GuiSlider(Rect(x, y, 100, 20), "", "Height", selection.triggerMessage->rect.height, 1, 30);
				y += 25;
				selection.triggerMessage->once = GuiCheckBox(Rect(x, y, 20, 20), "Trigger once", selection.triggerMessage->once);
				y += 25;
			} break;

			case EDITOR_SELECTION_KIND_TILE:
			{
				Tile tile = selection.tile;
				int variant = selection.tileVariant;
				int numVariants = tileTextureVariants[tile].numVariants;
				Texture texture = tileTextureVariants[tile].variants[variant];
				DrawRectangle(x, y, texture.width + 4, texture.height + 4, BLACK);
				DrawTexturePro(texture,
					Rect(0, texture.height, texture.width, -texture.height),
					Rect(x + 2, y + 2, texture.width, texture.height),
					Vec2(0, 0), 0, WHITE);
				//DrawTexture(texture, x + 2, y + 2, WHITE);
				y += texture.height + 6;

				Rectangle spinnerRect = Rect(x, y, 80, 20);
				bool isFocused = CheckCollisionPointRec(lastMouseClickPos, spinnerRect);
				isFocused = GuiSpinner(spinnerRect, "", &variant, 0, numVariants - 1, isFocused);
				selection.tileVariant = ClampInt(variant, 0, numVariants - 1);
				GuiLabel(Rect(x + 90, y, 80, 20), "Variant");
				y += 25;

				if (GuiButton(Rect(x, y, 100, 20), "Fill"))
				{
					for (int ty = 0; ty < numTilesY; ++ty)
						for (int tx = 0; tx < numTilesX; ++tx)
							if (tiles[ty][tx] == selection.tile)
								tileVariants[ty][tx] = selection.tileVariant;
				}
			} break;

			default: // Room properties
			{
				Rectangle roomNameRect = Rect(x, y, 140, 20);
				bool isFocused = CheckCollisionPointRec(lastMouseClickPos, roomNameRect);
				GuiTextBox(roomNameRect, currentRoom.name, sizeof currentRoom.name, isFocused);
				GuiLabel(Rect(x + 145, y, 20, 20), "Name");
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

				/*
				float et0 = evenTileTint0.a / 255.0f;
				float et1 = evenTileTint1.a / 255.0f;
				float ot0 = oddTileTint0.a / 255.0f;
				float ot1 = oddTileTint1.a / 255.0f;
				
				GuiText(Rect(x, y, 100, 20), "Even tint 0"); y += 20;
				evenTileTint0 = GuiColorPicker(Rect(x, y, 60, 60), evenTileTint0);
				et0 = GuiColorBarHue(Rect(x + 100, y, 20, 60), et0);
				y += 60;

				GuiText(Rect(x, y, 100, 20), "Even tint 1"); y += 20;
				evenTileTint1 = GuiColorPicker(Rect(x, y, 60, 60), evenTileTint1); 
				et1 = GuiColorBarHue(Rect(x + 100, y, 20, 60), et1);
				y += 60;
				
				GuiText(Rect(x, y, 100, 20), "Odd tint 0"); y += 20;
				oddTileTint0 = GuiColorPicker(Rect(x, y, 60, 60), oddTileTint0); 
				ot0 = GuiColorBarHue(Rect(x + 100, y, 20, 60), ot0);
				y += 60;
				
				GuiText(Rect(x, y, 100, 20), "Odd tint 1"); y += 20;
				oddTileTint1 = GuiColorPicker(Rect(x, y, 60, 60), oddTileTint1); 
				ot1 = GuiColorBarHue(Rect(x + 100, y, 20, 60), ot1);
				y += 60;

				evenTileTint0.a = (u8)(et0 * 255.5f);
				evenTileTint1.a = (u8)(et1 * 255.5f);
				oddTileTint0.a = (u8)(ot0 * 255.5f);
				oddTileTint1.a = (u8)(ot1 * 255.5f);
				*/
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
	//@TODO: Check is this can work. Sometimes it causes weird flickering when drawing circles.
	//SetConfigFlags(FLAG_MSAA_4X_HINT);
	
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Snapper");
	SetTargetFPS(FPS);
	InitAudioDevice();
	rlDisableBackfaceCulling(); // It's a 2D game we don't need this..
	rlDisableDepthTest();

	rng = SeedRandom(time(NULL));

	LoadAllTextures();
	LoadAllSounds();

	if (devMode)
	{
		gameState = GAME_STATE_PLAYING;
		LoadRoom(&currentRoom, devModeStartRoom);
		CopyRoomToGame(&currentRoom);
		Playing_Init(GAME_STATE_PLAYING);
	}
	else
	{
		gameState = GAME_STATE_MAIN_MENU;
		MainMenu_Init(GAME_STATE_MAIN_MENU);
	}
}
void GameLoopOneIteration(void)
{
	TempReset();

	timeAtStartOfFrame = GetTime();

	// Do update and draw in 2 completely separate steps because state could change in the update function.
	GameState oldState = gameState;
	switch (gameState)
	{
		case GAME_STATE_MAIN_MENU:        gameState = MainMenu_Update();        break;
		case GAME_STATE_FADE_IN:          gameState = FadeIn_Update();          break;
		case GAME_STATE_PLAYING:          gameState = Playing_Update();         break;
		case GAME_STATE_PAUSED:           gameState = Paused_Update();          break;
		case GAME_STATE_GAME_OVER:        gameState = GameOver_Update();        break;
		case GAME_STATE_LEVEL_TRANSITION: gameState = LevelTransition_Update(); break;
		case GAME_STATE_LEVEL_EDITOR:     gameState = LevelEditor_Update();     break;
		case GAME_STATE_CREDITS:          assert(false);                        break;
		case GAME_STATE_RESTARTING:       gameState = Restarting_Update();      break;
	}

	if (gameState != oldState)
	{
		switch (gameState)
		{
			case GAME_STATE_MAIN_MENU:	      MainMenu_Init(oldState);        break;
			case GAME_STATE_FADE_IN:          FadeIn_Init(oldState);          break;
			case GAME_STATE_PLAYING:          Playing_Init(oldState);         break;
			case GAME_STATE_PAUSED:           Paused_Init(oldState);          break;
			case GAME_STATE_GAME_OVER:        GameOver_Init(oldState);        break;
			case GAME_STATE_LEVEL_TRANSITION: LevelTransition_Init(oldState); break;
			case GAME_STATE_LEVEL_EDITOR:     LevelEditor_Init(oldState);     break;
			case GAME_STATE_CREDITS:          assert(false);                  break;
			case GAME_STATE_RESTARTING:       Restarting_Init(oldState);      break;
		}
	}
	
	BeginDrawing();
	ClearBackground(BLACK);
	{
		switch (gameState)
		{
			case GAME_STATE_MAIN_MENU:        MainMenu_Draw();        break;
			case GAME_STATE_FADE_IN:          FadeIn_Draw();          break;
			case GAME_STATE_PLAYING:          Playing_Draw();         break;
			case GAME_STATE_PAUSED:           Paused_Draw();          break;
			case GAME_STATE_GAME_OVER:        GameOver_Draw();        break;
			case GAME_STATE_LEVEL_TRANSITION: LevelTransition_Draw(); break;
			case GAME_STATE_LEVEL_EDITOR:     LevelEditor_Draw();     break;
			case GAME_STATE_CREDITS:          assert(false);          break;
			case GAME_STATE_RESTARTING:       Restarting_Draw();      break;
		}
	}
	EndDrawing();
}