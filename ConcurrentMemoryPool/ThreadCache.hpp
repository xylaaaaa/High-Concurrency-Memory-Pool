#pragma once
#include "Common.hpp"

class ThreadCache
{
public:
    void *Allocate(size_t size)
    {
        assert(size <= MAX_BYTES);
        size_t alignSize = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(size);

        if (!_freeLists[index].Empty())
        {
            return _freeLists[index].Pop();
        }
        else
        {
            return FetchFromCentralCache(index, alignSize); //FetchFromCentralCache 是怎么实现的? 为什么要传入index和alignSzie?
        }
    }

    void Deallocate(void *ptr, size_t size)
    {
        assert(ptr);
        assert(size <= MAX_BYTES);

        size_t index = SizeClass::Index(size);
        _freeLists[index].Push(ptr);
    }

    void *FetchFromCentralCache(size_t index, size_t size)
    {
        return nullptr;
    }

private:
    FreeList _freeLists[NFREELIST];
};

#ifdef _WIN32
static _declspec(thread) ThreadCache *pTLSThreadCache = nullptr;
#else
static thread_local ThreadCache *pTLSThreadCache = nullptr;
#endif
 