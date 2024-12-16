// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Unit tests for LRU cache (gtest framework).

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "gmock/gmock-function-mocker.h"
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

#include "lru_cache.h"

template<>
struct lru::serde::Serde<std::string> {
    static Bytes Serialize(const std::string &str) {
        return {str.begin(), str.end()};
    }

    static std::string Deserialize(const View chunk) {
        return {reinterpret_cast<const char*>(chunk.data()), chunk.size()};
    }
};

// ============================================================================
// CRUD test suite
// ============================================================================

// We have some redundant code below when generating cache samples.
// However, this approach is more descriptive than alternatives.

template<typename Cache>
class CrudTestSuite : public testing::Test {
protected:
    // {{3, 'c'}, {2, 'b'}, {1, 'a'}}
    Cache sample_;
    const Cache sample_const_;
    // {{3, 'z'}, {2, 'y'}, {1, 'x'}}
    Cache sample_alt_;
    const Cache sample_alt_const_;
    // {{6, 'f'}, {5, 'e'}, {4, 'd'}, {3, 'c'}, {2, 'b'}, {1, 'a'}}
    Cache sample_ext_;
    Cache sample_ext_const_;
    // {{1, 'a'}, {2, 'b'}, {3, 'c'}}
    Cache rsample_;
    Cache rsample_const_;
    // {{1, 'x'}, {2, 'y'}, {3, 'z'}}
    Cache rsample_alt_;
    Cache rsample_alt_const_;
    // {{1, 'a'}, {2, 'b'}, {3, 'c'}, {4, 'd'}, {5, 'e'}, {6, 'f'}}
    Cache rsample_ext_;
    Cache rsample_ext_const_;
    // {}
    Cache fresh_;
    const Cache fresh_const_;

    const typename Cache::Buf sample_buf_ = {
        {3, 'c'},
        {2, 'b'},
        {1, 'a'}
    };

    const typename Cache::Buf rsample_buf_{
        {1, 'a'},
        {2, 'b'},
        {3, 'c'}
    };

    void SetUp() override {
        SetCalls(sample_);
        SetCalls(const_cast<Cache &>(sample_const_));
        SetCallsAlt(sample_alt_);
        SetCallsAlt(const_cast<Cache &>(sample_alt_const_));
        SetCallsExt(sample_ext_);
        SetCallsExt(const_cast<Cache &>(sample_ext_const_));
        ReversedSetCalls(rsample_);
        ReversedSetCalls(const_cast<Cache &>(rsample_const_));
        ReversedSetCallsAlt(rsample_alt_);
        ReversedSetCallsAlt(const_cast<Cache &>(rsample_alt_const_));
        ReversedSetCallsExt(rsample_ext_);
        ReversedSetCallsExt(const_cast<Cache &>(rsample_ext_const_));
    }

    template<typename Iter>
    static typename Cache::Buf CopyBuf(Iter first, Iter last) {
        return typename Cache::Buf(std::move(first), std::move(last));
    }

    template<typename T>
    static void SetCalls(T &sample) {
        sample.Set(1, 'a');
        sample.Set(2, 'b');
        sample.Set(3, 'c');
    }

    template<typename T>
    static void SetCallsAlt(T &sample) {
        sample.Set(1, 'x');
        sample.Set(2, 'y');
        sample.Set(3, 'z');
    }

    template<typename T>
    static void SetCallsExt(T &sample) {
        sample.Set(1, 'a');
        sample.Set(2, 'b');
        sample.Set(3, 'c');
        sample.Set(4, 'd');
        sample.Set(5, 'e');
        sample.Set(6, 'f');
    }

    template<typename T>
    static void ReversedSetCalls(T &sample) {
        sample.Set(3, 'c');
        sample.Set(2, 'b');
        sample.Set(1, 'a');
    }

    template<typename T>
    static void ReversedSetCallsAlt(T &sample) {
        sample.Set(3, 'z');
        sample.Set(2, 'y');
        sample.Set(1, 'x');
    }

    template<typename T>
    static void ReversedSetCallsExt(T &sample) {
        sample.Set(6, 'f');
        sample.Set(5, 'e');
        sample.Set(4, 'd');
        sample.Set(3, 'c');
        sample.Set(2, 'b');
        sample.Set(1, 'a');
    }
};

using CacheTypes = testing::Types<lru::Cache<int, char>, lru::SafeCache<int,
    char> >;
TYPED_TEST_SUITE(CrudTestSuite, CacheTypes);

