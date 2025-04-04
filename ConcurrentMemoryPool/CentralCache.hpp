#pragma once

#include "Common.hpp"

#include "PageCache.hpp"

class CentralCache
{
public:
    static CentralCache* GetInstance()
    {
        return &_sInst;
    }

    Span* GetOneSpan(SpanList& list, size_t size)
    {
        Span* it = list.Begin();
        while (it != list.End())
        {
            if (it -> _freeList != nullptr)
            {
                return it;
            }
            else
            {
                it = it->_next;
            }
        }
        //解锁, 不然如果有释放内存回来的无法回来
        list._mtx.unlock();

        //没有空闲Span了 需要从page Cache 获取
        PageCache::GetInstance()->_pageMtx.lock();
        Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
        PageCache::GetInstance()->_pageMtx.unlock();

        //对span切分,不需要加锁,因为其他线程访问不到这个span

        char* start = (char*)(span->_pageID << PAGE_SHIFT); //起始地址
        size_t bytes = span->_n << PAGE_SHIFT; //大块内存大小
        char* end = start + bytes;

        span->_freeList = start;
        start += size;
        void* tail = span -> _freeList;
        int i = 1;
        while(start < end)
        {
            i++;
            NextObj(tail) = start;
            tail = NextObj(tail);
            start += size;
        }
        
        // 还回去要加锁
        list._mtx.lock();
        list.PushFront(span);
        return span;
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
        span->_useCount += actualNum;
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