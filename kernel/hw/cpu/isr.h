#pragma once

#include <base.h>
#include <ec/array.h>

#include "../core/gfx/output.h"

namespace apic
{
    enum class LocalReg
    {
        ID = 0x20, /* Local APIC ID Register (R/W) */
        VER = 0x30, /* Local APIC Version Register (R) */
        TPR = 0x80, /* Task Priority Register (R/W) */
        APR = 0x90, /* Arbitration Priority Register (R) */
        PPR = 0xa0, /* Processor Priority Register (R) */
        EOI = 0xb0, /* EOI Register (W) */
        RRR = 0xc0, /* Remote Read Register () */
        LDR = 0xd0, /* Logical Destination Register (R/W) */
        DFR = 0xe0, /* Destination Format Register (0-27 R, 28-31 R/W) */
        SIVR = 0xf0, /* Spurious Interrupt Vector Register (0-3 R, 4-9 R/W) */
        ISR = 0x100, /* Interrupt Service Register 0-255 (R) */
        TMR = 0x180, /* Trigger Mode Register 0-255 (R) */
        IRR = 0x200, /* Interrupt Request Register 0-255 (r) */
        ESR = 0x280, /* Error Status Register (R) */
        ICR0 = 0x300, /* Interrupt Command Register 0-31 (R/W) */
        ICR1 = 0x310, /* Interrupt Command Register 32-63 (R/W) */
        TMR_LVTR = 0x320, /* Timer Local Vector Table (R/W) */
        THRM_LVTR = 0x330, /* Thermal Local Vector Table */
        PC_LVTR = 0x340, /* Performance Counter Local Vector Table (R/W) */
        LINT0 = 0x350, /* LINT0 Local Vector Table (R/W) */
        LINT1 = 0x360, /* LINT1 Local Vector Table (R/W) */
        ERR_LVTR = 0x370, /* Error Local Vector Table (R/W) */
        TICR = 0x380, /* Initial Count Register for Timer (R/W) */
        TCCR = 0x390, /* Current Count Register for Timer (R) */
        TDCR = 0x3e0, /* Timer Divide Configuration Register (R/W) */
        EAFR = 0x400, /* Extended APIC Feature register (R/W) */
        EACR = 0x410, /* Extended APIC Control Register (R/W) */
        SEOI = 0x420, /* Specific End Of Interrupt Register (W) */
        EXT0_LVTR = 0x500, /* Extended Interrupt 0 Local Vector Table */
        EXT1_LVTR = 0x510, /* Extended Interrupt 1 Local Vector Table */
        EXT2_LVTR = 0x520, /* Extended Interrupt 2 Local Vector Table */
        EXT3_LVTR = 0x530  /* Extended Interrupt 3 Local Vector Table */
    };

    enum class IoReg
    {
        ID = 0x0, /* IOAPIC ID */
        VER = 0x1, /* IOAPIC Version */
        ARB = 0x2, /* IOAPIC Arbitration ID */
        REDIR = 0x10 /* Redirection Table (0-23, 64 bits each) */
    };

    enum class SivrFlag
    {
        ApicEnable = 1 << 8
    };

    enum class Delivery : u32
    {
        Fixed,
        LowPri,
        Smi,
        Remote,
        Nmi,
        Init,
        Startup,
        ExtInt
    };

    enum class Polarity : u32
    {
        ActiveHigh,
        ActiveLow
    };

    enum class Trigger : u32
    {
        Edge,
        Level
    };

#pragma pack(1)
    union IoRedirectionEntry
    {
        struct
        {
            u32 vector : 8; /* Allowed: 16 to 254 */
            Delivery delivery : 3;
            u32 logical : 1;
            u32 pending : 1;
            Polarity polarity : 1;
            u32 remote_irr : 1;
            Trigger trigger : 1;
            u32 disabled : 1;
            u32 reserved : 15;
            union
            {
                struct
                {
                    u32 reserved0 : 24;
                    u32 apic_id : 4;
                    u32 reserved1 : 4;
                } physical;
                struct
                {
                    u32 reserved : 24;
                    u32 apic_id : 8;
                } logical;
            } dst;
        };
        struct
        {
            u32 low32;
            u32 high32;
        };
        u64 bits;
    };
    static_assert(sizeof IoRedirectionEntry == 0x8);

