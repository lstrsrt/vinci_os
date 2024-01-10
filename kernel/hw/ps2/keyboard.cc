#include <ec/array.h>

#include "keyboard.h"

// temporary..
#define EXTENDED   0xe0
#define RELEASE    0xf0
#define L_ALT      0x11
#define L_SHIFT    0x12
#define L_CTRL     0x14
#define CAPS_LOCK  0x58
#define R_SHIFT    0x59

namespace kbd
{

// The compiler thinks these aren't used because they are
// only referenced by an ISR, which isn't directly called.
// This forces them into .data as initialized values.
#pragma data_seg(".data")
    static ec::bitfield<KeyFlag> flags{};
    static ec::array<KeyboardCallback, 3> callbacks{};
    static bool released = false;
#pragma data_seg()

    bool HandleInput(u8 code, Key& key)
    {
        if (code == EXTENDED)
        {
            flags.set(KeyFlag::Extended);
            return false;
        }
        if (code == RELEASE)
        {
            released = true;
            return false;
        }

        if (released)
        {
            released = false;
            if (code == L_ALT)
                flags.unset(KeyFlag::Alt);
            else if (code == L_SHIFT || code == R_SHIFT)
                flags.unset(KeyFlag::Shift);
            else if (code == L_CTRL)
                flags.unset(KeyFlag::Ctrl);
            return false;
        }

        if (flags.is_set(KeyFlag::Extended))
        {
            code |= RELEASE;
            flags.unset(KeyFlag::Extended);
        }

        if (code == L_ALT)
        {
            flags.set(KeyFlag::Alt);
            return false;
        }
        if (code == L_SHIFT || code == R_SHIFT)
        {
            flags.set(KeyFlag::Shift);
            return false;
        }
        if (code == L_CTRL)
        {
            flags.set(KeyFlag::Ctrl);
            return false;
        }
        if (code == CAPS_LOCK)
        {
            flags.toggle(KeyFlag::CapsLock);
            return false;
        }

        // If we get to the end, allow the keycode to be set
        key.code = code;
        key.flags = flags;

        for (auto cb : callbacks)
        {
            if (cb)
                cb(key);
        }

        return true;
    }

    bool RegisterCallback(KeyboardCallback callback)
    {
        for (auto& cb : callbacks)
        {
            if (!cb)
            {
                cb = callback;
                return true;
            }
        }

        return false;
    }
}
