#include "cmos.h"
#include "../timer/timer.h"
#include "../cpu/x64.h"
#include "../cpu/isr.h"
#include "../gfx/output.h"

namespace cmos
{
    static bool valid;

    static void SelectReg(u8 reg)
    {
        // Setting bit 7 also disables NMIs.
        // This is necessary so the RTC doesn't get interrupted
        // which can cause its state to become undefined.
        WritePort8(port::address, reg | (1 << 7));
        x64::IoDelay();
    }

    void UpdateRtc()
    {
        static const bool bcd = !(Read(port::rtc_status1) & ( u8 )RtcStatusB::Binary);
        static const bool h_12 = !(Read(port::rtc_status1) & ( u8 )RtcStatusB::Format);

        const auto bcd2bin = [](u8 u) -> u8
        {
            return bcd ? ((u >> 4) * 10) + (u & 0xf) : u;
        };

        time.s = bcd2bin(Read(port::rtc_s));
        time.m = bcd2bin(Read(port::rtc_m));

        // Hours are a little annoying because they may be in 12-hour format
        time.h = Read(port::rtc_h);
        bool pm = time.h & 0x80;
        time.h = bcd2bin(time.h & 0x7f);
        if (h_12)
        {
            time.h %= 12;
            if (pm)
                time.h += 12;
        }

        time.weekday = bcd2bin(Read(port::rtc_wd));
        time.day = bcd2bin(Read(port::rtc_d));
        time.month = bcd2bin(Read(port::rtc_m));
        time.year = bcd2bin(Read(port::rtc_y));
        time.century = rtc_has_century ? bcd2bin(Read(port::rtc_c)) : 20;
    }

    static void Isr()
    {
        Read(port::rtc_status2); // Acknowledge the interrupt
        timer::seconds++;
        UpdateTime();
    }

    void UpdateTime()
    {
        if (time.s != 59)
        {
            time.s++;
        }
        else
        {
            time.s = 0;
            if (time.m != 59)
            {
                time.m++;
            }
            else
            {
                time.m = 0;

                // Do a full update every hour to prevent clock drift.
                // Make sure it's not in an update cycle first.
                // First wait for the next cycle to come in...
                while (!(Read(port::rtc_status0) & ( u8 )RtcStatusA::Update))
                    ;

                // ...then wait for it to finish.
                // This makes us lag behind by 1 second at most.
                while (Read(port::rtc_status0) & ( u8 )RtcStatusA::Update)
                    ;

                UpdateRtc();
            }
        }
    }

    EARLY void Initialize()
    {
        // Check the CMOS battery once
        valid = Read(port::rtc_status3) & ( u8 )RtcStatusD::Valid;
        if (!valid)
        {
            Print("CMOS battery error. Unable to use RTC.\n");
            return;
        }

        // Set the local time
        UpdateRtc();

        // Connect the update end notification interrupt
        Read(port::rtc_status2);
        Write(port::rtc_status1, Read(port::rtc_status1) | ( u8 )RtcStatusB::UpdateInt);
        Read(port::rtc_status2);
        x64::ConnectIsr(Isr, 8);
    }

    u8 Read(u8 reg)
    {
        _disable();
        SelectReg(reg);
        u8 r = ReadPort8(port::data);
        _enable();
        x64::EnableNmi();
        return r;
    }

    void Write(u8 reg, u8 data)
    {
        _disable();
        SelectReg(reg);
        WritePort8(port::data, data);
        _enable();
        x64::EnableNmi();
    }

}
