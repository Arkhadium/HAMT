#include <gtest/gtest.h>
#include <hamt.hpp>
#include <string>
#include <type_traits>

namespace
{
using Map = hamt::Hamt<int, std::string>;

TEST(HamtFind, EmptyMapReturnsEnd)
{
    Map map;
    EXPECT_EQ(map.find(123), map.end());
}

TEST(HamtFind, FindsExistingElement)
{
    Map map;

    EXPECT_EQ(map.find(2), map.end());

    map.emplace(1, "one");
    map.emplace(2, "two");
    map.emplace(3, "three");

    auto it = map.find(2);

    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, "two");

    map.emplace(1, "one");
    map.emplace(3, "three");

    auto it2 = map.find(2);

    ASSERT_NE(it2, map.end());
    EXPECT_EQ(it2->first, 2);
    EXPECT_EQ(it2->second, "two");
}

TEST(HamtFind, MissingKeyReturnsEnd)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    EXPECT_EQ(map.find(999), map.end());
}

TEST(HamtFind, ConstMapReturnsConstIterator)
{
    Map map;
    map.emplace(5, "five");

    const Map& constMap = map;
    auto it = constMap.find(5);

    static_assert(std::is_same_v<decltype(it), Map::const_iterator>);

    ASSERT_NE(it, constMap.end());
    EXPECT_EQ(it->first, 5);
    EXPECT_EQ(it->second, "five");
}

TEST(HamtFind, FindingOneKeyDoesNotReturnAnotherValue)
{
    Map map;
    map.emplace(11, "eleven");
    map.emplace(12, "twelve");

    auto eleven = map.find(11);
    auto twelve = map.find(12);

    ASSERT_NE(eleven, map.end());
    ASSERT_NE(twelve, map.end());
    EXPECT_NE(eleven, twelve);
    EXPECT_EQ(eleven->second, "eleven");
    EXPECT_EQ(twelve->second, "twelve");
}
} // namespace
