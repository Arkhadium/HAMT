#include <hamt.hpp>
#include <iostream>

int main()
{
    std::cout << "Hello world" << std::endl;
    hamt::Hamt<std::string, int> map;
    std::string a = "abc";
    map.emplace(a, 2);
    map.emplace(a, 2);
    return 0;
}