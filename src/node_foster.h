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

#ifndef FOSTER_BTREE_FOSTER_NODE_H
#define FOSTER_BTREE_FOSTER_NODE_H

#include <array>

#include "exceptions.h"
#include "assertions.h"
#include "lrtype.h"
#include "move_records.h"

namespace foster {

class FosterNodePayloads
{
public:

    // We assume that PayloadPtr is never larger than 32 bits. This is necessary because the
    // tpye alias SoltArray::PayloadPtr is not available here.
    using PayloadPtr = uint32_t;

    FosterNodePayloads()
    {
        foster_payloads_.fill(0);
        foster_payload_valid_.fill(false);
    }

    static constexpr unsigned LowKey = 0;
    static constexpr unsigned HighKey = 1;
    static constexpr unsigned FosterKey = 2;
    static constexpr unsigned FosterPtr = 3;
    static constexpr unsigned FieldCount = 4;

    PayloadPtr get_foster_field(unsigned field) const
    {
        return foster_payloads_[field];
    }

    bool is_valid_foster_field(unsigned field) const
    {
        return foster_payload_valid_[field];
    }

    template <typename PayloadPtr>
    void set_foster_field(unsigned field, PayloadPtr p)
    {
        foster_payloads_[field] = p;
        foster_payload_valid_[field] = true;
    }

    void unset_foster_field(unsigned field)
    {
        foster_payload_valid_[field] = false;
    }

    PayloadPtr get_foster_field_payload(unsigned field)
    {
        return foster_payloads_[field];
    }

    void shift_foster_payloads(unsigned field, int shift)
    {
        auto max_to_move = foster_payloads_[field];
        for (unsigned f = 0; f < FieldCount; f++) {
            if (foster_payload_valid_[f] && foster_payloads_[f] <= max_to_move) {
                foster_payloads_[f] += shift;
            }
        }
    }

    void shift_all_foster_payloads(int shift)
    {
        for (unsigned f = 0; f < FieldCount; f++) {
            if (foster_payload_valid_[f]) {
                foster_payloads_[f] += shift;
            }
        }
    }

protected:

    std::array<PayloadPtr, FieldCount> foster_payloads_;
    std::array<bool, FieldCount> foster_payload_valid_;
};

template <
    class BaseNode,
    template <typename> class NodePointer,
    template <typename> class Encoder
>
class FosterNode : public BaseNode
{
public:

    using ThisType = FosterNode<BaseNode, NodePointer, Encoder>;
    using KeyType = typename BaseNode::KeyType;

    template <typename N, typename T>
    static bool get_foster_child(const N& node, T& value)
    {
        bool ret = get_foster_field(node, FosterNodePayloads::FosterPtr, value);
        // foster child pointer is special case: we must return false if it exists but is null
        return ret && value;
    }

    template <typename N, typename T>
    static bool get_low_key(const N& node, T& value)
    {
        return get_foster_field(node, FosterNodePayloads::LowKey, value);
    }

    template <typename N>
    static bool is_low_key_infinity(const N& node)
    {
        return !is_valid_foster_field(node, FosterNodePayloads::LowKey);
    }

    template <typename N, typename T>
    static bool get_high_key(const N& node, T& value)
    {
        return get_foster_field(node, FosterNodePayloads::HighKey, value);
    }

    template <typename N>
    static bool is_high_key_infinity(const N& node)
    {
        return !is_valid_foster_field(node, FosterNodePayloads::HighKey);
    }

    template <typename N, typename T>
    static bool get_foster_key(const N& node, T& value)
    {
        return get_foster_field(node, FosterNodePayloads::FosterKey, value);
    }

    /// \brief Checks if given key is within the fence borders
    template <typename N>
    static bool fence_contains(const N& node, const KeyType& key)
    {
        KeyType low, high;
        bool low_finite = get_low_key(node, low);
        bool high_finite = get_high_key(node, high);

        if (low_finite && high_finite && key >= low && key <= high) { return true; }

        // comparison failed, but key may still be contained if one of the keys is infinity
        if (key >= low && !high_finite) { return true; }
        if (key <= high && !low_finite) { return true; }
        if (!low_finite && !high_finite) { return true; }

        return false;
    }

