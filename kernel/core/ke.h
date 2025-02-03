#pragma once

#include <base.h>
#include <ec/list.h>

#include "../common/mm.h"
#include "../hw/cpu/x64.h"

enum class Status
{
    Success = 0,
    OutOfMemory,
    OutOfIds,
    DoubleFree,
    UnsupportedSystem,
    Unreachable,
};

namespace ke
{
    using tid_t = u64;
    using ThreadStartFunction = int(*)(u64);

    struct Thread
    {
        constexpr Thread() = default;
        constexpr ~Thread() = default;

        enum class State
        {
            Ready,
            Running,
            Waiting,
            Terminating,
        };

        ec::slist_entry thread_list_entry;
        x64::Context context;
        uptr_t user_stack;
        State state;
        u64 delay;
        ThreadStartFunction function;
        u64 arg;
        tid_t id;
    };

    //
    // This is like the KPRCB on Windows.
    // It contains per-core kernel data and is stored in GS.
    // The kernel only supports one core right now so this only exists once.
    //
    struct Core
    {
        Core* self; // avoid having to rdgsbase
        Thread* current_thread;
        Thread* idle_thread;
        ec::slist_entry thread_list_head;
        mm::PageTable* page_table;
        uptr_t kernel_stack;
        uptr_t user_stack;

        const x64::GdtEntry* gdt;
        const x64::IdtEntry* idt;
        x64::Tss* tss;

        INLINE auto GetFirstThread()
        {
            return this->thread_list_head.m_next;
        }
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

    Thread* CreateThread(ThreadStartFunction function, u64 arg, vaddr_t kstack = 0);
    Thread* CreateThreadInternal(ThreadStartFunction function, u64 arg, vaddr_t kstack);
    void CreateUserThread(void* user_function);

    bool SelectNextThread();
    void StartScheduler();

    NO_RETURN void ExitThread(int exit_code);
    void Yield();
    void Delay(u64 ticks);

    enum_flags(AllocFlag, u32)
    {
        None = 0,
        Uninitialized = 1 << 0, // Ignored with ALLOC_POISON
    };

    inline bool alloc_initialized = false;

    void InitializeAllocator();
    ALLOC_FN void* Allocate(size_t size, AllocFlag flags = AllocFlag::None);

    template<class T>
    ALLOC_FN INLINE T* Allocate(size_t size = sizeof(T), AllocFlag flags = AllocFlag::None)
    {
        return ( T* )Allocate(size, flags);
    }

    void Free(void* address);
    DEBUG_FN void PrintAllocations();

    NO_RETURN void Panic(Status);
    NO_RETURN void Panic(Status, size_t, size_t = 0, size_t = 0, size_t = 0);
}
