/*
 * MIT License
 *
 * Copyright (c) 2016 Caetano Sauer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FOSTER_BTREE_ENCODING_H
#define FOSTER_BTREE_ENCODING_H

/**
 * \file encoding.h
 *
 * Classes and utilities to encode objets, tuples, or arbitrary key-value pairs into the format
 * accepted by the lower-level slot array, which supports fixed-length keys (usually a poor man's
 * normalized key) and uninterpreted byte sequences as payloads.
 *
 * Encoding and decoding functionality is implemented with the Encoder policy, which supports both
 * stateless and stateful encoding. The former does not maintain any internal buffers or metadata,
 * and always performs encoding/decoding on-the-fly. The latter allows for more complex
 * serialization schemes, where an intermediate buffer is required; this would be useful to encode
 * arbitrary tuples or objects, for instance.
 *
 * These functions only support scalar types for fixed-length keys and payloads and string for
 * variable length. Other variable-length types like vector result in a compilation error due to a
 * static assertion failure. This is not considered a serious restriction though, as strings can
 * easily be used to encode binary data in C++.
 */

#include <climits>
#include <type_traits>
#include <cstdint>
#include <cstring>
#include <string>

using std::string;

namespace foster {

/**
 * \brief Utility function to swap the endianness of an arbitrary input variable.
 *
 * Credit due to stackoverflow user alexandre-c
 */
template <typename T>
T swap_endianness(T u)
{
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++) {
        dest.u8[k] = source.u8[sizeof(T) - k - 1];
    }

    return dest.u;
}

/**
 * \brief Dummy prefixing function that returns its input unaltered.
 */
template <class K>
struct NoPrefixing
{
    K operator()(const K& key) { return key; }
};

/**
 * \brief Function object used for prefixing, i.e., extracting a poor man's normalized key.
 *
 * Takes a key K, which must be of a scalar type, and extracts the appropriate poor man's normalized
 * key of type PMNK_Type.
 *
 * There are two possible behaviors with respect to endianness:
 * 1. If K and PMNK_Type are of the same size, no endianness conversion is performed. In this case,
 * the function basically behaves as a cast that maintains the same byte representation. For
 * example, a simple cast from a float 27.5 into an int would yield 27, which does not have the same
 * byte representation.
 * 2. If K is larger than PMNK_Type (as per sizeof), then the endianness must be swapped twice:
 * first to extract the sizeof(PMNK_Type) most significant bytes of the key, and then to restore the
 * original little-endian representation used for comparisons. This overhead my be substantial.
 * However, scalar keys with smaller PMNK type is not a situation for which we aim to optimize at
 * the moment. It might be best to simply use an 8-byte PMNK for an 8-byte key, for instance. This
 * also saves the overhead of encoding the rest of the key on the payload area of the slot array.
 */
template <class K, class PMNK_Type>
struct PoormanPrefixing
{
    static_assert(std::is_scalar<K>::value,
            "Encoding with poor man's normalized keys requires scalar or string key types");
    static_assert(sizeof(K) >= sizeof(PMNK_Type),
            "The type of a poor man's normalized key cannot be larger than the type of the original key");

    PMNK_Type operator()(const K& key)
    {
        union {
            K swapped;
            PMNK_Type prefix;
        };

        if (sizeof(K) >= sizeof(PMNK_Type)) {
            swapped = swap_endianness<K>(key);
            prefix = swap_endianness<PMNK_Type>(prefix);
        }
        else { // same size -- do nothing
            swapped = key;
        }
        return prefix;
    }
};

/**
 * \brief Specialization of PoormanPrefixing for string keys.
 *
 * This is probably the common case, where keys are of variable length. In this case, the first
 * sizeof(PMNK_Type) bytes are extracted and converted into little-endian representation.
 */
template <class PMNK_Type>
struct PoormanPrefixing<string, PMNK_Type>
{
    PMNK_Type operator()(const string& key)
    {
        union {
            unsigned char bytes[sizeof(PMNK_Type)];
            PMNK_Type prefix;
        };

        size_t amount = sizeof(PMNK_Type);
        if (key.length() < amount) {
            prefix = 0;
            amount = key.length();
        }
        memcpy(&bytes, key.data(), amount);
        prefix = swap_endianness<PMNK_Type>(prefix);
        return prefix;
    }
};

/**
 * \brief Base class of all encoders which use a common PMNK extraction mechanism.
 */
template <class K, class PMNK_Type = K>
class PNMKEncoder
{
public:
    /**
     * The function is picked at compile time based on the type parameters. If key and PMNK are of
     * the same type, no conversion is required and the dummy NoPrefixing is used. Otherwise,
     * PoormanPrefixing, the default PMNK extractor, is chosen.
     */
    using PrefixingFunction = typename
        std::conditional<std::is_same<PMNK_Type, K>::value,
        NoPrefixing<K>,
        PoormanPrefixing<K, PMNK_Type>>::type;

    PMNK_Type get_pmnk(const K& key)
    {
        // TODO: compiler should be able to inline this -- verify!
        PrefixingFunction f;
        return f(key);
    }
};

/**
 * \brief Basic encoder used for fixed-length key-value pairs.
 *
 * If K and PMNK_Type are the same, keys are not encoded as part of the payload; otherwise the first
 * sizeof(K) bytes of the payload are used to store the key, and the remainder ones for the value.
 */
