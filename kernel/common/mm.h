#pragma once

/*
*  Shared x64 virtual memory mapping code.
*/

#include <libc/mem.h>

#include "../common/va.h"

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
    static_assert(sizeof Pml4Entry == sizeof u64);
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
    static_assert(sizeof PdptEntry == sizeof u64);
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
    static_assert(sizeof PageDirEntry == sizeof u64);
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
    static_assert(sizeof PageTableEntry == sizeof u64);
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
    static_assert(sizeof PhysicalPage == sizeof u8);

    // One byte is one PageInfo, which means that one page can store information about 4096 pages.
    // So the maximum size of a page pool is 4096 pages, of which 4095 can be used.
    static constexpr size_t max_page_pool_pages = (page_size / sizeof PhysicalPage);

    // This structure is used when allocating physical pages.
    // The first page in a pool stores information about the remaining pages.
    // The second page is the PML4.
    // Remaining pages are used for the other paging structures (one page per entry).
    struct PagePool
    {
        PagePool(vaddr_t virtual_base, paddr_t physical_base, size_t page_count)
            : virt(virtual_base), phys(physical_base), pages(page_count)
        {
            phys_info[0].present = true;
        }

        size_t pages;
        union
        {
            vaddr_t virt;
            PhysicalPage* phys_info;
        };
        paddr_t phys;
        paddr_t root; // CR3 (currently one page after m_physical)
    };

    //
    // Some of these functions were adapted from
    // https://github.com/toddsharpe/MetalOS/blob/master/src/arch/x64/PageTables.cpp
    //

    size_t AllocatePhysical(PagePool& pool, paddr_t* phys_out)
    {
        for (size_t page = 1 /* Skip ourselves */; page < pool.pages; page++)
        {
            // Skip present pages
            if (pool.phys_info[page].present)
                continue;

            pool.phys_info[page].present = true;
            *phys_out = pool.phys + (page * page_size);

            return page;
        }

        return 0;
    }

    size_t AllocatePhysical(PagePool& pool, paddr_t* phys_out, size_t count)
    {
        size_t page = AllocatePhysical(pool, phys_out);
        if (!page)
            return 0;

        paddr_t tmp;
        for (size_t i = 1; i < count; i++)
        {
            if (!AllocatePhysical(pool, &tmp))
                return 0;
        }

        return page;
    }

    // Converts the physical address of a paging structure within a pool to a virtual address
    INLINE vaddr_t GetPoolEntryVa(PagePool& pool, paddr_t phys)
    {
        return (phys - pool.phys) + pool.virt;
    }

#define GetPml4Entry(pool, virt) \
    ((( x64::Pml4 )GetPoolEntryVa(pool, pool.root))[VA_PML4_INDEX(virt)])

#define GetPdptEntry(pool, pml4e, virt) \
    ((( x64::Pdpt )GetPoolEntryVa(pool, pml4e.value & ~page_mask))[VA_PDPT_INDEX(virt)])

#define GetPdEntry(pool, pdpte, virt) \
    ((( x64::PageDir )GetPoolEntryVa(pool, pdpte.value & ~page_mask))[VA_PD_INDEX(virt)])

#define GetPtEntry(pool, pde, virt) \
    ((( x64::PageTable )GetPoolEntryVa(pool, pde.value & ~page_mask))[VA_PT_INDEX(virt)])

    x64::PageTableEntry* GetPresentPte(PagePool& pool, vaddr_t virt)
    {
        const auto pml4e = GetPml4Entry(pool, virt);
        if (pml4e.present)
        {
            const auto pdpte = GetPdptEntry(pool, pml4e, virt);
            if (pdpte.present)
            {
                const auto pde = GetPdEntry(pool, pdpte, virt);
                if (pde.present)
                    return &GetPtEntry(pool, pde, virt);
            }
        }

        return nullptr;
    }

    bool IsPagePresent(PagePool& pool, vaddr_t virt)
    {
        auto pte = GetPresentPte(pool, virt);
        return pte ? pte->present : false;
    }

    bool UnmapPage(PagePool& pool, vaddr_t virt)
    {
        auto pte = GetPresentPte(pool, virt);
        if (pte)
            pte->present = false;
        return pte != nullptr;
    }

    x64::PageTableEntry* MapPage(PagePool& pool, vaddr_t virt, paddr_t phys, bool user = false)
    {
        if (!IsPageAligned(virt) || !IsPageAligned(phys))
            return nullptr;

        paddr_t physical_entry;

        auto& pml4e = GetPml4Entry(pool, virt);
        if (!pml4e)
        {
            // If this VA indexed a PDPTE that does not exist yet, allocate one from the pool.
            // It is reused for all other VAs with the same index.
            if (!AllocatePhysical(pool, &physical_entry))
                return nullptr;

            pml4e.value = physical_entry; // same as PFN = physical_entry * PAGE_SIZE
            pml4e.present = true;
            pml4e.writable = true;
            pml4e.user_mode = user;
        }

        auto& pdpte = GetPdptEntry(pool, pml4e, virt);
        if (!pdpte)
        {
            if (!AllocatePhysical(pool, &physical_entry))
                return nullptr;

            pdpte.value = physical_entry;
            pdpte.present = true;
            pdpte.writable = true;
            pdpte.user_mode = user;
        }

        auto& pde = GetPdEntry(pool, pdpte, virt);
        if (!pde)
        {
            if (!AllocatePhysical(pool, &physical_entry))
                return nullptr;

            pde.value = physical_entry;
            pde.present = true;
            pde.writable = true;
            pde.user_mode = user;
        }

        auto* pte = &GetPtEntry(pool, pde, virt);
        pte->value = phys;
        pte->present = true;
        pte->writable = true;
        pte->global = true;
        pte->user_mode = user;

        return pte;
    }

    bool MapPages(PagePool& pool, vaddr_t virt, paddr_t phys, size_t count)
    {
        for (vaddr_t page_offset = 0; page_offset < (count * page_size); page_offset += page_size)
        {
            vaddr_t va = virt + page_offset;
            paddr_t pa = phys + page_offset;

            if (!MapPage(pool, va, pa))
                return false;
        }

        return true;
    }

    // phys_virt is the physical address on input and the virtual address on return (if successful).
    template<Region rg>
    bool MapPagesInRegion(PagePool& pool, uptr_t* phys_virt, size_t count)
    {
        if ((*phys_virt + count * page_size) > rg.End())
            return false;

        for (vaddr_t page = rg.base; page < (page + rg.size); page += page_size)
        {
            if (IsPagePresent(pool, page))
                continue;

            // TODO - lookahead
            // currently we alloc sequentially so it doesn't matter

            if (MapPages(pool, page, *phys_virt, count))
            {
                *phys_virt = page;
                return true;
            }

            break;
        }

        return false;
    }
}
