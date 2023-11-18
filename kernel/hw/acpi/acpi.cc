#include "acpi.h"
#include "../cpu/isr.h"
#include "../../core/gfx/output.h"

namespace acpi
{
    EARLY void ParseMadt(acpi::Madt* header, x64::ProcessorInfo& info)
    {
        if (info.pic_present = header->Flags & ACPI_MADT_PCAT_COMPAT)
            Print("PIC compatibility mode supported\n");

        Print("Local APIC: 0x%x\n", header->LocalApicAddress);
        apic::local = header->LocalApicAddress;
        info.cores = 0;

        // First entry is located after the main MADT header.
        // Every entry starts with its type and length.
        auto entry = ( SubtableHeader* )(( u64 )header + sizeof * header);

        while (( u64 )entry < ( u64 )header + header->Header.Length)
        {
            switch (entry->Type)
            {
            case ACPI_MADT_PROCESSOR_LOCAL_APIC:
            {
                auto ptr = ( acpi::MadtLocalApic* )entry;
                Print("  PROCESSOR_LOCAL_APIC: ID %u Core ID %u Type %u Flags 0x%x\n",
                    ptr->ApicId, ptr->AcpiProcessorId, ptr->Type, ptr->Flags);
                if (ptr->Flags & ACPI_MADT_LAPIC_ENABLED)
                    info.cores++;
                break;
            }
            case ACPI_MADT_IO_APIC:
            {
                auto ptr = ( acpi::MadtIoApic* )entry;
                Print("  IO_APIC: ID %u Vector 0x%x\n", ptr->IoApicId, ptr->GlobalSystemInterruptBase);
                // there are multiple?
                apic::io = ptr->IoApicAddress;
                break;
            }
            case ACPI_MADT_INTERRUPT_SOURCE_OVERRIDE:
            {
                auto ptr = ( acpi::MadtIntSourceOverride* )entry;
                Print("  INTERRUPT_SOURCE_OVERRIDE: %u to %u\n", ptr->Source, ptr->GlobalSystemInterrupt);
                if (ptr->Source < 16)
                    apic::irq_gsi_overrides[ptr->Source] = ptr->GlobalSystemInterrupt;
                break;
            }
            case ACPI_MADT_LOCAL_APIC_NMI:
            {
                auto ptr = ( acpi::MadtLocalApicNmi* )entry;
                Print("  LOCAL_APIC_NMI: LINTN %u\n", ptr->LocalApicLint);
                // TODO - do something
                apic::UpdateLvtEntry(ptr->LocalApicLint ? apic::LocalReg::LINT1 : apic::LocalReg::LINT0,
                    254, apic::Delivery::Nmi, true);
                break;
            }
            default:
                Print("  MADT: Ignoring entry of type %u\n", entry->Type);
                break;
            }
            entry = ( SubtableHeader* )(( u64 )entry + entry->Length);
        }

        info.using_apic = apic::io && apic::local;
        Print("Found %u core%s\n", info.cores, info.cores == 1 ? "" : "s");
    }
}
