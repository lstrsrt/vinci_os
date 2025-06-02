#include "timer.h"
#include "../cpu/x64.h"
#include "../cpu/isr.h"
#include "../cmos/cmos.h"
#include "../gfx/output.h"

namespace timer
{
    EARLY void Initialize(u64 hpet_address)
    {
        x64::ConnectIsr(timer::Isr, 0);

        // cmos::Initialize();

        if (hpet::Initialize(hpet_address))
        {
            Print("Using HPET for timer interrupts.\n");
        }
        else
        {
            Print("Using PIT for timer interrupts.\n");
            pit::Initialize();
        }
    }

    void Isr()
    {
        ticks++;
    }
}

namespace timer::pit
{
    EARLY void Initialize()
    {
        const auto channel = ( u8 )Channel::Zero;
        const auto access = ( u8 )Access::LowHigh;
        const auto mode = ( u8 )Mode::RateGenerator;
        const auto bcd_binary = ( u8 )BcdBinary::Binary;

        WritePort8(port::ctrl, channel | access | mode | bcd_binary);
        WritePort8(port::data0, LOW8(divisor));
        WritePort8(port::data0, HIGH8(divisor));
    }
}

namespace timer::hpet
{
    EARLY bool Initialize(u64 hpet_address)
    {
        // TODO - accessing registers in Release builds hangs the system
        auto regs = ( volatile Registers* )hpet_address;
        auto time = regs->tick_period / hz;
        auto period = ( u64 )regs->counter + time;

        // general[12:8] is the highest timer # counting from 0.
        u32 timer_count = EXTRACT64(regs->general, 8, 12) + 1;
        for (u32 i = 0; i < timer_count; i++)
        {
            auto& timer = regs->timers[i];
            if (timer.general & TmrPeriodicSupport)
            {
                // Enable timer and set to periodic
                timer.general = ( TimerConfig )(timer.general |
                    TmrEnable | TmrPeriodic | TmrSetAccumulator);

                // Also enable HPET here
                regs->config = ( Config )(regs->config | CfgEnable);

                // Reset counter and set the comparator and accumulator
                regs->counter = 0;
                timer.comparator = ( u64 )regs->tick_period + period;
                timer.comparator = period;
                return true;
            }
        }

        return false;
    }
}
