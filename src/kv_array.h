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

#ifndef FOSTER_BTREE_KV_ARRAY_H
#define FOSTER_BTREE_KV_ARRAY_H

/**
 * \file kv_array.h
 *
 * A fixed-length array that encodes arbitrary key-value pairs and stores them in a slot array in
 * the appropriate position.
 */

#include <cstddef>
#include <iostream>

#include "exceptions.h"

namespace foster {

/**
 * \brief Provides ordered storage and search of key-value pairs in a slot array.
 *
 * This class builds upon the lower-level slot array class to provide higher-level key-value storage
 * and search functionality. Like the underlying slot array, it occupies a fixed amount of memory.
 * Using a given Encoder policy (passed as a template argument), arbitrary key-value pairs are
 * stored into the lower-level slot array. Using a given Search policy, key-value pairs are inserted
 * in the position the maintains order by key and search for a given key is supported.
 *
 * \tparam K Type of keys
 * \tparam V Type of values
 * \tparam SlotArray Class that implements the lower-level slot array (e.g., SlotArray).
 * \tparam Search Class that implements the search policy (e.g., BinarySearch).
 * \tparam Encoder Class that implements the encoding policy (e.g., DefaultEncoder).
 */
template <
    class K,
    class V,
    class SlotArray,
    class Search,
    class Encoder
>
class KeyValueArray : protected SlotArray
{
public:

    using KeyType = K;
    using ValueType = V;
    using EncoderType = Encoder;
    using SlotNumber = typename SlotArray::SlotNumber;
    using PayloadPtr = typename SlotArray::PayloadPtr;
    using PMNK_Type = typename SlotArray::KeyType;
    using ThisType = KeyValueArray<K, V, SlotArray, Search, Encoder>;

    static constexpr size_t Alignment = SlotArray::AlignmentSize;

    /**
     * \brief Insert a key-value pair into the array.
     */
    bool insert(const K& key, const V& value)
    {
        // 1. Insert key and allocate empty payload space for the pair
        SlotNumber slot;
        size_t payload_length = Encoder::get_payload_length(key, value);
        if (!insert_key(key, payload_length, slot)) {
            return false;
        }

        // 2. Encode (or serialize) the key-value pair into the payload.
        void* payload_addr = this->get_payload_for_slot(slot);
        Encoder::encode(key, value, payload_addr);

        return true;
    }

    /**
     * \brief Insert a slot with the given key and reserve the given amount of payload.
     */
    bool insert_key(const K& key, size_t payload_length, SlotNumber& slot)
    {
        // 1. Find slot into which to insert new pair.
        if (find_slot(key, nullptr, slot)) {
            throw ExistentKeyException<K>(key);
        }

        // 2. Allocate space in the slot array for the encoded payload.
        PayloadPtr payload;
        if (!this->allocate_payload(payload, payload_length)) {
            // No space left
            return false;
        }

        // 3. Insert slot with PMNK and pointer to the allocated payload.
        if (!this->insert_slot(slot)) {
            // No space left -- free previously allocated payload.
            this->free_payload(payload, payload_length);
            return false;
        }
        this->get_slot(slot).key = Encoder::get_pmnk(key);
        this->get_slot(slot).ptr = payload;
        this->get_slot(slot).ghost = false;

        return true;
    }

    /**
     * \brief Removes a key-value pair from the array.
     */
    void remove(const K& key)
    {
        // 1. Find slot containing the given key.
        SlotNumber slot;
        if (!find_slot(key, nullptr, slot)) {
            throw KeyNotFoundException<K>(key);
        }

        // 2. Free the payload and delete the slot.
        // TODO: set ghost bit and implement support for ghost records (e.g., in Search)
        PayloadPtr payload = this->get_slot(slot).payload;
        size_t payload_length = Encoder::get_payload_length(this->get_payload(payload));
        this->free_payload(payload, payload_length);

        this->delete_slot(slot);
    }

    /**
     * \brief Searches for a given key in the array.
     */
    bool find(const K& key, V* value)
    {
        SlotNumber slot;
        return find_slot(key, value, slot);
    }

protected:

    /**
     * \brief Internal implementation of the find method.
     *
     * Uses the search policy to locate the slot based on the PMNK. Then, it decodes the full key to
     * check against false positives -- if keys do not match, continue searching sequentially as
     * long as the PMNK matches.
     *
     * \param[in] key Key for which to search.
     * \param[in] value It not null, value data will be decoded into the pointed object.
     * \param[out] slot If key is found in the array, its position; otherwise, the position on which
     *      the given key would be inserted.
     * \returns true if key was found in the array; false otherwise.
     */
    // TODO parametrize comparison function
    bool find_slot(const K& key, V* value, SlotNumber& slot)
    {
        PMNK_Type pmnk = Encoder::get_pmnk(key);
        if (Search{}(*this, pmnk, slot, 0, this->slot_count())) {
            // Found poor man's normalized key -- now check if rest of the key matches
            PMNK_Type found_pmnk = pmnk;
            while (found_pmnk == pmnk) {
                K found_key;
                Encoder::decode(pmnk, &found_key, value, this->get_payload_for_slot(slot));

                if (found_key == key) {
                    return true;
                }
                else if (found_key > key) {
                    // Already passed the searched key
                    return false;
                }

                // Not a match. Try next slot, which might have the same pmnk.
                slot++;
                if (slot >= this->slot_count()) { return false; }
                found_pmnk = this->get_slot(slot).key;
            }
            return false;
        }
        else {
            // slot will contain the position into which key would be inserted
            return false;
        }
    }

#ifdef ENABLE_TESTING
public:

    /// Debugging/utility function to print the array's contents.
    void print(std::ostream& o)
    {
        using std::endl;

        for (SlotNumber i = 0; i < this->slot_count(); i++) {
            o << "Slot " << i << " [pmnk = " << this->get_slot(i).key << ", payload = "
                << this->get_slot(i).ptr << ", ghost = " << this->get_slot(i).ghost << "]"
                << endl;

            K key;
            V value;
            PMNK_Type pmnk = this->get_slot(i).key;
            Encoder::decode(pmnk, &key, &value, this->get_payload_for_slot(i));
            o << "\tk = " << key << ", v = " << value << endl;
        }
    }

    /// Debugging/testing function to verify if contents are sorted
    bool is_sorted()
    {
        PMNK_Type pmnk, prev_pmnk;
        K key, prev_key;

        for (SlotNumber i = 0; i < this->slot_count(); i++) {
            pmnk = this->get_slot(i).key;
            Encoder::decode(pmnk, &key, nullptr, this->get_payload_for_slot(i));

            if (i > 0) {
                if (pmnk < prev_pmnk) return false;
                if (key < prev_key) return false;
            }

            prev_pmnk = pmnk;
            prev_key = key;
        }

        return true;
    }

#endif // ENABLE_TESTING

};


} // namespace foster

#endif
