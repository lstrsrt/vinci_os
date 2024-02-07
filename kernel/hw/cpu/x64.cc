#include <ec/array.h>

#include "x64.h"
#include "cpuid.h"
#include "isr.h"
#include "msr.h"
#include "../../core/ke.h"
#include "../../core/gfx/output.h"

namespace x64
{
    EXTERN_C_START
    void ReloadSegments(u16 code_selector, u16 data_selector);
    void LoadTr(u16 offset);
    void x64Syscall();
    EXTERN_C_END

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

    alignas(64) static ec::array<IdtEntry, 256> idt{
        IdtEntry(_Isr0, 0), // Only has to be explicit the first time
        { _Isr1, 0, DebugIst },
        { _Isr2, 0, NmiIst },
        { _Isr3, 0, DebugIst },
        { _Isr4, 0 },
        { _Isr5, 0 },
        { _Isr6, 0 },
        { _Isr7, 0 },
        { _Isr8, 0, DoubleFaultIst },
        { _Isr9, 0 },
        { _Isr10, 0 },
        { _Isr11, 0 },
        { _Isr12, 0 },
        { _Isr13, 0 },
        { _Isr14, 0 },
        { _Isr15, 0 },
        { _Isr16, 0 },
        { _Isr17, 0 },
        { _Isr18, 0, MachineCheckIst },
        { _Isr19, 0 },
        { _Isr20, 0 },
        { _Isr21, 0 },
        { _Isr22, 0 },
        { _Isr23, 0 },
        { _Isr24, 0 },
        { _Isr25, 0 },
        { _Isr26, 0 },
        { _Isr27, 0 },
        { _Isr28, 0 },
        { _Isr29, 0 },
        { _Isr30, 0 },
        { _Isr31, 0 },
        { _Isr32, 0 },
        { _Isr33, 0 },
        { _Isr34, 0 },
        { _Isr35, 0 },
        { _Isr36, 0 },
        { _Isr37, 0 },
        { _Isr38, 0 },
        { _Isr39, 0 },
        { _Isr40, 0 },
        { _Isr41, 0 },
        { _Isr42, 0 },
        { _Isr43, 0 },
        { _Isr44, 0 },
        { _Isr45, 0 },
        { _Isr46, 0 },
        { _Isr47, 0 },
        { _Isr48, 0 },
        { _Isr49, 0 },
        { _Isr50, 0 },
        { _Isr51, 0 },
        { _Isr52, 0 },
        { _Isr53, 0 },
        { _Isr54, 0 },
        { _Isr55, 0 },
        { _Isr56, 0 },
        { _Isr57, 0 },
        { _Isr58, 0 },
        { _Isr59, 0 },
        { _Isr60, 0 },
        { _Isr61, 0 },
        { _Isr62, 0 },
        { _Isr63, 0 },
        { _Isr64, 0 },
        { _Isr65, 0 },
        { _Isr66, 0 },
        { _Isr67, 0 },
        { _Isr68, 0 },
        { _Isr69, 0 },
        { _Isr70, 0 },
        { _Isr71, 0 },
        { _Isr72, 0 },
        { _Isr73, 0 },
        { _Isr74, 0 },
        { _Isr75, 0 },
        { _Isr76, 0 },
        { _Isr77, 0 },
        { _Isr78, 0 },
        { _Isr79, 0 },
        { _Isr80, 0 },
        { _Isr81, 0 },
        { _Isr82, 0 },
        { _Isr83, 0 },
        { _Isr84, 0 },
        { _Isr85, 0 },
        { _Isr86, 0 },
        { _Isr87, 0 },
        { _Isr88, 0 },
        { _Isr89, 0 },
        { _Isr90, 0 },
        { _Isr91, 0 },
        { _Isr92, 0 },
        { _Isr93, 0 },
        { _Isr94, 0 },
        { _Isr95, 0 },
        { _Isr96, 0 },
        { _Isr97, 0 },
        { _Isr98, 0 },
        { _Isr99, 0 },
        { _Isr100, 0 },
        { _Isr101, 0 },
        { _Isr102, 0 },
        { _Isr103, 0 },
        { _Isr104, 0 },
        { _Isr105, 0 },
        { _Isr106, 0 },
        { _Isr107, 0 },
        { _Isr108, 0 },
        { _Isr109, 0 },
        { _Isr110, 0 },
        { _Isr111, 0 },
        { _Isr112, 0 },
        { _Isr113, 0 },
        { _Isr114, 0 },
        { _Isr115, 0 },
        { _Isr116, 0 },
        { _Isr117, 0 },
        { _Isr118, 0 },
        { _Isr119, 0 },
        { _Isr120, 0 },
        { _Isr121, 0 },
        { _Isr122, 0 },
        { _Isr123, 0 },
        { _Isr124, 0 },
        { _Isr125, 0 },
        { _Isr126, 0 },
        { _Isr127, 0 },
        { _Isr128, 0 },
        { _Isr129, 0 },
        { _Isr130, 0 },
        { _Isr131, 0 },
        { _Isr132, 0 },
        { _Isr133, 0 },
        { _Isr134, 0 },
        { _Isr135, 0 },
        { _Isr136, 0 },
        { _Isr137, 0 },
        { _Isr138, 0 },
        { _Isr139, 0 },
        { _Isr140, 0 },
        { _Isr141, 0 },
        { _Isr142, 0 },
        { _Isr143, 0 },
        { _Isr144, 0 },
        { _Isr145, 0 },
        { _Isr146, 0 },
        { _Isr147, 0 },
        { _Isr148, 0 },
        { _Isr149, 0 },
        { _Isr150, 0 },
        { _Isr151, 0 },
        { _Isr152, 0 },
        { _Isr153, 0 },
        { _Isr154, 0 },
        { _Isr155, 0 },
        { _Isr156, 0 },
        { _Isr157, 0 },
        { _Isr158, 0 },
        { _Isr159, 0 },
        { _Isr160, 0 },
        { _Isr161, 0 },
        { _Isr162, 0 },
        { _Isr163, 0 },
        { _Isr164, 0 },
        { _Isr165, 0 },
        { _Isr166, 0 },
        { _Isr167, 0 },
        { _Isr168, 0 },
        { _Isr169, 0 },
        { _Isr170, 0 },
        { _Isr171, 0 },
        { _Isr172, 0 },
        { _Isr173, 0 },
        { _Isr174, 0 },
        { _Isr175, 0 },
        { _Isr176, 0 },
        { _Isr177, 0 },
        { _Isr178, 0 },
        { _Isr179, 0 },
        { _Isr180, 0 },
        { _Isr181, 0 },
        { _Isr182, 0 },
        { _Isr183, 0 },
        { _Isr184, 0 },
        { _Isr185, 0 },
        { _Isr186, 0 },
        { _Isr187, 0 },
        { _Isr188, 0 },
        { _Isr189, 0 },
        { _Isr190, 0 },
        { _Isr191, 0 },
        { _Isr192, 0 },
        { _Isr193, 0 },
        { _Isr194, 0 },
        { _Isr195, 0 },
        { _Isr196, 0 },
        { _Isr197, 0 },
        { _Isr198, 0 },
        { _Isr199, 0 },
        { _Isr200, 0 },
        { _Isr201, 0 },
        { _Isr202, 0 },
        { _Isr203, 0 },
        { _Isr204, 0 },
        { _Isr205, 0 },
        { _Isr206, 0 },
        { _Isr207, 0 },
        { _Isr208, 0 },
        { _Isr209, 0 },
        { _Isr210, 0 },
        { _Isr211, 0 },
        { _Isr212, 0 },
        { _Isr213, 0 },
        { _Isr214, 0 },
        { _Isr215, 0 },
        { _Isr216, 0 },
        { _Isr217, 0 },
        { _Isr218, 0 },
        { _Isr219, 0 },
        { _Isr220, 0 },
        { _Isr221, 0 },
        { _Isr222, 0 },
        { _Isr223, 0 },
        { _Isr224, 0 },
        { _Isr225, 0 },
        { _Isr226, 0 },
        { _Isr227, 0 },
        { _Isr228, 0 },
        { _Isr229, 0 },
        { _Isr230, 0 },
        { _Isr231, 0 },
        { _Isr232, 0 },
        { _Isr233, 0 },
        { _Isr234, 0 },
        { _Isr235, 0 },
        { _Isr236, 0 },
        { _Isr237, 0 },
        { _Isr238, 0 },
        { _Isr239, 0 },
        { _Isr240, 0 },
        { _Isr241, 0 },
        { _Isr242, 0 },
        { _Isr243, 0 },
        { _Isr244, 0 },
        { _Isr245, 0 },
        { _Isr246, 0 },
        { _Isr247, 0 },
        { _Isr248, 0 },
        { _Isr249, 0 },
        { _Isr250, 0 },
        { _Isr251, 0 },
        { _Isr252, 0 },
        { _Isr253, 0 },
        { _Isr254, 0 },
        { _Isr255, 0 }
    };

