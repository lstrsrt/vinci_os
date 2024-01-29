#include <ec/util.h>
#include <libc/str.h>

#include "../../boot/boot.h"
#include "../common/mm.h"
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

namespace pe
{
    enum SectionFlag
    {
        Discardable = 0x02000000,
        NotCached = 0x04000000,
        NotPaged = 0x08000000,
        Shared = 0x10000000,
        Execute = 0x20000000,
        Read = 0x40000000,
        Write = 0x80000000,
    };

    using Section = IMAGE_SECTION_HEADER;
    using NtHeaders = IMAGE_NT_HEADERS;
    using DosHeader = IMAGE_DOS_HEADER;

    struct File
    {
        DosHeader* m_dos_header;
        NtHeaders* m_nt_headers;

        void ForEachSection(const auto&& callback)
        {
            auto section = IMAGE_FIRST_SECTION(m_nt_headers);
            for (u16 i = 0; i < m_nt_headers->FileHeader.NumberOfSections; i++, section++)
            {
                if (!callback(section))
                    return;
            }
        }

        static File FromImageBase(void* base)
        {
            File pe;
            pe.m_nt_headers = ImageNtHeaders(base);
            pe.m_dos_header = ( DosHeader* )base;
            return pe;
        }
    };
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

void test(void*)
{
    int i = 0;
    volatile u64 next = timer::ticks + 1000;
    while (1)
    {
        Print("thread A %d\n", i++);
        while (next > timer::ticks)
            ;
        next = timer::ticks + 1000;
    }
}

void test2(void*)
{
    int i = 0;
    volatile u64 next = timer::ticks + 1000;
    while (1)
    {
        Print("thread B %d\n", i++);
        while (next > timer::ticks)
            ;
        next = timer::ticks + 1000;
    }
}

namespace x64
{
    EXTERN_C void Switch(x64::Context*);
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

    // FIXME
    // table->phys = loader_block->page_table;
    // mm::AllocatePhysical(*table, &table->root);

    const auto kernel_pages = SizeToPages(kernel.size);
    const auto frame_buffer_pages = SizeToPages(display.frame_buffer_size);

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

    // ke::CreateThread(test, nullptr);
    // ke::CreateThread(test2, nullptr);

    // Enter idle loop!
    x64::Switch(&ke::GetCore()->first_thread->ctx);

    x64::Idle();
}
