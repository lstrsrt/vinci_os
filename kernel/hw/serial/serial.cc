#include <libc/print.h>

#include "serial.h"
#include "../cpu/x64.h"
#include "../cpu/isr.h"
#include "../../core/gfx/output.h"

namespace serial
{
    static u16 output_port;

    void InitPort(u16 port)
    {
        auto write_reg = [port](u16 reg, u8 data)
        {
            for (u32 i = 0; i < 1000; i++)
                x64::Delay();
            WritePort8(port + reg, data);
        };

        // Disable interrupts
        write_reg(reg::int_enabled, 0);

        // Enable the Divisor Latch Access Bit and set divisor to 2 (57600 baud)
        write_reg(reg::line_ctrl, ( u8 )Line::Dlab);
        write_reg(reg::baud_divisor_low, 2);
        write_reg(reg::baud_divisor_high, 0);

        // Clear DLAB and set up the rest
        write_reg(reg::line_ctrl, ( u8 )(Line::DataEight | Line::StopOne | Line::ParityNone));
        write_reg(reg::fifo_ctrl, ( u8 )(Fifo::Enable | Fifo::ClearReceive |
            Fifo::ClearTransmit | Fifo::TriggerLvl4));
        write_reg(reg::modem_ctrl, ( u8 )(Modem::DataReady | Modem::RequestSend | Modem::Loopback));

        // Test the port by verifying that we get the same value back
        write_reg(reg::data, 0xff);

        for (u32 i = 0; i < 1000; i++)
            x64::Delay();

        if (ReadPort8(port + reg::data) != 0xff)
        {
            Print("Serial: Port %x returned an invalid response\n", port);
            return;
        }

        // Test was successful, unset loopback again
        write_reg(reg::modem_ctrl, ( u8 )(Modem::DataReady | Modem::RequestSend));
    }

    EARLY void Initialize()
    {
        // Try to initialize COM1 and COM2.
        // TODO - Add support for finding more ports
        InitPort(port::com1);
        InitPort(port::com2);
        SetPort(port::com1);
    }

    void SetPort(u16 port)
    {
        output_port = port;
    }

    u8 Read(u16 port, u16 reg)
    {
        while (!(ReadPort8(port + reg::line_status) & ( u8 )LineStatus::DataReady))
            x64::Delay();
        return ReadPort8(port + reg);
    }

    void Write(u16 port, u16 reg, u8 data)
    {
        while (!(ReadPort8(port + reg::line_status) & ( u8 )LineStatus::EmptyTransmitReg))
            x64::Delay();
        WritePort8(port + reg, data);
    }

    void Write(const char* fmt, ...)
    {
        char str[512]{};
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(str, sizeof str, fmt, ap);
        va_end(ap);

        const char* s = str;
        while (*s)
            Write(output_port, reg::data, *s++);
    }
}
