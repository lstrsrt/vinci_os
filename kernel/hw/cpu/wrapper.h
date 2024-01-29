#pragma once

#ifdef COMPILER_CLANG
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

INLINE void _mm_pause()
{
    asm volatile("pause");
}

INLINE void __cpuidex(i32 regs[4], i32 leaf, i32 subleaf)
{
    asm volatile("cpuid"
        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(leaf), "c"(subleaf));
}

INLINE void __wbinvd()
{
    asm volatile("wbinvd" ::: "memory");
}

INLINE void _lgdt(x64::DescriptorTable* desc)
{
    asm volatile("lgdt %0" :: "m"(*desc));
}

INLINE void __lidt(x64::DescriptorTable* desc)
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
    asm volatile("outb %0, %1" ::"a"(data), "Nd"(port));
}

INLINE void __outword(u16 port, u16 data)
{
    asm volatile("outw %0, %1" ::"a"(data), "Nd"(port));
}

INLINE void __outdword(u16 port, u32 data)
{
    asm volatile("outl %0, %1" ::"a"(data), "Nd"(port));
}

INLINE u64 __readcr0()
{
    u64 cr0;
    asm("mov %%cr0, %%rax" : "=a"(cr0));
    return cr0;
}

INLINE void __writecr0(u64 cr0)
{
    asm volatile("mov %%rax, %%cr0" :: "a"(cr0));
}

INLINE u64 __readcr2()
{
    u64 cr2;
    asm("mov %%cr2, %%rax" : "=a"(cr2));
    return cr2;
}

INLINE u64 __readcr3()
{
    u64 cr3;
    asm("mov %%cr4, %%rax" : "=a"(cr3));
    return cr3;
}

INLINE void __writecr3(u64 cr3)
{
    asm volatile("mov %%rax, %%cr3" :: "a"(cr3));
}

INLINE u64 __readcr4()
{
    u64 cr4;
    asm("mov %%cr4, %%rax" : "=a"(cr4));
    return cr4;
}

INLINE void __writecr4(u64 cr4)
{
    asm volatile("mov %%rax, %%cr4" :: "a"(cr4));
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
