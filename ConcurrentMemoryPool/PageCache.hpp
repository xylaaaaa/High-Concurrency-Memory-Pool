#pragma once 

#include "Common.hpp"

class PageCache
{
public:
    static PageCache* GetInstance()
    {
        return &_sInst;
    }

    Span* NewSpan(size_t k)
    {
        assert(k > 0 && k < NPAGES);

        if (!_spanLists[k].Empty())
        {
            return _spanLists->PopFront();
        }

        for (size_t i = k + 1; i < NPAGES; i++)
        {
            if (!_spanLists[i].Empty())
            {
                Span* nSpan = _spanLists[i].PopFront();
                Span* kSpan = new Span;

                kSpan->_pageID = nSpan->_pageID;
                kSpan->_n = k;

                nSpan->_pageID += k;
                nSpan->_n -= k;

                _spanLists[nSpan->_n].PushFront(nSpan);
            }
        }

        Span* bigSpan = new Span;
        void* ptr = SystemAlloc(NPAGES - 1);
        bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT; //得到大内存块的起始页号
        bigSpan->_n = NPAGES - 1;

        _spanLists[bigSpan->_n].PushFront(bigSpan);

        return NewSpan(k);
    }

    std::mutex _pageMtx;

private:
    SpanList _spanLists[NPAGES];

    PageCache()
    {}

    PageCache(const PageCache&) = delete;

    static PageCache _sInst;
};

PageCache PageCache::_sInst;