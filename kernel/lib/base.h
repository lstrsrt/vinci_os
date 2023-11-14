#pragma once

#include <cstdint>

#define INLINE          __forceinline
#define NO_INLINE       __declspec(noinline)
#define NO_RETURN       [[noreturn]]
#define UNUSED          [[maybe_unused]]
#define SUPPRESS(x)     (( void )x)
#define EMPTY_STMT      (( void )0)
#define EXTERN_C        extern "C"
#define EXTERN_C_START  EXTERN_C {
#define EXTERN_C_END    }
#define LIKELY          [[likely]]
#define UNLIKELY        [[unlikely]]
#define CODE_SEG(name)  __declspec(code_seg(name))
#define EARLY           NO_INLINE CODE_SEG("INIT")

typedef __int8 i8;
typedef __int16 i16;
typedef __int32 i32;
typedef __int64 i64;

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

typedef float f32;
typedef double f64;

typedef u8 byte;
typedef u8 uchar;
typedef i64 ssize_t;
typedef uintptr_t vaddr_t;
typedef uintptr_t paddr_t;

#define I8_MAX   INT8_MAX
#define I16_MAX  INT16_MAX
#define I32_MAX  INT32_MAX
#define I64_MAX  INT64_MAX

#define U8_MAX   UINT8_MAX
#define U16_MAX  UINT16_MAX
#define U32_MAX  UINT32_MAX
#define U64_MAX  UINT64_MAX

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
