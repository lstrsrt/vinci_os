#pragma once

/*
*  Shared x64 virtual memory mapping code.
*/

#include "../lib/libc/mem.h"

#include "va.h"

static constexpr size_t va_index_mask = 0x1ff;

static constexpr size_t va_pml4_shift = 39;
static constexpr size_t va_pdpt_shift = 30;
static constexpr size_t va_pd_shift   = 21;
static constexpr size_t va_pt_shift   = 12;

#define VA_PML4_INDEX(va)   ((va >> va_pml4_shift) & va_index_mask)
#define VA_PDPT_INDEX(va)   ((va >> va_pdpt_shift) & va_index_mask)
#define VA_PD_INDEX(va)     ((va >> va_pd_shift) & va_index_mask)
#define VA_PT_INDEX(va)     ((va >> va_pt_shift) & va_index_mask)

namespace x64
{
#pragma pack(1)
    union Pml4Entry
    {
        struct
        {
            u64 present : 1;
            u64 writable : 1;
            u64 user_mode : 1;
            u64 write_through : 1;
            u64 cache_disable : 1;
            u64 accessed : 1;
            u64 ignored0 : 1;
            u64 page_size : 1;
            u64 ignored1 : 4;
            u64 page_frame_number : 36;
            u64 reserved : 4;
            u64 ignored2 : 11;
            u64 execute_disable : 1;
        };
        u64 value;

        operator bool() { return value; }
    };
    static_assert(sizeof(Pml4Entry) == sizeof(u64));
    using Pml4 = Pml4Entry*;

    union PdptEntry
    {
        struct
        {
            u64 present : 1;
            u64 writable : 1;
            u64 user_mode : 1;
            u64 write_through : 1;
            u64 cache_disable : 1;
            u64 accessed : 1;
            u64 reserved0 : 6;
            u64 page_frame_number : 36;
            u64 reserved1 : 15;
            u64 execute_disable : 1;
        };
        u64 value;

        operator bool() { return value; }
    };
    static_assert(sizeof(PdptEntry) == sizeof(u64));
    using Pdpt = PdptEntry*;

    union PageDirEntry
    {
        struct
        {
            u64 present : 1;
            u64 writable : 1;
            u64 user_mode : 1;
            u64 write_through : 1;
            u64 cache_disable : 1;
            u64 accessed : 1;
            u64 ignored0 : 1;
            u64 page_size : 1;
            u64 ignored1 : 4;
            u64 page_frame_number : 36;
            u64 reserved : 4;
            u64 ignored2 : 11;
            u64 execute_disable : 1;
        };
        u64 value;

        operator bool() { return value; }
    };
    static_assert(sizeof(PageDirEntry) == sizeof(u64));
    using PageDir = PageDirEntry*;

    union PageTableEntry
    {
        struct
        {
            u64 present : 1;
            u64 writable : 1;
            u64 user_mode : 1;
            u64 write_through : 1;
            u64 cache_disable : 1;
            u64 accessed : 1;
            u64 dirty : 1;
            u64 pat : 1;
            u64 global : 1;
            u64 ignored0 : 3;
            u64 page_frame_number : 36;
            u64 reserved : 4;
            u64 ignored1 : 7;
            u64 protection_key : 4;
            u64 execute_disable : 1;
        };
        u64 value;

        operator bool() { return value; }
    };
    static_assert(sizeof(PageTableEntry) == sizeof(u64));
    using PageTable = PageTableEntry*;
#pragma pack()
}

namespace mm
{
    union PhysicalPage
    {
        struct
        {
            u8 present : 1;
            u8 reserved : 7; // to be used in the future
        };
        u8 value;
    };
    static_assert(sizeof(PhysicalPage) == sizeof(u8));

    struct PageTable;
    static size_t AllocatePhysical(PageTable&, paddr_t*);

    static constexpr size_t max_page_table_pages = (page_size / sizeof(PhysicalPage));

    // This structure is used to track physical pages in an address space.
    // It is not to be confused with x64::PageTable, which is architecture specific!
    // The first page in a table stores information about the remaining pages.
    // The second page is the PML4.
    // Remaining pages are used for the other paging structures (one page per entry).
    struct PageTable
    {
        PageTable(vaddr_t virt_base, paddr_t phys_base, size_t page_count, paddr_t cr3 = 0)
            : pages(page_count), virt(virt_base), phys(phys_base)
        {
            phys_info[0].present = true;
            if (cr3)
                root = cr3;
            else
                AllocatePhysical(*this, &root); // TODO - handle failure
        }

        size_t pages;
        union
        {
            vaddr_t virt;
            PhysicalPage* phys_info;
        };
        paddr_t phys;
        paddr_t root; // CR3 (currently one page after phys)
    };

    //
    // Some of these functions were adapted from
    // https://github.com/toddsharpe/MetalOS/blob/master/src/arch/x64/PageTables.cpp
    //

