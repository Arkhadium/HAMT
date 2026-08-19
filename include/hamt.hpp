#ifndef HAMT_HAMT_H
#define HAMT_HAMT_H

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

constexpr unsigned char k_hashSizeNode = 5;
constexpr unsigned char k_hashSizeRoot = 5;
constexpr unsigned char k_maxLevel =
    static_cast<unsigned char>((std::numeric_limits<std::size_t>::digits - 1) / k_hashSizeNode);

using Index = std::uint8_t;
using Base = std::uint32_t;

constexpr std::string_view k_outOfRangeMessage{ "hamt::Hamt::at: key not found" };

namespace hamt
{
constexpr Index getTheoricalIndex(std::size_t hashCode, unsigned level = 0) noexcept
{
    constexpr unsigned hashSize = std::numeric_limits<std::size_t>::digits;
    constexpr std::size_t mask = (std::size_t{ 1 } << k_hashSizeNode) - 1;

    const unsigned shift = level * k_hashSizeNode;

    if (shift >= hashSize)
        return 0;

    return static_cast<Index>((hashCode >> shift) & mask);
}

constexpr bool isPresent(Index idx, Base base) noexcept
{
    return (base & (Base{ 1 } << idx)) != 0;
}

constexpr Index getRealIndex(Base nodesPresent, Index theoreticalIndex) noexcept
{
    const Base bitsBefore = nodesPresent & ((Base{ 1 } << theoreticalIndex) - Base{ 1 });
    return static_cast<Index>(std::popcount(bitsBefore));
}

template <typename K, typename V, typename Hasher = std::hash<K>, typename EquilityComparator = std::equal_to<K>>
class Hamt
{
public:
    using element_type = V;
    using key_type = K;
    using value_type = std::pair<const key_type, element_type>;
    using size_type = std::size_t;
    using hash_type = Hasher;
    using equal_type = EquilityComparator;

    struct const_iterator;

    struct iterator
    {
        using value_type = Hamt::value_type;

        iterator() = default;

        value_type& operator*() const
        {
            return *m_value;
        }

        value_type* operator->() const
        {
            return m_value;
        }

        iterator& operator++()
        {
            m_value = m_owner->nextValue(m_value);
            return *this;
        }

        iterator operator++(int)
        {
            iterator old = *this;
            ++(*this);
            return old;
        }

        bool operator==(const iterator& other) const
        {
            return m_owner == other.m_owner && m_value == other.m_value;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

    private:
        iterator(Hamt* owner, value_type* value)
            : m_owner(owner)
            , m_value(value)
        {}

        Hamt* m_owner = nullptr;
        value_type* m_value = nullptr;

        friend class Hamt;
        friend struct const_iterator;
    };

    struct const_iterator
    {
        using value_type = Hamt::value_type;

        const_iterator() = default;

        const_iterator(const iterator& other)
            : m_owner(other.m_owner)
            , m_value(other.m_value)
        {}

        const value_type& operator*() const
        {
            return *m_value;
        }

        const value_type* operator->() const
        {
            return m_value;
        }

        const_iterator& operator++()
        {
            m_value = m_owner->nextValue(m_value);
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator old = *this;
            ++(*this);
            return old;
        }

        bool operator==(const const_iterator& other) const
        {
            return m_owner == other.m_owner && m_value == other.m_value;
        }

        bool operator!=(const const_iterator& other) const
        {
            return !(*this == other);
        }

    private:
        const_iterator(const Hamt* owner, const value_type* value)
            : m_owner(owner)
            , m_value(value)
        {}

        const Hamt* m_owner = nullptr;
        const value_type* m_value = nullptr;

        friend class Hamt;
    };

private:
    class Node
    {
    public:
        Node() = default;

        Node(const Node& other)
            : m_nodesPresent(other.m_nodesPresent)
        {
            if (other.m_value)
                m_value.emplace(*other.m_value);

            m_collisions.reserve(other.m_collisions.size());
            for (const auto& value : other.m_collisions)
                m_collisions.push_back(std::make_unique<value_type>(*value));

            const size_type count = std::popcount(m_nodesPresent);
            if (count != 0)
            {
                m_base = new Node[count];
                for (size_type i = 0; i < count; ++i)
                    m_base[i] = other.m_base[i];
            }
        }

        Node(Node&& other) noexcept
            : m_base(std::exchange(other.m_base, nullptr))
            , m_nodesPresent(std::exchange(other.m_nodesPresent, 0))
            , m_collisions(std::move(other.m_collisions))
        {
            if (other.m_value)
            {
                m_value.emplace(std::move(*other.m_value));
                other.m_value.reset();
            }
        }

