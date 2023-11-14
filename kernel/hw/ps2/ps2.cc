#include <ec/util.h>
#include <libc/str.h>

#include "ps2.h"
#include "keyboard.h"
#include "../cpu/x64.h"
#include "../cpu/isr.h"
#include "../../core/gfx/output.h"

namespace ps2
{
    static DeviceType DetectDevice(bool (*write_fn)(u8 cmd))
    {
        //if (write_fn(cmd::reset))
        //{
        //    u8 reply;
        //    Read(port::data, reply);
        //    if (reply == reply::device_test_passed
        //        && write_fn(cmd::get_id))
        //    {
        //        u8 h, l;
        //        Read(port::data, h);
        //        Read(port::data, l);
        //        return ( DeviceType )MAKE16(h, l);
        //    }
        //}
        //return DeviceType::Invalid;

        if (write_fn(cmd::reset_no_scan)
            && write_fn(cmd::get_id))
        {
            u8 h, l;
            Read(port::data, h);
            Read(port::data, l);
            if (write_fn(cmd::reset))
                return ( DeviceType )MAKE16(h, l);
        }
        return DeviceType::Invalid;
    }

    static bool CheckStatus(bool writable)
    {
        auto status = ReadPort8(port::ctrl);
        for (i32 i = 0; i < 10; i++)
        {
            if (writable ? status & StatusFlag::InFull : !(status & StatusFlag::OutFull))
                status = ReadPort8(port::ctrl);
            else return true;
        }
        return false;
    }

    static void Drain()
    {
        u8 reply;
        while (Read(port::data, reply))
            Print("PS/2: Drain 0x%x\n", reply);
    }

    void Initialize()
    {
        // TODO - initialize USB controllers and disable legacy

        // Flush output buffer
        Drain();

        // Test controller.
        // Should be unnecessary but seems to be expected on hardware
        // for the configuration byte to have the right values (?)
        u8 reply;
        Write(port::ctrl, cmd::test_controller);
        if (!Read(port::data, reply) || reply != reply::controller_test_passed)
            return;

        // Test device port 1
        Write(port::ctrl, cmd::test_port1);
        if (!Read(port::data, reply) || reply != reply::port_test_passed)
            return;

        // Get configuration byte
        Config cfg;
        Write(port::ctrl, cmd::read0);
        if (!Read(port::data, cfg.bits))
            return;

        x64::ConnectIsr(IsrKeyboard, 1);

        Print("PS/2: Read config 0x%x\n", cfg.bits);

        // Reset device 1 and get the device type
        auto device_type = DetectDevice(WriteDevice0);
        Print("PS/2: Device 1 Type 0x%x\n", ( u16 )device_type);

        if (device_type != DeviceType::Invalid)
        {
            cfg.first_irq = TRUE;
            cfg.first_clock_disabled = FALSE;
            cfg.port1_translation = FALSE;

            // Write back the configuration byte
            Write(port::ctrl, cmd::write0);
            Write(port::data, cfg.bits);
            Print("PS/2: Wrote config 0x%x\n", cfg.bits);
        }

        // TODO - detect device 2 and support mouse
        // LEDs
        // do something with DeviceType
    }

    void Write(u16 port, u8 data)
    {
        if (CheckStatus(true))
            WritePort8(port, data);
        else
            Print("PS/2: Write failed\n");
    }

    bool Read(u16 port, u8& reply)
    {
        // Give it some time so the previous command can complete
        for (u32 i = 0; i < 5000; i++)
            x64::Delay();

        if (!CheckStatus(false))
        {
            reply = reply::invalid;
            return false;
        }

        reply = ReadPort8(port);
        return true;
    }

    bool WriteDevice0(u8 data)
    {
        u8 res = reply::resend;
        for (i32 i = 0; i < 3 && res == reply::resend; i++)
        {
            Write(port::data, data);
            Read(port::data, res);
        }
        return res == reply::ack;
    }

    bool WriteDevice1(u8 data)
    {
        u8 res = reply::resend;
        for (i32 i = 0; i < 3 && res == reply::resend; i++)
        {
            Write(port::ctrl, cmd::write_port2);
            Write(port::data, data);
            Read(port::data, res);
        }
        return res == reply::ack;
    }

    void IsrKeyboard()
    {
        // Translate key and print to screen if valid
        kbd::Key key{};
        if (kbd::HandleInput(ReadPort8(port::data), key))
            gfx::OnKey(key);
    }

    void IsrMouse()
    {

    }
}
