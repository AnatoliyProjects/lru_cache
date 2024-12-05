// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Non thread-safe LRU cache.
// For thread-safe implementation see lru_cache/thread_safe_cache.h

#ifndef LRU_CACHE_CACHE_H
#define LRU_CACHE_CACHE_H

#include <algorithm>
#include <format>
#include <optional>
#include <ostream>
#include <string>

#include "lru_cache/detail/utils.h"
#include "lru_cache/serde.h"
#include "lru_cache/stats.h"
#include "lru_cache/traits.h"

namespace lru {
    // Non thread-safe LRU cache (see also lru_cache/thread_safe_cache.h).
    // The interface mimics the Memcached text protocol (where it makes sense).
    // We support the following Memcached-like commands:
    // - set, add, replace, get, delete, stats, flush (the last one equal to clear)
    // The following Memcached analogous are not provided:
    // - append/prepend/incr/decr (this cache is typed, just modify the value)
    // - cas/gets (if you need synchronization, use SafeCache)
    // - stats items/slabs/sizes (send cache to stream; it prints the content)
    // Also, our implementation provides some extended functionality:
    // - cache serialization/deserialization (via dump and load commands)
    // - limiting of size/memory during execution (via maxsize/maxmem commands)
    // - item iteration (pointer dereferencing doesn't touch LRU)
    // - printing of cache content to the std::ostream (feature for debugging)
    template<
        typename Key,
        typename Value,
        typename Hash = std::hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<std::pair<const Key, Value> > >
    class Cache {
    public:
        // Cache traits.
        using Traits = CacheTraits<Key, Value, Hash, KeyEqual, Allocator>;
        using Item = typename Traits::Item;
        using Buf = typename Traits::Buf;
        using Iter = typename Traits::Iter;
        using ConstIter = typename Traits::ConstIter;
        using ReverseIter = typename Traits::ReverseIter;
        using ConstReverseIter = typename Traits::ConstReverseIter;
        using Table = typename Traits::Table;
        static constexpr size_t kItemMem = Traits::kItemMem;

        // Writes a string representation of the cache to the stream.
        template<typename K, typename V>
        friend std::ostream &operator<<(std::ostream &, const Cache<K, V> &);

        // Returns true if cache items and their LRU order are equal.
        // Non-optimized implementation. Use only for debugging/testing.
        bool operator ==(const Cache &other) const {
            return buf_ == other.buf_;
        }

        // Returns true if cache items or their LRU order are not equal.
        // Non-optimized implementation. Use only for debugging/testing.
        bool operator !=(const Cache &other) const {
            return buf_ != other.buf_;
        }

        // Creates a new cache.
        // If maxsize and maxmem are not specified, the cache turns to the unbounded cache.
        // However, the performance of such an unbounded cache will not be ideal because of LRU.
        explicit Cache(const size_t maxsize = nval,
                       const size_t maxmem = nval): stats_{
            0, 0, maxsize, 0, maxmem, 0
        } {
        }

        // Returns an iterator to the beginning of the cache buffer.
        Iter begin() { return buf_.begin(); }

        // Returns a const iterator to the beginning of the cache buffer.
        ConstIter begin() const { return buf_.begin(); }

        // Returns an iterator to the end of the cache buffer.
        Iter end() { return buf_.end(); }

        // Returns a const iterator to the end of the cache buffer.
        ConstIter end() const { return buf_.end(); }

        // Returns a reverse iterator to the beginning of the cache buffer.
        ReverseIter rbegin() { return buf_.rbegin(); }

        // Returns a const reverse iterator to the beginning of the cache buffer.
        ConstReverseIter rbegin() const { return buf_.rbegin(); }

        // Returns a reverse iterator to the end of the cache buffer.
        ReverseIter rend() { return buf_.rend(); }

        // Returns a const reverse iterator to the end of the cache buffer.
        ConstReverseIter rend() const { return buf_.rend(); }

        // Stores data, possibly overwriting any existing data.
        // New items are at the top of the LRU.
        template<typename V>
        void Set(const Key &key, V &&value) {
            auto it = table_.find(key);
            if (it == table_.end()) {
                // missed
                Push(key, std::forward<V>(value));
            } else {
                // exists
                it->second->second = std::forward<V>(value);
                Touch(it->second);
            }
        }

        // Stores data, only if it does not already exist.
        // New items are at the top of the LRU. If an item already exists and an add fails,
        // it promotes the item to the front of the LRU anyway.
        // Returns true if addition succeeded.
        template<typename V>
        bool Add(const Key &key, V &&value) {
            auto it = table_.find(key);
            if (it == table_.end()) {
                // missed
                Push(key, std::forward<V>(value));
                return true;
            }
            // exists
            Touch(it->second);
            return false;
        }

        // Stores this data, but only if the data already exists.
        // Returns true if replacement succeeded.
        template<typename V>
        bool Replace(const Key &key, V &&value) {
            auto it = table_.find(key);
            // missed
            if (it == table_.end()) { return false; }
            // exists
            it->second->second = std::forward<V>(value);
            Touch(it->second);
            return true;
        }

        // Retrieves data by the key.
        // Returns empty optional if an item was not found.
        std::optional<std::reference_wrapper<Value> > Get(const Key &key) {
            auto it = table_.find(key);
            if (it == table_.end()) {
                stats_.misses++;
                return {};
            }
            // exists
            Touch(it->second);
            stats_.hits++;
            return it->second->second;
        }

