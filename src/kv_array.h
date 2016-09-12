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
#include "assertions.h"
#include "lrtype.h"
#include "dummies.h"

namespace foster {

/**
 * \brief Provides ordered storage and search of key-value pairs in a slot array.
 *
 * This class builds upon the lower-level slot array class to provide higher-level key-value storage
 * and search functionality. Like the underlying slot array, it occupies a fixed amount of memory.
 * Using a given Encoder policy (passed as a template argument), arbitrary key-value pairs are
 * stored into the lower-level slot array. Using a given Search policy, key-value pairs are inserted
 * in the position that maintains order by key and search for a given key is supported.
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
    class Encoder,
    class Logger = DummyLogger,
    bool Sorted = true
>
class KeyValueArray : public SlotArray, public Logger
{
public:

    /**
     * Type aliases for convenience and allowing other classes to use this one's template arguments.
     */
    using KeyType = K;
    using ValueType = V;
    using EncoderType = Encoder;
    using LoggerType = Logger;
    using SlotNumber = typename SlotArray::SlotNumber;
    using PayloadPtr = typename SlotArray::PayloadPtr;
    using PMNK_Type = typename SlotArray::KeyType;
    using ThisType = KeyValueArray<K, V, SlotArray, Search, Encoder, Logger, Sorted>;

    static constexpr size_t Alignment = SlotArray::AlignmentSize;

    /**
     * \brief Insert a key-value pair into the array.
     * \returns true if insertion succeeded (i.e., if there was enough free space)
     */
    bool insert(const K& key, const V& value, bool logit = true)
    {
        // 1. Insert key and allocate empty payload space for the pair
        SlotNumber slot {0};
        size_t payload_length = Encoder::get_payload_length(key, value);
        if (!insert_key(key, payload_length, slot)) {
            return false;
        }

        // 2. Encode (or serialize) the key-value pair into the payload.
        void* payload_addr = this->get_payload_for_slot(slot);
        Encoder::encode(payload_addr, key, value);
        // TODO in a profile run with debug level 0, the call below is still registered!
        assert<3>(!Sorted || is_sorted());

        if (logit) { this->log(LRType::Insert, key, value); }

        return true;
    }

