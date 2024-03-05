#pragma once

#include "../base.h"

EXTERN_C_START

#ifndef COMPILER_MSVC
constexpr size_t strlen(const char* str)
{
    size_t i = 0;
    while (str[i])
        i++;
    return i;
}
#endif
#define strlen __builtin_strlen
size_t strnlen(const char* str, size_t max);
i32 strcmp(const char* str1, const char* str2);
i32 stricmp(const char* str1, const char* str2);
i32 strncmp(const char* str1, const char* str2, size_t n);
i32 strnicmp(const char* str1, const char* str2, size_t n);
char* strstr(const char* haystack, const char* needle);
char* stristr(const char* haystack, const char* needle);
char* strchr(const char* dst, char c);
char* strrchr(const char* dst, char c);
size_t strlcat(char* dst, const char* src, size_t n);
size_t strlcpy(char* dst, const char* src, size_t n);
char* strnset(char* str, int c, size_t n);
char* strrev(char* str);
char* strlwr(char* str);
char* strupr(char* str);

EXTERN_C_END

template<size_t n>
char* strset(char(&str)[n], int c)
{
    return strnset(str, c, n - 1);
}

template<size_t n>
char* strrchr(char(&dst)[n], int c)
{
    return strrchr(dst, c, n);
}

template<size_t n>
size_t strcpy(char(&dst)[n], const char* src)
{
    return strlcpy(dst, src, n);
}

template<size_t n>
size_t strcat(char(&dst)[n], const char* src)
{
    return strlcat(dst, src, n);
}

constexpr i32 toupper(uchar uc)
{
    if (uc >= 'a' && uc <= 'z')
        return 'A' + (uc - 'a');
    return uc;
}

constexpr i32 tolower(uchar uc)
{
    if (uc >= 'A' && uc <= 'Z')
        return 'a' + (uc - 'A');
    return uc;
}
