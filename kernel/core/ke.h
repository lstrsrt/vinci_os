#pragma once

#include <base.h>

#include "../hw/cpu/x64.h"

enum class Status
{
    Success = 0,
    UnsupportedSystem,
    Unreachable,
    OutOfMemory,
};

namespace ke
{
    struct SList
    {
        SList* next;
    };

    struct Thread
    {
        SList* next;
        x64::Context ctx;
        void(*function)(void*);
        void* arg;
        u64 id;
    };

    //
    // This is like the KPRCB on Windows.
    // It contains (or will contain) per-core kernel data and is stored in GS.
    // The kernel only supports one core right now so this only exists once in memory.
    //
    struct Core
    {
        Core* self; // avoid having to rdgsbase
        Thread* first_thread;
        Thread* current_thread;
    };

    void InitializeCore();

    INLINE u64 __readgsqword(u64 offset)
    {
        u64 value;
        asm volatile(
            "mov %%gs:%a[off], %[val]"
            : [val] "=r"(value)
            : [off] "ir"(offset)
        );
        return value;
    }

    INLINE void __writegsqword(u64 offset, u64 value)
    {
        asm volatile(
            "mov %[val], %%gs:%a[off]"
            :: [off] "ir"(offset), [val] "r"(value)
            : "memory"
        );
    }

    INLINE Core* GetCore()
    {
        return ( Core* )(__readgsqword(__builtin_offsetof(Core, self)));
    }

    INLINE Thread* GetCurrentThread()
    {
        return ( Thread* )(__readgsqword(__builtin_offsetof(Core, current_thread)));
    }

#pragma data_seg(".data")
    inline bool schedule = false;
#pragma data_seg()

    Thread* CreateThread(void(*function)(void*), void* arg, vaddr_t kstack = 0);
    void SelectNextThread();
    void StartScheduler();

    enum_flags(AllocFlag, u32)
    {
        None = 0,
        Uninitialized = 1 << 0, // Ignored with ALLOC_POISON
    };

    void InitializeAllocator();
    ALLOC_FN void* Allocate(size_t size, AllocFlag flags = AllocFlag::None);

    template<class T>
    ALLOC_FN INLINE T* Allocate(size_t size = sizeof(T), AllocFlag flags = AllocFlag::None)
    {
        return ( T* )Allocate(size, flags);
    }

    void Free(void* address);
    void PrintAllocations();

    NO_RETURN void Panic(Status);
}
