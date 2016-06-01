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

#include "slot_array.h"
#include "search.h"
#include "encoding.h"
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
 * \tparam ArrayBytes Size to be occupied by the array in memory.
 * \tparam Alignment Internal alignment with which entries are stored (\see SlotArray).
 * \tparam SArray Class that implements the lower-level slot array (default SlotArray).
 * \tparam Search Class that implements the search policy (default BinarySearch).
 * \tparam Encoder Class that implements the encoding policy (default StatelessEncoder).
 */
template <
    class K,
    class V,
    class PMNK_Type,
    size_t ArrayBytes,
    size_t Alignment = ArrayBytes / 1024,
    template <class, size_t, size_t> class SArray = SlotArray,
    template <class> class Search = BinarySearch,
    template <class, class, class> class Encoder = StatelessEncoder
>
class KeyValueArray :
    protected Encoder<K, V, PMNK_Type>,
    protected SArray<PMNK_Type, ArrayBytes, Alignment>
{
public:

    using SlotNumber = typename SArray<PMNK_Type, ArrayBytes, Alignment>::SlotNumber;
    using PayloadPtr = typename SArray<PMNK_Type, ArrayBytes, Alignment>::PayloadPtr;

    /**
     * \brief Insert a key-value pair into the array.
     */
    bool insert(const K& key, const V& value)
    {
        // 1. Find slot into which to insert new pair.
        SlotNumber slot;
        if (find_slot(key, nullptr, slot)) {
            throw ExistentKeyException<K>(key);
        }

        // 2. Allocate space in the slot array for the encoded payload.
        PayloadPtr payload;
        size_t payload_length = this->get_payload_length(key, value);
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
        (*this)[slot].key = this->get_pmnk(key);
        (*this)[slot].ptr = payload;
        (*this)[slot].ghost = false;

        // 4. Encode (or serialize) the key-value pair into the payload.
        this->encode(key, value, this->get_payload(payload));

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
        PayloadPtr payload = (*this)[slot].payload;
        size_t payload_length = this->get_payload_length(this->get_payload(payload));
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

    /**
     * Debugging/utility function to print the array's contents.
     */
    void print(std::ostream& o)
    {
        using std::endl;

        for (SlotNumber i = 0; i < this->slot_count(); i++) {
            o << "Slot " << i << " [pmnk = " << (*this)[i].key << ", payload = " << (*this)[i].ptr
                << ", ghost = " << (*this)[i].ghost << "]" << endl;

            K key;
            V value;
            this->decode(key, &value, this->get_payload((*this)[i].ptr));
            o << "\tk = " << key << ", v = " << value << endl;
        }
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
        Search<SArray<PMNK_Type, ArrayBytes, Alignment>> search_func;

        PMNK_Type pmnk = this->get_pmnk(key);
        if (search_func(*this, pmnk, slot, 0, this->slot_count())) {
            // Found poor man's normalized key -- now check if rest of the key matches
            PMNK_Type found_pmnk = pmnk;
            while (found_pmnk == pmnk) {
                K found_key;
                this->decode(found_key, value, this->get_payload((*this)[slot].ptr));

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
                found_pmnk = (*this)[slot].key;
            }
            return false;
        }
        else {
            // slot will contain the position into which key would be inserted
            return false;
        }
    }

};


} // namespace foster

#endif