        // Removes an item from the cache, if it exists.
        // Returns true if deletion succeeded.
        bool Delete(const Key &key) {
            auto it = table_.find(key);
            if (it == table_.end()) { return false; }
            // exists
            buf_.erase(it->second);
            table_.erase(it);
            stats_.currsize--;
            stats_.currmem -= kItemMem;
            return true;
        }

        // Clears the cache without touching hits/misses statistics.
        void Flush() {
            buf_.clear();
            table_.clear();
            stats_.currsize = 0;
            stats_.currmem = 0;
        }

        // Returns current item count.
        [[nodiscard]] size_t Size() const { return table_.size(); }

        // Returns current memory usage.
        [[nodiscard]] size_t Memory() const { return table_.size() * kItemMem; }

        // Returns upper limit of item count.
        [[nodiscard]] size_t Maxsize() const { return stats_.maxsize; }

        // Returns upper limit of memory usage.
        [[nodiscard]] size_t Maxmem() const { return stats_.maxmem; }

        // Limits maximum item count.
        // It also shrinks the cache to the new limit (if needed).
        void Maxsize(size_t items) {
            auto start = std::next(buf_.cbegin(),
                                   std::min(items, buf_.size()));
            auto stop = buf_.cend();
            std::for_each(start, stop,
                          [this](const auto &item) { table_.erase(item.first); }
            );
            buf_.erase(start, stop);
            stats_.maxsize = items;
            stats_.currsize = Size();
            // stats_.maxmem unchanged
            stats_.currmem = Memory();
        }

        // Limits maximum memory usage.
        // It also shrinks the cache to the new limit (if needed).
        void Maxmem(const size_t bytes) {
            size_t currmem = Memory();
            while (currmem > bytes) {
                Pop();
                currmem = Memory();
            }
            // stats_.maxsize unchanged
            stats_.currsize = Size();
            stats_.maxmem = bytes;
            stats_.currmem = Memory();
        }

        // Returns cache statistics.
        [[nodiscard]] CacheInfo Stats() const { return stats_; }

        // Serializes cached items to the user-provided output iterator.
        // Single-pass iterators are also suitable. But don't use std::ostream_iterator
        // because it performs formatting and corrupts the binary data.
        // Use std::ostreambuf_iterator instead.
        template<serde::output_byte_iterator Iter>
        void Dump(Iter it) const {
            // We load deserialized items with Cache.Set() which affects LRU,
            // so at serialization stage items go in the revert order.
            using iter = serde::aux::SerializingIterator<Traits>;
            auto first = iter(buf_.rbegin());
            auto last = iter(buf_.rend());
            std::for_each(first, last, [&it](auto bytes) {
                std::copy(bytes.begin(), bytes.end(), it);
            });
        }

        // Serializes cached items to the user-provided container.
        template<serde::byte_sequence_container Container>
        void Dump(Container &c) const {
            // We load deserialized items with Cache.Set() which affects LRU,
            // so at serialization stage items go in the revert order.
            using iter = serde::aux::SerializingIterator<Traits>;
            auto first = iter(buf_.rbegin());
            auto last = iter(buf_.rend());
            std::for_each(first, last, [&c](auto bytes) {
                // Here we choose the most optimal way to insert row bytes.
                if constexpr (aux::HasInsertMethodV<Container, decltype(first
                )>) {
                    c.insert(c.end(), bytes.begin(), bytes.end());
                } else {
                    std::copy(bytes.begin(), bytes.end(),
                              std::back_inserter(c));
                }
            });
        }

        // Deserializes cached items from the user-provided range.
        // Single-pass iterators are also suitable. But don't use std::istream_iterator
        // because it performs formatting and corrupts the binary data.
        // Use std::istreambuf_iterator instead.
        template<serde::input_byte_iterator Iter>
        void Load(Iter first, Iter last) {
            Flush();
            using iter = serde::aux::DeserializingIterator<Traits, Iter>;
            std::for_each(iter(first), iter(last), [this](auto &&item) {
                Set(item.first, std::move(item.second));
            });
        }

        // Deserializes cached items from the user-provided container.
        template<serde::byte_sequence_container Container>
        void Load(const Container &c) {
            Flush();
            using iter = serde::aux::DeserializingIterator<Traits, typename
                Container::const_iterator>;
            auto first = iter(c.begin());
            auto last = iter(c.end());
            std::for_each(first, last, [this](auto &&item) {
                Set(item.first, std::move(item.second));
            });
        }

    private:
        Buf buf_;
        Table table_;
        mutable CacheInfo stats_;

        void Touch(Iter it) { buf_.splice(buf_.cbegin(), buf_, it); }

        template<typename V>
        void Push(const Key &key, V &&value) {
            buf_.emplace_front(key, std::forward<V>(value));
            table_.emplace(key, buf_.begin());
            if (Size() > stats_.maxsize or Memory() > stats_.maxmem) { Pop(); }
            stats_.currsize = Size();
            stats_.currmem = Memory();
        }

        void Pop() {
            if (buf_.empty()) return;
            // not empty
            const Key &last = buf_.back().first;
            table_.erase(last);
            buf_.pop_back();
        }
    };

    // Writes a string representation of the cache to the stream.
    template<typename Key, typename Value>
    std::ostream &operator <<(std::ostream &out,
                              const Cache<Key, Value> &cache) {
        const std::string name = std::format("lru::Cache<Key={}, Value={}>",
                                             typeid(Key).name(),
                                             typeid(Value).name());
        out << name << " at " << &cache << '\n';
        out << cache.Stats() << '\n';
        size_t n = 0;
        for (const auto &item: cache) {
            out << aux::ItemToStr(item, n) << '\n';
            n++;
        }
        return out << std::flush;
    }
}

#endif // LRU_CACHE_CACHE_H
