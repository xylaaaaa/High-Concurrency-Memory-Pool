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
        while(start < end)
        {
            NextObj(tail) = start;
            tail = NextObj(tail);
            start += size; // 按照size大小切分
        }
        NextObj(tail) = nullptr;
        
        // 还回去要加锁
        list._mtx.lock();
        list.PushFront(span);
        return span;
    }

    size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
    {
        size_t index = SizeClass::Index(size);
        size_t transferNum = FetchRangeObjFromTransferCache(index, batchNum, start, end);
        if (transferNum > 0)
        {
            return transferNum;
        }

        _spanLists[index]._mtx.lock();

        Span* span = GetOneSpan(_spanLists[index], size);
        assert(span);
        assert(span->_freeList);

        //从span中获取batchNum个对象
        start = span->_freeList;
        end = start;
        size_t actualNum = 1;
        while (actualNum < batchNum && NextObj(end) != nullptr)
        {
            end = NextObj(end);
            actualNum++;
        }
        span->_freeList = NextObj(end);
        NextObj(end) = nullptr;
        span->_useCount += actualNum;
        _spanLists[index]._mtx.unlock();

        return actualNum;
    }

    void ReleaseListToSpans(void* start, size_t size, size_t n)
    {
        if (start == nullptr || n == 0)
        {
            return;
        }

        size_t index = SizeClass::Index(size);
        size_t cachedNum = PushRangeObjToTransferCache(index, size, start, n);
        n -= cachedNum;
        if (n == 0 || start == nullptr)
        {
            return;
        }

        _spanLists[index]._mtx.lock();
        while (start && n > 0)
        {
            void* next = NextObj(start);
            Span* span = PageCache::GetInstance()->MapObjectToSpan(start); //获取start对象对应的span
            NextObj(start) = span->_freeList;
            span->_freeList = start;
            span->_useCount--;
            
            if(span->_useCount == 0)
            {
                _spanLists[index].Erase(span); // 从spanList中移除
                span->_freeList = nullptr;
                span->_next = nullptr;
                span->_prev = nullptr;

                _spanLists[index]._mtx.unlock(); // 如果有其他线程需要使用这个span,需要解锁

                PageCache::GetInstance()->_pageMtx.lock();
                PageCache::GetInstance()->ReleaseSpanToPageCache(span);
                PageCache::GetInstance()->_pageMtx.unlock();

                _spanLists[index]._mtx.lock();
            }

            start = next;
            --n;
        }
        _spanLists[index]._mtx.unlock();
    }
    
private:
    size_t FetchRangeObjFromTransferCache(size_t index, size_t batchNum, void*& start, void*& end)
    {
        std::lock_guard<std::mutex> lock(_transferMtx[index]);
        size_t transferSize = _transferLists[index].Size();
        if (transferSize == 0)
        {
            start = nullptr;
            end = nullptr;
            return 0;
        }

        size_t fetchNum = std::min(batchNum, transferSize);
        start = _transferLists[index].Pop();
        end = start;
        for (size_t i = 1; i < fetchNum; ++i)
        {
            void* obj = _transferLists[index].Pop();
            NextObj(end) = obj;
            end = obj;
        }
        NextObj(end) = nullptr;
        return fetchNum;
    }

    size_t PushRangeObjToTransferCache(size_t index, size_t size, void*& start, size_t n)
    {
        std::lock_guard<std::mutex> lock(_transferMtx[index]);
        InitTransferCacheMaxSize(index, size);

        size_t transferSize = _transferLists[index].Size();
        if (transferSize >= _transferCacheMaxSize[index])
        {
            return 0;
        }

        size_t remainCapacity = _transferCacheMaxSize[index] - transferSize;
        size_t pushNum = std::min(n, remainCapacity);
        for (size_t i = 0; i < pushNum; ++i)
        {
            void* obj = start;
            start = NextObj(start);
            _transferLists[index].Push(obj);
        }

        return pushNum;
    }

    void InitTransferCacheMaxSize(size_t index, size_t size)
    {
        if (_transferCacheMaxSize[index] != 0)
        {
            return;
        }

        size_t base = SizeClass::NumMoveSize(size);
        _transferCacheMaxSize[index] = std::max(base * 4, static_cast<size_t>(2));
    }

    SpanList _spanLists[NFREELIST];
    FreeList _transferLists[NFREELIST];
    std::mutex _transferMtx[NFREELIST];
    size_t _transferCacheMaxSize[NFREELIST] = {0};

    CentralCache()
    {}

    CentralCache(const CentralCache&) = delete;

    static CentralCache _sInst;

};

CentralCache CentralCache::_sInst;
