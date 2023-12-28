#include <ec/array.h>

#include "x64.h"
#include "cpuid.h"
#include "isr.h"
#include "../../core/ke.h"
#include "../../core/gfx/output.h"

namespace x64
{
    alignas(page_size) inline volatile u8 int_stack_tables[IstCount][ist_size]{};

    alignas(64) static Tss kernel_tss(
        ( u64 )(int_stack_tables[0] + ist_size),
        ( u64 )(int_stack_tables[1] + ist_size),
        ( u64 )(int_stack_tables[2] + ist_size),
        ( u64 )(int_stack_tables[3] + ist_size)
    );

#pragma data_seg("PROTDATA")
    alignas(64) static const GdtEntry gdt[]{
        GdtEntry::Null(),                              // Empty
        GdtEntry(0, GDT_ENTRY_CODE | GDT_READ),        // Ring 0 code
        GdtEntry(0, GDT_ENTRY_DATA | GDT_WRITE),       // Ring 0 data
        GdtEntry(3, GDT_ENTRY_CODE | GDT_READ, false), // Ring 3 code, 32 bit
        GdtEntry(3, GDT_ENTRY_DATA | GDT_WRITE),       // Ring 3 data
        GdtEntry(3, GDT_ENTRY_CODE | GDT_READ),        // Ring 3 code
        GdtEntry::TssLow(&kernel_tss),                 // TSS low
        GdtEntry::TssHigh(&kernel_tss)                 // TSS high
    };

    alignas(64) static ec::array<IdtEntry, 256> idt;

    alignas(sizeof u16) static DescriptorTable gdt_desc(&gdt, sizeof gdt - 1);
    alignas(sizeof u16) static DescriptorTable idt_desc(&idt, sizeof idt - 1);
#pragma data_seg()

    EARLY static void CheckFeatures()
    {
        Print("CPU vendor: %s\n", GetVendorString(cpu_info.vendor_string));

        Cpuid ids(CpuidLeaf::Info);

        cpu_info.stepping = EXTRACT32(ids.eax, 0, 4);
        cpu_info.model = EXTRACT32(ids.eax, 4, 8);
        cpu_info.family = EXTRACT32(ids.eax, 8, 12);

        if (cpu_info.family >= 6)
        {
            cpu_info.model += (EXTRACT32(ids.eax, 16, 20) << 4);
            if (cpu_info.family == 15)
                cpu_info.family += EXTRACT32(ids.eax, 20, 28);
        }

        Print("F %u M %u S %u\n", cpu_info.family, cpu_info.model, cpu_info.stepping);
        Print("CPUID: ");

        if (CheckCpuid(ids.ecx, CpuidFeature::HYPERVISOR))
            Print("HV ");

        // Supported features
        if (cpu_info.tsc_supported = CheckCpuid(ids.edx, CpuidFeature::TSC))
            Print("TSC ");
        if (cpu_info.msr_supported = CheckCpuid(ids.edx, CpuidFeature::MSR))
            Print("MSR ");
        if (cpu_info.pat_supported = CheckCpuid(ids.edx, CpuidFeature::PAT))
            Print("PAT ");
        // if (cpu_info.using_apic = CheckCpuid(ids.edx, CpuidFeature::APIC))
        //     Print("APIC ");
        if (CheckCpuid(ids.ecx, ( CpuidFeature )(1 << 21)))
            Print("X2APIC ");

        Print("\n");
    }

    EARLY static void SetCr0Bits()
    {
        // TODO - set ET?
        WriteCr0(ReadCr0() & ~(Cr0::MP | Cr0::NE));
    }

    EARLY static void SetCr4Bits()
    {
        Cpuid ids(CpuidLeaf::ExtendedFeatures);
        auto cr4 = ReadCr4();

        Print("CR4: ");

        if (CheckCpuid(ids.ebx, CpuidFeature::FSGSBASE))
        {
            cr4 |= Cr4::FSGSBASE;
            Print("FSGSBASE ");
        }

        if (CheckCpuid(ids.ebx, CpuidFeature::SMEP))
        {
            cr4 |= Cr4::SMEP;
            Print("SMEP ");
        }

        if (cpu_info.smap_supported =
            CheckCpuid(ids.ebx, CpuidFeature::SMAP))
        {
            cr4 |= Cr4::SMAP;
            Print("SMAP ");
        }

        if (CheckCpuid(ids.ecx, CpuidFeature::UMIP))
        {
            cr4 |= Cr4::UMIP;
            Print("UMIP ");
        }

        Print("\n");

        WriteCr4(cr4);
    }

#define MSR_MTRR_DEF_TYPE 0x2ff
#define MSR_PAT 0x277

#define MTRR_FIXED_ENABLED (1 << 10)
#define MTRR_ENABLED (1 << 11)

