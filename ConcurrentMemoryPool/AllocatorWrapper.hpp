#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "ConcurrentAlloc.hpp"

namespace cmp
{
template <class T>
inline T *AllocateRaw(std::size_t n = 1)
{
    if (n == 0)
    {
        return nullptr;
    }

    void *mem = ConcurrentAlloc(sizeof(T) * n);
    return static_cast<T *>(mem);
}

template <class T>
inline void DeallocateRaw(T *ptr, std::size_t n = 1)
{
    if (ptr == nullptr)
    {
        return;
    }

    ConcurrentFree(static_cast<void *>(ptr), sizeof(T) * n);
}

template <class T, class... Args>
inline T *New(Args &&...args)
{
    T *ptr = AllocateRaw<T>(1);
    try
    {
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }
    catch (...)
    {
        DeallocateRaw(ptr, 1);
        throw;
    }
}

template <class T>
inline void Delete(T *ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    ptr->~T();
    DeallocateRaw(ptr, 1);
}

template <class T>
struct Deleter
{
    void operator()(T *ptr) const
    {
        Delete(ptr);
    }
};

template <class T, class... Args>
inline std::unique_ptr<T, Deleter<T>> MakeUnique(Args &&...args)
{
    return std::unique_ptr<T, Deleter<T>>(New<T>(std::forward<Args>(args)...));
}

template <class T>
class PoolAllocator
{
public:
    typedef T value_type;
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    template <class U>
    struct rebind
    {
        typedef PoolAllocator<U> other;
    };

    PoolAllocator() noexcept
    {
    }

    template <class U>
    PoolAllocator(const PoolAllocator<U> &) noexcept
    {
    }

    pointer allocate(size_type n)
    {
        if (n > max_size())
        {
            throw std::bad_alloc();
        }
        return AllocateRaw<T>(n);
    }

    void deallocate(pointer p, size_type n) noexcept
    {
        DeallocateRaw<T>(p, n);
    }

    size_type max_size() const noexcept
    {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }
};

template <class T, class U>
inline bool operator==(const PoolAllocator<T> &, const PoolAllocator<U> &) noexcept
{
    return true;
}

template <class T, class U>
inline bool operator!=(const PoolAllocator<T> &, const PoolAllocator<U> &) noexcept
{
    return false;
}

} // namespace cmp
