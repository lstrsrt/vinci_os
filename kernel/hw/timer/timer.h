#pragma once

#include <ec/enums.h>

namespace timer
{
#pragma data_seg(".data")
    inline volatile u64 ticks = 0;
    inline volatile u64 seconds = 0;
#pragma data_seg()

    EARLY void Initialize(u64 hpet_address);

    void Isr();

    namespace pit
    {
        enum class BcdBinary : u8
        {
            Binary,
            Bcd = 1 << 0,
        };

        enum class Mode : u8
        {
            IntOnTerminalCount = 0 << 1,
            HwOneShot = 1 << 1,
            RateGenerator = 2 << 1,
            SquareWave = 3 << 1,
            SwStrobe = 4 << 1,
            HwStrobe = 5 << 1
        };

        enum class Access : u8
        {
            LatchCountValue = 0 << 4,
            Low = 1 << 4,
            High = 1 << 5,
            LowHigh = 3 << 4
        };

        enum class Channel : u8
        {
            Zero = 0 << 6,
            One = 1 << 6,
            Two = 1 << 7,
            ReadBack = 3 << 6
        };

        namespace port
        {
            static constexpr u16 data0 = 0x40;
            static constexpr u16 data1 = 0x41;
            static constexpr u16 data2 = 0x42;
            static constexpr u16 ctrl = 0x43;
        }

        static constexpr u32 clock_rate = 1193182;
        static constexpr u16 divisor = clock_rate / 1000;

        EARLY void Initialize();
    }

    namespace hpet
    {
        enum Features : u32
        {
            FeatLegacyReplacement = 1 << 15
        };

        enum Config : u64
        {
            CfgEnable = 1 << 0,
            CfgLegacyReplacement = 1 << 1
        };

        enum TimerConfig : u64
        {
            TmrLevelTriggered = 1 << 1,
            TmrEnable = 1 << 2,
            TmrPeriodic = 1 << 3,
            TmrPeriodicSupport = 1 << 4,
            Tmr64 = 1 << 5,
            TmrSetAccumulator = 1 << 6,
            TmrForce32 = 1 << 8,
            TmrApicRouting = 1 << 13,
            TmrFsb = 1 << 14,
            TmrFsbSupport = 1 << 15,
        };

#pragma pack(1)
        struct Timer
        {
            TimerConfig general;
            u64 comparator;
            u64 fsb;
            PAD(8)
        };

#pragma warning(disable: 4200) // zero-sized array
        struct Registers
        {
            Features general;
            u32 tick_period;
            PAD(8)
            Config config;
            PAD(8)
            u32 t0_int_status : 1;
            u32 t1_int_status : 1;
            u32 t2_int_status : 1;
            u32 reserved0 : 29;
            u32 reserved1;
            PAD(200)
            u64 counter;
            PAD(8)
            Timer timers[];
        };
        static_assert(sizeof(Registers) == 0x100);
#pragma warning(default: 4200)
#pragma pack()

        static constexpr u32 hz = 1000;

        EARLY bool Initialize(u64 hpet_address);
    }
}
