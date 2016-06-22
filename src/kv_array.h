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

namespace foster {

// Forward declaration for friend function declaration below
namespace internal {
    template<class T, class S>
    bool move_kv_records(T&, S, T&, S, size_t);
}

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
    class Encoder
>
class KeyValueArray : protected SlotArray
{
public:

    /**
     * Type aliases for convenience and allowing other classes to use this one's template arguments.
     */
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
     * \returns true if insertion succeeded (i.e., if there was enough free space)
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
        // TODO in a profile run with debug level 0, the call below is still registered!
        assert<3>(is_sorted());

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
     * \throws KeyNotFoundException if key does not exist in the array.
     */
    void remove(const K& key)
    {
        // 1. Find slot containing the given key.
        SlotNumber slot;
        if (!find_slot(key, nullptr, slot)) {
            throw KeyNotFoundException<K>(key);
        }

        // 2. Free the payload and delete the slot.
        PayloadPtr payload = this->get_slot(slot).ptr;
        size_t payload_length = Encoder::get_payload_length(this->get_payload(payload));
        this->free_payload(payload, payload_length);

        this->delete_slot(slot);
    }

    /**
     * \brief Searches for a given key in the array.
     * \see find_slot
     */
    bool find(const K& key, V* value = nullptr)
    {
        SlotNumber slot;
        return find_slot(key, value, slot);
    }

    /// \brief Number of key-value pairs currently present in the array.
    size_t size()
    {
        return this->slot_count();
    }

    class Iterator
    {
    public:
        Iterator(ThisType* kv) :
            current_slot_{0}, kv_{kv}
        {}

        bool next(K* key, V* value)
        {
            if (current_slot_ >= kv_->size()) { return false; }

            typename SlotArray::Slot& slot = kv_->get_slot(current_slot_);
            Encoder::decode(kv_->get_payload(slot.ptr), key, value, &slot.key);
            current_slot_++;

            return true;
        }

    private:
        SlotNumber current_slot_;
        ThisType* kv_;
    };

    Iterator iterate()
    {
        return Iterator{this};
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
                    return false;
                }

                // Not a match. Try next slot, which might have the same pmnk.
                slot++;
                if (slot >= this->slot_count()) { return false; }
                found_pmnk = this->get_slot(slot).key;
            }
        }

        // Key was not found; slot will contain the position into which key would be inserted and
        // the value pointer, if a valid one is given, will contain whatever value was found in that
        // slot -- provided that the array contains at least one slot. This is useful for traversal
        // of branch nodes in a B-tree (\see BtreeLevel::traverse).
        if (value && this->slot_count() > 0) {
            assert<1>(slot <= this->slot_count());
            // Slot contains a key value greater than or equal to the searched key. In the former
            // case, we must return the value of the previous slot for correct traversal. The latter
            // case cannot occur here, because we would have returned true above.
            slot--;
            Encoder::decode(this->get_payload_for_slot(slot), nullptr, value, nullptr);
        }
        return false;
    }

    /*
     * Friend declaration of the move_kv_records. It's currently implemented as an external friend
     * function instead of a method just to achieve some separation and keep the class itself
     * simple. The function is templated to support different array implementations. Think of this
     * like the STL sort function, which works on arbitrary data structures and is a separate
     * function.
     */
    template<class T, class S>
    friend bool internal::move_kv_records(T&, S, T&, S, size_t);

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

namespace internal {

/**
 * \brief Function template to move key-value pairs between arrays.
 *
 * \param[in] dest Key-value array into which pairs will be moved.
 * \param[in] dest_slot Slot position in which pairs will be inserted in the destination array.
 * \param[in] src Source array from which pairs will be moved.
 * \param[in] src_slot First slot to be moved in the source array.
 * \param[in] slot_count Number of slots to move
 * \returns true if movement succeeds; false otherwise
 *
 * One important aspect of this function is that the movement is atomic; i.e., if any of the
 * individual movements fails (possibliy due to lack of free space on destination) then the whole
 * movement is "rolled back" by reinserting the already-moved pairs into the source array.
 */
template <class KVArray, class SlotNumber = typename KVArray::SlotNumber>
bool move_kv_records(
        KVArray& dest, SlotNumber dest_slot,
        KVArray& src, SlotNumber src_slot,
        size_t slot_count)
{
    using Encoder = typename KVArray::EncoderType;
    using PayloadPtr = typename KVArray::PayloadPtr;

    SlotNumber last_slot = src_slot + slot_count - 1;
    assert<1>(last_slot < src.slot_count());
    assert<1>(src.is_sorted());

    bool success = true;
    SlotNumber i = src_slot, j = dest_slot;

    // First copy slots from the source array into the destination one. If an insertion or payload
    // allocation fails in the destination, we break out of the loop and success is set to false.
    while (i <= last_slot) {
        // 1. Insert slot
        success = dest.insert_slot(j);
        if (!success) { break; }

        // 2. Allocate payload
        void* payload_src = src.get_payload_for_slot(i);
        size_t length = Encoder::get_payload_length(payload_src);

        PayloadPtr payload_dest_ptr;
        success = dest.allocate_payload(payload_dest_ptr, length);
        if (!success) {
            dest.delete_slot(j);
            break;
        }

        // 3. Copy slot and payload data into the reserved space.
        // TODO add Slot::init or reset or something like that
        dest.get_slot(j).key = src.get_slot(i).key;
        dest.get_slot(j).ptr = payload_dest_ptr;
        dest.get_slot(j).ghost = src.get_slot(i).ghost;

        memcpy(dest.get_payload(payload_dest_ptr), payload_src, length);

        i++;
        j++;
    }

    // If copy failed above, we need to roll back by removing the entries that were already copied
    if (!success) {
        while (j > dest_slot) {
            j--;
            dest.free_payload(dest.get_slot(j).ptr,
                    Encoder::get_payload_length(dest.get_payload_for_slot(j)));
            dest.delete_slot(j);
        }
    }
    // Otherwise, we delete the copied slots on the source array, thus completing the move operation
    else {
        assert<1>(i == last_slot + 1);
        while (i > src_slot) {
            i--;
            src.free_payload(src.get_slot(i).ptr,
                    Encoder::get_payload_length(src.get_payload_for_slot(i)));
            src.delete_slot(i);
        }
    }

    assert<1>(dest.is_sorted());
    assert<1>(src.is_sorted());

    return success;
}

} // namespace internal

} // namespace foster

#endif