    enum MemoryType : u64
    {
        Uncacheable = 0x00, // (UC)
        WriteCombining = 0x01,
        WriteThrough = 0x04,
        WriteProtected = 0x05,
        WriteBack = 0x06,
        Uncached = 0x07, // (UC-) PAT only, reserved on MTRRs
    };

    EARLY static void LoadPageAttributeTable()
    {
        if (!cpu_info.pat_supported)
        {
            Print("PAT not supported, skipping initialization.\n");
            return;
        }

        // This is the same as the default PAT entries
        // but with one exception: PAT4 selects WC instead of WB
        static constexpr auto pat_entries = ( u64 )(
            (MemoryType::WriteBack)              // PAT0: WB
            | (MemoryType::WriteThrough << 8)    // PAT1: WT
            | (MemoryType::Uncached << 16)       // PAT2: UC-
            | (MemoryType::Uncacheable << 24)    // PAT3: UC+
            | (MemoryType::WriteCombining << 32) // PAT4: WC
            | (MemoryType::WriteThrough << 40)   // PAT5: WT
            | (MemoryType::Uncached << 48)       // PAT6: UC-
            | (MemoryType::Uncacheable << 56));  // PAT7: UC+

        TlbFlush();
        __wbinvd();
        __writemsr(MSR_PAT, pat_entries);
        __wbinvd();
        TlbFlush();
    }

    EARLY static void InitGdt()
    {
        _lgdt(&gdt_desc);
        ReloadSegments(GetGdtOffset(GdtIndex::R0Code), GetGdtOffset(GdtIndex::R0Data));
        LoadTr(GetGdtOffset(GdtIndex::TssLow));
    }

