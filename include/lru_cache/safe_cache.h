// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Thread-safe LRU cache.
// For non thread-safe implementation see lru_cache/cache.h

#ifndef LRU_CACHE_SAFE_CACHE_H
#define LRU_CACHE_SAFE_CACHE_H

#include <mutex>
#include <ostream>
#include <string>

#include "lru_cache/detail/utils.h"
#include "lru_cache/cache.h"
#include "lru_cache/guard.h"
#include "lru_cache/traits.h"

namespace lru {
    // Thread-safe LRU cache (see also lru_cache/cache.h).
    // The interface mimics the Memcached text protocol (where it makes sense).
    // We support the following Memcached-like commands:
    // - set, add, replace, get, delete, stats, flush (the last one equal to clear)
    // The following Memcached analogous are not provided:
    // - append/prepend/incr/decr (this cache is typed, just modify the value)
    // - cas/gets (you already have synchronization with SafeCache)
    // - stats items/slabs/sizes (send cache to stream; it prints the content)
    // Also, our implementation provides some extended functionality:
    // - cache serialization/deserialization (via dump and load commands)
    // - limiting of size/memory during execution (via maxsize/maxmem commands)
    // - item iteration (pointer dereferencing doesn't touch LRU)
    // - printing of cache content to the std::ostream (feature for debugging)
    // - thread-safety (see below)
    // Synchronization is implemented with a recursive mutex, handled internally by the cache.
    // To prevent race conditions on reads, all returning values are wrapped in a special RAII wrapper,
    // which prolongs mutex locking during its lifetime. To make things work, you should receive data
    // from the cache by a named variable whose lifetime corresponds to the scope where you work with
    // returned data. This approach is similar to std::lock_guard, std::unique_lock, and other wrappers.
    template<
        typename Key,
        typename Value,
        typename Hash = std::hash<Key>,
        typename KeyEqual = std::equal_to<Key>,
        typename Allocator = std::allocator<std::pair<const Key, Value> > >
    class SafeCache : Cache<Key, Value, Hash, KeyEqual, Allocator> {
    public:
        // Cache traits.
        using Base = Cache<Key, Value, Hash, KeyEqual, Allocator>;
        using Traits = typename Base::Traits;
        using Item = typename Traits::Item;
        using KeyMem = typename Traits::KeyMem;
        using ValueMem = typename Traits::ValueMem;
        using Buf = typename Traits::Buf;
        using Iter = typename Traits::Iter;
        using ConstIter = typename Traits::ConstIter;
        using ReverseIter = typename Traits::ReverseIter;
        using ConstReverseIter = typename Traits::ConstReverseIter;
        using Table = typename Traits::Table;
        using Mutex = typename Traits::Mutex;
        using Lock = typename Traits::Lock;
        using Guard = typename Traits::Guard;
        static constexpr size_t kItemMem = Traits::kItemMem;

        // Writes a string representation of the cache to the stream.
        template<typename K, typename V>
        friend std::ostream &operator
        <<(std::ostream &, const SafeCache<K, V> &);

        // Returns true if cache items and their LRU order are equal.
        // Non-optimized implementation. Use only for debugging/testing.
        ScopeGuard<bool> operator ==(const SafeCache &other) const {
            Lock lock(mutex_);
            return {std::move(lock), Base::operator==(other)};
        }

        // Returns true if cache items or their LRU order are not equal.
        // Non-optimized implementation. Use only for debugging/testing.
        ScopeGuard<bool> operator !=(const SafeCache &other) const {
            Lock lock(mutex_);
            return {std::move(lock), Base::operator!=(other)};
        }

        // Creates a new cache.
        // 1) If maxsize and maxmem are not specified, the cache turns to the unbounded cache.
        // However, the performance of such an unbounded cache will not be ideal because of LRU.
        // 2) For accurate memory monitoring, the client may provide key_mem and value_mem hint functions
        // that return the actual size of the dynamic buffer allocated by Key and Value.
        // The return value should not include the size of the Key/Value type itself
        // because it is already calculated by the cache with the sizeof() function.
        explicit SafeCache(const size_t maxsize = nval,
                           const size_t maxmem = nval,
                           KeyMem key_mem = nullptr,
                           ValueMem value_mem = nullptr): Base(
            maxsize, maxmem, std::move(key_mem), std::move(value_mem)) {
        }

        // Returns an iterator to the beginning of the cache buffer.
        ScopeGuard<Iter> begin() {
            Lock lock(mutex_);
            return {std::move(lock), Base::begin()};
        }

