#pragma once

#define va_start __crt_va_start
#define va_arg __crt_va_arg
#define va_end __crt_va_end

#include <base.h>
#include <ec/util.h>

#pragma function(memcpy)
void* memcpy(void* dst, const void* src, size_t n)
{
    u8* d = ( u8* )dst;
    const u8* s = ( const u8* )src;

    while (n--)
        *d++ = *s++;

    return dst;
}

#pragma function(memset)
void* memset(void* dst, u32 val, size_t n)
{
    u8* d = ( u8* )dst;

    while (n--)
        *d++ = ( u8 )val;

    return dst;
}

constexpr i32 strncmp(const char* str1, const char* str2, size_t n)
{
    if (!n)
        return 0;

    while (n-- && *str1 && *str1 == *str2)
        str1++, str2++;

    return *( i32* )str1 - *( i32* )str2;
}

constexpr size_t strlen16(const char16_t* s)
{
    char16_t* start = const_cast<char16_t*>(s);

    while (*s != 0)
        s++;

    return start - s;
}

size_t strlcpy16(char16_t* dst, const char16_t* src, size_t n)
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

size_t strcpy16(char16_t* dst, const char16_t* src)
{
    return strlcpy16(dst, src, strlen16(src) + 1);
}

// Adapted from https://github.com/RoelofBerg/Utf8Ucs2Converter/

bool utf8_to_ucs2_char(char16_t* ucs2, const char8_t* utf8, size_t& utf8_len)
{
    auto* unit = ( const unsigned char* )utf8;

    *ucs2 = u'?';
    utf8_len = 1;

    if (unit[0] < 0x80)
    {
        *ucs2 = ( char16_t )unit[0];
    }
    else if ((unit[0] & 0xe0) == 0xc0)
    {
        if ((unit[1] & 0xc0) != 0x80)
            return false;
        utf8_len = 2;
        *ucs2 = ( char16_t )((unit[0] & 0x1f) << 6 | (unit[1] & 0x3f));
    }
    else if ((unit[0] & 0xf0) == 0xe0)
    {
        if (((unit[1] & 0xc0) != 0x80) || ((unit[2] & 0xc0) != 0x80))
            return false;
        utf8_len = 3;
        *ucs2 = ( char16_t )((unit[0] & 0x0f) << 12 | (unit[1] & 0x3f) << 6 | (unit[2] & 0x3f));
    }
    // From here on, the character exceeds the UCS-2 range
    else if ((unit[0] & 0xf8) == 0xf0)
    {
        utf8_len = 4;
        return false;
    }
    else if ((unit[0] & 0xfc) == 0xf8)
    {
        utf8_len = 5;
        return false;
    }
    else if ((unit[0] & 0xfe) == 0xfc)
    {
        utf8_len = 6;
        return false;
    }
    else
    {
        return false;
    }
    return true;
}

bool utf8_to_ucs2(char16_t* ucs2, size_t ucs2_len, const char8_t* utf8, size_t utf8_len)
{
    for (size_t i = 0, j = 0; i < ucs2_len && j < utf8_len; )
    {
        size_t len = 1;
        if (!utf8_to_ucs2_char(ucs2 + i, utf8 + j, len))
            return false;
        j += len; // The UTF-8 string has to be advanced by the amount of code units
        i++; // The UCS-2 string is only advanced by 1 because it only uses 1 unit per code point
    }
    return true;
}

char16_t* i64tow(int64_t val, char16_t* str, int32_t radix = 10, bool sign = true)
{
    static const char16_t table[] = {
        u"0123456789abcdefghijklmnopqrstuvwxyz"
    };

    uint64_t uval = ( uint64_t )val;
    bool negative = sign && val < 0;
    char16_t buffer[64 + sizeof('\0')];

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
    } while (uval);

    if (negative)
        *--pos = '-';

    auto len = (end - pos) * sizeof(char16_t);
    memcpy(( void* )str, ( const void* )pos, len);
    str[len] = '\0';

    return str;
}

char16_t* itow(int32_t val, char16_t* str, int32_t radix = 10, bool sign = true)
{
    static const char16_t table[] = {
        u"0123456789abcdefghijklmnopqrstuvwxyz"
    };

    uint32_t uval = ( uint32_t )val;
    bool negative = sign && val < 0;
    char16_t buffer[32 + sizeof('\0')];

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
    } while (uval);

    if (negative)
        *--pos = '-';

    auto len = (end - pos) * sizeof(char16_t);
    memcpy(( void* )str, ( const void* )pos, len);
    str[len] = '\0';

    return str;
}

namespace {

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

    int parse_l(const char16_t* fmt)
    {
        if (*fmt++ == 'l')
        {
            if (*fmt == 'x')
                return SPEC_LLONG | SPEC_HEX;
            else if (*fmt == 'u')
                return SPEC_LLONG | SPEC_UNSIGNED;
            else
                return SPEC_LLONG;
        }
        else if (*--fmt == 'u')
            return SPEC_LONG | SPEC_UNSIGNED;
        else if (*fmt == 'x')
            return SPEC_LONG | SPEC_HEX;
        return SPEC_LONG;
    }

    Fmt parse(const char16_t* fmt)
    {
        switch (*fmt++)
        {
        case '%': return SPEC_PERCENT;
        case 'c': return SPEC_CHAR;
        case 's': return SPEC_STRING;
        case 'b': return SPEC_BOOL;
        case 'd': return SPEC_INT;
        case 'u': return SPEC_UNSIGNED;
        case 'x': return SPEC_HEX;
        case 'p': return SPEC_POINTER;
        case 'l': return ( Fmt )parse_l(fmt);
        default: return SPEC_NONE;
        }
        return SPEC_NONE;
    }

}

size_t vwsnprintf(char16_t* str, size_t n, const char16_t* fmt, va_list ap)
{
    size_t len{};
    while (auto c = *fmt++)
    {
        if (c == '%')
        {
            auto f = parse(fmt);
            int skip = 1;
            if (f & (SPEC_LONG | SPEC_LLONG))
            {
                if (f & SPEC_LLONG)
                    skip++;
                if (f & (SPEC_UNSIGNED | SPEC_HEX))
                    skip++;
            }
            fmt += skip;
            if (f & SPEC_POINTER)
                f = ( Fmt )(SPEC_LLONG | SPEC_HEX);
            if (f & (SPEC_INT | SPEC_UNSIGNED | SPEC_LONG | SPEC_LLONG | SPEC_HEX))
            {
                char16_t buf[64]{};
                if (f & SPEC_LLONG)
                {
                    auto tmp_llong = va_arg(ap, long long);
                    i64tow(tmp_llong, buf, (f & SPEC_HEX) ? 16 : 10, !(f & (SPEC_UNSIGNED | SPEC_HEX)));
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
                auto tmp_ch = va_arg(ap, char16_t);
                str[len++] = tmp_ch;
            }
            else if (f & SPEC_STRING)
            {
                auto tmp_str = va_arg(ap, char16_t*);
                len += strlcpy16(&str[len], tmp_str, n - len);
            }
            else if (f & SPEC_BOOL)
            {
                auto tmp_bool = va_arg(ap, bool);
                str[len++] = tmp_bool;
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
    return len;
}

size_t wsnprintf(char16_t* str, uint32_t n, const char16_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    auto len = vwsnprintf(str, n, fmt, ap);
    va_end(ap);
    return len;
}
