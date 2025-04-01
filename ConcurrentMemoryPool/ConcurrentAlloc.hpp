#pragma once

#include "Common.hpp"
#include "ThreadCache.hpp"

static void *ConcurrentAlloc(size_t size)
{
    if (pTLSThreadCache == nullptr)
    {
        pTLSThreadCache = new ThreadCache;
    }
    cout << "Thread ID: " << get_thread_id_str() << " ThreadCache: " << pTLSThreadCache << endl;

    return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void *ptr, size_t size)
{
    assert(pTLSThreadCache);
    pTLSThreadCache->Deallocate(ptr, size);
}
