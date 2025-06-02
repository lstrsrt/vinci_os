#include "boot.h"

#include "../kernel/common/pe64.h"
#include "../kernel/common/va.h"
#include "../kernel/common/mm.h"

#include "mini-libc.h"
#include "cpp-uefi.hh"

#ifdef COMPILER_MSVC
#include <intrin.h>
#else
INLINE void __writecr0(u64 cr0)
{
    asm volatile("mov %%rax, %%cr0" :: "a"(cr0));
}

INLINE u64 __readcr0()
{
    u64 cr0;
    asm("mov %%cr0, %%rax" : "=a"(cr0));
    return cr0;
}

INLINE u64 __readcr3()
{
    u64 cr3;
    asm("mov %%cr3, %%rax" : "=a"(cr3));
    return cr3;
}
#endif

static uefi::simple_text_output_protocol* g_con_out;
static uefi::simple_text_input_protocol* g_con_in;
static bool g_leaving_boot_services;

static void print_string(const char16_t* fmt, ...)
{
    char16_t str[512];
    va_list ap;

    va_start(ap, fmt);
    size_t len = vwsnprintf(str, sizeof str, fmt, ap);
    va_end(ap);

    str[len] = '\0';

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

static void report_error(uefi::status code)
{
    g_con_out->set_attribute(uefi::bg_red | uefi::black);
    print_string(u"An error occurred (0x%llx).\r\n", code);
    g_con_out->set_attribute(uefi::bg_black | uefi::white);

    print_string(u"Press any key to exit.\r\n");
    ( void )wait_for_key();
}

#define efi_check(expr) \
    { uefi::status x = (expr); \
    if (uefi::is_error(x)) { if (!g_leaving_boot_services) { report_error(x); } return x; } }

DEBUG_FN static void print_runtime_descriptors(uefi::memory_descriptor* map, uintn map_size, uintn desc_size)
{
    uintn i = 0;
    for (auto current = map;
        current < uefi_next_memory_descriptor(map, map_size);
        current = uefi_next_memory_descriptor(current, desc_size))
    {
        if (current->virtual_start || current->attribute & uefi::memory_runtime)
        {
            print_string(
                u"[%llu]: Type: %d   PA: 0x%p   VA: 0x%p (pages: %llu) Attr 0x%llx\r\n",
                i++,
                current->type,
                current->physical_start,
                current->virtual_start,
                current->number_of_pages,
                current->attribute
            );
        }
    }
}

static uefi::status zero_allocate_pages(
    uefi::allocation_type type,
    uefi::memory_type mem_type,
    uintn pages,
    physical_address* memory
)
{
    auto s = g_bs->allocate_pages(type, mem_type, pages, memory);

    if (s == uefi::success)
        memzero(( void* )*memory, pages * uefi::page_size);

    return s;
}

static uefi::status get_protocol(handle h, uefi::guid* protocol, void** interface_, handle image_handle)
{
    return g_bs->open_protocol(
        h,
        protocol,
        interface_,
        image_handle,
        nullptr,
        uefi::open_protocol_get_protocol
    );
}

static uefi::status load_kernel_executable(uefi::file* file, LoaderBlock* loader_block)
{
    if (!file)
        return uefi::err_invalid_parameter;

    // Read DOS header
    pe::DosHeader dos;
    uintn size = sizeof dos;
    efi_check(file->read(&size, &dos));

    if (!dos.IsValid())
    {
        print_string(u"Kernel DOS header invalid\r\n");
        return uefi::err_unsupported;
    }

    // Read NT headers
    pe::NtHeaders nt_header;
    size = sizeof nt_header;
    efi_check(file->set_position(( uint64 )dos.e_lfanew));
    efi_check(file->read(&size, &nt_header));

    if (!nt_header.IsValid())
    {
        print_string(u"Kernel NT headers invalid\r\n");
        return uefi::err_unsupported;
    }

    if (nt_header.FileHeader.Machine != pe::Machine::x64)
    {
        print_string(u"Kernel arch unsupported\r\n");
        return uefi::err_unsupported;
    }

    // Allocate pages for the image
    auto& physical_base = loader_block->kernel.physical_base;
    efi_check(
        g_bs->allocate_pages(
            uefi::allocation_type::any_pages,
            uefi::memory_type::loader_data,
            uefi::size_to_pages(nt_header.OptionalHeader.SizeOfImage),
            &physical_base
        )
    );

    // Set size so headers can be read next
    size = nt_header.OptionalHeader.SizeOfHeaders;

    // First read headers into memory at physical_base
    efi_check(file->set_position(0));
    efi_check(file->read(&size, ( void* )physical_base));
    auto nt = ( pe::NtHeaders* )(physical_base + dos.e_lfanew);

    // Do another check, this time from memory
    if (!nt->IsValid())
    {
        print_string(u"Kernel NT headers invalid\r\n");
        return uefi::err_unsupported;
    }

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
    
    /*pe::File file(&dos, nt);
    file.ForEachSection([](pe::Section* section)
    {
        physical_address section_dest = physical_base + section[i].VirtualAddress;
        if (uintn raw_size = section[i].SizeOfRawData)
        {
            efi_check(file->set_position(section[i].PointerToRawData));
            efi_check(file->read(&raw_size, ( void* )section_dest));
        }
    });*/

    // Set image base to our virtual address for later mapping
    nt->OptionalHeader.ImageBase = kva::kernel_image.base;

    loader_block->kernel.size = image_size;
    loader_block->kernel.entry_point = nt->OptionalHeader.ImageBase + entry_point;

    return uefi::success;
}

static uefi::boolean acpi_validate_checksum(acpi::DescriptionHeader* header)
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

            if (xsdt->Header.Signature == acpi::signature::xsdt
                && acpi_validate_checksum(&xsdt->Header))
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
    loader_block->madt_header = nullptr;
    bool found_fadt = false;
    auto table_count = (xsdt->Header.Length - sizeof xsdt->Header) / sizeof(uint64);

    for (uint64 i = 0; i < table_count; i++)
    {
        auto header = ( acpi::DescriptionHeader* )xsdt->Tables[i];
        switch (header->Signature)
        {
        case acpi::signature::madt:
        {
            // The MADT is parsed in the kernel because it interacts with APIC initialization
            print_string(u"ACPI: Found MADT\r\n");
            auto madt_header = ( acpi::Madt* )header;

            // Needs to be 4k aligned and multiple of 4k in size
            efi_check(
                g_bs->allocate_pages(
                uefi::allocation_type::any_pages,
                uefi::memory_type::acpi_memory_nvs,
                1,
                ( uefi::physical_address* )&loader_block->madt_header)
            );
            g_bs->copy_mem(loader_block->madt_header, madt_header, madt_header->Header.Length);

            print_string(u"MADT allocated at 0x%p\r\n", loader_block->madt_header);
            break;
        }
        case acpi::signature::fadt:
        {
            found_fadt = true;
            print_string(u"ACPI: Found FADT (revision %d)\r\n", header->Revision);

            // This is the only time the ACPI version is of interest to us.
            // If Revision is < 2, the system is too old to not support i8042.
            // Otherwise, IaPcBootArch exists and can be checked.

            if (!(loader_block->i8042 = header->Revision < 2))
            {
                auto fadt = ( acpi::Fadt* )header;
                loader_block->i8042 = (fadt->IaPcBootArch & ACPI_FADT_8042) ? TRUE : FALSE;
            }
            print_string(u"Legacy 8042 support: %d\r\n", loader_block->i8042);
            break;
        }
        case acpi::signature::hpet:
        {
            print_string(u"ACPI: Found HPET\r\n");
            auto hpet = ( acpi::Hpet* )header;
            loader_block->hpet = hpet->BaseAddress.Address;
            break;
        }
        default:
        {
            char8 signature_u8[4];
            char16 signature[5];
            memcpy(( u8* )signature_u8, ( const u8* )&header->Signature, sizeof(uint32));

            if (utf8_to_ucs2(
                signature,
                ARRAY_SIZE(signature),
                signature_u8,
                ARRAY_SIZE(signature_u8))
            )
            {
                signature[4] = u'\0';
                print_string(u"ACPI: Found %s (ignored)\r\n", signature);
            }
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

    // There may be multiple CRT sections (?)
    auto section = GetFirstSection(nt_header);
    for (uint16 i = 0; i < nt_header->FileHeader.NumberOfSections; i++)
    {
        char8 name[pe::section_name_size + 1];
        memcpy(( u8* )name, ( const u8* )section[i].Name, pe::section_name_size);

        if (!strncmp(( char* )name, ".CRT", 4))
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

extern "C" uefi::status EfiMain(uefi::handle image_handle, uefi::system_table* sys_table)
{
    uefi::initialize_lib(sys_table);
    g_con_out = sys_table->con_out;
    g_con_in = sys_table->con_in;

    // Clear the screen
    // FIXME: This is not enough when booting from USB on real hardware
    g_con_out->clear_screen();
    g_con_out->set_attribute(uefi::bg_black | uefi::white);

    // This gives the user infinite time to respond to a key press request
    g_bs->set_watchdog_timer(0, 0, 0, nullptr);

    // Allocate the loader block passed to the kernel.
    // Type is boot services data so it can be reused later
    LoaderBlock* loader_block = nullptr;
    efi_check(
        zero_allocate_pages(
            uefi::allocation_type::any_pages,
            uefi::memory_type::boot_services_data,
            uefi::size_to_pages(sizeof(LoaderBlock)), // Should be one page (due to alignment requirements)
            ( physical_address* )&loader_block
        )
    );

    loader_block->config_table = g_st->configuration_table;
    loader_block->config_table_entries = g_st->number_of_table_entries;

    // Get the image data type
    uefi::loaded_image_protocol* loaded_image_protocol;
    uefi::guid loaded_image_guid = uefi::protocol::loaded_image;
    efi_check(get_protocol(image_handle, &loaded_image_guid, ( void** )&loaded_image_protocol, image_handle));

    // Allocate path to our kernel image
    char16_t* kernel_path;
    static constexpr auto path = u"\\EFI\\BOOT\\kernel.exe";
    auto alloc_type = loaded_image_protocol->image_data_type;
    efi_check(g_bs->allocate_pool(alloc_type, 256 * sizeof(char16_t), ( void** )&kernel_path));
    strcpy16(kernel_path, path);

    // Allocate boot loader page tables
    uefi::physical_address bl_page_table;
    static constexpr auto bl_page_pool_pages = uefi::size_to_pages(MiB(2));

    efi_check(
        zero_allocate_pages(
            uefi::allocation_type::any_pages,
            uefi::memory_type::boot_services_data,
            bl_page_pool_pages,
            &bl_page_table
        )
    );

    // Allocate kernel page tables
    // TODO - what is a good size for this?
    static constexpr auto kernel_page_table_pages = uefi::size_to_pages(MiB(16));
    loader_block->page_table_size = kernel_page_table_pages;

    efi_check(
        zero_allocate_pages(
            uefi::allocation_type::any_pages,
            alloc_type,
            loader_block->page_table_size,
            &loader_block->page_table
        )
    );

    // Allocate kernel page pool
    static constexpr auto kernel_page_pool_pages = kva::kernel_pool.PageCount();
    loader_block->page_pool_size = kernel_page_pool_pages;

    efi_check(
        zero_allocate_pages(
            uefi::allocation_type::any_pages,
            alloc_type,
            loader_block->page_pool_size,
            &loader_block->page_pool
        )
    );

    // Get file system interface and current drive root
    uefi::simple_file_system_protocol* fs_protocol;
    auto fs_guid = uefi::protocol::simple_file_system;
    efi_check(get_protocol(loaded_image_protocol->device_handle, &fs_guid, ( void** )&fs_protocol, image_handle));

    uefi::file_protocol* drive;
    efi_check(fs_protocol->open_volume(&drive));

    print_string(u"Loading kernel from ");
    g_con_out->set_attribute(uefi::light_cyan);
    print_string(u"%s\r\n", kernel_path);
    g_con_out->set_attribute(uefi::white);
#ifdef _RELEASE
    print_string(u"Press Q to quit or any other key to continue.\r\n");
    if (wait_for_key() == 'q')
        return uefi::err_aborted;
#endif

    uefi::file* kernel_file;
    efi_check(
        drive->open(
            &kernel_file,
            ( char16* )kernel_path,
            uefi::file_mode_read,
            uefi::file_read_only
        )
    );

    // Load the executable into memory and store details in the loader block
    if (auto s = load_kernel_executable(kernel_file, loader_block);
        s != uefi::success)
    {
        // Errors are reported in the function already so no need for efi_check
        return s;
    }

    // Do ACPI initialization
    auto xsdt = locate_xsdt();
    if (!xsdt)
    {
        report_error(uefi::err_unsupported);
        return uefi::err_unsupported;
    }

    efi_check(acpi_initialize(xsdt, loader_block));

    // Set up graphics
    uefi::graphics_output_protocol* graphics_protocol;
    uefi::guid graphics_guid = uefi::protocol::graphics_output;
    efi_check(g_bs->locate_protocol(&graphics_guid, nullptr, ( void** )&graphics_protocol));

    // TODO - free graphics_mode_info?
    uefi::graphics_output_mode_information* graphics_mode_info;
    uintn size;
    // TODO - which mode to use?
    efi_check(graphics_protocol->query_mode(graphics_protocol->mode->mode_number, &size, &graphics_mode_info));

    // print_string(u"Frame buffer base: 0x%p\r\n", graphics_protocol->Mode->FrameBufferBase);
    // print_string(u"Frame buffer size: 0x%p\r\n", graphics_protocol->Mode->FrameBufferSize);
    print_string(u"Resolution: %u x %u\r\n",
        graphics_mode_info->horizontal_resolution, graphics_mode_info->vertical_resolution);

    loader_block->display.frame_buffer = graphics_protocol->mode->frame_buffer_base;
    loader_block->display.frame_buffer_size = graphics_protocol->mode->frame_buffer_size;
    loader_block->display.width = graphics_mode_info->horizontal_resolution;
    loader_block->display.height = graphics_mode_info->vertical_resolution;
    loader_block->display.pitch = graphics_mode_info->pixels_per_scanline * 4; // depends on PixelFormat

    if (graphics_mode_info->pixel_format != uefi::pixel_fmt_bgr_reserved_8bit_per_color)
    {
        report_error(uefi::err_unsupported);
        return uefi::err_unsupported;
    }

    //
    // Here we create a temporary higher half mapping for the kernel to jump to.
    // See va.h for the addresses used
    //

#define CR0_WP (1 << 16)

    // Clear WP flag to allow rewriting page tables
    __writecr0(__readcr0() & ~CR0_WP);

    // Everything is identity mapped, so virtual and physical bases for the page table are the same.
    mm::PageTable table(
        bl_page_table,
        bl_page_table,
        bl_page_pool_pages,
        __readcr3() // Use UEFI page tables because we still need the boot services etc.
    );

    const auto pml4 = ( x64::Pml4 )GetPoolEntryVa(table, table.root);
    // Clear out PML4 entries starting from higher half
    for (u32 i = 256; i < 512; i++)
        pml4[i].value = 0;

    const auto kernel_pages = uefi::size_to_pages(loader_block->kernel.size);
    mm::MapPages(table, kva::kernel_image.base, loader_block->kernel.physical_base, kernel_pages);
    mm::MapPages(table, kva::kernel_pt.base, loader_block->page_table, loader_block->page_table_size);
    mm::MapPages(table, kva::kernel_pool.base, loader_block->page_pool, loader_block->page_pool_size);

    // Set WP flag
    __writecr0(__readcr0() | CR0_WP);

    // Give user time to read everything
    print_string(u"Press any key to continue...\r\n");
    ( void )wait_for_key();

    // It is now safe to free the path string and close open handles
    efi_check(g_bs->free_pool(kernel_path));
    efi_check(kernel_file->close());
    efi_check(drive->close());
    efi_check(g_bs->close_protocol(image_handle, &loaded_image_guid, image_handle, nullptr));
    efi_check(g_bs->close_protocol(loaded_image_protocol->device_handle, &fs_guid, image_handle, nullptr));

    // Get memory map
    // First call is just to find out the required size
    uint32 desc_ver = 0;
    uintn desc_size = 0, map_size = 0, map_key = 0;
    uefi::memory_descriptor* map = nullptr;
    if (auto s = g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver);
        s != uefi::err_buffer_too_small)
    {
        report_error(s);
        return uefi::err_load_error;
    }

    map_size += desc_size;
    efi_check(g_bs->allocate_pool(uefi::memory_type::loader_data, map_size, ( void** )&map));
    efi_check(g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver));

    // Execute kernel .CRT* functions
    run_cxx_initializers(kva::kernel_image.base);

    // Exit boot services
    g_leaving_boot_services = true;
    if (g_bs->exit_boot_services(image_handle, map_key) != uefi::success)
    {
        // The first call may fail
        // In this case, we have to get the memory map again
        efi_check(g_bs->free_pool(map));
        map_size = 0;

        if (auto s = g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver);
            s == uefi::err_buffer_too_small)
        {
            efi_check(g_bs->allocate_pool(uefi::memory_type::loader_data, map_size, ( void** )&map));
            efi_check(g_bs->get_memory_map(&map_size, map, &map_key, &desc_size, &desc_ver));
        }
        else if (s != uefi::success)
        {
            return s;
        }

        efi_check(g_bs->exit_boot_services(image_handle, map_key));
    }

    // Set runtime memory virtual addresses
    for (auto desc = map;
        desc < uefi_next_memory_descriptor(map, map_size);
        desc = uefi_next_memory_descriptor(desc, desc_size))
    {
        if (desc->attribute & uefi::memory_runtime)
            desc->virtual_start = desc->physical_start + kva::uefi.base;
    }

    // Update UEFI virtual address map
    efi_check(g_rt->set_virtual_address_map(map_size, desc_size, desc_ver, map));

    // Finalize the loader block
    loader_block->memory_map.map = map;
    loader_block->memory_map.size = map_size;
    loader_block->memory_map.descriptor_size = desc_size;
    loader_block->memory_map.descriptor_version = desc_ver;

    // Call the kernel. This should not return.
    using Entry = void(__cdecl*)(LoaderBlock*);
    auto entry = ( Entry )loader_block->kernel.entry_point;
    entry(loader_block);

    // If we do get here, return an error
    return uefi::err_load_error;
}
