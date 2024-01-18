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

#define ReadCore64(field) (( decltype(Core::##field) )__readgsqword(__builtin_offsetof(Core, field)))
#define WriteCore64(field, x) (__writegsqword(__builtin_offsetof(Core, field), ( u64 )x))

    INLINE Core* GetCore()
    {
        return ReadCore64(self);
    }

    INLINE Thread* GetCurrentThread()
    {
        return ReadCore64(current_thread);
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
    ALLOC_FN INLINE T* Allocate(size_t size = sizeof T, AllocFlag flags = AllocFlag::None)
    {
        return ( T* )Allocate(size, flags);
    }

    void Free(void* address);
    void PrintAllocations();

    NO_RETURN void Panic(Status);
}
