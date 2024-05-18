#include <libc/mem.h>
#include <ec/array.h>

#include "ke.h"
#include "gfx/output.h"

#define DEBUG_ALLOC     0
#define ALLOC_POISON    0

#if DEBUG_ALLOC == 0
#define DbgPrint(x, ...) EMPTY_STMT
#else
#define DbgPrint(x, ...) Print(x, __VA_ARGS__)
#endif

#if ALLOC_POISON == 0
#define SetMemory(addr, val, n) EMPTY_STMT
#else
#define SetMemory(addr, val, n) memset(addr, val, n)
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
    static ec::array<u8, kva::kernel_pool.size / block_size / 8> alloc_map;
    static size_t total_free;
    static size_t total_used;
#pragma data_seg()

    void InitializeAllocator()
    {
        memzero(alloc_map.data(), alloc_map.size());
        total_free = kva::kernel_pool.size; // FIXME - This is wrong; it depends on how much is mapped!
        total_used = 0;
    }

    static constexpr auto AlignUp(size_t value, size_t align)
    {
        return (value + align - 1) & ~(align - 1);
    };

    static void SetAllocationState(Allocation* info, bool allocating)
    {
        if (allocating)
        {
            for (size_t i = info->offset; i < (info->offset + info->blocks); i++)
                alloc_map[i / 8] |= (1 << (i % 8));
        }
        else // Freeing
        {
            for (size_t i = info->offset; i < (info->offset + info->blocks); i++)
                alloc_map[i / 8] &= ~(1 << (i % 8));
        }
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
            // TODO - unreachable macro
        }

        u32 blocks_needed = ( u32 )(size / block_size);
        u32 free_blocks = 0, first_block = 0;

        for (u32 i = 0; i < alloc_map.size(); i++)
        {
            if (alloc_map[i] == ec::umax_v<u8>)
            {
                // No free bits means no available blocks
                free_blocks = 0;
                continue;
            }

            for (u32 j = 0; j < 8; j++)
            {
                // Is this block in use?
                if (!(alloc_map[i] & (1 << j)))
                {
                    if (!free_blocks)
                        first_block = (i * 8) + j;

                    if (++free_blocks == blocks_needed)
                    {
                        auto alloc = ( Allocation* )(kva::kernel_pool.base + (first_block * block_size));
                        alloc->blocks = blocks_needed;
                        alloc->offset = first_block;

                        DbgPrint("Found suitable block starting at 0x%llx\n", alloc);

                        SetAllocationState(alloc, true);

                        // Skip the allocation info when returning to the caller.
                        void* memory = ( void* )(( vaddr_t )alloc + sizeof(Allocation));

                        // This can be too aggressive - for example if we want to allocate 112 bytes,
                        // alignment will give us 8 extra bytes to memset.
#if ALLOC_POISON == 0
                        if (!(flags & AllocFlag::Uninitialized))
                            memzero(memory, size - sizeof(Allocation));
#else
                        SetMemory(memory, fresh, size - sizeof(Allocation));
#endif
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
        DbgPrint("Offset: %u, Blocks: %u\n", alloc->offset, alloc->blocks);

        SetAllocationState(alloc, false);

        const auto bytes = alloc->blocks * block_size;
        SetMemory(( void* )real_address, poison, bytes);

        total_used -= bytes;
        total_free += bytes;
    }

    DEBUG_FN void PrintAllocations()
    {
        Print("Total used: %llu bytes, free: %llu bytes\n", total_used, total_free);
        for (size_t i = 0; i < alloc_map.size(); i++)
        {
            if (u8 b = alloc_map[i])
                Print("[%llu]: %u\n", i, b);
        }
    }
}
