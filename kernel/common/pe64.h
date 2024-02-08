#pragma once

#include <stdint.h>

#include "../../boot/gnu-efi/x86_64/pe.h"

typedef uint64_t UINT64;
typedef uintptr_t UINTN;

typedef struct _IMAGE_OPTIONAL_HEADER64
{
    UINT16 Magic;
    UINT8 MajorLinkerVersion;
    UINT8 MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT64 SizeOfStackReserve;
    UINT64 SizeOfStackCommit;
    UINT64 SizeOfHeapReserve;
    UINT64 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER64, * PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64
{
    UINT32 Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, * PIMAGE_NT_HEADERS64;

namespace pe
{
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

    using Section = IMAGE_SECTION_HEADER;
    using NtHeaders = IMAGE_NT_HEADERS;
    using DosHeader = IMAGE_DOS_HEADER;

    inline DosHeader* GetDosHeader(void* image_base)
    {
        auto dos_header = ( IMAGE_DOS_HEADER* )image_base;
        if (!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE)
            return nullptr;
        return dos_header;
    }

    inline NtHeaders* GetNtHeaders(void* base, DosHeader* dos_header)
    {
        auto nt_headers = ( NtHeaders* )(( uintptr_t )base + dos_header->e_lfanew);
        if (!nt_headers || nt_headers->Signature != IMAGE_NT_SIGNATURE)
            return nullptr;
        return nt_headers;
    }

    struct File
    {
        DosHeader* m_dos_header;
        NtHeaders* m_nt_headers;

        void ForEachSection(const auto&& callback)
        {
            auto section = IMAGE_FIRST_SECTION(m_nt_headers);
            for (UINT16 i = 0; i < m_nt_headers->FileHeader.NumberOfSections; i++, section++)
            {
                if (!callback(section))
                    return;
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
