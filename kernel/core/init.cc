#include <ec/util.h>
#include <libc/str.h>

#include "../../boot/boot.h"
#include "../common/mm.h"
#include "../common/pe64.h"
#include "../hw/acpi/acpi.h"
#include "../hw/cpu/isr.h"
#include "../hw/cpu/msr.h"
#include "../hw/cpu/x64.h"
#include "../hw/gfx/output.h"
#include "../hw/ps2/ps2.h"
#include "../hw/serial/serial.h"
#include "../hw/timer/timer.h"
#include "ke.h"

static constexpr size_t kernel_stack_size = KiB(4);
alignas(page_size) volatile u8 kernel_stack[kernel_stack_size];
EXTERN_C auto kernel_stack_base = ( uptr_t )&kernel_stack[kernel_stack_size];

static inline void IterateMemoryDescriptors(const MemoryMap& m, auto&& callback)
{
    for (auto desc = m.map; desc < uefi_next_memory_descriptor(m.map, m.size);
        desc = uefi_next_memory_descriptor(desc, m.descriptor_size))
    {
        callback(desc);
    }
}

DEBUG_FN EARLY static void SerialPrintDescriptors(MemoryMap& m)
{
    size_t i = 0;
    serial::Write("==== MEMORY MAP ====\n");
    IterateMemoryDescriptors(m, [&i](uefi::memory_descriptor* desc) {
        serial::Write("[%llu]: Type: %d   PA: 0x%p   VA: 0x%p (pages: %llu) Attr 0x%p\n", i++, desc->type,
            desc->physical_start, desc->virtual_start, desc->number_of_pages, desc->attribute);
    });
    serial::Write("====================\n");
}

EARLY static void ReclaimBootPages(const MemoryMap& m)
{
    size_t count = 0;
    IterateMemoryDescriptors(m, [&count](uefi::memory_descriptor* desc) {
        if ((desc->type == uefi::memory_type::boot_services_code || desc->type == uefi::memory_type::boot_services_data
                || desc->type == uefi::memory_type::loader_code)
            && !(desc->attribute & uefi::memory_attribute::memory_runtime))
        {
            count++;
            desc->type = uefi::memory_type::conventional_memory;
        }
    });
    Print("Reclaimed %llu boot pages\n", count);
}

EARLY static void CoalesceMemoryDescriptors(MemoryMap& memory_map)
{
    if (!memory_map.size)
        return;

    size_t count = 0;
    const auto desc_size = memory_map.descriptor_size;
    const auto end = uefi_next_memory_descriptor(memory_map.map, memory_map.size);

    // Compact the map in place: `last` is the most recent descriptor we kept.
    auto last = memory_map.map;
    for (auto desc = uefi_next_memory_descriptor(last, desc_size); desc < end;
        desc = uefi_next_memory_descriptor(desc, desc_size))
    {
        // If these entries are of the same type and directly next to each other
        // (last one ends where the next one starts) we can merge them into one
        if (desc->type == last->type && desc->attribute == last->attribute
            && (last->physical_start + last->number_of_pages * page_size) == desc->physical_start)
        {
            count++;
            last->number_of_pages += desc->number_of_pages;
        }
        else
        {
            last = uefi_next_memory_descriptor(last, desc_size);
            if (last != desc)
                memcpy(last, desc, desc_size);
        }
    }

    memory_map.size = ( u8* )uefi_next_memory_descriptor(last, desc_size) - ( u8* )memory_map.map;
    Print("Coalesced %llu memory descriptors.\n", count);
}

EARLY static void MapUefiRuntime(const MemoryMap& memory_map, mm::PageTable& table)
{
    IterateMemoryDescriptors(memory_map, [&table](uefi::memory_descriptor* desc) {
        if (desc->attribute & uefi::memory_runtime)
        {
            Print("Mapping UEFI runtime: 0x%p -> 0x%p (%llu pages)\n", desc->physical_start, desc->virtual_start,
                desc->number_of_pages);
            mm::MapPages(table, desc->virtual_start, desc->physical_start, desc->number_of_pages);
        }
    });
}

