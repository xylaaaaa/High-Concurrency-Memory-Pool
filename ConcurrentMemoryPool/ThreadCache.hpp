#pragma once
#include "Common.hpp"
#include "CentralCache.hpp"

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

        //当链表长度大于一次批量申请的内存时就开始还一段list给central cache
        if (_freeLists[index].Size() >= _freeLists[index].MaxSize())
        {
            ListTooLong(_freeLists[index], size);
        }
    }

    void ListTooLong(FreeList& list, size_t size)
    {
        void* start = nullptr;
        void* end = nullptr;
        list.PopRange(start, end, list.MaxSize());

        CentralCache::GetInstance()->ReleaseListToSpans(start, size);
    }

    void *FetchFromCentralCache(size_t index, size_t size)
    {
        //慢开始反馈调节算法

        size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));

        if (_freeLists[index].MaxSize() == batchNum)
        {
            _freeLists[index].MaxSize()++;
        }

        void* start = nullptr;
        void* end = nullptr;
        size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);

        assert(actualNum > 0); 

        if (actualNum == 1)
        {
            assert(start == end);
            return start;
        }
        else
        {
            _freeLists[index].PushRange(NextObj(start), end, actualNum - 1);
            return start;
        }
    }

private:
    FreeList _freeLists[NFREELIST];
};

#ifdef _WIN32
static _declspec(thread) ThreadCache *pTLSThreadCache = nullptr;
#else
static thread_local ThreadCache *pTLSThreadCache = nullptr;
#endif
 
