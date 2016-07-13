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

/**
 * \brief Class that encapsulates management of fence keys, foster key, and foster child pointer.
 *
 * The general (non-specialized) template simply stores each field directly into member variables,
 * and thus it is made for fixed-length keys with proper support for copy and assignment. For
 * variable-length keys, we recommend the specialized template for string keys (see below).
 *
 * Note that the general template in principle also works for variable-length key types such as
 * vector and string, but in that case it would only store pointers to the actual variable data.
 * This could be a problem in a database application that wishes to store keys in-place inside nodes
 * of a data structure. Such distinction between the handling of fixed- and variable-length fields
 * is also present in the encoder classes (\see file encoding.h).
 *
 * The name "Fenster" is not only a combination of "fence" and "foster", but also the German word
 * for "window", which is appropriate given that such an object provides a peek into a node's data.
 *
 * \tparam Key Type of keys.
 * \tparam PointerType Type of the child foster pointer.
 */
template <class Key, class PointerType>
class Fenster
{
public:

    using LengthType = uint16_t;

    /**
     * \brief Construct fesnter object with given keys.
     *
     * If a key is given as null, it is interpreted as being an infinity -- negative for the low
     * fence, positive for the high fence, and either of them for the foster key (because in either
     * case the foster child would be empty).
     */
    Fenster(Key* low, Key* high, Key* foster, PointerType foster_ptr)
        : foster_ptr(foster_ptr)
    {
        is_low_key_infinity_ = false;
        is_high_key_infinity_ = false;
        is_foster_empty_ = false;

        Key empty_key = Key{0};
        if (!low) { low = &empty_key; is_low_key_infinity_ = true; }
        if (!high) { high = &empty_key; is_high_key_infinity_ = true; }
        if (!foster) { foster = &empty_key; is_foster_empty_ = true; }

        low_fence = *low;
        high_fence = *high;
        foster_key = *foster;

        // Not having a foster child must imply is_foster_empty
        assert<1>(foster_ptr || is_foster_empty_);
    }

    /*
     * Unlike the variable-length specialization, the size here is simply the result of sizeof().
     */
    static size_t compute_size(Key*, Key*, Key*)
    {
        return sizeof(Fenster<Key, PointerType>);
    }

    size_t get_size() const
    {
        return sizeof(Fenster<Key, PointerType>);
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

    // Explicit flags are required to determine if the keys are infinity and if a foster pointer is
    // valid or not.
    bool is_low_key_infinity_ : 1;
    bool is_high_key_infinity_ : 1;
    bool is_foster_empty_ : 1;
};

/**
 * \brief Specialized Fenster class template for string (i.e., variable-length) keys.
 *
 * This class uses a dirty trick to encode the keys "in-place" (or perhaps semi-in-place): it
 * assumes that the allocated memory area (i.e., the address of "this") contains enough bytes not
 * just for the object data itself (i.e., sizeof(Fenster)), but also for the encoded keys.
 * The actual object data contains only the length of each field and the foster child pointer, since
 * such information is fixed-size. The encoded keys are then stored in the memory area immediately
 * after the object data, i.e., starting on the address of ((char*) this + sizeof(Fenster)). I refer
 * to this memory area as the offload area. The size of it can be determined prior to constructing
 * the fenster object with the static method compute_size. Here's an example of how to construct it:
 *
 * \code{.cpp}
 *   Fenster<P>* create_fenster(string* low, string* high, string* foster, P foster_ptr)
 *   {
 *       size_t fenster_size = Fenster<P>::compute_size(low, high, foster);
 *       void* addr = new char[fenster_size];
 *       return new (addr) Fenster<P> {low, high, foster, foster_ptr};
 *   }
 * \endcode
 *
 * I'm aware of the ugliness of this approach, but sometimes a man's gotta do what a man's gotta do.
 *
 * TODO: make this dirty trick less dirty by forbidding direct construction and using a static
 * construction method that always allocates the right amount of memory (from an allocator
 * parameter) and intializes both the lengths and the encoded region.
 *
 * The encoding supports prefix truncation by identifying the common leading bytes on the low and
 * high fence keys. We keep track of the prefix length and only store it along with the low fence
 * key -- high fence and foster keys are then truncated.
 *
 * \tparam PointerType Type of the child foster pointer.
 */
template <class PointerType>
class Fenster<string, PointerType>
{
public:

    using LengthType = uint16_t;

