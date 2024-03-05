#pragma once

#include "../base.h"

EXTERN_C_START

int memcmp(const void* buf1, const void* buf2, unsigned long long n);
void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, u32 val, size_t n);

EXTERN_C_END

#define memzero(dst, n) memset(dst, 0, n)
