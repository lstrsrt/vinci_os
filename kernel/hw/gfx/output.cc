#include <ec/string.h>

#include "output.h"

#include "font.h"
#define SSFN_CONSOLEBITMAP_TRUECOLOR
#include "ssfn.h"

#include "../../core/ke.h"
#include "../cpu/asm-wrappers.h"

namespace gfx
{
    // The console draws into a back buffer in normal RAM and copies changed lines to the
    // frame buffer, which is write-combining memory and very slow to read from.
    // The back buffer is a ring of text lines: on-screen line L is stored in physical line
    // (top + L) % n_lines, so scrolling only clears one line and moves `top`.
    struct Console
    {
        u8* fb;
        u8* back;
        u32 width;
        u32 line_h;
        u32 cell_w;
        size_t line_bytes; // line_h rows of the frame buffer pitch
        u32 n_lines;
        u32 top;           // physical line shown at the top of the screen
        u32 cur_line;      // cursor, as an on-screen line
        u32 cur_x;
        // Lines to copy on the next Present(). No lines are dirty while dirty_lo > dirty_hi.
        u32 dirty_lo;
        u32 dirty_hi;
        bool all_dirty;
    };

    static Console con;

    static constexpr u32 tab_cells = 8;

    static u8* LineAddress(u8* base, u32 line)
    {
        return base + line * con.line_bytes;
    }

    static u32 PhysicalLine(u32 screen_line)
    {
        return (con.top + screen_line) % con.n_lines;
    }

    static void FillPixels(void* dst, u32 color, size_t count)
    {
        __stosd(( unsigned long* )dst, color, count);
    }

    // line_bytes is a multiple of 8 (line_h * pitch, pitch is a multiple of 4 and line_h is even).
    static void CopyLines(u8* dst, const u8* src, u32 lines)
    {
        __movsq(( u64* )dst, ( const u64* )src, lines * con.line_bytes / sizeof(u64));
    }

    static void MarkDirty(u32 screen_line)
    {
        if (screen_line < con.dirty_lo)
            con.dirty_lo = screen_line;
        if (screen_line > con.dirty_hi)
            con.dirty_hi = screen_line;
    }

    static void NewLine()
    {
        con.cur_x = 0;

        if (con.cur_line + 1 < con.n_lines)
        {
            con.cur_line++;
            return;
        }

        // Scroll: the oldest line becomes the new, empty bottom line.
        FillPixels(LineAddress(con.back, con.top), ssfn_dst.bg, con.line_bytes / sizeof(u32));
        con.top = (con.top + 1) % con.n_lines;
        con.all_dirty = true;
    }

    static void PutCodepoint(u32 c)
    {
        switch (c)
        {
        case '\r':
            con.cur_x = 0;
            return;
        case '\n':
            NewLine();
            return;
        case '\t': {
            const u32 stop = con.cell_w * tab_cells;
            con.cur_x = (con.cur_x / stop + 1) * stop;
            if (con.cur_x + con.cell_w > con.width)
                NewLine();
            return;
        }
        }

        if (con.cur_x + con.cell_w > con.width)
            NewLine();

        ssfn_dst.ptr = LineAddress(con.back, PhysicalLine(con.cur_line));
        ssfn_dst.x = con.cur_x;
        ssfn_dst.y = 0;

        if (ssfn_putc(c) == SSFN_OK)
        {
            con.cur_x = ssfn_dst.x;
            MarkDirty(con.cur_line);
        }
    }

    static void Present()
    {
        if (con.all_dirty)
        {
            // Physical lines [top, n_lines) are the top of the screen, [0, top) the rest.
            const u32 upper = con.n_lines - con.top;
            CopyLines(con.fb, LineAddress(con.back, con.top), upper);
            CopyLines(LineAddress(con.fb, upper), con.back, con.top);
        }
        else
        {
            for (u32 line = con.dirty_lo; line <= con.dirty_hi; line++)
                CopyLines(LineAddress(con.fb, line), LineAddress(con.back, PhysicalLine(line)), 1);
        }

        con.all_dirty = false;
        con.dirty_lo = con.n_lines;
        con.dirty_hi = 0;
    }

    void SetColor(u8 r, u8 g, u8 b)
    {
        ssfn_dst.fg = BgrPixel(r, g, b).full;
    }