        Node& operator=(const Node& other)
        {
            if (this == &other)
                return *this;

            clear();
            m_nodesPresent = other.m_nodesPresent;
            if (other.m_value)
                m_value.emplace(*other.m_value);

            m_collisions.reserve(other.m_collisions.size());
            for (const auto& value : other.m_collisions)
                m_collisions.push_back(std::make_unique<value_type>(*value));

            const size_type count = std::popcount(m_nodesPresent);
            if (count != 0)
            {
                m_base = new Node[count];
                for (size_type i = 0; i < count; ++i)
                    m_base[i] = other.m_base[i];
            }

            return *this;
        }

        Node& operator=(Node&& other) noexcept
        {
            if (this == &other)
                return *this;

            clear();
            m_base = std::exchange(other.m_base, nullptr);
            m_nodesPresent = std::exchange(other.m_nodesPresent, 0);
            m_collisions = std::move(other.m_collisions);

            if (other.m_value)
            {
                m_value.emplace(std::move(*other.m_value));
                other.m_value.reset();
            }
            return *this;
        }

        ~Node()
        {
            delete[] m_base;
        }

        bool isLeaf() const noexcept
        {
            return m_value.has_value();
        }

        bool empty() const noexcept
        {
            return !m_value && m_nodesPresent == 0;
        }

        Node& insertChild(Index theoreticalIndex)
        {
            const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
            const size_type oldSize = std::popcount(m_nodesPresent);
            Node* newBase = new Node[oldSize + 1];

            size_type oldIndex = 0;
            for (size_type newIndex = 0; newIndex < oldSize + 1; ++newIndex)
            {
                if (newIndex == realIndex)
                    continue;
                newBase[newIndex] = std::move(m_base[oldIndex++]);
            }

            delete[] m_base;
            m_base = newBase;
            m_nodesPresent |= Base{ 1 } << theoreticalIndex;
            return m_base[realIndex];
        }

        void removeChild(Index theoreticalIndex)
        {
            const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
            const size_type oldSize = std::popcount(m_nodesPresent);

            if (oldSize == 1)
            {
                delete[] m_base;
                m_base = nullptr;
                m_nodesPresent = 0;
                return;
            }

            Node* newBase = new Node[oldSize - 1];
            size_type newIndex = 0;
            for (size_type oldIndex = 0; oldIndex < oldSize; ++oldIndex)
            {
                if (oldIndex == realIndex)
                    continue;
                newBase[newIndex++] = std::move(m_base[oldIndex]);
            }

            delete[] m_base;
            m_base = newBase;
            m_nodesPresent &= ~(Base{ 1 } << theoreticalIndex);
        }

        void clear() noexcept
        {
            delete[] m_base;
            m_base = nullptr;
            m_nodesPresent = 0;
            m_value.reset();
            m_collisions.clear();
        }

        Node* m_base = nullptr;
        Base m_nodesPresent = 0;
        std::optional<value_type> m_value;
        std::vector<std::unique_ptr<value_type>> m_collisions;
    };

public:
    Hamt() = default;

    explicit Hamt(const hash_type& hasher, const equal_type& equal = equal_type{})
        : m_hasher(hasher)
        , m_equal(equal)
    {}

    Hamt(const Hamt& other)
        : m_nodesPresent(other.m_nodesPresent)
        , m_size(other.m_size)
        , m_hasher(other.m_hasher)
        , m_equal(other.m_equal)
    {
        const size_type count = std::popcount(m_nodesPresent);
        if (count != 0)
        {
            m_base = new Node[count];
            for (size_type i = 0; i < count; ++i)
                m_base[i] = other.m_base[i];
        }
    }

    Hamt& operator=(const Hamt& other)
    {
        if (this == &other)
            return *this;

        Hamt copy(other);
        swap(copy);
        return *this;
    }

    Hamt(Hamt&& other) noexcept
        : m_base(std::exchange(other.m_base, nullptr))
        , m_nodesPresent(std::exchange(other.m_nodesPresent, 0))
        , m_size(std::exchange(other.m_size, 0))
        , m_hasher(std::move(other.m_hasher))
        , m_equal(std::move(other.m_equal))
    {}

    Hamt& operator=(Hamt&& other) noexcept
    {
        if (this == &other)
            return *this;

        delete[] m_base;
        m_base = std::exchange(other.m_base, nullptr);
        m_nodesPresent = std::exchange(other.m_nodesPresent, 0);
        m_size = std::exchange(other.m_size, 0);
        m_hasher = std::move(other.m_hasher);
        m_equal = std::move(other.m_equal);
        return *this;
    }

