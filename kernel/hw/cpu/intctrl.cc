#include <ec/const.h>

#include "x64.h"
#include "isr.h"
#include "../serial/serial.h"
#include "../../core/gfx/output.h"

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

    EXTERN_C void IsrCommon(InterruptFrame* frame, u8 int_no)
    {
        if (int_no < irq_base)
        {
            if (int_no == 14)
            {
                auto present = frame->error_code & 1 ? "present" : "not present";
                auto op = frame->error_code & 2 ? "write" : "read";
                auto ring = frame->error_code & 4 ? "ring 3" : "ring 0";
                auto virt = __readcr2();
                serial::Write("Page fault @ 0x%llx (%s, %s, %s)\n", virt, present, op, ring);
            }
            else
            {
                Print("IsrCommon: Received interrupt %d (%s)\n", int_no, exception_strings[int_no]);
            }
        }
        else if (int_no < irq_base + irq_count)
        {
            u8 irq = int_no - irq_base;
            if (cpu_info.using_apic || pic::ConfirmIrq(irq))
            {
                if (irq_handlers[irq])
                    irq_handlers[irq]();
                send_eoi(irq);
            }
        }
        else
        {
            // IsrUnexpected does not keep the int_no so we can't print it
            Print("IsrCommon: Unexpected interrupt.\n");
        }
    }
}

// TODO - msr header with enums
#define MSR_APIC_BASE 0x1b
#define APIC_GLOBAL_ENABLE_FLAG (1 << 11)

namespace apic
{
    static u8 connect_cnt;

    u32 ReadLocal(LocalReg reg)
    {
        return *( volatile u32* )(local + ( u32 )reg);
    }

    void WriteLocal(LocalReg reg, u32 data)
    {
        *( volatile u32* )(local + ( u32 )reg) = data;
    }

    u32 ReadIo(u32 reg)
    {
        auto i = ( volatile u32* )io;
        *i = reg & 0xff;
        return *(i + 4);
    }

    void WriteIo(u32 reg, u32 data)
    {
        auto i = ( volatile u32* )io;
        *i = reg & 0xff;
        *(i + 4) = data;
    }

    void UpdateLvtEntry(LocalReg reg, u8 vector, Delivery type, bool enable)
    {
        LvtEntry entry;
        entry.bits = ReadLocal(reg);

        entry.disabled = !enable;
        entry.vector = vector;
        entry.delivery = type;
        /* "When the delivery mode is NMI, SMI, or INIT, the trigger mode is always edge sensitive."
            Intel SDM 3A, pg 376 */
        entry.trigger = (
            type == Delivery::Nmi
            || type == Delivery::Smi
            || type == Delivery::Init
            ) ? Trigger::Edge : Trigger::Level;

        WriteLocal(reg, entry.bits);
    }

    static u32 EntryRegFromIrq(u8 irq)
    {
        u32 entry = irq_gsi_overrides[irq];
        u32 entry_reg = (entry * 2) + 16; // 2 ports per entry, so multiply by 2
        if (entry_reg > 63) // Min is 16, max is 63
            return ec::u32_max;
        return entry_reg;
    }

    static IoRedirectionEntry ReadRedirEntry(u32 entry_reg)
    {
        IoRedirectionEntry entry;
        entry.low32 = ReadIo(entry_reg);
        entry.high32 = ReadIo(entry_reg + 1);
        return entry;
    }

    static void WriteRedirEntry(u32 entry_reg, IoRedirectionEntry entry)
    {
        WriteIo(entry_reg, entry.low32);
        WriteIo(entry_reg + 1, entry.high32);
    }

    void ConnectRedirEntry(u8 irq, u8 apic_id, bool enable)
    {
        u8 vector = irq + x64::irq_base;
        if (vector < 16 || vector > 254)
            return;

        auto entry_reg = EntryRegFromIrq(irq);
        if (entry_reg == ec::u32_max)
            return;

        auto entry = ReadRedirEntry(entry_reg);

        entry.disabled = !enable;
        entry.vector = vector;
        entry.dst.physical.apic_id = apic_id & 0xf;

        WriteRedirEntry(entry_reg, entry);
        connect_cnt++;
    }

    void SetRedirEntryState(u8 irq, bool enable)
    {
        auto entry_reg = EntryRegFromIrq(irq);
        auto entry = ReadRedirEntry(entry_reg);

        entry.disabled = !enable;

        WriteRedirEntry(entry_reg, entry);
    }

    void InitializeController()
    {
        // Mask all interrupts
        for (u8 irq = 16; irq < 64; irq++)
            SetRedirEntryState(irq, false);

        // Map spurious interrupts to vector 255.
        // Do not set the enable flag yet
        WriteLocal(LocalReg::SIVR, spurious_int_vec);

        u64 msr_apic = __readmsr(MSR_APIC_BASE);
        if (!(msr_apic & APIC_GLOBAL_ENABLE_FLAG))
        {
            Print("APIC was disabled in APIC_BASE MSR -- enabling now.\n");
            __writemsr(MSR_APIC_BASE, msr_apic | APIC_GLOBAL_ENABLE_FLAG);
        }
    }
}

namespace pic
{
    static INLINE void Write(u16 port, u8 data)
    {
        WritePort8(port, data);
        x64::Delay();
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