    EARLY static void InitIdt()
    {
        // TODO - can probably do this in a constructor

        memzero(&idt, sizeof idt);

        idt[0].Set(_Isr0, 0);
        idt[1].Set(_Isr1, 0, DebugIst);
        idt[2].Set(_Isr2, 0, NmiIst);
        idt[3].Set(_Isr3, 0, DebugIst);
        idt[4].Set(_Isr4, 0);
        idt[5].Set(_Isr5, 0);
        idt[6].Set(_Isr6, 0);
        idt[7].Set(_Isr7, 0);
        idt[8].Set(_Isr8, 0, DoubleFaultIst);
        idt[9].Set(_Isr9, 0);
        idt[10].Set(_Isr10, 0);
        idt[11].Set(_Isr11, 0);
        idt[12].Set(_Isr12, 0);
        idt[13].Set(_Isr13, 0);
        idt[14].Set(_Isr14, 0);
        idt[15].Set(_Isr15, 0);
        idt[16].Set(_Isr16, 0);
        idt[17].Set(_Isr17, 0);
        idt[18].Set(_Isr18, 0, MachineCheckIst);
        idt[19].Set(_Isr19, 0);
        idt[20].Set(_Isr20, 0);
        idt[21].Set(_Isr21, 0);
        idt[22].Set(_Isr22, 0);
        idt[23].Set(_Isr23, 0);
        idt[24].Set(_Isr24, 0);
        idt[25].Set(_Isr25, 0);
        idt[26].Set(_Isr26, 0);
        idt[27].Set(_Isr27, 0);
        idt[28].Set(_Isr28, 0);
        idt[29].Set(_Isr29, 0);
        idt[30].Set(_Isr30, 0);
        idt[31].Set(_Isr31, 0);

        idt[32].Set(_Isr32, 0);
        idt[33].Set(_Isr33, 0);
        idt[34].Set(_Isr34, 0);
        idt[35].Set(_Isr35, 0);
        idt[36].Set(_Isr36, 0);
        idt[37].Set(_Isr37, 0);
        idt[38].Set(_Isr38, 0);
        idt[39].Set(_Isr39, 0);
        idt[40].Set(_Isr40, 0);
        idt[41].Set(_Isr41, 0);
        idt[42].Set(_Isr42, 0);
        idt[43].Set(_Isr43, 0);
        idt[44].Set(_Isr44, 0);
        idt[45].Set(_Isr45, 0);
        idt[46].Set(_Isr46, 0);
        idt[47].Set(_Isr47, 0);

        idt[48].Set(_Isr48, 0);
        idt[49].Set(_Isr49, 0);
        idt[50].Set(_Isr50, 0);
        idt[51].Set(_Isr51, 0);
        idt[52].Set(_Isr52, 0);
        idt[53].Set(_Isr53, 0);
        idt[54].Set(_Isr54, 0);
        idt[55].Set(_Isr55, 0);
        idt[56].Set(_Isr56, 0);
        idt[57].Set(_Isr57, 0);
        idt[58].Set(_Isr58, 0);
        idt[59].Set(_Isr59, 0);
        idt[60].Set(_Isr60, 0);
        idt[61].Set(_Isr61, 0);
        idt[62].Set(_Isr62, 0);
        idt[63].Set(_Isr63, 0);
        idt[64].Set(_Isr64, 0);
        idt[65].Set(_Isr65, 0);
        idt[66].Set(_Isr66, 0);
        idt[67].Set(_Isr67, 0);
        idt[68].Set(_Isr68, 0);
        idt[69].Set(_Isr69, 0);
        idt[70].Set(_Isr70, 0);
        idt[71].Set(_Isr71, 0);
        idt[72].Set(_Isr72, 0);
        idt[73].Set(_Isr73, 0);
        idt[74].Set(_Isr74, 0);
        idt[75].Set(_Isr75, 0);
        idt[76].Set(_Isr76, 0);
        idt[77].Set(_Isr77, 0);
        idt[78].Set(_Isr78, 0);
        idt[79].Set(_Isr79, 0);
        idt[80].Set(_Isr80, 0);
        idt[81].Set(_Isr81, 0);
        idt[82].Set(_Isr82, 0);
        idt[83].Set(_Isr83, 0);
        idt[84].Set(_Isr84, 0);
        idt[85].Set(_Isr85, 0);
        idt[86].Set(_Isr86, 0);
        idt[87].Set(_Isr87, 0);
        idt[88].Set(_Isr88, 0);
        idt[89].Set(_Isr89, 0);
        idt[90].Set(_Isr90, 0);
        idt[91].Set(_Isr91, 0);
        idt[92].Set(_Isr92, 0);
        idt[93].Set(_Isr93, 0);
        idt[94].Set(_Isr94, 0);
        idt[95].Set(_Isr95, 0);
        idt[96].Set(_Isr96, 0);
        idt[97].Set(_Isr97, 0);
        idt[98].Set(_Isr98, 0);
        idt[99].Set(_Isr99, 0);

        idt[100].Set(_Isr100, 0);
        idt[101].Set(_Isr101, 0);
        idt[102].Set(_Isr102, 0);
        idt[103].Set(_Isr103, 0);
        idt[104].Set(_Isr104, 0);
        idt[105].Set(_Isr105, 0);
        idt[106].Set(_Isr106, 0);
        idt[107].Set(_Isr107, 0);
        idt[108].Set(_Isr108, 0);
        idt[109].Set(_Isr109, 0);
        idt[110].Set(_Isr110, 0);
        idt[111].Set(_Isr111, 0);
        idt[112].Set(_Isr112, 0);
        idt[113].Set(_Isr113, 0);
        idt[114].Set(_Isr114, 0);
        idt[115].Set(_Isr115, 0);
        idt[116].Set(_Isr116, 0);
        idt[117].Set(_Isr117, 0);
        idt[118].Set(_Isr118, 0);
        idt[119].Set(_Isr119, 0);
        idt[120].Set(_Isr120, 0);
        idt[121].Set(_Isr121, 0);
        idt[122].Set(_Isr122, 0);
        idt[123].Set(_Isr123, 0);
        idt[124].Set(_Isr124, 0);
        idt[125].Set(_Isr125, 0);
        idt[126].Set(_Isr126, 0);
        idt[127].Set(_Isr127, 0);
        idt[128].Set(_Isr128, 0);
        idt[129].Set(_Isr129, 0);
        idt[130].Set(_Isr130, 0);
        idt[131].Set(_Isr131, 0);
        idt[132].Set(_Isr132, 0);
        idt[133].Set(_Isr133, 0);
        idt[134].Set(_Isr134, 0);
        idt[135].Set(_Isr135, 0);
        idt[136].Set(_Isr136, 0);
        idt[137].Set(_Isr137, 0);
        idt[138].Set(_Isr138, 0);
        idt[139].Set(_Isr139, 0);
        idt[140].Set(_Isr140, 0);
        idt[141].Set(_Isr141, 0);
        idt[142].Set(_Isr142, 0);
        idt[143].Set(_Isr143, 0);
        idt[144].Set(_Isr144, 0);
        idt[145].Set(_Isr145, 0);
        idt[146].Set(_Isr146, 0);
        idt[147].Set(_Isr147, 0);
        idt[148].Set(_Isr148, 0);
        idt[149].Set(_Isr149, 0);
        idt[150].Set(_Isr150, 0);
        idt[151].Set(_Isr151, 0);
        idt[152].Set(_Isr152, 0);
        idt[153].Set(_Isr153, 0);
        idt[154].Set(_Isr154, 0);
        idt[155].Set(_Isr155, 0);
        idt[156].Set(_Isr156, 0);
        idt[157].Set(_Isr157, 0);
        idt[158].Set(_Isr158, 0);
        idt[159].Set(_Isr159, 0);
        idt[160].Set(_Isr160, 0);
        idt[161].Set(_Isr161, 0);
        idt[162].Set(_Isr162, 0);
        idt[163].Set(_Isr163, 0);
        idt[164].Set(_Isr164, 0);
        idt[165].Set(_Isr165, 0);
        idt[166].Set(_Isr166, 0);
        idt[167].Set(_Isr167, 0);
        idt[168].Set(_Isr168, 0);
        idt[169].Set(_Isr169, 0);
        idt[170].Set(_Isr170, 0);
        idt[171].Set(_Isr171, 0);
        idt[172].Set(_Isr172, 0);
        idt[173].Set(_Isr173, 0);
        idt[174].Set(_Isr174, 0);
        idt[175].Set(_Isr175, 0);
        idt[176].Set(_Isr176, 0);
        idt[177].Set(_Isr177, 0);
        idt[178].Set(_Isr178, 0);
        idt[179].Set(_Isr179, 0);
        idt[180].Set(_Isr180, 0);
        idt[181].Set(_Isr181, 0);
        idt[182].Set(_Isr182, 0);
        idt[183].Set(_Isr183, 0);
        idt[184].Set(_Isr184, 0);
        idt[185].Set(_Isr185, 0);
        idt[186].Set(_Isr186, 0);
        idt[187].Set(_Isr187, 0);
        idt[188].Set(_Isr188, 0);
        idt[189].Set(_Isr189, 0);
        idt[190].Set(_Isr190, 0);
        idt[191].Set(_Isr191, 0);
        idt[192].Set(_Isr192, 0);
        idt[193].Set(_Isr193, 0);
        idt[194].Set(_Isr194, 0);
        idt[195].Set(_Isr195, 0);
        idt[196].Set(_Isr196, 0);
        idt[197].Set(_Isr197, 0);
        idt[198].Set(_Isr198, 0);
        idt[199].Set(_Isr199, 0);
        idt[200].Set(_Isr200, 0);
        idt[201].Set(_Isr201, 0);
        idt[202].Set(_Isr202, 0);
        idt[203].Set(_Isr203, 0);
        idt[204].Set(_Isr204, 0);
        idt[205].Set(_Isr205, 0);
        idt[206].Set(_Isr206, 0);
        idt[207].Set(_Isr207, 0);
        idt[208].Set(_Isr208, 0);
        idt[209].Set(_Isr209, 0);
        idt[210].Set(_Isr210, 0);
        idt[211].Set(_Isr211, 0);
        idt[212].Set(_Isr212, 0);
        idt[213].Set(_Isr213, 0);
        idt[214].Set(_Isr214, 0);
        idt[215].Set(_Isr215, 0);
        idt[216].Set(_Isr216, 0);
        idt[217].Set(_Isr217, 0);
        idt[218].Set(_Isr218, 0);
        idt[219].Set(_Isr219, 0);
        idt[220].Set(_Isr220, 0);
        idt[221].Set(_Isr221, 0);
        idt[222].Set(_Isr222, 0);
        idt[223].Set(_Isr223, 0);
        idt[224].Set(_Isr224, 0);
        idt[225].Set(_Isr225, 0);
        idt[226].Set(_Isr226, 0);
        idt[227].Set(_Isr227, 0);
        idt[228].Set(_Isr228, 0);
        idt[229].Set(_Isr229, 0);
        idt[230].Set(_Isr230, 0);
        idt[231].Set(_Isr231, 0);
        idt[232].Set(_Isr232, 0);
        idt[233].Set(_Isr233, 0);
        idt[234].Set(_Isr234, 0);
        idt[235].Set(_Isr235, 0);
        idt[236].Set(_Isr236, 0);
        idt[237].Set(_Isr237, 0);
        idt[238].Set(_Isr238, 0);
        idt[239].Set(_Isr239, 0);
        idt[240].Set(_Isr240, 0);
        idt[241].Set(_Isr241, 0);
        idt[242].Set(_Isr242, 0);
        idt[243].Set(_Isr243, 0);
        idt[244].Set(_Isr244, 0);
        idt[245].Set(_Isr245, 0);
        idt[246].Set(_Isr246, 0);
        idt[247].Set(_Isr247, 0);
        idt[248].Set(_Isr248, 0);
        idt[249].Set(_Isr249, 0);
        idt[250].Set(_Isr250, 0);
        idt[251].Set(_Isr251, 0);
        idt[252].Set(_Isr252, 0);
        idt[253].Set(_Isr253, 0);
        idt[254].Set(_Isr254, 0);
        idt[255].Set(_Isr255, 0);

        if (cpu_info.using_apic)
            idt[apic::spurious_int_vec].Set(_IsrSpurious, 0);

        __lidt(&idt_desc);
    }