        // Returns a const iterator to the beginning of the cache buffer.
        ScopeGuard<ConstIter> begin() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::begin()};
        }

        // Returns an iterator to the end of the cache buffer.
        ScopeGuard<Iter> end() {
            Lock lock(mutex_);
            return {std::move(lock), Base::end()};
        }

        // Returns a const iterator to the end of the cache buffer.
        ScopeGuard<ConstIter> end() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::end()};
        }

        // Returns a reverse iterator to the beginning of the cache buffer.
        ScopeGuard<ReverseIter> rbegin() {
            Lock lock(mutex_);
            return {std::move(lock), Base::rbegin()};
        }

        // Returns a const reverse iterator to the beginning of the cache buffer.
        ScopeGuard<ConstReverseIter> rbegin() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::rbegin()};
        }

        // Returns a reverse iterator to the end of the cache buffer.
        ScopeGuard<ReverseIter> rend() {
            Lock lock(mutex_);
            return {std::move(lock), Base::rend()};
        }

        // Returns a const reverse iterator to the end of the cache buffer.
        ScopeGuard<ConstReverseIter> rend() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::rend()};
        }

        // Stores data, possibly overwriting any existing data.
        // New items are at the top of the LRU.
        template<typename V>
        void Set(const Key &key, V &&value) {
            Guard guard(mutex_);
            Base::Set(key, std::forward<V>(value));
        }

        // Stores data, only if it does not already exist.
        // New items are at the top of the LRU. If an item already exists and an add fails,
        // it promotes the item to the front of the LRU anyway.
        // Returns true if addition succeeded.
        template<typename V>
        ScopeGuard<bool> Add(const Key &key, V &&value) {
            Lock lock(mutex_);
            return {std::move(lock), Base::Add(key, std::forward<V>(value))};
        }

        // Stores this data, but only if the data already exists.
        // Returns true if replacement succeeded.
        template<typename V>
        ScopeGuard<bool> Replace(const Key &key, V &&value) {
            Lock lock(mutex_);
            return {
                std::move(lock), Base::Replace(key, std::forward<V>(value))
            };
        }

        // Retrieves data by the key.
        // Returns empty optional if an item was not found.
        ScopeGuard<std::optional<std::reference_wrapper<Value> > > Get(
            const Key &key) {
            Lock lock(mutex_);
            return {std::move(lock), Base::Get(key)};
        }

        // Removes an item from the cache, if it exists.
        // Returns true if deletion succeeded.
        ScopeGuard<bool> Delete(const Key &key) {
            Lock lock(mutex_);
            return {std::move(lock), Base::Delete(key)};
        }

        // Clears the cache without touching hits/misses statistics.
        void Flush() {
            Guard guard(mutex_);
            Base::Flush();
        }

        // Returns current item count.
        [[nodiscard]] ScopeGuard<size_t> Size() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::Size()};
        }

        // Returns current memory usage.
        [[nodiscard]] ScopeGuard<size_t> Memory() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::Memory()};
        }

        // Returns upper limit of item count.
        [[nodiscard]] ScopeGuard<size_t> Maxsize() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::Maxsize()};
        }

        // Returns upper limit of memory usage.
        [[nodiscard]] ScopeGuard<size_t> Maxmem() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::Maxmem()};
        }

        // Limits maximum item count.
        // It also shrinks the cache to the new limit (if needed).
        void Maxsize(size_t items) {
            Guard guard(mutex_);
            Base::Maxsize(items);
        }

        // Limits maximum memory usage.
        // It also shrinks the cache to the new limit (if needed).
        void Maxmem(const size_t bytes) {
            Guard guard(mutex_);
            Base::Maxmem(bytes);
        }

        // Returns cache statistics.
        [[nodiscard]] ScopeGuard<CacheInfo> Stats() const {
            Lock lock(mutex_);
            return {std::move(lock), Base::Stats()};
        }

        // Serializes cached items to the user-provided output iterator.
        // Single-pass iterators are also suitable. But don't use std::ostream_iterator
        // because it provides formatting and corrupts the binary data.
        // Use std::ostreambuf_iterator instead.
        template<serde::output_byte_iterator Iter>
        void Dump(Iter it) const {
            Guard guard(mutex_);
            Base::Dump(std::move(it));
        }

        // Serializes cached items to the user-provided container.
        template<serde::byte_sequence_container Container>
        void Dump(Container &c) const {
            Guard guard(mutex_);
            Base::Dump(c);
        }

        // Deserializes cached items from the user-provided range.
        // Single-pass iterators are also suitable. But don't use std::istream_iterator
        // because it provides formatting and corrupts the binary data.
        // Use std::istreambuf_iterator instead.
        template<serde::input_byte_iterator Iter>
        void Load(Iter first, Iter last) {
            Guard guard(mutex_);
            Base::Load(std::move(first), std::move(last));
        }

        // Deserializes cached items from the user-provided container.
        template<serde::byte_sequence_container Container>
        void Load(const Container &c) {
            Guard guard(mutex_);
            Base::Load(c);
        }

    private:
        mutable Mutex mutex_;
    };

    template<typename Key, typename Value>
    std::ostream &operator<<(std::ostream &out,
                             const SafeCache<Key, Value> &cache) {
        // <syncstream> from C++20 standard is not widely supported by the compilers.
        // So as a workaround we implement stream synchronization ourselves.
        static std::mutex m;
        std::scoped_lock guard{m, cache.mutex_};
        const std::string name = std::format("lru::SafeCache<Key={}, Value={}>",
                                             typeid(Key).name(),
                                             typeid(Value).name());
        out << name << " at " << &cache << '\n';
        out << cache.Stats() << '\n';
        size_t n = 0;
        // We don't need SafeCache here, because the cache mutex is already locked.
        for (const auto &item: static_cast<const Cache<Key, Value> &>(cache)) {
            out << aux::ItemToStr(item, n) << '\n';
            n++;
        }
        return out << std::flush;
    }
}

#endif // LRU_CACHE_SAFE_CACHE_H
