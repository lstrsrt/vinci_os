#pragma once

#include <base.h>

// TODO we don't use these yet

namespace ec::inline constants
{
    static constexpr u8 u8_max = 0xff;
    static constexpr u16 u16_max = 0xffff;
    static constexpr u32 u32_max = 0xffff'ffff;
    static constexpr u64 u64_max = 0xffffffff'ffffffff;

    static constexpr i8 i8_max = u8_max >> 1;
    static constexpr i16 i16_max = u16_max >> 1;
    static constexpr i32 i32_max = u32_max >> 1;
    static constexpr i64 i64_max = u64_max >> 1;
     
    static constexpr i8 i8_min = ~i8_max;
    static constexpr i16 i16_min = ~i16_max;
    static constexpr i32 i32_min = ~i32_max;
    static constexpr i64 i64_min = ~i64_max;
     
    static constexpr size_t size_t_max = u64_max;
    static constexpr ssize_t ssize_t_max = i64_max; 
     
    static constexpr f32 f32_max = 3.402823466e+38f;
    static constexpr f64 f64_max = 1.7976931348623158e+308;
     
    static constexpr f32 f32_min = 1.175494351e-38f;
    static constexpr f64 f64_min = 2.2250738585072014e-308;
}
