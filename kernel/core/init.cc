#include <ec/util.h>
#include <libc/str.h>

#include "../../boot/boot.h"
#include "../common/mm.h"
#include "../hw/acpi/acpi.h"
#include "../hw/cmos/cmos.h"
#include "../hw/cpu/isr.h"
#include "../hw/cpu/x64.h"
#include "../hw/cpu/cpuid.h"
#include "../hw/ps2/ps2.h"
#include "../hw/serial/serial.h"
#include "../hw/timer/timer.h"
#include "gfx/output.h"

static constexpr size_t kernel_stack_size = KiB(8);
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
    auto kernel_nt = ImageNtHeaders(( void* )kva::kernel.base);
    auto section = IMAGE_FIRST_SECTION(kernel_nt);

    for (u16 i = 0; i < kernel_nt->FileHeader.NumberOfSections; i++)
    {
        const auto start = kva::kernel.base + section->VirtualAddress;
        const auto size = section->Misc.VirtualSize;
        const auto end = start + size;

        if (section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
            memzero(( void* )start, size);

        if (!(section->Characteristics & IMAGE_SCN_MEM_WRITE))
        {
            for (auto page = start; page < end; page += page_size)
            {
                auto pte = mm::GetPresentPte(pool, start);
                pte->writable = false;
                x64::TlbFlushAddress(( void* )page);
            }
        }

        section++;
    }
}

#define MSR_MTRR_DEF_TYPE 0x2ff
#define MTRR_FIXED_ENABLED (1 << 10)
#define MTRR_ENABLED (1 << 11)

#define MSR_PAT 0x277

static void LoadPat(u64 pat)
{
    using namespace x64;

    // Normally we would also have to disable interrupts here
    // but this gets called before we enable them in the first place

    auto cr0 = ReadCr0();
    WriteCr0(cr0 & ~(Cr0::CD | Cr0::NW));

    __wbinvd();
    __writemsr(MSR_MTRR_DEF_TYPE, __readmsr(MSR_MTRR_DEF_TYPE) & ~(MTRR_ENABLED | MTRR_FIXED_ENABLED));
    __wbinvd(); // Flush caches twice (required on Xeon and P4)

    auto cr4 = ReadCr4();
    WriteCr4(cr4 & ~(Cr4::PGE));
    TlbFlush();

    __writemsr(MSR_PAT, pat);

    __wbinvd();
    TlbFlush();

    WriteCr4(cr4);
    WriteCr0(cr0);
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
    auto hpet = loader_block->hpet;
    auto i8042 = loader_block->i8042;
    const auto pt_physical = loader_block->page_table;
    const auto pt_pages = loader_block->page_table_size;

    ReclaimBootPages(memory_map);
    CoalesceMemoryDescriptors(memory_map);

    // Build a new page table (the bootloader one is temporary)
    mm::PagePool pool(kva::kernel_pt.base, pt_physical, pt_pages);
    mm::AllocatePhysical(pool, &pool.root);

    const auto kernel_pages = SizeToPages(kernel.size);
    const auto frame_buffer_pages = SizeToPages(display.frame_buffer_size);

    mm::MapPages(pool, kva::kernel.base, kernel.physical_base, kernel_pages);
    mm::MapPages(pool, kva::kernel_pt.base, pt_physical, pt_pages);

    enum MemoryType : u64
    {
        MT_Uncacheable = 0x00, // (UC)
        MT_WriteCombining = 0x01,
        MT_WriteThrough = 0x04,
        MT_WriteProtected = 0x05,
        MT_WriteBack = 0x06,
        MT_Uncached = 0x07, // (UC-) PAT only, reserved on MTRRs
    };

    u64 ia32_mtrr_def_type = __readmsr(MSR_MTRR_DEF_TYPE);
    Print("IA32_MTRR_DEF_TYPE: 0x%llx\n", ia32_mtrr_def_type);

    // This is the same as the default PAT entries
    // but with one exception: PAT4 selects WC instead of WB
    static constexpr u64 pat_entries =
          (MT_WriteBack)            // PAT0
        | (MT_WriteThrough << 8)    // PAT1
        | (MT_Uncached << 16)       // PAT2
        | (MT_Uncacheable << 24)    // PAT3
        | (MT_WriteCombining << 32) // PAT4
        | (MT_WriteThrough << 40)   // PAT5
        | (MT_Uncached << 48)       // PAT6
        | (MT_Uncacheable << 56);   // PAT7

    // NOTE: If we use UC as the default MTRR type then
    // we can only use WC from the PAT, all other entries don't matter anymore.

    auto ia32_pat = __readmsr(MSR_PAT);
    Print("IA32_PAT: 0x%llx\n", ia32_pat);

    // Update the PAT to have a WC entry
    LoadPat(pat_entries);
    Print("IA32_PAT: 0x%llx\n", __readmsr(MSR_PAT));

    // turns out vbox page faults at fb base + 0x3000000 when we reach the end so just map the whole range
    mm::MapPagesInRegion<kva::frame_buffer>(pool, &display.frame_buffer, kva::frame_buffer.PageCount());

    // This should already be UC due to default MTRR memory type
    mm::MapPagesInRegion<kva::devices>(pool, &hpet, 1);
    mm::MapPagesInRegion<kva::devices>(pool, &apic::io, 1);
    mm::MapPagesInRegion<kva::devices>(pool, &apic::local, 1);

    IterateMemoryDescriptors(memory_map, [&pool](uefi::memory_descriptor* desc)
    {
        if (desc->attribute & uefi::memory_runtime)
        {
            // FIXME - on hardware this maps into very low VM?
            Print(
                "Mapping UEFI runtime: 0x%llx -> 0x%llx (%llu pages)\n",
                desc->physical_start,
                desc->virtual_start,
                desc->number_of_pages
            );
            MapPages(pool, desc->virtual_start, desc->physical_start, desc->number_of_pages);
        }
    });

    // SerialPrintDescriptors(memory_map);

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

    // Now that kernel init has completed, zero out discardable sections
    // and write-protect every section not marked writable.
    FinalizeKernelMapping(pool);

    x64::unmask_interrupts();
    x64::Idle();
}
