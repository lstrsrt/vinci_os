#include <libc/mem.h>
#include <ec/new.h>

#include "ke.h"
#include "gfx/output.h"
#include "../hw/timer/timer.h"

#define DEBUG_CTX_SWITCH

#ifdef DEBUG_CTX_SWITCH
#include "../hw/serial/serial.h"
#define DbgPrint(x, ...) serial::Write(x, __VA_ARGS__)
#else
#define DbgPrint(x, ...) EMPTY_STMT
#endif

EXTERN_C uptr_t kernel_stack_top;

namespace ke
{
    void PushListEntry(SList* list, void* entry);
    void* PopListEntry(SList* list);
    size_t GetListLength(SList* list);
    void* RemoveListEntry(SList* list, void* entry);

    NO_RETURN static int IdleLoop(void*)
    {
        schedule = true;

        // HLT forever with interrupts enabled
        x64::Idle();
    }

    static void RegisterThread(Thread* thread)
    {
        PushListEntry(&GetCore()->thread_list, thread);
    }

    static void UnregisterThread(Thread* thread)
    {
        RemoveListEntry(&GetCore()->thread_list, thread);
    }

#pragma data_seg(".data")
    static size_t thread_i = 0;
#pragma data_seg()

    NO_RETURN void ExitThread(int exit_code)
    {
        auto core = GetCore();
        auto thread = GetCurrentThread();

        _disable();

        Print("Thread %llu exited with code %d\n", thread->id, exit_code);

        thread_i--;

        // Update state so it's ignored by the scheduler
        // and pick the next thread for execution.
        thread->state = Thread::State::Terminating;
        SelectNextThread();

        UnregisterThread(thread);
        Free(thread);

        // Update to the next thread.
        thread = GetCurrentThread();

        core->tss->rsp0 = thread->context.rsp;

        core->kernel_stack = thread->context.rsp;
        core->user_stack = thread->user_stack;

        Print("Switching to new thread %llu\n", thread->id);

        serial::Write(
            "RSP0 0x%llx RIP 0x%llx RSP3 0x%llx\n",
            thread->context.rsp,
            thread->context.rip,
            thread->user_stack
        );

        // Let the next thread take over.
        x64::LoadContext(&thread->context, thread->user_stack);
    }

    static int UserThreadEntry(void*)
    {
        auto thread = GetCurrentThread();

        // Get user code address from temporary storage
        thread->context.rip = thread->context.rdx;

        x64::LoadContext(&thread->context, thread->user_stack);
        return 0;
    }

    static void KernelThreadEntry()
    {
        auto thread = GetCurrentThread();

        int ret = thread->function(thread->arg);

        ExitThread(ret);
    }

    Thread* CreateThreadInternal(ThreadStartFunction function, void* arg, vaddr_t kstack)
    {
        auto thread = new Thread();

        thread->id = thread_i++; // Idle thread is 0, other threads start at 1
        thread->function = function;
        thread->arg = arg;

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
        core->thread_list.next = nullptr;

        // FIXME - can we use kernel_stack_top here?
        // since every thread is going to have its own kernel stack,
        // we should be able to use the boot stack for the idle loop
        auto idle_thread = CreateThreadInternal(IdleLoop, nullptr, kernel_stack_top);
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

        auto kthread = CreateThread(UserThreadEntry, nullptr);

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

        // TODO - randomize all stack offsets
        kthread->user_stack = ustack + page_size - 32;
    }

    Thread* CreateThread(ThreadStartFunction function, void* arg, vaddr_t kstack)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto thread = CreateThreadInternal(function, arg, kstack);
        RegisterThread(thread);

        return thread;
    }

    // Returns true if a new thread was selected.
    bool SelectNextThread()
    {
        _disable();

        auto core = GetCore();
        if (!core->thread_list.next) // No threads available, leave early.
            return false;

        Thread* next;
        auto prev = core->current_thread;
        if (prev == core->idle_thread)
        {
            // Switching from idle loop.
            // In this case, start searching from the first thread.
            next = ( Thread* )core->thread_list.next;

            for (;;)
            {
                if (next->state == Thread::State::Ready)
                    break;
                if (next->state == Thread::State::Waiting && next->delay <= timer::ticks)
                    break;

                next = ( Thread* )next->next;

                // If we're at the end and nothing was found, keep the idle thread.
                if (!next)
                    return false;
            }
        }
        else /* Switching from standard thread */
        {
            next = prev->next;

            for (;;)
            {
                if (!next)
                    next = ( Thread* )core->thread_list.next; // Restart if we're at the end

                if (next->state == Thread::State::Ready)
                    break;
                if (next->state == Thread::State::Waiting && next->delay <= timer::ticks)
                    break;

                if (next == prev) // Back at where we began
                {
                    if (next->state == Thread::State::Running)
                        return false; // No other suitable thread found, so just take this one again

                    // If we're here, all threads are waiting. Switch to the idle loop until
                    // there is something to do again.
                    next = core->idle_thread;
                    break;
                }

                next = ( Thread* )next->next;
            }
        }

        // Set the old thread back to ready, unless it is still waiting.
        if (prev->state == Thread::State::Running)
            prev->state = Thread::State::Ready;

        next->state = Thread::State::Running;
        core->current_thread = next;

        return true;
    }

    void Yield()
    {
        auto prev = GetCurrentThread();

        if (SelectNextThread())
        {
            auto core = GetCore();
            auto next = GetCurrentThread();

            core->tss->rsp0 = next->context.rsp;

            core->kernel_stack = next->context.rsp;
            core->user_stack = next->user_stack;

            DbgPrint(
                "\n"
                "  Yielding\n"
                "  From id %llu to id %llu\n"
                "  RSP0: 0x%llx RSP3: 0x%llx\n"
                "  TSS0 RSP is 0x%llx\n",
                prev->id, next->id,
                next->context.rsp, next->user_stack,
                core->tss->rsp0
            );

            x64::SwitchContext(&prev->context, &next->context, next->user_stack);
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




    void PushListEntry(SList* list, void* entry)
    {
        auto next = ( SList* )entry;
        next->next = list->next;
        list->next = next;
    }

    void* PopListEntry(SList* list)
    {
        if (list->next)
            list->next = list->next->next;
        return list->next;
    }

    size_t GetListLength(SList* list)
    {
        size_t len = 0;
        for (auto p = list->next; p; p = p->next)
            len++;
        return len;
    }

    void* RemoveListEntry(SList* list, void* entry)
    {
        auto head = ( SList* )list;
        auto p1 = head;
        auto p2 = head->next;

        while (p2)
        {
            if (p2 == entry)
            {
                p1->next = p2->next;
                return p1->next;
            }
            p1 = p1->next;
            p2 = p2->next;
        }

        return p2;
    }
}
