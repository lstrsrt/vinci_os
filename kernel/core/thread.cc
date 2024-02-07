#include <libc/mem.h>
#include <ec/new.h>

#include "ke.h"
#include "gfx/output.h"
#include "../hw/timer/timer.h"
#include "../common/mm.h"

EXTERN_C uptr_t kernel_stack_top;

namespace ke
{
    void* PushListEntry(void* list, void* ptr);
    void* RemoveListEntry(void* list, void* ptr);
    void* FindListEntry(void* list, void* ptr);

    static void IdleLoop(void*)
    {
        schedule = true;

        for (;;)
        {
            _disable();
            _mm_pause();
            _enable();
        }
    }

    static void EntryPointThread()
    {
        auto core = GetCore();
        auto cur_thread = core->current_thread;
        auto first_thread = core->first_thread;

        cur_thread->function(cur_thread->arg);

        _disable();

        RemoveListEntry(first_thread, cur_thread);

        // HACK
        // If this is the first thread, swap out the head pointer
        if (first_thread == cur_thread)
            core->first_thread = ( Thread* )cur_thread->next;

        Free(cur_thread);

        // Let the next thread take over.
        SelectNextThread();
        x64::LoadContext(&GetCurrentThread()->ctx);
    }

#pragma data_seg(".data")
    static size_t thread_i = 0;
#pragma data_seg()

    static Thread* CreateThreadInternal(void(*function)(void*), void* arg, vaddr_t kstack)
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
        // FIXME - can we use kernel_stack_top here?
        // since every thread is going to have its own kernel stack,
        // we should be able to use the boot stack for the idle loop

        auto core = GetCore();
        auto idle_thread = CreateThreadInternal(IdleLoop, nullptr, kernel_stack_top);

        core->idle_thread = idle_thread;
        core->current_thread = idle_thread;
    }

    Thread* CreateThread(void(*function)(void*), void* arg, vaddr_t kstack)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto first_thread = GetCore()->first_thread;
        auto thread = CreateThreadInternal(function, arg, kstack);

        if (!first_thread)
            GetCore()->first_thread = thread;
        else
            PushListEntry(first_thread, thread);

        return thread;
    }

    void SelectNextThread()
    {
        _disable();

        auto core = GetCore();
        auto prev = core->current_thread;
        Thread* next;

        if (!core->first_thread) // No threads available, leave early
            return;

        if (prev == core->idle_thread)
        {
            // Switching from idle loop.
            // In this case, start searching from the first thread.
            next = core->first_thread;

            for (;;)
            {
                if (next->state == Thread::State::Ready)
                    break;
                if (next->state == Thread::State::Waiting && next->delay <= timer::ticks)
                    break;

                next = ( Thread* )next->next;

                if (!next)
                {
                    // We're at the end and nothing was found, so keep the idle thread.
                    _enable();
                    return;
                }
            }
        }
        else /* Switching from standard thread */
        {
            next = prev;

            for (;;)
            {
                next = ( Thread* )next->next;
                if (!next)
                    next = core->first_thread; // Restart if we're at the end

                if (next->state == Thread::State::Ready)
                    break;
                if (next->state == Thread::State::Waiting && next->delay <= timer::ticks)
                    break;

                if (next == prev) // Back at where we began
                {
                    if (next->state == Thread::State::Running)
                        break; // No other suitable thread found, so just take this one again

                    // If we're here, all threads are waiting. Switch to the idle loop until
                    // there is something to do again.
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

        _enable();
    }

    void Yield()
    {
        auto prev = GetCurrentThread();
        SelectNextThread();
        x64::kernel_tss.rsp0 = GetCurrentThread()->ctx.rsp;
        x64::SwitchContext(&prev->ctx, &GetCurrentThread()->ctx);
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



    void* PushListEntry(void* list, void* ptr)
    {
        auto p = ( SList* )list;
        if (p)
        {
            while (p && p->next)
                p = p->next;
            p->next = ( SList* )ptr;
        }
        return p;
    }

    void* FindListEntry(void* list, void* ptr)
    {
        auto p = ( SList* )list;
        while (p)
        {
            if (p == ptr)
                return p;
            p = p->next;
        }
        return p;
    }

    void* RemoveListEntry(void* list, void* ptr)
    {
        auto p = ( SList* )list;
        while (p)
        {
            if (p->next == ptr)
            {
                p->next = p->next->next;
                return p;
            }
            p = p->next;
        }
        return p;
    }
}
