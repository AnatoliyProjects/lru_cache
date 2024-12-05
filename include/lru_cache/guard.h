// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Scope guard (end-user api).

#ifndef LRU_CACHE_GUARD_H
#define LRU_CACHE_GUARD_H

#include "lru_cache/detail/guard_impl.h"

namespace lru {
    // ========================================================================
    // Scope guard (end-user api)
    // ========================================================================

    // Non-copyable (but movable) RAII wrapper, which prolongs mutex locking during its lifetime.
    // Uses as a wrapper for SafeCache returning values (T) to avoid race conditions on reads.
    template<typename T>
    class ScopeGuard : public aux::GuardSwitcher<T> {
        using Base = aux::GuardSwitcher<T>;

    public:
        // Lock type.
        using Lock = typename Base::Lock;

        // Creates a new ScopeGuard from the lock object with acquired mutex.
        // Mutex will be released within ScopeGuard dtor.
        template<typename... Ts>
        ScopeGuard(Lock lock, Ts... args): Base(
            std::move(lock), std::forward<Ts>(args)...) {
        }
    };
}

#endif // LRU_CACHE_GUARD_H
