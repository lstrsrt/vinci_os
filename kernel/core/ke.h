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

#pragma data_seg(".data")
    inline Thread* first_thread{};
    inline Thread* cur_thread{};
    inline bool schedule = false;
#pragma data_seg()

    Thread* CreateThread(void(*function)(void*), void* arg, vaddr_t stack);
    void SelectNextThread();
    void StartScheduler();

    enum_flags(AllocFlag, u32)
    {
        None = 0,
        Uninitialized = 1 << 0, // Ignored with ALLOC_POISON
    };

    void InitializeAllocator();
    void* Allocate(size_t size);
    void Free(void* address);

    template<class T>
    INLINE T* Allocate(size_t size = sizeof(T))
    {
        return ( T* )Allocate(size);
    }

    NO_RETURN void Panic(Status);
}
