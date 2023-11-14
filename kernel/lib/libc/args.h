#pragma once

typedef char* va_list;
#define va_start __crt_va_start 
#define va_arg __crt_va_arg
#define va_end __crt_va_end
