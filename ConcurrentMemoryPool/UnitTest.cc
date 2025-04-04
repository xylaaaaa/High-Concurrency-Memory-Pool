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

void TestConcurrentAlloc()
{
    void* p1 = ConcurrentAlloc(6);
    void *p2 = ConcurrentAlloc(8);
    void *p3 = ConcurrentAlloc(1);
    void *p4 = ConcurrentAlloc(7);
    void *p5 = ConcurrentAlloc(8);

    cout << p1 << endl;
    cout << p2 << endl;
    cout << p3 << endl;
    cout << p4 << endl;
    cout << p5 << endl;
}

void TestConcurrentAlloc2()
{
    for (size_t i = 0; i < 1024; i++)
    {
        void* p1 = ConcurrentAlloc(6);
        cout << p1 << endl;
    }

    void* p2 = ConcurrentAlloc(6);
    cout << p2 << endl;
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
    // TLSTest();
    // TestConcurrentAlloc();
    TestConcurrentAlloc2();
    return 0;
}
