#pragma once

#include <base.h>

namespace ps2
{
    namespace port
    {
        constexpr u16 data = 0x60;
        constexpr u16 ctrl = 0x64;
    }

    namespace cmd
    {
        // Controller commands
        constexpr u8 disable_port1 = 0xad;
        constexpr u8 disable_port2 = 0xa7;
        constexpr u8 read0 = 0x20;
        constexpr u8 write0 = 0x60;
        constexpr u8 test_controller = 0xaa;
        constexpr u8 test_port1 = 0xab;
        constexpr u8 test_port2 = 0xa9;
        constexpr u8 enable_port1 = 0xae;
        constexpr u8 enable_port2 = 0xa8;
        constexpr u8 write_port2 = 0xd4;

        // Device commands
        constexpr u8 reset_no_scan = 0xf5;
        constexpr u8 get_id = 0xf2;
        constexpr u8 reset = 0xff;
    }

    namespace reply
    {
        constexpr u8 ack = 0xfa;
        constexpr u8 resend = 0xfe;
        constexpr u8 controller_test_passed = 0x55;
        constexpr u8 port_test_passed = 0x00;
        constexpr u8 device_test_passed = 0xaa;

        // Not a real reply, just used if nothing comes back
        constexpr u8 invalid = 0xcc;
    }

    enum class DeviceType : u16
    {
        Invalid = 0xffff,
        StdMouse = 0x0000,
        MouseScroll = 0x0003,
        FiveBtnMouse = 0x0004,
        Mf2KbTransl0 = 0xab41,
        Mf2KbTransl1 = 0xabc1,
        Mf2Kb = 0xab83
    };

    enum StatusFlag : u8
    {
        OutFull = 1 << 0,
        InFull = 1 << 1,
        System = 1 << 2,
        DataTarget = 1 << 3, // Device/controller
        Unknown = (1 << 4) | (1 << 5),
        Timeout = 1 << 6,
        Parity = 1 << 7,
    };

    union Config
    {
        struct
        {
            u8 first_irq : 1;
            u8 second_irq : 1;
            u8 system : 1;
            u8 reserved0 : 1;
            u8 first_clock_disabled : 1;
            u8 second_clock_disabled : 1;
            u8 port1_translation : 1;
            u8 reserved1 : 1;
        };
        u8 bits;
    };
    static_assert(sizeof Config == 0x1);

    EARLY void Initialize();

    void Write(u16 port, u8 byte);
    bool Read(u16 port, u8& reply);

    bool WriteDevice0(u8 data);
    bool WriteDevice1(u8 data);

    void IsrKeyboard();
    void IsrMouse();
}
