#ifndef HAMT_HAMT_H
#define HAMT_HAMT_H

#include <bit>
#include <concepts>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdint.h>
#include <utility>

constexpr unsigned char k_hash_size_node = 5;
constexpr unsigned char k_hash_size_root = 5;
constexpr unsigned char k_max_level = 5;
using Index = uint8_t;
using Base = uint32_t;

namespace hamt
{
constexpr Index getTheoricalIndex(std::size_t hashCode, unsigned level = 0) noexcept
{
    constexpr unsigned hashSize = std::numeric_limits<std::size_t>::digits;

    const unsigned bitsAlreadyUsed = k_hash_size_root + level * k_hash_size_node;

    const unsigned shift = hashSize - bitsAlreadyUsed;

    constexpr std::size_t mask = (std::size_t{ 1 } << k_hash_size_node) - 1;

    return static_cast<Index>((hashCode >> shift) & mask);
}

bool isPresent(Index idx, Base base)
{
    uint32_t mask = 1 << idx;
    return base & mask;
}

constexpr Index getRealIndex(Base nodesPresent, Index theoreticalIndex) noexcept
{
    const Base bitsBefore = nodesPresent & ((Base{ 1 } << theoreticalIndex) - Base{ 1 });

    return static_cast<Index>(std::popcount(bitsBefore));
}

template <class Node>
Node* reallocBase(Index insertionIndex, Node* oldBase, std::size_t size)
{
    Node* newBase = new Node[size + 1];
    std::size_t j = 0;
    for (std::size_t i = 0; i < size; ++i, ++j)
    {
        if (j == insertionIndex)
        {
            ++j;
        }
        newBase[j] = oldBase[i];
    }
    if (oldBase)
        delete[] oldBase;
    return newBase;
}

template <typename A, typename B>
concept hash_compatible = requires(const A& a, const B& b) {
    { std::hash<A>()(a) } -> std::same_as<decltype(std::hash<B>()(b))>;
    { std::equal_to<A>()(b) } -> std::same_as<bool>;
};

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

    struct iterator
    {}; // to be defined

    struct const_iterator
    {}; // to be defined

private:
    // template <K, V, Hasher, EquilityComparator>
    class Node
    {
    public:
        Node& operator=(const Node& other)
        {
            if (this == &other)
                return *this;
            if (m_base)
                delete[] m_base;

            m_base = other.m_base;
            m_nodesPresent = other.m_nodesPresent;
            m_value.reset();
            if (other.m_value)
            {
                m_value.emplace(*other.m_value);
            }
            return *this;
        };

        Node& updateBase(Index insertionIndex)
        {
            auto size = std::popcount(m_nodesPresent);
            m_base = reallocBase(insertionIndex, m_base, size);
            return m_base[insertionIndex];
        }

        std::pair<iterator, bool> insert(value_type&& val, std::size_t hashCode, int level)
        {
            auto index = getTheoricalIndex(hashCode, level);

            if (isPresent(index, m_nodesPresent))
            {
                if (level == k_max_level)
                {
                    std::cout << val.first << " not added" << std::endl;
                    return { iterator{}, false };
                }

                if (std::popcount(m_nodesPresent) == 1)
                {
                    if (*m_value == val)
                    {
                        return { iterator{}, false };
                    }
                    else
                    {
                        Node* m_base = new Node[2];
                        auto currentValueIdx = getTheoricalIndex(hash_type{}(m_value->first), level);

                        m_nodesPresent |= Base{ 1 } << index;

                        if (currentValueIdx < index)
                        {
                            m_base[0].m_value.emplace(std::move(*m_value));
                            return m_base[1].insert(std::move(val), hashCode, level);
                        }
                        else
                        {
                            m_base[1].m_value.emplace(std::move(*m_value));
                            return m_base[0].insert(std::move(val), hashCode, level);
                        }
                    }
                }
                auto realIndex = getRealIndex(m_nodesPresent, index);
                auto& nextNode = m_base[realIndex];

                return nextNode.insert(std::move(val), hashCode, level + 1);
            }
            else
            {
                auto realIndex = getRealIndex(m_nodesPresent, index);
                auto& nextNode = updateBase(realIndex);
                std::cout << val.first << " added" << std::endl;
                nextNode.m_value.emplace(std::move(val));

                return { iterator{}, true };
            }
        }

        Node* m_base = nullptr;
        Base m_nodesPresent = 0;
        std::optional<value_type> m_value;
    };

public:
    Hamt()
        : m_base(nullptr)
    {}

