#pragma once

/*
*  Main include file for the bootloader and kernel entry point.
*  gnu-efi headers are only included to support the ACPI headers.
*/

#include "gnu-efi/efi.h"
#include "gnu-efi/efilink.h"
#include "gnu-efi/x86_64/pe.h"
#include "../common/acpi/Base.h"
#include "../common/acpi/Acpi.h"
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
    uint32 width;
    uint32 height;
    uint32 pitch;
};

struct KernelData
{
    physical_address physical_base;
    virtual_address entry_point;
    uint32 size;
};

struct LoaderBlock
{
    alignas(4096) EFI_ACPI_2_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER* madt_header;
    boolean i8042;
    uint64 hpet;

    uefi::configuration_table* config_table;
    uintn config_table_entries;

    MemoryMap memory_map;
    physical_address page_tables_pool;
    uint32 page_tables_pool_count; // in pages

    KernelData kernel;

    DisplayInfo display;
};