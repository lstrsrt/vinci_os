#pragma once

#include <ec/bitfield.h>

namespace kbd
{
    enum class KeyFlag
    {
        Alt = 1 << 0,
        Shift = 1 << 1,
        Ctrl = 1 << 2,
        CapsLock = 1 << 3,
        Extended = 1 << 4,
        Release = 1 << 5,
    };

    struct Key
    {
        u8 code;
        ec::bitfield<KeyFlag> flags;
    };

    using KeyboardCallback = void(*)(Key&);

    // Returns true if no flags were triggered (i.e. normal key, down press)
    bool HandleInput(u8 code, Key& key);

    bool RegisterCallback(KeyboardCallback);
}
