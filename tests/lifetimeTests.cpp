#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <utility>

#include "hamt.hpp"

namespace
{
struct Tracked
{
    static inline int alive = 0;
    static inline int defaultConstructed = 0;
    static inline int valueConstructed = 0;
    static inline int copyConstructed = 0;
    static inline int moveConstructed = 0;
    static inline int copyAssigned = 0;
    static inline int moveAssigned = 0;
    static inline int destroyed = 0;

    std::string value;

    Tracked()
    {
        ++alive;
        ++defaultConstructed;
        std::cout << "Tracked()\n";
    }

    explicit Tracked(std::string v)
        : value(std::move(v))
    {
        ++alive;
        ++valueConstructed;
        std::cout << "Tracked(value)\n";
    }

    Tracked(const Tracked& other)
        : value(other.value)
    {
        ++alive;
        ++copyConstructed;
        std::cout << "Tracked(copy)\n";
    }

    Tracked(Tracked&& other) noexcept
        : value(std::move(other.value))
    {
        ++alive;
        ++moveConstructed;
        std::cout << "Tracked(move)\n";
    }

    Tracked& operator=(const Tracked& other)
    {
        ++copyAssigned;
        value = other.value;
        std::cout << "Tracked::operator=(copy)\n";
        return *this;
    }

    Tracked& operator=(Tracked&& other) noexcept
    {
        ++moveAssigned;
        value = std::move(other.value);
        std::cout << "Tracked::operator=(move)\n";
        return *this;
    }

    ~Tracked()
    {
        --alive;
        ++destroyed;
        std::cout << "~Tracked()\n";
    }

    friend bool operator==(const Tracked&, const Tracked&) = default;

    static void resetCounters()
    {
        alive = 0;
        defaultConstructed = 0;
        valueConstructed = 0;
        copyConstructed = 0;
        moveConstructed = 0;
        copyAssigned = 0;
        moveAssigned = 0;
        destroyed = 0;
    }

    static int totalConstructions()
    {
        return defaultConstructed + valueConstructed + copyConstructed + moveConstructed;
    }
};

class HamtLifetime : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Tracked::resetCounters();
    }
};

using Map = hamt::Hamt<int, Tracked>;

TEST_F(HamtLifetime, EmplacedObjectRemainsAliveWhileStored)
{
    {
        Map map;
        EXPECT_EQ(Tracked::alive, 0);

        auto [it, inserted] = map.emplace(1, Tracked{ "hello" });

        ASSERT_TRUE(inserted);
        ASSERT_NE(it, map.end());
        EXPECT_EQ(it->second.value, "hello");
        EXPECT_EQ(Tracked::alive, 1);
    }

    EXPECT_EQ(Tracked::alive, 0);
    EXPECT_EQ(Tracked::destroyed, Tracked::totalConstructions());
}

TEST_F(HamtLifetime, EraseDestroysStoredObject)
{
    Map map;
    map.emplace(1, Tracked{ "hello" });

    ASSERT_EQ(Tracked::alive, 1);
    const int destroyedBeforeErase = Tracked::destroyed;

    EXPECT_EQ(map.erase(1), 1u);

    EXPECT_EQ(Tracked::alive, 0);
    EXPECT_GT(Tracked::destroyed, destroyedBeforeErase);
}

TEST_F(HamtLifetime, FindDoesNotCopyOrMoveStoredObject)
{
    Map map;
    map.emplace(1, Tracked{ "hello" });

    const int copiesBefore = Tracked::copyConstructed;
    const int movesBefore = Tracked::moveConstructed;
    const int copyAssignmentsBefore = Tracked::copyAssigned;
    const int moveAssignmentsBefore = Tracked::moveAssigned;

    auto it = map.find(1);

    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->second.value, "hello");
    EXPECT_EQ(Tracked::copyConstructed, copiesBefore);
    EXPECT_EQ(Tracked::moveConstructed, movesBefore);
    EXPECT_EQ(Tracked::copyAssigned, copyAssignmentsBefore);
    EXPECT_EQ(Tracked::moveAssigned, moveAssignmentsBefore);
}

TEST_F(HamtLifetime, IteratorDereferenceDoesNotCopyOrMoveStoredObject)
{
    Map map;
    map.emplace(1, Tracked{ "hello" });

    const int copiesBefore = Tracked::copyConstructed;
    const int movesBefore = Tracked::moveConstructed;

    auto it = map.begin();
    ASSERT_NE(it, map.end());

    Tracked& reference = it->second;
    EXPECT_EQ(reference.value, "hello");

    EXPECT_EQ(Tracked::copyConstructed, copiesBefore);
    EXPECT_EQ(Tracked::moveConstructed, movesBefore);
}

TEST_F(HamtLifetime, MultipleStoredObjectsAreAllDestroyed)
{
    {
        Map map;
        map.emplace(1, Tracked{ "one" });
        map.emplace(2, Tracked{ "two" });
        map.emplace(3, Tracked{ "three" });

        EXPECT_EQ(Tracked::alive, 3);
    }

    EXPECT_EQ(Tracked::alive, 0);
    EXPECT_EQ(Tracked::destroyed, Tracked::totalConstructions());
}

TEST_F(HamtLifetime, RvalueInsertionShouldPreferMovesOverCopies)
{
    Map map;

    Tracked value{ "hello" };
    const int copiesBefore = Tracked::copyConstructed;
    const int movesBefore = Tracked::moveConstructed;

    auto [it, inserted] = map.emplace(1, std::move(value));

    ASSERT_TRUE(inserted);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(Tracked::copyConstructed, copiesBefore);
    EXPECT_GT(Tracked::moveConstructed, movesBefore);
}
} // namespace