TYPED_TEST(CrudTestSuite, TestComparisonOperators) {
    EXPECT_TRUE(this->sample_ == this->sample_const_);
    EXPECT_FALSE(this->fresh_const_ == this->sample_);
    EXPECT_FALSE(this->sample_const_ != this->sample_);
    EXPECT_TRUE(this->fresh_ != this->sample_const_);
}

TYPED_TEST(CrudTestSuite, TestRanges) {
    EXPECT_EQ(this->CopyBuf(this->sample_.begin(), this->sample_.end()),
              this->sample_buf_);
    EXPECT_EQ(
        this->CopyBuf(this->sample_const_.begin(), this->sample_const_.end()),
        this->sample_buf_);
    EXPECT_EQ(this->CopyBuf(this->sample_.rbegin(), this->sample_.rend()),
              this->rsample_buf_);
    EXPECT_EQ(
        this->CopyBuf(this->sample_const_.rbegin(), this->sample_const_.rend()),
        this->rsample_buf_);
}

TYPED_TEST(CrudTestSuite, TestSetMethod) {
    this->SetCalls(this->fresh_);
    EXPECT_EQ(this->fresh_, this->sample_const_);
    this->SetCallsAlt(this->fresh_);
    EXPECT_EQ(this->fresh_, this->sample_alt_const_);
}

TYPED_TEST(CrudTestSuite, TestAddMethod) {
    EXPECT_TRUE(this->fresh_.Add(1, 'a'));
    EXPECT_TRUE(this->fresh_.Add(2, 'b'));
    EXPECT_TRUE(this->fresh_.Add(3, 'c'));
    EXPECT_EQ(this->fresh_, this->sample_const_);
    EXPECT_FALSE(this->fresh_.Add(3, 'z'));
    EXPECT_FALSE(this->fresh_.Add(2, 'y'));
    EXPECT_FALSE(this->fresh_.Add(1, 'x'));
    EXPECT_EQ(this->fresh_, this->rsample_const_);
}

TYPED_TEST(CrudTestSuite, TestReplaceMethod) {
    EXPECT_TRUE(this->sample_.Replace(1, 'x'));
    EXPECT_TRUE(this->sample_.Replace(2, 'y'));
    EXPECT_TRUE(this->sample_.Replace(3, 'z'));
    EXPECT_EQ(this->sample_, this->sample_alt_const_);
    EXPECT_FALSE(this->fresh_.Replace(1, 'x'));
    EXPECT_FALSE(this->fresh_.Replace(2, 'y'));
    EXPECT_FALSE(this->fresh_.Replace(3, 'z'));
    EXPECT_EQ(this->fresh_, this->fresh_const_);
}

TYPED_TEST(CrudTestSuite, TestGetMethod) {
    EXPECT_EQ(this->sample_.Get(3), 'c');
    EXPECT_EQ(this->sample_.Get(2), 'b');
    EXPECT_EQ(this->sample_.Get(1), 'a');
    EXPECT_EQ(this->sample_, this->rsample_const_);
    EXPECT_FALSE(this->sample_.Get(4));
    EXPECT_FALSE(this->sample_.Get(5));
    EXPECT_FALSE(this->sample_.Get(6));
    EXPECT_EQ(this->sample_, this->rsample_const_);
}

TYPED_TEST(CrudTestSuite, TestDeleteMethod) {
    EXPECT_TRUE(this->sample_.Delete(1));
    EXPECT_TRUE(this->sample_.Delete(2));
    EXPECT_TRUE(this->sample_.Delete(3));
    EXPECT_EQ(this->sample_, this->fresh_const_);
    EXPECT_FALSE(this->sample_.Delete(1));
    EXPECT_FALSE(this->sample_.Delete(2));
    EXPECT_FALSE(this->sample_.Delete(3));
    EXPECT_EQ(this->sample_, this->fresh_const_);
}

TYPED_TEST(CrudTestSuite, TestFlushMethod) {
    this->sample_.Flush();
    EXPECT_EQ(this->sample_, this->fresh_const_);
}

TYPED_TEST(CrudTestSuite, TestSizeGetter) {
    EXPECT_EQ(this->fresh_.Size(), 0);
    EXPECT_EQ(this->sample_.Size(), 3);
    EXPECT_EQ(this->sample_ext_.Size(), 6);
}

