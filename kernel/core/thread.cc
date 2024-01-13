#include <libc/mem.h>

#include "ke.h"
#include "gfx/output.h"
#include "../common/mm.h"

EXTERN_C uptr_t kernel_stack_top;

namespace ke
{
    void* PushListEntry(void* list, void* ptr);
    void* RemoveListEntry(void* list, void* ptr);
    void* FindListEntry(void* list, void* ptr);

    static void IdleLoop(void*)
    {
        while (1)
        {
            _disable();
            _mm_pause();
            _enable();
        }
    }

    static void EntryPointThread()
    {
        auto cur_thread = GetCurrentThread();
        auto first_thread = GetFirstThread();

        cur_thread->function(cur_thread->arg);

        Free(cur_thread);
        RemoveListEntry(first_thread, cur_thread);

        // Wait for a new thread to take over.
        // TODO - is this right? maybe call the scheduler instead
        x64::Idle();
    }

    static Thread* CreateThreadInternal(void(*function)(void*), void* arg, vaddr_t kstack)
    {
        static size_t thread_i = 0;

        auto thread = Allocate<Thread>();

        thread->id = ++thread_i;
        thread->function = function;
        thread->arg = arg;

        thread->ctx.rip = ( u64 )EntryPointThread;
        thread->ctx.rsp = kstack;
        thread->ctx.rflags = ( u64 )(x64::RFLAG::IF | x64::RFLAG::ALWAYS);

        return thread;
    }

    void StartScheduler()
    {
        // FIXME - can we use kernel_stack_top here?
        // since every thread is going to have its own kernel stack,
        // we should be able to use the boot stack for the idle loop

        auto idle_thread = CreateThreadInternal(IdleLoop, nullptr, kernel_stack_top);

        WriteCore64(first_thread, idle_thread); // set first thread
        SetCurrentThread(idle_thread);

        schedule = true;
    }

    Thread* CreateThread(void(*function)(void*), void* arg, vaddr_t kstack)
    {
        if (!kstack)
        {
            kstack = ( vaddr_t )Allocate(page_size);
            kstack += page_size; // Stack starts at the top...
        }

        auto thread = CreateThreadInternal(function, arg, kstack);
        PushListEntry(GetFirstThread(), thread);
        return thread;
    }

    void SelectNextThread()
    {
        auto cur = GetCurrentThread();
        if (!cur->next)
            cur = GetFirstThread();
        else
            cur = ( Thread* )cur->next;

        SetCurrentThread(cur);
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
