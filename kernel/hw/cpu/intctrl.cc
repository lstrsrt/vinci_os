#include <ec/const.h>
#include <libc/mem.h>

#include "../../core/ke.h"
#include "../timer/timer.h"
#include "isr.h"
#include "msr.h"
#include "x64.h"

// #define DEBUG_CTX_SWITCH

#ifdef DEBUG_CTX_SWITCH
#define DbgPrint(x, ...) gfx::Print(x, __VA_ARGS__)
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

    // clang-format off
    static constexpr ec::array exception_strings{
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
        "Control Protection Exception",
        "Reserved 22",
        "Reserved 23",

        "Reserved 24",
        "Reserved 25",
        "Reserved 26",
        "Reserved 27",
        "Hypervisor Injection Exception",
        "VMM Communication Exception",
        "Security Exception",
        "Reserved 31"
    };
    // clang-format on

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
                Print("Page fault: 0x%p (%s, %s, %s) at IP 0x%p\n", __readcr2(), present, op, ring, frame->rip);
            }
            else
            {
                Print("Interrupt %u (0x%llx) (%s) at IP 0x%p\n", int_no, frame->error_code, exception_strings[int_no],
                    frame->rip);
            }

            if ((frame->cs & 3) && int_no != 2 && int_no != 18)
            {
                Print("exception came from user mode\n");
                if (int_no == 3)
                    Print("breakpoints are currently unsupported\n");
                ke::ExitThread(1);
            }

            // Returning from a fault re-runs the faulting instruction forever.
            // Only debug, NMI and breakpoint can safely continue.
            if (int_no != 1 && int_no != 2 && int_no != 3)
                Halt();
        }
        else if (int_no < irq_base + irq_count)
        {
            u8 irq = int_no - irq_base;
            if (cpu_info.using_apic || pic::ConfirmIrq(irq))
            {
                if (irq == 0 && (timer::ticks % 10) == 0 && ke::schedule)
                {
                    auto prev = ke::GetCurrentThread();
                    bool int_state;

                    if (ke::SelectNextThread(&int_state))
                    {
                        auto next = ke::GetCurrentThread();

                        // Save old context
                        prev->context = *frame;

                        // Switch to new context
                        *frame = next->context;

                        DbgPrint("Thread switch\n"
                                 "  From id %llu to id %llu\n"
                                 "  RSP: 0x%p KSTACK: 0x%p\n"
                                 "  Set new RSP to 0x%p\n"
                                 "  TSS0 RSP is 0x%p\n",
                            prev->id, next->id, next->context.rsp, next->kernel_stack_top, frame->rsp,
                            ke::GetCore()->tss->rsp0);
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
        else if (cpu_info.using_apic && int_no == apic::error_vec)
        {
            using namespace apic;
            WriteLocal(LocalReg::ESR, 0);
            u32 esr = ReadLocal(LocalReg::ESR);
            Print("APIC error: ESR 0x%x\n", esr);
            SendEoi(0);
        }
        else
        {
            Print("IsrCommon: Unexpected interrupt %u.\n", int_no);
            Halt();
        }
    }
} // namespace x64

namespace apic
{
    static u32 redir_entry_count;

    void UpdateLvtEntry(LocalReg reg, u8 vector, Delivery type, bool enable)
    {
        if (vector < 16 && type != Delivery::Nmi)
            return;

        LvtEntry entry;
        entry.bits = ReadLocal(reg);

        entry.disabled = !enable;
        entry.vector = vector;
        entry.delivery = type;
        /* "When the delivery mode is NMI, SMI, or INIT, the trigger mode is always edge sensitive."
           "Software should always set the trigger mode in the LVT LINT1 register to 0 (edge sensitive)."
           Intel SDM 3A, Local Vector Table */
        if (reg == LocalReg::LINT0 || reg == LocalReg::LINT1)
        {
            entry.trigger
                = (type == Delivery::Nmi || type == Delivery::Smi || type == Delivery::Init) || (reg == LocalReg::LINT1)
                      ? Trigger::Edge
                      : Trigger::Level;
        }

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
        WriteIo(offset + 1, entry.high32);
        WriteIo(offset, entry.low32);
    }

    void ConnectRedirEntry(u8 irq, u32 apic_id, bool enable)
    {
        u32 gsi = irq;
        auto polarity = Polarity::ActiveHigh;
        auto trigger = Trigger::Edge;

        if (irq < int_src_overrides.size())
        {
            gsi = int_src_overrides[irq].gsi;
            auto flags = int_src_overrides[irq].flags;
            if ((flags & 0b11) == 0b11)
                polarity = Polarity::ActiveLow;
            if ((flags & 0b1100) == 0b1100)
                trigger = Trigger::Level;
        }
        if (gsi >= redir_entry_count)
            return;

        auto entry = ReadRedirEntry(gsi);
        u32 vector = irq + x64::irq_base;
        if (vector > 254)
            return;

        entry.disabled = !enable;
        entry.vector = vector;
        entry.trigger = trigger;
        entry.polarity = polarity;
        entry.delivery = Delivery::Fixed;
        entry.dst_apic_id = apic_id;

        Print("irq %u gsi %u: 0x%x 0x%x\n", irq, gsi, entry.high32, entry.low32);

        WriteRedirEntry(gsi, entry);
    }

    void DumpRedirEntries()
    {
        for (u32 gsi = 0; gsi < redir_entry_count; gsi++)
        {
            auto redir_entry = ReadRedirEntry(gsi);
            Print("Entry %u 0x%x 0x%x\n", gsi, redir_entry.low32, redir_entry.high32);
        }
    }

    void SetRedirEntryState(u8 irq, bool enable)
    {
        auto gsi = irq < int_src_overrides.size() ? int_src_overrides[irq].gsi : irq;
        if (gsi >= redir_entry_count)
            return;

        auto entry = ReadRedirEntry(gsi);

        entry.disabled = !enable;

        WriteRedirEntry(gsi, entry);
    }

    void InitializeController()
    {
        using enum x64::MsrApicBase;
        auto msr_apic = ( x64::MsrApicBase )ReadMsr(x64::Msr::APIC_BASE);
        Print("MSR_APIC_BASE: 0x%llx\n", msr_apic);

        if (!!(msr_apic & EXTD))
        {
            Print("x2APIC enabled in APIC_BASE MSR -- disabling now.\n");
            msr_apic &= ~(EXTD | GLOBAL_ENABLE);
            WriteMsr(x64::Msr::APIC_BASE, msr_apic);
        }

        if (!(msr_apic & GLOBAL_ENABLE))
        {
            Print("APIC was disabled in APIC_BASE MSR -- enabling now.\n");
            msr_apic |= BSP | GLOBAL_ENABLE;
            WriteMsr(x64::Msr::APIC_BASE, msr_apic);
        }

        // Enable APIC in SIVR so we can set LVT entries.
        // Interrupts are still off after this call since IF=0 and IO APIC entries are masked.
        WriteLocal(LocalReg::SIVR, ( u32 )SivrFlag::APIC_ENABLE | spurious_int_vec);

        // Clear stale errors
        WriteLocal(LocalReg::ESR, 0);
        Print("VER: 0x%x ESR: 0x%x TPR: 0x%x\n", ReadLocal(LocalReg::VER), ReadLocal(LocalReg::ESR),
            ReadLocal(LocalReg::TPR));
        WriteLocal(LocalReg::ERR_LVTR, error_vec);
        Print("ERR_LVTR: 0x%x\n", ReadLocal(LocalReg::ERR_LVTR));
        for (u32 tries = 0; tries < 256; ++tries)
        {
            bool any = false;
            for (u32 i = 0; i < 8; ++i)
                any |= ReadLocal(( LocalReg )(( u32 )LocalReg::ISR + i * 0x10)) != 0;
            if (!any)
                break;
            SendEoi(0);
        }

        // Configure LINT pins
        WriteLocal(LocalReg::LINT0, 1 << 16 /* mask */);
        UpdateLvtEntry(LocalReg::LINT1, 2, Delivery::Nmi, ON);

        redir_entry_count = EXTRACT32(ReadIo(IoReg::VER), 16, 24) + 1;

        // Mask other LVT entries
        const u32 max_lvt = EXTRACT32(ReadLocal(LocalReg::VER), 16, 24);
        WriteLocal(LocalReg::TMR_LVTR, ( u32 )LvtFlag::MASK);
        if (max_lvt >= 4)
            WriteLocal(LocalReg::PC_LVTR, ( u32 )LvtFlag::MASK);
        if (max_lvt >= 5)
            WriteLocal(LocalReg::THRM_LVTR, ( u32 )LvtFlag::MASK);
        if (max_lvt >= 6)
            WriteLocal(LocalReg::CMCI, ( u32 )LvtFlag::MASK);

        // Mask all interrupts
        for (u32 gsi = 0; gsi < redir_entry_count; gsi++)
        {
            auto redir_entry = ReadRedirEntry(gsi);
            redir_entry.disabled = true;
            WriteRedirEntry(gsi, redir_entry);
        }

        MaskInterrupts();
    }
} // namespace apic

namespace pic
{
    static INLINE void Write(u16 port, u8 data)
    {
        x64::WritePort8(port, data);
    }

    static bool IsSpurious(u16 port)
    {
        OperationCmdWord ocw;
        ocw.bits = 0;
        ocw.ocw3.sbo = 1;       // OCW3 (not 2)
        ocw.ocw3.read_type = 3; // ISR

        Write(port, ocw.bits);
        const InServiceRegister isr = x64::ReadPort8(port);

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
            ocw.ocw2.irq = 2;      // Cascade

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
} // namespace pic
