#include <libc/mem.h>
#include <ec/bitmap.h>

#include "ke.h"
#include "gfx/output.h"
#include "../hw/serial/serial.h"

#define DEBUG_ALLOC
#define ALLOC_POISON

#ifdef DEBUG_ALLOC
#define DbgPrint(x, ...) EMPTY_STMT
#else
#define DbgPrint(x, ...) serial::Write(x, __VA_ARGS__)
#endif

#ifdef ALLOC_POISON
#define PoisonMemory(addr, val, n) EMPTY_STMT
#else
#define PoisonMemory(addr, val, n) memset(addr, val, n)
#endif

namespace ke
{
    struct Allocation
    {
        u32 offset;
        u32 blocks;
    };

    static constexpr size_t block_size = 32;
    [[maybe_unused]] static constexpr u8 fresh = 0xaa, poison = 0xcc;

#pragma data_seg(".data")
    static ec::const_bitmap<u64, kva::kernel_pool.size / block_size / 64> alloc_map;
#pragma data_seg()

    void InitializeAllocator()
    {
        total_free = kva::kernel_pool.size; // FIXME - This is wrong; it depends on how much is mapped!
        total_used = 0;
        ke::alloc_initialized = true;
    }

    constexpr auto AlignUp(size_t value, size_t align)
    {
        return (value + align - 1) & ~(align - 1);
    };

    void SetAllocationState(Allocation* info, bool allocating)
    {
        if (allocating)
        {
            for (size_t i = info->offset; i < (info->offset + info->blocks); i++)
                alloc_map.set_bit(i);
        }
        else // Freeing
        {
            for (size_t i = info->offset; i < (info->offset + info->blocks); i++)
                alloc_map.clear_bit(i);
        }
    }

    void InitMemory(void* block, size_t size)
    {
        // This can be too aggressive - for example if we want to allocate 112 bytes,
        // alignment will give us 8 extra bytes to memset.
#ifdef ALLOC_POISON
        PoisonMemory(memory, fresh, size - sizeof(Allocation));
#else
        if (!(flags & AllocFlag::Uninitialized))
            memzero(memory, size - sizeof(Allocation));
#endif
    }

    ALLOC_FN void* Allocate(size_t size, AllocFlag flags)
    {
        DbgPrint("Allocate() - size %llu\n", size);

        size += sizeof(Allocation);
        size = AlignUp(size, block_size);

        if (size > total_free)
        {
            Print("Allocation error (out of memory for size %llu. Free: %llu)\n", size, total_free);
            Panic(Status::OutOfMemory);
        }

        u32 blocks_needed = ( u32 )(size / block_size);
        u32 free_blocks = 0, first_block = 0;

        for (u64 i = 0; i < alloc_map.size(); i++)
        {
            if (alloc_map[i] == ec::umax_v<u64>)
            {
                free_blocks = 0;
                continue;
            }

            for (u64 j = 0; j < alloc_map.bits_per_member; j++)
            {
                u64 block = i * alloc_map.bits_per_member + j;

                // Is this block in use?
                if (!alloc_map.has_bit(block))
                {
                    if (!free_blocks)
                        first_block = block;

                    if (++free_blocks == blocks_needed)
                    {
                        auto alloc = ( Allocation* )(kva::kernel_pool.base + (first_block * block_size));
                        alloc->blocks = blocks_needed;
                        alloc->offset = first_block;

                        DbgPrint("Found suitable block starting at 0x%llx\n", alloc);

                        SetAllocationState(alloc, true);

                        // Skip the allocation info when returning to the caller.
                        void* memory = ( void* )(( vaddr_t )alloc + sizeof(Allocation));

                        InitMemory(memory, size);

                        DbgPrint("Used: %llu -> %llu\n", total_used, total_used + size);

                        total_used += size;
                        total_free -= size;

                        return memory;
                    }
                }
                else
                {
                    // If it's used and we haven't reached blocks_needed yet, restart the search.
                    free_blocks = 0;
                }
            }
        }

        Print("Allocation error (no suitable blocks for size %llu. Free: %llu)\n", size, total_free);
        Panic(Status::OutOfMemory);
    }

    void Free(void* address)
    {
        DbgPrint("Free() - address 0x%llx\n", address);

        if (!address)
            return;

        // Assume that this is the address returned by Allocate()
        // i.e. starting after the allocation info.
        const auto real_address = ( uptr_t )address - sizeof(Allocation);
        if (!kva::kernel_pool.Contains(( vaddr_t )real_address))
        {
            DbgPrint("Invalid address passed to Free (0x%llx, 0x%llx), returning.\n", address, real_address);
            return;
        }

        DbgPrint("Freeing 0x%llx\n", real_address);
        const auto alloc = ( Allocation* )real_address;
        SetAllocationState(alloc, false);

        const auto size = alloc->blocks * block_size;
        PoisonMemory(( void* )real_address, poison, bytes);

        DbgPrint("Used: %llu -> %llu\n", total_used, total_used - size);

        total_used -= size;
        total_free += size;
    }

    DEBUG_FN void PrintAllocations()
    {
        Print("Total used: %llu bytes, free: %llu bytes\n", total_used, total_free);
        for (size_t i = 0; i < alloc_map.size(); i++)
        {
            if (auto b = alloc_map[i])
                Print("[%llu]: 0x%llx\n", i, b);
        }
    }
}