    EARLY static void InitInterrupts()
    {
        if (cpu_info.pic_present)
            pic::InitializeController();

        // FIXME does not work on real hardware
        if (cpu_info.using_apic)
        {
            Print("Using APIC for interrupts.\n");

            apic::InitializeController();
            mask_interrupts = apic::MaskInterrupts;
            unmask_interrupts = apic::UnmaskInterrupts;
            send_eoi = apic::SendEoi;
        }
        else if (cpu_info.pic_present)
        {
            mask_interrupts = pic::MaskInterrupts;
            unmask_interrupts = pic::UnmaskInterrupts;
            send_eoi = pic::SendEoi;
        }
        else
        {
            ke::Panic(Status::UnsupportedSystem);
        }

        // Interrupts are still masked at this point.
        _enable();
        EnableNmi();
    }

#pragma pack(1)
    union SegmentSelector
    {
        struct
        {
            u16 rpl : 2;
            u16 table : 1; // 0: GDT, 1: LDT
            u16 index : 13;
        };
        u16 bits;
    };
    static_assert(sizeof SegmentSelector == sizeof u16);

    union Ia32Star
    {
        struct
        {
            u32 reserved;
            u16 syscall_cs;
            u16 sysret_cs;
        };
        u64 bits;
    };
    static_assert(sizeof Ia32Star == sizeof u64);
#pragma pack()

