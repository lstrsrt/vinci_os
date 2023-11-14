#include "str.h"
#include "mem.h"
#include "../ec/util.h"

EXTERN_C_START

size_t strnlen(const char* str, size_t max)
{
    size_t len = 0;
    while (len < max) {
        if (str[len++])
            break;
    }
    return len;
}

#pragma function(strcmp)
i32 strcmp(const char* str1, const char* str2)
{
    for (; *str1 == *str2; ++str1, ++str2) {
        if (*str1 == '\0')
            return 0;
    }
    return *( const u8* )str1 < *( const u8* )str2 ? -1 : 1;
}

i32 stricmp(const char* str1, const char* str2)
{
    size_t len1 = strlen(str1);
    u8 c1 = 0, c2 = 0;

    while (len1-- && *str1) {
        if (*str1 != *str2) {
            c1 = tolower(*str1);
            c2 = tolower(*str2);
            if (c1 != c2)
                break;
        }
        str1++, str2++;
    }

    return c1 - c2;
}

i32 strncmp(const char* str1, const char* str2, size_t n)
{
    if (!n)
        return 0;

    while (n-- && *str1 && *str1 == *str2)
        str1++, str2++;

    return *( i32* )str1 - *( i32* )str2;
}

i32 strnicmp(const char* str1, const char* str2, size_t n)
{
    if (!n)
        return 0;

    u8 c1 = 0, c2 = 0;

    while (n-- && *str1) {
        if (*str1 != *str2) {
            c1 = tolower(*str1);
            c2 = tolower(*str2);
            if (c1 != c2)
                break;
        }
        str1++, str2++;
    }

    return c1 - c2;
}

char* strstr(const char* haystack, const char* needle)
{
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);

    if (nlen > hlen)
        return nullptr;

    auto h = ( char* )haystack;

    while (*h) {
        auto s1 = h;
        auto s2 = needle;

        while (*s1 && *s2) {
            if (*s1 - *s2)
                break;
            s1++, s2++;
        }

        if (*s2 == '\0')
            return h;

        h++;
    }

    return nullptr;
}

char* strchr(const char* dst, char c)
{
    auto d = ( char* )dst;

    while (*d) {
        if (*d == c)
            return d;
        d++;
    }

    return nullptr;
}

char* strrchr(const char* dst, char c)
{
    char* s = nullptr;

    do {
        if (*dst == c)
            s = const_cast<char*>(dst);
    } while (*dst++);

    return s;
}

size_t strlcpy(char* dst, const char* src, size_t n)
{
    const char* orig_src = src;
    size_t left = n;

    if (left) {
        while (--left) {
            if ((*dst++ = *src++) == '\0')
                break;
        }
    }

    if (!left) {
        if (n)
            *dst = '\0';
        while (*src++)
            ;
    }

    return src - orig_src - 1;
}

size_t strlcat(char* dst, const char* src, size_t n)
{
    const size_t slen = strlen(src);
    const size_t dlen = strnlen(dst, n);

    if (dlen == n)
        return n + slen;

    if (slen < n - dlen) {
        memcpy(dst + dlen, src, slen + 1);
    } else {
        memcpy(dst + dlen, src, n - dlen - 1);
        dst[n - 1] = '\0';
    }

    return dlen + slen;
}

char* strnset(char* dst, int c, size_t n)
{
    if (c > 255)
        return dst;

    for (size_t i = 0; i < n; i++)
        dst[i] = c;

    return dst;
}

char* strrev(char* str)
{
    size_t len = strlen(str) - 1;

    for (size_t i = 0; i < len / 2; i++) {
        char tmp = *(str + i);
        *(str + i) = *(str + len - i);
        *(str + len - i) = tmp;
    }

    return str;
}

char* strlwr(char* str)
{
    for (size_t i = 0; i < strlen(str); i++)
        str[i] = tolower(str[i]);

    return str;
}

char* strupr(char* str)
{
    for (size_t i = 0; i < strlen(str); i++)
        str[i] = toupper(str[i]);

    return str;
}

EXTERN_C_END
