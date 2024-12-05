# lru_cache v. 1.0

**Header-only C++ implementation of LRU cache with all the gems.**

Features: thread safety, serialization, memory monitoring, statistics, etc.

The MIT License (MIT). Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>

# Description

LRU cache is a data structure that combines fast item access by the key with 
invalidation of least-recently-used items in case of cache limit exhaustion.

These traits make the LRU cache useful when you need to effectively memoize 
some computations (requests, resources, etc.) without risking memory blowup.

Our implementation uses `std::unordered_map` as a hashtable (for item lookup) and
`std::list` as a buffer for cached items. Any time the client requests an item, 
the requested item is moved to the front of the buffer, which allows us to track 
item usage and delete the least-recently-used item when the cache has exceeded the limit.

Thus, we have the constant time complexity for average reads/writes and linear time complexity 
for the worst case (key collision for every item, which is possible only in case of attack
or terrible hash function).

Also, we have some insignificant overhead in keeping `std::unordered_map` and `std::list` in sync, 
which doesn't affect time complexity in Big O notation.

# Overview

`lru_cache` interface mimics the Memcached text protocol (where it makes sense).

We support the following Memcached-like commands with the same semantics
(except the last one, which clears the cache instead of its invalidation):
- `Set`, `Add`, `Replace`, `Get`, `Delete`, `Stats`, `Flush`

The following Memcached analogous are not supported:
- `append`/`prepend`/`incr`/`decr` (our cache is typed; just modify the value)
- `cas`/`gets` (if you need synchronization, use `lru::SafeCache`)
- `stats items`/ `stats slabs`/`stats sizes` (send cache to stream; it prints the content)

