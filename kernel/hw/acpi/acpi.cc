#include "acpi.h"
#include "../cpu/isr.h"
#include "../../core/gfx/output.h"

#pragma pack(1)
struct ACPI_SUBTABLE_HEADER
{
    UINT8 Type;
    UINT8 Length;
};

typedef struct
{
    UINT8     Type;
    UINT8     Length;
    UINT8     AcpiProcessorId;
    UINT8     ApicId;
    UINT32    Flags;
} EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC_STRUCTURE;

typedef struct
{
    UINT8     Type;
    UINT8     Length;
    UINT8     IoApicId;
    UINT8     Reserved;
    UINT32    IoApicAddress;
    UINT32    GlobalSystemInterruptBase;
} EFI_ACPI_2_0_IO_APIC_STRUCTURE;

typedef struct
{
    UINT8     Type;
    UINT8     Length;
    UINT8     AcpiProcessorId;
    UINT16    Flags;
    UINT8     LocalApicLint;
} EFI_ACPI_2_0_LOCAL_APIC_NMI_STRUCTURE;

typedef struct
{
    UINT8     Type;
    UINT8     Length;
    UINT8     Bus;
    UINT8     Source;
    UINT32    GlobalSystemInterrupt;
    UINT16    Flags;
} EFI_ACPI_2_0_INTERRUPT_SOURCE_OVERRIDE_STRUCTURE;

#define EFI_ACPI_2_0_PCAT_COMPAT                    (1 << 0)
#define EFI_ACPI_2_0_LOCAL_APIC_ENABLED             (1 << 0)

#define EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC           0
#define EFI_ACPI_2_0_IO_APIC                        1
#define EFI_ACPI_2_0_INTERRUPT_SOURCE_OVERRIDE      2
#define EFI_ACPI_2_0_NON_MASKABLE_INTERRUPT_SOURCE  3
#define EFI_ACPI_2_0_LOCAL_APIC_NMI                 4
#define EFI_ACPI_2_0_LOCAL_APIC_ADDRESS_OVERRIDE    5
#define EFI_ACPI_2_0_IO_SAPIC                       6
#define EFI_ACPI_2_0_PROCESSOR_LOCAL_SAPIC          7
#define EFI_ACPI_2_0_PLATFORM_INTERRUPT_SOURCES     8
#pragma pack()

namespace acpi {

    EARLY void ParseMadt(acpi::Madt* header, x64::ProcessorInfo& info)
    {
        if (info.pic_present = header->Flags & EFI_ACPI_2_0_PCAT_COMPAT)
            Print("PIC compatibility mode supported\n");

        Print("Local APIC: 0x%x\n", header->LocalApicAddress);
        apic::local = header->LocalApicAddress;
        info.cores = 0;

        // First entry is located after the main MADT header.
        // Every entry starts with its type and length.
        auto entry = ( ACPI_SUBTABLE_HEADER* )(( u64 )header + sizeof * header);

        while (( u64 )entry < ( u64 )header + header->Header.Length)
        {
            switch (entry->Type)
            {
            case EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC:
            {
                auto* ptr = ( EFI_ACPI_2_0_PROCESSOR_LOCAL_APIC_STRUCTURE* )entry;
                Print("  PROCESSOR_LOCAL_APIC: ID %u Core ID %u Type %u Flags 0x%x\n",
                    ptr->ApicId, ptr->AcpiProcessorId, ptr->Type, ptr->Flags);
                if (ptr->Flags & EFI_ACPI_2_0_LOCAL_APIC_ENABLED)
                    info.cores++;
                break;
            }
            case EFI_ACPI_2_0_IO_APIC:
            {
                auto* ptr = ( EFI_ACPI_2_0_IO_APIC_STRUCTURE* )entry;
                Print("  IO_APIC: ID %u Vector 0x%x\n", ptr->IoApicId, ptr->GlobalSystemInterruptBase);
                // there are multiple?
                apic::io = ptr->IoApicAddress;
                break;
            }
            case EFI_ACPI_2_0_INTERRUPT_SOURCE_OVERRIDE:
            {
                auto* ptr = ( EFI_ACPI_2_0_INTERRUPT_SOURCE_OVERRIDE_STRUCTURE* )entry;
                Print("  INTERRUPT_SOURCE_OVERRIDE: %u to %u\n", ptr->Source, ptr->GlobalSystemInterrupt);
                if (ptr->Source < 16)
                    apic::irq_gsi_overrides[ptr->Source] = ptr->GlobalSystemInterrupt;
                break;
            }
            case EFI_ACPI_2_0_LOCAL_APIC_NMI:
            {
                auto* ptr = ( EFI_ACPI_2_0_LOCAL_APIC_NMI_STRUCTURE* )entry;
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
            entry = ( ACPI_SUBTABLE_HEADER* )(( u64 )entry + entry->Length);
        }

        info.using_apic = apic::io && apic::local;
        Print("Found %u core%s\n", info.cores, info.cores == 1 ? "" : "s");
    }

}