    /**
     * \brief Checks if given key is contained in this node.
     *
     * This is stricter than fence_contains, because it returns false if the key is not in this node
     * but in its foster child, whereas the former returns true.
     */
    template <typename N>
    static bool key_range_contains(const N& node, const KeyType& key)
    {
        if (!fence_contains(node, key)) { return false; }
        NodePointer<N> foster;
        if (get_foster_child(node, foster)) {
            KeyType foster_key;
            bool foster_finite = get_foster_key(node, foster_key);
            return !foster_finite || key < foster_key;
        }
        return true;
    }

    template <typename N>
    static void split(N& node, NodePointer<N> new_node)
    {
        add_foster_child(node, new_node);
        rebalance(node);
    }

    template <typename N, typename Ptr>
    static void add_foster_child(N& node, const Ptr& ptr)
    {
        assert<1>(ptr, "Invalid node pointer in set_foster_child");
        auto child = *ptr;
        bool success = true;

        // Foster key is equal to high key of parent, which is equal to both fence keys on child
        KeyType low_key, high_key, foster_key;
        Ptr foster_ptr;
        if (get_high_key(node, high_key)) {
            success = set_low_key(child, high_key);
            assert<1>(success, "Could not add foster child");
            success = set_high_key(child, high_key);
            assert<1>(success, "Could not add foster child");
        }
        if (get_high_key(node, foster_key)) {
            success = set_foster_key(child, foster_key);
            assert<1>(success, "Could not add foster child");
        }
        if (get_foster_child(node, foster_ptr)) {
            success = set_foster_child(child, foster_ptr);
            assert<1>(success, "Could not add foster child");
        }

        // A newly inserted foster child is always empty, which means the foster key is the same as
        // the high key. In that case, we use an empty foster key. This allows adding an empty
        // foster child without having to allocate more space for the foster key. This way, we make
        // sure that an empty foster child can always be added to an overflown node. For this to
        // work, however, we must make sure that there's at least space for a foster child pointer;
        // this is done in the initialize method.
        success = set_foster_child(node, ptr);
        assert<0>(success, "Could not add foster child");
    }

    template <typename N>
    static void rebalance(N& node, bool logit = true)
    {
        using SlotNumber = typename N::SlotNumber;
        // TODO support different policies for picking the split key
        // (pick middle key, divide by total payload size, suffix compression, etc.)
        // e.g., template <class RebalancePolicy = void>

        // TODO support merge and node deletion, i.e., rebalance in the opposite direction

        NodePointer<N> foster_ptr;
        get_foster_child(node, foster_ptr);
        N& child = *foster_ptr;

        // STEP 1: determine split key
        SlotNumber slot_count = node.slot_count();
        SlotNumber split_slot = slot_count / 2;
        assert<1>(split_slot > 0);

        KeyType split_key;
        BaseNode::read_slot(node, split_slot, &split_key, nullptr);

        // TODO for redo, key should be an argument of the method
        // TODO there should be an argument for "direction" of rebalance
        if (logit) { BaseNode::log(node, LRType::Rebalance, split_key); }

        // STEP 2: move records
        bool moved = RecordMover<typename BaseNode::EncoderType>::move_records
            (child, SlotNumber{0}, node, split_slot, slot_count - split_slot);
        assert<1>(moved, "Could not move records in node rebalance");

        // STEP 3: Adjust foster key
        bool success = set_foster_key(node, split_key);;
        assert<1>(success, "Could not update foster key of foster parent in node rebalance");

        // STEP 4: Adjust fence keys on foster child
        success = set_low_key(child, split_key);
        assert<1>(success, "Could not update low fence key of foster child in node rebalance");
        KeyType high_key;
        if (get_high_key(node, high_key)) {
            success = set_high_key(child, high_key);
            assert<1>(success, "Could not update high fence key of foster child in node rebalance");
        }

        assert<3>(all_keys_in_range(node));
        assert<3>(all_keys_in_range(child));
    }

