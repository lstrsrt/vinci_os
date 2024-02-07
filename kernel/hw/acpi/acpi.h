#pragma once

#include "../../lib/base.h"
#include "../cpu/x64.h"
#include "../common/acpi.h"

namespace acpi
{
    EARLY void ParseMadt(Madt* header, x64::ProcessorInfo& info);
}