    union LvtEntry
    {
        struct
        {
            u32 vector : 8; /* Allowed: 16 to 254 */
            Delivery delivery : 3;
            u32 reserved0 : 1;
            u32 pending : 1;
            Polarity polarity : 1;
            u32 irr : 1;
            Trigger trigger : 1;
            u32 disabled : 1;
            u32 timer_mode : 2;
            u32 reserved1 : 13;
        };
        u32 bits;
    };
    static_assert(sizeof LvtEntry == 0x4);
#pragma pack()

    inline u64 local;
    inline u64 io;

    inline ec::array<acpi::IntSrcOverride, 16> int_src_overrides{
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    constexpr u8 spurious_int_vec = 255;

    INLINE u32 ReadLocal(LocalReg reg)
    {
        return *( u32 volatile* )(local + ( u32 )reg);
    }

    INLINE void WriteLocal(LocalReg reg, u32 data)
    {
        *( u32 volatile* )(local + ( u32 )reg) = data;
    }

    INLINE u32 ReadIo(u32 reg)
    {
        auto io_reg_sel = ( u32 volatile* )io;
        *io_reg_sel = reg & 0xff;
        auto io_win = ( u32 volatile* )(io + 16);
        return *io_win;
    }

    INLINE u32 ReadIo(IoReg reg)
    {
        return ReadIo(( u32 )reg);
    }

    INLINE void WriteIo(u32 reg, u32 data)
    {
        auto io_reg_sel = ( u32 volatile* )io;
        *io_reg_sel = reg & 0xff;
        auto io_win = ( u32 volatile* )(io + 16);
        *io_win = data;
    }

    INLINE void WriteIo(IoReg reg, u32 data)
    {
        return WriteIo(( u32 )reg, data);
    }

    INLINE void SendEoi(UNUSED u8)
    {
        WriteLocal(LocalReg::EOI, 0);
    }

    INLINE void MaskInterrupts()
    {
        WriteLocal(LocalReg::SIVR, spurious_int_vec);
    }

    INLINE void UnmaskInterrupts()
    {
        WriteLocal(LocalReg::SIVR, ( u32 )SivrFlag::ApicEnable | spurious_int_vec);
    }

    // TODO - apic timer update function

    void UpdateLvtEntry(LocalReg reg, u8 vector, Delivery type, bool enable);

    void ConnectRedirEntry(u8 irq, u8 apic_id = ReadLocal(LocalReg::ID), bool enable = true);
    void SetRedirEntryState(u8 irq, bool enable);

    void InitializeController();
}

namespace pic
{
    namespace port
    {
        constexpr u16 ctrl0 = 0x20;
        constexpr u16 data0 = 0x21;
        constexpr u16 ctrl1 = 0xa0;
        constexpr u16 data1 = 0xa1;
    };

    struct InitCmdWord
    {
        union
        {
            struct InitCmdWord1
            {
                u8 icw4_needed : 1;
                u8 single_mode : 1;
                u8 call_interval : 1;
                u8 irq_mode : 1;
                u8 initialize : 1;
                u8 isr_addr : 3;
            } icw1;
            struct InitCmdWord2
            {
                u8 bits;
            } icw2;
            struct InitCmdWord3
            {
                union
                {
                    struct MasterCmdWord
                    {
                        u8 slave_irq0 : 1;
                        u8 slave_irq1 : 1;
                        u8 slave_irq2 : 1;
                        u8 slave_irq3 : 1;
                        u8 slave_irq4 : 1;
                        u8 slave_irq5 : 1;
                        u8 slave_irq6 : 1;
                        u8 slave_irq7 : 1;
                    } master;
                    struct SlaveCmdWord
                    {
                        u8 id : 3;
                        u8 reserved : 5;
                    } slave;
                    u8 bits;
                };
            } icw3;
            struct InitCmdWord4
            {
                u8 x86_mode : 1;
                u8 eoi_mode : 1;
                u8 buffered : 1;
                u8 special_fully_nested_mode : 1;
                u8 reserved : 3;
            } icw4;
            u8 bits;
        };

