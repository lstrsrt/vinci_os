#include <libc/mem.h>

#include "ke.h"
#include "gfx/output.h"

EXTERN_C uptr_t kernel_stack_top;

EXTERN_C void Switch(x64::Context* ctx);


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
        ke::cur_thread->function(ke::cur_thread->arg);

        Free(ke::cur_thread);
        RemoveListEntry(ke::first_thread, ke::cur_thread);

        for (auto p = ke::first_thread; p; p = ( Thread* )p->next)
            Print("0x%llx\n", ( u64 )p);

        // Wait for a new thread to take over.
        // TODO - is this right?
        x64::Idle();
    }

    static Thread* CreateThreadInternal(void(*function)(void*), void* arg, vaddr_t stack)
    {
        static size_t thread_i = 0;

        auto thread = Allocate<Thread>();

        thread->id = ++thread_i;
        thread->function = function;
        thread->arg = arg;

        thread->ctx.rip = ( u64 )EntryPointThread;
        thread->ctx.rsp = stack;
        thread->ctx.rflags = ( u64 )(x64::RFLAG::IF | x64::RFLAG::ALWAYS);

        return thread;
    }

    void StartScheduler()
    {
        // FIXME - can we use kernel_stack_top here?
        // since every thread is going to have its own kernel stack,
        // we should be able to use the boot stack for the idle loop
        first_thread = CreateThreadInternal(IdleLoop, nullptr, kernel_stack_top);
        cur_thread = first_thread;
        schedule = true;
    }

    Thread* CreateThread(void(*function)(void*), void* arg, vaddr_t stack)
    {
        auto thread = CreateThreadInternal(function, arg, stack);
        PushListEntry(first_thread, thread);
        return thread;
    }

    void SelectNextThread()
    {
        cur_thread = ( Thread* )cur_thread->next;
        if (!cur_thread)
            cur_thread = first_thread;
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
