#include <libc/print.h>

#include "serial.h"
#include "../cpu/x64.h"
#include "../cpu/isr.h"
#include "../gfx/output.h"

#define SERIAL_STDIO 0

namespace serial
{
    static u16 output_port;
    static bool has_com1, has_com2;

    void Isr()
    {
        char c = ReadPort8(output_port);
        if (c == '\r')
            c = '\n';

        PutChar(c); // Just send it to our framebuffer

#if SERIAL_STDIO == 1
        Write(output_port, reg::data, c); // Send to the console as well
#endif
    }

    EARLY static bool InitializePort(u16 port)
    {
        auto write_reg = [port](u16 reg, u8 data)
        {
            for (u32 i = 0; i < 1000; i++)
                x64::IoDelay();
            WritePort8(port + reg, data);
        };

#if SERIAL_STDIO == 1
        // Hardcode this to COM1 for now...
        if (port == port::com1)
        {
            write_reg(reg::int_enabled, 1);
            x64::ConnectIsr(Isr, 4);
        }
        else
#endif
        {
            write_reg(reg::int_enabled, 0);
        }

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
            x64::IoDelay();

        if (ReadPort8(port + reg::data) != 0xff)
        {
            Print("Serial: Port 0x%x returned an invalid response\n", port);
            return false;
        }

        // Test was successful, unset loopback again
        write_reg(reg::modem_ctrl, ( u8 )(Modem::DataReady | Modem::RequestSend));
        return true;
    }

    EARLY void Initialize()
    {
        // Try to initialize COM1 and COM2.
        // TODO - Add support for finding more ports
        has_com1 = InitializePort(port::com1);
        has_com2 = InitializePort(port::com2);
        if (!SetPort(port::com1))
            Print("Serial port debugging unavailable.\n");
    }

    bool SetPort(u16 port)
    {
        if ((port == port::com1 && has_com1) || (port == port::com2 && has_com2))
        {
            output_port = port;
            return true;
        }
        return false;
    }

    u16 GetPort()
    {
        return output_port;
    }

    u8 Read(u16 port, u16 reg)
    {
        while (!(ReadPort8(port + reg::line_status) & ( u8 )LineStatus::DataReady))
            x64::IoDelay();
        return ReadPort8(port + reg);
    }

    void Write(u16 port, u16 reg, u8 data)
    {
        while (!(ReadPort8(port + reg::line_status) & ( u8 )LineStatus::EmptyTransmitReg))
            x64::IoDelay();
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
