#pragma once

#include <base.h>
#include <ec/enums.h>

namespace serial
{
    enum_flags(Fifo, u8)
    {
        Enable = 1 << 0,
        ClearReceive = 1 << 1,
        ClearTransmit = 1 << 2,
        Enable64 = 1 << 5,
        TriggerLvl1 = 0 << 6,
        TriggerLvl2 = 1 << 6,
        TriggerLvl3 = 2 << 6,
        TriggerLvl4 = 3 << 6
    };

    enum_flags(Line, u8)
    {
        // Data bits
        DataFive,
        DataSix,
        DataSeven,
        DataEight,

        // Stop bits
        StopOne = 0 << 2,
        StopTwo = 1 << 2,

        // Parity
        ParityNone = 0 << 3,
        ParityOdd = 1 << 3,
        ParityEven = 3 << 3,
        ParityMark = 5 << 3,
        ParitySpace = 7 << 3,

        Dlab = 1 << 7
    };

    enum_flags(Modem, u8)
    {
        DataReady = 1 << 0, // DTR
        RequestSend = 1 << 1, // RTS
        AuxOutput1 = 1 << 2,
        AuxOutput2 = 1 << 3,
        Loopback = 1 << 4,
        AutoflowCtrl = 1 << 5
    };

    enum class LineStatus
    {
        DataReady = 1 << 0,
        OverrunError = 1 << 1,
        ParityError = 1 << 2,
        FramingError = 1 << 3,
        BreakIndicator = 1 << 4,
        EmptyTransmitReg = 1 << 5,
        EmptyTransmit = 1 << 6,
        InputError = 1 << 7
    };

    namespace port
    {
        static constexpr u16 com1 = 0x3f8;
        static constexpr u16 com2 = 0x2f8;
    }

    namespace reg
    {
        static constexpr u16 data = 0;              // DLAB = 0
        static constexpr u16 int_enabled = 1;       // DLAB = 0
        static constexpr u16 baud_divisor_low = 0;  // DLAB = 1
        static constexpr u16 baud_divisor_high = 1; // DLAB = 1
        static constexpr u16 fifo_ctrl = 2;
        static constexpr u16 line_ctrl = 3;
        static constexpr u16 modem_ctrl = 4;
        static constexpr u16 line_status = 5;
        static constexpr u16 modem_status = 6;
        static constexpr u16 scratch = 7;
    }

    EARLY void Initialize();

    // Default is COM1.
    // Returns false without changes if the port is invalid.
    bool SetPort(u16 port);
    u16 GetPort();

    u8 Read(u16 port, u16 reg = reg::data);
    void Write(u16 port, u16 reg, u8 data);
    void Write(const char* str, ...);
}
