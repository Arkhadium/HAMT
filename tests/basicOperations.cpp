#include <gtest/gtest.h>
#include <hamt.hpp>

template <class Key, class T>
bool emplace(Key k, T t)
{
    Hamt<Key, T> hamt;
    auto result = hamt.emplace(k, t);

    if (!result.second)
        return false;

    return result.first == t;
}

TEST(EmplaceTests, SingleEmplace)
{
    ASSERT_TRUE(emplace("abc", 3));
}