Also, this library provides some extended functionality:
- cache serialization/deserialization (via `Dump` and `Load` commands)
- limiting of size/memory during execution (via `Maxsize`/`Maxmem` commands)
- item iteration via range protocol: `begin`, `end`, `rbegin`, `rend` (it doesn't touch LRU)
- sending of cache content to the `std::ostream` (feature for debugging)

`lru_cache` interface is straightforward. Just take a look at the [cache.h](include/lru_cache/cache.h) 
or [safe_cache.h](include/lru_cache/safe_cache.h) headers.

Also, you may consult with the detailed usage example at [main.cpp](example/main.cpp).

Here is the simplified version:

```cpp
#include <fstream>
#include <iostream>
#include <string>

#include "lru_cache.h"

void CacheUsageExample() {
    // Create a new cache limited by three items with key = int, value = string.
    lru::Cache<int, std::string> cache(3);
    cache.Set(0, "I will gone :(");
    cache.Set(1, "Value 1");
    cache.Set(2, "Value 2");
    cache.Set(3, "Value 3"); // Here, the last-recently-used item was deleted.
    // Current state: {{3, "Value 3"}, {2, "Value 2"}, {1, "Value 1"}}
    // Let's check it!
    std::cout << cache << std::endl;
    // The following Add command will fail because the item already exists.
    assert(!cache.Add(2, "Value 42"));
    // However, existing items may be successfully changed using the Set/Replace commands.
    cache.Set(2, "Value 41"); // Now 41
    assert(cache.Replace(2, "Value 42")); // Now 42
    // ...they may be got...
    // (Here cache.Get(2) result is std::optional with std::reference_wrapper to std::string
    // so we need this annoying '.value().get()' part)
    assert(cache.Get(2).value().get() == "Value 42");
    // ...or deleted.
    assert(cache.Delete(2));
    // We also may try to get non-existent items.
    auto res = cache.Get(2);  // empty std::optional
    assert(!res.has_value());

    // Now, let's dump the cache to a file...
    std::fstream f("temp.txt",
                   std::ios::trunc | std::ios::binary | std::ios::in |
                   std::ios::out);
    assert(f.is_open());
    cache.Dump(std::ostreambuf_iterator(f));
    // ...clear it...
    cache.Flush();
    // ...set a new memory limit...
    cache.Maxmem(cache.kItemMem);  // maxsize == 3, maxmem == cache.kItemMem
    // ...and reload it.
    f.seekp(0);
    cache.Load(std::istreambuf_iterator(f), std::istreambuf_iterator<char>());
    // Because memory usage is limited by one item, the cache looks as follows: {{3, "Value 3"}}.
    // Let's check it!
    std::cout << "Cache after reloading:\n" << cache;
}
```

Possible output:

>lru::Cache<Key=i, Value=NSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE> at 0x7ff7b4528e70  
hits 0 | misses 0 | maxsize 3 | currsize 3 | maxmem inf | currmem 144  
0: [3] = 'Value 3'  
1: [2] = 'Value 2'  
2: [1] = 'Value 1'  
Cache after reloading:  
lru::Cache<Key=i, Value=NSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE> at 0x7ff7b4528e70  
hits 1 | misses 1 | maxsize 3 | currsize 1 | maxmem 48 | currmem 48  
0: [3] = 'Value 3'

# Thread safety

We have two available implementations of LRU cache with a similar interface:

| Class name       | Source file            | Description           |
|------------------|------------------------|-----------------------| 
| `lru::Cache`     | lru_cache/cache.h      | Non thread-safe cache |
| `lru::SafeCache` | lru_cache/safe_cache.h | Thread-safe cache     | 

`lru::Cache` contains no synchronization primitives and should not be used in a multithreading environment.

`lru::SafeCache` is thread-safe. Synchronization is implemented with a recursive mutex, 
handled internally by the cache. Every method of `lru::SafeCache` uses `std::lock_guard` or another block,
so manual synchronization is not needed.

Also, to prevent race conditions on reads, all returning values are wrapped in a special RAII wrapper
(`lru::ScopeGuard`), which prolongs mutex locking during its lifetime. To make things work, you should 
receive data from the `lru::SafeCache` by a named variable whose lifetime corresponds to the scope 
where you work with the returned data. This approach is similar to `std::lock_guard`,` std::unique_lock`, etc.

Here is the simple example:

```cpp
#include <string>

#include "lru_cache.h"

void MultithreadingExample() {
    // Create a new thread-safe cache limited by one item.
    lru::SafeCache<int, std::string> cache(1);
    // Thread-safe. Synchronization is provided by the cache.
    cache.Set(1, "Value 1");
    // Thread-safe. While the 'res' variable exists, cache mutex is still locked.
    {
        auto res = cache.Get(1);
        // Do something...
    }
    // Non thread-safe!
    // Here, cache.Get(1) is a temporary object that will be immediately destroyed.
    // So, cache mutex will be released before the if-body starts to execute.
    if (cache.Get(1).value().get() == "Value 1") {
        // Race condition! Here, the item value may not be equal to "Value 1"
        // because another thread may change it!
    }
    // Thread-safe. Now, cache mutex is still locked until the end of the current scope
    // because 'res' is a named variable.
    auto res = cache.Get(1);
    if (res.value().get() == "Value 1") {
        // Do something...
    }
}
```

# Serialization

Both cache implementations provide serialization/deserialization functionality with `Dump` and `Load` methods,
which have overloads for pointers and containers:

```cpp
// Serializes cached items to the user-provided output iterator.
// Single-pass iterators are also suitable.
template<serde::output_byte_iterator Iter>
void Cache::Dump(Iter it) const;

// Serializes cached items to the user-provided container.
template<serde::byte_sequence_container Container>
void Cache::Dump(Container &c) const;

// Deserializes cached items from the user-provided range.
// Single-pass iterators are also suitable.
template<serde::input_byte_iterator Iter>
void Cache::Load(Iter first, Iter last);

// Deserializes cached items from the user-provided container.
template<serde::byte_sequence_container Container>
void Cache::Load(const Container &c);
```

But don't use `std::istream_iterator`/`std::ostream_iterator` because they perform formatting 
that corrupts the binary data! Use `std::istreambuf_iterator`/`std::ostreambuf_iterator` instead.

Underlying translation to/form binary data required the appropriate `lru::serde::Serde`
template specialization. We have predefined specializations for integral types 
(`char`, `int`, fixed-width integer types) and sequences with integral value type
(`std::string`, `std::vector`, `std::deque`, `std::forward_list`, `std::list`, `std::array`, row arrays).

A possible serializer/deserializer for `std::string` (already provided) may look as follows:

```cpp
template<>
struct lru::serde::Serde<std::string> {
    static Bytes Serialize(const std::string &str) {
        return {str.begin(), str.end()};
    }

    static std::string Deserialize(const View chunk) {
        return {reinterpret_cast<const char*>(chunk.data()), chunk.size()};
    }
};
```

If you need to serialize/deserialize complex objects or you have strong performance requirements,
consider specialized libraries, including [boost/serialization](https://www.boost.org/doc/libs/1_86_0/libs/serialization/doc/index.html).
We don't use any of them to avoid third-party dependencies.

# Memory monitoring

Unlike the majority of LRU cache implementations, with `lru_cache`, you may
limit maximum size both in terms of item count or memory usage.

Item count/memory usage may be limited in ctor...

```cpp
// Creates a new cache.
explicit Cache(const size_t maxsize = nval, const size_t maxmem = nval);

// Creates a new cache.
explicit SafeCache(const size_t maxsize = nval, const size_t maxmem = nval);
```

...or dynamically during program execution:

```cpp
// Limits maximum item count.
// It also shrinks the cache to the new limit (if needed).
void Cache::Maxsize(size_t items);

// Limits maximum memory usage.
// It also shrinks the cache to the new limit (if needed).
void Cache::Maxmem(const size_t bytes)
```

If `maxsize` and `maxmem` are not specified (default), the cache turns to the unbounded cache.
However, the performance of such an unbounded cache will not be ideal because LRU tracking still performs.

You may receive item count and memory usage with `Stats` method (see below) or 
with `Maxsize`/`Maxmem` (limits) and `Size`/`Memory` (actual) methods.

It should be mentioned that `lru_cache` returns only *approximate memory usage*.
The actual memory usage for non-POD/complex types may differ significantly.
For example, if the desired type contains a pointer to a dynamic buffer, we will consider 
only the pointer size, not the buffer itself.

Also, memory usage value doesn't include the size of internal cache structures
or the size of allocated, but not filled memory (it is, the `currmem` value shows only 
the approximate (calculated) size of all cached items, that's all).

If you need more control over memory usage, you may provide your own allocator.

# Statistics

Cache statics are available via `Stats` method, which returns an object with the following fields:

```cpp
// Helper structure representing cache statistics.
// Mimics Python lru_cache statistics.
struct CacheInfo {
    size_t hits; // Cache::Get() hits
    size_t misses; // Cache::Get() misses
    size_t maxsize; // Item count upper limit
    size_t currsize; // Current item count
    size_t maxmem; // Memory usage upper limit
    size_t currmem; // Current memory usage
    
    // ...
};
```

Hits/misses statistics are counting only for the `Get` command.

# Testing

`lru_cache` is hardly tested with `gtest` framework. Every public method has at least one test case. 
Obviously, it doesn't exclude potential bugs. If you find one, feel free to make an issue on GitHub.

# Building

`lru_cache` is a header-only library. You can include it in your project without special compile/link steps.

The CMake files are provided for [lru_cache_test.cpp](test/lru_cache_test.cpp) (unit tests) 
and [main.cpp](example/main.cpp) (usage example).

# License

`lru_cache` is licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.