        void operator=(u8 bits)
        {
            this->bits = bits;
        }
    };
    static_assert(sizeof InitCmdWord == 0x1);

    struct OperationCmdWord
    {
        union
        {
            struct OperationCmdWord2
            {
                u8 irq : 3;
                u8 sbz : 2;
                u8 eoi_mode : 3;
            } ocw2;
            struct OperationCmdWord3
            {
                u8 read_type : 2;
                u8 poll_cmd : 1;
                u8 sbo : 1;
                u8 sbz : 1;
                u8 special_mask_mode : 2;
                u8 reserved : 1;
            } ocw3;
            u8 bits;
        };
    };
    static_assert(sizeof OperationCmdWord == 0x1);

    struct InServiceRegister
    {
        union
        {
            struct
            {
                u8 irq0 : 1;
                u8 irq1 : 1;
                u8 irq2 : 1;
                u8 irq3 : 1;
                u8 irq4 : 1;
                u8 irq5 : 1;
                u8 irq6 : 1;
                u8 irq7 : 1;
            };
            u8 bits;
        };

        InServiceRegister(u8 bits)
            : bits(bits)
        {
        }
    };
    static_assert(sizeof InServiceRegister == 0x1);

    void InitializeController();

    void SetInterruptMask(u8 mask);

    INLINE void MaskInterrupts()
    {
        SetInterruptMask(0xff);
    }

    INLINE void UnmaskInterrupts()
    {
        SetInterruptMask(0x00);
    }

    bool ConfirmIrq(u8 irq);
    void SendEoi(u8 irq);
}

namespace x64
{
    static constexpr u8 irq_base = 32;
    static constexpr u8 irq_count = 16;

    using Isr = void(__cdecl*)();
    inline Isr irq_handlers[irq_count];

    void ConnectIsr(Isr isr, u8 irq);

    inline void (*mask_interrupts)();
    inline void (*unmask_interrupts)();
    inline void (*send_eoi)(u8);

    EXTERN_C_START

    /* Generic ISRs */
    void IsrCommon(InterruptFrame* frame, u8 int_no);
    void _IsrSpurious();
    void _IsrUnexpected();

    /* Autogenerated entry points (see isr.asm) */
    void _Isr0();
    void _Isr1();
    void _Isr2();
    void _Isr3();
    void _Isr4();
    void _Isr5();
    void _Isr6();
    void _Isr7();
    void _Isr8();
    void _Isr9();
    void _Isr10();
    void _Isr11();
    void _Isr12();
    void _Isr13();
    void _Isr14();
    void _Isr15();
    void _Isr16();
    void _Isr17();
    void _Isr18();
    void _Isr19();
    void _Isr20();
    void _Isr21();
    void _Isr22();
    void _Isr23();
    void _Isr24();
    void _Isr25();
    void _Isr26();
    void _Isr27();
    void _Isr28();
    void _Isr29();
    void _Isr30();
    void _Isr31();

    void _Isr32();
    void _Isr33();
    void _Isr34();
    void _Isr35();
    void _Isr36();
    void _Isr37();
    void _Isr38();
    void _Isr39();
    void _Isr40();
    void _Isr41();
    void _Isr42();
    void _Isr43();
    void _Isr44();
    void _Isr45();
    void _Isr46();
    void _Isr47();

    EXTERN_C_END
}
