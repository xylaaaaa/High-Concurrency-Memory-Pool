#pragma once

#include "Common.hpp"

class PageCache
{
public:
    static PageCache *GetInstance()
    {
        return &_sInst;
    }

    Span *NewSpan(size_t k)
    {
        assert(k > 0 && k < NPAGES);

        if (!_spanLists[k].Empty())
        {
            return _spanLists[k].PopFront();
        }

        for (size_t i = k + 1; i < NPAGES; i++)
        {
            if (!_spanLists[i].Empty())
            {
                Span *nSpan = _spanLists[i].PopFront();
                Span *kSpan = new Span;

                kSpan->_pageID = nSpan->_pageID;
                kSpan->_n = k;

                nSpan->_pageID += k;
                nSpan->_n -= k;

                _spanLists[nSpan->_n].PushFront(nSpan);
            }
        }

        Span *bigSpan = new Span;
        void *ptr = SystemAlloc(NPAGES - 1);
        bigSpan->_pageID = (PAGE_ID)ptr >> PAGE_SHIFT; // 得到大内存块的起始页号
        bigSpan->_n = NPAGES - 1;

        _spanLists[bigSpan->_n].PushFront(bigSpan);

        return NewSpan(k);
    }

    Span* MapObjectToSpan(void *obj)
    {
        PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT); // 将obj强转为PAGE_ID, 然后右移PAGE_SHIFT位,得到页号
        auto ret = _idSpanMap.find(id);
        if (ret != _idSpanMap.end())
        {
            return ret->second;
        }
        else
        {
            assert(false);
            return nullptr;
        }

    }

    void ReleaseSpanToPageCache(Span* span)
    {
        // 对span前后的页进行合并,缓解外内存碎片问题
        while(1)
        {
            PAGE_ID prevId = span->_pageID - 1;
            auto ret = _idSpanMap.find(prevId);
            if (ret == _idSpanMap.end())
            {   
                break;
            }
            Span* prevSpan = ret->second;
            if (prevSpan->_isUse == true)
            {
                break;
            }

            if (prevSpan->_n + span->_n > NPAGES - 1)
            {
                break;
            }

            span->_pageID = prevSpan->_pageID;
            span->_n += prevSpan->_n;

            _spanLists[prevSpan->_n].Erase(prevSpan);
            delete prevSpan;
        }

        while(1)
        {
            PAGE_ID nextId = span->_pageID + span->_n;
            auto ret = _idSpanMap.find(nextId);

            if (ret == _idSpanMap.end())
            {
                break;
            }

            Span* nextSpan = ret->second;
            if (nextSpan->_isUse == true)
            {
                break;
            }
            
            if (nextSpan->_n + span->_n > NPAGES - 1)
            {
                break;
            } 
            span->_n += nextSpan->_n;
            _spanLists[nextSpan->_n].Erase(nextSpan);
            delete nextSpan;
        }

        _spanLists[span->_n].PushFront(span);
        span->_isUse = false;
        _idSpanMap[span->_pageID] = span;
        _idSpanMap[span->_pageID + span->_n - 1] = span;
    }


    std::mutex _pageMtx;

private:
    SpanList _spanLists[NPAGES];

    std::unordered_map<PAGE_ID, Span*> _idSpanMap;

    PageCache()
    {
    }

    PageCache(const PageCache &) = delete;

    static PageCache _sInst;
};

PageCache PageCache::_sInst;