#pragma once

#include <base.h>
#include <intrin.h>
#include <ec/enums.h>

#define ReadPort8 __inbyte
#define ReadPort16 __inword
#define ReadPort32 __indword
#define WritePort8 __outbyte
#define WritePort16 __outword
#define WritePort32 __outdword

namespace x64
{
    void Initialize();

    enum class Cr0 : u64
    {
        PE = 1 << 0,
        MP = 1 << 1,
        EM = 1 << 2,
        TS = 1 << 3,
        ET = 1 << 4,
        NE = 1 << 5,
        WP = 1 << 16,
        AM = 1 << 18,
        NW = 1 << 29,
        CD = 1 << 30,
        PG = 1 << 31,
    };
    GSTL_ENUM_BIT_OPS(Cr0)

    enum class Cr4 : u64
    {
        VME = 1 << 0,
        PVI = 1 << 1,
        TSD = 1 << 2,
        DE = 1 << 3,
        PSE = 1 << 4,
        PAE = 1 << 5,
        MCE = 1 << 6,
        PGE = 1 << 7,
        PCE = 1 << 8,
        OSFXSR = 1 << 9,
        OSXMMEXCPT = 1 << 10,
        UMIP = 1 << 11,
        LA57 = 1 << 12,
        VMXE = 1 << 13,
        SMXE = 1 << 14,
        FSGSBASE = 1 << 16,
        PCIDE = 1 << 17,
        OSXSAVE = 1 << 18,
        KL = 1 << 19,
        SMEP = 1 << 20,
        SMAP = 1 << 21,
        PKE = 1 << 22,
        CET = 1 << 23,
        PKS = 1 << 24,
        UINTR = 1 << 25,
    };
    GSTL_ENUM_BIT_OPS(Cr4)

    INLINE Cr0 ReadCr0()
    {
        return ( Cr0 )__readcr0();
    }

    INLINE void WriteCr0(Cr0 flags)
    {
        __writecr0(ec::to_underlying(flags));
    }

    INLINE Cr4 ReadCr4()
    {
        return ( Cr4 )__readcr4();
    }

    INLINE void WriteCr4(Cr4 flags)
    {
        __writecr4(ec::to_underlying(flags));
    }

    struct ProcessorInfo
    {
        char vendor_string[13];
        u8 stepping;
        u8 model;
        u8 family;
        u8 cores;
        union
        {
            struct
            {
                bool pic_present;
                bool using_apic; // PIC if 0
                bool tsc_supported;
                bool msr_supported;
                bool smap_supported;
            };
            u32 support_flags;
        };
    } inline cpu_info;

    struct InterruptFrame
    {
        u64 rax, rcx, rdx, rbx, rsi, rdi;
        u64 r8, r9, r10, r11, r12, r13, r14, r15;
        u64 fs, gs;
        u64 rbp;
        u64 error_code;
        u64 rip, cs, rflags, rsp, ss;
    };

#pragma pack(1)
    struct DescriptorTable
    {
        u16 limit;
        const void* base;

        constexpr DescriptorTable(const void* table, u16 table_limit)
            : limit(table_limit), base(table)
        {
        }
    };
    static_assert(sizeof DescriptorTable == 0xa);

    struct Tss
    {
        u32 reserved0;
        u64 rsp0;
        u64 rsp1;
        u64 rsp2;
        u64 reserved1;
        u64 ist1;
        u64 ist2;
        u64 ist3;
        u64 ist4;
        u64 ist5;
        u64 ist6;
        u64 ist7;
        u64 reserved2;
        u16 reserved3;
        u16 iomap_base;

        constexpr Tss(u64 ist1_addr, u64 ist2_addr, u64 ist3_addr, u64 ist4_addr)
            : iomap_base(sizeof Tss),
            ist1(ist1_addr),
            ist2(ist2_addr),
            ist3(ist3_addr),
            ist4(ist4_addr)
        {
        }
    };
    static_assert(sizeof Tss == 0x68);

    // ignored: base fields, segment limit, granularity/readable/accessed bits

/* S bit */
#define GDT_SYS_SEG     (0 << 4) // LDT/TSS/gates
#define GDT_USER_SEG    (1 << 4) // Data/code/stack

/* User segment type field */
#define GDT_READ        (1 << 1) // Use with code segments
#define GDT_WRITE       (1 << 1) // Use with data segments
#define GDT_EXECUTE     (1 << 3)
#define GDT_ENTRY_DATA  GDT_USER_SEG
#define GDT_ENTRY_CODE  GDT_USER_SEG | GDT_EXECUTE

/* System segment type field */
#define GDT_ENTRY_TSS   GDT_SYS_SEG | (1 << 0)
#define GDT_32BIT_TSS   (1 << 3)
#define GDT_AVAILABLE   (0 << 1)

