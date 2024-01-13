#pragma once

#include <libc/mem.h>

namespace x64
{
    enum class CpuidLeaf : u32
    {
        VendorString = 0x0,
        Info = 0x1,
        ExtendedFeatures = 0x7,
        ExtendedInfo = 0x80000001,
    };

    enum class CpuidFeature
    {
        // EDX (leaf 0x1) or EDX (leaf 0x80000001).
        // AMD-specific features are listed at the end.
        FPU = 1 << 0,
        VME = 1 << 1,
        DE = 1 << 2,
        PSE = 1 << 3,
        TSC = 1 << 4,
        MSR = 1 << 5,
        PAE = 1 << 6,
        MCE = 1 << 7,
        CX8 = 1 << 8,
        APIC = 1 << 9,
        INTEL_SEP = 1 << 11,
        MTRR = 1 << 12,
        PGE = 1 << 13,
        MCA = 1 << 14,
        CMOV = 1 << 15,
        PAT = 1 << 16,
        PSE36 = 1 << 17,
        INTEL_PSN = 1 << 18,
        INTEL_CLFSH = 1 << 19,
        INTEL_DS = 1 << 21,
        INTEL_ACPI = 1 << 22,
        MMX = 1 << 23,
        FXSR = 1 << 24,
        SSE = 1 << 25,
        SSE2 = 1 << 26,
        INTEL_SS = 1 << 27,
        INTEL_HTT = 1 << 28,
        INTEL_TM = 1 << 29,
        INTEL_IA64 = 1 << 30,
        INTEL_PBE = 1 << 31,

        // ECX (leaf 0x1)
        SSE3 = 1 << 0,
        PCLMULQDQ = 1 << 1,
        DTES64 = 1 << 2,
        MONITOR = 1 << 3,
        DS_CPL = 1 << 4,
        VMX = 1 << 5,
        SMX = 1 << 6,
        EST = 1 << 7,
        TM2 = 1 << 8,
        SSSE3 = 1 << 9,
        CNXT_ID = 1 << 10,
        SDBG = 1 << 11,
        FMA = 1 << 12,
        CX16 = 1 << 13,
        XTPR = 1 << 14,
        PDCM = 1 << 15,
        PCID = 1 << 17,
        DCA = 1 << 18,
        SSE4_1 = 1 << 19,
        SSE4_2 = 1 << 20,
        X2APIC = 1 << 21,
        MOVBE = 1 << 22,
        POPCNT = 1 << 23,
        TSC_DEADLINE = 1 << 24,
        AES_NI = 1 << 25,
        XSAVE = 1 << 26,
        OSXSAVE = 1 << 27,
        AVX = 1 << 28,
        F16C = 1 << 29,
        RDRND = 1 << 30,
        HYPERVISOR = 1 << 31, // Reserved on AMD but seems to be identical...

        // EBX (leaf 0x7)
        FSGSBASE = 1 << 0,
        TSC_ADJUST = 1 << 1,
        SGX = 1 << 2,
        BMI1 = 1 << 3,
        HLE = 1 << 4,
        AVX2 = 1 << 5,
        FDP_EXCPTN_ONLY = 1 << 6,
        SMEP = 1 << 7,
        BMI2 = 1 << 8,
        ERMS = 1 << 9,
        INVPCID = 1 << 10,
        RTM = 1 << 11,
        PQM = 1 << 12,
        FPU_CS_DS_DEPRECATED = 1 << 13,
        MPX = 1 << 14,
        PQE = 1 << 15,
        AVX512_F = 1 << 16,
        AVX512_DQ = 1 << 17,
        RDSEED = 1 << 18,
        ADX = 1 << 19,
        SMAP = 1 << 20,
        AVX512_IFMA = 1 << 21,
        PCOMMIT = 1 << 22,
        CLFLUSHOPT = 1 << 23,
        CLWB = 1 << 24,
        INTEL_PT = 1 << 25,
        AVX512_PF = 1 << 26,
        AVX512_ER = 1 << 27,
        AVX512_CD = 1 << 28,
        SHA = 1 << 29,
        AVX512_BW = 1 << 30,
        AVX512_VL = 1 << 31,

        // ECX (leaf 0x7)
        PREFETCHWT1 = 1 << 0,
        AVX512_VBMI = 1 << 1,
        UMIP = 1 << 2,
        PKU = 1 << 3,
        OSPKE = 1 << 4,
        WAITPKG = 1 << 5,
        AVX512_VBMI2 = 1 << 6,
        CET_SS = 1 << 7,
        GFNI = 1 << 8,
        VAES = 1 << 9,
        VPCLMULQDQ = 1 << 10,
        AVX512_VNNI = 1 << 11,
        AVX512_BITALG = 1 << 12,
        TME_EN = 1 << 13,
        AVX512_VPOPCNTDQ = 1 << 14,
        LVL5PG = 1 << 16,
        MAWAU = 1 << 17, // 17 to 21
        RDPID = 1 << 22,
        KL = 1 << 23,
        CIDEMOTE = 1 << 25,
        MOVDIRI = 1 << 27,
        MOVDIR64B = 1 << 28,
        ENQCMD = 1 << 29,
        SGX_LC = 1 << 30,
        PKS = 1 << 31,

        // EDX (leaf 0x7)
        AVX512_4VNNIW = 1 << 2,
        AVX512_4FMAPS = 1 << 3,
        FSRM = 1 << 4,
        UINTR = 1 << 5,
        AVX512_VP2INTERSECT = 1 << 8,
        SRBDS_CTRL = 1 << 9,
        MD_CLEAR = 1 << 10,
        RTM_ALWAYS_ABORT = 1 << 11,
        TSX_FORCE_ABORT = 1 << 13,
        SERIALIZE = 1 << 14,
        HYBRID = 1 << 15,
        TSXLDTRK = 1 << 16,
        PCONFIG = 1 << 18,
        LBR = 1 << 19,
        CET_IBT = 1 << 20,
        AMX_BF16 = 1 << 22,
        AVX512_FP16 = 1 << 23,
        AMX_TILE = 1 << 24,
        AMX_INT8 = 1 << 25,
        SPEC_CTRL = 1 << 26, // IBRS and IBPB
        STIBP = 1 << 27,
        L1D_FLUSH = 1 << 28,
        IA32_ARCH_CAPS = 1 << 29,
        IA32_CORE_CAPS = 1 << 30,
        SSBD = 1 << 31,

        // EDX (leaf 0x80000001)
        SYSCALL = 1 << 11,
        MP = 1 << 19,
        NX = 1 << 20,
        MMXEXT = 1 << 22,
        FXSR_OPT = 1 << 25,
        PDPE1GB = 1 << 26,
        RDTSCP = 1 << 27,
        LM = 1 << 29,
        _3DNOW_EXT = 1 << 30,
        _3DNOW = 1 << 31,
    };

    union Cpuid
    {
        struct
        {
            i32 eax, ebx, ecx, edx;
        };
        i32 data[4]{};

        Cpuid(CpuidLeaf leaf, i32 subleaf = 0)
        {
            __cpuidex(data, ( i32 )leaf, subleaf);
        }
    };

    INLINE char* GetVendorString(char str[13])
    {
        Cpuid ids(CpuidLeaf::VendorString);
        memcpy(str, &ids.ebx, 4);
        memcpy(str + 4, &ids.edx, 4);
        memcpy(str + 8, &ids.ecx, 4);
        str[12] = '\0';
        return str;
    }

    INLINE bool CheckCpuid(u32 reg, CpuidFeature feature)
    {
        return reg & ( u32 )feature;
    }
}
