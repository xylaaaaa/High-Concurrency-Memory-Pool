#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "AllocatorWrapper.hpp"

struct Session
{
    int id;
    std::string user;

    Session(int sid, const std::string &name)
        : id(sid), user(name)
    {
    }
};

int main()
{
    std::unique_ptr<Session, cmp::Deleter<Session> > session = cmp::MakeUnique<Session>(1001, "alice");
    std::cout << "session: id=" << session->id << ", user=" << session->user << std::endl;

    std::vector<int, cmp::PoolAllocator<int> > values;
    for (int i = 0; i < 100000; ++i)
    {
        values.push_back(i);
    }
    std::cout << "vector size: " << values.size() << ", first=" << values.front() << ", last=" << values.back() << std::endl;

    typedef std::basic_string<char, std::char_traits<char>, cmp::PoolAllocator<char> > PoolString;
    std::list<PoolString, cmp::PoolAllocator<PoolString> > logs;
    logs.push_back("allocator wrapper is ready");
    logs.push_back("stl container uses pool allocator");

    std::cout << "logs: " << logs.front() << " | " << logs.back() << std::endl;
    return 0;
}
