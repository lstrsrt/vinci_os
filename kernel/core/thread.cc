#include <libc/mem.h>

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

        Free(cur_thread);
        RemoveListEntry(first_thread, cur_thread);

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

        thread->id = ++thread_i;
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

        core->first_thread = idle_thread;
        core->current_thread = idle_thread;
    }

    Thread* CreateThread(void(*function)(void*), void* arg, vaddr_t kstack)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto thread = CreateThreadInternal(function, arg, kstack);
        PushListEntry(GetCore()->first_thread, thread);
        return thread;
    }

    void SelectNextThread()
    {
        auto core = GetCore();
        auto prev = core->current_thread;

        auto next = core->first_thread;
        do
        {
            next = ( Thread* )next->next;
            if (!next)
                next = core->first_thread;

            if (next->state == Thread::State::Waiting)
            {
                if (next->delay <= timer::ticks)
                    break;
            }
        } while (next->state != Thread::State::Ready);

        if (next == core->first_thread)
            Print("selected idle thread\n");

        next->state = Thread::State::Running;
        core->current_thread = next;

        // If we came from a wait state, don't switch to ready yet
        if (prev->state == Thread::State::Running)
            prev->state = Thread::State::Ready;
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
        if (ticks > 0)
        {
            GetCurrentThread()->state = Thread::State::Waiting;
            GetCurrentThread()->delay = timer::ticks + ticks;
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
