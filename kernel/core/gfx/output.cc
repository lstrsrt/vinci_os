#include "output.h"

#include "font.h"
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#define SSFN_CONSOLEBITMAP_CONTROL
#include "ssfn.h"

#include "../ke.h"
#include <ec/string.h>

namespace gfx
{
    void SetColor(u8 r, u8 g, u8 b)
    {
        ssfn_dst.fg = BgrPixel(r, g, b).full;
    }

    void Print(const char* fmt, ...)
    {
        bool prev = x64::DisableInterrupts();
        char* s;

        if (!ke::alloc_initialized)
        {
            char str[512]{};

            va_list ap;
            va_start(ap, fmt);
            size_t len = vsnprintf(str, sizeof str, fmt, ap);
            va_end(ap);

            if (len == ec::umax_v<size_t>)
            {
                constexpr auto err = "Print: Stack buffer too small!\n";
                strlcpy(str, err, sizeof str);
            }

            s = str;
        }
        else
        {
            char str[512]{};
            size_t len;

            va_list ap, copy;
            va_start(ap, fmt);
            va_copy(copy, ap);
            len = vsnprintf(str, sizeof str, fmt, ap);
            va_end(ap);

            if (len == ec::umax_v<size_t>)
            {
                ec::string heap_str;
                heap_str.reserve(2048);

                va_start(copy, fmt);
                len = vsnprintf(heap_str.data(), heap_str.capacity(), fmt, copy);
                va_end(copy);

                if (len == ec::umax_v<size_t>)
                {
                    constexpr auto err = "Print: Heap buffer too small!\n";
                    strlcpy(str, err, sizeof str);
                    s = str;
                }
                else
                {
                    s = heap_str.data();
                }
            }
            else
            {
                s = str;
            }
        }

        while (*s)
            ssfn_putc(ssfn_utf8(&s));

        if (prev)
            x64::EnableInterrupts();
    }

    void PutChar(char c)
    {
        bool prev = x64::DisableInterrupts();

        char* s = &c;
        while (*s)
            ssfn_putc(ssfn_utf8(&s));

        if (prev)
            x64::EnableInterrupts();
    }

    void SetFrameBufferAddress(u64 address)
    {
        ssfn_dst.ptr = ( uint8_t* )address;
    }

    static constexpr char k_keys[]{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '`', '\t', 0,
        0, 0, 0, 0, 0, 0, 'q', '1', 0, 0, 0, 'z', 's', 'a',
        'w', '2', 0, 0, 'c', 'x', 'd', 'e', '4', '3', 0, 0,
        ' ', 'v', 'f', 't', 'r', '5', 0, 0, 'n', 'b', 'h',
        'g', 'y', '6', 0, 0, 0, 'm', 'j', 'u', '7', '8', 0,
        0, ',', 'k', 'i', 'o', '0', '9', 0, 0, '.', '/', 'l',
        ';', 'p', '-', 0, 0, 0, '\'', 0, '[', '=', 0, 0, 0,
        0, '\n', ']', 0, '\\'
    };

    static constexpr char k_shift_keys[]{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '`', '\t', 0,
        0, 0, 0, 0, 0, 0, 'Q', '!', 0, 0, 0, 'Z', 'S', 'A',
        'W', '@', 0, 0, 'C', 'X', 'D', 'E', '$', '#', 0, 0,
        ' ', 'V', 'F', 'T', 'R', '%', 0, 0, 'N', 'B', 'H',
        'G', 'Y', '^', 0, 0, 0, 'M', 'J', 'U', '&', '*', 0,
        0, '<', 'K', 'I', 'O', ')', '(', 0, 0, '>', '?', 'L',
        ':', 'P', '_', 0, 0, 0, '"', 0, '{', '=', 0, 0, 0,
        0, '\n', '}', 0, '|'
    };
    /* This makes sure we only need one array size check. */
    static_assert(sizeof k_keys == sizeof k_shift_keys);

    void OnKey(const kbd::Key& key)
    {
        const bool shift = key.flags.is_set(kbd::KeyFlag::Shift) ^ key.flags.is_set(kbd::KeyFlag::CapsLock);
        const auto map = shift ? k_shift_keys : k_keys;
        const auto size = shift ? ec::array_size(k_shift_keys) : ec::array_size(k_keys);
        const char c = map[key.code];

        if (key.code < size)
            ssfn_putc(c);
    }

    EARLY void Initialize(const DisplayInfo& display)
    {
        BgrPixel bg(0, 0, 0);

        for (u32 x = 0; x < display.width; x++)
        {
            for (u32 y = 0; y < display.height; y++)
            {
                // TODO - should copy a whole buffer instead of doing inefficient loops
                SetPixel(display, x, y, bg);
            }
        }

        ssfn_src = ( ssfn_font_t* )&FONT;

        ssfn_dst.ptr = ( uint8_t* )display.frame_buffer;
        ssfn_dst.w = 0 - display.width; // Negative means ABGR
        ssfn_dst.h = display.height;
        ssfn_dst.p = display.pitch;
        ssfn_dst.x = ssfn_dst.y = 0;
        ssfn_dst.fg = BgrPixel(255, 255, 255).full;
        ssfn_dst.bg = bg.full;
    }
}
