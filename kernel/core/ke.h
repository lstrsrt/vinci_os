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

    using ThreadStartFunction = int(*)(void*);

    struct Thread
    {
        enum class State
        {
            Ready,
            Running,
            Waiting,
            Terminating,
        };

        Thread* next; // IMPORTANT: Has to be at offset 0!
        x64::Context ctx;
        State state;
        u64 id;
        u64 delay;
        ThreadStartFunction function;
        void* arg;
    };

    //
    // This is like the KPRCB on Windows.
    // It contains per-core kernel data and is stored in GS.
    // The kernel only supports one core right now so this only exists once.
    //
    struct Core
    {
        Core* self; // avoid having to rdgsbase
        SList thread_list;
        Thread* current_thread;
        Thread* idle_thread;
    };

    void InitializeCore();

    INLINE Core* GetCore()
    {
        return ( Core* )__readgsqword(OFFSET(Core, self));
    }

    INLINE Thread* GetCurrentThread()
    {
        return ( Thread* )__readgsqword(OFFSET(Core, current_thread));
    }

#pragma data_seg(".data")
    inline bool schedule = false;
#pragma data_seg()

    Thread* CreateThread(ThreadStartFunction function, void* arg, vaddr_t kstack = 0);
    bool SelectNextThread();
    void StartScheduler();

    void Yield();

    void Delay(u64 ticks);

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
