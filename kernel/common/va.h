#pragma once

/*
*  Virtual address constants for the bootloader and kernel.
*/

#include <base.h>

using vaddr_t = uintptr_t;
using paddr_t = uintptr_t;

static constexpr size_t page_size  = 0x1000;
static constexpr size_t page_mask  = 0xfff;
static constexpr size_t page_shift = 12;

inline constexpr auto SizeToPages(size_t size)
{
    return (size >> page_shift) + (size & page_mask ? 1 : 0);
}

struct Region
{
    vaddr_t base;
    size_t size;

    constexpr vaddr_t End() const { return base + size; }
    constexpr size_t PageCount() const { return SizeToPages(size); }
};

namespace kva
{
    // Kernel image: 0xffffffff'80000000 - 0xffffffff'81000000
    constexpr Region kernel{
        0xffffffff'80000000,
        MiB(16)
    };

    // Page tables: 0xffffffff'81000000 - 0xffffffff'85000000
    constexpr Region kernel_pt{
        kernel.End(),
        MiB(64)
    };

    // Device mappings: 0xffffffff'85000000 - 0xffffffff'86000000
    constexpr Region devices{
        kernel_pt.End(),
        MiB(16)
    };

    // Framebuffer: 0xffffffff'86000000 - 0xffffffff'8a000000
    constexpr Region frame_buffer{
        devices.End(),
        MiB(64)
    };

    // UEFI runtime: 0xffffffff'8a000000 - 0xffffffff'9a000000
    constexpr Region uefi{
        frame_buffer.End(),
        MiB(256)
    };
}
