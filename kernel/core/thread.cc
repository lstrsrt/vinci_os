#include <libc/mem.h>
#include <ec/bitmap.h>
#include <ec/new.h>

#include "ke.h"
#include "gfx/output.h"
#include "../hw/timer/timer.h"

// #define DEBUG_CTX_SWITCH

#ifdef DEBUG_CTX_SWITCH
#include "../hw/serial/serial.h"
#define DbgPrint(x, ...) serial::Write(x, __VA_ARGS__)
#else
#define DbgPrint(x, ...) EMPTY_STMT
#endif

EXTERN_C uptr_t kernel_stack_top;

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
        GetCore()->thread_list_head.push(&(thread->thread_list_entry));
    }

    void UnregisterThread(Thread* thread)
    {
        GetCore()->thread_list_head.remove(&(thread->thread_list_entry));
    }

    void FreeThread(Thread* thread)
    {
        DbgPrint("Freeing thread %u\n", thread->id);

        if (!thread_map.has_bit(thread->id))
            Panic(Status::DoubleFree, thread->id);

        thread_map.clear_bit(thread->id);

        Free(( void* )thread->kernel_stack_top);
        if (thread->user_stack_top)
            Free(( void* )thread->user_stack_top);

        delete thread;
    }

    NO_RETURN void ExitThread(int exit_code)
    {
        auto thread = GetCurrentThread();

        _disable();

        Print("Thread %llu exited with code %d\n", thread->id, exit_code);

        // Update state so it's ignored by the scheduler
        // and pick the next thread for execution.
        thread->state = Thread::State::Terminating;
        SelectNextThread();

        UnregisterThread(thread);
        FreeThread(thread);

        // Update to the next thread.
        thread = GetCurrentThread();

        DbgPrint("Switching to %llu\n", thread->id);
        DbgPrint(
            "  RSP0 0x%llx RIP 0x%llx RSP3 0x%llx\n",
            thread->context.rsp,
            thread->context.rip,
            thread->user_stack
        );

        // Let the next thread take over.
        x64::LoadContext(&thread->context, thread->user_stack);
        Panic(Status::Unreachable);
    }

    int UserThreadEntry(u64)
    {
        auto thread = GetCurrentThread();

        // Get user code address from temporary storage
        thread->context.rip = thread->context.rdx;

        x64::LoadContext(&thread->context, thread->user_stack);
        return 0;
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

    Thread* CreateThreadInternal(ThreadStartFunction function, u64 arg, vaddr_t kstack)
    {
        auto thread = new Thread();

        AllocateThreadId(thread);
        thread->function = function;
        thread->arg = arg;

        thread->kernel_stack_top = kstack - page_size;

        thread->context.rip = ( u64 )KernelThreadEntry;
        thread->context.rsp = kstack;
        thread->context.rflags = ( u64 )(x64::RFLAG::IF | x64::RFLAG::ALWAYS);
        thread->context.cs = x64::GetGdtOffset(x64::GdtIndex::R0Code);
        thread->context.ss = x64::GetGdtOffset(x64::GdtIndex::R0Data);

        thread->state = Thread::State::Ready;

        return thread;
    }

    void StartScheduler()
    {
        auto core = GetCore();
        memzero(thread_map.data(), thread_map.size());

        // FIXME - can we use kernel_stack_top here?
        // since every thread is going to have its own kernel stack,
        // we should be able to use the boot stack for the idle loop
        auto idle_thread = CreateThreadInternal(IdleLoop, 0, kernel_stack_top);
        core->idle_thread = idle_thread;
        core->current_thread = idle_thread;
    }

    //
    // This whole function is a temporary hack.
    // All user threads will be bound to a process in the future,
    // with every process being loaded from an image and having its own page table.
    //
    void CreateUserThread(void* user_function)
    {
        paddr_t pa;
        auto& table = *GetCore()->page_table;

        const auto code = 0x7fff'fff0'0000;
        const auto ustack = code + page_size;

        auto kthread = CreateThread(UserThreadEntry, 0);

        mm::AllocatePhysical(table, &pa);
        mm::MapPage(table, ustack, pa, true);
        mm::AllocatePhysical(table, &pa);
        mm::MapPage(table, code, pa, true);

        // This is the start address of the actual user code.
        // RIP is already set to UserThreadEntry so it can't be used here.
        kthread->context.rdx = code;

        x64::SmapSetAc();
        memzero(( void* )ustack, page_size);
        memcpy(( void* )code, user_function, 100);
        x64::SmapClearAc();

        kthread->user_stack_top = ustack;
        // TODO - randomize all stack offsets
        kthread->user_stack = ustack + page_size - 32;
    }

    Thread* CreateThread(ThreadStartFunction function, u64 arg, vaddr_t kstack)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto thread = CreateThreadInternal(function, arg, kstack);
        RegisterThread(thread);
        DbgPrint("New thread with ID: %llu\n", thread->id);

        return thread;
    }

    //
    // Returns true if a new thread was selected.
    // Always returns with interrupts disabled.
    //
    bool SelectNextThread()
    {
        _disable();

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

            for (entry = core->GetFirstThread(); ; entry = entry->m_next)
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
            for (entry = prev->thread_list_entry.m_next; ; entry = entry->m_next)
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
        core->kernel_stack = core->tss->rsp0 = next->context.rsp;
        core->user_stack = next->user_stack;

        DbgPrint("SelectNextThread: switching from %llu to %llu\n", prev->id, next->id);
        return true;
    }

    void Yield()
    {
        auto prev = GetCurrentThread();

        if (SelectNextThread())
        {
            auto next = GetCurrentThread();

            DbgPrint("%llu: Yielding\n", prev->id);

            x64::SwitchContext(&prev->context, &next->context, next->user_stack);
        }
        else
        {
            DbgPrint("%llu: Failed to yield\n", prev->id);
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
}
