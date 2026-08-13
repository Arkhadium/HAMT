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
    // EXPECT_EQ(map.find(1), it);
    // ASSERT_NE(it, map.end());
    // EXPECT_EQ(it->first, 1);
    // EXPECT_EQ(it->second, "one");
}

TEST(HamtEmplace, InsertsSeveralElements)
{
    Map map;

    EXPECT_TRUE(map.emplace(1, "one").second);
    EXPECT_TRUE(map.emplace(2, "two").second);
    EXPECT_TRUE(map.emplace(3, "three").second);

    // EXPECT_NE(map.find(1), map.end());
    // EXPECT_NE(map.find(2), map.end());
    // EXPECT_NE(map.find(3), map.end());

    // EXPECT_EQ(map.find(1)->second, "one");
    // EXPECT_EQ(map.find(2)->second, "two");
    // EXPECT_EQ(map.find(3)->second, "three");
}

TEST(HamtEmplace, DuplicateKeyIsNotInserted)
{
    Map map;

    auto [first, firstInserted] = map.emplace(42, "first");
    auto [second, secondInserted] = map.emplace(42, "second");

    EXPECT_TRUE(firstInserted);
    EXPECT_FALSE(secondInserted);

    // auto found = map.find(42);
    // ASSERT_NE(found, map.end());
    // EXPECT_EQ(found->second, "first");
}

TEST(HamtEmplace, StoredValueCanBeModifiedThroughIterator)
{
    Map map;

    auto [it, inserted] = map.emplace(7, "before");
    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());

    it->second = "after";

    // auto found = map.find(7);
    // ASSERT_NE(found, map.end());
    // EXPECT_EQ(found->second, "after");
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
} // namespace
