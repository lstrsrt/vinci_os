#include "acpi.h"
#include "../cpu/isr.h"
#include "../gfx/output.h"

#ifdef DEBUG_MADT
#define DbgPrint(x, ...) Print(x, __VA_ARGS__)
#else
#define DbgPrint(x, ...) EMPTY_STMT
#endif

namespace acpi
{
    EARLY void ParseMadt(acpi::Madt* header, x64::ProcessorInfo& info)
    {
        info.pic_present = header->Flags & ACPI_MADT_PCAT_COMPAT;
        if (info.pic_present)
            Print("PIC compatibility mode supported\n");

        DbgPrint("Local APIC: 0x%x\n", header->LocalApicAddress);
        apic::local = header->LocalApicAddress;
        info.cores = 0;

        // First entry is located after the main MADT header.
        // Every entry starts with its type and length.
        auto entry = ( SubtableHeader* )(( u64 )header + sizeof(*header));

        while (( u64 )entry < ( u64 )header + header->Header.Length)
        {
            switch (entry->Type)
            {
            case ACPI_MADT_PROCESSOR_LOCAL_APIC: {
                auto ptr = ( acpi::MadtLocalApic* )entry;
                DbgPrint("  PROCESSOR_LOCAL_APIC: ID %u Core ID %u Type %u Flags 0x%x\n", ptr->ApicId,
                    ptr->AcpiProcessorId, ptr->Type, ptr->Flags);
                if (ptr->Flags & ACPI_MADT_LAPIC_ENABLED)
                    info.cores++;
                break;
            }
            case ACPI_MADT_IO_APIC: {
                auto ptr = ( acpi::MadtIoApic* )entry;
                DbgPrint("  IO_APIC: ID %u GSI 0x%x\n", ptr->IoApicId, ptr->GlobalSystemInterruptBase);
                if (!apic::io || ptr->GlobalSystemInterruptBase == 0)
                    apic::io = ptr->IoApicAddress;
                break;
            }
            case ACPI_MADT_INTERRUPT_SOURCE_OVERRIDE: {
                auto ptr = ( acpi::MadtIntSourceOverride* )entry;
                DbgPrint("  INTERRUPT_SOURCE_OVERRIDE: %u to %u\n", ptr->Source, ptr->GlobalSystemInterrupt);
                if (ptr->Source < apic::int_src_overrides.size())
                    apic::int_src_overrides[ptr->Source] = IntSrcOverride(ptr->GlobalSystemInterrupt, ptr->Flags);
                break;
            }
            case ACPI_MADT_LOCAL_APIC_NMI: {
                auto ptr = ( acpi::MadtLocalApicNmi* )entry;
                DbgPrint("  LOCAL_APIC_NMI: LINTN %u\n", ptr->LocalApicLint);
                ( void )ptr;
                break;
            }
            default:
                DbgPrint("  MADT: Ignoring entry of type %u\n", entry->Type);
                break;
            }
            entry = ( SubtableHeader* )(( u64 )entry + entry->Length);
        }

        info.using_apic = apic::io && apic::local;
        Print("Found %u core%s\n", info.cores, info.cores == 1 ? "" : "s");
    }
} // namespace acpi