    ~Hamt()
    {
        delete[] m_base;
    }

    void swap(Hamt& other) noexcept
    {
        using std::swap;
        swap(m_base, other.m_base);
        swap(m_nodesPresent, other.m_nodesPresent);
        swap(m_size, other.m_size);
        swap(m_hasher, other.m_hasher);
        swap(m_equal, other.m_equal);
    }

    size_type size() const noexcept
    {
        return m_size;
    }

    bool empty() const noexcept
    {
        return m_size == 0;
    }

    template <typename KeyLike>
    element_type& at(const KeyLike& key_like)
    {
        auto it = find(key_like);
        if (it == end())
            throw std::out_of_range(k_outOfRangeMessage);
        return it->second;
    }

    template <typename KeyLike>
    const element_type& at(const KeyLike& key_like) const
    {
        auto it = find(key_like);
        if (it == end())
            throw std::out_of_range(k_outOfRangeMessage);
        return it->second;
    }

    template <typename KeyLike>
    requires std::constructible_from<key_type, const KeyLike&> && std::default_initializable<element_type>
    element_type& operator[](const KeyLike& key_like)
    {
        if (auto it = find(key_like); it != end())
            return it->second;

        auto [it, inserted] = emplace(key_type(key_like), element_type{});
        return it->second;
    }

    template <typename KeyLike>
    const element_type& operator[](const KeyLike& key_like) const
    {
        return at(key_like);
    }

    template <typename... Args>
    requires std::constructible_from<value_type, Args...>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        value_type value(std::forward<Args>(args)...);
        const std::size_t hashCode = m_hasher(value.first);
        auto [ptr, inserted] = insertRoot(std::move(value), hashCode);
        if (inserted)
            ++m_size;
        return { iterator(this, ptr), inserted };
    }

