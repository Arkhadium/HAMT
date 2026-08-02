#include <hamt.hpp>
#include <iostream>

int main()
{
    std::cout << "Hello world" << std::endl;
    hamt::Hamt<std::string, int> map;
    std::string a = "abc";
    map.emplace(a, 2);
    a = "hamt";
    map.emplace(a, 4);
    a = "b";
    map.emplace(a, 7);
    a = "hello";
    map.emplace(a, 8);
    a = "a";
    map.emplace(a, 9);
    a = "abcdefghijklm";
    map.emplace(a, 10);
    return 0;
}