#include "../ec/util.h"
#include "print.h"
#include "mem.h"
#include "str.h"

char* i64toa(int64_t val, char* str, int32_t radix, bool sign)
{
    static const char table[] = {
        "0123456789abcdefghijklmnopqrstuvwxyz"
    };

    uint64_t uval = ( uint64_t )val;
    bool negative = sign && val < 0;
    char buffer[64 + sizeof('\0')];

    if (radix < 2)
        radix = 2;
    else if (radix > 36)
        radix = 36;

    if (negative)
        uval = 0ull - uval;

    auto end = &buffer[sizeof buffer - 1];
    auto pos = end;

    do
    {
        *--pos = table[uval % radix];
        uval /= radix;
    } while (uval);

    if (negative)
        *--pos = '-';

    auto len = (end - pos) * sizeof(char);
    memcpy(( void* )str, ( const void* )pos, len);
    str[len] = '\0';

    return str;
}

char* i32toa(int32_t val, char* str, int32_t radix, bool sign)
{
    static const char table[] = {
        "0123456789abcdefghijklmnopqrstuvwxyz"
    };

    uint32_t uval = ( uint32_t )val;
    bool negative = sign && val < 0;
    char buffer[32 + sizeof('\0')];

    if (radix < 2)
        radix = 2;
    else if (radix > 36)
        radix = 36;

    if (negative)
        uval = 0ul - uval;

    auto end = &buffer[sizeof buffer - 1];
    auto pos = end;

    do
    {
        *--pos = table[uval % radix];
        uval /= radix;
    } while (uval);

    if (negative)
        *--pos = '-';

    auto len = (end - pos) * sizeof(char);
    memcpy(( void* )str, ( const void* )pos, len);
    str[len] = '\0';

    return str;
}

enum Fmt
{
    FMT_NONE,
    FMT_PERCENT = 1 << 0,

    FMT_STRING = 1 << 1,
    FMT_CHAR = 1 << 2,
    FMT_BOOL = 1 << 3,
    FMT_INT = 1 << 4,
    FMT_UNSIGNED = 1 << 5,
    FMT_LONG = 1 << 6,
    FMT_LLONG = 1 << 7,
    FMT_HEX = 1 << 8,
};

static int parse_l(const char* fmt)
{
    if (*fmt++ == 'l')
    {
        if (*fmt == 'x')
            return FMT_LLONG | FMT_HEX;
        else if (*fmt == 'u')
            return FMT_LLONG | FMT_UNSIGNED;
        else
            return FMT_LLONG;
    }
    else if (*--fmt == 'u')
        return FMT_LONG | FMT_UNSIGNED;
    else if (*fmt == 'x')
        return FMT_LONG | FMT_HEX;
    return FMT_LONG;
}

static Fmt parse(const char* fmt)
{
    switch (*fmt++)
    {
    case '%': return FMT_PERCENT;
    case 'c': return FMT_CHAR;
    case 's': return FMT_STRING;
    case 'b': return FMT_BOOL;
    case 'd': return FMT_INT;
    case 'u': return FMT_UNSIGNED;
    case 'x': return FMT_HEX;
    case 'l': return ( Fmt )parse_l(fmt);
    }
    return FMT_NONE;
}

size_t vsnprintf(char* str, size_t n, const char* fmt, va_list ap)
{
    // FIXME - str[len++] is not a good idea
    size_t len = 0;
    while (auto c = *fmt++)
    {
        if (c == '%')
        {
            auto f = parse(fmt);
            int skip = 1;
            if (f & (FMT_LONG | FMT_LLONG))
            {
                if (f & FMT_LLONG)
                    skip++;
                if (f & (FMT_UNSIGNED | FMT_HEX))
                    skip++;
            }
            fmt += skip;
            if (f & (FMT_INT | FMT_UNSIGNED | FMT_LONG | FMT_LLONG | FMT_HEX))
            {
                char buf[64];
                if (f & FMT_LLONG)
                {
                    auto tmp_llong = va_arg(ap, long long);
                    i64toa(tmp_llong, buf, (f & FMT_HEX) ? 16 : 10, !(f & (FMT_UNSIGNED | FMT_HEX)));
                }
                else
                {
                    auto tmp_int = va_arg(ap, int);
                    i32toa(tmp_int, buf, (f & FMT_HEX) ? 16 : 10, !(f & (FMT_UNSIGNED | FMT_HEX)));
                }
                len += strlcpy(&str[len], buf, n - len);
            }
            else if (f & FMT_CHAR)
            {
                auto tmp_ch = ( char )va_arg(ap, int);
                str[len++] = tmp_ch;
            }
            else if (f & FMT_STRING)
            {
                auto tmp_str = va_arg(ap, char*);
                len += strlcpy(&str[len], tmp_str, n - len);
            }
            else if (f & FMT_BOOL)
            {
                auto tmp_bool = ( bool )va_arg(ap, int);
                str[len++] = tmp_bool;
            }
            else if (f & FMT_PERCENT)
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

size_t snprintf(char* str, size_t n, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	size_t len = vsnprintf(str, n, fmt, ap);
	va_end(ap);
	return len;
}