EARLY static void PrintSplash()
{
    gfx::SetColor(255, 255, 0);
    // clang-format off
    Print("░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░▒▓███████▓▒░ ░▒▓██████▓▒░░▒▓█▓▒░            ░▒▓██████▓▒░ ░▒▓███████▓▒░\n"
    "░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░            ▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░\n"
    " ░▒▓█▓▒▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░            ▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░\n"
    " ░▒▓█▓▒▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░            ▒▓█▓▒░░▒▓█▓▒░░▒▓██████▓▒░\n"
    "  ░▒▓█▓▓█▓▒░ ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░      ░▒▓█▓▒░            ▒▓█▓▒░░▒▓█▓▒░      ░▒▓█▓▒░\n"
    "  ░▒▓█▓▓█▓▒░ ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░▒▓█▓▒░            ▒▓█▓▒░░▒▓█▓▒░      ░▒▓█▓▒░\n"
    "   ░▒▓██▓▒░  ░▒▓█▓▒░▒▓█▓▒░░▒▓█▓▒░░▒▓██████▓▒░░▒▓█▓▒░░▒▓█████▓▒░░░▒▓██████▓▒░░▒▓███████▓▒░\n");
    // clang-format on

    gfx::SetColor(255, 255, 255);
    Print(":.....::.....:::.......=%%:%%+-=*@@@@#@%%.#..........:=:...........\n"
          "......::.::....::.:....--=-.-:%%@@@@@@@#*:.-..........:-=..:.....\n"
          "....:::::::.......::..:..=..:+@@@@@@@@@+-:-....:......-+=.::....\n"
          "....:.:::...........:....:...-#@@@@@@@@@==-.:.-.:.....-*+.......\n"
          "......:.:.::+...........-....-:*@@@@@@@@%%:-:.+--......++=.......\n"
          "..:...::.:==@..+...-=.....:...:*@@@@@@@@@*::.=*-=:...=+*........\n"
          "..........-=.......==*.......--*@@@@@@@@@%%....:**==-=*-.........\n"
          ".........::===...:=+%%+.......=.%%@@@@@@@@@@-:....................\n"
          "...........:==-==*#@#:::...:.=:-*@@@@@@@@@#:...##+-::-:.........\n"
          "...............:=+=:.:-::...-.+=@@@@@@@@@@@%%.::%%@%%+*=:-:.-:.....\n"
          "..........+:::.##@@%%*-:::..:-=*#@@@@@@@@@@%%@:*@@@@@@@@#-=:=.....\n"
          ".........::..=%%@@@@@#==:+..+-*@@@@@@@@@@@@@@@@@@@@@@@@@%%*=:.:-..\n"
          "-:=-::::-+*%%@@@@@@@@@#+=-+:#+#@@@@@@@@@@@@@@@@@@@@@@@@@@#=+==-++\n"
          ":++.::-:=*+=%%@@@@@@@@@@+@%%@%%@@@@@@@@@@@@@@@@@@@@@@@@@@@@@**%%##%%%%\n"
          "==-=++**+=-*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%%%%@@@@\n"
          "###@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%@@@@@@@@@@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%=#@@@@@@@@@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#+*@@@@@@@@@@@@@@@@@@@@%%@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#+@@%%%%#*%%@@@@@@@@@@%%@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%###%%@@@@@@@@@@@@@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#*+*#@@@@@@@@@@@@%%@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%=%%@@#%%%%%%%%@@@%%@@@@@@@@@@@@@@@"
          "\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%%%+::*##%%%%@%%%%%%%%%%%%%%@@%%%%@@@@"
          "@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%%*+-::.....:----**#*#%%%%@%%@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@%%**=--=++-:::....:.:.:+*#%%%%%%@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@+-=:::==-:::::....:.:-*##%%%%%%@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@%%+=-=:-:::....::-=*#%%%%@@@@@@@@@@@@@@@@\n"
          "@@@@@@@@@@@@@@@@@@@@@@@@@@%%@%%%%##+=-.-:+++*%%#@@@@@@@@@@@%%@%%@@@@@@"
          "\n");
}

