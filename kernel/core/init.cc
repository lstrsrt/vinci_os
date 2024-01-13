#include <ec/util.h>
#include <libc/str.h>

#include "../../boot/boot.h"
#include "../common/mm.h"
#include "../hw/acpi/acpi.h"
#include "../hw/cmos/cmos.h"
#include "../hw/cpu/isr.h"
#include "../hw/cpu/x64.h"
#include "../hw/cpu/msr.h"
#include "../hw/ps2/ps2.h"
#include "../hw/serial/serial.h"
#include "../hw/timer/timer.h"
#include "ke.h"

static constexpr size_t kernel_stack_size = KiB(4);
alignas(page_size) volatile u8 kernel_stack[kernel_stack_size];
EXTERN_C auto kernel_stack_top = ( uptr_t )&kernel_stack[kernel_stack_size];

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
    size_t i = 0;
    serial::Write("==== MEMORY MAP ====\n");
    IterateMemoryDescriptors(m, [&i](uefi::memory_descriptor* desc)
    {
        serial::Write(
            "[%llu]: Type: %d   PA: 0x%llx   VA: 0x%llx (pages: %llu) Attr 0x%llx\n",
            i++,
            desc->type,
            desc->physical_start,
            desc->virtual_start,
            desc->number_of_pages,
            desc->attribute
        );
    });
    serial::Write("====================\n");
}

EARLY static void ReclaimBootPages(const MemoryMap& m)
{
    size_t count = 0;
    IterateMemoryDescriptors(m, [&count](uefi::memory_descriptor* desc)
    {
        if ((desc->type == uefi::memory_type::boot_services_code
            || desc->type == uefi::memory_type::boot_services_data
            || desc->type == uefi::memory_type::loader_code)
            && !(desc->attribute & uefi::memory_attribute::memory_runtime))
        {
            count++;
            desc->type = uefi::memory_type::conventional_memory;
        }
    });
    Print("Reclaimed %llu boot pages\n", count);
}

EARLY static void CoalesceMemoryDescriptors(const MemoryMap& memory_map)
{
    size_t count = 0;
    // can't use IterateMemoryDescriptors because the passed pointer is modified
    for (auto desc = memory_map.map;
        desc < uefi_next_memory_descriptor(memory_map.map, memory_map.size);
        desc = uefi_next_memory_descriptor(desc, memory_map.descriptor_size))
    {
        if (auto next = uefi_next_memory_descriptor(desc, memory_map.descriptor_size))
        {
            // If these entries are of the same type and directly next to each other
            // (last one ends where the next one starts) we can merge them into one
            if (desc->type == next->type && desc->attribute == next->attribute
                && (desc->physical_start + desc->number_of_pages * page_size) == next->physical_start)
            {
                count++;
                next->physical_start = desc->physical_start;
                next->number_of_pages += desc->number_of_pages;
                desc = next;
            }
        }
    }
    Print("Coalesced %llu memory descriptors.\n", count);
}

EARLY static void MapUefiRuntime(const MemoryMap& memory_map, mm::PagePool& pool)
{
    IterateMemoryDescriptors(memory_map, [&pool](uefi::memory_descriptor* desc)
    {
        if (desc->attribute & uefi::memory_runtime)
        {
            Print(
                "Mapping UEFI runtime: 0x%llx -> 0x%llx (%llu pages)\n",
                desc->physical_start,
                desc->virtual_start,
                desc->number_of_pages
            );
            mm::MapPages(pool, desc->virtual_start, desc->physical_start, desc->number_of_pages);
        }
    });
}

static IMAGE_NT_HEADERS* ImageNtHeaders(void* image_base)
{
    auto dos_header = ( IMAGE_DOS_HEADER* )image_base;
    if (!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE)
        return nullptr;

    auto nt_headers = ( IMAGE_NT_HEADERS* )(( uptr_t )image_base + dos_header->e_lfanew);
    if (!nt_headers || nt_headers->Signature != IMAGE_NT_SIGNATURE)
        return nullptr;

    return nt_headers;
}

