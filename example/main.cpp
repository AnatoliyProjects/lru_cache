// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// LRU cache usage example.
// Here, we show how to use the LRU cache to reduce DB reads.

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "lru_cache.h"

// Model fields.
using Id = size_t;
using Name = std::array<char, 42>;

// User model.
struct User {
    Id id;
    Name name;
};

// Fake database.
static std::unordered_map<Id, User> db;

// Thread-safe cache.
static lru::SafeCache<Id, User> cache;

// User serialization/deserialization.
// To make things work, we only need to provide lru::serde::Serde specialization
// for the User type. This is a simple task because specializations for the integral types
// and integral sequences are already implemented (see 'lru_cache/serde.h').
// Use them as building blocks.
template<>
struct lru::serde::Serde<User> {
    // Converts the User object to row bytes.
    static Bytes Serialize(const User &user) {
        Bytes buf;
        // id
        Bytes id = Serde<Id>::Serialize(user.id);
        buf.insert(buf.end(), id.begin(), id.end());
        // name
        Bytes name = Serde<Name>::Serialize(user.name);
        buf.insert(buf.end(), user.name.begin(), user.name.end());
        return buf;
    }

    // Converts chunk of row bytes to the User object.
    static User Deserialize(const View chunk) {
        // id
        Id id = Serde<Id>::Deserialize({chunk.begin(), sizeof(Id)});
        // name
        Name name;
        auto first = chunk.begin() + sizeof(Id);
        auto last = first + sizeof(Name);
        std::copy(first, last, name.begin());
        return {id, name};
    }
};

// Saves user to DB (highly costly, but we haven't alternatives).
void SaveUserDb(const User &user) {
    std::cout << "DB: save User id=" << user.id << std::endl;
    db[user.id] = user;
}

// Loads user from DB (also costly, but we do have an alternative - read from cache).
User LoadUserDb(size_t id) {
    std::cout << "DB: load User id=" << id << std::endl;
    return db.at(id);
}

// Receives POST request (just mock).
User ReceivePostRequest(const User &user) {
    std::cout << "Request: POST example.com/user/new/";
    std::cout << std::format("Body: {id={}, name={}}", user.id, user.name);
    std::cout << std::endl;
    return user;
}

// Receives GET request (another one mock).
size_t ReceiveGetRequest(size_t id) {
    std::cout << "Request: GET example.com/user/" << id << std::endl;
    return id;
}

int main(int, char *[]) {
    // Assume we are dealing with HTTP requests, and the code below is inside our REST API endpoints.
    // We have a pool of workers to process HTTP requests in parallel, so our code should be thread-safe.
    // Let's try to use the LRU cache to reduce the number of DB reads.
    // ...
    // Request: POST example.com/user/new (create a new User)
    // JSON body: {"id"=123, "name"="John Smith"}
    {
        User new_user = ReceivePostRequest({123, "John Smith"});
        SaveUserDb(new_user);
        cache.Set(new_user.id, new_user);
    }
    // ...
    // Request: GET example/com/user/123 (find the User with id=123)
    {
        Id id = ReceiveGetRequest(123);
        // Here, the 'user' variable is, in fact, a RAII wrapper that encapsulates both
        // a reference to the cached User object and std::unique_lock with the acquired cache mutex.
        // Thus, while the 'user' variable exists, the cache mutex is locked, which excludes
        // possible data races on 'user'.
        auto user = cache.Get(id);
        if (user) {
            // The user is cached, so no DB request is needed.
            std::cout << "No DB request, User loaded from the cache!" << '\n';
            // ...
        } else {
            // The user is not found in the cache; a DB request is needed.
            User db_user = LoadUserDb(id);
            cache.Set(db_user.id, db_user);
            // ...
        }
    }
    // ...
    // We can print cache items and stats.
    std::cout << "\nInfo:\n" << cache;
    // Or save our cache to a file...
    std::fstream f("temp.txt",
                   std::ios::trunc | std::ios::binary | std::ios::in |
                   std::ios::out);
    assert(f.is_open());
    cache.Dump(std::ostreambuf_iterator(f));
    // ...clear the cache...
    cache.Flush();
    // ...set new limits...
    cache.Maxmem(cache.kItemMem);
    cache.Maxsize(1);
    // ...and reload it.
    f.seekp(0);
    cache.Load(std::istreambuf_iterator(f), std::istreambuf_iterator<char>());
    std::cout << "\nAfter dump:\n" << cache;
    // [Anti-pattern]
    // ...or print the cached item in a non thread-safe manner :(
    // (Don't do this! Code below is not thread-safe, because ScopeGuard, returned by cache.Get(),
    // is used only as a temporary object. When this temporary destroys, cache mutex releases).
    auto user = cache.Get(123).value().get();
    // (Here, cache mutex has already been released, so we may get a dangling reference
    // instead of a User object if this User will be deleted from the cache by another thread)
    std::cout << "\nValue at key == 123:";
    std::cout << " {" << user.id << " " << user.name.data() << "}";
    return 0;
}
