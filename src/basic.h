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

Vector2 Vec2(float x, float y);
Vector3 Vec3(float x, float y, float z);
Vector4 Vec4(float x, float y, float z, float w);
Vector2 Vec2Broadcast(float xy);
Vector3 Vec3Broadcast(float xyz);
Vector4 Vec4Broadcast(float xyzw);
Rectangle Rect(float x, float y, float width, float height);
Rectangle RectVec(Vector2 pos, Vector2 size); 
Rectangle RectMinMax(Vector2 min, Vector2 max);

typedef u64 Random;
Random SeedRandom(u64 seed);
u32 Random32(Random *rand);
uint RandomUint(Random *rand, uint inclusiveMin, uint exclusiveMax);
int RandomInt(Random *rand, int inclusiveMin, int exclusiveMax);
float RandomFloat(Random *rand, float inclusiveMin, float exclusiveMax);
float RandomFloat01(Random *rand);
bool RandomProbability(Random *rand, float prob);
void RandomShuffle(Random *rand, void *items, uptr elementCount, uptr elementSize);

float NoiseX(uint seed, int x);
float NoiseXY(uint seed, int x, int y);
float NoiseXYZ(uint seed, int x, int y, int z);

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