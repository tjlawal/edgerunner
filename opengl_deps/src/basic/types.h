#pragma once

// Keywords Macros
#define global   static
#define internal static
#define local    static

// Base Types
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s8  b8;
typedef s16 b16;
typedef s32 b32;
typedef s64 b64;

typedef float  f32;
typedef double f64;

// TJ's OpenGL Types
typedef int8_t glbyte;
typedef int8_t glchar;
typedef int8_t glcharARB;

typedef int16_t glshort;

typedef int32_t glint;
typedef int32_t glclampx;
typedef int32_t glsizei;

typedef uint8_t glbool;
typedef uint8_t glubyte;

typedef uint16_t glushort;

typedef uint32_t gluint;
typedef uint32_t glenum;
typedef uint32_t glbitfield;

typedef float glfloat;
typedef float glclampf;
typedef double gldouble;
typedef double glclampd;

typedef void glvoid;

// Units
#define KB(n)       (((u64)(n)) << 10)
#define MB(n)       (((u64)(n)) << 20)
#define GB(n)       (((u64)(n)) << 30)
#define TB(n)       (((u64)(n)) << 40)
#define Thousand(n) ((n) * 1000)
#define Million(n)  ((n) * 1000000)
#define Billion(n)  ((n) * 1000000000)