TYPED_TEST(CrudTestSuite, TestMemoryGetter) {
    EXPECT_EQ(this->fresh_.Memory(), 0);
    EXPECT_EQ(this->sample_.Memory(), 3 * TypeParam::kItemMem);
    EXPECT_EQ(this->sample_ext_.Memory(), 6 * TypeParam::kItemMem);
}

TYPED_TEST(CrudTestSuite, TestMaxsizeGetter) {
    EXPECT_EQ(this->fresh_.Maxsize(), lru::nval);
    this->fresh_.Maxsize(0);
    EXPECT_EQ(this->fresh_.Maxsize(), 0);
}

TYPED_TEST(CrudTestSuite, TestMaxmemGetter) {
    EXPECT_EQ(this->fresh_.Maxmem(), lru::nval);
    this->fresh_.Maxmem(0);
    EXPECT_EQ(this->fresh_.Maxmem(), 0);
}

TYPED_TEST(CrudTestSuite, TestMaxsizeSetterLru) {
    this->fresh_.Maxsize(3);
    this->ReversedSetCallsExt(this->fresh_);
    EXPECT_EQ(this->fresh_, this->rsample_const_);
}

TYPED_TEST(CrudTestSuite, TestMaxsizeSetterLimit) {
    this->rsample_ext_.Maxsize(3);
    EXPECT_EQ(this->rsample_ext_, this->rsample_const_);
    this->rsample_ext_.Maxsize(0);
    EXPECT_EQ(this->rsample_ext_, this->fresh_const_);
}

TYPED_TEST(CrudTestSuite, TestMaxmemSetterLru) {
    this->fresh_.Maxmem(3 * TypeParam::kItemMem);
    this->ReversedSetCallsExt(this->fresh_);
    EXPECT_EQ(this->fresh_, this->rsample_const_);
}

TYPED_TEST(CrudTestSuite, TestMaxmemSetterLimit) {
    this->rsample_ext_.Maxmem(3 * TypeParam::kItemMem);
    EXPECT_EQ(this->rsample_ext_, this->rsample_const_);
    this->rsample_ext_.Maxmem(0);
    EXPECT_EQ(this->rsample_ext_, this->fresh_const_);
}

TYPED_TEST(CrudTestSuite, TestStatsMethod) {
    auto &cache = this->fresh_;
    auto req = lru::CacheInfo{0, 0, lru::nval, 0, lru::nval, 0};
    EXPECT_EQ(cache.Stats(), req);
    cache.Set(1, 'a');
    cache.Set(2, 'b');
    cache.Set(3, 'c');
    req.currsize = 3;
    req.currmem = 3 * cache.kItemMem;
    EXPECT_EQ(cache.Stats(), req);
    cache.Add(3, 'd');
    EXPECT_EQ(cache.Stats(), req);
    cache.Get(1);
    cache.Get(2);
    req.hits = 2;
    EXPECT_EQ(cache.Stats(), req);
    cache.Get(4);
    cache.Get(5);
    req.misses = 2;
    EXPECT_EQ(cache.Stats(), req);
    cache.Delete(1);
    cache.Delete(4);
    req.currsize = 2;
    req.currmem = 2 * cache.kItemMem;
    EXPECT_EQ(cache.Stats(), req);
    cache.Maxsize(10);
    req.maxsize = 10;
    EXPECT_EQ(cache.Stats(), req);
    cache.Maxmem(1000);
    req.maxmem = 1000;
    EXPECT_EQ(cache.Stats(), req);
    cache.Flush();
    req.currsize = 0;
    req.currmem = 0;
    EXPECT_EQ(cache.Stats(), req);
}

TYPED_TEST(CrudTestSuite, TestStreamOutputBasic) {
    std::ostringstream ss;
    ss << this->fresh_;
    const char *fresh_re =
            R"(.*hits 0 \| misses 0 \| maxsize inf \| currsize 0 \| maxmem inf \| currmem.*)";
    EXPECT_THAT(ss.str(), testing::ContainsRegex(fresh_re));
}

struct Foo {
};

TYPED_TEST(CrudTestSuite, TestStreamOutputFormatting) {
    lru::SafeCache<int, Foo> cache;
    cache.Set(1, Foo());
    std::ostringstream ss;
    ss << cache;
    const char *fmt_re = R"(\[1\] = '<val at 0x[0-9a-f]+>')";
    EXPECT_THAT(ss.str(), testing::ContainsRegex(fmt_re));
}

// ============================================================================
// Allocated memory monitoring test suite
// ============================================================================

