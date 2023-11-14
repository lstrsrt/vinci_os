#include "x64.h"
#include "cpuid.h"
#include "isr.h"
#include "../../core/gfx/output.h"

namespace x64
{
    volatile u8 int_stack_tables[IstCount][ist_size]{};

    static Tss tss{
        ( u64 )(int_stack_tables[0] + ist_size),
        ( u64 )(int_stack_tables[1] + ist_size),
        ( u64 )(int_stack_tables[2] + ist_size),
        ( u64 )(int_stack_tables[3] + ist_size)
    };

    static GdtEntry gdt[]{
        GdtEntry::Null(),
        GdtEntry(0, GDT_ENTRY_CODE | GDT_READ),
        GdtEntry(0, GDT_ENTRY_DATA | GDT_WRITE),
        GdtEntry(3, GDT_ENTRY_CODE | GDT_READ),
        GdtEntry(3, GDT_ENTRY_DATA | GDT_WRITE),
        GdtEntry::TssLow(&tss),
        GdtEntry::TssHigh(&tss)
    };

    static IdtEntry idt[256]{};

    static DescriptorTable gdt_desc(&gdt, sizeof gdt - 1);
    static DescriptorTable idt_desc(&idt, sizeof idt - 1);

    static void CheckFeatures()
    {
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

        Print("\n");
    }

    static void SetCr0Bits()
    {
        // TODO - set ET?
        WriteCr0(ReadCr0() & ~(Cr0::MP | Cr0::NE));
    }

    static void SetCr4Bits()
    {
        Cpuid ids(CpuidLeaf::ExtendedFeatures);

        Print("CR4: ");

        if (CheckCpuid(ids.ebx, CpuidFeature::SMEP))
        {
            WriteCr4(ReadCr4() | Cr4::SMEP);
            Print("SMEP ");
        }

        if (cpu_info.smap_supported =
            CheckCpuid(ids.ebx, CpuidFeature::SMAP))
        {
            WriteCr4(ReadCr4() | Cr4::SMAP);
            Print("SMAP ");
        }

        if (CheckCpuid(ids.ecx, CpuidFeature::UMIP))
        {
            WriteCr4(ReadCr4() | Cr4::UMIP);
            Print("UMIP ");
        }

        Print("\n");
    }

    void InitGdt()
    {
        _lgdt(&gdt_desc);
        ReloadSegments(gdt_offset::r0_code, gdt_offset::r0_data);
        LoadTr(gdt_offset::tss_low);
    }

    void InitIdt()
    {
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

        for (u64 i = 48; i < max_idt_entry; i++)
            idt[i].Set(_IsrUnexpected, 0);

        __lidt(&idt_desc);
    }

    void InitInterrupts()
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
        else
        {
            mask_interrupts = pic::MaskInterrupts;
            unmask_interrupts = pic::UnmaskInterrupts;
            send_eoi = pic::SendEoi;
        }

        // Interrupts are still masked at this point.
        _enable();
        EnableNmi();
    }

    void Initialize()
    {
        Print("CPU vendor: %s\n", GetVendorString(cpu_info.vendor_string));

        // Fill out cpu_info and initialize CR0
        CheckFeatures();
        SetCr0Bits();

        // Enable extended features
        SetCr4Bits();

        // Set up descriptor tables
        InitGdt();
        InitIdt();

        // Initialize interrupt controllers
        InitInterrupts();
    }
}