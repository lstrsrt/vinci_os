#pragma once

#include <base.h>
#include <ec/const.h>
#include <ec/enums.h>
#include <intrin.h>

#include "../common/va.h"

#define ReadPort8 __inbyte
#define ReadPort16 __inword
#define ReadPort32 __indword
#define WritePort8 __outbyte
#define WritePort16 __outword
#define WritePort32 __outdword

namespace x64
{
    EARLY void Initialize(uptr_t kernel_stack);

    enum class RFLAGS : u64
    {
        CF = 1 << 0,                  // Carry Flag
        PF = 1 << 2,                  // Parity Flag
        AF = 1 << 4,                  // Auxiliary Carry Flag
        ZF = 1 << 6,                  // Zero Flag
        SF = 1 << 7,                  // Sign Flag
        TF = 1 << 8,                  // Trap Flag
        IF = 1 << 9,                  // Interrupt Flag
        DF = 1 << 10,                 // Direction Flag
        OF = 1 << 11,                 // Overflow Flag
        IOPL = (1 << 12) | (1 << 13), // I/O Privilege Level
        NT = 1 << 14,                 // Nested Task Flag
        MD = 1 << 15,                 // Mode Flag
        RF = 1 << 16,                 // Resume Flag
        VM = 1 << 17,                 // Virtual 8086 Mode
        AC = 1 << 18,                 // Alignment Check/SMAP Access Check
        VIF = 1 << 19,                // Virtual Interrupt Flag
        VIP = 1 << 20,                // Virtual Interrupt Pending
        ID = 1 << 21                  // CPUID Support
    };
    EC_ENUM_BIT_OPS(RFLAGS)

    enum class Cr0 : u64
    {
        PE = 1 << 0,   // Protected Mode
        MP = 1 << 1,   // Monitor co-processor
        EM = 1 << 2,   // Emulation
        TS = 1 << 3,   // Task switched
        ET = 1 << 4,   // Extension type
        NE = 1 << 5,   // Numeric error
        WP = 1 << 16,  // Write protect
        AM = 1 << 18,  // Alignment mask
        NW = 1 << 29,  // Not-write through
        CD = 1 << 30,  // Cache disable
        PG = 1 << 31,  // Paging
    };
    EC_ENUM_BIT_OPS(Cr0)

    enum class Cr4 : u64
    {
        VME = 1 << 0,          // Virtual 8086 Mode Extensions
        PVI = 1 << 1,          // Protected-mode Virtual Extensions
        TSD = 1 << 2,          // Time Stamp Disable
        DE = 1 << 3,           // Debugging Extensions
        PSE = 1 << 4,          // Page Size Extensions
        PAE = 1 << 5,          // Physical Address Extensions
        MCE = 1 << 6,          // Machine Check Exception
        PGE = 1 << 7,          // Page Global Enabled
        PCE = 1 << 8,          // Performance-Monitoring Counter
        OSFXSR = 1 << 9,       // FXSAVE and FXRSTOR
        OSXMMEXCPT = 1 << 10,  // SSE Exceptions
        UMIP = 1 << 11,        // User-Mode Instruction Prevention
        LA57 = 1 << 12,        // 57-Bit Linear Addresses
        VMXE = 1 << 13,        // Virtual Machine Extensions
        SMXE = 1 << 14,        // Safer Mode Extensions
        FSGSBASE = 1 << 16,    // {RD,WR}{FS,GS}BASE
        PCIDE = 1 << 17,       // Process Context Identifiers
        OSXSAVE = 1 << 18,     // XSAVE and Processor Extended States
        KL = 1 << 19,          // Key Locker
        SMEP = 1 << 20,        // Supervisor Mode Execution Protection
        SMAP = 1 << 21,        // Supervisor Mode Access Prevention
        PKE = 1 << 22,         // Supervisor Mode Access Prevention
        CET = 1 << 23,         // Control-flow Enforcement Technology
        PKS = 1 << 24,         // Protection Keys for Supervisor-Mode Pages
        UINTR = 1 << 25,       // User Interrupts
    };
    EC_ENUM_BIT_OPS(Cr4)

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
                bool apic_nmi_pin; // LINT0/LINT1
                bool pat_supported;
                bool tsc_supported;
                bool msr_supported;
                bool smap_supported;
            };
            u32 support_flags;
        };
    } inline cpu_info;

