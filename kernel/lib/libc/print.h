#pragma once

#include "../base.h"
#include "args.h"

size_t vsnprintf(char* str, size_t n, const char* fmt, va_list ap);
size_t snprintf(char* str, size_t n, const char* fmt, ...);
