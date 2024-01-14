#pragma once

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* address);
void operator delete[](void* address);
