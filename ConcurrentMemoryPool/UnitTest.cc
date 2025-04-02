#include "Objectpool.hpp"

#include "ConcurrentAlloc.hpp"

#include "CentralCache.hpp"

void Alloc1()
{
    for (size_t i = 0; i < 5; i++)
    {
        void *ptr = ConcurrentAlloc(6);
    }
}

void Alloc2()
{
    for (size_t i = 0; i < 5; i++)
    {
        void *ptr = ConcurrentAlloc(7);
    }
}

void TLSTest()
{
    // 创建两个线程，并行执行
    std::thread t1(Alloc1);
    std::thread t2(Alloc2);

    // 等待两个线程都完成
    t1.join();
    t2.join();
}

int main()
{
    // TestObjectPool();
    TLSTest();
    return 0;
}