template <class K, class V, class PMNK_Type = K>
class StatelessEncoder : public PNMKEncoder<K, PMNK_Type>
{
public:

    /** \brief Returns encoded length of a key-value pair */
    size_t get_payload_length(const K&, const V&)
    {
        return get_payload_length(nullptr);
    }

    /** \brief Returns length of an encoded payload */
    size_t get_payload_length(void*)
    {
        if (!std::is_same<K, PMNK_Type>::value) {
            return sizeof(K) + sizeof(V);
        }
        else { return sizeof(V); }
    }

    /** \breif Encodes a given key-value pair into a given memory area */
    void encode(const K& key, const V& value, void* dest)
    {
        if (!std::is_same<K, PMNK_Type>::value) {
            memcpy(dest, &key, sizeof(K));
        }
        memcpy(dest, &value, sizeof(V));
    }

    /**
     * \brief Decodes a given memory area into a key-value pair
     *
     * If value is given as a null pointer, only the key is decoded.
     */
    void decode(const PMNK_Type& pmnk, K& key, V* value, void* src)
    {
        if (!std::is_same<K, PMNK_Type>::value) {
            memcpy(&key, src, sizeof(K));
        }
        else { key = pmnk; }

        if (value) {
            memcpy(&value, src, sizeof(V));
        }
    }
};

/**
 * \brief Specialization of the basic encoder for variable-length (i.e., string) values
 *
 * Works just like the generic StatelessEncoder, but the length of the value must be encoded in the
 * payload prior to the actual value bytes.
 */
template <class K, class PMNK_Type>
class StatelessEncoder<K, string, PMNK_Type> : public PNMKEncoder<K, PMNK_Type>
{
public:

    /**
     * Size of the encoded length. This could be a template argument, but C++ does not allow partial
     * template specialization with default arguments.
     */
    using LengthType = uint16_t;

    size_t get_payload_length(const K&, const string& value)
    {
        size_t ksize = std::is_same<K, PMNK_Type>::value ? 0 : sizeof(K);
        return ksize + sizeof(LengthType) + value.length();
    }

    size_t get_payload_length(void* payload)
    {
        size_t ksize = std::is_same<K, PMNK_Type>::value ? 0 : sizeof(K);
        LengthType vsize = *(reinterpret_cast<LengthType*>(payload)) + sizeof(LengthType);
        return ksize + vsize;
    }

    void encode(const K& key, const string& value, void* dest)
    {
        if (!std::is_same<K, PMNK_Type>::value) {
            memcpy(dest, &key, sizeof(K));
        }
        *(reinterpret_cast<LengthType*>(dest)) = value.length();
        memcpy(dest, value.data(), value.length());
    }

    void decode(const PMNK_Type& pmnk, K& key, string* value, void* src)
    {
        if (!std::is_same<K, PMNK_Type>::value) {
            memcpy(&key, src, sizeof(K));
        }
        else { key = pmnk; }

        if (value) {
            LengthType length = *(reinterpret_cast<LengthType*>(src));
            value->assign(reinterpret_cast<char*>(src) + sizeof(LengthType), length);
        }
    }
};

/**
 * \brief Specialization of the basic encoder for variable-length (i.e., string) keys and values.
 *
 * Payload contains the length of the key, followed by the actual key bytes (including the PMNK
 * bytes), followed by the length of the value, and finally the actual value bytes.
 */
template <class PMNK_Type>
class StatelessEncoder<string, string, PMNK_Type> : public PNMKEncoder<string, PMNK_Type>
{
public:

    /**
     * Size of the encoded length. This could be a template argument, but C++ does not allow partial
     * template specialization with default arguments.
     */
    using LengthType = uint16_t;

    size_t get_payload_length(const string& key, const string& value)
    {
        return 2 * sizeof(LengthType) + key.length() + value.length();
    }

    size_t get_payload_length(void* payload)
    {
        LengthType klen = *(reinterpret_cast<LengthType*>(payload));
        char* vsrc = reinterpret_cast<char*>(payload) + sizeof(LengthType) + klen;
        LengthType vlen = *(reinterpret_cast<LengthType*>(vsrc));
        return klen + vlen;
    }

    void encode(const string& key, const string& value, void* dest)
    {
        char* buf = reinterpret_cast<char*>(dest);

        *(reinterpret_cast<LengthType*>(buf)) = key.length();
        buf += sizeof(LengthType);
        memcpy(buf, key.data(), key.length());
        buf += key.length();

        *(reinterpret_cast<LengthType*>(buf)) = value.length();
        buf += sizeof(LengthType);
        memcpy(buf, value.data(), value.length());
        // buf += value.length();
    }

    void decode(string& key, string* value, void* src)
    {
        LengthType klen = *(reinterpret_cast<LengthType*>(src));
        key.assign(reinterpret_cast<char*>(src) + sizeof(LengthType), klen);

        if (value) {
            char* vsrc = reinterpret_cast<char*>(src) + sizeof(LengthType) + klen;
            LengthType vlen = *(reinterpret_cast<LengthType*>(vsrc));
            value->assign(reinterpret_cast<char*>(vsrc) + sizeof(LengthType), vlen);
        }
    }
};


} // namespace foster

#endif
