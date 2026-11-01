#include <ec/bitmap.h>
#include <ec/new.h>
#include <libc/mem.h>
#include <libc/str.h>

#include "../hw/gfx/output.h"
#include "../hw/timer/timer.h"
#include "ke.h"

// #define DEBUG_CTX_SWITCH

#ifdef DEBUG_CTX_SWITCH
#define DbgPrint(x, ...) gfx::Print(x, __VA_ARGS__)
#else
#define DbgPrint(x, ...) EMPTY_STMT
#endif

EXTERN_C uptr_t kernel_stack_base;

namespace ke
{
    ec::const_bitmap<u64, 8> thread_map{}; // max 511 threads (idle = 0)

    NO_RETURN int IdleLoop(u64)
    {
        schedule = true;

        // HLT forever with interrupts enabled
        x64::Idle();
    }

    void RegisterThread(Thread* thread)
    {
        bool b = x64::DisableInterrupts();
        GetCore()->thread_list_head.push(&(thread->thread_list_entry));
        if (b)
            x64::EnableInterrupts();
    }

    void UnregisterThread(Thread* thread)
    {
        bool b = x64::DisableInterrupts();
        GetCore()->thread_list_head.remove(&(thread->thread_list_entry));
        if (b)
            x64::EnableInterrupts();
    }

    void FreeThread(Thread* thread)
    {
        DbgPrint("Freeing thread %u\n", thread->id);

        if (!thread_map.has_bit(thread->id))
            Panic(Status::DoubleFree, thread->id);

        thread_map.clear_bit(thread->id);

        Free(( void* )thread->kernel_stack_base);
        // TODO: free user stack here once we have UnmapPage and FreePhysical :D

        delete thread;
    }

    NO_RETURN void ExitThread(int exit_code)
    {
        auto thread = GetCurrentThread();

        x64::DisableInterrupts();

        Print("Thread %llu exited with code %d\n", thread->id, exit_code);

        // Update state so it's ignored by the scheduler
        // and pick the next thread for execution.
        thread->state = Thread::State::Terminating;
        bool int_state;
        SelectNextThread(&int_state);

        UnregisterThread(thread);

        // Update to the next thread.
        auto* next = GetCurrentThread();

        DbgPrint("Switching to %llu\n", next->id);
        DbgPrint("  RSP 0x%p RIP 0x%p KSTACK 0x%p\n", next->context.rsp, next->context.rip, next->kernel_stack_top);

        // Free this thread and let the next thread take over.
        // This frees our stack so it must be done late.
        FreeThread(thread);
        x64::LoadContext(&next->context);
        Panic(Status::Unreachable);
    }

    EXTERN_C NO_RETURN void _ExitThread(int exit_code)
    {
        ke::ExitThread(exit_code);
    }

    void KernelThreadEntry()
    {
        auto thread = GetCurrentThread();

        int ret = thread->function(thread->arg);

        ExitThread(ret);
    }

    void AllocateThreadId(Thread* thread)
    {
        for (u64 i = 0; i < thread_map.size(); i++)
        {
            if (thread_map[i] == ec::umax_v<u64>)
                continue;

            for (u64 j = 0; j < thread_map.bits_per_member; j++)
            {
                u64 id = i * thread_map.bits_per_member + j;

                if (!thread_map.has_bit(id))
                {
                    thread_map.set_bit(id);
                    thread->id = id;
                    return;
                }
            }
        }

        Panic(Status::OutOfIds);
    }

    Thread* CreateThreadInternal(ThreadStartFunction function, u64 arg, vaddr_t kstack, bool kernel)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto thread = new Thread();

        AllocateThreadId(thread);
        thread->function = function;
        thread->arg = arg;

        thread->kernel_stack_base = kstack - page_size;
        thread->kernel_stack_top = kstack & ~0xFull;

        if (kernel)
        {
            thread->context.rip = ( u64 )KernelThreadEntry;
            thread->context.rsp = thread->kernel_stack_top - 40;
            thread->context.rflags = ( u64 )(x64::RFLAG::IF | x64::RFLAG::ALWAYS);
            thread->context.cs = x64::GetGdtOffset(x64::GdtIndex::R0Code);
            thread->context.ss = x64::GetGdtOffset(x64::GdtIndex::R0Data);
        }

