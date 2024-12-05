// Header-only C++ implementation of LRU cache with all the gems.
// Features: thread safety, serialization, memory monitoring, statistics, etc.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Serialization/deserialization.

#ifndef LRU_CACHE_SERDE_H
#define LRU_CACHE_SERDE_H

#include <string_view>
#include <type_traits>
#include <vector>

#include "lru_cache/detail/utils.h"
#include "lru_cache/traits.h"

namespace lru::serde {
    // ========================================================================
    // End-user api
    // ========================================================================

    // ------------------------------------------------------------------------
    // Typedefs and concepts
    // ------------------------------------------------------------------------

    // To avoid platform-specific bugs, we use only fixed-width integer types.

    using Byte = uint8_t;
    using Size = uint64_t;
    using Bytes = std::vector<Byte>;
    using View = std::basic_string_view<Byte>;

    // Byte concept.
    // The public API accepts all byte-like integer types (char, unsigned char, signed char, uint8_t, int8_t).
    // But for better portability, we highly recommend using only the uint8_t.
    template<typename T>
    concept byte = std::is_same_v<T, char>
                   or std::is_same_v<T, unsigned char>
                   or std::is_same_v<T, signed char>
                   or std::is_same_v<T, uint8_t>
                   or std::is_same_v<T, int8_t>;

    // Concept for entity with byte value type.
    template<typename T>
    concept byte_value_type = byte<aux::ValueTypeT<T> >;

    // Input iterator concept for Cache.Load() method.
    template<typename I>
    concept input_byte_iterator = std::input_iterator<I> and byte_value_type<I>;

    // Output iterator concept for Cache.Dump() method.
    template<typename I>
    concept output_byte_iterator = std::output_iterator<I, Byte>
                                   and byte_value_type<I>;

    // Container concept for Cache.Load()/Cache.Dump() methods.
    // It should be at least SequenceContainer.
    // For better dumping performance, you may also use ContiguousContainer.
    template<typename Container>
    concept byte_sequence_container = byte_value_type<Container>
                                      // Suppress ambiguity of Cache.Dump() call
                                      and not output_byte_iterator<Container>;

    // Integral concept for Serde specialization.
    template<typename T>
    concept integral = std::is_integral_v<T>;

    // Integral sequence concept for Serde specialization.
    template<typename T>
    concept integral_sequence = aux::IsSequenceV<T> and integral<aux::ValueTypeT<T> >;

    // ------------------------------------------------------------------------
    // Serde template
    // ------------------------------------------------------------------------

    // Serializer/deserializer.
    // The client may provide specializations for arbitrary types in lru::serde namespace.
    // See existing specializations for interface requirements.
    template<typename T>
    struct Serde;

    // Specialization for integral types.
    template<integral T>
    struct Serde<T> {
        static Bytes Serialize(T val) {
            Bytes buf;
            auto curr = reinterpret_cast<Byte *>(&val);
            for (int i = 0; i < sizeof(T); ++i) {
                buf.push_back(*curr);
                ++curr;
            }
            return buf;
        }

        static T Deserialize(const View chunk) {
            T val = 0;
            auto it = std::to_address(chunk.begin());
            for (int i = 0; i < sizeof(T); ++i) {
                val |= static_cast<T>(*it) << i * 8;
                ++it;
            }
            return val;
        }
    };

    // Specialization for integral sequences.
    template<integral_sequence T>
    struct Serde<T> {
        using ValueT = typename aux::ValueType<T>::type;
        static constexpr size_t Length = sizeof(ValueT);

        static Bytes Serialize(const T &seq) {
            Bytes buf;
            for (auto val: seq) {
                Bytes repr = Serde<ValueT>::Serialize(val);
                buf.insert(buf.end(), repr.begin(), repr.end());
            }
            return buf;
        }

        T Deserialize(const View chunk) {
            T seq{};
            for (auto curr = chunk.begin(); curr != chunk.end(); curr += Length) {
                ValueT val = Serde<ValueT>::Deserialize({curr, Length});
                if constexpr (aux::IsForwardListV<T>) {
                    seq.push_front(val);
                } else if constexpr (aux::IsArrayV<T>) {
                    size_t n = std::distance(chunk.begin(), curr) / Length;
                    seq[n] = val;
                } else { seq.push_back(val); }
            }
            if constexpr (aux::IsForwardListV<T>) { seq.reverse(); }
            return seq;
        }
    };
}

namespace lru::serde::aux {
    // ========================================================================
    // Implementation details
    // ========================================================================

    // ------------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------------

    // Encodes size variable as row bytes with system-specific byte order.
    // On most platforms, it will be a little-endian.
    // The byte order at the encoding and decoding stages must be the same.
    inline Bytes EncodeSize(Size size) {
        const Byte *byte = reinterpret_cast<Byte *>(&size);
        Bytes res;
        for (int i = 0; i < sizeof(Size); ++i) { res.push_back(byte[i]); }
        return res;
    }

