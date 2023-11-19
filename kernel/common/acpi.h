#pragma once

#include <base.h>

#pragma pack(1)

namespace acpi
{
    namespace signature
    {
        static constexpr auto madt = 'CIPA';
        static constexpr auto fadt = 'PCAF';
        static constexpr auto hpet = 'TEPH';
        static constexpr auto xsdt = 'TDSX';
    }

    struct GenericAddress
    {
        u8 AddressSpaceId;
        u8 RegisterBitWidth;
        u8 RegisterBitOffset;
        u8 Reserved;
        u64 Address;
    };

    struct DescriptionHeader
    {
        u32 Signature;
        u32 Length;
        u8 Revision;
        u8 Checksum;
        u8 OemId[6];
        u64 OemTableId;
        u32 OemRevision;
        u32 CreatorId;
        u32 CreatorRevision;
    };

    struct Madt
    {
        DescriptionHeader Header;
        u32 LocalApicAddress;
        u32 Flags;
    };

#define ACPI_MADT_PCAT_COMPAT    (1 << 0)
#define ACPI_MADT_LAPIC_ENABLED  (1 << 0)

#define ACPI_MADT_PROCESSOR_LOCAL_APIC           0
#define ACPI_MADT_IO_APIC                        1
#define ACPI_MADT_INTERRUPT_SOURCE_OVERRIDE      2
#define ACPI_MADT_NON_MASKABLE_INTERRUPT_SOURCE  3
#define ACPI_MADT_LOCAL_APIC_NMI                 4
#define ACPI_MADT_LOCAL_APIC_ADDRESS_OVERRIDE    5
#define ACPI_MADT_IO_SAPIC                       6
#define ACPI_MADT_PROCESSOR_LOCAL_SAPIC          7
#define ACPI_MADT_PLATFORM_INTERRUPT_SOURCES     8

    struct Fadt
    {
        DescriptionHeader Header;
        u32 FirmwareCtrl;
        u32 Dsdt;
        u8 Reserved0;
        u8 PreferredPmProfile;
        u16 SciInt;
        u32 SmiCmd;
        u8 AcpiEnable;
        u8 AcpiDisable;
        u8 S4BiosReq;
        u8 PstateCnt;
        u32 Pm1aEvtBlk;
        u32 Pm1bEvtBlk;
        u32 Pm1aCntBlk;
        u32 Pm1bCntBlk;
        u32 Pm2CntBlk;
        u32 PmTmrBlk;
        u32 Gpe0Blk;
        u32 Gpe1Blk;
        u8 Pm1EvtLen;
        u8 Pm1CntLen;
        u8 Pm2CntLen;
        u8 PmTmrLen;
        u8 Gpe0BlkLen;
        u8 Gpe1BlkLen;
        u8 Gpe1Base;
        u8 CstCnt;
        u16 PLvl2Lat;
        u16 PLvl3Lat;
        u16 FlushSize;
        u16 FlushStride;
        u8 DutyOffset;
        u8 DutyWidth;
        u8 DayAlrm;
        u8 MonAlrm;
        u8 Century;
        u16 IaPcBootArch;
        u8 Reserved1;
        u32 Flags;
        GenericAddress ResetReg;
        u8 ResetValue;
        u8 Reserved2[3];
        u64 XFirmwareCtrl;
        u64 XDsdt;
        GenericAddress XPm1aEvtBlk;
        GenericAddress XPm1bEvtBlk;
        GenericAddress XPm1aCntBlk;
        GenericAddress XPm1bCntBlk;
        GenericAddress XPm2CntBlk;
        GenericAddress XPmTmrBlk;
        GenericAddress XGpe0Blk;
        GenericAddress XGpe1Blk;
    };

#define ACPI_FADT_8042 (1 << 1)

    struct Rsdp
    {
        u64 Signature;
        u8 Checksum;
        u8 OemId[6];
        u8 Revision;
        u32 RsdtAddress;
        u32 Length;
        u64 XsdtAddress;
        u8 ExtendedChecksum;
        u8 Reserved[3];
    };

    struct SubtableHeader
    {
        u8 Type;
        u8 Length;
    };

#pragma warning(disable: 4200)
    struct Xsdt
    {
        DescriptionHeader Header;
        u64* Tables[];
    };
#pragma warning(default: 4200)

    struct Hpet
    {
        DescriptionHeader Header;
        u32 EventTimerBlockId;
        GenericAddress BaseAddress;
        u8 Number;
        u16 MainCounterMinClockTick;
        u8 PageProtection : 4;
        u8 OemAttributes : 4;
    };

    struct MadtLocalApic
    {
        u8 Type;
        u8 Length;
        u8 AcpiProcessorId;
        u8 ApicId;
        u32 Flags;
    };

    struct MadtIoApic
    {
        u8 Type;
        u8 Length;
        u8 IoApicId;
        u8 Reserved;
        u32 IoApicAddress;
        u32 GlobalSystemInterruptBase;
    };

    struct MadtLocalApicNmi
    {
        u8 Type;
        u8 Length;
        u8 AcpiProcessorId;
        u16 Flags;
        u8 LocalApicLint;
    };

    struct MadtIntSourceOverride
    {
        u8 Type;
        u8 Length;
        u8 Bus;
        u8 Source;
        u32 GlobalSystemInterrupt;
        u16 Flags;
    };
}

#pragma pack()
