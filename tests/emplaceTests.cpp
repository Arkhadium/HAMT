#include <gtest/gtest.h>
#include <hamt.hpp>
#include <string>
#include <utility>

namespace
{
using Map = hamt::Hamt<int, std::string>;

TEST(HamtEmplace, InsertsOneElement)
{
    Map map;

    auto [it, inserted] = map.emplace(1, "one");

    EXPECT_TRUE(inserted);
    EXPECT_EQ(map.find(1), it);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->first, 1);
    EXPECT_EQ(it->second, "one");
}

TEST(HamtEmplace, InsertsSeveralElements)
{
    Map map;

    EXPECT_TRUE(map.emplace(1, "one").second);
    EXPECT_TRUE(map.emplace(2, "two").second);
    EXPECT_TRUE(map.emplace(3, "three").second);

    EXPECT_NE(map.find(1), map.end());
    EXPECT_NE(map.find(2), map.end());
    EXPECT_NE(map.find(3), map.end());

    EXPECT_EQ(map.find(1)->second, "one");
    EXPECT_EQ(map.find(2)->second, "two");
    EXPECT_EQ(map.find(3)->second, "three");
}

TEST(HamtEmplace, DuplicateKeyIsNotInserted)
{
    Map map;

    auto [first, firstInserted] = map.emplace(42, "first");
    auto [second, secondInserted] = map.emplace(42, "second");

    EXPECT_TRUE(firstInserted);
    EXPECT_FALSE(secondInserted);

    auto found = map.find(42);
    ASSERT_NE(found, map.end());
    EXPECT_EQ(found->second, "first");
}

TEST(HamtEmplace, StoredValueCanBeModifiedThroughIterator)
{
    Map map;

    auto [it, inserted] = map.emplace(7, "before");
    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    it->second = "after";

    auto found = map.find(7);
    ASSERT_NE(found, map.end());
    EXPECT_EQ(found->second, "after");
}

TEST(HamtEmplace, SupportsEmptyStringValue)
{
    Map map;

    auto [it, inserted] = map.emplace(10, std::string{});

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->first, 10);
    EXPECT_TRUE(it->second.empty());
}

TEST(HamtEmplace, InsertedIteratorPointsToInsertedElement)
{
    Map map;

    auto [it, inserted] = map.emplace(12, "twelve");

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(it->first, 12);
    EXPECT_EQ(it->second, "twelve");
}

TEST(HamtEmplace, DuplicateDoesNotOverwriteExistingValue)
{
    Map map;

    ASSERT_TRUE(map.emplace(5, "original").second);

    auto [it, inserted] = map.emplace(5, "replacement");

    EXPECT_FALSE(inserted);

    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->first, 5);
    EXPECT_EQ(it->second, "original");
}

TEST(HamtEmplace, InsertsNegativeKey)
{
    Map map;

    auto [it, inserted] = map.emplace(-1, "negative");

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(it->first, -1);
    EXPECT_EQ(it->second, "negative");
}

TEST(HamtEmplace, InsertsZeroKey)
{
    Map map;

    auto [it, inserted] = map.emplace(0, "zero");

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(it->first, 0);
    EXPECT_EQ(it->second, "zero");
}

TEST(HamtEmplace, InsertsLargeKey)
{
    Map map;

    constexpr int key = std::numeric_limits<int>::max();

    auto [it, inserted] = map.emplace(key, "max");

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(it->first, key);
    EXPECT_EQ(it->second, "max");
}

TEST(HamtEmplace, InsertsManyElements)
{
    Map map;

    constexpr int count = 1000;

    for (int i = 0; i < count; ++i)
    {
        auto [it, inserted] = map.emplace(i, std::to_string(i));

        ASSERT_TRUE(inserted) << "Insertion failed for key " << i;
        ASSERT_NE(it, map.end());

        EXPECT_EQ(it->first, i);
        EXPECT_EQ(it->second, std::to_string(i));
    }
}

TEST(HamtEmplace, ReinsertingManyKeysFails)
{
    Map map;

    constexpr int count = 100;

    for (int i = 0; i < count; ++i)
        ASSERT_TRUE(map.emplace(i, "first").second);

    for (int i = 0; i < count; ++i)
    {
        auto [it, inserted] = map.emplace(i, "second");

        EXPECT_FALSE(inserted) << "Duplicate inserted for key " << i;

        ASSERT_NE(it, map.end());
        EXPECT_EQ(it->second, "first");
    }
}

TEST(HamtEmplace, SupportsLongString)
{
    Map map;

    std::string value(10'000, 'x');

    auto [it, inserted] = map.emplace(1, value);

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(it->second, value);
}

TEST(HamtEmplace, AcceptsMovedString)
{
    Map map;

    std::string value = "hello";

    auto [it, inserted] = map.emplace(1, std::move(value));

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    EXPECT_EQ(it->second, "hello");
}
} // namespace