        return thread;
    }

    void ReadyThread(Thread* thread)
    {
        DbgPrint("New thread with ID: %llu\n", thread->id);
        thread->state = Thread::State::Ready;
        RegisterThread(thread);
    }

    Thread* CreateThread(ThreadStartFunction function, u64 arg, vaddr_t kstack)
    {
        auto* thread = CreateThreadInternal(function, arg, kstack, true);
        ReadyThread(thread);
        return thread;
    }

    void StartScheduler()
    {
        auto core = GetCore();
        memzero(thread_map.data(), thread_map.size());

        auto idle_thread = CreateThreadInternal(IdleLoop, 0, kernel_stack_base, true);
        core->idle_thread = idle_thread;
        core->current_thread = idle_thread;
    }

    //
    // This whole function is a temporary hack.
    // All user threads will be bound to a process in the future,
    // with every process being loaded from an image and having its own page table.
    //
    void CreateUserThread(void* user_function, u64 arg)
    {
        paddr_t pa;
        auto& table = *GetCore()->page_table;

        const auto code = 0x7fff'fff0'0000;
        const auto ustack = code + page_size;

        auto kthread = CreateThreadInternal(nullptr, arg, 0, false);

        mm::AllocatePhysical(table, &pa);
        mm::MapPage(table, ustack, pa, true);
        mm::AllocatePhysical(table, &pa);
        mm::MapPage(table, code, pa, true);

        x64::SmapSetAc();
        memzero(( void* )ustack, page_size);
        Print("copying UserThreadEntry to 0x%p\n", ( void* )code);
        memcpy(( void* )code, ( void* )&UserThreadEntry, 128);
        Print("copying code to 0x%p\n", ( void* )(( uintptr_t )code + 128));
        memcpy(( void* )(( uintptr_t )code + 128), user_function, 128);
        x64::SmapClearAc();

        kthread->user_stack_base = ustack;
        kthread->context.rcx = arg;
        kthread->context.rdx = ( uintptr_t )code + 128;
        kthread->context.rip = code;
        // TODO - randomize all stack offsets
        kthread->context.rsp = ustack + page_size - 40;
        kthread->context.rflags = ( u64 )(x64::RFLAG::IF | x64::RFLAG::ALWAYS);
        kthread->context.cs = x64::GetGdtOffset(x64::GdtIndex::R3Code) | 3;
        kthread->context.ss = x64::GetGdtOffset(x64::GdtIndex::R3Data) | 3;

        ReadyThread(kthread);
    }

    //
    // Returns true if a new thread was selected.
    // Always returns with interrupts disabled.
    //
    bool SelectNextThread(bool* prev_interrupt_state)
    {
        *prev_interrupt_state = x64::DisableInterrupts();

        auto core = GetCore();
        if (!core->GetFirstThread()) // No threads available, leave early.
        {
            DbgPrint("SelectNextThread: empty\n");
            return false;
        }

        ec::slist_entry* entry;
        Thread* next;
        auto prev = core->current_thread;

        if (prev == core->idle_thread)
        {
            // Switching from idle loop.
            // In this case, start searching from the first thread.
            DbgPrint("SelectNextThread: attempting switch from idle\n");

            for (entry = core->GetFirstThread();; entry = entry->m_next)
            {
                next = CONTAINING_RECORD(entry, Thread, thread_list_entry);

                if (next->state == Thread::State::Ready)
                    break;
                if (next->state == Thread::State::Waiting && next->delay <= timer::ticks)
                    break;

                // If we're at the end and nothing was found, keep the idle thread.
                if (entry->m_next == core->GetFirstThread())
                {
                    DbgPrint("SelectNextThread: staying idle\n");
                    return false;
                }
            }
        }
        else /* Switching from standard thread */
        {
            DbgPrint("SelectNextThread: attempting switch from thread %llu\n", prev->id);

            // Start searching from the next thread
            for (entry = prev->thread_list_entry.m_next;; entry = entry->m_next)
            {
                next = CONTAINING_RECORD(entry, Thread, thread_list_entry);

                if (next->state == Thread::State::Ready)
                    break;
                if (next->state == Thread::State::Waiting && next->delay <= timer::ticks)
                    break;

                if (next == prev) // Back at where we began
                {
                    if (next->state == Thread::State::Running)
                        return false; // No other suitable thread found, so just take this one again

                    // If we're here, all threads are waiting.
                    // Switch to the idle loop until there is something to do.
                    next = core->idle_thread;
                    break;
                }
            }
        }

        // Set the old thread back to ready, unless it is still waiting.
        if (prev->state == Thread::State::Running)
            prev->state = Thread::State::Ready;

        next->state = Thread::State::Running;
        core->current_thread = next;

        // Architecture-specific changes
        core->kernel_stack_top = core->tss->rsp0 = next->kernel_stack_top;

        DbgPrint("SelectNextThread: switching from %llu to %llu\n", prev->id, next->id);
        return true;
    }

    void Yield()
    {
        auto prev = GetCurrentThread();
        bool int_state;

        if (SelectNextThread(&int_state))
        {
            auto next = GetCurrentThread();

            DbgPrint("%llu: Yielding\n", prev->id);

            x64::SwitchContext(&prev->context, &next->context);
        }
        else
        {
            DbgPrint("%llu: Failed to yield\n", prev->id);
        }

        if (int_state)
        {
            x64::EnableInterrupts();
        }
    }

    void Delay(u64 ticks)
    {
        auto thread = GetCurrentThread();

        thread->state = Thread::State::Waiting;
        if (ticks > 0)
            thread->delay = timer::ticks + ticks;

        Yield();
    }
} // namespace ke
