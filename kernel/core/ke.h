#pragma once

#include <base.h>

enum class Status
{
    Success = 0,
    UnsupportedSystem,
};

namespace ke
{
    void Panic(Status);
}