    /// Debugging/testing method to verify node's state
    template <typename N>
    static bool all_keys_in_range(const N& node)
    {
        KeyType key;
        auto iter = BaseNode::iterate(node);
        while (iter.next(&key, nullptr)) {
            if (!key_range_contains(node, key)) { return false; }
        }
        return true;
    }

protected:

    template <typename N, typename T>
    static bool set_foster_key(N& node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::FosterKey, value);
    }

    template <typename N>
    static void unset_foster_key(N& node)
    {
        unset_foster_field(node, FosterNodePayloads::FosterKey);
    }

    template <typename N, typename T>
    static bool set_foster_child(N& node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::FosterPtr, value);
    }

    template <typename N>
    static void unset_foster_child(N& node)
    {
        unset_foster_field(node, FosterNodePayloads::FosterPtr);
    }

    template <typename N, typename T>
    static bool set_low_key(N& node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::LowKey, value);
    }

    template <typename N>
    static void unset_low_key(N& node)
    {
        unset_foster_field(node, FosterNodePayloads::LowKey);
    }

    template <typename N, typename T>
    static bool set_high_key(N& node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::HighKey, value);
    }

    template <typename N>
    static void unset_high_key(N& node)
    {
        unset_foster_field(node, FosterNodePayloads::HighKey);
    }

    template <typename N, typename T>
    static bool set_foster_field(N& node, unsigned field, const T& new_value)
    {
        T old_value;
        bool valid = get_foster_field(node, field, old_value);
        size_t old_length = valid ? Encoder<T>::get_payload_length(old_value) : 0;
        size_t old_payload_length = N::get_payload_count(old_length);

        size_t new_length = Encoder<T>::get_payload_length(new_value);
        size_t new_payload_length = N::get_payload_count(new_length);
        int p_diff = old_payload_length - new_payload_length;

        using PayloadPtr = typename N::PayloadPtr;

        PayloadPtr payload;
        if (!node.is_valid_foster_field(field)) {
            // field invalid so far -- allocate new payload for it
            assert<1>(p_diff < 0);
            bool success = node.allocate_end_payload(payload, new_length);
            if (!success) { return false; }
            node.shift_all_foster_payloads(p_diff);
            node.set_foster_field(field, payload);
        }
        else if (p_diff != 0) {
            // field will be resized -- shift payloads accordingly
            PayloadPtr p_from = node.get_first_payload();
            PayloadPtr p_to = p_from + p_diff;
            auto p_count = node.get_foster_field_payload(field) - p_from;

            bool success = node.shift_payloads(p_to, p_from, p_count);
            if (!success) { return false; }
            node.shift_foster_payloads(field, p_diff);
        }

        payload = node.get_foster_field_payload(field);
        assert<1>(payload < node.get_payload_end());
        Encoder<T>::encode(reinterpret_cast<char*>(node.get_payload(payload)), new_value);
        return true;
    }

    template <typename N>
    static void unset_foster_field(N& node, unsigned field)
    {
        if (node.is_valid_foster_field(field)) {
            auto payload = node.get_foster_field(field);
            auto old_length = Encoder<NodePointer<N>>::get_payload_length(node.get_payload(payload));
            node.free_payload(payload, old_length);
            node.unset_foster_field(field);
        }
    }

    template <typename N, typename T>
    static bool get_foster_field(const N& node, unsigned field, T& value)
    {
        if (!node.is_valid_foster_field(field)) { return false; }
        auto payload = node.get_foster_field(field);
        Encoder<T>::decode(reinterpret_cast<const char*>(node.get_payload(payload)), &value);
        return true;
    }

    template <typename N>
    static bool is_valid_foster_field(const N& node, unsigned field)
    {
        return node.is_valid_foster_field(field);
    }
};

} // namespace foster

#endif
