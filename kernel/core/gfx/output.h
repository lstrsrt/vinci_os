#pragma once

#include "../../lib/libc/print.h"
#include "../../../boot/boot.h"
#include "../../hw/ps2/keyboard.h"

namespace gfx
{
    union BgrPixel
    {
        struct
        {
            u32 blue : 8;
            u32 green : 8;
            u32 red : 8;
            u32 reserved : 8;
        };
        u32 full{};

        explicit BgrPixel() = default;
        explicit BgrPixel(u8 r, u8 g, u8 b)
            : full(b | (g << 8) | (r << 16))
        {
        }
    };

    INLINE void SetPixel(const DisplayInfo& d, u32 x, u32 y, BgrPixel p)
    {
        if (x > d.width)
            x = 0;
        if (y > d.height)
            y = 0;

        *(( BgrPixel* )(
            d.frame_buffer
            + d.pitch * y
            + sizeof(BgrPixel) * x)
            ) = p;
    }

    void SetFrameBufferAddress(u64 address);

    void OnKey(const kbd::Key& key);

    void Print(const char* fmt, ...);
    void PutChar(char c);

    EARLY void Initialize(const DisplayInfo& display);
}

using gfx::Print;
using gfx::PutChar;
