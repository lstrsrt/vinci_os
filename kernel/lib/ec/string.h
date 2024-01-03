#pragma once

#include <base.h>

namespace ec
{
    struct string
    {
        char* buffer;
        u64 length;
        u64 capacity;
    };
}
