#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <time.h>
#include <assert.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <sstream> // 添加 ostringstream 的头文件
#ifdef _WIN32
#include <windows.h> // 添加 Windows API 头文件，用于 GetCurrentThreadId()
#endif

// 获取线程ID的跨平台函数
inline std::string get_thread_id_str()
{
    std::ostringstream oss;
#ifdef _WIN32
    // Windows下获取线程ID
    oss << GetCurrentThreadId();
#else
    // Linux/Unix下获取线程ID
    oss << std::this_thread::get_id();
#endif
    return oss.str();
}

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELIST = 208;

static void *&NextObj(void *obj) // 返回void*的引用
{
    // 这里需要加引用&的原因:
    // 1. 我们需要修改obj指向的内存中存储的指针值,如果不加引用,返回的是一个临时值拷贝
    // 2. 如果不加引用,*(void**)obj 会返回一个右值,右值无法被修改
    // 3. 加了引用后返回的是内存位置的引用,可以通过这个引用修改指针值
    // 4. 在自由链表中,我们需要修改next指针来维护链表关系,所以必须返回引用
    return *(void **)obj;
}

class FreeList
{
public:
    void Push(void *obj)
    {
        assert(obj);
        NextObj(obj) = _freeList;
        _freeList = obj;
    }

    void *Pop()
    {
        assert(_freeList);
        void *obj = _freeList;
        _freeList = NextObj(obj);
        return obj;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }

private:
    void *_freeList = nullptr;
};

// 计算对象大小的对齐映射规则
class SizeClass
{
public:
    // 整体控制在最多10%左右的内碎片浪费
    // [1, 128] 8byte对齐 freeList[0, 16)
    // [128+1, 1024] 16byte对齐 freeList[16, 72)
    // [1024+1, 8*1024] 128byte对齐 freeList[72, 128)
    // [8*1024+1, 64*1024] 1024byte对齐 freeList[128, 184)
    // [64*1024+1, 256*1024] 8*1024byte对齐 freeList[184, 208)

    /*size_t _RoundUp(size_t bytes, size_t alignNum)
    {
        size_t alignSize;
        if (bytes % alignNum == 0)
        {
            alignSize = bytes;
        }
        else
        {
            alignSize = (bytes / alignNum + 1) * alignNum;
        }
        return alignSize;
    }*/

    inline static size_t _RoundUp(size_t bytes, size_t alignNum)
    {
        return ((bytes + alignNum - 1) & ~(alignNum - 1));
    }

    inline static size_t RoundUp(size_t bytes)
    {
        if (bytes <= 128)
        {
            return _RoundUp(bytes, 8);
        }
        else if (bytes <= 1024)
        {
            return _RoundUp(bytes, 16);
        }
        else if (bytes <= 8 * 1024)
        {
            return _RoundUp(bytes, 128);
        }
        else if (bytes <= 64 * 1024)
        {
            return _RoundUp(bytes, 1024);
        }
        else if (bytes <= 256 * 1024)
        {
            return _RoundUp(bytes, 8 * 1024);
        }
        else
        {
            assert(false);
            return -1;
        }
    }

    // 这个函数用于计算给定字节数在特定对齐方式下的自由链表下标
    // bytes: 需要分配的字节数
    // align_shift: 对齐位数(2^align_shift 表示对齐大小)
    // 例如:
    // 如果 bytes=500, align_shift=4 (对齐到16字节)
    // 则返回 ((500 + 16 - 1) >> 4) - 1 = 31
    // 表示应该使用自由链表中的第31个位置
    inline static size_t _Index(size_t bytes, size_t align_shfit)
    {
        return ((bytes + (1 << align_shfit) - 1) >> align_shfit) - 1;
    }

    inline static size_t Index(size_t bytes)
    {
        assert(bytes <= MAX_BYTES);
        static int group_array[4] = {16, 56, 56, 56};
        if (bytes <= 128)
        {
            return _Index(bytes, 3);
        }
        else if (bytes <= 1024)
        {
            return _Index(bytes - 128, 4) + group_array[0]; // 为什么减去128? 因为128字节是第一个区间 所以需要减去128 为什么要+group_array[0]? 因为group_array[0]表示第一个区间的自由链表个数 _Index返回的是相对位置
        }
        else if (bytes <= 8 * 1024)
        {
            return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (bytes <= 64 * 1024)
        {
            return _Index(bytes - 8 * 1024, 8) + group_array[2] + group_array[1] + group_array[0];
        }
        else if (bytes <= 256 * 1024)
        {
            return _Index(bytes - 64 * 1024, 9) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
        }
        return -1;
    }
};

inline pid_t get_process_id()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}