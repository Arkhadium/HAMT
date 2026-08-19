#include <gtest/gtest.h>
#include <string>

#include "hamt.hpp"

namespace
{
using Map = hamt::Hamt<int, std::string>;

TEST(HamtErase, ErasingMissingKeyReturnsZero)
{
    Map map;
    map.emplace(1, "one");

    EXPECT_EQ(map.erase(999), 0u);
    EXPECT_NE(map.find(1), map.end());
}

TEST(HamtErase, ErasesExistingKey)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    EXPECT_EQ(map.erase(1), 1u);
    EXPECT_EQ(map.find(1), map.end());

    auto remaining = map.find(2);
    ASSERT_NE(remaining, map.end());
    EXPECT_EQ(remaining->second, "two");
}

TEST(HamtErase, ErasingSameKeyTwiceOnlySucceedsOnce)
{
    Map map;
    map.emplace(42, "value");

    EXPECT_EQ(map.erase(42), 1u);
    EXPECT_EQ(map.erase(42), 0u);
    EXPECT_EQ(map.find(42), map.end());
}

TEST(HamtErase, EraseByIteratorRemovesElement)
{
    Map map;
    map.emplace(10, "ten");
    map.emplace(20, "twenty");

    auto it = map.find(10);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(map.erase(it), 1u);
    EXPECT_EQ(map.find(10), map.end());
    EXPECT_NE(map.find(20), map.end());
}

TEST(HamtErase, ErasingOneElementDoesNotInvalidateOtherValuesSemantically)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");
    map.emplace(3, "three");

    ASSERT_EQ(map.erase(2), 1u);

    auto one = map.find(1);
    auto three = map.find(3);

    ASSERT_NE(one, map.end());
    ASSERT_NE(three, map.end());
    EXPECT_EQ(one->second, "one");
    EXPECT_EQ(three->second, "three");
}

TEST(HamtErase, EraseAllElementsLeavesEmptyIterationRange)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    EXPECT_EQ(map.erase(1), 1u);
    EXPECT_EQ(map.erase(2), 1u);

    EXPECT_EQ(map.begin(), map.end());
}
} // namespace
