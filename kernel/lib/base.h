#pragma once

#include <stdint.h>

#ifdef __clang__
#define COMPILER_CLANG
#else
#ifdef _MSC_VER
#define COMPILER_MSVC
#endif
#endif

#ifdef COMPILER_CLANG
#define INLINE          __attribute__((always_inline)) __inline__
#define NO_INLINE       __attribute__((noinline))
#else
#define INLINE          __forceinline
#define NO_INLINE       __declspec(noinline)
#endif

#ifdef COMPILER_MSVC
#define MSVC_INTRINSIC  [[msvc::intrinsic]]
#else
#define MSVC_INTRINSIC
#endif

#ifdef COMPILER_CLANG
#define CODE_SEG(name)  __attribute__((section(name)))
#define ALLOC_FN        __attribute__((malloc))
#else
#define CODE_SEG(name)  __declspec(code_seg(name))
#define ALLOC_FN        __declspec(restrict)
#endif

#ifdef COMPILER_CLANG
#define UNREACHABLE()   __builtin_unreachable()
#else
#define UNREACHABLE()   __assume(false)
#endif

#define OFFSET(t, f)    __builtin_offsetof(t, f)
#define EARLY           NO_INLINE CODE_SEG("INIT")
#define SUPPRESS(x)     (( void )x)
#define EMPTY_STMT      (( void )0)
#define EXTERN_C        extern "C"
#define EXTERN_C_START  EXTERN_C {
#define EXTERN_C_END    }
#define NO_RETURN       [[noreturn]]
#define UNUSED          [[maybe_unused]]
#define LIKELY          [[likely]]
#define UNLIKELY        [[unlikely]]

#ifdef COMPILER_CLANG
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef __int8 i8;
typedef __int16 i16;
typedef __int32 i32;
typedef __int64 i64;

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;
#endif

typedef float f32;
typedef double f64;

typedef u8 byte;
typedef u8 uchar;
typedef u64 size_t;
typedef i64 ssize_t;
typedef uintptr_t uptr_t;
typedef intptr_t iptr_t;

/* Some headers may already have this defined. */
#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

#define OFF    0
#define ON     1

#define KB(x)    (1000ULL * x)
#define MB(x)    (1000ULL * KB(x))
#define GB(x)    (1000ULL * MB(x))

#define KiB(x)   (1024ULL * x)
#define MiB(x)   (1024ULL * KiB(x))
#define GiB(x)   (1024ULL * MiB(x))

/* Some third party include may have these defined already. */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof a / sizeof a[0])
#endif

#ifndef IS_ALIGNED
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)
#endif

/*
*  Bit extraction macros.
*  x's bits in range [a, b) (counting from 0) are returned.
*  x's size is never taken into account!
*  In other words, HIGH16 on a u64 will not return bits 48-64, but bits 16-32.
*  Instead, use EXTRACT64(x, 48, 64).
*/

#define EXTRACT16(x, a, b)  (( u16 )(((( u16 )x >> a) & (1 << (b - a)) - 1)))
#define EXTRACT32(x, a, b)  (( u32 )(((( u32 )x >> a) & (1 << (b - a)) - 1)))
#define EXTRACT64(x, a, b)  (( u64 )(((( u64 )x >> a) & (( u64 )1 << (b - a)) - 1)))

#define LOW8(x)   (( u8 )EXTRACT16(x, 0, 8))
#define LOW16(x)  (( u16 )EXTRACT32(x, 0, 16))
#define LOW32(x)  (( u32 )EXTRACT64(x, 0, 32))

#define HIGH8(x)   (( u8 )EXTRACT16(x, 8, 16))
#define HIGH16(x)  (( u16 )EXTRACT32(x, 16, 32))
#define HIGH32(x)  (( u32 )EXTRACT64(x, 32, 64))

#define MAKE16(h, l)  (( u16 )(( u16 )h << 8) | l)
#define MAKE32(h, l)  (( u32 )(( u32 )h << 16) | l)
#define MAKE64(h, l)  (( u64 )(( u64 )h << 32) | l)

#define CONCAT_IMPL(a, b)  a##b
#define CONCAT(a, b)       CONCAT_IMPL(a, b)

/* Generic padding macro for types with opaque/unused/reserved members. */
#define PAD(size) private: UNUSED byte CONCAT(__pad, __COUNTER__)[size]; public:
