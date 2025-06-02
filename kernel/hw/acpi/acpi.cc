#include "acpi.h"
#include "../cpu/isr.h"
#include "../gfx/output.h"

namespace acpi
{
    EARLY void ParseMadt(acpi::Madt* header, x64::ProcessorInfo& info)
    {
        info.pic_present = header->Flags & ACPI_MADT_PCAT_COMPAT;
        if (info.pic_present)
            Print("PIC compatibility mode supported\n");

        Print("Local APIC: 0x%x\n", header->LocalApicAddress);
        apic::local = header->LocalApicAddress;
        info.cores = 0;

        // First entry is located after the main MADT header.
        // Every entry starts with its type and length.
        auto entry = ( SubtableHeader* )(( u64 )header + sizeof(*header));

        while (( u64 )entry < ( u64 )header + header->Header.Length)
        {
            switch (entry->Type)
            {
            case ACPI_MADT_PROCESSOR_LOCAL_APIC:
            {
                auto ptr = ( acpi::MadtLocalApic* )entry;
                // Print("  PROCESSOR_LOCAL_APIC: ID %u Core ID %u Type %u Flags 0x%x\n",
                //     ptr->ApicId, ptr->AcpiProcessorId, ptr->Type, ptr->Flags);
                if (ptr->Flags & ACPI_MADT_LAPIC_ENABLED)
                    info.cores++;
                break;
            }
            case ACPI_MADT_IO_APIC:
            {
                auto ptr = ( acpi::MadtIoApic* )entry;
                // Print("  IO_APIC: ID %u Vector 0x%x\n", ptr->IoApicId, ptr->GlobalSystemInterruptBase);
                apic::io = ptr->IoApicAddress;

                // apic::WriteIo(apic::IoReg::ID, ptr->IoApicId);

                break;
            }
            case ACPI_MADT_INTERRUPT_SOURCE_OVERRIDE:
            {
                auto ptr = ( acpi::MadtIntSourceOverride* )entry;
                // Print("  INTERRUPT_SOURCE_OVERRIDE: %u to %u\n", ptr->Source, ptr->GlobalSystemInterrupt);
                if (ptr->Source < apic::int_src_overrides.size())
                    apic::int_src_overrides[ptr->Source] = IntSrcOverride(ptr->GlobalSystemInterrupt, ptr->Flags);
                break;
            }
            case ACPI_MADT_LOCAL_APIC_NMI:
            {
                auto ptr = ( acpi::MadtLocalApicNmi* )entry;
                // Print("  LOCAL_APIC_NMI: LINTN %u\n", ptr->LocalApicLint);
                info.apic_nmi_pin = (ptr->LocalApicLint != 0);
                // TODO - do something
                // apic::UpdateLvtEntry(
                //     info.apic_nmi_pin ? apic::LocalReg::LINT1 : apic::LocalReg::LINT0,
                //     2,
                //     apic::Delivery::Nmi,
                //     ON
                // );
                break;
            }
            default:
                // Print("  MADT: Ignoring entry of type %u\n", entry->Type);
                break;
            }
            entry = ( SubtableHeader* )(( u64 )entry + entry->Length);
        }

        info.using_apic = apic::io && apic::local;
        Print("Found %u core%s\n", info.cores, info.cores == 1 ? "" : "s");
    }
}
