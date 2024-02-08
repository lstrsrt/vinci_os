#include <ec/util.h>
#include <libc/str.h>

#include "../../boot/boot.h"
#include "../common/mm.h"
#include "../common/pe64.h"
#include "../hw/acpi/acpi.h"
#include "../hw/cpu/isr.h"
#include "gfx/output.h"
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

EARLY static void MapUefiRuntime(const MemoryMap& memory_map, mm::PageTable& table)
{
    IterateMemoryDescriptors(memory_map, [&table](uefi::memory_descriptor* desc)
    {
        if (desc->attribute & uefi::memory_runtime)
        {
            Print(
                "Mapping UEFI runtime: 0x%llx -> 0x%llx (%llu pages)\n",
                desc->physical_start,
                desc->virtual_start,
                desc->number_of_pages
            );
            mm::MapPages(table, desc->virtual_start, desc->physical_start, desc->number_of_pages);
        }
    });
}

static void FinalizeKernelMapping(mm::PageTable& table)
{
    auto pe = pe::File::FromImageBase(( void* )kva::kernel_image.base);

    pe.ForEachSection([&table](pe::Section* section) -> bool
    {
        const auto start = kva::kernel_image.base + section->VirtualAddress;
        const auto size = section->Misc.VirtualSize;
        const auto end = start + size;

        char name[IMAGE_SIZEOF_SHORT_NAME + 1];
        strlcpy(name, ( const char* )section->Name, IMAGE_SIZEOF_SHORT_NAME);

        if (section->Characteristics & pe::SectionFlag::Discardable)
        {
            memzero(( void* )start, size);
            for (auto page = start; page < end; page += page_size)
            {
                mm::UnmapPage(table, page);
                x64::TlbFlushAddress(( void* )page);
            }
            return true;
        }

        if (!(section->Characteristics & pe::SectionFlag::Write))
        {
            for (auto page = start; page < end; page += page_size)
            {
                auto pte = mm::GetPresentPte(table, page);
                pte->writable = false;
                x64::TlbFlushAddress(( void* )page);
            }
        }

        return true;
    });
}

namespace ke
{
    void InitializeCore()
    {
        auto core = new Core();
        core->self = core;
        WriteMsr(x64::Msr::KERNEL_GS_BASE, ( uptr_t )core);
        _writegsbase_u64(( uptr_t )core);
    }
}

int test(void*)
{
    int i = 0;
    while (i < 10)
    {
        Print("thread A %d\n", i++);
        ke::Delay(1000);
    }
    return 1;
}

int test2(void*)
{
    int i = 0;
    while (i < 10)
    {
        Print("thread B %d\n", i++);
        ke::Delay(1000);
    }
    return 2;
}

int test3(void*)
{
    int i = 0;
    while (i < 20)
    {
        Print("thread C %d\n", i++);
        ke::Delay(1000);
    }
    return 3;
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
    const auto pp_physical = loader_block->page_pool;
    const auto pp_pages = loader_block->page_pool_size;

    ReclaimBootPages(memory_map);
    CoalesceMemoryDescriptors(memory_map);

    acpi::ParseMadt(loader_block->madt_header, x64::cpu_info);

    x64::cpu_info.using_apic = false;
    x64::Initialize(kernel_stack_top);

    // Init COM ports so we have early debugging capabilities.
    serial::Initialize();

    // From here on we can use the heap.
    ke::InitializeAllocator();

    // Build a new page table (the bootloader one is temporary)
    // TODO - store somewhere so it's accessible elsewhere
    auto table = new mm::PageTable(kva::kernel_pt.base, pt_physical, pt_pages);

    const auto kernel_pages = SizeToPages(kernel.size);
    UNUSED const auto frame_buffer_pages = SizeToPages(display.frame_buffer_size);

    mm::MapPages(*table, kva::kernel_image.base, kernel.physical_base, kernel_pages);
    mm::MapPages(*table, kva::kernel_pt.base, pt_physical, pt_pages);

    // turns out vbox page faults at fb base + 0x3000000 when we reach the end so just map the whole range
    mm::MapPagesInRegion<kva::frame_buffer>(*table, &display.frame_buffer, kva::frame_buffer.PageCount());

    // Map the framebuffer as write combining (PAT4)
    for (auto page = kva::frame_buffer.base; page < kva::frame_buffer.End(); page += page_size)
        mm::GetPresentPte(*table, page)->pat = true;

    // Map devices and set CD bit
    {
        mm::MapPagesInRegion<kva::devices>(*table, &hpet, 1);
        mm::GetPresentPte(*table, hpet)->cache_disable = true;

        mm::MapPagesInRegion<kva::devices>(*table, &apic::io, 1);
        mm::GetPresentPte(*table, apic::io)->cache_disable = true;

        mm::MapPagesInRegion<kva::devices>(*table, &apic::local, 1);
        mm::GetPresentPte(*table, apic::local)->cache_disable = true;
    }

    MapUefiRuntime(memory_map, *table);

    mm::MapPages(*table, kva::kernel_pool.base, pp_physical, pp_pages);

    __writecr3(table->root);
    gfx::SetFrameBufferAddress(display.frame_buffer);

    timer::Initialize(hpet);

    if (i8042)
        ps2::Initialize();
    else
        Print("No PS/2 legacy support.\n");

    // Now that kernel init has completed, zero out discardable sections
    // and write-protect every section not marked writable.
    FinalizeKernelMapping(*table);

    ke::InitializeCore();

    x64::unmask_interrupts();

    ke::StartScheduler();

    ke::CreateThread(test, nullptr);
    ke::CreateThread(test2, nullptr);
    ke::CreateThread(test3, nullptr);

    // Enter idle loop!
    x64::LoadContext(&ke::GetCore()->idle_thread->ctx);

    x64::Idle();
}
