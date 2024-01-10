#include <ec/util.h>
#include <libc/str.h>

#include "../common/mm.h"
#include "../../boot/boot.h"
#include "../hw/acpi/acpi.h"
#include "../hw/cmos/cmos.h"
#include "../hw/cpu/isr.h"
#include "../hw/cpu/x64.h"
#include "../hw/cpu/cpuid.h"
#include "../hw/ps2/ps2.h"
#include "../hw/serial/serial.h"
#include "../hw/timer/timer.h"
#include "ke.h"

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

void PrintContext(x64::Context* ctx)
{
    serial::Write("RAX: 0x%llx\n", ctx->rax);
    serial::Write("RCX: 0x%llx\n", ctx->rcx);
    serial::Write("RDX: 0x%llx\n", ctx->rdx);
    serial::Write("RBX: 0x%llx\n", ctx->rbx);
    serial::Write("RSI: 0x%llx\n", ctx->rsi);
    serial::Write("RDI: 0x%llx\n", ctx->rdi);

    serial::Write("R8 : 0x%llx\n", ctx->r8);
    serial::Write("R9 : 0x%llx\n", ctx->r9);
    serial::Write("R10: 0x%llx\n", ctx->r10);
    serial::Write("R11: 0x%llx\n", ctx->r11);
    serial::Write("R12: 0x%llx\n", ctx->r12);
    serial::Write("R13: 0x%llx\n", ctx->r13);
    serial::Write("R14: 0x%llx\n", ctx->r14);
    serial::Write("R15: 0x%llx\n", ctx->r15);

    serial::Write("RSP: 0x%llx\n", ctx->rsp);
    serial::Write("RBP: 0x%llx\n", ctx->rbp);

    serial::Write("RIP: 0x%llx\n", ctx->rip);

    serial::Write("RFL: 0x%llx\n", ctx->rflags);
    serial::Write("*********\n");
}

static void ThreadAFunc()
{
    int i = 0;
    while (1)
    {
        _disable();
        Print("AAAAAAA %d\n", i++);
        serial::Write("********* Reg Dump A:\n");
        PrintContext(&ke::cur_thread->ctx);
        _enable();
    }
}

static void ThreadBFunc()
{
    int i = 0;
    while (1)
    {
        _disable();
        Print("BBBBBBB %d\n", i++);
        serial::Write("********* Reg Dump B:\n");
        PrintContext(&ke::cur_thread->ctx);
        _enable();
    }
}

#pragma data_seg(".data")
static u64 thread_i = 0;
#pragma data_seg()

EXTERN_C void Switch(x64::Context* ctx);

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

    x64::cpu_info.using_apic = false;
    x64::Initialize(kernel_stack_top);

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

    {
        mm::MapPagesInRegion<kva::devices>(pool, &hpet, 1);
        mm::GetPresentPte(pool, hpet)->cache_disable = true;

        mm::MapPagesInRegion<kva::devices>(pool, &apic::io, 1);
        mm::GetPresentPte(pool, apic::io)->cache_disable = true;

        mm::MapPagesInRegion<kva::devices>(pool, &apic::local, 1);
        mm::GetPresentPte(pool, apic::local)->cache_disable = true;
    }

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

    // SerialPrintDescriptors(memory_map);

    // vaddr_t stk_va = kva::kernel_image.base + (200 * page_size);
    // vaddr_t stk_va2 = stk_va + page_size;
    // Print("stk_va 0x%llx stk_va2 0x%llx\n", stk_va + page_size, stk_va2 + page_size);
    // {
    //     paddr_t tmp_stk;
    //     mm::AllocatePhysical(pool, &tmp_stk);
    //     mm::MapPage(pool, stk_va, tmp_stk);
    //     mm::AllocatePhysical(pool, &tmp_stk);
    //     mm::MapPage(pool, stk_va2, tmp_stk);
    // }

    // paddr_t user_page;
    // vaddr_t user_page_va = 0x7ff7f0000000;
    // vaddr_t user_stack_va = 0x7ff7f0001000;
    //
    // mm::AllocatePhysical(pool, &user_page);
    // mm::MapPage(pool, user_page_va, user_page, true);
    //
    // FIXME why does this set 4 pages to present instead of 1?
    // mm::AllocatePhysical(pool, &user_page);
    // mm::MapPage(pool, user_stack_va, user_page, true);

    uptr_t kpool;
    mm::AllocatePhysical(pool, &kpool, kva::kernel_pool.PageCount());
    mm::MapPagesInRegion<kva::kernel_pool>(pool, &kpool, kva::kernel_pool.PageCount());

    __writecr3(pool.root);

    gfx::SetFrameBufferAddress(display.frame_buffer); // TODO - set this earlier?

    // {
    //     x64::SmapSetAc();
    //     memzero(( void* )user_stack_va, page_size);
    //     memcpy(( void* )user_page_va, ( const void* )x64::Ring3Function, 40);
    //     x64::SmapClearAc();
    // }

    timer::Initialize(hpet);

    if (i8042)
        ps2::Initialize();
    else
        Print("No PS/2 legacy support.\n");

    // Now that kernel init has completed, zero out discardable sections
    // and write-protect every section not marked writable.
    FinalizeKernelMapping(pool);

    x64::unmask_interrupts();

    ke::InitializeAllocator();

    // ke::StartScheduler();
    // ke::CreateThread(ThreadAFunc, stk_va + page_size);
    // ke::CreateThread(ThreadBFunc, stk_va2 + page_size);
    //
    // for (auto thread = ke::first_thread; thread != nullptr; thread->next)
    //     Print("    0x%llx\n", thread);

    // Switch(&ke::cur_thread->ctx);

    // x64::EnterUserMode(user_page_va, user_stack_va + page_size /* stack starts at top */);

    x64::Idle();
}
