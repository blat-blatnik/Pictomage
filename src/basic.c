#include "basic.h"

#define TEMP_STORAGE_SIZE (256*1024) // 256kB

static u8 TempStorage[TEMP_STORAGE_SIZE];
static BiStack TempStorageAllocator;

void BasicInit(void)
{
	TempStorageAllocator = CreateBiStackFromBuffer(TempStorage, sizeof TempStorage);
}

void *TempAlloc(size_t size)
{
	void *result = BiStackAllocFront(&TempStorageAllocator, size);
	if (!result)
		TraceLog(LOG_WARNING, "Temporary storage was full when trying to allocate %zu bytes.", size);
	else
		memset(result, 0, size);
	return result;
}
void TempReset(void)
{
	BiStackResetAll(&TempStorageAllocator);
}

char *TempPrint(FORMAT_STRING format, ...)
{
	ASSERT(format);
	va_list args;
	va_start(args, format);
	char *result = TempPrintv(format, args);
	va_end(args);
	return result;
}
char *TempPrintv(FORMAT_STRING format, va_list args)
{
	ASSERT(format);
	
	va_list argsCopy;
	va_copy(argsCopy, args);
	int neededLength = vsnprintf(NULL, 0, format, argsCopy);
	va_end(argsCopy);
	if (neededLength < 0)
		return TempPrint("[FORMAT ERROR]");

	size_t neededBytes = (size_t)neededLength + 1;
	char *result = TempAlloc(neededBytes); // +1 for NULL terminator.
	if (!result)
		return NULL;

	vsnprintf(result, neededBytes, format, args);
	return result;
}

u64 HashBytes(const void *data, size_t size)
{
	ASSERT(data || size == 0);
	const u8 *bytes = data;

	// FNV-1a -- https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#The_hash
	u64 hash = 14695981039346656037u;
	for (size_t i = 0; i < size; ++i)
		hash = (hash * 1099511628211u) ^ bytes[i];
	return hash;
}
void SwapMemory(void *a, void *b, size_t size)
{
	ASSERT((a && b) || size == 0);
	u8 *bytesA = a;
	u8 *bytesB = b;
	for (size_t i = 0; i < size; ++i)
	{
		u8 temp = bytesA[i];
		bytesA[i] = bytesB[i];
		bytesB[i] = temp;
	}
}

