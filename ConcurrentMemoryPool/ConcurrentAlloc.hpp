#pragma once

#include <cstdlib>

#include "Common.hpp"
#include "ThreadCache.hpp"

static void *ConcurrentAlloc(size_t size)
{
    if (size == 0)
    {
        size = 1;
    }

    if (size > THREAD_CACHE_MAX_BYTES)
    {
        return std::malloc(size);
    }

    if (pTLSThreadCache == nullptr)
    {
        pTLSThreadCache = new ThreadCache;
    }
#if defined(CMP_ALLOC_TRACE) && CMP_ALLOC_TRACE
    cout << "Thread ID: " << get_thread_id_str() << " ThreadCache: " << pTLSThreadCache << endl;
#endif

    return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void *ptr, size_t size)
{
    if (ptr == nullptr)
    {
        return;
    }

    if (size == 0)
    {
        size = 1;
    }

    if (size > THREAD_CACHE_MAX_BYTES)
    {
        std::free(ptr);
        return;
    }

    assert(pTLSThreadCache);
    pTLSThreadCache->Deallocate(ptr, size);
}
