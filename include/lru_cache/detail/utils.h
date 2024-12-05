// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Metafunction helpers.

#ifndef LRU_CACHE_DETAIL_UTILS_H
#define LRU_CACHE_DETAIL_UTILS_H

#include <array>
#include <deque>
#include <format>
#include <forward_list>
#include <iterator>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

namespace lru::aux {
    // ========================================================================
    // Value type getter
    // ========================================================================

    template<typename I>
    using CharTypeT = typename I::char_type;

    template<typename T>
    struct ValueType {
        using type = typename T::value_type;
    };

    template<typename T, size_t N>
    struct ValueType<T[N]> {
        using type = T;
    };

    template<typename... Ts>
    struct ValueType<std::istream_iterator<Ts...> > {
        using type = CharTypeT<std::istream_iterator<Ts...> >;
    };

    template<typename... Ts>
    struct ValueType<std::ostream_iterator<Ts...> > {
        using type = CharTypeT<std::ostream_iterator<Ts...> >;
    };

    template<typename... Ts>
    struct ValueType<std::istreambuf_iterator<Ts...> > {
        using type = CharTypeT<std::istreambuf_iterator<Ts...> >;
    };

    template<typename... Ts>
    struct ValueType<std::ostreambuf_iterator<Ts...> > {
        using type = CharTypeT<std::ostreambuf_iterator<Ts...> >;
    };

    template<typename T>
    using ValueTypeT = typename ValueType<T>::type;

    // ========================================================================
    // Predicate for forward_list
    // ========================================================================

    template<typename>
    struct IsForwardList : std::false_type {
    };

    template<typename... Ts>
    struct IsForwardList<std::forward_list<Ts...> > : std::true_type {
    };

    template<typename T>
    inline constexpr bool IsForwardListV = IsForwardList<T>::value;

    // ========================================================================
    // Predicate for array (row array + std::array)
    // ========================================================================

    template<typename T>
    struct IsArray : std::is_array<T> {
    };

    template<class T, std::size_t N>
    struct IsArray<std::array<T, N> > : std::true_type {
    };

    template<class T>
    inline constexpr bool IsArrayV = IsArray<T>::value;

    // ========================================================================
    // Predicate for sequence (sequence containers + row array)
    // ========================================================================

    template<typename>
    struct IsSequence : std::false_type {
    };

    template<typename... Ts>
    struct IsSequence<std::basic_string<Ts...> > : std::true_type {
    };

    template<typename... Ts>
    struct IsSequence<std::vector<Ts...> > : std::true_type {
    };

    template<typename... Ts>
    struct IsSequence<std::deque<Ts...> > : std::true_type {
    };

    template<typename... Ts>
    struct IsSequence<std::forward_list<Ts...> > : std::true_type {
    };

    template<typename... Ts>
    struct IsSequence<std::list<Ts...> > : std::true_type {
    };

    template<class T, std::size_t N>
    struct IsSequence<std::array<T, N> > : std::true_type {
    };

    template<class T, std::size_t N>
    struct IsSequence<T[N]> : std::true_type {
    };

    template<typename T>
    inline constexpr bool IsSequenceV = IsSequence<T>::value;

    // ========================================================================
    // Predicate for Container.insert(pos, first, last) support
    // ========================================================================

    template<typename C, typename It>
    constexpr auto HasInsertMethodImpl(
        int) -> decltype(std::declval<C>().insert(
                             std::declval<typename C::iterator>(),
                             std::declval<It>(),
                             std::declval<It>()), std::true_type());

    template<typename... Ts>
    constexpr std::false_type HasInsertMethodImpl(...);

    template<typename C, typename It>
    using HasInsertMethodT = decltype(HasInsertMethodImpl<std::remove_cvref_t<C>, It>(0));

    template<typename C, typename It>
    static constexpr bool HasInsertMethodV = HasInsertMethodT<C, It>::value;

    // ========================================================================
    // Predicate for std::formatter<T>() support
    // ========================================================================

    template<typename T>
    constexpr auto IsFormattableImpl(T) -> decltype(std::formatter<T>(), std::true_type());

    template<typename>
    constexpr std::false_type IsFormattableImpl(...);

    template<typename T>
    using IsFormattableT = decltype(IsFormattableImpl<std::remove_cvref_t<T> >(0));

    template<typename T>
    static constexpr bool IsFormattableV = IsFormattableT<T>::value;

    // ========================================================================
    // Item formatting
    // ========================================================================

    template<typename Key, typename Value>
    std::string ItemToStr(const std::pair<Key, Value> &item, size_t n) {
        std::string key;
        if constexpr (IsFormattableV<Key>) {
            key = std::format("{}", item.first);
        } else {
            key = std::format("<key at {:#x}>",
                              reinterpret_cast<uintptr_t>(&item.first));
        }
        std::string val;
        if constexpr (IsFormattableV<Value>) {
            val = std::format("{}", item.second);
        } else {
            val = std::format("<val at {:#x}>",
                              reinterpret_cast<uintptr_t>(&item.second));
        }
        return std::format("{}: [{}] = '{}'", n, key, val);
    }
}

#endif // LRU_CACHE_DETAIL_UTILS_H
