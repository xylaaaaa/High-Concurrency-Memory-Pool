#pragma once

#include "Common.hpp"

class CentralCache
{
public:
    static CentralCache* GetInstance()
    {
        return &_sInst;
    }

    Span* GetOneSpan(SpanList& list, size_t size)
    {
        //..
        return nullptr;

    }

    size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
    {
        size_t index = SizeClass::Index(size);
        _spanLists[index]._mtx.lock();

        Span* span = GetOneSpan(_spanLists[index], size);
        assert(span);
        assert(span->_freeList);

        //从span中获取batchNum个对象
        start = span->_freeList;
        end = start;
        size_t i = 0;
        size_t actualNum = 1;
        while (i < batchNum && NextObj(end) != nullptr)
        {
            end = NextObj(end);
            i++;
            actualNum++;
        }
        span->_freeList = NextObj(end);
        NextObj(end) = nullptr;
        _spanLists[index]._mtx.unlock();

        return actualNum;

    }

private:
    SpanList _spanLists[NFREELIST];

    CentralCache()
    {}

    CentralCache(const CentralCache&) = delete;

    static CentralCache _sInst;

};

CentralCache CentralCache::_sInst;