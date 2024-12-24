// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Cache statistics.

#ifndef LRU_CACHE_STATS_H
#define LRU_CACHE_STATS_H

#include <format>
#include <string>

#include "lru_cache/traits.h"

namespace lru {
    // ========================================================================
    // CacheInfo
    // ========================================================================

    // Helper structure representing cache statistics.
    // Mimics Python lru_cache statistics.
    struct CacheInfo {
        size_t hits{0}; // Cache::Get() hits
        size_t misses{0}; // Cache::Get() misses
        size_t maxsize{nval}; // Item count upper limit
        size_t currsize{0}; // Current item count
        size_t maxmem{nval}; // Memory usage upper limit
        size_t currmem{0}; // Current memory usage

        // Converts statistics to the string.
        [[nodiscard]] std::string to_string() const {
            const std::string smaxsize = maxsize == nval
                                             ? "inf"
                                             : std::to_string(maxsize);
            const std::string smaxmem = maxmem == nval
                                            ? "inf"
                                            : std::to_string(maxmem);
            return std::format(
                "hits {} | misses {} | maxsize {} | currsize {} | maxmem {} | currmem {}",
                hits, misses, smaxsize, currsize, smaxmem, currmem);
        }

        // Returns true if statistics are equal.
        bool operator==(const CacheInfo &other) const {
            return hits == other.hits and misses == other.misses
                   and maxsize == other.maxsize and currsize == other.currsize
                   and maxmem == other.maxmem and currmem == other.currmem;
        }

        // Returns true if statistics are not equal.
        bool operator!=(const CacheInfo &other) const {
            return not(*this == other);
        }
    };

    // Writes string representation of the statistics to the stream.
    inline std::ostream &operator<<(std::ostream &out, const CacheInfo &stats) {
        return out << stats.to_string();
    }
}

#endif // LRU_CACHE_STATS_H
