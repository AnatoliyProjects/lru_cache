// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Cache traits and constants.

#ifndef LRU_CACHE_TRAITS_H
#define LRU_CACHE_TRAITS_H

#include <limits>
#include <list>
#include <mutex>
#include <unordered_map>

namespace lru {
    // ========================================================================
    // Constants
    // ========================================================================

    // Maximum size_t value.
    // It may be passed as a maxsize or maxmem argument to the cache ctor.
    // In this case, the corresponding cache limit would not be set.
    constexpr size_t nval = std::numeric_limits<size_t>::max();

    // ========================================================================
    // Cache traits
    // ========================================================================

    // Cache traits. Defines core types and constants.
    // The client has access only to part of the cache fields representing these traits.
    // However, the client must know the underlying implementation for complexity assumptions.
    // In addition, the client may want to provide the custom CacheTraits specialization for
    // particular keys/values. Thus, all traits are part of the public API.
    template<
        typename Key,
        typename Value,
        typename Hash = std::hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<std::pair<const Key, Value> > >
    struct CacheTraits {
        // --------------------------------------------------------------------
        // Template arguments
        // --------------------------------------------------------------------

        // Item key.
        using KeyParam = const Key;

        // Item value.
        using ValueParam = Value;

        // Hash function.
        using HashParam = Hash;

        // Key equality comparison.
        using KeyEqualParam = KeyEqual;

        // Cache buffer allocator.
        using AllocatorParam = Allocator;

        // --------------------------------------------------------------------
        // Deduced types and constants
        // --------------------------------------------------------------------

        // Cached item.
        using Item = std::pair<const Key, Value>;

        // Functor returns the size of a key dynamic buffer (if present).
        using KeyMem = std::function<size_t(const Key &)>;

        // Functor returns the size of a value dynamic buffer (if present).
        using ValueMem = std::function<size_t(const Value &)>;

        // Cache buffer for storing items (client has no forward access).
        using Buf = std::list<Item, Allocator>;

        // Cache buffer iterator.
        using Iter = typename Buf::iterator;

        // Const cache buffer iterator.
        using ConstIter = typename Buf::const_iterator;

        // Reverse cache buffer iterator.
        using ReverseIter = typename Buf::reverse_iterator;

        // Const reverse cache buffer iterator.
        using ConstReverseIter = typename Buf::const_reverse_iterator;

        // Cache table for item lookup (client has no forward access).
        using Table = std::unordered_map<Key, Iter, Hash, KeyEqual>;

        // Mutex for SafeCache (client has no forward access).
        using Mutex = std::recursive_mutex;

        // Lock for SafeCache mutex (client has no forward access).
        using Lock = std::unique_lock<Mutex>;

        // Guard for SafeCache mutex (client has no forward access).
        using Guard = std::lock_guard<Mutex>;

        // Approximate memory usage for caching the one item.
        // The actual memory usage for non-POD/complex types may differ significantly.
        // For example, if the desired type contains a pointer to a dynamic buffer,
        // we will consider only the pointer size, not a buffer.
        // To handle such cases, you may provide the appropriate hint function
        // to the Cache/SafeCache ctor (see ctor comments for details).
        static constexpr size_t kItemMem = sizeof(typename Buf::value_type)
                                           + sizeof(typename Table::value_type);
    };
}

#endif // LRU_CACHE_TRAITS_H