    alignas(sizeof(u16)) static DescriptorTable gdt_desc(&gdt, sizeof gdt - 1);
    alignas(sizeof(u16)) static DescriptorTable idt_desc(&idt, sizeof idt - 1);
#pragma data_seg()

    EARLY static void GetCpuModel()
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
    }

    NO_RETURN EARLY static void CpuidPanic(const char* feature)
    {
        Print("\nMissing required feature: %s\n", feature);
        ke::Panic(Status::UnsupportedSystem);
    }

#define CheckRequiredFeature(reg, feat) \
    if (!CheckCpuid(reg, feat)) { \
        CpuidPanic(#feat); \
    } else { \
        Print("%s", #feat " "); \
    }

#define CheckOptionalFeature(info, reg, feat) \
    info = CheckCpuid(reg, feat); \
    if (info) { \
        Print("%s", #feat " "); \
    }

    EARLY static void CheckFeatures()
    {
        using enum CpuidFeature;

        Print("CPUID: ");

        {
            Cpuid ids(CpuidLeaf::Info);

            CheckOptionalFeature(cpu_info.tsc_supported, ids.ecx, TSC);
            CheckOptionalFeature(cpu_info.hypervisor, ids.ecx, HYPERVISOR);
            CheckOptionalFeature(cpu_info.has_x2apic, ids.ecx, X2APIC);

            // TODO - do this when it works on hardware
            // CheckOptionalFeature(cpu_info.using_apic, ids.edx, APIC);
        }

        {
            Cpuid ids(CpuidLeaf::ExtendedInfo);

            CheckRequiredFeature(ids.edx, SYSCALL);
        }

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

        cpu_info.smap_supported = CheckCpuid(ids.ebx, CpuidFeature::SMAP);
        if (cpu_info.smap_supported)
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
        // This is the same as the default PAT entries
        // but with one exception: PAT4 selects WC instead of WB
        static constexpr auto pat_entries = (
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
        WriteMsr(MSR_PAT, pat_entries);
        __wbinvd();
        TlbFlush();
    }

    EARLY static void LoadGdt(x64::DescriptorTable* desc)
    {
        _lgdt(( uptr_t* )desc);
        ReloadSegments(GetGdtOffset(GdtIndex::R0Code), GetGdtOffset(GdtIndex::R0Data));
        LoadTr(GetGdtOffset(GdtIndex::TssLow));
    }

    EARLY static void LoadIdt(x64::DescriptorTable* desc)
    {
        if (cpu_info.using_apic)
            idt[apic::spurious_int_vec].Set(_IsrSpurious, 0);

        __lidt(( uptr_t* )desc);
    }

    EARLY static void InitializeInterrupts()
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

    EARLY void InitializeSyscalls()
    {
        MsrStar star{};

        // On syscall:
        // ss = IA32_STAR[47:32] + 8
        // cs = IA32_STAR[47:32]
        star.syscall_cs.index = ( u16 )GdtIndex::R0Code;
        star.syscall_cs.rpl = 0;
        star.syscall_cs.table = TABLE_GDT;

        // On sysret:
        // ss = IA32_STAR[63:48] + 8
        // cs = IA32_STAR[63:48] + 16
        star.sysret_cs.index = ( u16 )GdtIndex::R3Code32;
        star.sysret_cs.rpl = 3;
        star.sysret_cs.table = TABLE_GDT;

        WriteMsr(Msr::STAR, star.bits);

        // Mask interrupts and traps on syscalls
        WriteMsr(Msr::LSTAR, ( uptr_t )x64Syscall);
        WriteMsr(Msr::FMASK, ( u64 )(RFLAG::IF | RFLAG::TF));

        // Enable syscall extensions
        auto efer = ReadMsr(Msr::EFER);
        WriteMsr(Msr::EFER, efer | EFER_SCE);
    }

    EXTERN_C u64 x64SyscallCxx(SyscallFrame* frame, u64 sys_no)
    {
        Print("Syscall number: 0x%llx\n", sys_no);

        if (sys_no == 1)
            Print("%d\n", ( int )frame->rdi);

        Print("Returning to 0x%llx with flags 0x%llx\n", frame->rcx, frame->rflags);

        return 0;
    }

    EARLY void Initialize(uptr_t kernel_stack)
    {
        kernel_tss.rsp0 = kernel_stack;

        GetCpuModel();
        CheckFeatures();
        SetCr0Bits();
        SetCr4Bits();

        LoadPageAttributeTable();

        LoadGdt(&gdt_desc);
        LoadIdt(&idt_desc);

        InitializeInterrupts();

        InitializeSyscalls();
    }
}
