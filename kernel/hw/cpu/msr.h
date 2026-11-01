#pragma once

#include <base.h>

#include "x64.h"

#define ReadMsr(msr) __readmsr(( u32 )msr)
#define WriteMsr(msr, x) __writemsr(( u32 )msr, ( u64 )x)

#define EFER_SCE (1 << 0)

namespace x64
{
    enum class Msr : u32
    {
        APIC_BASE = 0x1b,

        PAT = 0x277,

        FS_BASE = 0xc0000100,
        GS_BASE = 0xc0000101,
        KERNEL_GS_BASE = 0xc0000102,

        EFER = 0xc0000080,
        STAR = 0xc0000081,
        LSTAR = 0xc0000082,
        SFMASK = 0xc0000084,
    };

#pragma pack(1)

    enum_flags(MsrApicBase, u64){
        BSP = 1 << 8,
        EXTD = 1 << 10,
        GLOBAL_ENABLE = 1 << 11,
    };

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
} // namespace x64