static void FinalizeKernelMapping(mm::PagePool& pool)
{
    auto kernel_nt = ImageNtHeaders(( void* )kva::kernel_image.base);
    auto section = IMAGE_FIRST_SECTION(kernel_nt);

    for (u16 i = 0; i < kernel_nt->FileHeader.NumberOfSections; i++, section++)
    {
        const auto start = kva::kernel_image.base + section->VirtualAddress;
        const auto size = section->Misc.VirtualSize;
        const auto end = start + size;

        if (section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
        {
            memzero(( void* )start, size);
            for (auto page = start; page < end; page += page_size)
            {
                mm::UnmapPage(pool, page);
                x64::TlbFlushAddress(( void* )page);
            }
            continue;
        }

        if (!(section->Characteristics & IMAGE_SCN_MEM_WRITE))
        {
            for (auto page = start; page < end; page += page_size)
            {
                auto pte = mm::GetPresentPte(pool, page);
                pte->writable = false;
                x64::TlbFlushAddress(( void* )page);
            }
        }
    }
}

namespace ke
{
    void InitializeCore()
    {
        auto core = Allocate<Core>();
        core->self = core;
        // current_thread is initialized by the first call to SelectNextThread

        WriteMsr(x64::Msr::KERNEL_GS_BASE, ( uptr_t )core);
        _writegsbase_u64(( uptr_t )core);
    }
}

EXTERN_C NO_RETURN void OsInitialize(LoaderBlock* loader_block)
{
    gfx::Initialize(loader_block->display);

    // Copy all the important stuff because the loader block gets freed in ReclaimBootPages
    MemoryMap memory_map = loader_block->memory_map;
    DisplayInfo display = loader_block->display;
    KernelData kernel = loader_block->kernel;
    auto hpet = loader_block->hpet;
    auto i8042 = loader_block->i8042;
    const auto pt_physical = loader_block->page_table;
    const auto pt_pages = loader_block->page_table_size;

    ReclaimBootPages(memory_map);
    CoalesceMemoryDescriptors(memory_map);

    acpi::ParseMadt(loader_block->madt_header, x64::cpu_info);

    x64::cpu_info.using_apic = false;
    x64::Initialize(kernel_stack_top);

    // Init COM ports so we have early debugging capabilities.
    serial::Initialize();

    // Build a new page table (the bootloader one is temporary)
    mm::PagePool pool(kva::kernel_pt.base, pt_physical, pt_pages);
    mm::AllocatePhysical(pool, &pool.root);

    const auto kernel_pages = SizeToPages(kernel.size);
    const auto frame_buffer_pages = SizeToPages(display.frame_buffer_size);

    mm::MapPages(pool, kva::kernel_image.base, kernel.physical_base, kernel_pages);
    mm::MapPages(pool, kva::kernel_pt.base, pt_physical, pt_pages);

    // turns out vbox page faults at fb base + 0x3000000 when we reach the end so just map the whole range
    mm::MapPagesInRegion<kva::frame_buffer>(pool, &display.frame_buffer, kva::frame_buffer.PageCount());

    // Map the framebuffer as write combining (PAT4)
    for (auto page = kva::frame_buffer.base; page < kva::frame_buffer.End(); page += page_size)
        mm::GetPresentPte(pool, page)->pat = true;

    // Map devices and set CD bit
    {
        mm::MapPagesInRegion<kva::devices>(pool, &hpet, 1);
        mm::GetPresentPte(pool, hpet)->cache_disable = true;

        mm::MapPagesInRegion<kva::devices>(pool, &apic::io, 1);
        mm::GetPresentPte(pool, apic::io)->cache_disable = true;

        mm::MapPagesInRegion<kva::devices>(pool, &apic::local, 1);
        mm::GetPresentPte(pool, apic::local)->cache_disable = true;
    }

    MapUefiRuntime(memory_map, pool);

    uptr_t kpool;
    mm::AllocatePhysical(pool, &kpool, kva::kernel_pool.PageCount());
    mm::MapPagesInRegion<kva::kernel_pool>(pool, &kpool, kva::kernel_pool.PageCount());

    uptr_t stack;
    vaddr_t stack_va = kva::kernel_image.base + (200 * page_size);
    mm::AllocatePhysical(pool, &stack);
    mm::MapPage(pool, stack_va, stack);

    __writecr3(pool.root);

    gfx::SetFrameBufferAddress(display.frame_buffer); // TODO - set this earlier?

    timer::Initialize(hpet);

    if (i8042)
        ps2::Initialize();
    else
        Print("No PS/2 legacy support.\n");

    // Now that kernel init has completed, zero out discardable sections
    // and write-protect every section not marked writable.
    FinalizeKernelMapping(pool);

    ke::InitializeAllocator();
    ke::InitializeCore();

    x64::unmask_interrupts();

    ke::StartScheduler();

    x64::Idle();
}
