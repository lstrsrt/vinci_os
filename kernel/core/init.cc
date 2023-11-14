#include <libc/str.h>
#include <ec/util.h>

#include "../../boot/boot.h"
#include "../hw/cpu/x64.h"
#include "../hw/cpu/isr.h"
#include "../hw/ps2/ps2.h"
#include "../hw/serial/serial.h"
#include "../hw/timer/timer.h"
#include "../hw/cmos/cmos.h"
#include "../hw/acpi/acpi.h"
#include "gfx/output.h"

#include "../common/mm.h"

static constexpr size_t kernel_stack_size = KiB(8);
alignas(page_size) volatile u8 kernel_stack[kernel_stack_size]{};
EXTERN_C u64 kernel_stack_top = ( u64 )&kernel_stack[kernel_stack_size];

EARLY static void IterateMemoryDescriptors(const MemoryMap& m, auto&& callback)
{
    for (auto desc = m.map;
        desc < uefi_next_memory_descriptor(m.map, m.size);
        desc = uefi_next_memory_descriptor(desc, m.descriptor_size))
    {
        callback(desc);
    }
}

EARLY static void SerialPrintDescriptors(MemoryMap& m)
{
    int i = 0;
    serial::Write("==== MEMORY MAP ====\n");
    for (auto current = m.map;
        current < uefi_next_memory_descriptor(m.map, m.size);
        current = uefi_next_memory_descriptor(current, m.descriptor_size))
    {
        serial::Write(
            "[%d]: Type: %d   PA: 0x%llx   VA: 0x%llx (pages: %llu) Attr 0x%llx\n",
            i++,
            current->type,
            current->physical_start,
            current->virtual_start,
            current->number_of_pages,
            current->attribute
        );
    }
    serial::Write("====================\n");
}

static IMAGE_NT_HEADERS* ImageNtHeaders(void* image_base)
{
    auto dos_header = ( IMAGE_DOS_HEADER* )image_base;
    if (!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;

    auto nt_headers = ( IMAGE_NT_HEADERS* )(( uintptr_t )image_base + dos_header->e_lfanew);
    if (!nt_headers || nt_headers->Signature != IMAGE_NT_SIGNATURE)
        return nullptr;

    return nt_headers;
}

EXTERN_C NO_RETURN void OsInitialize(LoaderBlock* loader_block)
{
    gfx::Initialize(loader_block->display);

    // Init COM ports so we have early debugging capabilities.
    // TODO - Remap APIC earlier when we use serial interrupts
    serial::Initialize();

    acpi::ParseMadt(loader_block->madt_header, x64::cpu_info);

    // Copy all the important stuff because the loader block gets freed in ReclaimBootPages
    MemoryMap memory_map = loader_block->memory_map;
    DisplayInfo display = loader_block->display;
    KernelData kernel = loader_block->kernel;
    uint64 hpet = loader_block->hpet;
    boolean i8042 = loader_block->i8042;
    const auto pt_physical = loader_block->page_tables_pool;
    const auto pt_count = loader_block->page_tables_pool_count;

    // Build a new page table (the bootloader one is temporary)
    PagePool pool(kva::page_tables, pt_physical, pt_count);
    AllocatePoolEntry(pool, &pool.root);

    const auto kernel_pages = SizeToPages(kernel.size);
    const auto frame_buffer_pages = SizeToPages(display.frame_buffer_size);

    MapPages(pool, kva::kernel_base, kernel.physical_base, kernel_pages);
    MapPages(pool, kva::page_tables, pt_physical, pt_count);

    // turns out vbox page faults at fb base + 0x3000000 when we reach the end so just map the whole range
    MapPages(pool, kva::frame_buffer_base, display.frame_buffer, 4096 /*frame_buffer_pages*/);
    display.frame_buffer = kva::frame_buffer_base;

    Region device_rg{ kva::device_base, 4096 };

    MapPagesInRegion(pool, device_rg, &hpet, 1);
    MapPagesInRegion(pool, device_rg, &apic::io, 1);
    MapPagesInRegion(pool, device_rg, &apic::local, 1);

    // SerialPrintDescriptors(memory_map);

    IterateMemoryDescriptors(memory_map, [&pool](uefi::memory_descriptor* desc)
    {
        if (desc->attribute & uefi::memory_runtime)
            MapPages(pool, desc->virtual_start, desc->physical_start, desc->number_of_pages);
    });

    __writecr3(pool.root);

    gfx::SetFrameBufferAddress(display.frame_buffer); // TODO - set this earlier?

    x64::cpu_info.using_apic = false;
    x64::Initialize();

    timer::Initialize(hpet);

    if (i8042)
        ps2::Initialize();
    else
        Print("No PS/2 legacy support.\n");

    // while (1)
    // {
    //     volatile u8 last = time.s;
    //     u64 x = __rdtsc();
    //     volatile u64 next = timer::ticks + 1000;
    //     while (next > timer::ticks)
    //         ;
    //     Print("%llu (%llu) ", ++a, __rdtsc() - x);
    //     while (last == time.s)
    //         ;
    // }

    // Now that kernel init has completed, zero out discardable sections (INIT, CRT, RELOC)
    auto kernel_nt = ImageNtHeaders(( void* )kva::kernel_base);
    auto section = IMAGE_FIRST_SECTION(kernel_nt);
    for (u16 i = 0; i < kernel_nt->FileHeader.NumberOfSections; i++)
    {
        if (section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
        {
            char section_name[IMAGE_SIZEOF_SHORT_NAME + 1]{};
            strlcpy(section_name, ( const char* )section->Name, IMAGE_SIZEOF_SHORT_NAME);
            Print(
                "Zeroing section %s at 0x%llx (%u bytes)\n",
                section_name,
                kva::kernel_base + section->VirtualAddress,
                section->Misc.VirtualSize
            );
            memzero(( void* )(kva::kernel_base + section->VirtualAddress), section->SizeOfRawData);
        }
        section++;
    }

    x64::unmask_interrupts();
    x64::Idle();
}
