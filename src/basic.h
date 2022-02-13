#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "lib/raylib.h"
#include "lib/raygui.h"
#include "lib/raymath.h"
#include "lib/rlgl.h"
#include "lib/rmem.h"

#ifdef _MSC_VER
#	define FORMAT_STRING _Printf_format_string_ const char *
#	define ASSERT(condition)do{\
		if (!(condition)){\
			__debugbreak();\
			TraceLog(LOG_FATAL,"Assertion failed! '%s' in %s(), file %s, line %d.",#condition,__func__,__FILE__,__LINE__);\
			abort();\
		}\
	}while(0)
#else
#	define FORMAT_STRING const char *
#	define ASSERT(condition) assert(condition)
#endif

#define COUNTOF(array) (sizeof(array)/sizeof(array[0]))

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

void *TempAlloc(size_t size);
void TempReset(void);

char *TempPrint(FORMAT_STRING format, ...);
char *TempPrintv(FORMAT_STRING format, va_list args);

u64 HashBytes(const void *bytes, size_t size);
void SwapMemory(void *a, void *b, size_t size);

float Smoothstep(float edge0, float edge1, float x);
float Wrap(float x, float max); // x -> [0, max)
float WrapMinMax(float x, float min, float max); // x -> [min, max)
bool IsAngleBetween(float target, float angle1, float angle2);
float AngleBetween(Vector2 a, Vector2 b);
int ClampInt(int x, int min, int max);
Vector2 Vec2(float x, float y);
Vector2 Vec2Broadcast(float xy);
Vector2 Vec2FromPolar(float length, float angleRadians);
Color FloatRGBA(float r, float g, float b, float a);
Color RGBA8(int r, int g, int b, int a);
Color Grayscale(float whiteness);
Color LerpColor(Color from, Color to, float amount);
Rectangle Rect(float x, float y, float width, float height);
Rectangle RectVec(Vector2 pos, Vector2 size); 
Rectangle RectMinMax(Vector2 min, Vector2 max);
Vector2 RectanglePos(Rectangle rect);
Vector2 RectangleCenter(Rectangle rect);
Rectangle ExpandRectangle(Rectangle rect, float expansion);
Rectangle ExpandRectangleEx(Rectangle rect, float left, float right, float up, float down);
Vector2 Vector2Min(Vector2 a, Vector2 b);
Vector2 Vector2Max(Vector2 a, Vector2 b);
Vector2 Vector2Floor(Vector2 v);
Vector2 Vector2Ceil(Vector2 v);
bool Vector2Equal(Vector2 a, Vector2 b);
float Vec2Angle(Vector2 v);

float ToRaylibDegrees(float radians);

typedef u64 Random;
Random SeedRandom(u64 seed);
u32 Random32(Random *rand);
int RandomInt(Random *rand, int inclusiveMin, int exclusiveMax);
float RandomFloat(Random *rand, float inclusiveMin, float exclusiveMax);
float RandomFloat01(Random *rand);
bool RandomProbability(Random *rand, float prob);
Vector2 RandomVector(Random *rand, float magnitude);
Vector2 RandomNormal(Random *rand, float mean, float sdev);

void rlColor(Color color);
void rlVertex2fv(Vector2 v);

float DistanceLineToPoint(Vector2 p0, Vector2 p1, Vector2 p);
bool CheckCollisionConeCircle(Vector2 coneCenter, float coneRadius, float coneAngleStart, float coneAngleEnd, Vector2 circleCenter, float circleRadius);
bool CheckCollisionLineCircle(Vector2 p0, Vector2 p1, Vector2 center, float radius);
Vector2 ResolveCollisionCircleRec(Vector2 center, float radius, Rectangle rect);
Vector2 ResolveCollisionCircles(Vector2 center, float radius, Vector2 obstacleCenter, float obstacleRadius);
RayCollision GetProjectileCollisionWithRect(Vector2 pos, Vector2 vel, Rectangle rect);
bool _fixed_CheckCollisionCircleRec(Vector2 center, float radius, Rectangle rect); // I found a bug with CheckCollisionCircleRec, so I made a custom one.
#define CheckCollisionCircleRec _fixed_CheckCollisionCircleRec

void GuiText(Rectangle rect, FORMAT_STRING format, ...);

void DrawDebugText(FORMAT_STRING format, ...);
void DrawTextCentered(const char *text, float cx, float cy, float fontSize, Color color);
void DrawTextFormat(float x, float y, float fontSize, Color color, FORMAT_STRING format, ...);
void DrawTextFormatv(float x, float y, float fontSize, Color color, FORMAT_STRING format, va_list args);
void DrawTextRightAligned(const char *text, float x, float y, float fontSize, Color color);
void DrawRectangleVCentered(Vector2 center, Vector2 size, Color color);
void DrawTex(Texture t, Vector2 center, Vector2 extent, Color tint);
void DrawTexRotated(Texture t, Vector2 center, Vector2 extent, Color tint, float angleRadians);
void DrawTexRec(Texture t, Rectangle rect, Color tint);
void DrawTexRecRotated(Texture t, Rectangle rect, Color tint, float angleRadians);
void DrawCircleGradientV(Vector2 center, Vector2 radius, Color innerColor, Color outerColor);
void DrawTrail(Vector2 pos, Vector2 vel, Vector2 origin, float radius, float trailLength, Color color0, Color color1);