    /**
     * \brief Constructs object and encodes the given keys into the offload area.
     *
     * IMPORTANT: memory allocated for the object must be large enough to contain not only the
     * object (i.e., sizeof(*this)), but also the encoded data in the offload area.
     *
     * \see compute_size
     */
    Fenster(string* low, string* high, string* foster, PointerType foster_ptr)
        : foster_ptr(foster_ptr)
    {
        string empty_key {""};
        if (!low) { low = &empty_key; }
        if (!high) { high = &empty_key; }

        // Compute common prefix and subtract its length from all keys' lengths
        size_t prefix_len = get_common_prefix_length(*low, *high);
        assert<1>(!foster || foster->substr(0, prefix_len) == low->substr(0, prefix_len),
                "Foster key does not match the prefix of low- and high-fence keys");

        if (!foster) { foster = &empty_key; }

        this->prefix_len = prefix_len;
        low_fence_len = low->length() - prefix_len;
        high_fence_len = high->length() - prefix_len;

        foster_key_len = 0;
        if (foster->length() > 0) {
            foster_key_len = foster->length() - prefix_len;
        }

        // Finally copy key data into the offload area
        char* dest = get_data_offset();
        memcpy(dest, low->data(), prefix_len);
        dest += prefix_len;
        memcpy(dest, low->substr(prefix_len).data(), low_fence_len);
        dest += low_fence_len;
        memcpy(dest, high->substr(prefix_len).data(), high_fence_len);
        dest += high_fence_len;
        if (!foster->empty()) {
            memcpy(dest, foster->substr(prefix_len).data(), foster_key_len);
        }
    }

    /// \brief Returns total number of bytes occupied by object and encoded keys in the offload area
    size_t get_size() const
    {
        return sizeof(Fenster<string, PointerType>)
            + prefix_len + low_fence_len + high_fence_len + foster_key_len;
    }

    size_t get_prefix_size() { return prefix_len; }

    /// \brief Returns the number of bytes required to encode the given keys in a fenster object
    static size_t compute_size(string* low, string* high, string* foster)
    {
        string empty_key {""};
        if (!low) { low = &empty_key; }
        if (!high) { high = &empty_key; }
        if (!foster) { foster = &empty_key; }

        size_t prefix_len = get_common_prefix_length(*low, *high);
        size_t result = sizeof(Fenster<string, PointerType>) + low->length() + high->length()
            - prefix_len;

        // If foster child is not empty, foster key is also prefixed and size must be adjusted
        if (foster->length() > 0) {
            result += foster->length() - prefix_len;
        }

        return result;
    }

    /// \brief Computes the common prefix of two keys. Used for prefix compression.
    static size_t get_common_prefix_length(const string& a, const string& b)
    {
        size_t i = 0;
        while (i < a.length() && i < b.length() && a[i] == b[i]) { i++; }
        return i;
    }

    /**
     * \brief Decodes the keys into the given pointers.
     *
     * Keys are only decoded if the given pointer is not null, which means the method can also be
     * used to read only a subset of the keys (i.e., just the foster key).
     */
    void get_keys(string* low, string* high, string* foster) const
    {
        if (low) {
            low->assign(get_data_offset(), prefix_len + low_fence_len);
        }
        if (high) {
            // high_fence_len does not include prefix, so we need to reserve space in the string.
            high->reserve(prefix_len + high_fence_len);
            high->assign(get_data_offset(), prefix_len);
            high->append(get_data_offset() + prefix_len + low_fence_len, high_fence_len);
        }
        if (foster) {
            if (!is_foster_empty()) {
                foster->reserve(prefix_len + foster_key_len);
                foster->assign(get_data_offset(), prefix_len);
                foster->append(get_data_offset() + prefix_len + low_fence_len + high_fence_len,
                        foster_key_len);
            }
            else {
                // If foster child is empty, foster key == high key
                foster->reserve(prefix_len + high_fence_len);
                foster->assign(get_data_offset(), prefix_len);
                foster->append(get_data_offset() + prefix_len + low_fence_len, high_fence_len);
            }
        }
    }

    void get_prefix(string& prefix) const
    {
        prefix.assign(get_data_offset(), prefix_len);
    }

    PointerType get_foster_ptr () const { return foster_ptr; }

    /**
     * The infinity keys are encoded simply as empty strings. Note that the user can still use an
     * empty string as a valid key -- it does not affect the behavior of a B-tree with regard to
     * fence keys and foster relationships because it is the minimum key value. In other words, if
     * the low fence key is an empty string because it actually exists in a descendent node, then
     * the current node must be in the leftmost root-to-leaf path, where the low fence key would be
     * minus infinity anyway.
     *
     * An empty string key can also not be a foster key, except in a chain of empty nodes, in which
     * case the foster key would also be infinity.
     */
    bool is_low_key_infinity() const { return prefix_len + low_fence_len == 0; }
    bool is_high_key_infinity() const { return prefix_len + high_fence_len == 0; }
    // Foster key length can never be equal to the prefix, so prefix_len is not considered here
    bool is_foster_empty() const { return foster_key_len == 0; }

protected:

    LengthType prefix_len;
    LengthType low_fence_len;
    LengthType high_fence_len;
    LengthType foster_key_len;
    PointerType foster_ptr;

    /// \brief Return pointer to the offload area, where keys are encoded.
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