float Smoothstep(float edge0, float edge1, float x)
{
	// Actually smootherstep because it looks better: https://en.wikipedia.org/wiki/Smoothstep#Variations
	x = Clamp((x - edge0) / (edge1 - edge0), 0, 1);
	return x * x * x * (x * (x * 6 - 15) + 10);
}
float Wrap(float x, float max)
{
	return fmodf(max + fmodf(x, max), max);
}
float WrapMinMax(float x, float min, float max)
{
	ASSERT(max > min);
	return min + Wrap(x - min, max - min);
}
bool IsAngleBetween(float target, float angle1, float angle2)
{
	float a = WrapMinMax(angle1 - target, -PI, +PI);
	float b = WrapMinMax(angle2 - target, -PI, +PI);
	if (a * b >= 0)
		return false;
	return fabsf(a - b) < PI;
}
float AngleBetween(Vector2 a, Vector2 b)
{
	//         b
	//       .   \
	//     .      \
	//   .   angle \
	// a  .  .  .  .
	Vector2 d = Vector2Subtract(b, a);
	return atan2f(d.y, d.x);
}
int ClampInt(int x, int min, int max)
{
	return x < min ? min : (x > max ? max : x);
}
Vector2 Vec2(float x, float y)
{
	return (Vector2) { x, y };
}
Vector2 Vec2FromPolar(float length, float angleRadians)
{
	float s = sinf(angleRadians);
	float c = cosf(angleRadians);
	Vector2 result = { c * length, s * length };
	return result;
}
Vector2 Vec2Broadcast(float xy)
{
	return (Vector2) { xy, xy };
}
Color FloatRGBA(float r, float g, float b, float a)
{
	Color result = {
		(u8)(255.5f * Clamp(r, 0, 1)),
		(u8)(255.5f * Clamp(g, 0, 1)),
		(u8)(255.5f * Clamp(b, 0, 1)),
		(u8)(255.5f * Clamp(a, 0, 1)),
	};
	return result;
}
Color RGBA8(int r, int g, int b, int a)
{
	Color result = {
		(u8)(ClampInt(r, 0, 255)),
		(u8)(ClampInt(g, 0, 255)),
		(u8)(ClampInt(b, 0, 255)),
		(u8)(ClampInt(a, 0, 255)),
	};
	return result;
}
Color Grayscale(float whiteness)
{
	return FloatRGBA(whiteness, whiteness, whiteness, 1);
}
Color LerpColor(Color from, Color to, float amount)
{
	float r0 = from.r / 255.0f;
	float g0 = from.g / 255.0f;
	float b0 = from.b / 255.0f;
	float a0 = from.a / 255.0f;
	float r1 = to.r / 255.0f;
	float g1 = to.g / 255.0f;
	float b1 = to.b / 255.0f;
	float a1 = to.a / 255.0f;
	return FloatRGBA(
		Lerp(r0, r1, amount),
		Lerp(g0, g1, amount),
		Lerp(b0, b1, amount),
		Lerp(a0, a1, amount));
}
Rectangle Rect(float x, float y, float width, float height)
{
	return (Rectangle) { x, y, width, height };
}
Rectangle RectVec(Vector2 pos, Vector2 size)
{
	return (Rectangle) { pos.x, pos.y, size.x, size.y };
}
Rectangle RectMinMax(Vector2 min, Vector2 max)
{
	Rectangle result = {
		min.x,
		min.y,
		max.x - min.x,
		max.y - min.y
	};
	return result;
}
Vector2 RectanglePos(Rectangle rect)
{
	return Vec2(rect.x, rect.y);
}
Vector2 RectangleCenter(Rectangle rect)
{
	Vector2 center = {
		rect.x + rect.width / 2,
		rect.y + rect.height / 2
	};
	return center;
}
Rectangle ExpandRectangle(Rectangle rect, float expansion)
{
	rect.x -= expansion;
	rect.y -= expansion;
	rect.width += (expansion + expansion);
	rect.height += (expansion + expansion);
	return rect;
}
Rectangle ExpandRectangleEx(Rectangle rect, float left, float right, float up, float down)
{
	rect.x -= left;
	rect.y -= down;
	rect.width += (left + right);
	rect.height += (up + down);
	return rect;
}
Vector2 Vector2Min(Vector2 a, Vector2 b)
{
	Vector2 result = {
		fminf(a.x, b.x),
		fminf(a.y, b.y)
	};
	return result;
}
Vector2 Vector2Max(Vector2 a, Vector2 b)
{
	Vector2 result = {
		fmaxf(a.x, b.x),
		fmaxf(a.y, b.y)
	};
	return result;
}
Vector2 Vector2Floor(Vector2 v)
{
	Vector2 result = {
		floorf(v.x),
		floorf(v.y)
	};
	return result;
}
Vector2 Vector2Ceil(Vector2 v)
{
	Vector2 result = {
		ceilf(v.x),
		ceilf(v.y)
	};
	return result;
}
bool Vector2Equal(Vector2 a, Vector2 b)
{
	return a.x == b.x && a.y == b.y;
}
float Vec2Angle(Vector2 v)
{
	return atan2f(v.y, v.x);
}

Random SeedRandom(u64 seed)
{
	// PCG-XSH-RR -- https://en.wikipedia.org/wiki/Permuted_congruential_generator#Example_code
	Random rand = seed + 1442695040888963407u;
	Random32(&rand);
	return rand;
}
u32 Random32(Random *rand)
{
	ASSERT(rand);
	u64 x = *rand;
	int count = (int)(x >> 59);
	*rand = x * 6364136223846793005u + 1442695040888963407u;
	x ^= x >> 21;
	u32 y = (u32)(x >> 27);
	return (y >> count) | (y << (-count & 31));
}
int RandomInt(Random *rand, int inclusiveMin, int exclusiveMax)
{
	ASSERT(rand && inclusiveMin < exclusiveMax);

	// Optimal bounded generation: https://github.com/apple/swift/pull/39143
	u64 range = (u64)(exclusiveMax - inclusiveMin);
	u32 r1 = Random32(rand);
	u32 r2 = Random32(rand);
	u64 hi = range * r1;
	u64 lo = (u32)hi + ((range * r2) >> 32);
	return inclusiveMin + (int)((hi >> 32) + (lo >> 32));
}
float RandomFloat(Random *rand, float inclusiveMin, float exclusiveMax)
{
	ASSERT(rand && inclusiveMin < exclusiveMax);
	float f = RandomFloat01(rand);
	return inclusiveMin + (exclusiveMax - inclusiveMin) * f;
}
float RandomFloat01(Random *rand)
{
	ASSERT(rand);
	u32 r = Random32(rand);
	// https://prng.di.unimi.it/#remarks
	return (r >> 8) * 0x1.0p-24f;
}
bool RandomProbability(Random *rand, float prob)
{
	return RandomFloat01(rand) < prob;
}
Vector2 RandomVector(Random *rand, float magnitude)
{
	float angle = RandomFloat(rand, -PI, +PI);
	float s = sinf(angle);
	float c = cosf(angle);
	Vector2 result = {
		c * magnitude,
		s * magnitude
	};
	return result;
}
Vector2 RandomNormal(Random *rand, float mean, float sdev)
{
	// https://en.wikipedia.org/wiki/Marsaglia_polar_method#Implementation
	float u, v, s;
	do
	{
		u = RandomFloat(rand, -1, +1);
		v = RandomFloat(rand, -1, +1);
		s = u * u + v * v;
	} while (s >= 1 || s == 0);
	s = sqrtf(-2 * logf(s) / s);
	return Vec2(
		mean + u * sdev * s,
		mean + v * sdev * s);
}

