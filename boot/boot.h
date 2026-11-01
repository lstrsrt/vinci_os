#pragma once

/*
 *  Main include file for the bootloader and kernel entry point.
 */

#include "../kernel/common/acpi.h"
#include "cpp-uefi.hh"

struct MemoryMap
{
    uefi::memory_descriptor* map;
    uintn size;
    uintn descriptor_size;
    uint32 descriptor_version;
};

struct DisplayInfo
{
    uintn frame_buffer;
    uintn frame_buffer_size;
    uintn back_buffer; // height * pitch bytes of RAM for the kernel console
    uint32 width;
    uint32 height;
    uint32 pitch;
};

struct KernelData
{
    physical_address physical_base;
    virtual_address entry_point;
    uintn size;
};

struct LoaderBlock
{
    acpi::Madt* madt_header;
    boolean i8042;
    uint64 hpet;
    uefi::configuration_table* config_table;
    uintn config_table_entries;
    MemoryMap memory_map;
    physical_address page_table;
    uintn page_table_size;
    physical_address page_pool;
    uintn page_pool_size;
    KernelData kernel;
    DisplayInfo display;
};
