#pragma once

#include <base.h>

#include "x64.h"

#define ReadMsr(msr) __readmsr(( u32 )msr)
#define WriteMsr(msr, x) __writemsr(( u32 )msr, ( u64 )x)

#define EFER_SCE (1 << 0)

#define APIC_BSP (1 << 8)
#define APIC_GLOBAL_ENABLE (1 << 11)

namespace x64
{
    enum class Msr : u32
    {
        APIC_BASE = 0x1b,

        FS_BASE = 0xc0000100,
        GS_BASE = 0xc0000101,
        KERNEL_GS_BASE = 0xc0000102,

        EFER = 0xc0000080,
        STAR = 0xc0000081,
        LSTAR = 0xc0000082,
        FMASK = 0xc0000084
    };

#pragma pack(1)
    union MsrStar
    {
        struct
        {
            u32 reserved;
            SegmentSelector syscall_cs;
            SegmentSelector sysret_cs;
        };
        u64 bits;
    };
    static_assert(sizeof(MsrStar) == sizeof(u64));
#pragma pack()
}