    // Decodes row bytes as size variable, keeping the input byte order.
    // The byte order at the encoding and decoding stages must be the same.
    // Here, we accept a reference to the iterator, so further advancing is unnecessary.
    // This approach makes the function suitable for single-pass iterators, which is crucial
    // to stream support.
    template<input_byte_iterator It>
    Size DecodeSize(It &it) {
        Size size = 0;
        for (int i = 0; i < sizeof(Size); ++i) {
            size |= static_cast<Size>(*it) << i * 8;
            ++it;
        }
        return size;
    }

    // ------------------------------------------------------------------------
    // Serializing iterator
    // ------------------------------------------------------------------------

    // Serializing iterator over the cached items.
    // Wrapper for the Cache::ConstReverseIter.
    // At every dereferencing, it provides a new chunk of row bytes containing serialized item.
    // Unlike std::output_iterator, increment only moves the underlying pointer; no other work has a place.
    template<typename Traits>
    class SerializingIterator {
    public:
        using Key = std::remove_cvref_t<typename Traits::KeyParam>;
        using Value = std::remove_cvref_t<typename Traits::ValueParam>;
        using WrappedIter = typename Traits::ConstReverseIter;

        using difference_type = std::ptrdiff_t;
        using value_type = Bytes;

        explicit SerializingIterator(WrappedIter it): curr_{std::move(it)} {
        }

        bool operator==(const SerializingIterator &other) const {
            return curr_ == other.curr_;
        }

        bool operator!=(const SerializingIterator &other) const {
            return curr_ != other.curr_;
        }

        SerializingIterator &operator++() {
            ++curr_;
            return *this;
        }

        SerializingIterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        Bytes operator*() { return Serialize(); }

    private:
        WrappedIter curr_;
        Serde<Key> key_serde_;
        Serde<Value> value_serde_;

        Bytes Serialize() {
            Bytes buf;
            SerializeChunk(key_serde_, curr_->first, buf);
            SerializeChunk(value_serde_, curr_->second, buf);
            return buf;
        }

        template<typename S, typename T>
        static void SerializeChunk(S &serde, const T &obj, Bytes &buf) {
            const Bytes chunk = serde.Serialize(obj);
            WriteChunk(EncodeSize(chunk.size()), buf);
            WriteChunk(chunk, buf);
        }

        static void WriteChunk(const Bytes &source, Bytes &dest) {
            dest.insert(dest.end(), source.begin(), source.end());
        }
    };

    // ------------------------------------------------------------------------
    // Deserializing iterator
    // ------------------------------------------------------------------------

    // Deserializing iterator over the row bytes.
    // Wrapper for the arbitrary byte iterator.
    // At every dereferencing, it provides the new deserialized item.
    // Attention! The increment operator actually does nothing; all the work is in dereferencing.
    // So, this wrapper is suitable only for full-range algorithms (std::copy, std::for_each, etc.).
    template<typename Traits, input_byte_iterator Iter>
    class DeserializingIterator {
    public:
        using Key = std::remove_cvref_t<typename Traits::KeyParam>;
        using Value = std::remove_cvref_t<typename Traits::ValueParam>;
        using WrappedIter = Iter;

        using difference_type = std::ptrdiff_t;
        using value_type = typename CacheTraits<Key, Value>::Item;

        explicit DeserializingIterator(WrappedIter it): curr_(std::move(it)) {
        }

        bool operator==(const DeserializingIterator &other) const {
            return curr_ == other.curr_;
        }

        bool operator!=(const DeserializingIterator &other) const {
            return curr_ != other.curr_;
        }

        DeserializingIterator &operator++() { return *this; }

        DeserializingIterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        value_type operator*() { return Deserialize(); }

    private:
        WrappedIter curr_;
        Serde<Key> key_serde_;
        Serde<Value> value_serde_;

        value_type Deserialize() {
            return {
                DeserializeChunk(key_serde_), DeserializeChunk(value_serde_)
            };
        }

        template<typename S>
        auto DeserializeChunk(S &serde) {
            const Size size = DecodeSize(curr_);
            if constexpr (std::contiguous_iterator<Iter>) {
                // Underlying byte data is contiguous.
                // We are dealing with ContiguousContainer (string, vector, etc.).
                // Thus, we could specify the required chunk with the memory view.
                auto data = reinterpret_cast<const Byte *>(std::to_address(curr_));
                auto res = serde.Deserialize({data, static_cast<size_t>(size)});
                std::advance(curr_, size);
                return res;
            } else {
                // Underlying byte data is not contiguous.
                // Highly-likely we are dealing with the input stream.
                // Thus, we should manually create the required chunk.
                Bytes chunk = ReadChunk(size);
                return serde.Deserialize({chunk.data(), chunk.size()});
            }
        }

        Bytes ReadChunk(const Size size) {
            // We don't use iterator algorithms here to make the implementation suitable
            // for single-pass iterators, which is crucial to stream support.
            Bytes res;
            for (int n = 0; n < size; ++n) {
                res.push_back(*curr_);
                ++curr_;
            }
            return res;
        }
    };
}

#endif // LRU_CACHE_SERDE_H
