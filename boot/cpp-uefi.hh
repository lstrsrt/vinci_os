#pragma once

#include <stdint.h>

#if !defined(__clang__) && defined(_MSC_VER)
#ifndef EFIAPI
#define EFIAPI __cdecl
#endif
#else
#ifndef EFIAPI
#define EFIAPI __attribute__((ms_abi))
#endif
#endif

#if !defined(__clang__) && defined(_MSC_VER)
#define OPTIONAL_INSERT(...) __VA_ARGS__
#define OPTIONAL_INSERT_COMMA(...) , __VA_ARGS__
#else
#define OPTIONAL_INSERT(...) __VA_OPT__(__VA_ARGS__)
#define OPTIONAL_INSERT_COMMA(...) __VA_OPT__(, __VA_ARGS__)
#endif

#define THISPTR_FN(ret, name, args, ...) using name##_t = ret(EFIAPI*)(void* thisptr OPTIONAL_INSERT_COMMA(__VA_ARGS__)); \
    private: name##_t _##name; \
    public: inline auto name(OPTIONAL_INSERT(__VA_ARGS__)) { return _##name args; }

#define MEMBER_FN(ret, name, ...) using name##_t = ret(EFIAPI*)(OPTIONAL_INSERT(__VA_ARGS__)); \
    public: name##_t name

namespace uefi {

    inline namespace types {

        using boolean = bool;

        using int8 = int8_t;
        using int16 = int16_t;
        using int32 = int32_t;
        using int64 = int64_t;
        using uint8 = uint8_t;
        using uint16 = uint16_t;
        using uint32 = uint32_t;
        using uint64 = uint64_t;
        using intn = intptr_t;
        using uintn = uintptr_t;

        using char8 = char8_t;
        using char16 = char16_t;

        using physical_address = uint64;
        using virtual_address = uint64;
        using handle = void*;
        using event = void*;
        using lba = uint64;

    }

#pragma region basic_constants
    namespace signature
    {
        static constexpr uint64 system_table = 0x5453595320494249;
        static constexpr uint64 boot_services = 0x56524553544f4f42;
    }

    namespace revision
    {
        static constexpr uint32 system_table_2_100 = (2 << 16) | 100;
        static constexpr uint32 system_table_2_90 = (2 << 16) | 90;
        static constexpr uint32 system_table_2_80 = (2 << 16) | 80;
        static constexpr uint32 system_table_2_70 = (2 << 16) | 70;
        static constexpr uint32 system_table_2_60 = (2 << 16) | 60;
        static constexpr uint32 system_table_2_50 = (2 << 16) | 50;
        static constexpr uint32 system_table_2_40 = (2 << 16) | 40;
        static constexpr uint32 system_table_2_31 = (2 << 16) | 31;
        static constexpr uint32 system_table_2_30 = (2 << 16) | 30;
        static constexpr uint32 system_table_2_20 = (2 << 16) | 20;
        static constexpr uint32 system_table_2_10 = (2 << 16) | 10;
        static constexpr uint32 system_table_2_00 = (2 << 16) | 00;
        static constexpr uint32 system_table_1_10 = (1 << 16) | 10;
        static constexpr uint32 system_table_1_02 = (1 << 16) | 02;
        static constexpr uint32 system_table = system_table_2_100;

        static constexpr uint32 specification_version = system_table;
        static constexpr uint32 boot_services = specification_version;

        /* Some revision fields are 64 bit */
        static constexpr uint64 file_system = 0x10000;
        static constexpr uint64 file_system2 = 0x20000;
        static constexpr uint64 file_io = file_system2;

        static constexpr uint32 loaded_image = 0x1000;
        static constexpr uint32 image_information = loaded_image;
    }
#pragma endregion

#pragma region guids
    struct guid
    {
        uint32 data1;
        uint16 data2;
        uint16 data3;
        uint8 data4[8];
    };

    namespace protocol
    {
        static constexpr guid simple_text_output{ 0x387477c2, 0x69c7, 0x11d2, { 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
        static constexpr guid simple_text_input{ 0x387477c1, 0x69c7, 0x11d2, { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
        static constexpr guid simple_file_system{ 0x964e5b22, 0x6459, 0x11d2, { 0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
        static constexpr guid loaded_image{ 0x5b1b31a1, 0x9562, 0x11d2, { 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
        static constexpr guid loaded_image_device_path{ 0xbc62157e, 0x3e33, 0x4fec, { 0x99, 0x20, 0x2d, 0x3b, 0x36, 0xd7, 0x50, 0xdf } };
        static constexpr guid device_path{ 0x09576e91, 0x6d3f, 0x11d2, { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };
        static constexpr guid graphics_output{ 0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };
    }

    namespace table
    {
        static constexpr guid mps{ 0xeb9d2d2f, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } };
        static constexpr guid acpi{ 0xeb9d2d30, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } };
        static constexpr guid acpi20{ 0x8868e871, 0xe4f1, 0x11d3, { 0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } };
        static constexpr guid smbios{ 0xeb9d2d31, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } };
        static constexpr guid smbios3{ 0xf2fd1544, 0x9794, 0x4a2c, { 0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94 } };
        static constexpr guid sal_system_table{ 0xeb9d2d32, 0x2d88, 0x11d3, { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } };
    }
#pragma endregion

#pragma region general
    enum status : uintn
    {
        mask_error = 0x8000000000000000,

        success = 0,

        warn_unknown_glyph = 1,
        warn_delete_failure,
        warn_write_failure,
        warn_buffer_too_small,
        warn_stale_data,
        warn_file_system,
        warn_reset_required,

        err_load_error = mask_error | 1,
        err_invalid_parameter,
        err_unsupported,
        err_bad_buffer_size,
        err_buffer_too_small,
        err_not_ready,
        err_device_error,
        err_write_protected,
        err_out_of_resources,
        err_volume_corrupted,
        err_volume_full,
        err_no_media,
        err_media_changed,
        err_not_found,
        err_access_denied,
        err_no_response,
        err_no_mapping,
        err_timeout,
        err_not_started,
        err_already_started,
        err_aborted,
        err_icmp_error,
        err_tftp_error,
        err_protocol_error,
        err_incompatible_version,
        err_security_violation,
        err_crc_error,
        err_end_of_media,
        err_end_of_file = mask_error | 31, // !!!
        err_invalid_language,
        err_compromised_data,
        err_ip_address_conflict,
        err_http_error,
    };

    inline constexpr boolean is_error(status s) { return s & mask_error; };

    enum class image_machine
    {
        ia32 = 0x014c,
        ia64 = 0x0200,
        ebc = 0x0ebc,
        x64 = 0x8664,
        armthumb_mixed = 0x01c2,
        aarch64 = 0xaa64,
        riscv32 = 0x5032,
        riscv64 = 0x5064,
        riscv128 = 0x5128,
        loong_arch32 = 0x6232,
        loong_arch64 = 0x6264,
    };

    enum class memory_type
    {
        reserved,
        loader_code,
        loader_data,
        boot_services_code,
        boot_services_data,
        runtime_services_code,
        runtime_services_data,
        conventional_memory,
        unusable_memory,
        acpi_reclaim_memory,
        acpi_memory_nvs,
        memory_mapped_io,
        memory_mapped_io_port_space,
        pal_code,
    };

    enum class allocation_type
    {
        any_pages,
        max_address,
        address,
    };

    enum memory_attribute : uint64
    {
        /* Cacheability */
        memory_uc = 1 << 0,
        memory_wc = 1 << 1,
        memory_wt = 1 << 2,
        memory_wb = 1 << 3,
        memory_uce = 1 << 4,

        /* Physical protection */
        memory_wp = 1 << 12,
        memory_rp = 1 << 13,
        memory_xp = 1 << 14,

        /* Miscellaneous */
        memory_nv = 1 << 15,
        memory_more_reliable = 1 << 16,
        memory_ro = 1 << 17,
        memory_sp = 1 << 18,
        memory_cpu_crypto = 1 << 19,
        memory_isa_valid = ( uint64 )1 << 62,
        memory_runtime = ( uint64 )1 << 63,
        memory_isa_mask = 0x0ffff00000000000,
    };

    struct memory_descriptor
    {
        memory_type type;
        uint32 pad;
        physical_address physical_start;
        virtual_address virtual_start;
        uint64 number_of_pages;
        uint64 attribute;
    };

#define uefi_next_memory_descriptor(ptr, size)  ((uefi::memory_descriptor *)(((uint8 *)ptr) + size))

    static constexpr uintn page_size = 4096;
    static constexpr uintn page_mask = 0xfff;
    static constexpr uintn page_shift = 12;

    inline constexpr uintn size_to_pages(uintn size)
    {
        return (size >> page_shift) + (size & page_mask ? 1 : 0);
    }

    enum class time_zone : int16
    {
        adjust_daylight = 0x0001,
        time_in_daylight = 0x0002,
        unspecified_timezone = 0x07ff,
    };

    struct time
    {
        uint16 year;
        uint8 month;
        uint8 day;
        uint8 hour;
        uint8 minute;
        uint8 second;
        uint8 pad1;
        uint32 nanosecond;
        time_zone time_zone;
        uint8 daylight;
        uint8 pad2;
    };

    struct time_capabilities
    {
        uint32 resolution;
        uint32 accuracy;
        boolean sets_to_zero;
    };

    using entry_point_t = status(EFIAPI*)(handle image_handle, struct system_table* system_table);

    struct table_header
    {
        uint64 signature;
        uint32 revision;
        uint32 header_size;
        uint32 crc32;
        uint32 reserved;
    };
#pragma endregion

#pragma region text
    enum text_attribute : uintn
    {
        bg_shift = 4,

        black = 0,
        blue,
        green,
        cyan = blue | green,
        red,
        magenta = blue | red,
        brown = green | red,
        light_gray = blue | green | red,
        bright,
        dark_gray = bright,
        light_blue = blue | bright,
        light_green = green | bright,
        light_cyan = cyan | bright,
        light_red = red | bright,
        light_magenta = magenta | bright,
        yellow = brown | bright,
        white = blue | green | red | bright,

        bg_black = black << bg_shift,
        bg_blue = blue << bg_shift,
        bg_green = green << bg_shift,
        bg_cyan = bg_blue | bg_green,
        bg_red = red << bg_shift,
        bg_magenta = bg_blue | bg_red,
        bg_brown = bg_green | bg_red,
        bg_light_gray = bg_blue | bg_green | bg_red,
    };

    struct simple_text_output_mode
    {
        int32 max_mode;
        int32 mode_number;
        int32 attribute;
        int32 cursor_col;
        int32 cursor_row;
        boolean cursor_visible;
    };

    struct simple_text_output_protocol
    {
        THISPTR_FN(status, reset, (this, extended_verification), boolean extended_verification);
        THISPTR_FN(status, output_string, (this, string), const char16* string);
        THISPTR_FN(status, test_string, (this, string), const char16* string);
        THISPTR_FN(status, query_mode, (this, mode_, cols, rows), uintn mode_, uintn* cols, uintn* rows);
        THISPTR_FN(status, set_mode, (this, mode_), uintn mode_);
        THISPTR_FN(status, set_attribute, (this, attribute), uintn attribute);
        THISPTR_FN(status, clear_screen, (this));
        THISPTR_FN(status, set_cursor_position, (this, col, row), uintn col, uintn row);
        THISPTR_FN(status, enable_cursor, (this, enable), boolean enable);
        simple_text_output_mode* mode;
    };

    struct input_key
    {
        uint16 scan_code;
        uint16 unicode_char;
    };

    struct simple_text_input_protocol
    {
        THISPTR_FN(status, reset, (this, extended_verification), boolean extended_verification);
        THISPTR_FN(status, read_key_stroke, (this, key), input_key* key);
        event wait_for_key;
    };

#pragma endregion

#pragma region files
    enum file_mode : uint64
    {
        file_mode_read = 1 << 0,
        file_mode_write = 1 << 1,
        file_mode_create = 1ull << 63,

        file_read_only = 1 << 0,
        file_hidden = 1 << 1,
        file_system = 1 << 2,
        file_reserved = 1 << 3,
        file_directory = 1 << 4,
        file_archive = 1 << 5,
        file_valid_attr = 0x37,
    };

    struct file_io_token
    {
        event event;
        status status;
        uintn buffer_size;
        void* buffer;
    };

    struct file_protocol
    {
        uint64 revision;
        THISPTR_FN(status, open, (this, new_handle, file_name, open_mode, attributes),
            file_protocol** new_handle, char16* file_name, uint64 open_mode, uint64 attributes);
        THISPTR_FN(status, close, (this));
        THISPTR_FN(status, delete_, (this));
        THISPTR_FN(status, read, (this, buffer_size, buffer), uintn* buffer_size, void* buffer);
        THISPTR_FN(status, write, (this, buffer_size, buffer), uintn* buffer_size, void* buffer);
        THISPTR_FN(status, get_position, (this, position), uint64* position);
        THISPTR_FN(status, set_position, (this, position), uint64 position);
        THISPTR_FN(status, get_info, (this, information_type, buffer_size, buffer), guid* information_type, uintn* buffer_size, void* buffer);
        THISPTR_FN(status, set_info, (this, information_type, buffer_size, buffer), guid* information_type, uintn* buffer_size, void* buffer);
        THISPTR_FN(status, flush, (this));
        THISPTR_FN(status, open_ex, (this, new_handle, file_name, open_mode, attributes, token),
            file_protocol** new_handle, char16* file_name, uint64 open_mode, uint64 attributes, file_io_token* token);
        THISPTR_FN(status, read_ex, (this, token), file_io_token* token);
        THISPTR_FN(status, write_ex, (this, token), file_io_token* token);
        THISPTR_FN(status, flush_ex, (this, token), file_io_token* token);
    };
    using file_handle = file_protocol;
    using file = file_protocol;

    struct simple_file_system_protocol
    {
        uint64 revision;
        THISPTR_FN(status, open_volume, (this, root), file_handle** root);
    };
    using file_io_interface = simple_file_system_protocol;

    enum class device_path_type : uint8
    {
        hardware = 1,
        acpi,
        messaging,
        media,
        bios_boot_specification,
        end_of_hardware = 0x7f,
    };

    struct device_path_protocol
    {
        device_path_type type;
        uint8 sub_type;
        uint8 length[2];
    };
    using device_path = device_path_protocol;

    struct loaded_image_protocol
    {
        uint32 revision;
        handle parent_handle;
        struct system_table* system_table;
        handle device_handle;
        device_path* file_path;
        void* reserved;
        uint32 load_options_size;
        void* load_options;
        void* image_base;
        uint64 image_size;
        memory_type image_code_type;
        memory_type image_data_type;
        MEMBER_FN(status, unload, handle image_handle);
    };

#pragma endregion

#pragma region graphics
    struct pixel_bitmask
    {
        uint32 red_mask;
        uint32 green_mask;
        uint32 blue_mask;
        uint32 reserved_mask;
    };

    enum graphics_pixel_format
    {
        pixel_fmt_rgb_reserved_8bit_per_color,
        pixel_fmt_bgr_reserved_8bit_per_color,
        pixel_fmt_bitmask,
        pixel_fmt_blt_only,
    };

    struct graphics_output_mode_information
    {
        uint32 version;
        uint32 horizontal_resolution;
        uint32 vertical_resolution;
        graphics_pixel_format pixel_format;
        pixel_bitmask pixel_information;
        uint32 pixels_per_scanline;
    };

    struct graphics_output_protocol_mode
    {
        uint32 max_mode;
        uint32 mode_number;
        graphics_output_mode_information* info;
        uintn size_of_info;
        physical_address frame_buffer_base;
        uintn frame_buffer_size;
    };

    struct graphics_output_blt_pixel
    {
        uint8 blue;
        uint8 green;
        uint8 red;
        uint8 reserved;
    };

    union graphics_output_blt_pixel_union
    {
        graphics_output_blt_pixel pixel;
        uint32 raw;
    };

    enum graphics_output_blt_operation
    {
        blt_video_fill,
        blt_video_to_blt_buffer,
        blt_buffer_to_video,
        blt_video_to_video,
    };

    struct graphics_output_protocol
    {
        THISPTR_FN(status, query_mode, (this, mode_number, size_of_info, info),
            uint32 mode_number, uintn* size_of_info, graphics_output_mode_information** info);
        THISPTR_FN(status, set_mode, (this, mode_number), uint32 mode_number);
        THISPTR_FN(status, blt, (this, blt_buffer, blt_operation, source_x, source_y, destination_x, destination_y, width, height, delta),
            graphics_output_blt_pixel* blt_buffer, graphics_output_blt_operation blt_operation,
            uintn source_x, uintn source_y, uintn destination_x, uintn destination_y,
            uintn width, uintn height, uintn delta)
            graphics_output_protocol_mode* mode;
    };
#pragma endregion

#pragma region services
    enum class timer_delay
    {
        cancel,
        periodic,
        relative,
    };

    enum class tpl : uintn
    {
        tpl_application = 4,
        tpl_callback = 8,
        tpl_notify = 16,
        tpl_high_level = 31,
    };

    enum event_type : uint32
    {
        evt_timer = ( uint32 )1 << 31,
        evt_runtime = 1 << 30,
        evt_notify_wait = 1 << 8,
        evt_notify_signal = 1 << 9,
        evt_signal_exit_boot_services = evt_notify_signal | 1,
        evt_signal_virtual_address_change = evt_notify_signal | 2,
    };

    enum open_protocol_type
    {
        open_protocol_by_handle_protocol = 1 << 0,
        open_protocol_get_protocol = 1 << 1,
        open_protocol_test_protocol = 1 << 2,
        open_protocol_by_child_controller = 1 << 3,
        open_protocol_by_driver = 1 << 4,
        open_protocol_exclusive = 1 << 5,
    };

    enum class interface_type
    {
        native_interface,
        pcode_interface,
    };

    enum class locate_search_type
    {
        all_handles,
        by_register_notify,
        by_protocol,
    };

    enum reset_type
    {
        reset_cold,
        reset_warm,
        reset_shutdown,
        reset_platform_specific,
    };

    enum debug_disposition : uintn
    {
        optional_pointer = 1 << 0,
    };

    enum capsule_attribute : uint32
    {
        persist_across_reset = 1 << 16,
        populate_system_table = 1 << 17,
        initiate_reset = 1 << 18,
    };

    using event_notify_t = void(EFIAPI*)(event, void*);

    struct open_protocol_information_entry
    {
        handle agent_handle;
        handle controller_handle;
        uint32 attributes;
        uint32 open_count;
    };

    struct capsule_block_descriptor
    {
        uint64 length;
        union
        {
            physical_address data_block;
            physical_address continuation_pointer;
        } u;
    };

    struct capsule_header
    {
        guid capsule_guid;
        uint32 header_size;
        uint32 flags;
        uint32 capsule_image_size;
    };

    struct boot_services
    {
        table_header header;
        MEMBER_FN(tpl, raise_tpl, tpl level);
        MEMBER_FN(void, restore_tpl, tpl level);
        MEMBER_FN(status, allocate_pages, allocation_type type, memory_type mem_type, uintn pages, physical_address* memory);
        MEMBER_FN(status, free_pages, physical_address address, uintn pages);
        MEMBER_FN(status, get_memory_map,
            uintn* memory_map_size, memory_descriptor* memory_map, uintn* map_key, uintn* descriptor_size, uint32* descriptor_version);
        MEMBER_FN(status, allocate_pool, memory_type pool_type, uintn size, void** buffer);
        MEMBER_FN(status, free_pool, void* buffer);
        MEMBER_FN(status, create_event, uint32 type, tpl notify_tpl, event_notify_t notify_function, void* notify_context, event* event_);
        MEMBER_FN(status, set_timer, event event_, timer_delay type, uint64 trigger_time);
        MEMBER_FN(status, wait_for_event, uintn number_of_events, event* event, uintn* index);
        MEMBER_FN(status, signal_event, event event_);
        MEMBER_FN(status, close_event, event event_);
        MEMBER_FN(status, check_event, event event_);
        MEMBER_FN(status, install_protocol_interface, handle* handle_, guid* protocol, interface_type interface_type_, void* interface);
        MEMBER_FN(status, reinstall_protocol_interface, handle handle_, guid* protocol, void* old_interface, void* new_interface);
        MEMBER_FN(status, uninstall_protocol_interface, handle handle_, guid* protocol, void* interface);
        MEMBER_FN(status, handle_protocol, handle* handle_, guid* protocol, void** interface);
        MEMBER_FN(status, pc_handle_protocol, handle* handle_, guid* protocol, void** interface);
        MEMBER_FN(status, register_protocol_notify, guid* protocol, event event_, void** registration);
        MEMBER_FN(status, locate_handle,
            locate_search_type search_type, guid* protocol, void* search_key, uintn* buffer_size, handle* buffer);
        MEMBER_FN(status, locate_device_path, guid* protocol, device_path** device_path_, handle* device);
        MEMBER_FN(status, install_configuration_table, guid* guid_, void* table);
        MEMBER_FN(status, load_image, handle image_handle);
        MEMBER_FN(status, start_image, handle image_handle, uintn* exit_data_size, char16** exit_data);
        MEMBER_FN(status, exit, handle image_handle, status exit_status, uintn exit_data_size, char16* exit_data);
        MEMBER_FN(status, unload_image, handle image_handle);
        MEMBER_FN(status, exit_boot_services, handle image_handle, uintn map_key);
        MEMBER_FN(status, get_next_monotonic_count, uint64* count);
        MEMBER_FN(status, stall, uintn ms);
        MEMBER_FN(status, set_watchdog_timer, uintn timeout, uint64 watchdog_code, uintn data_size, char16* watchdog_data);
        MEMBER_FN(status, connect_controller,
            handle controller_handle, handle* driver_image_handle, device_path* remaining_device_path, boolean recursive);
        MEMBER_FN(status, disconnect_controller,
            handle controller_handle, handle driver_image_handle, handle child_handle);
        MEMBER_FN(status, open_protocol,
            handle handle_, guid* protocol, void** interface, handle agent_handle, handle controller_handle, uint32 attributes);
        MEMBER_FN(status, close_protocol, handle handle_, guid* protocol, handle agent_handle, handle controller_handle);
        MEMBER_FN(status, open_protocol_information,
            handle handle_, guid* protocol, open_protocol_information_entry** entry_buffer, uintn* entry_count);
        MEMBER_FN(status, protocols_per_handle, handle handle_, guid*** protocol_buffer, uintn* protocol_buffer_count);
        MEMBER_FN(status, locate_handle_buffer,
            locate_search_type search_type, guid* protocol, void* search_key, uintn* handles, handle** buffer);
        MEMBER_FN(status, locate_protocol, guid* protocol, void* registration, void** interface_);
        MEMBER_FN(status, install_multiple_protocol_interfaces, handle, ...);
        MEMBER_FN(status, uninstall_multiple_protocol_interfaces, handle, ...);
        MEMBER_FN(status, calculate_crc32, void* data, uintn data_size, uint32* crc32);
        MEMBER_FN(status, copy_mem, void* destination, void* source, uintn length);
        MEMBER_FN(status, set_mem, void* destination, uintn size, uint8 value);
        MEMBER_FN(status, create_event_ex,
            uint32 type, tpl notify_tpl, event_notify_t notify_function, const void* notify_context, const guid* event_group, event* event_);
    };

    struct runtime_services
    {
        table_header header;
        MEMBER_FN(status, get_time, time* time_, time_capabilities* time_capabilities_);
        MEMBER_FN(status, set_time, time* time_);
        MEMBER_FN(status, get_wakeup_time, boolean* enabled, boolean* pending, time* time_);
        MEMBER_FN(status, set_wakeup_time, boolean enabled, time* time_);
        MEMBER_FN(status, set_virtual_address_map,
            uintn memory_map_size, uintn descriptor_size, uint32 descriptor_version, memory_descriptor* virtual_map);
        MEMBER_FN(status, convert_pointer, uintn debug_disposition_, void** address);
        MEMBER_FN(status, get_variable, char16* variable_name, guid* vendor_guid, uint32* attributes, uintn* data_size, void* data);
        MEMBER_FN(status, get_next_variable_name, uintn* variable_name_size, char16* variable_name, guid* vendor_guid);
        MEMBER_FN(status, set_variable, char16* variable_name, guid* vendor_guid, uint32 attributes, uintn data_size, void* data);
        MEMBER_FN(status, get_next_high_monotonic_count, uint32* high_count);
        MEMBER_FN(status, reset_system, reset_type reset_type_, status reset_status, uintn data_size, char16* reset_data);
        MEMBER_FN(status, update_capsule, capsule_header** capsule_header_array, uintn capsule_count, physical_address scatter_gather_list);
        MEMBER_FN(status, query_capsule_capabilities,
            capsule_header** capsule_header_array, uintn capsule_count, uint64* maximum_capsule_size, reset_type* reset_type_);
        MEMBER_FN(status, query_variable_info,
            uint32 attributes, uint64* maximum_variable_storage_size, uint64* remaining_variable_storage_size, uint64* maximum_variable_size);
    };

    struct configuration_table
    {
        guid vendor_guid;
        void* vendor_table;
    };

    struct system_table
    {
        table_header header;
        char16* firmware_vendor;
        uint32 firmware_revision;
        handle con_in_handle;
        simple_text_input_protocol* con_in;
        handle con_out_handle;
        simple_text_output_protocol* con_out;
        handle std_err_handle;
        simple_text_output_protocol* std_err;
        runtime_services* runtime_services;
        boot_services* boot_services;
        uintn number_of_table_entries;
        configuration_table* configuration_table;
    };
#pragma endregion

}

inline uefi::system_table* g_st;
inline uefi::boot_services* g_bs;
inline uefi::runtime_services* g_rt;

namespace uefi {

    inline void initialize_lib(uefi::system_table* sys_table)
    {
        g_st = sys_table;
        g_bs = sys_table->boot_services;
        g_rt = sys_table->runtime_services;
    }

}

using namespace uefi::types;

#undef THISPTR_FN
#undef MEMBER_FN
