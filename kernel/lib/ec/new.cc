#include "new.h"
#include "../core/ke.h"

void* operator new(size_t size)
{
    return ke::Allocate(size);
}

void* operator new[](size_t size)
{
    return ke::Allocate(size);
}

void operator delete(void* address)
{
    ke::Free(address);
}

void operator delete[](void* address)
{
    ke::Free(address);
}
