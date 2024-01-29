#include "mem.h"

EXTERN_C_START

#ifdef COMPILER_MSVC
#pragma function(memcmp)
#pragma function(memcpy)
#pragma function(memset)
#endif

int memcmp(const void* buf1, const void* buf2, size_t n)
{
    const u8* a = ( const u8* )buf1;
    const u8* b = ( const u8* )buf2;
    
    while (n--) {
        if (*a++ != *b++)
            return a[-1] < b[-1] ? -1 : 1;
    }
    
    return 0;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    u8* d = ( u8* )dst;
    const u8* s = ( const u8* )src;

    while (n--)
        *d++ = *s++;

    return dst;
}

void* memset(void* dst, u32 val, size_t n)
{
    u8* d = ( u8* )dst;

    while (n--)
        *d++ = ( u8 )val;

    return dst;
}

EXTERN_C_END
