#pragma once

#include <stdint.h>
#include "../lib/ec/concepts.h"

namespace pe
{
    static constexpr u16 dos_magic = 0x5a4d;
    static constexpr u32 nt_signature = 0x4550;
    static constexpr u32 optional_header_magic = 0x10b;

    enum class Machine : u16
    {
        i386 = 0x14c,
        x64 = 0x8664,
    };

    struct DosHeader
    {
        u16 e_magic;    // Magic number
        u16 e_cblp;     // Bytes on last page of file
        u16 e_cp;       // Pages in file
        u16 e_crlc;     // Relocations
        u16 e_cparhdr;  // Size of header in paragraphs
        u16 e_minalloc; // Minimum extra paragraphs needed
        u16 e_maxalloc; // Maximum extra paragraphs needed
        u16 e_ss;       // Initial (relative) SS value
        u16 e_sp;       // Initial SP value
        u16 e_csum;     // Checksum
        u16 e_ip;       // Initial IP value
        u16 e_cs;       // Initial (relative) CS value
        u16 e_lfarlc;   // File address of relocation table
        u16 e_ovno;     // Overlay number
        u16 e_res[4];   // Reserved words
        u16 e_oemid;    // OEM identifier (for e_oeminfo)
        u16 e_oeminfo;  // OEM information; e_oemid specific
        u16 e_res2[10]; // Reserved words
        u32 e_lfanew;   // File address of new exe header
        
        inline bool IsValid()
        {
            return this->e_magic == dos_magic;
        }
    };

    struct FileHeader
    {
        Machine Machine;
        u16 NumberOfSections;
        u32 TimeDateStamp;
        u32 PointerToSymbolTable;
        u32 NumberOfSymbols;
        u16 SizeOfOptionalHeader;
        u16 Characteristics;
    };

    struct DataDirectory
    {
        u32 VirtualAddress;
        u32 Size;
    };

    struct OptionalHeader64
    {
        u16 Magic;
        u8 MajorLinkerVersion;
        u8 MinorLinkerVersion;
        u32 SizeOfCode;
        u32 SizeOfInitializedData;
        u32 SizeOfUninitializedData;
        u32 AddressOfEntryPoint;
        u32 BaseOfCode;
        u64 ImageBase;
        u32 SectionAlignment;
        u32 FileAlignment;
        u16 MajorOperatingSystemVersion;
        u16 MinorOperatingSystemVersion;
        u16 MajorImageVersion;
        u16 MinorImageVersion;
        u16 MajorSubsystemVersion;
        u16 MinorSubsystemVersion;
        u32 Win32VersionValue;
        u32 SizeOfImage;
        u32 SizeOfHeaders;
        u32 CheckSum;
        u16 Subsystem;
        u16 DllCharacteristics;
        u64 SizeOfStackReserve;
        u64 SizeOfStackCommit;
        u64 SizeOfHeapReserve;
        u64 SizeOfHeapCommit;
        u32 LoaderFlags;
        u32 NumberOfRvaAndSizes;
        DataDirectory DataDirectory[16];
    };

    using OptionalHeader = OptionalHeader64;

    struct NtHeaders64
    {
        u32 Signature;
        FileHeader FileHeader;
        OptionalHeader64 OptionalHeader;
        
        inline bool IsValid()
        {
            return this->Signature == nt_signature;
        }
    };

    using NtHeaders = NtHeaders64;
    
    static constexpr size_t section_name_size = 8;

    struct Section
    {
        u8 Name[section_name_size];
        union
        {
            u32 PhysicalAddress;
            u32 VirtualSize;
        } Misc;
        u32 VirtualAddress;
        u32 SizeOfRawData;
        u32 PointerToRawData;
        u32 PointerToRelocations;
        u32 PointerToLinenumbers;
        u16 NumberOfRelocations;
        u16 NumberOfLinenumbers;
        u32 Characteristics;
    };

    enum SectionFlag
    {
        Discardable = 0x02000000,
        NotCached = 0x04000000,
        NotPaged = 0x08000000,
        Shared = 0x10000000,
        Execute = 0x20000000,
        Read = 0x40000000,
        Write = 0x80000000,
    };

    inline DosHeader* GetDosHeader(void* image_base)
    {
        auto dos_header = ( DosHeader* )image_base;
        if (!dos_header || dos_header->e_magic != dos_magic)
            return nullptr;
        return dos_header;
    }

    inline NtHeaders* GetNtHeaders(void* base, DosHeader* dos_header)
    {
        auto nt_headers = ( NtHeaders* )(( uintptr_t )base + dos_header->e_lfanew);
        if (!nt_headers || nt_headers->Signature != nt_signature)
            return nullptr;
        return nt_headers;
    }

    // My version of IMAGE_FIRST_SECTION
    inline Section* GetFirstSection(NtHeaders* nt_headers)
    {
        return ( Section* )(
            ( uintptr_t )nt_headers +
            OFFSET(NtHeaders, OptionalHeader) +
            nt_headers->FileHeader.SizeOfOptionalHeader
        );
    }

    struct File
    {
        DosHeader* m_dos_header;
        NtHeaders* m_nt_headers;

        template<class F> requires(ec::$predicate<F, Section*>)
        void ForEachSection(const F&& callback)
        {
            auto section = GetFirstSection(m_nt_headers);
            auto section_count = m_nt_headers->FileHeader.NumberOfSections;

            while (section && section_count--)
            {
                if (!callback(section))
                    return;

                section++;
            }
        }

        static File FromImageBase(void* base)
        {
            File pe;
            pe.m_dos_header = GetDosHeader(base);
            pe.m_nt_headers = GetNtHeaders(base, pe.m_dos_header);
            return pe;
        }
    };
}
