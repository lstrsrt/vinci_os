#pragma once

#include <base.h>

namespace cmos
{
    enum class RtcStatusA : u8
    {
        Update = 1 << 7
    };

    enum class RtcStatusB : u8
    {
        Dst = 1 << 0,
        Format = 1 << 1, // 24 if 1, 12 if 0
        Binary = 1 << 2, // BCD if 0
        SquareWave = 1 << 3,
        UpdateInt = 1 << 4,
        AlarmInt = 1 << 5,
        PeriodicInt = 1 << 6,
        CycleUpdate = 1 << 7
    };

    enum class RtcStatusC : u8
    {
        UpdateIntFlag = 1 << 4,
        AlarmIntFlag = 1 << 5,
        PeriodicIntFlag = 1 << 6,
        IntRequestFlag = 1 << 7
    };

    enum class RtcStatusD : u8
    {
        Valid = 1 << 7
    };

    struct Rtc
    {
        u8 s, m, h;
        u8 weekday, day, month;
        u8 year, century;
    };

    namespace port
    {
        constexpr u16 address = 0x70;
        constexpr u16 data = 0x71;

        constexpr u16 rtc_s = 0x0;
        constexpr u16 rtc_m = 0x2;
        constexpr u16 rtc_h = 0x4;
        constexpr u16 rtc_wd = 0x6;
        constexpr u16 rtc_d = 0x7;
        constexpr u16 rtc_mon = 0x8;
        constexpr u16 rtc_y = 0x9;
        constexpr u16 rtc_c = 0x32;
        constexpr u16 rtc_status0 = 0xa;
        constexpr u16 rtc_status1 = 0xb;
        constexpr u16 rtc_status2 = 0xc;
        constexpr u16 rtc_status3 = 0xd;
    }

    inline bool rtc_has_century; // TODO - check in FADT
    inline Rtc time;

    EARLY void Initialize();

    /* Full update.
       Assumes that CMOS update flag is not set. */
    void UpdateRtc();

    /* Manual update. May invoke UpdateRtc(). */
    void UpdateTime();

    u8 Read(u8 reg);
    void Write(u8 reg, u8 data);
}