    static size_t AllocatePhysical(PageTable& table, paddr_t* phys_out)
    {
        for (size_t page = 1 /* Skip ourselves */; page < table.pages; page++)
        {
            // Skip present pages
            if (table.phys_info[page].present)
                continue;

            table.phys_info[page].present = true;
            *phys_out = table.phys + (page * page_size);

            return page;
        }

        return 0;
    }

    static size_t AllocatePhysical(PageTable& table, paddr_t* phys_out, size_t count)
    {
        size_t page = AllocatePhysical(table, phys_out);
        if (!page)
            return 0;

        paddr_t tmp;
        for (size_t i = 1; i < count; i++)
        {
            if (!AllocatePhysical(table, &tmp))
                return 0;
        }

        return page;
    }

    // Converts the physical address of a paging structure within a table to a virtual address
    INLINE vaddr_t GetPoolEntryVa(PageTable& table, paddr_t phys)
    {
        return (phys - table.phys) + table.virt;
    }

#define GetPml4Entry(pool, virt) \
    ((( x64::Pml4 )GetPoolEntryVa(pool, pool.root))[VA_PML4_INDEX(virt)])

#define GetPdptEntry(pool, pml4e, virt) \
    ((( x64::Pdpt )GetPoolEntryVa(pool, pml4e.value & ~page_mask))[VA_PDPT_INDEX(virt)])

#define GetPdEntry(pool, pdpte, virt) \
    ((( x64::PageDir )GetPoolEntryVa(pool, pdpte.value & ~page_mask))[VA_PD_INDEX(virt)])

#define GetPtEntry(pool, pde, virt) \
    ((( x64::PageTable )GetPoolEntryVa(pool, pde.value & ~page_mask))[VA_PT_INDEX(virt)])

    static x64::PageTableEntry* GetPresentPte(PageTable& table, vaddr_t virt)
    {
        const auto pml4e = GetPml4Entry(table, virt);
        if (pml4e.present)
        {
            const auto pdpte = GetPdptEntry(table, pml4e, virt);
            if (pdpte.present)
            {
                const auto pde = GetPdEntry(table, pdpte, virt);
                if (pde.present)
                    return &GetPtEntry(table, pde, virt);
            }
        }

        return nullptr;
    }

    INLINE bool IsPagePresent(PageTable& table, vaddr_t virt)
    {
        auto pte = GetPresentPte(table, virt);
        return pte && pte->present;
    }

    INLINE bool UnmapPage(PageTable& table, vaddr_t virt)
    {
        auto pte = GetPresentPte(table, virt);
        if (pte)
            pte->present = false;
        return pte;
    }

    static x64::PageTableEntry* MapPage(PageTable& table, vaddr_t virt, paddr_t phys, bool user = false)
    {
        if (!IsPageAligned(virt) || !IsPageAligned(phys))
            return nullptr;

        paddr_t physical_entry;

        auto& pml4e = GetPml4Entry(table, virt);
        if (!pml4e)
        {
            // If this VA indexed a PDPTE that does not exist yet, allocate one from the pool.
            // It is reused for all other VAs with the same index.
            if (!AllocatePhysical(table, &physical_entry))
                return nullptr;

            pml4e.value = physical_entry; // same as PFN = physical_entry * PAGE_SIZE
            pml4e.present = true;
            pml4e.writable = true;
            pml4e.user_mode = user;
        }

        auto& pdpte = GetPdptEntry(table, pml4e, virt);
        if (!pdpte)
        {
            if (!AllocatePhysical(table, &physical_entry))
                return nullptr;

            pdpte.value = physical_entry;
            pdpte.present = true;
            pdpte.writable = true;
            pdpte.user_mode = user;
        }

        auto& pde = GetPdEntry(table, pdpte, virt);
        if (!pde)
        {
            if (!AllocatePhysical(table, &physical_entry))
                return nullptr;

            pde.value = physical_entry;
            pde.present = true;
            pde.writable = true;
            pde.user_mode = user;
        }

        auto* pte = &GetPtEntry(table, pde, virt);
        pte->value = phys;
        pte->present = true;
        pte->writable = true;
        pte->global = false;
        pte->user_mode = user;

        return pte;
    }

    static bool MapPages(PageTable& table, vaddr_t virt, paddr_t phys, size_t count)
    {
        for (vaddr_t page_offset = 0; page_offset < (count * page_size); page_offset += page_size)
        {
            vaddr_t va = virt + page_offset;
            paddr_t pa = phys + page_offset;

            if (!MapPage(table, va, pa))
                return false;
        }

        return true;
    }

    // phys_virt is the physical address on input and the virtual address on return (if successful).
    template<Region rg>
    static bool MapPagesInRegion(PageTable& table, uptr_t* phys_virt, size_t count)
    {
        if ((*phys_virt + count * page_size) > rg.End())
            return false;

        for (vaddr_t page = rg.base; page < (page + rg.size); page += page_size)
        {
            if (IsPagePresent(table, page))
                continue;

            // TODO - lookahead
            // currently we alloc sequentially so it doesn't matter

            if (MapPages(table, page, *phys_virt, count))
            {
                *phys_virt = page;
                return true;
            }

            break;
        }

        return false;
    }
}