    template <typename... Args>
    requires std::constructible_from<element_type, Args...>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
    {
        if (auto it = find(key); it != end())
            return { it, false };

        // TODO optimiser
        return emplace(std::piecewise_construct,
                       std::forward_as_tuple(key),
                       std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <typename... Args>
    requires std::constructible_from<element_type, Args...>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args)
    {
        if (auto it = find(key); it != end())
            return { it, false };

        // TODO optimiser
        return emplace(std::piecewise_construct,
                       std::forward_as_tuple(std::move(key)),
                       std::forward_as_tuple(std::forward<Args>(args)...));
    }

    size_type erase(iterator position)
    {
        if (position.m_owner != this || position == end())
            return 0;

        key_type key = position->first;
        return erase(key);
    }

    template <typename KeyLike>
    size_type erase(const KeyLike& key_like)
    {
        if (!m_base)
            return 0;

        const std::size_t hashCode = m_hasher(key_like);
        const Index theoreticalIndex = getTheoricalIndex(hashCode, 0);

        if (!isPresent(theoreticalIndex, m_nodesPresent))
            return 0;

        const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
        const bool removed = eraseNode(m_base[realIndex], key_like, hashCode, 1);

        if (!removed)
            return 0;

        --m_size;
        if (m_base[realIndex].empty())
            removeRootChild(theoreticalIndex);

        return 1;
    }

    template <typename KeyLike>
    iterator find(const KeyLike& key_like)
    {
        if (!m_base)
            return end();

        const std::size_t hashCode = m_hasher(key_like);
        const Index theoreticalIndex = getTheoricalIndex(hashCode, 0);

        if (!isPresent(theoreticalIndex, m_nodesPresent))
            return end();

        const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
        return iterator(this, findNode(m_base[realIndex], key_like, hashCode, 1));
    }

    template <typename KeyLike>
    const_iterator find(const KeyLike& key_like) const
    {
        if (!m_base)
            return cend();

        const std::size_t hashCode = m_hasher(key_like);
        const Index theoreticalIndex = getTheoricalIndex(hashCode, 0);

        if (!isPresent(theoreticalIndex, m_nodesPresent))
            return cend();

        const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
        return const_iterator(this, findNode(m_base[realIndex], key_like, hashCode, 1));
    }

    iterator begin()
    {
        return iterator(this, nextValue(nullptr));
    }

    const_iterator begin() const
    {
        return cbegin();
    }

    const_iterator cbegin() const
    {
        return const_iterator(this, nextValue(nullptr));
    }

    iterator end()
    {
        return iterator(this, nullptr);
    }

    const_iterator end() const
    {
        return cend();
    }

    const_iterator cend() const
    {
        return const_iterator(this, nullptr);
    }

private:
    Node& insertRootChild(Index theoreticalIndex)
    {
        const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
        const size_type oldSize = std::popcount(m_nodesPresent);
        Node* newBase = new Node[oldSize + 1];

        size_type oldIndex = 0;
        for (size_type newIndex = 0; newIndex < oldSize + 1; ++newIndex)
        {
            if (newIndex == realIndex)
                continue;

            newBase[newIndex] = std::move(m_base[oldIndex++]);
        }

        delete[] m_base;
        m_base = newBase;
        m_nodesPresent |= Base{ 1 } << theoreticalIndex;
        return m_base[realIndex];
    }

    void removeRootChild(Index theoreticalIndex)
    {
        const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
        const size_type oldSize = std::popcount(m_nodesPresent);

        if (oldSize == 1)
        {
            delete[] m_base;
            m_base = nullptr;
            m_nodesPresent = 0;
            return;
        }

        Node* newBase = new Node[oldSize - 1];
        size_type newIndex = 0;
        for (size_type oldIndex = 0; oldIndex < oldSize; ++oldIndex)
        {
            if (oldIndex == realIndex)
                continue;
            newBase[newIndex++] = std::move(m_base[oldIndex]);
        }

        delete[] m_base;
        m_base = newBase;
        m_nodesPresent &= ~(Base{ 1 } << theoreticalIndex);
    }

    std::pair<value_type*, bool> insertRoot(value_type&& value, std::size_t hashCode)
    {
        const Index theoreticalIndex = getTheoricalIndex(hashCode, 0);

        if (!isPresent(theoreticalIndex, m_nodesPresent))
        {
            Node& node = insertRootChild(theoreticalIndex);
            node.m_value.emplace(std::move(value));
            return { &*node.m_value, true };
        }

        const Index realIndex = getRealIndex(m_nodesPresent, theoreticalIndex);
        return insertNode(m_base[realIndex], std::move(value), hashCode, 1);
    }

    std::pair<value_type*, bool> insertNode(Node& node, value_type&& value, std::size_t hashCode, unsigned level)
    {
        if (node.m_value)
        {
            if (m_equal(node.m_value->first, value.first))
                return { &*node.m_value, false };

            for (auto& collision : node.m_collisions)
            {
                if (m_equal(collision->first, value.first))
                    return { collision.get(), false };
            }

            if (level > k_maxLevel)
            {
                node.m_collisions.push_back(std::make_unique<value_type>(std::move(value)));
                return { node.m_collisions.back().get(), true };
            }

            value_type oldValue(std::move(*node.m_value));
            const std::size_t oldHash = m_hasher(oldValue.first);
            node.m_value.reset();

            auto oldCollisions = std::move(node.m_collisions);
            node.m_collisions.clear();

            insertNode(node, std::move(oldValue), oldHash, level);
            for (auto& oldCollision : oldCollisions)
            {
                const std::size_t collisionHash = m_hasher(oldCollision->first);
                insertNode(node, std::move(*oldCollision), collisionHash, level);
            }

            return insertNode(node, std::move(value), hashCode, level);
        }

        if (level > k_maxLevel)
        {
            node.m_value.emplace(std::move(value));
            return { &*node.m_value, true };
        }

        const Index theoreticalIndex = getTheoricalIndex(hashCode, level);
        if (!isPresent(theoreticalIndex, node.m_nodesPresent))
        {
            Node& child = node.insertChild(theoreticalIndex);
            child.m_value.emplace(std::move(value));
            return { &*child.m_value, true };
        }

        const Index realIndex = getRealIndex(node.m_nodesPresent, theoreticalIndex);
        return insertNode(node.m_base[realIndex], std::move(value), hashCode, level + 1);
    }

    template <typename KeyLike>
    value_type* findNode(Node& node, const KeyLike& key_like, std::size_t hashCode, unsigned level)
    {
        if (node.m_value)
        {
            if (m_equal(node.m_value->first, key_like))
                return &*node.m_value;

            for (auto& collision : node.m_collisions)
            {
                if (m_equal(collision->first, key_like))
                    return collision.get();
            }
            return nullptr;
        }

        if (level > k_maxLevel)
            return nullptr;

        const Index theoreticalIndex = getTheoricalIndex(hashCode, level);
        if (!isPresent(theoreticalIndex, node.m_nodesPresent))
            return nullptr;

        const Index realIndex = getRealIndex(node.m_nodesPresent, theoreticalIndex);
        return findNode(node.m_base[realIndex], key_like, hashCode, level + 1);
    }

    template <typename KeyLike>
    const value_type* findNode(const Node& node, const KeyLike& key_like, std::size_t hashCode, unsigned level) const
    {
        if (node.m_value)
        {
            if (m_equal(node.m_value->first, key_like))
                return &*node.m_value;

            for (const auto& collision : node.m_collisions)
            {
                if (m_equal(collision->first, key_like))
                    return collision.get();
            }
            return nullptr;
        }

        if (level > k_maxLevel)
            return nullptr;

        const Index theoreticalIndex = getTheoricalIndex(hashCode, level);
        if (!isPresent(theoreticalIndex, node.m_nodesPresent))
            return nullptr;

        const Index realIndex = getRealIndex(node.m_nodesPresent, theoreticalIndex);
        return findNode(node.m_base[realIndex], key_like, hashCode, level + 1);
    }

    template <typename KeyLike>
    bool eraseNode(Node& node, const KeyLike& key_like, std::size_t hashCode, unsigned level)
    {
        if (node.m_value)
        {
            if (m_equal(node.m_value->first, key_like))
            {
                if (!node.m_collisions.empty())
                {
                    auto replacement = std::move(node.m_collisions.back());
                    node.m_collisions.pop_back();
                    node.m_value.reset();
                    node.m_value.emplace(std::move(*replacement));
                }
                else
                {
                    node.m_value.reset();
                }
                return true;
            }

            for (size_type i = 0; i < node.m_collisions.size(); ++i)
            {
                if (m_equal(node.m_collisions[i]->first, key_like))
                {
                    node.m_collisions[i] = std::move(node.m_collisions.back());
                    node.m_collisions.pop_back();
                    return true;
                }
            }
            return false;
        }

        if (level > k_maxLevel)
            return false;

        const Index theoreticalIndex = getTheoricalIndex(hashCode, level);
        if (!isPresent(theoreticalIndex, node.m_nodesPresent))
            return false;

        const Index realIndex = getRealIndex(node.m_nodesPresent, theoreticalIndex);
        const bool removed = eraseNode(node.m_base[realIndex], key_like, hashCode, level + 1);

        if (removed && node.m_base[realIndex].empty())
            node.removeChild(theoreticalIndex);

        return removed;
    }

    static value_type* nextValueInNodes(Node* nodes, size_type count, value_type* current, bool& currentSeen)
    {
        if (!nodes)
            return nullptr;

        for (size_type i = 0; i < count; ++i)
        {
            Node& node = nodes[i];

            if (node.m_value)
            {
                value_type* value = &*node.m_value;
                if (currentSeen)
                    return value;

                if (value == current)
                    currentSeen = true;

                for (auto& collision : node.m_collisions)
                {
                    value = collision.get();
                    if (currentSeen)
                        return value;
                    if (value == current)
                        currentSeen = true;
                }
            }

            if (node.m_base)
            {
                const size_type childCount = std::popcount(node.m_nodesPresent);
                if (auto* result = nextValueInNodes(node.m_base, childCount, current, currentSeen))
                    return result;
            }
        }
        return nullptr;
    }

    static const value_type*
    nextValueInNodes(const Node* nodes, size_type count, const value_type* current, bool& currentSeen)
    {
        if (!nodes)
            return nullptr;

        for (size_type i = 0; i < count; ++i)
        {
            const Node& node = nodes[i];

            if (node.m_value)
            {
                const value_type* value = &*node.m_value;
                if (currentSeen)
                    return value;

                if (value == current)
                    currentSeen = true;

                for (const auto& collision : node.m_collisions)
                {
                    value = collision.get();
                    if (currentSeen)
                        return value;
                    if (value == current)
                        currentSeen = true;
                }
            }

            if (node.m_base)
            {
                const size_type childCount = std::popcount(node.m_nodesPresent);
                if (auto* result = nextValueInNodes(node.m_base, childCount, current, currentSeen))
                    return result;
            }
        }
        return nullptr;
    }

    value_type* nextValue(value_type* current)
    {
        bool currentSeen = (current == nullptr);
        return nextValueInNodes(m_base, std::popcount(m_nodesPresent), current, currentSeen);
    }

    const value_type* nextValue(const value_type* current) const
    {
        bool currentSeen = (current == nullptr);
        return nextValueInNodes(m_base, std::popcount(m_nodesPresent), current, currentSeen);
    }

    Node* m_base = nullptr;
    Base m_nodesPresent = 0;
    size_type m_size = 0;
    [[no_unique_address]] hash_type m_hasher{};
    [[no_unique_address]] equal_type m_equal{};
};

template <typename K, typename V, typename H, typename E>
void swap(Hamt<K, V, H, E>& lhs, Hamt<K, V, H, E>& rhs) noexcept
{
    lhs.swap(rhs);
}
} // namespace hamt

#endif // HAMT_HAMT_H