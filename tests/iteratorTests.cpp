#include <gtest/gtest.h>
#include <hamt.hpp>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
using Map = hamt::Hamt<int, std::string>;

TEST(HamtIterator, EmptyMapBeginEqualsEnd)
{
    Map map;
    EXPECT_EQ(map.begin(), map.end());
}

TEST(HamtIterator, ConstEmptyMapBeginEqualsEnd)
{
    const Map map;
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(map.cbegin(), map.cend());
}

TEST(HamtIterator, BeginPointsToOnlyElement)
{
    Map map;
    map.emplace(42, "forty-two");

    auto it = map.begin();

    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->first, 42);
    EXPECT_EQ(it->second, "forty-two");
}

TEST(HamtIterator, IncrementSingleElementReachesEnd)
{
    Map map;
    map.emplace(42, "forty-two");

    auto it = map.begin();
    ASSERT_NE(it, map.end());

    ++it;

    EXPECT_EQ(it, map.end());
}

TEST(HamtIterator, PostIncrementReturnsPreviousIterator)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    auto it = map.begin();
    auto previous = it++;

    EXPECT_NE(previous, map.end());
    EXPECT_NE(it, previous);
}

TEST(HamtIterator, PreIncrementReturnsCurrentIterator)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    auto it = map.begin();
    auto& returned = ++it;

    EXPECT_EQ(&returned, &it);
}

TEST(HamtIterator, IteratesOverAllElements)
{
    Map map;

    map.emplace(1, "one");
    map.emplace(2, "two");
    map.emplace(3, "three");
    map.emplace(4, "four");
    map.emplace(5, "five");

    std::set<int> keys;

    for (auto it = map.begin(); it != map.end(); ++it)
        keys.insert(it->first);

    EXPECT_EQ(keys.size(), 5u);
    EXPECT_TRUE(keys.contains(1));
    EXPECT_TRUE(keys.contains(2));
    EXPECT_TRUE(keys.contains(3));
    EXPECT_TRUE(keys.contains(4));
    EXPECT_TRUE(keys.contains(5));
}

TEST(HamtIterator, RangeForVisitsAllElements)
{
    Map map;

    map.emplace(10, "ten");
    map.emplace(20, "twenty");
    map.emplace(30, "thirty");

    std::set<int> keys;

    for (const auto& [key, value] : map)
    {
        keys.insert(key);
        EXPECT_FALSE(value.empty());
    }

    EXPECT_EQ(keys.size(), 3u);
    EXPECT_TRUE(keys.contains(10));
    EXPECT_TRUE(keys.contains(20));
    EXPECT_TRUE(keys.contains(30));
}

TEST(HamtIterator, ValueCanBeModifiedThroughIterator)
{
    Map map;
    map.emplace(7, "before");

    auto it = map.begin();

    ASSERT_NE(it, map.end());

    it->second = "after";

    EXPECT_EQ(it->second, "after");
}

TEST(HamtIterator, KeyIsConstThroughIterator)
{
    using Reference = decltype(*std::declval<Map::iterator&>());
    using KeyReference = decltype((std::declval<Reference>().first));

    static_assert(std::is_const_v<std::remove_reference_t<KeyReference>>);
}

TEST(HamtIterator, IteratorCanConvertToConstIterator)
{
    Map map;
    map.emplace(1, "one");

    Map::iterator it = map.begin();
    Map::const_iterator constIt = it;

    ASSERT_NE(constIt, map.cend());
    EXPECT_EQ(constIt->first, 1);
    EXPECT_EQ(constIt->second, "one");
}

TEST(HamtIterator, ConstIteratorReadsElements)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    const Map& constMap = map;

    std::set<int> keys;

    for (auto it = constMap.begin(); it != constMap.end(); ++it)
        keys.insert(it->first);

    EXPECT_EQ(keys.size(), 2u);
    EXPECT_TRUE(keys.contains(1));
    EXPECT_TRUE(keys.contains(2));
}

TEST(HamtIterator, CBeginAndBeginPointToSameElement)
{
    Map map;
    map.emplace(123, "value");

    const Map& constMap = map;

    auto begin = constMap.begin();
    auto cbegin = constMap.cbegin();

    ASSERT_NE(begin, constMap.end());
    ASSERT_NE(cbegin, constMap.cend());

    EXPECT_EQ(begin->first, cbegin->first);
    EXPECT_EQ(begin->second, cbegin->second);
}

TEST(HamtIterator, TwoBeginIteratorsCompareEqual)
{
    Map map;
    map.emplace(1, "one");

    auto lhs = map.begin();
    auto rhs = map.begin();

    EXPECT_EQ(lhs, rhs);
}

TEST(HamtIterator, DifferentPositionsCompareDifferent)
{
    Map map;
    map.emplace(1, "one");
    map.emplace(2, "two");

    auto first = map.begin();
    auto second = first;

    ++second;

    ASSERT_NE(first, map.end());
    EXPECT_NE(first, second);
}

TEST(HamtIterator, TraversalDoesNotDuplicateElements)
{
    Map map;

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
        ASSERT_TRUE(map.emplace(i, std::to_string(i)).second);

    std::set<int> seen;

    for (auto it = map.begin(); it != map.end(); ++it)
    {
        const auto [_, inserted] = seen.insert(it->first);
        EXPECT_TRUE(inserted) << "Iterator returned key " << it->first << " more than once";
    }

    EXPECT_EQ(seen.size(), static_cast<std::size_t>(count));
}

TEST(HamtIterator, TraversalWorksAfterSeveralInsertions)
{
    Map map;

    constexpr int count = 1'000;

    for (int i = 0; i < count; ++i)
        ASSERT_TRUE(map.emplace(i, std::to_string(i)).second);

    std::size_t visited = 0;

    for (auto it = map.begin(); it != map.end(); ++it)
        ++visited;

    EXPECT_EQ(visited, static_cast<std::size_t>(count));
}
} // namespace
