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
        static constexpr auto xsdt = 'TSDX';
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

#define EFI_ACPI_2_0_8042 (1 << 1)

    struct Rsdp
    {
        u64    Signature;
        u8     Checksum;
        u8     OemId[6];
        u8     Revision;
        u32    RsdtAddress;
        u32    Length;
        u64    XsdtAddress;
        u8     ExtendedChecksum;
        u8     Reserved[3];
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
}

#pragma pack()
