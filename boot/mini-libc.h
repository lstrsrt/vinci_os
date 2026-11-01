#pragma once

#include <stdarg.h>

#include "../kernel/lib/base.h"
#include "../kernel/lib/ec/concepts.h"

#ifdef COMPILER_MSVC
#pragma function(memcmp)
#pragma function(memcpy)
#pragma function(memset)
#endif

#ifndef COMPILER_MSVC
inline
#endif
    int memcmp(const void* buf1, const void* buf2, size_t n)
{
    const u8* a = ( const u8* )buf1;
    const u8* b = ( const u8* )buf2;

    while (n--)
    {
        if (*a++ != *b++)
            return a[-1] < b[-1] ? -1 : 1;
    }

    return 0;
}

#ifndef COMPILER_MSVC
inline
#endif
    void* memcpy(void* dst, const void* src, size_t n)
{
    u8* d = ( u8* )dst;
    const u8* s = ( const u8* )src;

    while (n--)
        *d++ = *s++;

    return dst;
}

#ifndef COMPILER_MSVC
inline
#endif
    void* memset(void* dst, u32 val, size_t n)
{
    u8* d = ( u8* )dst;

    while (n--)
        *d++ = ( u8 )val;

    return dst;
}

constexpr i32 strncmp(const char* str1, const char* str2, size_t n)
{
    const u8* c1 = ( const u8* )str1;
    const u8* c2 = ( const u8* )str2;
    u8 ch = 0;
    i32 d = 0;

    while (n--)
    {
        d = ( i32 )(ch = *c1++) - ( i32 )*c2++;
        if (d || !ch)
            break;
    }

    return d;
}

inline size_t strlcpy16(char16_t* dst, const char16_t* src, size_t n)
{
    const char16_t* orig_src = src;
    size_t left = n;

    if (left)
    {
        while (--left)
        {
            if ((*dst++ = *src++) == '\0')
                break;
        }
    }

    if (!left)
    {
        if (n)
            *dst = '\0';
        while (*src++)
            ;
    }

    return src - orig_src - 1;
}

template<class T>
inline char16_t* itow(T val, char16_t* str, int32_t radix = 10, bool sign = true)
{
    static const char16_t table[] = {u"0123456789abcdefghijklmnopqrstuvwxyz"};

    auto uval = static_cast<ec::unsigned_t<T>>(val);
    bool negative = sign && val < 0;
    char16_t buffer[(sizeof(T) * 8) + sizeof('-') + sizeof('\0')];

    if (radix < 2)
        radix = 2;
    else if (radix > 36)
        radix = 36;

    if (negative)
        uval = 0 - uval;

    auto end = &buffer[ARRAY_SIZE(buffer) - 1];
    auto pos = end;

    do
    {
        *--pos = table[uval % radix];
        uval /= radix;
    }
    while (uval);

    if (negative)
        *--pos = '-';

    auto count = end - pos;
    memcpy(str, pos, count * sizeof(char16_t));
    str[count] = '\0';

    return str;
}

namespace
{

    enum Fmt
    {
        SPEC_NONE,
        SPEC_PERCENT = 1 << 0,

        SPEC_STRING = 1 << 1,
        SPEC_CHAR = 1 << 2,
        SPEC_BOOL = 1 << 3,
        SPEC_INT = 1 << 4,
        SPEC_UNSIGNED = 1 << 5,
        SPEC_LONG = 1 << 6,
        SPEC_LLONG = 1 << 7,
        SPEC_HEX = 1 << 8,
        SPEC_POINTER = 1 << 9,
    };

    inline int parse_l(const char16_t* fmt)
    {
        if (*fmt++ == 'l')
        {
            if (*fmt == 'x')
                return SPEC_LLONG | SPEC_HEX;
            if (*fmt == 'u')
                return SPEC_LLONG | SPEC_UNSIGNED;
            if (*fmt == 'd')
                return SPEC_LLONG | SPEC_INT;
            return SPEC_LLONG;
        }
        if (*--fmt == 'u')
            return SPEC_LONG | SPEC_UNSIGNED;
        if (*fmt == 'x')
            return SPEC_LONG | SPEC_HEX;
        if (*fmt == 'd')
            return SPEC_LONG | SPEC_INT;
        return SPEC_LONG;
    }

    inline Fmt parse(const char16_t* fmt)
    {
        switch (*fmt++)
        {
        case '%':
            return SPEC_PERCENT;
        case 'c':
            return SPEC_CHAR;
        case 's':
            return SPEC_STRING;
        case 'b':
            return SPEC_BOOL;
        case 'd':
            return SPEC_INT;
        case 'u':
            return SPEC_UNSIGNED;
        case 'x':
            return SPEC_HEX;
        case 'p':
            return SPEC_POINTER;
        case 'l':
            return ( Fmt )parse_l(fmt);
        }
        return SPEC_NONE;
    }

} // namespace

inline size_t vwsnprintf(char16_t* str, size_t n, const char16_t* fmt, va_list ap)
{
    if (n == 0)
        return 0;

    size_t len{};
    char16_t c;
    while (len < n && (c = *fmt++))
    {
        if (c == '%')
        {
            auto f = parse(fmt);
            if (f == SPEC_NONE)
                continue;
            int skip = 1;
            if (f & (SPEC_LONG | SPEC_LLONG))
            {
                if (f & SPEC_LLONG)
                    skip++;
                if (f & (SPEC_UNSIGNED | SPEC_HEX | SPEC_INT))
                    skip++;
            }
            fmt += skip;
            if (f & SPEC_POINTER)
                f = ( Fmt )(SPEC_UNSIGNED | SPEC_LLONG | SPEC_HEX);
            if (f & (SPEC_INT | SPEC_UNSIGNED | SPEC_LONG | SPEC_LLONG | SPEC_HEX))
            {
                char16_t buf[64];
                if (f & SPEC_LLONG)
                {
                    auto tmp_llong = va_arg(ap, long long);
                    itow(tmp_llong, buf, (f & SPEC_HEX) ? 16 : 10, !(f & (SPEC_UNSIGNED | SPEC_HEX)));
                }
                else
                {
                    auto tmp_int = va_arg(ap, int);
                    itow(tmp_int, buf, (f & SPEC_HEX) ? 16 : 10, !(f & (SPEC_UNSIGNED | SPEC_HEX)));
                }
                len += strlcpy16(&str[len], buf, n - len);
            }
            else if (f & SPEC_CHAR)
            {
                auto tmp_ch = ( char16_t )va_arg(ap, int);
                str[len++] = tmp_ch;
            }
            else if (f & SPEC_STRING)
            {
                auto tmp_str = va_arg(ap, char16_t*);
                len += strlcpy16(&str[len], tmp_str, n - len);
            }
            else if (f & SPEC_BOOL)
            {
                auto tmp_bool = ( bool )va_arg(ap, int);
                str[len++] = tmp_bool ? u'1' : u'0';
            }
            else if (f & SPEC_PERCENT)
            {
                str[len++] = '%';
            }
        }
        else
        {
            str[len++] = c;
        }
    }
    if (len > n)
        len = n;
    str[len] = '\0';
    return len;
}
