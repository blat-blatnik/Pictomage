#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "lib/raylib.h"
#include "lib/raygui.h"
#include "lib/raymath.h"
#include "lib/rgestures.h"
#include "lib/rcamera.h"
#include "lib/physac.h"
#include "lib/easings.h"
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
typedef intptr_t iptr;
typedef uintptr_t uptr;
typedef unsigned int uint;

void *TempAlloc(uptr size);
void TempReset(void);

void *TempCopy(const void *data, uptr size);
char *TempString(const char *str);
char *TempPrint(FORMAT_STRING format, ...);
char *TempPrintv(FORMAT_STRING format, va_list args);

u64 HashBytes(const void *bytes, uptr size);
u64 HashString(const char *str);
void SwapMemory(void *a, void *b, uptr size);

float Wrap(float x, float max); // x -> [0, max)
float WrapMinMax(float x, float min, float max); // x -> [min, max)
bool IsAngleBetween(float target, float angle1, float angle2);
float AngleBetween(Vector2 a, Vector2 b);
int ClampInt(int x, int min, int max);
Vector2 Vec2(float x, float y);
Vector3 Vec3(float x, float y, float z);
Vector4 Vec4(float x, float y, float z, float w);
Vector2 Vec2Broadcast(float xy);
Vector3 Vec3Broadcast(float xyz);
Vector4 Vec4Broadcast(float xyzw);
Color FloatRGBA(float r, float g, float b, float a);
Color RGBA8(int r, int g, int b, int a);
Rectangle Rect(float x, float y, float width, float height);
Rectangle RectVec(Vector2 pos, Vector2 size); 
Rectangle RectMinMax(Vector2 min, Vector2 max);
Vector2 RectangleCenter(Rectangle rect);
Rectangle ExpandRectangle(Rectangle rect, float expansion);
Rectangle ExpandRectangleEx(Rectangle rect, float left, float right, float up, float down);
Vector2 Vector2Min(Vector2 a, Vector2 b);
Vector2 Vector2Max(Vector2 a, Vector2 b);
Vector2 Vector2Floor(Vector2 v);
Vector2 Vector2Ceil(Vector2 v);

float ToRaylibDegrees(float radians);

typedef u64 Random;
Random SeedRandom(u64 seed);
u32 Random32(Random *rand);
uint RandomUint(Random *rand, uint inclusiveMin, uint exclusiveMax);
int RandomInt(Random *rand, int inclusiveMin, int exclusiveMax);
float RandomFloat(Random *rand, float inclusiveMin, float exclusiveMax);
float RandomFloat01(Random *rand);
bool RandomProbability(Random *rand, float prob);
void RandomShuffle(Random *rand, void *items, uptr elementCount, uptr elementSize);

typedef struct StringBuilder
{
	char *buffer;
	uptr capacity;
	uptr cursor;
	uptr bytesNeeded;
} StringBuilder;
StringBuilder CreateStringBuilder(char *buffer, uptr capacity);
void StringAppendString(StringBuilder *builder, const char *str);
void StringAppendBytes(StringBuilder *builder, const void *bytes, uptr size);
void StringAppendChar(StringBuilder *builder, char c);
void PrintToString(StringBuilder *builder, FORMAT_STRING format, ...);
void PrintvToString(StringBuilder *builder, FORMAT_STRING format, va_list args);

void rlColor(Color color);
void rlVertex2fv(Vector2 v);

float DistanceLineToPoint(Vector2 p0, Vector2 p1, Vector2 p);
bool CheckCollisionConeCircle(Vector2 coneCenter, float coneRadius, float coneAngleStart, float coneAngleEnd, Vector2 circleCenter, float circleRadius);
bool CheckCollisionLineCircle(Vector2 p0, Vector2 p1, Vector2 center, float radius);

void GuiText(Rectangle rect, FORMAT_STRING format, ...);

void DrawDebugText(FORMAT_STRING format, ...);
void DrawTextCentered(const char *text, float cx, float cy, float fontSize, Color color);
void DrawTextFormat(float x, float y, float fontSize, Color color, FORMAT_STRING format, ...);
void DrawTextFormatv(float x, float y, float fontSize, Color color, FORMAT_STRING format, va_list args);
void DrawRectangleVCentered(Vector2 center, Vector2 size, Color color);