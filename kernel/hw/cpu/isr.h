#pragma once

#include <base.h>
#include <ec/array.h>

#include "x64.h"
#include "../gfx/output.h"

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
    static_assert(sizeof(IoRedirectionEntry) == 0x8);

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
    static_assert(sizeof(LvtEntry) == 0x4);
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
            struct
            {
                u8 icw4_needed : 1;
                u8 single_mode : 1;
                u8 call_interval : 1;
                u8 irq_mode : 1;
                u8 initialize : 1;
                u8 isr_addr : 3;
            } icw1;
            struct
            {
                u8 bits;
            } icw2;
            struct
            {
                union
                {
                    struct
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
                    struct
                    {
                        u8 id : 3;
                        u8 reserved : 5;
                    } slave;
                    u8 bits;
                };
            } icw3;
            struct
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
    static_assert(sizeof(InitCmdWord) == 0x1);

    struct OperationCmdWord
    {
        union
        {
            struct
            {
                u8 irq : 3;
                u8 sbz : 2;
                u8 eoi_mode : 3;
            } ocw2;
            struct
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
    static_assert(sizeof(OperationCmdWord) == 0x1);

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
    static_assert(sizeof(InServiceRegister) == 0x1);

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

    inline Isr irq_handlers[irq_count];

    void ConnectIsr(Isr isr, u8 irq);

    inline void (*mask_interrupts)();
    inline void (*unmask_interrupts)();
    inline void (*send_eoi)(u8);

    EXTERN_C_START

    /* Generic ISRs */
    void IsrCommon(InterruptFrame* frame, u8 int_no);
    void _IsrSpurious();

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
    void _Isr47();
    void _Isr48();
    void _Isr49();
    void _Isr50();
    void _Isr51();
    void _Isr52();
    void _Isr53();
    void _Isr54();
    void _Isr55();
    void _Isr56();
    void _Isr57();
    void _Isr58();
    void _Isr59();
    void _Isr60();
    void _Isr61();
    void _Isr62();
    void _Isr63();
    void _Isr64();
    void _Isr65();
    void _Isr66();
    void _Isr67();
    void _Isr68();
    void _Isr69();
    void _Isr70();
    void _Isr71();
    void _Isr72();
    void _Isr73();
    void _Isr74();
    void _Isr75();
    void _Isr76();
    void _Isr77();
    void _Isr78();
    void _Isr79();
    void _Isr80();
    void _Isr81();
    void _Isr82();
    void _Isr83();
    void _Isr84();
    void _Isr85();
    void _Isr86();
    void _Isr87();
    void _Isr88();
    void _Isr89();
    void _Isr90();
    void _Isr91();
    void _Isr92();
    void _Isr93();
    void _Isr94();
    void _Isr95();
    void _Isr96();
    void _Isr97();
    void _Isr98();
    void _Isr99();
    void _Isr100();
    void _Isr101();
    void _Isr102();
    void _Isr103();
    void _Isr104();
    void _Isr105();
    void _Isr106();
    void _Isr107();
    void _Isr108();
    void _Isr109();
    void _Isr110();
    void _Isr111();
    void _Isr112();
    void _Isr113();
    void _Isr114();
    void _Isr115();
    void _Isr116();
    void _Isr117();
    void _Isr118();
    void _Isr119();
    void _Isr120();
    void _Isr121();
    void _Isr122();
    void _Isr123();
    void _Isr124();
    void _Isr125();
    void _Isr126();
    void _Isr127();
    void _Isr128();
    void _Isr129();
    void _Isr130();
    void _Isr131();
    void _Isr132();
    void _Isr133();
    void _Isr134();
    void _Isr135();
    void _Isr136();
    void _Isr137();
    void _Isr138();
    void _Isr139();
    void _Isr140();
    void _Isr141();
    void _Isr142();
    void _Isr143();
    void _Isr144();
    void _Isr145();
    void _Isr146();
    void _Isr147();
    void _Isr148();
    void _Isr149();
    void _Isr150();
    void _Isr151();
    void _Isr152();
    void _Isr153();
    void _Isr154();
    void _Isr155();
    void _Isr156();
    void _Isr157();
    void _Isr158();
    void _Isr159();
    void _Isr160();
    void _Isr161();
    void _Isr162();
    void _Isr163();
    void _Isr164();
    void _Isr165();
    void _Isr166();
    void _Isr167();
    void _Isr168();
    void _Isr169();
    void _Isr170();
    void _Isr171();
    void _Isr172();
    void _Isr173();
    void _Isr174();
    void _Isr175();
    void _Isr176();
    void _Isr177();
    void _Isr178();
    void _Isr179();
    void _Isr180();
    void _Isr181();
    void _Isr182();
    void _Isr183();
    void _Isr184();
    void _Isr185();
    void _Isr186();
    void _Isr187();
    void _Isr188();
    void _Isr189();
    void _Isr190();
    void _Isr191();
    void _Isr192();
    void _Isr193();
    void _Isr194();
    void _Isr195();
    void _Isr196();
    void _Isr197();
    void _Isr198();
    void _Isr199();
    void _Isr200();
    void _Isr201();
    void _Isr202();
    void _Isr203();
    void _Isr204();
    void _Isr205();
    void _Isr206();
    void _Isr207();
    void _Isr208();
    void _Isr209();
    void _Isr210();
    void _Isr211();
    void _Isr212();
    void _Isr213();
    void _Isr214();
    void _Isr215();
    void _Isr216();
    void _Isr217();
    void _Isr218();
    void _Isr219();
    void _Isr220();
    void _Isr221();
    void _Isr222();
    void _Isr223();
    void _Isr224();
    void _Isr225();
    void _Isr226();
    void _Isr227();
    void _Isr228();
    void _Isr229();
    void _Isr230();
    void _Isr231();
    void _Isr232();
    void _Isr233();
    void _Isr234();
    void _Isr235();
    void _Isr236();
    void _Isr237();
    void _Isr238();
    void _Isr239();
    void _Isr240();
    void _Isr241();
    void _Isr242();
    void _Isr243();
    void _Isr244();
    void _Isr245();
    void _Isr246();
    void _Isr247();
    void _Isr248();
    void _Isr249();
    void _Isr250();
    void _Isr251();
    void _Isr252();
    void _Isr253();
    void _Isr254();
    void _Isr255();

    EXTERN_C_END
}
