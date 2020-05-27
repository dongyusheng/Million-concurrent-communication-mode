#ifndef __ALLOCTOR_H__
#define __ALLOCTOR_H__

#include "MemoryMgr.hpp"

void *operator new(size_t size);
void operator delete(void* p);

void *operator new[](size_t size);
void operator delete[](void* p);

void *mem_alloc(size_t size);
void mem_free(void* p);

#endif