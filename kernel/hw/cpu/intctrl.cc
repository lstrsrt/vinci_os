#include <libc/mem.h>
#include <ec/const.h>

#include "x64.h"
#include "isr.h"
#include "msr.h"
#include "../timer/timer.h"
#include "../../core/ke.h"

// #define DEBUG_CTX_SWITCH

#ifdef DEBUG_CTX_SWITCH
#include "../serial/serial.h"
#define DbgPrint(x, ...) serial::Write(x, __VA_ARGS__)
#else
#define DbgPrint(x, ...) EMPTY_STMT
#endif

namespace x64
{
    void ConnectIsr(Isr isr, u8 irq)
    {
        // should maybe assert that irq < irq_count
        irq_handlers[irq] = isr;
        if (cpu_info.using_apic)
            apic::ConnectRedirEntry(irq);
    }

    static constexpr const char* exception_strings[32]{
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "Out of Bounds",
        "Invalid Opcode",
        "No Math Coprocessor",

        "Double Fault",
        "Coprocessor Segment Overrun",
        "Bad TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Unknown Interrupt",

        "Coprocessor Fault",
        "Alignment Check",
        "Machine Check",
        "SIMD Fault",
        "Assertion Failure",
        "Reserved 21",
        "Reserved 22",
        "Reserved 23",

        "Reserved 24",
        "Reserved 25",
        "Reserved 26",
        "Reserved 27",
        "Reserved 28",
        "Reserved 29",
        "Reserved 30",
        "Reserved 31"
    };

    EXTERN_C u64 spurious_irqs = 0;

    EXTERN_C void IsrCommon(InterruptFrame* frame, u8 int_no)
    {
        if (int_no < irq_base)
        {
            if (int_no == 14)
            {
                auto present = frame->error_code & 1 ? "present" : "not present";
                auto op = frame->error_code & 2 ? "write" : "read";
                auto ring = frame->error_code & 4 ? "ring 3" : "ring 0";
                Print(
                    "Page fault: 0x%llx (%s, %s, %s) at IP 0x%llx\n",
                    __readcr2(),
                    present,
                    op,
                    ring,
                    frame->rip
                );
            }
            else
            {
                Print(
                    "Interrupt %u (%llu) (%s) at IP 0x%llx\n",
                    int_no,
                    frame->error_code,
                    exception_strings[int_no],
                    frame->rip
                );
            }
        }
        else if (int_no < irq_base + irq_count)
        {
            u8 irq = int_no - irq_base;
            if (cpu_info.using_apic || pic::ConfirmIrq(irq))
            {
                if (irq == 0 && (timer::ticks % 10) == 0 && ke::schedule)
                {
                    auto prev = ke::GetCurrentThread();

                    if (ke::SelectNextThread())
                    {
                        auto next = ke::GetCurrentThread();

                        // Save old context
                        prev->context = *frame;

                        // Switch to new context
                        *frame = next->context;

                        DbgPrint(
                            "Thread switch\n"
                            "  From id %llu to id %llu\n"
                            "  RSP0: 0x%llx RSP3: 0x%llx\n"
                            "  Set new RSP to 0x%llx\n"
                            "  TSS0 RSP is 0x%llx\n",
                            prev->id, next->id,
                            next->context.rsp, next->user_stack,
                            frame->rsp,
                            ke::GetCore()->tss->rsp0
                        );
                    }
                }

                if (irq_handlers[irq])
                    irq_handlers[irq]();

                send_eoi(irq);
            }
            else // PIC and spurious
            {
                spurious_irqs++;
            }
        }
        else
        {
            Print("IsrCommon: Unexpected interrupt %u.\n", int_no);
            Halt();
        }
    }
}

namespace apic
{
    static u32 max_irq;

    void UpdateLvtEntry(LocalReg reg, u8 vector, Delivery type, bool enable)
    {
        if (vector < 16)
            return;

        LvtEntry entry;
        entry.bits = ReadLocal(reg);

        entry.disabled = !enable;
        entry.vector = vector;
        entry.delivery = type;
        /* "When the delivery mode is NMI, SMI, or INIT, the trigger mode is always edge sensitive."
           "Software should always set the trigger mode in the LVT LINT1 register to 0 (edge sensitive)."
           Intel SDM 3A, pg 376 */
        entry.trigger =
            (type == Delivery::Nmi || type == Delivery::Smi || type == Delivery::Init)
            || (reg == LocalReg::LINT1)
            ? Trigger::Edge : Trigger::Level;

        WriteLocal(reg, entry.bits);
    }

    static IoRedirectionEntry ReadRedirEntry(u32 gsi)
    {
        IoRedirectionEntry entry;

        u32 offset = ( u32 )IoReg::REDIR + (gsi * 2);
        entry.low32 = ReadIo(offset);
        entry.high32 = ReadIo(offset + 1);

        return entry;
    }

    static void WriteRedirEntry(u32 gsi, IoRedirectionEntry entry)
    {
        u32 offset = ( u32 )IoReg::REDIR + (gsi * 2);
        WriteIo(offset, entry.low32);
        WriteIo(offset + 1, entry.high32);
    }

