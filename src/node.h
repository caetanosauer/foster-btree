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

#ifndef FOSTER_BTREE_NODE_H
#define FOSTER_BTREE_NODE_H

#include <cstddef>
#include <type_traits>

#include "exceptions.h"
#include "assertions.h"
#include "lrtype.h"
#include "adoption.h"

namespace foster {

template <
    class K,
    class V,
    class Search,
    template <class, class> class Encoder,
    class Logger = void,
    bool Sorted = true
>
class Node
{
public:

    using KeyType = K;
    using ValueType = V;
    using LoggerType = Logger;
    using ThisType = Node<K, V, Search, Encoder, Logger, Sorted>;
    using EncoderType = Encoder<K, V>;

    template <typename N>
    using SlotNumber = typename N::PointeeType::SlotNumber;
    template <typename N>
    using PayloadPtr = typename N::PointeeType::PayloadPtr;

    static constexpr bool LoggingOn = !std::is_void<Logger>::value;

    template <typename N>
    static void initialize(N) {}

    /**
     * \brief Insert a key-value pair into the array.
     * \returns true if insertion succeeded (i.e., if there was enough free space)
     */
    template <typename N>
    static bool insert(N node, const K& key, const V& value, bool logit = true)
    {
        // 1. Insert key and allocate empty payload space for the pair
        SlotNumber<N> slot {0};
        size_t payload_length = Encoder<K,V>::get_payload_length(key, value);
        if (!insert_key(node, key, payload_length, slot)) {
            return false;
        }

        // 2. Encode (or serialize) the key-value pair into the payload.
        void* payload_addr = node->get_payload_for_slot(slot);
        Encoder<K,V>::encode(payload_addr, key, value);
        // TODO in a profile run with debug level 0, the call below is still registered!
        // assert<3>(!Sorted || is_sorted());

        if (logit) { log(node, LRType::Insert, key, value); }

        return true;
    }

    /**
     * '&& sizeof...(Args)' is just required to make enable_if work, because it must depend on all
     * template arguments
     */
    template <typename N, typename... Args>
    static meta::EnableIf<LoggingOn && sizeof...(Args) && sizeof(N)>
        log(N node, const Args&... args)
    {
        node->log(args...);
    }

    template <typename... Args>
    static meta::EnableIf<!(LoggingOn && sizeof...(Args))>
        log(const Args&...) {}


    /**
     * \brief Insert a slot with the given key and reserve the given amount of payload.
     *
     * This method is a building block of the main insert method that does all steps of an insertion
     * except for encoding the value into the reserved payload area.
     *
     * \returns true if insertion succeeded (i.e., if there was enough free space)
     */
    template <typename N>
    static bool insert_key(N node, const K& key, size_t payload_length,
            SlotNumber<N>& slot)
    {
        // 1. Find slot into which to insert new pair.
        if (Sorted && find_slot(node, key, slot)) {
            throw ExistentKeyException<K>(key);
        }
        else if (!Sorted) {
            slot = node->slot_count();
        }

        // 2. Allocate space in the slot array for the encoded payload.
        PayloadPtr<N> payload;
        if (!node->allocate_payload(payload, payload_length)) {
            // No space left
            return false;
        }

        // 3. Insert slot with PMNK and pointer to the allocated payload.
        if (!node->insert_slot(slot)) {
            // No space left -- free previously allocated payload.
            node->free_payload(payload, payload_length);
            return false;
        }
        node->get_slot(slot) = { Encoder<K,V>::get_pmnk(key), payload, false };

        return true;
    }

    /**
     * \brief Removes a key-value pair from the array.
     * \throws KeyNotFoundException if key does not exist in the array.
     */
    template <typename N>
    static bool remove(N node, const K& key, bool must_exist = true, bool logit = true)
    {
        // 1. Find slot containing the given key.
        SlotNumber<N> slot;
        if (!find_slot(node, key, slot)) {
            if (must_exist) { throw KeyNotFoundException<K>(key); }
            else { return false; }
        }

        // 2. Free the payload and delete the slot.
        PayloadPtr<N> payload = node->get_slot(slot).ptr;
        size_t payload_length = Encoder<K,V>::get_payload_length(node->get_payload(payload));
        node->free_payload(payload, payload_length);
        node->delete_slot(slot);

        if (logit) { log(node, LRType::Remove, key); }

        return true;
    }

    template <typename N>
    static void truncate_keys(N node, size_t length)
    {
        static_assert(std::is_same<K, string>::value,
                "Method truncate_keys is only available for string keys");

        if (length == 0) { return; }

        K key;
        V value;
        SlotNumber<N> i = 0;
        while (i < node->slot_count()) {
            auto& slot = node->get_slot(i);
            Encoder<K,V>::decode(node->get_payload(slot.ptr), &key, &value, &slot.key);

            void* payload_addr = node->get_payload_for_slot(i);
            size_t old_len = node->get_payload_count(Encoder<K,V>::get_payload_length(payload_addr));

            // truncate first 'length' characters of key and update PMNK
            key.erase(0, length);
            slot.key = Encoder<K,V>::get_pmnk(key);

            // update payload with truncated key
            Encoder<K,V>::encode(payload_addr, key, value);
            size_t new_len = node->get_payload_count(Encoder<K,V>::get_payload_length(payload_addr));

            // if truncated key occupies less payload blocks, shift accordingly
            assert<1>(new_len <= old_len);
            if (new_len < old_len) {
                PayloadPtr<N> p_end = slot.ptr + new_len;
                node->free_payload(p_end, length);
            }

            i++;
        }
    }