static void FinalizeKernelMapping(mm::PageTable& table)
{
    auto pe = pe::File::FromImageBase(( void* )kva::kernel_image.base);

    pe.ForEachSection([&table](pe::Section* section) -> bool {
        const auto start = kva::kernel_image.base + section->VirtualAddress;
        const auto size = section->Misc.VirtualSize;
        const auto end = start + size;

        char name[pe::section_name_size + 1];
        strlcpy(name, ( const char* )section->Name, pe::section_name_size);

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
    void InitializeCore(mm::PageTable* page_table)
    {
        auto core = new Core();

        core->self = core;
        core->page_table = page_table;

        core->gdt = x64::gdt.data();
        core->idt = x64::idt.data();
        core->tss = &x64::kernel_tss;

        // When going to user mode, swapgs will move core to KERNEL_GS_BASE and 0 to gs.
        // So this looks reversed but it's not.
        WriteMsr(x64::Msr::KERNEL_GS_BASE, 0);
        WriteMsr(x64::Msr::GS_BASE, ( uptr_t )core);
    }
} // namespace ke

int test(u64)
{
    int i = 0;
    while (i < 100)
    {
        Print("thread A %d\n", i++);
        // serial::Write("thread A %d\n", i);
        ke::Delay(100);
    }
    // trigger int 254
    // apic::WriteLocal(apic::LocalReg::ICR1, 0);
    // apic::WriteLocal(apic::LocalReg::ICR0, (1 << 18) | 3);
    return 1;
}

int test2(u64)
{
    int i = 0;
    while (i < 100)
    {
        Print("thread B %llu %d\n", ke::GetCurrentThread()->id, i++);
        ke::Delay(100);
    }
    return 2;
}

static bool MapDeviceUncached(mm::PageTable* page_table, uptr_t* phys_virt, size_t page_count = 1)
{
    if (!mm::MapPagesInRegion<kva::devices>(*page_table, phys_virt, page_count))
    {
        return false;
    }
    while (page_count--)
    {
        auto* pte = mm::GetPresentPte(*page_table, (*phys_virt) + page_count * page_size);
        if (!pte)
        {
            return false;
        }
        pte->cache_disable = true;
    }
    return true;
}

static void PrintContext(const x64::Context& ctx)
{
    Print("rax=%p rbx=%p rcx=%p rdx=%p\n", ctx.rax, ctx.rbx, ctx.rcx, ctx.rdx);
    Print("rsi=%p rdi=%p r8 =%p r9 =%p\n", ctx.rsi, ctx.rdi, ctx.r8, ctx.r9);
    Print("r10=%p r11=%p r12=%p r13=%p\n", ctx.r10, ctx.r11, ctx.r12, ctx.r13);
    Print("r14=%p r15=%p\n", ctx.r14, ctx.r15);
    Print("cs=%lu ss=%lu\n", ctx.cs, ctx.ss);
}

EXTERN_C NO_RETURN void OsInitialize(LoaderBlock* loader_block)
{
    gfx::Initialize(loader_block->display);

    // Copy all the important stuff because the loader block gets freed in
    // ReclaimBootPages
    auto memory_map = loader_block->memory_map;
    auto display = loader_block->display;
    auto kernel = loader_block->kernel;
    auto hpet = loader_block->hpet;
    auto madt = loader_block->madt_header;
    const auto i8042 = loader_block->i8042;
    const auto pt_physical = loader_block->page_table;
    const auto pt_pages = loader_block->page_table_size;
    const auto pp_physical = loader_block->page_pool;
    const auto pp_pages = loader_block->page_pool_size;

    ReclaimBootPages(memory_map);
    CoalesceMemoryDescriptors(memory_map);

    acpi::ParseMadt(madt, x64::cpu_info);

    x64::InitializeStage1(kernel_stack_base);

    // From here on we can use the heap.
    ke::InitializeAllocator();

    // Build a new page table (the bootloader one is temporary)
    auto table = new mm::PageTable(kva::kernel_pt.base, pt_physical, pt_pages);

    const auto kernel_pages = SizeToPages(kernel.size);
    UNUSED const auto frame_buffer_pages = SizeToPages(display.frame_buffer_size);

    mm::MapPages(*table, kva::kernel_image.base, kernel.physical_base, kernel_pages);
    mm::MapPages(*table, kva::kernel_pt.base, pt_physical, pt_pages);

    // turns out vbox page faults at fb base + 0x3000000 when we reach the end so
    // just map the whole range
    mm::MapPagesInRegion<kva::frame_buffer>(*table, &display.frame_buffer, kva::frame_buffer.PageCount());

    // Normal write-back RAM, unlike the frame buffer.
    if (!mm::MapPagesInRegion<kva::back_buffer>(
            *table, &display.back_buffer, SizeToPages(( size_t )display.height * display.pitch)))
        ke::Panic(Status::OutOfMemory);

    // Map the framebuffer as write combining (PAT4)
    for (auto page = kva::frame_buffer.base; page < kva::frame_buffer.End(); page += page_size)
        mm::GetPresentPte(*table, page)->pat = true;

    // Map devices with CD bit set in PTE
    if (hpet)
    {
        if (!MapDeviceUncached(table, &hpet))
            hpet = 0;
    }
    if (x64::cpu_info.using_apic)
        x64::cpu_info.using_apic = MapDeviceUncached(table, &apic::io) && MapDeviceUncached(table, &apic::local);

    MapUefiRuntime(memory_map, *table);

    mm::MapPages(*table, kva::kernel_pool.base, pp_physical, pp_pages);

    // Switch over to kernel tables.
    __writecr3(table->root);

    gfx::SetFrameBufferAddress(display.frame_buffer, display.back_buffer);

    // Set up interrupts and syscalls.
    x64::InitializeStage2();

    // Init COM ports so we have early debugging capabilities.
    serial::Initialize();

    timer::Initialize(hpet);

    if (i8042)
        ps2::Initialize();
    else
        Print("No PS/2 legacy support.\n");

    // PrintSplash();

    // Now that kernel init has completed, zero out discardable sections
    // and write-protect every section not marked writable.
    FinalizeKernelMapping(*table);

    ke::InitializeCore(table);

    x64::unmask_interrupts();

    ke::StartScheduler();

    // ke::CreateThread(&test3, 0);
    ke::CreateThread(&test, 0);
    ke::CreateThread(&test2, 0);
    ke::CreateUserThread(( void* )&x64::Ring3Function, 5);

    // Enter idle loop!
    x64::LoadContext(&ke::GetCore()->idle_thread->context);

    ke::Panic(Status::Unreachable);
}
