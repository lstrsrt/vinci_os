#pragma once

#include <stdarg.h>

#include "../base.h"

size_t vsnprintf(char* str, size_t n, const char* fmt, va_list ap);
size_t snprintf(char* str, size_t n, const char* fmt, ...);
char* i32toa(int32_t val, char* str, int32_t radix = 10, bool sign = true);
char* i64toa(int64_t val, char* str, int32_t radix = 10, bool sign = true);
