// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Scope guard (implementation).

#ifndef LRU_CACHE_GUARD_IMPL_H
#define LRU_CACHE_GUARD_IMPL_H

#include <cassert>
#include <mutex>
#include <type_traits>

namespace lru::aux {
    // ========================================================================
    // Wrappers
    // ========================================================================

    // Encapsulates functionality common for all wrappers.
    class BaseGuard {
    public:
        using Lock = std::unique_lock<std::recursive_mutex>;

        explicit BaseGuard(Lock lock): lock_(std::move(lock)) {
            assert(
                lock_.owns_lock() &&
                "Mutex is not locked [lru::aux::BaseGuard::BaseGuard()]");
        }

        BaseGuard(const BaseGuard &) = delete;

        BaseGuard(BaseGuard &&other) = default;

        BaseGuard &operator=(const BaseGuard &) = delete;

        BaseGuard &operator=(const BaseGuard &&) = delete;

    private:
        Lock lock_; // dtor: lock_.unlock();
    };

    // Wrapper for fundamentals.
    template<typename T>
    class FundamentalGuard : public BaseGuard {
    public:
        explicit
        FundamentalGuard(Lock lock, T value): BaseGuard(std::move(lock)),
                                              value_(value) {
        }

        operator T() const { return value_; }

    private:
        T value_;
    };

    // Wrapper for class objects.
    template<typename T>
    struct ObjectGuard : BaseGuard, T {
        template<typename... Ts>
        explicit
        ObjectGuard(Lock lock, Ts &&... args): BaseGuard(std::move(lock)),
                                               T(std::forward<Ts>(args)...) {
        }

        // Implicit:
        // operator T &() { return *this; }
        // operator const T &() const { return *this; }
    };

    // ========================================================================
    // Switcher
    // ========================================================================

    // Chooses the appropriate wrapper depends on T (implementation).
    template<typename T>
    auto GuardSwitcherImpl(std::true_type is_fundamental,
                           std::false_type is_class) -> FundamentalGuard<T>;

    template<typename T>
    auto GuardSwitcherImpl(std::false_type is_fundamental,
                           std::true_type is_class) -> ObjectGuard<T>;

    // Chooses the appropriate wrapper depends on T (facade).
    template<typename T>
    using GuardSwitcher = decltype(GuardSwitcherImpl<T>(
        std::is_fundamental<T>(), std::is_class<T>()));
}

#endif // LRU_CACHE_GUARD_IMPL_H