    union GdtEntry
    {
        struct
        {
            u16 limit_low;
            u16 base_low;
            u8 base_mid;
            union
            {
                struct
                {
                    u8 type : 5;
                    u8 dpl : 2;
                    u8 present : 1;
                };
                u8 access_byte;
            };
            union
            {
                struct
                {
                    u8 limit_high : 4;
                    u8 available : 1;
                    u8 long_mode : 1;
                    u8 operation_size : 1;
                    u8 granularity : 1;
                };
                u8 granularity_byte;
            };
            u8 base_high;
        };
        u64 bits{};

        constexpr GdtEntry() = default;

        explicit constexpr GdtEntry(u8 dpl, u8 type)
            : limit_low(UINT16_MAX),
            base_low(0),
            base_mid(0),
            base_high(0)
        {
            this->type = type;
            this->dpl = dpl;
            this->present = TRUE;
            this->long_mode = TRUE;
        }

        static constexpr GdtEntry Null()
        {
            return GdtEntry();
        }

        static constexpr GdtEntry TssLow(Tss* tss)
        {
            GdtEntry entry{};

            entry.limit_low = sizeof Tss - 1;

            entry.base_low = EXTRACT64(tss, 0, 16);
            entry.base_mid = EXTRACT64(tss, 16, 24);
            entry.base_high = EXTRACT64(tss, 24, 32);

            entry.type = GDT_ENTRY_TSS | GDT_EXECUTE | GDT_AVAILABLE;
            entry.present = TRUE;

            return entry;
        }

        static constexpr GdtEntry TssHigh(Tss* tss)
        {
            GdtEntry entry{};
            entry.bits = HIGH32(tss);
            return entry;
        }
    };
    static_assert(sizeof GdtEntry == 0x8);

    namespace gdt_offset
    {
        consteval u16 get(u8 index) { return ( u16 )(sizeof GdtEntry * index); }
        static constexpr u16 null     = get(0);
        static constexpr u16 r0_code  = get(1);
        static constexpr u16 r0_data  = get(2);
        static constexpr u16 r3_code  = get(3);
        static constexpr u16 r3_data  = get(4);
        static constexpr u16 tss_low  = get(5);
        static constexpr u16 tss_high = get(6);
    };

#define INT_GATE 0xe
#define TRAP_GATE 0xf

    struct IdtEntry
    {
        u16 isr_low;
        u16 segment_selector;
        union
        {
            struct
            {
                u16 ist : 3;
                u16 reserved0 : 5;
                u16 gate : 4;
                u16 null : 1;
                u16 dpl : 2;
                u16 present : 1;
            };
            u16 flags;
        };
        u16 isr_mid;
        u32 isr_high;
        u32 reserved1;

        void Set(void* function, u32 dpl, u8 ist = 0)
        {
            this->isr_low = EXTRACT64(function, 0, 16);
            this->isr_mid = EXTRACT64(function, 16, 32);
            this->isr_high = HIGH32(function);

            this->segment_selector = gdt_offset::r0_code;

            this->reserved0 = this->reserved1 = this->null = 0;
            this->ist = ist;
            this->dpl = dpl;
            this->gate = INT_GATE;
            this->present = TRUE;
        }
    };
    static_assert(sizeof IdtEntry == 0x10);
#pragma pack()

    static constexpr u64 max_idt_entry = 256;

    enum IstIndex
    {
        // Index in TSS starts from 1
        DoubleFaultIst = 1,
        NmiIst,
        DebugIst,
        MachineCheckIst,

        IstCount = MachineCheckIst
    };

    static constexpr u64 ist_size = KiB(4);

    EXTERN_C_START

    /* Implemented in assembly (see cpu.asm) */

    void ReloadSegments(u16 code_selector, u16 data_selector);
    void LoadTr(u16 offset);

    EXTERN_C_END

    NO_RETURN INLINE void Halt()
    {
        _disable();
        while (1)
            __halt();
    }

    NO_RETURN INLINE void Idle()
    {
        _enable();
        while (1)
            __halt();
    }

    INLINE void Delay()
    {
        WritePort8(0x80, 0);
    }

    INLINE void DisableNmi()
    {
        WritePort8(0x70, ReadPort8(0x70) | 0x80);
        Delay();
        ReadPort8(0x71);
    }

    INLINE void EnableNmi()
    {
        WritePort8(0x70, ReadPort8(0x70) & 0x7f);
        Delay();
        ReadPort8(0x71);
    }

    INLINE void SmapClearAc()
    {
        if (cpu_info.smap_supported)
            _clac();
    }

    INLINE void SmapSetAc()
    {
        if (cpu_info.smap_supported)
            _stac();
    }
}
