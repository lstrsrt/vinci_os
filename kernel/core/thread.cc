#include <libc/mem.h>
#include <ec/new.h>

#include "ke.h"
#include "gfx/output.h"
#include "../hw/timer/timer.h"

EXTERN_C uptr_t kernel_stack_top;

namespace ke
{
    void PushListEntry(SList* list, void* entry);
    void* PopListEntry(SList* list);
    size_t GetListLength(SList* list);
    void* RemoveListEntry(SList* list, void* entry);
    void* FindListEntry(SList* list, void* entry);

    static int IdleLoop(void*)
    {
        schedule = true;

        for (;;)
        {
            _disable();
            _mm_pause();
            _enable();
        }

        return 0;
    }

    static void EntryPointThread()
    {
        auto core = GetCore();
        auto cur_thread = core->current_thread;
        auto first_thread = &core->thread_list;

        int ret = cur_thread->function(cur_thread->arg);

        _disable();

        Print("Thread %llu exited with code %d\n", cur_thread->id, ret);

        // Update state so it's ignored by the scheduler
        // and pick the next thread for execution.
        cur_thread->state = Thread::State::Terminating;
        SelectNextThread();

        // Clean up the exited thread.
        RemoveListEntry(first_thread, cur_thread);
        Free(cur_thread);

        // Let the next thread take over.
        x64::LoadContext(&core->current_thread->ctx);
    }

#pragma data_seg(".data")
    static size_t thread_i = 0;
#pragma data_seg()

    static Thread* CreateThreadInternal(ThreadStartFunction function, void* arg, vaddr_t kstack)
    {
        auto thread = new Thread();

        thread->id = thread_i++; // Idle thread is 0, other threads start at 1
        thread->function = function;
        thread->arg = arg;

        thread->ctx.rip = ( u64 )EntryPointThread;
        thread->ctx.rsp = kstack;
        thread->ctx.rflags = ( u64 )(x64::RFLAG::IF | x64::RFLAG::ALWAYS);

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

    Thread* CreateThread(ThreadStartFunction function, void* arg, vaddr_t kstack)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto first_thread = &GetCore()->thread_list;
        auto thread = CreateThreadInternal(function, arg, kstack);

        PushListEntry(first_thread, thread);

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
            x64::kernel_tss.rsp0 = GetCurrentThread()->ctx.rsp;
            x64::SwitchContext(&prev->ctx, &GetCurrentThread()->ctx);
        }
    }

    void Delay(u64 ticks)
    {
        auto cur = GetCurrentThread();

        if (ticks > 0)
        {
            cur->state = Thread::State::Waiting;
            cur->delay = timer::ticks + ticks;
        }

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