    /**
     * \brief Insert a slot with the given key and reserve the given amount of payload.
     *
     * This method is a building block of the main insert method that does all steps of an insertion
     * except for encoding the value into the reserved payload area.
     *
     * \returns true if insertion succeeded (i.e., if there was enough free space)
     */
    bool insert_key(const K& key, size_t payload_length, SlotNumber& slot)
    {
        // 1. Find slot into which to insert new pair.
        if (Sorted && find_slot(key, nullptr, slot)) {
            throw ExistentKeyException<K>(key);
        }
        else if (!Sorted) {
            slot = this->slot_count();
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
     * \throws KeyNotFoundException if key does not exist in the array.
     */
    template <bool MustExist = true>
    bool remove(const K& key, bool logit = true)
    {
        // 1. Find slot containing the given key.
        SlotNumber slot;
        if (!find_slot(key, nullptr, slot)) {
            if (MustExist) { throw KeyNotFoundException<K>(key); }
            else { return false; }
        }

        // 2. Free the payload and delete the slot.
        PayloadPtr payload = this->get_slot(slot).ptr;
        size_t payload_length = Encoder::get_payload_length(this->get_payload(payload));
        this->free_payload(payload, payload_length);
        this->delete_slot(slot);

        if (logit) { this->log(LRType::Remove, key); }

        return true;
    }

    void truncate_keys(size_t length)
    {
        static_assert(std::is_same<K, string>::value,
                "Methor truncate_keys is only available for string keys");

        if (length == 0) { return; }

        K key;
        V value;
        SlotNumber i = 0;
        while (i < this->slot_count()) {
            auto& slot = this->get_slot(i);
            Encoder::decode(this->get_payload(slot.ptr), &key, &value, &slot.key);

            void* payload_addr = this->get_payload_for_slot(i);
            size_t old_len = this->get_payload_count(Encoder::get_payload_length(payload_addr));

            // truncate first 'length' characters of key and update PMNK
            key.erase(0, length);
            slot.key = Encoder::get_pmnk(key);

            // update payload with truncated key
            Encoder::encode(payload_addr, key, value);
            size_t new_len = this->get_payload_count(Encoder::get_payload_length(payload_addr));

            // if truncated key occupies less payload blocks, shift accordingly
            assert<1>(new_len <= old_len);
            if (new_len < old_len) {
                PayloadPtr p_end = slot.ptr + new_len;
                this->free_payload(p_end, length);
            }

            i++;
        }
    }

    /**
     * \brief Searches for a given key in the array.
     * \see find_slot
     */
    bool find(const K& key, V* value = nullptr)
    {
        SlotNumber slot {0};
        return find_slot(key, value, slot);
    }

    /// \brief Number of key-value pairs currently present in the array.
    size_t size()
    {
        return this->slot_count();
    }

    size_t get_payload_length(SlotNumber s)
    {
        auto& slot = this->get_slot(s);
        return Encoder::get_payload_length(this->get_payload(slot.ptr));
    }

    /// \brief Decodes key and value associated with a given slot number
    void read_slot(SlotNumber s, K* key, V* value)
    {
        auto& slot = this->get_slot(s);
        Encoder::decode(this->get_payload(slot.ptr), key, value, &slot.key);
    }

    /**
     * \brief Simple iterator class to support sequentially reading all key-value pairs
     */
    class Iterator
    {
    public:
        Iterator(ThisType* kv) :
            current_slot_{0}, kv_{kv}
        {}

        bool next(K* key, V* value)
        {
            if (current_slot_ >= kv_->size()) { return false; }

            kv_->read_slot(current_slot_, key, value);
            current_slot_++;

            return true;
        }

    private:
        SlotNumber current_slot_;
        ThisType* kv_;
    };

    /// \brief Yields an iterator instance to sequentially read all key-value pairs
    Iterator iterate()
    {
        return Iterator{this};
    }

    using SortedType = KeyValueArray<K, V, SlotArray, Search, Encoder, Logger, true>;
    SortedType* convert_to_sorted()
    {
        // if (Sorted) { return this; }
        this->sort_slots();
        return reinterpret_cast<SortedType*>(this);
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
     * \param[in] value It not null, value data will be decoded into the pointed object. If key is
     *      not found, value of the previous slot is decoded. This supports traversal through branch
     *      nodes in a B-tree.
     * \param[out] slot If key is found in the array, its position; otherwise, the position on which
     *      the given key would be inserted.
     * \returns true if key was found in the array; false otherwise.
     */
    // TODO parametrize comparison function
    bool find_slot(const K& key, V* value, SlotNumber& slot)
    {
        if (!Sorted) { return find_slot_unsorted(key, value, slot); }

        PMNK_Type pmnk = Encoder::get_pmnk(key);
        if (Search{}(*this, pmnk, slot, 0, this->slot_count())) {
            // Found poor man's normalized key -- now check if rest of the key matches
            PMNK_Type found_pmnk = pmnk;
            while (found_pmnk == pmnk) {
                K found_key;
                Encoder::decode(this->get_payload_for_slot(slot), &found_key, value, &pmnk);

                if (found_key == key) {
                    return true;
                }
                else if (found_key > key) {
                    // Already passed the searched key
                    break;
                }

                // Not a match. Try next slot, which might have the same pmnk.
                slot++;
                if (slot >= this->slot_count()) { break; }
                found_pmnk = this->get_slot(slot).key;
            }
        }

        // Key was not found; slot will contain the position into which key would be inserted and
        // the value pointer, if a valid one is given, will contain whatever value was found in that
        // slot -- provided that the array contains at least one slot. This is useful for traversal
        // of branch nodes in a B-tree (\see BtreeLevel::traverse).
        if (value && slot > 0) {
            assert<1>(slot <= this->slot_count());
            // Slot contains a key value greater than or equal to the searched key. In the former
            // case, we must return the value of the previous slot for correct traversal. The latter
            // case cannot occur here, because we would have returned true above.
            slot--;
            Encoder::decode(this->get_payload_for_slot(slot), nullptr, value, nullptr);
        }
        return false;
    }

    bool find_slot_unsorted(const K& key, V* value, SlotNumber& slot)
    {
        PMNK_Type pmnk = Encoder::get_pmnk(key);
        K found_key;

        // Simple linear search -- return one past the last slot if not found (append)
        for (SlotNumber i = 0; i < this->slot_count(); i++) {
            if (this->get_slot(i).key == pmnk) {
                Encoder::decode(this->get_payload_for_slot(i), &found_key, value, &pmnk);
                if (found_key == key) {
                    slot = i;
                    return true;
                }
            }
        }

        slot = this->slot_count();
        return false;
    }

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
            Encoder::decode(this->get_payload_for_slot(i), &key, &value, &pmnk);
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
            Encoder::decode(this->get_payload_for_slot(i), &key, nullptr, &pmnk);

            if (i > 0) {
                if (pmnk < prev_pmnk) return false;
                if (key < prev_key) return false;
            }

            prev_pmnk = pmnk;
            prev_key = key;
        }

        return true;
    }

};

} // namespace foster

#endif