    /**
     * \brief Searches for a given key in the array.
     * \see find_slot
     */
    template <typename N>
    static bool find(N node, const K& key)
    {
        SlotNumber<N> slot {0};
        return find_slot<N>(node, key, slot);
    }

    template <typename N>
    static bool find(N node, const K& key, V& value)
    {
        SlotNumber<N> slot {0};
        char* value_addr;
        bool ret = find_slot(node, key, slot, &value_addr);
        Encoder<K, V>::decode_value(value_addr, &value);
        return ret;
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
    template <typename N>
    static bool find_slot(N node, const K& key, SlotNumber<N>& slot,
            char** value_addr = nullptr)
    {
        if (!Sorted) { return find_slot_unsorted(node, key, slot, value_addr); }

        auto pmnk = Encoder<K,V>::get_pmnk(key);
        if (Search{}(*node, pmnk, slot, 0, node->slot_count())) {
            // Found poor man's normalized key -- now check if rest of the key matches
            auto found_pmnk = pmnk;
            while (found_pmnk == pmnk) {
                K found_key;
                auto payload = node->get_payload_for_slot(slot);
                char* vaddr = Encoder<K,V>::decode_key(payload, &found_key, &pmnk);
                if (value_addr) { *value_addr = vaddr; }

                if (found_key == key) {
                    return true;
                }
                else if (found_key > key) {
                    // Already passed the searched key
                    break;
                }

                // Not a match. Try next slot, which might have the same pmnk.
                slot++;
                if (slot >= node->slot_count()) { break; }
                found_pmnk = node->get_slot(slot).key;
            }
        }

        // Key was not found; slot will contain the position into which key would be inserted and
        // the value pointer, if a valid one is given, will contain whatever value was found in that
        // slot -- provided that the array contains at least one slot. This is useful for traversal
        // of branch nodes in a B-tree (\see BtreeLevel::traverse).
        if (value_addr && slot > 0) {
            assert<1>(slot <= node->slot_count());
            // Slot contains a key value greater than or equal to the searched key. In the former
            // case, we must return the value of the previous slot for correct traversal. The latter
            // case cannot occur here, because we would have returned true above.
            slot--;
            auto payload = node->get_payload_for_slot(slot);
            char* vaddr = Encoder<K,V>::decode_key(payload, nullptr, nullptr);
            if (value_addr) { *value_addr = vaddr; }
        }
        return false;
    }

    template <typename N>
    static bool find_slot_unsorted(N node, const K& key, SlotNumber<N>& slot, char** value_addr)
    {
        auto pmnk = Encoder<K,V>::get_pmnk(key);
        K found_key;
        char* ret;

        // Simple linear search -- return one past the last slot if not found (append)
        for (SlotNumber<N> i = 0; i < node->slot_count(); i++) {
            if (node->get_slot(i).key == pmnk) {
                auto payload = node->get_payload_for_slot(i);
                ret = Encoder<K,V>::decode_key(payload, &found_key, &pmnk);
                if (value_addr) { *value_addr = ret; }
                if (found_key == key) {
                    slot = i;
                    return true;
                }
            }
        }

        slot = node->slot_count();
        return false;
    }

public:

    template <typename N>
    static size_t get_payload_length(N node, SlotNumber<N> s)
    {
        const auto& slot = node->get_slot(s);
        return Encoder<K,V>::get_payload_length(node->get_payload(slot.ptr));
    }

    /// \brief Decodes key and value associated with a given slot number
    template <typename N>
    static void read_slot(N node, SlotNumber<N> s, K* key, V* value)
    {
        const auto& slot = node->get_slot(s);
        Encoder<K,V>::decode(node->get_payload(slot.ptr), key, value, &slot.key);
    }

    /**
     * \brief Simple iterator class to support sequentially reading all key-value pairs
     */
    template <typename N>
    class Iterator
    {
    public:
        Iterator(N node) :
            current_slot_{0}, node_{node}
        {}

        bool next(K* key, V* value)
        {
            if (current_slot_ >= node_->slot_count()) { return false; }

            read_slot(node_, current_slot_, key, value);
            current_slot_++;

            return true;
        }

    private:
        SlotNumber<N> current_slot_;
        N node_;
    };

    /// \brief Yields an iterator instance to sequentially read all key-value pairs
    template <typename N>
    static Iterator<N> iterate(N node)
    {
        return Iterator<N>{node};
    }

    /// Debugging/utility function to print the array's contents.
    template <typename N>
    static void print(N node, std::ostream& o)
    {
        using std::endl;

        for (SlotNumber<N> i = 0; i < node->slot_count(); i++) {
            o << "\tSlot " << i << " [pmnk = " << node->get_slot(i).key << ", payload = "
                << node->get_slot(i).ptr << ", ghost = " << node->get_slot(i).ghost << "]";
            // << endl;

            K key;
            V value;
            auto pmnk = node->get_slot(i).key;
            Encoder<K,V>::decode(node->get_payload_for_slot(i), &key, &value, &pmnk);
            o << "\tk = " << key << ", v = " << value << endl;
        }
    }

    /// Debugging/testing function to verify if contents are sorted
    template <typename N>
    static bool is_sorted(N node)
    {
        K key, prev_key;
        auto prev_pmnk = node->get_slot(0).key;

        for (SlotNumber<N> i = 0; i < node->slot_count(); i++) {
            auto pmnk = node->get_slot(i).key;
            Encoder<K,V>::decode(node->get_payload_for_slot(i), &key, nullptr, &pmnk);

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
