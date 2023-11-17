#pragma once

#include <base.h>

#include "../cpu/x64.h"
#include "../../../boot/boot.h"

namespace acpi {

    EARLY void ParseMadt(EFI_ACPI_2_0_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER* header, x64::ProcessorInfo& info);

}