TEST(CrudTestSuiteExt, TestAllocMemoryMonitoring) {
    using Cache = lru::Cache<std::string, std::string>;
    // For accurate results, we need to use std::string::capacity() method.
    // But we use std::string::size() for testing because it is more predictable.
    auto op = [](const std::string& str) { return str.size(); };
    Cache cache(lru::nval, lru::nval, op, op);
    size_t mem = 0;
    EXPECT_EQ(cache.Memory(), mem);
    const std::string key1 {"1"}, value1{"12"}, key2{"123"}, value2{"1234"};
    mem += cache.kItemMem + key1.size() * 2 + value1.size();
    cache.Set(key1, value1); // {key1, value1}
    EXPECT_EQ(cache.Memory(), mem);
    mem += cache.kItemMem + key2.size() * 2 + value2.size();
    cache.Add(key2, value2); // {key1, value1}, {key2, value2}
    EXPECT_EQ(cache.Memory(), mem);
    mem = mem - value2.size() + value1.size();
    cache.Replace(key2, value1); // {key1, value1}, {key2, value1}
    EXPECT_EQ(cache.Memory(), mem);
    mem -= cache.kItemMem + key1.size() * 2 + value1.size();
    cache.Delete(key1); // {key2, value1}
    EXPECT_EQ(cache.Memory(), mem);
    mem = cache.kItemMem + key1.size() * 2 + value2.size();
    cache.Maxsize(1);
    cache.Set(key1, value2); // {key1, value2} because maxsize == 1
    EXPECT_EQ(cache.Memory(), mem);
    EXPECT_EQ(cache.Size(), cache.Maxsize());
    mem = 0;
    cache.Maxmem(cache.kItemMem + key1.size() * 2 + value1.size()); // {}
    EXPECT_EQ(cache.Memory(), mem);
    mem += cache.kItemMem + key1.size() * 2 + value1.size();
    cache.Set(key1, value1); // {key1, value1}
    EXPECT_EQ(cache.Memory(), mem);
    EXPECT_EQ(cache.Memory(), cache.Maxmem());
}

// ============================================================================
// Serde loader test suite
// ============================================================================

// We need a Cartesian product for types (like mp_product from boost::mp11)
// to create all combinations for ByteT X ContainerT X CacheT and reduce code duplication below.
// However, we tend to avoid third-party dependencies or metaprogramming in tests.

template<typename Param>
class SerdeLoaderTestSuite : public testing::Test {
protected:
    using Cache = typename Param::second_type;

    Cache sample_;
    const Cache sample_const_;
    Cache fresh_;

    void SetUp() override {
        SetCalls(sample_);
        SetCalls(const_cast<Cache &>(sample_const_));
    }

    static void SetCalls(Cache &cache) {
        cache.Set("key 1", "value 1");
        cache.Set("key two", "value two");
        cache.Set("key three", "value three");
    }
};

using CacheT = lru::Cache<std::string, std::string>;
using SafeCacheT = lru::SafeCache<std::string, std::string>;
using ByteAndCacheParams = testing::Types<
    std::pair<char, CacheT>,
    std::pair<char, SafeCacheT>,
    std::pair<unsigned char, CacheT>,
    std::pair<unsigned char, SafeCacheT>,
    std::pair<signed char, CacheT>,
    std::pair<signed char, SafeCacheT>,
    std::pair<uint8_t, CacheT>,
    std::pair<uint8_t, SafeCacheT>,
    std::pair<int8_t, CacheT>,
    std::pair<int8_t, SafeCacheT> >;
TYPED_TEST_SUITE(SerdeLoaderTestSuite, ByteAndCacheParams);

