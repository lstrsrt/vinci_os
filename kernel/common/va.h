#pragma once

/*
*  Virtual address constants for the bootloader and kernel.
*/

#include <base.h>

// TODO - use in bootloader?
struct Region
{
    vaddr_t base;
    size_t page_count;
};

namespace kva
{
    // Kernel image: 0xffffffff'80000000 - 0xffffffff'81000000
    static constexpr vaddr_t kernel_base = 0xffffffff'80000000;

    // Page tables: 0xffffffff'81000000 - 0xffffffff'85000000
    static constexpr vaddr_t page_tables = kernel_base + MiB(16);

    // Device mappings: 0xffffffff'85000000 - 0xffffffff'86000000
    static constexpr vaddr_t device_base = page_tables + MiB(64);

    // Framebuffer: 0xffffffff'86000000 - 0xffffffff'8a000000
    static constexpr vaddr_t frame_buffer_base = device_base + MiB(16);

    // UEFI runtime: 0xffffffff'8a000000 -
    static constexpr vaddr_t uefi_base = frame_buffer_base + MiB(64);
}
