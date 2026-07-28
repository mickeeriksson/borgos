#include <stddef.h>
//#include "mem.h"
#include "types.h"

extern "C" void* kmalloc(size_t size, uint32_t flags);
extern "C" void* kfree(void* ptr);

void *operator new(size_t size)
{
    return kmalloc(size,0);
}

void *operator new[](size_t size)
{
    return kmalloc(size,0);
}

void operator delete(void *p)
{
    kfree(p);
}

void operator delete[](void *p)
{
    kfree(p);
}