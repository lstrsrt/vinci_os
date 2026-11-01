#pragma once

/*
 *  Virtual address constants for the bootloader and kernel.
 */

#include "../lib/base.h"

using vaddr_t = uintptr_t;
using paddr_t = uintptr_t;

static constexpr size_t page_size = 0x1000;
static constexpr size_t page_mask = 0xfff;
static constexpr size_t page_shift = 12;

INLINE constexpr bool IsPageAligned(uptr_t addr)
{
    return (addr & page_mask) == 0;
}

INLINE constexpr size_t SizeToPages(size_t size)
{
    return (size >> page_shift) + (size & page_mask ? 1 : 0);
}

INLINE constexpr auto AlignUp(size_t value, size_t align)
{
    return (value + align - 1) & ~(align - 1);
};

struct Region
{
    vaddr_t base;
    size_t size;

    consteval vaddr_t End() const { return base + size; }

    consteval size_t PageCount() const { return SizeToPages(size); }

    constexpr bool Contains(vaddr_t addr) const { return addr >= base && addr < (base + size); }
};

namespace kva
{
    // Kernel image: 0xffffffff'80000000 - 0xffffffff'81000000
    constexpr Region kernel_image{0xffffffff'80000000, MiB(16)};

    // Page tables: 0xffffffff'81000000 - 0xffffffff'82000000
    constexpr Region kernel_pt{kernel_image.End(), MiB(16)};

    // Device mappings: 0xffffffff'82000000 - 0xffffffff'83000000
    constexpr Region devices{kernel_pt.End(), MiB(16)};

    // Frame buffer: 0xffffffff'83000000 - 0xffffffff'87000000
    constexpr Region frame_buffer{devices.End(), MiB(64)};

    // Kernel pool: 0xffffffff'87000000 - 0xffffffff'87100000
    constexpr Region kernel_pool{frame_buffer.End(), MiB(1)};

    // UEFI runtime: 0xffffffff'87100000 - 0xffffffff'97100000
    constexpr Region uefi{kernel_pool.End(), MiB(256)};

    // Console back buffer: 0xffffffff'97100000 - 0xffffffff'9b100000
    constexpr Region back_buffer{uefi.End(), MiB(64)};
} // namespace kva