void rlColor(Color color)
{
	rlColor4ub(color.r, color.g, color.b, color.a);
}
void rlVertex2fv(Vector2 v)
{
	rlVertex2f(v.x, v.y);
}

float DistanceLineToPoint(Vector2 p0, Vector2 p1, Vector2 p)
{
	// https://stackoverflow.com/a/6853926
	float x = p.x;
	float y = p.y;
	float x1 = p0.x;
	float y1 = p0.y;
	float x2 = p1.x;
	float y2 = p1.y;
	float A = x - x1;
	float B = y - y1;
	float C = x2 - x1;
	float D = y2 - y1;

	float dot = A * C + B * D;
	float len_sq = C * C + D * D;
	float param = -1;
	if (len_sq != 0) //in case of 0 length line
		param = dot / len_sq;

	float xx, yy;
	if (param < 0)
	{
		xx = x1;
		yy = y1;
	}
	else if (param > 1)
	{
		xx = x2;
		yy = y2;
	}
	else
	{
		xx = x1 + param * C;
		yy = y1 + param * D;
	}

	float dx = x - xx;
	float dy = y - yy;
	return sqrtf(dx * dx + dy * dy);
}
bool CheckCollisionConeCircle(Vector2 coneCenter, float coneRadius, float coneAngleStart, float coneAngleEnd, Vector2 circleCenter, float circleRadius)
{
	if (!CheckCollisionCircles(coneCenter, coneRadius, circleCenter, circleRadius))
		return false;

	Vector2 dp = Vector2Subtract(circleCenter, coneCenter);
	float angle = atan2f(dp.y, dp.x);
	return IsAngleBetween(angle, coneAngleStart, coneAngleEnd);
}
bool CheckCollisionLineCircle(Vector2 p0, Vector2 p1, Vector2 center, float radius)
{
	float d = DistanceLineToPoint(p0, p1, center);
	return d < radius;
}
Vector2 ResolveCollisionCircleRec(Vector2 center, float radius, Rectangle rect)
{
	// https://www.youtube.com/watch?v=D2a5fHX-Qrs

	Vector2 nearestPoint;
	nearestPoint.x = Clamp(center.x, rect.x, rect.x + rect.width);
	nearestPoint.y = Clamp(center.y, rect.y, rect.y + rect.height);

	Vector2 toNearest = Vector2Subtract(nearestPoint, center);
	float len = Vector2Length(toNearest);
	float overlap = radius - len;
	if (overlap > 0 && len > 0)
		return Vector2Subtract(center, Vector2Scale(toNearest, overlap / len));
	else
		return center;
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
RayCollision GetProjectileCollisionWithRect(Vector2 pos, Vector2 vel, Rectangle rect)
{
	Ray ray;
	ray.position = (Vector3){ pos.x, pos.y, 0.5f };
	ray.direction = (Vector3){ vel.x, vel.y, 0 };

	BoundingBox box;
	box.min = (Vector3){ rect.x, rect.y, 0 };
	box.max = (Vector3){ rect.x + rect.width, rect.y + rect.height, 1 };

	return GetRayCollisionBox(ray, box);
}
bool _fixed_CheckCollisionCircleRec(Vector2 center, float radius, Rectangle rect)
{
	// I noticed bullets were passing through the corners of glass boxes when I was using Raylib's CheckCollisionCircleRec.
	// I then switched it to CheckCollisionPointRec and the problem went away. I thus concluded that CheckCollisionCircleRec
	// is bugged, and wrote this to replace it. After the gamejam, I tried to replicate the issue in a self contained program
	// but I couldn't..

	Vector2 nearestPoint;
	nearestPoint.x = Clamp(center.x, rect.x, rect.x + rect.width);
	nearestPoint.y = Clamp(center.y, rect.y, rect.y + rect.height);

	Vector2 toNearest = Vector2Subtract(nearestPoint, center);
	float dist2 = Vector2LengthSqr(toNearest);
	return dist2 < radius * radius;
}

void GuiText(Rectangle rect, FORMAT_STRING format, ...)
{
	va_list args;
	va_start(args, format);
	char *text = TempPrintv(format, args);
	va_end(args);
	GuiLabel(rect, text);
}

void DrawDebugText(FORMAT_STRING format, ...)
{
	va_list args;
	va_start(args, format);
	DrawTextFormatv(GetMouseX() + 10, GetMouseY() - 30, 20, GRAY, format, args);
	va_end(args);
}
void DrawTextCentered(const char *text, float cx, float cy, float fontSize, Color color)
{
	Font font = GetFontDefault();
	Vector2 size = MeasureTextEx(font, text, fontSize, 1);
	DrawTextEx(font, text, Vec2(cx - 0.5f * size.x, cy - 0.5f * size.y), fontSize, 1, color);
}
void DrawTextRightAligned(const char *text, float x, float y, float fontSize, Color color)
{
	Font font = GetFontDefault();
	Vector2 size = MeasureTextEx(font, text, fontSize, 1);
	DrawTextEx(font, text, Vec2(x - size.x, y), fontSize, 1, color);
}
void DrawTextFormat(float x, float y, float fontSize, Color color, FORMAT_STRING format, ...)
{
	va_list args;
	va_start(args, format);
	DrawTextFormatv(x, y, fontSize, color, format, args);
	va_end(args);
}
void DrawTextFormatv(float x, float y, float fontSize, Color color, FORMAT_STRING format, va_list args)
{
	char *text = TempPrintv(format, args);
	DrawTextEx(GuiGetFont(), text, Vec2(x, y), fontSize, 1, color);
}
void DrawRectangleVCentered(Vector2 center, Vector2 size, Color color)
{
	Vector2 pos = { center.x - size.x / 2, center.y - size.y / 2 };
	DrawRectangleV(pos, size, color);
}
void DrawTex(Texture t, Vector2 center, Vector2 extent, Color tint)
{
	rlBegin(RL_QUADS);
	{
		rlColor(tint);
		rlSetTexture(t.id);
		rlTexCoord2f(0, 0); rlVertex2f(center.x - extent.x, center.y - extent.y);
		rlTexCoord2f(1, 0); rlVertex2f(center.x + extent.x, center.y - extent.y);
		rlTexCoord2f(1, 1); rlVertex2f(center.x + extent.x, center.y + extent.y);
		rlTexCoord2f(0, 1); rlVertex2f(center.x - extent.x, center.y + extent.y);
	}
	rlEnd();
}
void DrawTexRotated(Texture t, Vector2 center, Vector2 extent, Color tint, float angleRadians)
{
	float s = sinf(angleRadians);
	float c = cosf(angleRadians);
	float ew = extent.x;
	float eh = extent.y;
	float cx = center.x;
	float cy = center.y;
	float x00 = cx + c * (-ew) - s * (-eh);
	float x10 = cx + c * (+ew) - s * (-eh);
	float x11 = cx + c * (+ew) - s * (+eh);
	float x01 = cx + c * (-ew) - s * (+eh);
	float y00 = cy + s * (-ew) + c * (-eh);
	float y10 = cy + s * (+ew) + c * (-eh);
	float y11 = cy + s * (+ew) + c * (+eh);
	float y01 = cy + s * (-ew) + c * (+eh);
	rlBegin(RL_QUADS);
	{
		rlColor(tint);
		rlSetTexture(t.id);
		rlTexCoord2f(0, 0); rlVertex2f(x00, y00);
		rlTexCoord2f(1, 0); rlVertex2f(x10, y10);
		rlTexCoord2f(1, 1); rlVertex2f(x11, y11);
		rlTexCoord2f(0, 1); rlVertex2f(x01, y01);
	}
	rlEnd();
}
void DrawTexRec(Texture t, Rectangle rect, Color tint)
{
	rlBegin(RL_QUADS);
	{
		rlColor(tint);
		rlSetTexture(t.id);
		rlTexCoord2f(0, 0); rlVertex2f(rect.x, rect.y);
		rlTexCoord2f(1, 0); rlVertex2f(rect.x + rect.width, rect.y);
		rlTexCoord2f(1, 1); rlVertex2f(rect.x + rect.width, rect.y + rect.height);
		rlTexCoord2f(0, 1); rlVertex2f(rect.x, rect.y + rect.height);
	}
	rlEnd();
}
void DrawTexRecRotated(Texture t, Rectangle rect, Color tint, float angleRadians)
{
	float s = sinf(angleRadians);
	float c = cosf(angleRadians);
	float ew = 0.5f * rect.width;
	float eh = 0.5f * rect.height;
	float cx = rect.x + ew;
	float cy = rect.y + eh;
	float x00 = cx + c * (-ew) - s * (-eh);
	float x10 = cx + c * (+ew) - s * (-eh);
	float x11 = cx + c * (+ew) - s * (+eh);
	float x01 = cx + c * (-ew) - s * (+eh);
	float y00 = cy + s * (-ew) + c * (-eh);
	float y10 = cy + s * (+ew) + c * (-eh);
	float y11 = cy + s * (+ew) + c * (+eh);
	float y01 = cy + s * (-ew) + c * (+eh);
	rlBegin(RL_QUADS);
	{
		rlColor(tint);
		rlSetTexture(t.id);
		rlTexCoord2f(0, 0); rlVertex2f(x00, y00);
		rlTexCoord2f(1, 0); rlVertex2f(x10, y10);
		rlTexCoord2f(1, 1); rlVertex2f(x11, y11);
		rlTexCoord2f(0, 1); rlVertex2f(x01, y01);
	}
	rlEnd();
}
void DrawCircleGradientV(Vector2 center, Vector2 radius, Color innerColor, Color outerColor)
{
	float angles[32];
	angles[0] = 0;
	for (int j = 1; j < COUNTOF(angles) - 1; ++j)
		angles[j] = ((float)j / COUNTOF(angles)) * 2 * PI;
	angles[COUNTOF(angles) - 1] = 2 * PI;

	Vector2 points[COUNTOF(angles)];
	for (int j = 0; j < COUNTOF(points); ++j)
	{
		float angle = angles[j];
		float s = sinf(angle);
		float c = cosf(angle);
		points[j].x = center.x + c * radius.x;
		points[j].y = center.y + s * radius.y;
	}

	rlBegin(RL_TRIANGLES);
	{
		for (int j = 0; j < COUNTOF(points) - 1; ++j)
		{
			rlColor(innerColor);
			rlVertex2fv(center);
			rlColor(outerColor);
			rlVertex2f(points[j + 0].x, points[j + 0].y);
			rlVertex2f(points[j + 1].x, points[j + 1].y);
		}
	}
	rlEnd();
}
void DrawTrail(Vector2 pos, Vector2 vel, Vector2 origin, float radius, float trailLength, Color color0, Color color1)
{
	Vector2 perp1 = Vector2Scale(Vector2Normalize(Vec2(-vel.y, +vel.x)), radius);
	Vector2 perp2 = Vector2Scale(Vector2Normalize(Vec2(+vel.y, -vel.x)), radius);
	Vector2 toOrigin = Vector2Subtract(origin, pos);
	Vector2 perp3 = Vector2Scale(Vector2Normalize(toOrigin), trailLength);
	if (Vector2LengthSqr(perp3) > Vector2LengthSqr(toOrigin))
		perp3 = toOrigin;
	Vector2 trail1 = Vector2Add(pos, perp1);
	Vector2 trail2 = Vector2Add(pos, perp2);
	Vector2 trail3 = Vector2Add(pos, perp3);
	rlBegin(RL_TRIANGLES);
	{
		rlColor(color1);
		rlVertex2fv(trail1);
		rlVertex2fv(trail2);
		rlColor(color0);
		rlVertex2fv(trail3);
	}
	rlEnd();
}