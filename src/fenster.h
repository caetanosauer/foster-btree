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

#ifndef FOSTER_BTREE_FENSTER_H
#define FOSTER_BTREE_FENSTER_H

/**
 * \file fenster.h
 *
 * Functionality needed to encapsulate fence keys, foster relationships, and compression metadata of
 * a B-tree node.
 */

#include "assertions.h"
#include "exceptions.h"

namespace foster {

template <class Key, class PointerType>
class Fenster
{
public:

    using LengthType = uint16_t;

    Fenster(Key* low, Key* high, Key* foster, PointerType foster_ptr)
        : foster_ptr(foster_ptr)
    {
        is_low_key_infinity_ = false;
        is_high_key_infinity_ = false;
        is_foster_empty_ = false;

        Key empty_key = Key{0};
        if (!low) { low = &empty_key; is_low_key_infinity_ = true; }
        if (!high) { high = &empty_key; is_low_key_infinity_ = true; }
        if (!foster) { foster = &empty_key; is_foster_empty_ = true; }

        low_fence = *low;
        high_fence = *high;
        foster_key = *foster;
    }

    static size_t compute_size(Key*, Key*, Key*)
    {
        return sizeof(Key) * 3 + sizeof(PointerType);
    }

    size_t get_size() const
    {
        return sizeof(Key) * 3 + sizeof(PointerType);
    }

    size_t get_prefix_size() const { return 0; }

    void get_keys(Key* low, Key* high, Key* foster) const
    {
        if (low) { *low = low_fence; }
        if (high) { *high = high_fence; }
        if (foster) {
            if (!is_foster_empty()) { *foster = foster_key; }
            else { *foster = high_fence; }
        }
    }

    bool is_low_key_infinity() const { return is_low_key_infinity_; }
    bool is_high_key_infinity() const { return is_high_key_infinity_; }
    bool is_foster_empty() const { return is_foster_empty_; }

    PointerType get_foster_ptr() const { return foster_ptr; }

protected:
    Key low_fence;
    Key high_fence;
    Key foster_key;
    PointerType foster_ptr;

    bool is_low_key_infinity_ : 1;
    bool is_high_key_infinity_ : 1;
    bool is_foster_empty_ : 1;
};

template <class PointerType>
class Fenster<string, PointerType>
{
public:

    using LengthType = uint16_t;

    Fenster(string* low, string* high, string* foster, PointerType foster_ptr)
        : foster_ptr(foster_ptr)
    {
        string empty_key {""};
        if (!low) { low = &empty_key; }
        if (!high) { high = &empty_key; }
        if (!foster) { foster = &empty_key; }

        size_t prefix_len = get_common_prefix_length(*low, *high);
        assert<1>(foster->substr(0, prefix_len) == low->substr(0, prefix_len),
                "Foster key does not match the prefix of low- and high-fence keys");

        this->prefix_len = prefix_len;
        low_fence_len = low->length() - prefix_len;
        high_fence_len = high->length() - prefix_len;
        foster_key_len = foster->length() - prefix_len;

        char* dest = get_data_offset();
        memcpy(dest, low->data(), prefix_len);
        dest += prefix_len;

        memcpy(dest, low->substr(prefix_len).data(), low_fence_len);
        dest += low_fence_len;

        memcpy(dest, high->substr(prefix_len).data(), high_fence_len);
        dest += high_fence_len;

        memcpy(dest, foster->substr(prefix_len).data(), foster_key_len);
    }

    size_t get_size() const
    {
        return sizeof(Fenster<string, PointerType>)
            + prefix_len + low_fence_len + high_fence_len + foster_key_len;
    }

    static size_t compute_size(string* low, string* high, string* foster)
    {
        string empty_key {""};
        if (!low) { low = &empty_key; }
        if (!high) { high = &empty_key; }
        if (!foster) { foster = &empty_key; }

        size_t prefix_len = get_common_prefix_length(*low, *high);
        return sizeof(Fenster<string, PointerType>)
            + low->length() + high->length() + foster->length() - (2 * prefix_len);
    }

    static size_t get_common_prefix_length(const string& a, const string& b)
    {
        size_t i = 0;
        while (i < a.length() && i < b.length() && a[i] == b[i]) { i++; }
        return i;
    }

    void get_keys(string* low, string* high, string* foster) const
    {
        if (low) {
            low->assign(get_data_offset(), prefix_len + low_fence_len);
        }
        if (high) {
            high->reserve(prefix_len + high_fence_len);
            high->assign(get_data_offset(), prefix_len);
            high->append(get_data_offset() + prefix_len + low_fence_len, high_fence_len);
        }
        if (foster) {
            foster->reserve(prefix_len + foster_key_len);
            foster->assign(get_data_offset(), prefix_len);
            foster->append(get_data_offset() + prefix_len + low_fence_len + high_fence_len,
                    foster_key_len);
        }
    }

    PointerType get_foster_ptr () const { return foster_ptr; }

    bool is_low_key_infinity() const { return prefix_len + low_fence_len == 0; }
    bool is_high_key_infinity() const { return prefix_len + high_fence_len == 0; }
    bool is_foster_empty() const { return prefix_len + foster_key_len == 0; }

protected:

    LengthType prefix_len;
    LengthType low_fence_len;
    LengthType high_fence_len;
    LengthType foster_key_len;
    PointerType foster_ptr;

    char* get_data_offset()
    {
        return reinterpret_cast<char*>(this) + sizeof(Fenster<string, PointerType>);
    }

    const char* get_data_offset() const
    {
        return reinterpret_cast<const char*>(this) + sizeof(Fenster<string, PointerType>);
    }
};

} // namespace foster

#endif