    Hamt(const Hamt&);
    Hamt& operator=(const Hamt&);
    Hamt(Hamt&&) noexcept;
    Hamt& operator=(Hamt&&) noexcept;

    ~Hamt() {}

    void swap(Hamt&) noexcept;

    size_type size() const noexcept;

    //! throws if key_like does not exist
    element_type& at(const hash_compatible<key_type> auto& key_like);
    const element_type& at(const hash_compatible<key_type> auto& key_like) const;

    //§ create element if key_like does not exist (default construction)
    element_type& operator[](const hash_compatible<key_type> auto& key_like);
    const element_type& operator[](const hash_compatible<key_type> auto& key_like) const;

    template <typename... Args>
    requires std::constructible_from<value_type, Args...>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        value_type val(std::forward<Args>(args)...);

        auto hashCode = hash_type{}(val.first);

        auto index = getTheoricalIndex(hashCode);

        if (isPresent(index, m_nodesPresent))
        {
            if (std::popcount(m_nodesPresent) == 1)
            {
                auto realIndex = getRealIndex(index, m_nodesPresent);
                if (m_base[realIndex].m_value && val == m_base[realIndex].m_value)
                {
                    std::cout << val.first << " nod added" << std::endl;
                    return { iterator{}, false };
                }
                else
                    return m_base[realIndex].insert(std::move(val), hashCode, 1);
            }
            auto realIndex = getRealIndex(index, m_nodesPresent);
            auto& nextNode = m_base[realIndex];
            m_nodesPresent |= Base{ 1 } << index;
            return nextNode.insert(std::move(val), hashCode, 1);
        }
        else
        {
            auto realIndex = getRealIndex(index, m_nodesPresent);
            auto& newNode = updateBase(realIndex);
            m_nodesPresent |= Base{ 1 } << index;
            std::cout << val.first << " added" << std::endl;
            newNode.m_value.emplace(std::move(val));
            return { iterator{}, true };
        }
    }

    Node& updateBase(Index insertionIndex)
    {
        auto size = std::popcount(m_nodesPresent);
        m_base = reallocBase(insertionIndex, m_base, size);
        return m_base[insertionIndex];
    }

    template <typename... Args>
    requires std::constructible_from<value_type, Args...>
    std::pair<iterator, bool> try_emplace(Args&&...);

    size_type erase(iterator position);
    size_type erase(const hash_compatible<key_type> auto& key_like);

    iterator find(const hash_compatible<key_type> auto& key_like);
    const_iterator find(const hash_compatible<key_type> auto& key_like) const;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;

private:
    Node* m_base;
    Base m_nodesPresent;
};

template <typename K, typename V, typename H, typename E>
inline bool operator==(typename Hamt<K, V, H, E>::iterator lhs, typename Hamt<K, V, H, E>::iterator rhs);

template <typename K, typename V, typename H, typename E>
inline bool operator==(typename Hamt<K, V, H, E>::const_iterator lhs, typename Hamt<K, V, H, E>::const_iterator rhs);
} // namespace hamt

#endif // HAMT_HAMT_H

// template <class Key, class T, class Hash = std::hash<Key>>
// class Hamt
// {
// public:
//     class Iterator
//     {
//     public:
//         Iterator(Node<Key, T>* node)
//             : m_node(node)
//         {}

//     private:
//         Node<Key, T>* m_node;
//     };

//     std::pair<Iterator, bool> insert(const& std::pair<Key, T> value)
//     {
//         auto hash = std::hash<Key>{}(value.first);
//         hash = hash % k_hash_size_root;
//         Index index = uint32_t{ 1 } << hash;

//         if (m_nodesPresent & index)
//         {
//             std::uint32_t bits_before = m_nodesPresent & (mask - 1);
//             auto idx = std::popcount(bits_before);
//             // node.insert
//             // return
//             return {};
//         }
//         else
//         {
//             m_nodesPresent = m_nodesPresent | index;
//             Node* roots = new Node[m_rootSize + 1];
//             std::uint32_t mask = 1u << index;
//             std::uint32_t bits_before = m_nodesPresent & (mask - 1);
//             uint32_t realIndex = std::popcount(bits_before);
//             for (unsigned char i = 0; i < m_rootSize + 1; ++i)
//             {
//                 // copy n first
//                 // add new
//                 // copy n last
//             }
//             return {};
//         }
//     }

//     Iterator erase(const Key&);
//     Iterator find(const Key&);

// private:
//     Node* m_roots;
//     uint32_t m_nodesPresent;

//     unsigned char m_rootSize;
//     uint32_t m_totalSize;
// };