    enum class Msr : u32
    {
        IA32_FS_BASE = 0xc0000100,
        IA32_GS_BASE = 0xc0000101,
        IA32_KERNEL_GS_BASE = 0xc0000102,

        IA32_EFER = 0xc0000080,
        IA32_STAR = 0xc0000081,
        IA32_LSTAR = 0xc0000082, // Target RIP
        IA32_FMASK = 0xc0000084,
    };

    EXTERN_C void x64SysCall();

    EARLY void Initialize(uptr_t kernel_stack)
    {
        kernel_tss.rsp0 = kernel_stack;

        // Fill out cpu_info and initialize CR0
        CheckFeatures();
        SetCr0Bits();

        // Enable extended features
        SetCr4Bits();

        // Update PAT
        LoadPageAttributeTable();

        // Set up descriptor tables
        InitGdt();
        InitIdt();

        // Initialize interrupt controllers
        InitInterrupts();

        // TODO - check via cpuid if syscall/sysret is supported

        constexpr auto star = ((( u64 )GetGdtOffset(GdtIndex::R0Code)) << 32) |
            /* sysret: 8 is added for SS which gets us to R3Data,
               16 is added for CS which is R3Code */
            (((( u64 )GetGdtOffset(GdtIndex::R3Code32) | 3)) << 48);
        __writemsr(( u32 )Msr::IA32_STAR, star);

        // TODO - rflags enum
        __writemsr(( u32 )Msr::IA32_LSTAR, ( u64 )x64SysCall);
        __writemsr(( u32 )Msr::IA32_FMASK, 0x200 | 0x100); // Mask interrupts and traps on syscall

        auto efer = __readmsr(( u32 )Msr::IA32_EFER);
        __writemsr(( u32 )Msr::IA32_EFER, efer | 0x1); // Enable syscall extensions
    }
}
