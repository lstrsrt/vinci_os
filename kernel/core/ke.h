#pragma once

#include <base.h>

enum class Status
{
    Success = 0,
    UnsupportedSystem,
    Unreachable,
    OutOfMemory,
};

namespace ke
{
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