TYPED_TEST(SerdeLoaderTestSuite, TestSerdeWithList) {
    using Byte = typename TypeParam::first_type;
    std::list<Byte> buf;
    this->sample_.Dump(buf);
    this->fresh_.Load(buf);
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

TYPED_TEST(SerdeLoaderTestSuite, TestSerdeWithDeque) {
    using Byte = typename TypeParam::first_type;
    std::deque<Byte> buf;
    this->sample_.Dump(buf);
    this->fresh_.Load(buf);
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

TYPED_TEST(SerdeLoaderTestSuite, TestSerdeWithVector) {
    using Byte = typename TypeParam::first_type;
    std::vector<Byte> buf;
    this->sample_.Dump(buf);
    this->fresh_.Load(buf);
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

TYPED_TEST(SerdeLoaderTestSuite, TestWithString) {
    using Byte = typename TypeParam::first_type;
    std::basic_string<Byte> buf;
    this->sample_.Dump(buf);
    this->fresh_.Load(buf);
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

TYPED_TEST(SerdeLoaderTestSuite, TestSerdeWithStringStream) {
    using Byte = typename TypeParam::first_type;
    std::basic_stringstream<Byte> ss;
    this->sample_.Dump(std::ostreambuf_iterator<Byte>(ss));
    this->fresh_.Load(std::istreambuf_iterator<Byte>(ss),
                      std::istreambuf_iterator<Byte>());
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

TYPED_TEST(SerdeLoaderTestSuite, TestSerdeWithFileStream) {
    using Byte = typename TypeParam::first_type;
    if (not std::is_same_v<Byte, char>) {
        // basic_fstream supports only char, so we skip others.
        GTEST_SKIP();
    }
    constexpr auto flags = std::ios::binary | std::ios::trunc | std::ios::in |
                           std::ios::out;
    std::fstream f{"temp.txt", flags};
    ASSERT_TRUE(f.is_open());
    this->sample_.Dump(std::ostreambuf_iterator(f));
    f.seekp(0);
    this->fresh_.Load(std::istreambuf_iterator(f),
                      std::istreambuf_iterator<char>());
    EXPECT_EQ(this->fresh_, this->sample_const_);
    f.close();
    std::filesystem::remove("temp.txt");
}

// ============================================================================
// Serde integral test suite
// ============================================================================

template<typename T>
class SerdeIntegralTestSuite : public testing::Test {
protected:
    using Cache = lru::Cache<T, T>;

    Cache sample_;
    const Cache sample_const_;
    Cache fresh_;

    void SetUp() override {
        SetCalls(sample_);
        SetCalls(const_cast<Cache &>(sample_const_));
    }

    static void SetCalls(Cache &cache) {
        cache.Set(1, 100);
        cache.Set(10, 10);
        cache.Set(100, 1);
    }
};

using IntegralTypes = testing::Types<char, unsigned char, signed char, uint8_t,
    int8_t, short int, unsigned short int, int, unsigned int, long int, unsigned
    long int, long long int, unsigned long long int>;
TYPED_TEST_SUITE(SerdeIntegralTestSuite, IntegralTypes);

TYPED_TEST(SerdeIntegralTestSuite, TestSerdeIntegral) {
    std::vector<lru::serde::Byte> buf;
    this->sample_.Dump(buf);
    this->fresh_.Load(buf);
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

// ============================================================================
// Serde integral sequence test suite
// ============================================================================

template<typename Seq>
class SerdeIntegralSequenceTestSuite : public testing::Test {
protected:
    using Cache = lru::Cache<int, Seq>;

    Cache sample_;
    const Cache sample_const_;
    Cache fresh_;

    void SetUp() override {
        SetCalls(sample_);
        SetCalls(const_cast<Cache &>(sample_const_));
    }

    static void SetCalls(Cache &cache) {
        cache.Set(1, Seq{1, 2, 3});
        cache.Set(2, Seq{4, 5, 6});
        cache.Set(3, Seq{7, 8, 9});
    }
};

using SequenceTypes = testing::Types<
    std::string,
    std::vector<char>,
    std::deque<char>,
    std::forward_list<char>,
    std::list<char>,
    std::array<char, 3> >;
TYPED_TEST_SUITE(SerdeIntegralSequenceTestSuite, SequenceTypes);

TYPED_TEST(SerdeIntegralSequenceTestSuite, TestSerdeIntegralSequence) {
    std::vector<lru::serde::Byte> buf;
    this->sample_.Dump(buf);
    this->fresh_.Load(buf);
    EXPECT_EQ(this->fresh_, this->sample_const_);
}

// ============================================================================
// Serde integral sequence test suite (additional tests)
// ============================================================================

TEST(SerdeIntegralSequenceTestSuiteEx, TestSerdeFullCharRange) {
    using Cache = lru::Cache<std::string, std::string>;
    Cache sample, result;
    for (char ch = std::numeric_limits<char>::min(); ch <= std::numeric_limits<char>::max(); ++ch) {
        for (auto [key_n, value_n]: std::vector<std::pair<int, int>>{{2048, 512}, {1024, 1024}, {512, 2048}}) {
            std::string key(key_n, ch);
            std::string value(value_n, ch);
            sample.Set(key, value);
            result.Set(key, value);
        }
        if (ch == std::numeric_limits<char>::max()) { break; }
    }
    std::stringstream ss;
    sample.Dump(std::ostreambuf_iterator(ss));
    sample.Flush();
    ss.seekp(0);
    sample.Load(std::istreambuf_iterator(ss), std::istreambuf_iterator<char>());
    EXPECT_EQ(sample, result);
}

TEST(SerdeIntegralSequenceTestSuiteEx, TestSerdeLongSequence) {
    using Cache = lru::Cache<std::string, std::string>;
    Cache sample, result;
    std::string seq(40000, 'a');
    sample.Set(seq, seq);
    result.Set(seq, seq);
    std::stringstream ss;
    sample.Dump(std::ostreambuf_iterator(ss));
    sample.Flush();
    ss.seekp(0);
    sample.Load(std::istreambuf_iterator(ss), std::istreambuf_iterator<char>());
    EXPECT_EQ(sample, result);
}

TEST(SerdeIntegralSequenceTestSuiteEx, TestSerdeDiffItemSize) {
    using Cache = lru::Cache<std::string, std::string>;
    for (int n = 0; n < 256; ++n) {
        Cache sample, result;
        std::string seq(n, 'a');
        sample.Set(seq, seq);
        result.Set(seq, seq);
        std::stringstream ss;
        sample.Dump(std::ostreambuf_iterator(ss));
        sample.Flush();
        ss.seekp(0);
        sample.Load(std::istreambuf_iterator(ss), std::istreambuf_iterator<char>());
        EXPECT_EQ(sample, result);
    }
}

// ============================================================================
// Synchronization test suite
// ============================================================================

// Extracts item number from the cache output.
std::string GetItemNum(const std::string &ln) {
    std::string res;
    int n = 0;
    while (ln[n] != ':') {
        res.push_back(ln[n]);
        n++;
    }
    return res;
}

TEST(SynchronizationTestSuite, TestScopeGuard) {
    static lru::SafeCache<int, char> cache;
    cache.Set(1, 'a');
    cache.Set(2, 'b');
    cache.Set(3, 'c');
    auto task1 = [] {
        {
            // While task 2 was sleeping, cache mutex was locked by the ret variable from task 1.
            auto ret = cache.Get(1);
            EXPECT_EQ(ret.value(), 'a');
            EXPECT_EQ(cache.Get(2), 'b');
            EXPECT_EQ(cache.Get(3), 'c');
            std::this_thread::sleep_for(std::chrono::seconds(2));
            // While task 1 was sleeping, task 2 started to execute but was immediately blocked
            // because the ret variable still existed and the cache mutex was still locked.
            // So, at this point, cache items should be unchanged.
            EXPECT_EQ(cache.Get(1), 'a');
            EXPECT_EQ(cache.Get(2), 'b');
            EXPECT_EQ(cache.Get(3), 'c');
        }
        // We have left the scope, so the ret variable was destroyed. Now, the cache mutex is unlocked.
        // Task 2 started to execute.
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Task 2 execution has ended, so all cache items should be changed.
        EXPECT_EQ(cache.Get(1), 'x');
        EXPECT_EQ(cache.Get(2), 'y');
        EXPECT_EQ(cache.Get(3), 'z');
    };
    auto task2 = [] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cache.Set(1, 'x');
        cache.Set(2, 'y');
        cache.Set(3, 'z');
    };
    std::thread t1(task1);
    std::thread t2(task2);
    t1.join();
    t2.join();
}

TEST(SynchronizationTestSuite, TestStreamOutput) {
    static lru::SafeCache<int, char> cache;
    static std::ostringstream ss;
    for (int n = 0; n < 256; ++n) { cache.Set(n, n); }
    auto print = [] { ss << cache; };
    std::thread t1(print);
    std::thread t2(print);
    t1.join();
    t2.join();
    // Now, we are checking that the output is not corrupted.
    std::string buf;
    std::vector<std::string> lines;
    std::istringstream in(ss.str());
    while (std::getline(in, buf, '\n')) {
        if (std::isdigit(buf[0])) { lines.push_back(buf); }
    }
    for (int n = 0; n < 256; ++n) {
        const std::string &ln = lines[n];
        EXPECT_EQ(GetItemNum(ln), std::to_string(n));
    }
    for (int n = 0; n < 256; ++n) {
        const std::string &ln = lines[n + 256];
        EXPECT_EQ(GetItemNum(ln), std::to_string(n));
    }
}
