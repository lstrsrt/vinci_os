#pragma once

/*
*  This file exists because only MSVC has <intrin.h>.
*  When cross compiling a PE, Clang is in MSVC compatibility mode
*  and also provides it, but then it fails in <mmintrin.h> for strange reasons.
*  The easiest (and most futureproof) fix is to just write these functions ourselves.
*
*  Note: the MSVC intrinsic names are used when possible so the same names
*  can be used everywhere without macros.
*/

#include <base.h>

#ifdef COMPILER_MSVC
#include <intrin.h>
#else
INLINE u64 __readgsqword(u64 offset)
{
    u64 value;
    asm volatile(
        "mov %%gs:%a[off], %[val]"
        : [val] "=r"(value)
        : [off] "ir"(offset)
        );
    return value;
}

INLINE void __writegsqword(u64 offset, u64 value)
{
    asm volatile(
        "mov %[val], %%gs:%a[off]"
        :: [off] "ir"(offset), [val] "r"(value)
        : "memory"
        );
}

INLINE void _writegsbase_u64(u64 value)
{
    asm volatile("wrgsbase %0" :: "r" (value) : "memory");
}

INLINE void NO_REDEF_mm_pause()
{
    asm volatile("pause");
}
#define _mm_pause NO_REDEF_mm_pause // workaround

INLINE void NO_REDEF__cpuidex(i32 regs[4], i32 leaf, i32 subleaf)
{
    asm volatile("cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(leaf), "c"(subleaf));
}
#define __cpuidex NO_REDEF__cpuidex // workaround

INLINE void __wbinvd()
{
    asm volatile("wbinvd" ::: "memory");
}

INLINE void _lgdt(uptr_t* desc)
{
    asm volatile("lgdt %0" :: "m"(*desc));
}

INLINE void __lidt(uptr_t* desc)
{
    asm volatile("lidt %0" :: "m"(*desc));
}

INLINE u8 __inbyte(u16 port)
{
    u8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

INLINE u16 __inword(u16 port)
{
    u16 value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

INLINE u32 __indword(u16 port)
{
    u32 value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

INLINE void __outbyte(u16 port, u8 data)
{
    asm volatile("outb %0, %1" :: "a"(data), "Nd"(port));
}

INLINE void __outword(u16 port, u16 data)
{
    asm volatile("outw %0, %1" :: "a"(data), "Nd"(port));
}

INLINE void __outdword(u16 port, u32 data)
{
    asm volatile("outl %0, %1" :: "a"(data), "Nd"(port));
}

INLINE u64 __readcr0()
{
    u64 cr0;
    asm("mov %%cr0, %%rax" : "=r"(cr0));
    return cr0;
}

INLINE void __writecr0(u64 cr0)
{
    asm volatile("mov %%rax, %%cr0" :: "r"(cr0));
}

INLINE u64 __readcr2()
{
    u64 cr2;
    asm volatile("mov %%cr2, %%rax" : "=r"(cr2));
    return cr2;
}

INLINE u64 __readcr3()
{
    u64 cr3;
    asm volatile("mov %%cr3, %%rax" : "=r"(cr3));
    return cr3;
}

INLINE void __writecr3(u64 cr3)
{
    asm volatile("mov %%rax, %%cr3" :: "r"(cr3) : "memory");
}

INLINE u64 __readcr4()
{
    u64 cr4;
    asm volatile("mov %%cr4, %%rax" : "=r"(cr4));
    return cr4;
}

INLINE void __writecr4(u64 cr4)
{
    asm volatile("mov %%rax, %%cr4" :: "r"(cr4));
}

INLINE u64 __readmsr(u32 msr)
{
    u32 low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return MAKE64(high, low);
}

INLINE void __writemsr(u32 msr, u64 data)
{
    u32 low = LOW32(data);
    u32 high = HIGH32(data);
    asm volatile("wrmsr" :: "c"(msr), "a"(low), "d"(high) : "memory");
}

INLINE void _disable()
{
    asm("cli");
}

INLINE void _enable()
{
    asm("sti");
}

INLINE void __halt()
{
    asm("hlt");
}

INLINE void _clac()
{
    asm("clac");
}

INLINE void _stac()
{
    asm("stac");
}

INLINE void __invlpg(void* addr)
{
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}
#endif
