#include "timer.h"
#include "../cmos/cmos.h"
#include "../cpu/isr.h"
#include "../cpu/x64.h"
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
} // namespace timer

namespace timer::pit
{
    EARLY void Initialize()
    {
        const auto channel = ( u8 )Channel::Zero;
        const auto access = ( u8 )Access::LowHigh;
        const auto mode = ( u8 )Mode::RateGenerator;
        const auto bcd_binary = ( u8 )BcdBinary::Binary;

        x64::WritePort8(port::ctrl, channel | access | mode | bcd_binary);
        x64::WritePort8(port::data0, LOW8(divisor));
        x64::WritePort8(port::data0, HIGH8(divisor));
    }
} // namespace timer::pit

namespace timer::hpet
{
    EARLY bool Initialize(u64 hpet_address)
    {
        if (!hpet_address)
            return false;

        auto regs = ( volatile Registers* )hpet_address;
        if (!regs->tick_period)
            return false;

        // tick_period is the length of one counter tick in femtoseconds (10^-15 s).
        const u64 ticks_per_interrupt = 1'000'000'000'000'000ull / regs->tick_period / hz;

        // Only timer 0 is supported (legacy conf).
        auto& timer = regs->timers[0];
        if (timer.general & TmrPeriodicSupport)
        {
            // Halt the main counter while resetting it (writing it while running is undefined).
            regs->config = ( Config )((regs->config & ~CfgEnable) | CfgLegacyReplacement);
            regs->counter = 0;

            // Enable timer and set to periodic
            timer.general
                = ( TimerConfig )((timer.general & ~TmrLevelTriggered) | TmrEnable | TmrPeriodic | TmrSetAccumulator);

            // With TmrSetAccumulator set, the first write sets the first deadline
            // and the second sets the period added after each interrupt.
            timer.comparator = ticks_per_interrupt;
            timer.comparator = ticks_per_interrupt;

            // Start the main counter
            regs->config = ( Config )(regs->config | CfgEnable);
            return true;
        }

        return false;
    }
} // namespace timer::hpet