#pragma pack(1)
    struct Context
    {
        u64 rax, rcx, rdx, rbx, rsi, rdi;
        u64 r8, r9, r10, r11, r12, r13, r14, r15;
        u64 rbp, rsp;
        u64 rip;
        u64 rflags;
    };

    struct InterruptFrame
    {
        u64 rax, rcx, rdx, rbx, rsi, rdi;
        u64 r8, r9, r10, r11, r12, r13, r14, r15;
        u64 rbp;
        u64 error_code;
        u64 rip, cs, rflags, rsp, ss;
    };

    // TEMP
    // this goes into ke/
    struct Thread
    {
        Context ctx;
        u64 id;
        Thread* next;
    };

    static constexpr size_t threadcount = 2;
#pragma data_seg(".data")
    inline Thread* cur, * next;
    inline Thread threads[threadcount]{};
    inline bool schedule = false;
#pragma data_seg()

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

    alignas(page_size) inline volatile u8 int_stack_tables[IstCount][ist_size]{};

    alignas(64) static Tss kernel_tss(
        ( u64 )(int_stack_tables[0] + ist_size),
        ( u64 )(int_stack_tables[1] + ist_size),
        ( u64 )(int_stack_tables[2] + ist_size),
        ( u64 )(int_stack_tables[3] + ist_size)
    );

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

        explicit constexpr GdtEntry(u8 dpl, u8 type, bool long_mode = true)
            : limit_low(ec::umax_v<u16>),
            base_low(0),
            base_mid(0),
            base_high(0)
        {
            this->type = type;
            this->dpl = dpl;
            this->present = TRUE;
            this->granularity = TRUE;

            // Not set if this is a data segment
            this->long_mode = long_mode && type != GDT_ENTRY_DATA;

            // Opposite of LM
            this->operation_size = this->long_mode ? FALSE : TRUE;
        }

        static constexpr GdtEntry Null()
        {
            return GdtEntry();
        }

        static constexpr GdtEntry TssLow(const Tss* tss)
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

        static constexpr GdtEntry TssHigh(const Tss* tss)
        {
            GdtEntry entry{};
            entry.bits = HIGH32(tss);
            return entry;
        }
    };
    static_assert(sizeof GdtEntry == 0x8);

    enum class GdtIndex
    {
        Null,
        R0Code,
        R0Data,
        R3Code32,
        R3Data,
        R3Code,
        TssLow,
        TssHigh,
    };

    consteval u16 GetGdtOffset(GdtIndex index)
    {
        return ( u16 )(sizeof GdtEntry * ( u16 )index);
    }

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

            this->segment_selector = GetGdtOffset(GdtIndex::R0Code);

            this->reserved0 = this->reserved1 = this->null = 0;
            this->ist = ist;
            this->dpl = dpl;
            this->gate = INT_GATE;
            this->present = TRUE;
        }
    };
    static_assert(sizeof IdtEntry == 0x10);

#define TABLE_GDT 0
#define TABLE_LDT 1

    union SegmentSelector
    {
        struct
        {
            u16 rpl : 2;
            u16 table : 1;
            u16 index : 13;
        };
        u16 bits;
    };
    static_assert(sizeof SegmentSelector == sizeof u16);
#pragma pack()

    static constexpr u64 max_idt_entry = 256;

    struct Core
    {
        uptr_t kernel_stack;
        uptr_t user_stack;
    };

    inline Core core{}; // This goes into GS.

    struct SyscallFrame
    {
        u64 rflags;
        u64 rsi, rdi;
        u64 rax, rdx, rcx;
        u64 r8, r9, r10, r11;
    };

    EXTERN_C_START

    /* Implemented in assembly (see cpu.asm) */
    void ReloadSegments(u16 code_selector, u16 data_selector);
    void LoadTr(u16 offset);
    void Ring3Function();
    void EnterUserMode(uptr_t code, uptr_t stack);
    void x64Syscall();

    // C++ handler
    u64 x64SyscallCxx(SyscallFrame* frame, u64 sys_no);

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

    INLINE void TlbFlush()
    {
        // Reloading CR3 invalidates all non-global TLB entries
        __writecr3(__readcr3());
    }

    INLINE void TlbFlushAddress(void* addr)
    {
        __invlpg(addr);
        // TODO - make MP compatible with APIC IPIs
    }
}