    void Print(const char* fmt, ...)
    {
        bool prev = x64::DisableInterrupts();

        // Both buffers have to live until the end since s points into one of them.
        // heap_str only allocates if reserve() is called.
        char str[512]{};
        ec::string heap_str;
        char* s = str;

        va_list ap;
        va_start(ap, fmt);
        size_t len = vsnprintf(str, sizeof str, fmt, ap);
        va_end(ap);

        if (len == ec::umax_v<size_t>)
        {
            if (!ke::alloc_initialized)
            {
                strlcpy(str, "Print: Stack buffer too small!\n", sizeof str);
            }
            else
            {
                heap_str.reserve(2048);

                va_start(ap, fmt);
                len = vsnprintf(heap_str.data(), heap_str.capacity(), fmt, ap);
                va_end(ap);

                if (len == ec::umax_v<size_t>)
                    strlcpy(str, "Print: Heap buffer too small!\n", sizeof str);
                else
                    s = heap_str.data();
            }
        }

        while (*s)
            PutCodepoint(ssfn_utf8(&s));

        Present();

        if (prev)
            x64::EnableInterrupts();
    }

    void PutChar(char c)
    {
        bool prev = x64::DisableInterrupts();

        if (c)
        {
            PutCodepoint(( u8 )c);
            Present();
        }

        if (prev)
            x64::EnableInterrupts();
    }

    void SetFrameBufferAddress(u64 frame_buffer, u64 back_buffer)
    {
        con.fb = ( u8* )frame_buffer;
        con.back = ( u8* )back_buffer;
    }

    static constexpr char k_keys[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\t', '`', 0, 0, 0, 0, 0, 0, 'q', '1', 0, 0,
        0, 'z', 's', 'a', 'w', '2', 0, 0, 'c', 'x', 'd', 'e', '4', '3', 0, 0, ' ', 'v', 'f', 't', 'r', '5', 0, 0, 'n',
        'b', 'h', 'g', 'y', '6', 0, 0, 0, 'm', 'j', 'u', '7', '8', 0, 0, ',', 'k', 'i', 'o', '0', '9', 0, 0, '.', '/',
        'l', ';', 'p', '-', 0, 0, 0, '\'', 0, '[', '=', 0, 0, 0, 0, '\n', ']', 0, '\\'};

    static constexpr char k_shift_keys[]{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\t', '~', 0, 0, 0, 0, 0, 0, 'Q', '!',
        0, 0, 0, 'Z', 'S', 'A', 'W', '@', 0, 0, 'C', 'X', 'D', 'E', '$', '#', 0, 0, ' ', 'V', 'F', 'T', 'R', '%', 0, 0,
        'N', 'B', 'H', 'G', 'Y', '^', 0, 0, 0, 'M', 'J', 'U', '&', '*', 0, 0, '<', 'K', 'I', 'O', ')', '(', 0, 0, '>',
        '?', 'L', ':', 'P', '_', 0, 0, 0, '"', 0, '{', '+', 0, 0, 0, 0, '\n', '}', 0, '|'};
    /* This makes sure we only need one array size check. */
    static_assert(sizeof k_keys == sizeof k_shift_keys);

    // Called from the keyboard ISR, so interrupts are already disabled.
    void OnKey(const kbd::Key& key)
    {
        const bool shift = key.flags.is_set(kbd::KeyFlag::Shift) ^ key.flags.is_set(kbd::KeyFlag::CapsLock);
        const auto map = shift ? k_shift_keys : k_keys;
        const auto size = shift ? ec::array_size(k_shift_keys) : ec::array_size(k_keys);
        if (key.code >= size)
            return;

        if (const char c = map[key.code])
        {
            PutCodepoint(( u8 )c);
            Present();
        }
    }

    EARLY void Initialize(const DisplayInfo& display)
    {
        BgrPixel bg(0, 0, 0);

        ssfn_src = ( ssfn_font_t* )&FONT;

        con.fb = ( u8* )display.frame_buffer;
        con.back = ( u8* )display.back_buffer;
        con.width = display.width;
        con.line_h = ssfn_src->height;
        con.cell_w = ssfn_src->width;
        con.line_bytes = ( size_t )con.line_h * display.pitch;
        con.n_lines = display.height / con.line_h;
        con.top = con.cur_line = con.cur_x = 0;
        con.all_dirty = false;
        con.dirty_lo = con.n_lines;
        con.dirty_hi = 0;

        // Clear the whole screen once, including any rows below the last text line.
        FillPixels(con.fb, bg.full, ( size_t )display.height * display.pitch / sizeof(u32));
        FillPixels(con.back, bg.full, con.n_lines * con.line_bytes / sizeof(u32));

        // ssfn_putc draws one line at a time; ptr, x and y are set per glyph.
        ssfn_dst.w = display.width;
        ssfn_dst.h = con.line_h;
        ssfn_dst.p = display.pitch;
        ssfn_dst.fg = BgrPixel(255, 255, 255).full;
        ssfn_dst.bg = bg.full;
    }
} // namespace gfx
