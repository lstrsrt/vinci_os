#include <ec/array.h>

#include "keyboard.h"

namespace kbd
{
    namespace
    {
        enum ScanCode : u8
        {
            LeftAlt = 0x11,  // Right Alt is E0 11
            LeftShift = 0x12,
            LeftCtrl = 0x14, // Right Ctrl is E0 14
            CapsLock = 0x58,
            RightShift = 0x59,

            ExtendedBit = 1 << 7, // OR'd into extended key codes
            Extended = 0xe0,      // Prefix
            Release = 0xf0,       // Prefix
        };
    } // namespace

// The compiler thinks these aren't used because they are
// only referenced by an ISR, which isn't directly called.
// This forces them into .data as initialized values.
#pragma data_seg(".data")
    static ec::bitfield<KeyFlag> flags{};
    static bool released = false;
#pragma data_seg()

    bool HandleInput(u8 code, Key& key)
    {
        if (code == Extended)
        {
            flags.set(KeyFlag::Extended);
            return false;
        }
        if (code == Release)
        {
            released = true;
            return false;
        }

        const bool extended = flags.is_set(KeyFlag::Extended);
        flags.unset(KeyFlag::Extended);

        // E0 12 / E0 59 are "fake shifts" some keyboards send around extended keys
        // (arrows, Insert, PrtSc...) depending on NumLock/Shift state. Ignore them,
        // both press and release, so they don't change the real Shift state.
        if (extended && (code == LeftShift || code == RightShift))
        {
            released = false;
            return false;
        }

        if (released)
        {
            released = false;
            if (code == LeftAlt)
                flags.unset(KeyFlag::Alt);
            else if (code == LeftShift || code == RightShift)
                flags.unset(KeyFlag::Shift);
            else if (code == LeftCtrl)
                flags.unset(KeyFlag::Ctrl);
            return false;
        }

        // Left and right (extended) Alt/Ctrl share the same flag.
        if (code == LeftAlt)
        {
            flags.set(KeyFlag::Alt);
            return false;
        }
        if (code == LeftCtrl)
        {
            flags.set(KeyFlag::Ctrl);
            return false;
        }
        if (code == LeftShift || code == RightShift)
        {
            flags.set(KeyFlag::Shift);
            return false;
        }
        if (code == CapsLock)
        {
            flags.toggle(KeyFlag::CapsLock);
            return false;
        }

        // If we get to the end, allow the keycode to be set
        key.code = extended ? (code | ExtendedBit) : code;
        key.flags = flags;

        return true;
    }
} // namespace kbd
