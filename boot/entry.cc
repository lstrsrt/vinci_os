#include "boot.h"

#include "../kernel/common/mm.h"
#include "../kernel/common/pe64.h"
#include "../kernel/common/va.h"

#include "cpp-uefi.hh"
#include "mini-libc.h"

#ifdef COMPILER_MSVC
#include <intrin.h>
#else
INLINE void __writecr0(uint64 cr0)
{
    asm volatile("mov %0, %%cr0" ::"r"(cr0) : "memory");
}

INLINE uint64 __readcr0()
{
    uint64 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

INLINE uint64 __readcr3()
{
    uint64 cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

INLINE void _disable()
{
    asm volatile("cli" ::: "cc", "memory");
}

INLINE void __halt()
{
    asm volatile("hlt");
}
#endif

static constexpr uint64 cr0_wp = 1 << 16;

static uefi::simple_text_output_protocol* g_con_out;
static uefi::simple_text_input_protocol* g_con_in;
static bool g_leaving_boot_services;

static void print_string(const char16* fmt, ...)
{
    char16 str[512];
    va_list ap;

    va_start(ap, fmt);
    vwsnprintf(str, ARRAY_SIZE(str) - 1, fmt, ap);
    va_end(ap);

    g_con_out->output_string(str);
}

static uefi::input_key wait_for_key()
{
    uefi::input_key key;
    uefi::status s;

    g_con_in->reset(false);
    do
        s = g_con_in->read_key_stroke(&key);
    while (s == uefi::err_not_ready);

    return key;
}

static uefi::status report_error(uefi::status code, const char16* message = nullptr)
{
    g_con_out->set_attribute(uefi::bg_red | uefi::black);
    if (message)
        print_string(u"An error occurred: %s.\r\n", message);
    else
        print_string(u"An error occurred (0x%llx).\r\n", code);
    g_con_out->set_attribute(uefi::bg_black | uefi::white);

    print_string(u"Press any key to exit.\r\n");
    ( void )wait_for_key();
    return code;
}

#define efi_check_impl(expr, st)         \
    do                                   \
    {                                    \
        uefi::status st = (expr);        \
        if (uefi::is_error(st))          \
        {                                \
            if (g_leaving_boot_services) \
            {                            \
                _disable();              \
                for (;;)                 \
                    __halt();            \
            }                            \
            return report_error(st);     \
        }                                \
    }                                    \
    while (0)

#define efi_check(expr) efi_check_impl(expr, CONCAT(_status, __COUNTER__))

DEBUG_FN static void print_runtime_descriptors(uefi::memory_descriptor* map, uintn map_size, uintn desc_size)
{
    uintn i = 0;
    for (auto current = map; current < uefi_next_memory_descriptor(map, map_size);
        current = uefi_next_memory_descriptor(current, desc_size))
    {
        if (current->virtual_start || current->attribute & uefi::memory_runtime)
        {
            print_string(u"[%llu]: Type: %d   PA: 0x%p   VA: 0x%p (pages: %llu) Attr 0x%llx\r\n", i++, current->type,
                current->physical_start, current->virtual_start, current->number_of_pages, current->attribute);
        }
    }
}

static uefi::status zero_allocate_pages(
    uefi::allocation_type type, uefi::memory_type mem_type, uintn pages, physical_address* memory)
{
    auto s = g_bs->allocate_pages(type, mem_type, pages, memory);

    if (s == uefi::success)
        memzero(( void* )*memory, pages * uefi::page_size);

    return s;
}

static uefi::status get_protocol(handle h, uefi::guid* protocol, void** interface_, handle image_handle)
{
    return g_bs->open_protocol(h, protocol, interface_, image_handle, nullptr, uefi::open_protocol_get_protocol);
}

static uefi::status load_kernel_executable(uefi::file* file, LoaderBlock* loader_block)
{
    pe::DosHeader dos;
    uintn size = sizeof dos;
    efi_check(file->read(&size, &dos));

    if (!dos.IsValid())
        return report_error(uefi::err_unsupported, u"Kernel DOS header invalid");

    pe::NtHeaders nt_header;
    size = sizeof nt_header;
    efi_check(file->set_position(( uint64 )dos.e_lfanew));
    efi_check(file->read(&size, &nt_header));

    if (!nt_header.IsValid())
        return report_error(uefi::err_unsupported, u"Kernel NT headers invalid");

    if (nt_header.FileHeader.Machine != pe::Machine::x64)
        return report_error(uefi::err_unsupported, u"Kernel arch unsupported");

    // Allocate pages for the image.
    // These must be zeroed: anything past a section's raw data (e.g. .bss) is never read from the file.
    auto& physical_base = loader_block->kernel.physical_base;
    efi_check(zero_allocate_pages(uefi::allocation_type::any_pages, uefi::memory_type::loader_data,
        uefi::size_to_pages(nt_header.OptionalHeader.SizeOfImage), &physical_base));

    // Set size so headers can be read next
    size = nt_header.OptionalHeader.SizeOfHeaders;

    // First read headers into memory at physical_base
    efi_check(file->set_position(0));
    efi_check(file->read(&size, ( void* )physical_base));
    auto nt = ( pe::NtHeaders* )(physical_base + dos.e_lfanew);

    // Do another check, this time from memory
    if (!nt->IsValid())
        return report_error(uefi::err_unsupported, u"Kernel NT headers invalid");

    const auto entry_point = nt->OptionalHeader.AddressOfEntryPoint;
    const auto image_size = nt->OptionalHeader.SizeOfImage;

    print_string(u"Image base (physical): 0x%p\r\n", physical_base);
    print_string(u"Image size: 0x%x\r\n", image_size);
    print_string(u"Entry point (physical): 0x%p\r\n", physical_base + entry_point);

    // Then read sections
    auto section = GetFirstSection(nt);
    for (uint16 i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        physical_address section_dest = physical_base + section[i].VirtualAddress;
        if (uintn raw_size = section[i].SizeOfRawData)
        {
            efi_check(file->set_position(section[i].PointerToRawData));
            efi_check(file->read(&raw_size, ( void* )section_dest));
        }
    }

    // Set image base to our virtual address for later mapping
    nt->OptionalHeader.ImageBase = kva::kernel_image.base;

    loader_block->kernel.size = image_size;
    loader_block->kernel.entry_point = nt->OptionalHeader.ImageBase + entry_point;

    return uefi::success;
}

static bool acpi_validate_checksum(acpi::DescriptionHeader* header)
{
    uint8 sum = 0;
    for (uint32 i = 0; i < header->Length; i++)
        sum += (( uint8* )header)[i];
    return sum == 0;
}

static acpi::Xsdt* locate_xsdt()
{
    uefi::guid xsdt_guid = uefi::table::acpi20;

    for (uintn i = 0; i < g_st->number_of_table_entries; i++)
    {
        auto table = &g_st->configuration_table[i];
        if (!memcmp(&table->vendor_guid, &xsdt_guid, sizeof(uefi::guid)))
        {
            auto entry = ( acpi::Rsdp* )table->vendor_table;
            auto xsdt = ( acpi::Xsdt* )entry->XsdtAddress;

            if (xsdt->Header.Signature == acpi::signature::xsdt && acpi_validate_checksum(&xsdt->Header))
            {
                print_string(u"ACPI: Found XSDT\r\n");
                return xsdt;
            }

            return nullptr;
        }
    }

    return nullptr;
}

static uefi::status acpi_initialize(acpi::Xsdt* xsdt, LoaderBlock* loader_block)
{
    bool found_fadt = false;
    auto table_count = (xsdt->Header.Length - sizeof xsdt->Header) / sizeof(uint64);

    for (uint64 i = 0; i < table_count; i++)
    {
        auto header = ( acpi::DescriptionHeader* )xsdt->Tables[i];
        switch (header->Signature)
        {
        case acpi::signature::madt: {
            print_string(u"ACPI: Found MADT at 0x%p\r\n", header);
            loader_block->madt_header = ( acpi::Madt* )header;
            break;
        }
        case acpi::signature::fadt: {
            found_fadt = true;
            print_string(u"ACPI: Found FADT (revision %d)\r\n", header->Revision);

            // This is the only time the ACPI version is of interest to us.
            // If Revision is < 2, the system is too old to not support i8042.
            // Otherwise, IaPcBootArch exists and can be checked.

            constexpr auto iapc_boot_arch_end = OFFSET(acpi::Fadt, IaPcBootArch) + sizeof acpi::Fadt::IaPcBootArch;
            if (header->Revision < 2 || header->Length < iapc_boot_arch_end)
                loader_block->i8042 = true;
            else
                loader_block->i8042 = (( acpi::Fadt* )header)->IaPcBootArch & ACPI_FADT_8042;
            print_string(u"Legacy 8042 support: %d\r\n", loader_block->i8042);
            break;
        }
        case acpi::signature::hpet: {
            print_string(u"ACPI: Found HPET\r\n");
            auto hpet = ( acpi::Hpet* )header;
            loader_block->hpet = hpet->BaseAddress.Address;
            break;
        }
        default: {
            char16 signature[5];
            for (int i = 0; i < 4; ++i)
                signature[i] = (( const char* )&header->Signature)[i];
            signature[4] = '\0';
            print_string(u"ACPI: Found %s (ignored)\r\n", signature);
            break;
        }
        }
    }

    // MADT and FADT have to exist
    // TODO - if HPET not found, PIT must be supported
    if (!loader_block->madt_header || !found_fadt)
        return uefi::err_unsupported;

    return uefi::success;
}

static void run_cxx_initializers(vaddr_t image_base)
{
    using PFVF = void(__cdecl*)();

    auto dos_header = ( pe::DosHeader* )image_base;
    auto nt_header = ( pe::NtHeaders* )(image_base + dos_header->e_lfanew);

    auto section = GetFirstSection(nt_header);
    for (uint16 i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        if (!strncmp(( const char* )section[i].Name, ".CRT", 4))
        {
            auto crt = &section[i];
            auto initializer = ( PFVF* )(image_base + crt->VirtualAddress);
            while (*initializer)
            {
                (*initializer)();
                initializer++;
            }
        }
    }
}

EXTERN_C uefi::status EfiMain(uefi::handle image_handle, uefi::system_table* sys_table)
{
    uefi::initialize_lib(sys_table);
    g_con_out = sys_table->con_out;
    g_con_in = sys_table->con_in;

    // FIXME: This is not enough when booting from USB on real hardware
    g_con_out->clear_screen();
    g_con_out->set_attribute(uefi::bg_black | uefi::white);

    // This gives the user infinite time to respond to a key press request
    g_bs->set_watchdog_timer(0, 0, 0, nullptr);

    // Allocate the loader block passed to the kernel.
    // Type is boot services data so it can be reused later
    LoaderBlock* loader_block = nullptr;
    efi_check(zero_allocate_pages(uefi::allocation_type::any_pages, uefi::memory_type::boot_services_data,
        uefi::size_to_pages(sizeof(LoaderBlock)), ( physical_address* )&loader_block));

    loader_block->config_table = g_st->configuration_table;
    loader_block->config_table_entries = g_st->number_of_table_entries;

    uefi::loaded_image_protocol* loaded_image_protocol;
    uefi::guid loaded_image_guid = uefi::protocol::loaded_image;
    efi_check(get_protocol(image_handle, &loaded_image_guid, ( void** )&loaded_image_protocol, image_handle));

    // Allocate boot loader page tables
    physical_address bl_page_table;
    static constexpr auto bl_page_pool_pages = uefi::size_to_pages(MiB(2));

    efi_check(zero_allocate_pages(
        uefi::allocation_type::any_pages, uefi::memory_type::boot_services_data, bl_page_pool_pages, &bl_page_table));

    // Allocate kernel page tables
    loader_block->page_table_size = kva::kernel_pt.PageCount();
    efi_check(zero_allocate_pages(uefi::allocation_type::any_pages, uefi::memory_type::loader_data,
        loader_block->page_table_size, &loader_block->page_table));

    // Allocate kernel page pool
    static constexpr auto kernel_page_pool_pages = kva::kernel_pool.PageCount();
    loader_block->page_pool_size = kernel_page_pool_pages;
    efi_check(zero_allocate_pages(uefi::allocation_type::any_pages, uefi::memory_type::loader_data,
        loader_block->page_pool_size, &loader_block->page_pool));

    // Get file system interface and current drive root
    uefi::simple_file_system_protocol* fs_protocol;
    auto fs_guid = uefi::protocol::simple_file_system;
    efi_check(get_protocol(loaded_image_protocol->device_handle, &fs_guid, ( void** )&fs_protocol, image_handle));

    uefi::file_protocol* drive;
    efi_check(fs_protocol->open_volume(&drive));

    static constexpr char16 kernel_path[] = u"\\EFI\\BOOT\\kernel.exe";

    print_string(u"Loading kernel from ");
    g_con_out->set_attribute(uefi::light_cyan);
    print_string(u"%s\r\n", kernel_path);
    g_con_out->set_attribute(uefi::white);
#ifdef _RELEASE
    print_string(u"Press Q to quit or any other key to continue.\r\n");
    if (wait_for_key().unicode_char == u'q')
        return uefi::err_aborted;
#endif

    uefi::file* kernel_file;
    efi_check(drive->open(&kernel_file, ( char16* )kernel_path, uefi::file_mode_read, uefi::file_read_only));

    if (auto s = load_kernel_executable(kernel_file, loader_block); s != uefi::success)
    {
        // Errors are reported in the function already so no need for efi_check
        return s;
    }

    auto xsdt = locate_xsdt();
    if (!xsdt)
        return report_error(uefi::err_unsupported, u"XSDT not found");

    efi_check(acpi_initialize(xsdt, loader_block));

    // Set up graphics
    uefi::graphics_output_protocol* graphics_protocol;
    uefi::guid graphics_guid = uefi::protocol::graphics_output;
    efi_check(g_bs->locate_protocol(&graphics_guid, nullptr, ( void** )&graphics_protocol));

    auto graphics_mode_info = graphics_protocol->mode->info;
    if (graphics_mode_info->pixel_format != uefi::pixel_fmt_bgr_reserved_8bit_per_color)
        return report_error(uefi::err_unsupported, u"Unsupported pixel format");

    print_string(u"Frame buffer base: 0x%p size: 0x%llx\r\n", graphics_protocol->mode->frame_buffer_base,
        graphics_protocol->mode->frame_buffer_size);
    print_string(
        u"Resolution: %u x %u\r\n", graphics_mode_info->horizontal_resolution, graphics_mode_info->vertical_resolution);

    loader_block->display.frame_buffer = graphics_protocol->mode->frame_buffer_base;
    loader_block->display.frame_buffer_size = graphics_protocol->mode->frame_buffer_size;
    loader_block->display.width = graphics_mode_info->horizontal_resolution;
    loader_block->display.height = graphics_mode_info->vertical_resolution;
    loader_block->display.pitch = graphics_mode_info->pixels_per_scanline * 4; // depends on PixelFormat

    // The kernel console draws here and copies to the frame buffer, which is slow to read from.
    const auto back_buffer_pages
        = uefi::size_to_pages(( uintn )loader_block->display.height * loader_block->display.pitch);
    efi_check(zero_allocate_pages(uefi::allocation_type::any_pages, uefi::memory_type::loader_data,
        back_buffer_pages, &loader_block->display.back_buffer));

    //
    // Here we create a temporary higher half mapping for the kernel to jump to.
    // See va.h for the addresses used
    //

    // Clear WP flag to allow rewriting page tables
    __writecr0(__readcr0() & ~cr0_wp);

    // Everything is identity mapped, so virtual and physical bases for the page table are the same.
    mm::PageTable table(bl_page_table, bl_page_table, bl_page_pool_pages,
        __readcr3() // Use UEFI page tables because we still need the boot services etc.
    );

    const auto pml4 = ( x64::Pml4 )GetPoolEntryVa(table, table.root);
    // Clear out PML4 entries starting from higher half
    for (uint32 i = 256; i < 512; i++)
        pml4[i].value = 0;

    const auto kernel_pages = uefi::size_to_pages(loader_block->kernel.size);
    mm::MapPages(table, kva::kernel_image.base, loader_block->kernel.physical_base, kernel_pages);
    mm::MapPages(table, kva::kernel_pt.base, loader_block->page_table, loader_block->page_table_size);
    mm::MapPages(table, kva::kernel_pool.base, loader_block->page_pool, loader_block->page_pool_size);

    __writecr0(__readcr0() | cr0_wp);

    // Give user time to read everything
    print_string(u"Press any key to continue...\r\n");
    ( void )wait_for_key();

    // It is now safe to close open handles
    efi_check(kernel_file->close());
    efi_check(drive->close());
    efi_check(g_bs->close_protocol(image_handle, &loaded_image_guid, image_handle, nullptr));
    efi_check(g_bs->close_protocol(loaded_image_protocol->device_handle, &fs_guid, image_handle, nullptr));

    // Get memory map
    // First call is just to find out the required size
    uint32 desc_ver = 0;
    uintn desc_size = 0, map_size = 0, map_key = 0;
    uefi::memory_descriptor* map = nullptr;
    if (auto s = g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver); s != uefi::err_buffer_too_small)
        return report_error(s);

    // Allocating the buffer can split a free region and add descriptors,
    // so leave room for a few extra. The same buffer is reused if exit_boot_services fails.
    const uintn map_capacity = map_size + 4 * desc_size;
    efi_check(g_bs->allocate_pool(uefi::memory_type::loader_data, map_capacity, ( void** )&map));
    map_size = map_capacity;
    efi_check(g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver));

    // Execute kernel .CRT* functions
    run_cxx_initializers(kva::kernel_image.base);

    // Exit boot services
    g_leaving_boot_services = true;
    if (g_bs->exit_boot_services(image_handle, map_key) != uefi::success)
    {
        // The first call may fail if the map changed since we read it.
        // In this case, read it again into the existing buffer and retry.
        // Reallocating here would change the map again.
        map_size = map_capacity;
        efi_check(g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver));
        efi_check(g_bs->exit_boot_services(image_handle, map_key));
    }

    // Set runtime memory virtual addresses
    for (auto desc = map; desc < uefi_next_memory_descriptor(map, map_size);
        desc = uefi_next_memory_descriptor(desc, desc_size))
    {
        if (desc->attribute & uefi::memory_runtime)
            desc->virtual_start = desc->physical_start + kva::uefi.base;
    }

    // Update UEFI virtual address map.
    // If this fails we can't print because we already exited boot services :(
    efi_check(g_rt->set_virtual_address_map(map_size, desc_size, desc_ver, map));

    loader_block->memory_map.map = map;
    loader_block->memory_map.size = map_size;
    loader_block->memory_map.descriptor_size = desc_size;
    loader_block->memory_map.descriptor_version = desc_ver;

    // Call the kernel. This should not return.
    using Entry = void(__cdecl*)(LoaderBlock*);
    auto entry = ( Entry )loader_block->kernel.entry_point;
    entry(loader_block);

    return uefi::err_load_error;
}