    void ConnectRedirEntry(u8 irq, u8 apic_id, bool enable)
    {
        u32 gsi = irq;
        auto polarity = Polarity::ActiveLow;
        auto trigger = Trigger::Level;

        if (irq < int_src_overrides.size())
        {
            gsi = int_src_overrides[irq].gsi;
            auto flags = int_src_overrides[irq].flags;
            if ((flags & 0b11) == 0b01)
                polarity = Polarity::ActiveHigh;
            if ((flags & 0b1100) == 0b0100)
                trigger = Trigger::Edge;
        }

        auto entry = ReadRedirEntry(gsi);

        Print("irq %u gsi %u: 0x%x 0x%x\n", irq, gsi, entry.high32, entry.low32);

        u32 vector = irq + x64::irq_base;
        if (vector > 254)
            return;

        entry.disabled = !enable;
        entry.vector = vector;
        entry.trigger = trigger;
        entry.polarity = polarity;
        entry.delivery = Delivery::Fixed;
        entry.dst.physical.apic_id = apic_id & 0xf;

        Print("irq %u gsi %u: 0x%x 0x%x\n", irq, gsi, entry.high32, entry.low32);

        WriteRedirEntry(gsi, entry);
    }

    void SetRedirEntryState(u8 irq, bool enable)
    {
        auto gsi = irq < int_src_overrides.size() ? int_src_overrides[irq].gsi : irq;
        auto entry = ReadRedirEntry(gsi);

        entry.disabled = !enable;

        WriteRedirEntry(gsi, entry);
    }

    void InitializeController()
    {
        max_irq = EXTRACT32(ReadIo(IoReg::VER), 16, 24) + 1;

        // Mask all interrupts
        //
        // should already be masked?
        // for (u8 irq = 0; irq < max_irq; irq++)
        //     SetRedirEntryState(irq, OFF);

        // This also maps spurious interrupts to vector 255.
        MaskInterrupts();

        /*for (u32 i = 0; i < max_irq; i++)
        {
            auto reg = 16 + (2 * i);
            auto low = ReadIo(reg);
            auto high = ReadIo(reg + 1);
            Print("Entry %u 0x%x 0x%x\n", i, low, high);
        }*/

        // TODO - set error int vector

        Print("VER: 0x%x\n", ReadLocal(LocalReg::VER));
        Print("ESR: 0x%x\n", ReadLocal(LocalReg::ESR));
        Print("TPR: 0x%x\n", ReadLocal(LocalReg::TPR));
        Print("ERR_LVTR: 0x%x\n", ReadLocal(LocalReg::ERR_LVTR));

        u64 msr_apic = ReadMsr(x64::Msr::APIC_BASE);
        // Print("MSR_APIC_BASE: 0x%llx\n", msr_apic);

        if (!(msr_apic & APIC_GLOBAL_ENABLE))
        {
            Print("APIC was disabled in APIC_BASE MSR -- enabling now.\n");
            WriteMsr(x64::Msr::APIC_BASE, msr_apic | APIC_BSP | APIC_GLOBAL_ENABLE);
        }

        WriteLocal(LocalReg::TPR, 0);
    }
}

namespace pic
{
    static INLINE void Write(u16 port, u8 data)
    {
        WritePort8(port, data);
        x64::IoDelay();
    }

    static bool IsSpurious(u16 port)
    {
        OperationCmdWord ocw;
        ocw.bits = 0;
        ocw.ocw3.sbo = 1; // OCW3 (not 2)
        ocw.ocw3.read_type = 3; // ISR

        Write(port, ocw.bits);
        const InServiceRegister isr = ReadPort8(port);

        return !isr.irq7;
    }

    void InitializeController()
    {
        InitCmdWord icw;
        icw.bits = 0;

        // Start initialization sequence
        icw.icw1.icw4_needed = TRUE;
        icw.icw1.initialize = TRUE;
        Write(port::ctrl0, icw.bits);
        Write(port::ctrl1, icw.bits);

        // Remap IRQs 0-7 (indices 0-31 are reserved)
        icw.icw2.bits = x64::irq_base;
        Write(port::data0, icw.bits);
        icw.icw2.bits = x64::irq_base + 8;
        Write(port::data1, icw.bits);

        // Map slave to IRQ 2
        icw = 0;
        icw.icw3.master.slave_irq2 = TRUE;
        Write(port::data0, icw.bits);

        // Set slave ID to 2
        icw = 0;
        icw.icw3.slave.id = 2;
        Write(port::data1, icw.bits);

        // Enable 8086 mode
        icw = 0;
        icw.icw4.x86_mode = TRUE;
        Write(port::data0, icw.bits);
        Write(port::data1, icw.bits);

        // Keep interrupts disabled for now.
        // x86 layer will unmask when ISRs are in place
        // (and the APIC isn't available)
        MaskInterrupts();
    }

    void SetInterruptMask(u8 mask)
    {
        Write(port::data0, mask);
        Write(port::data1, mask);
    }

    bool ConfirmIrq(u8 irq)
    {
        if (irq == 7 && IsSpurious(port::ctrl0))
            return false;

        if (irq == 15 && IsSpurious(port::ctrl1))
        {
            OperationCmdWord ocw;
            ocw.bits = 0;
            ocw.ocw2.eoi_mode = 3; // Specific
            ocw.ocw2.irq = 2; // Cascade

            Write(port::ctrl0, ocw.bits);
            return false;
        }

        return true;
    }

    void SendEoi(u8 irq)
    {
        OperationCmdWord ocw;
        ocw.bits = 0;
        ocw.ocw2.eoi_mode = 1; // Non-specific

        if (irq >= 8)
            Write(port::ctrl1, ocw.bits);
        Write(port::ctrl0, ocw.bits);
    }
}
