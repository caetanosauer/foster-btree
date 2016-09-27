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
#include <type_traits>

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
        : level_(0)
    {
        foster_payloads_.fill(0);
        foster_payload_valid_.fill(false);
    }

    static constexpr unsigned LowKey = 0;
    static constexpr unsigned HighKey = 1;
    static constexpr unsigned FosterKey = 2;
    static constexpr unsigned FosterPtr = 3;
    static constexpr unsigned Prefix = 4;
    static constexpr unsigned FieldCount = 5;

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

    void set_level(uint8_t l) { level_ = l; }
    uint8_t level() { return level_; }

protected:

    std::array<PayloadPtr, FieldCount> foster_payloads_;
    std::array<bool, FieldCount> foster_payload_valid_;
    uint8_t level_;
};

template <
    class K,
    class V,
    template <typename,typename> class Base,
    template <typename> class Encoder,
    class Latch = void
>
class FosterNode : public Base<K,V>
{
public:

    using KeyType = K;
    using ValueType = V;
    using ThisType = FosterNode<K, V, Base, Encoder>;
    using BaseNode = Base<K, V>;

    static constexpr bool LatchingOn = !std::is_void<Latch>::value;

    template <typename N>
    static void initialize(N node, uint8_t level = 0)
    {
        BaseNode::initialize(node);
        set_foster_child(node, N {nullptr});
        node->set_level(level);
    }

    template <typename N, typename T>
    static bool get_foster_child(N node, T& value)
    {
        bool ret = get_foster_field(node, FosterNodePayloads::FosterPtr, value);
        // foster child pointer is special case: we must return false if it exists but is null
        return ret && value;
    }

    template <typename N>
    static bool has_foster_child(N node)
    {
        N ptr;
        return get_foster_child(node, ptr);
    }

    template <typename N, typename T>
    static bool get_low_key(N node, T& value)
    {
        return get_foster_field(node, FosterNodePayloads::LowKey, value);
    }

    template <typename N>
    static bool is_low_key_infinity(N node)
    {
        return !is_valid_foster_field(node, FosterNodePayloads::LowKey);
    }

    template <typename N, typename T>
    static bool get_high_key(N node, T& value)
    {
        return get_foster_field(node, FosterNodePayloads::HighKey, value);
    }

    template <typename N>
    static bool is_high_key_infinity(N node)
    {
        return !is_valid_foster_field(node, FosterNodePayloads::HighKey);
    }

    template <typename N, typename T>
    static bool get_foster_key(N node, T& value)
    {
        return get_foster_field(node, FosterNodePayloads::FosterKey, value);
    }

    template <
        typename N,
        typename Adoption
    >
    static N traverse(N branch, const K& key, bool for_update,
            Adoption* adoption = nullptr, unsigned depth = 0)
    {
        // If this is root node, latch it here
        if (depth == 0) { latch_pointer(branch, for_update); }

        // Descend into the target child node
        auto child = branch->level() == 0 ? branch :
            descend_to_child(branch, key, for_update, adoption);
        assert<1>(is_latched(child));

        // Release latch on parent
        if (branch->level() > 0) { unlatch_pointer(branch, for_update); }

        // Now we found the target child node, but key may be somewhere in the foster chain
        assert<1>(fence_contains(child, key));
        while (child && !key_range_contains(child, key)) {
            N foster;
            bool has_foster = get_foster_child(child, foster);
            assert<1>(has_foster);

            latch_pointer(foster, for_update);
            unlatch_pointer(child, for_update);
            child = foster;
        }

        assert<1>(child, "Traversal reached null pointer");
        assert<1>(key_range_contains(child, key), "Traversal reached invalid node");

        if (child->level() == 0) { return child; }
        return traverse(child, key, for_update, adoption, depth + 1);
    }

protected:

    template <
        typename N,
        typename Adoption
    >
    static meta::EnableIf<std::is_same<N,V>::value && sizeof(Adoption), N>
        descend_to_child(N branch, const K& key, bool for_update,
            Adoption* adoption = nullptr)
    {
        N child {nullptr};

        assert<1>(branch);
        while (true) {
            assert<1>(is_latched(branch));
            // Branch nodes that participate in a traversal must not be empty
            assert<1>(branch->slot_count() > 0 || has_foster_child(branch));
            // assert<1>(branch->has_reader());

            // find method guarantees that the next child will contain the value (i.e., the branch
            // pointer) associated with the slot where the key would be inserted if not found.
            // This means that the return value of "find" is completely irrelevant.
            BaseNode::find(branch, key, child);
            assert<1>(child);

            // If current branch does not contain the key, it must be in a foster child
            if (!key_range_contains(branch, key)) {
                N foster {nullptr};
                get_foster_child(branch, foster);
                assert<1>(foster);

                latch_pointer(foster, for_update);
                unlatch_pointer(branch, for_update);
                branch = foster;
                continue;
            }
            assert<1>(key_range_contains(branch, key));

            // Latch child node before proceeding
            latch_pointer(child, for_update);

            // Try do adopt child's foster child -- restart traversal if it works
            if (adoption && adoption->try_adopt(branch, child)) {
                unlatch_pointer(child, for_update);
                continue;
            }

            break;
        }

        assert<1>(child);
        return child;
    }

    template <typename N, typename Adoption>
    static meta::EnableIf<!(std::is_same<N,V>::value && sizeof(Adoption)), N>
        descend_to_child(N branch, const K&, bool, Adoption* = nullptr)
    {
        return branch;
    }

public:

    /// \brief Checks if given key is within the fence borders
    template <typename N>
    static bool fence_contains(N node, const K& key)
    {
        K low, high;
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
    static bool key_range_contains(N node, const K& key)
    {
        if (!fence_contains(node, key)) { return false; }
        N foster;
        if (get_foster_child(node, foster)) {
            K foster_key;
            bool foster_finite = get_foster_key(node, foster_key);
            return !foster_finite || key < foster_key;
        }
        return true;
    }

    template <typename N>
    static void split(N node, N new_node)
    {
        add_foster_child(node, new_node);
        rebalance(node);
    }

    template <typename N>
    static void add_foster_child(N node, N child)
    {
        assert<1>(child, "Invalid node pointer in add_foster_child");
        bool success = true;

        // Foster key is equal to high key of parent, which is equal to both fence keys on child
        K high_key, foster_key;
        N foster_ptr;
        if (get_high_key(node, high_key)) {
            success = set_low_key(child, high_key);
            assert<1>(success, "Could not add foster child");
            success = set_high_key(child, high_key);
            assert<1>(success, "Could not add foster child");
        }
        if (get_foster_key(node, foster_key)) {
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
        success = set_foster_child(node, child);
        assert<0>(success, "Could not add foster child");
    }

    template <typename N>
    static void unset_foster_child(N node)
    {
        unset_foster_field(node, FosterNodePayloads::FosterPtr);
    }

    template <typename N>
    static void rebalance(N node, bool logit = true)
    {
        using SlotNumber = typename BaseNode::template SlotNumber<N>;
        // TODO support different policies for picking the split key
        // (pick middle key, divide by total payload size, suffix compression, etc.)
        // e.g., template <class RebalancePolicy = void>

        // TODO support merge and node deletion, i.e., rebalance in the opposite direction

        N foster_ptr;
        get_foster_child(node, foster_ptr);
        N child = foster_ptr;

        // STEP 1: determine split key
        SlotNumber slot_count = node->slot_count();
        SlotNumber split_slot = slot_count / 2;
        assert<1>(split_slot > 0);

        K split_key;
        BaseNode::read_slot(node, split_slot, &split_key, nullptr);

        // TODO for redo, key should be an argument of the method
        // TODO there should be an argument for "direction" of rebalance
        if (logit) { BaseNode::log(node, LRType::Rebalance, split_key); }

        // STEP 2: move records
        bool moved = RecordMover<typename BaseNode::EncoderType>::move_records
            (*child, SlotNumber{0}, *node, split_slot, slot_count - split_slot);
        assert<1>(moved, "Could not move records in node rebalance");

        // STEP 3: Adjust foster key
        bool success = set_foster_key(node, split_key);
        assert<1>(success, "Could not update foster key of foster parent in node rebalance");

        // STEP 4: Adjust fence keys on foster child
        success = set_low_key(child, split_key);
        assert<1>(success, "Could not update low fence key of foster child in node rebalance");
        K high_key;
        if (get_high_key(node, high_key)) {
            success = set_high_key(child, high_key);
            assert<1>(success, "Could not update high fence key of foster child in node rebalance");
        }

        assert<3>(all_keys_in_range(node));
        assert<3>(all_keys_in_range(child));
    }

    /// Debugging/testing method to verify node's state
    template <typename N>
    static bool all_keys_in_range(N node)
    {
        K key;
        auto iter = BaseNode::iterate(node);
        while (iter.next(&key, nullptr)) {
            if (!key_range_contains(node, key)) { return false; }
        }
        return true;
    }

    /// Debugging/utility function to print the array's contents.
    template <typename N>
    static void print(N node, std::ostream& o, bool print_slots = true)
    {
        K key;
        o << "Node low=";
        if (get_low_key(node, key)) { o << key; }
        else { o << "inf "; }

        o << " foster=";
        if (get_foster_key(node, key)) { o << key; }
        else { o << "inf"; }
        o << " high=";
        if (get_high_key(node, key)) { o << key; }
        else { o << "inf"; }

        o << std::endl;

        if (print_slots) { BaseNode::print(node, o); }
    }

// TODO: these are only public because caller must release latch manually after a call to traverse
// We should implement a guard object to release latches automatically
// (like Zero's (ugly) btree_page_h or stl's unique_lock<T>
// private:

    template <typename NodePtr>
    static meta::EnableIf<LatchingOn && sizeof(NodePtr)>
        latch_pointer(NodePtr child, bool ex_mode)
    {
        // Exclusive latch is only required at leaf nodes during normal traversal.
        // (If required, splits, merges, and adoptions will attempt upgrade on branch nodes)
        if (child->level() == 0 && ex_mode) { child->acquire_write(); }
        else { child->acquire_read(); }
    }

    template <typename NodePtr>
    static meta::EnableIf<LatchingOn && sizeof(NodePtr)>
        unlatch_pointer(NodePtr child, bool ex_mode)
    {
        // Exclusive latch is only required at leaf nodes during normal traversal.
        // (If required, splits, merges, and adoptions will attempt upgrade on branch nodes)
        if (child->level() == 0 && ex_mode) { child->release_write(); }
        else { child->release_read(); }
    }

    template <typename NodePtr>
    static meta::EnableIf<LatchingOn && sizeof(NodePtr), bool>
        is_latched(NodePtr child, bool ex_mode = false)
    {
        return (!ex_mode && child->has_reader()) || child->has_writer();
    }

    template <typename NodePtr>
    static meta::EnableIf<!(LatchingOn && sizeof(NodePtr))>
        latch_pointer(NodePtr, bool) {}

    template <typename NodePtr>
    static meta::EnableIf<!(LatchingOn && sizeof(NodePtr))>
        unlatch_pointer(NodePtr, bool) {}

    template <typename NodePtr>
    static meta::EnableIf<!(LatchingOn && sizeof(NodePtr)), bool>
        is_latched(NodePtr, bool = false) { return true; }

protected:

    template <typename N, typename T>
    static bool set_foster_key(N node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::FosterKey, value);
    }

    template <typename N>
    static void unset_foster_key(N node)
    {
        unset_foster_field(node, FosterNodePayloads::FosterKey);
    }

    template <typename N, typename T>
    static bool set_foster_child(N node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::FosterPtr, value);
    }

    template <typename N, typename T>
    static bool set_low_key(N node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::LowKey, value);
    }

    template <typename N>
    static void unset_low_key(N node)
    {
        unset_foster_field(node, FosterNodePayloads::LowKey);
    }

    template <typename N, typename T>
    static bool set_high_key(N node, const T& value)
    {
        return set_foster_field(node, FosterNodePayloads::HighKey, value);
    }

    template <typename N>
    static void unset_high_key(N node)
    {
        unset_foster_field(node, FosterNodePayloads::HighKey);
    }

    template <typename N, typename T>
    static bool set_foster_field(N node, unsigned field, const T& new_value)
    {
        T old_value;
        bool valid = get_foster_field(node, field, old_value);
        size_t old_length = valid ? Encoder<T>::get_payload_length(old_value) : 0;
        size_t old_payload_length = node->get_payload_count(old_length);

        size_t new_length = Encoder<T>::get_payload_length(new_value);
        size_t new_payload_length = node->get_payload_count(new_length);
        int p_diff = old_payload_length - new_payload_length;

        using PayloadPtr = typename BaseNode::template PayloadPtr<N>;

        PayloadPtr payload;
        if (!node->is_valid_foster_field(field)) {
            // field invalid so far -- allocate new payload for it
            assert<1>(p_diff < 0);
            bool success = node->allocate_end_payload(payload, new_length);
            if (!success) { return false; }
            node->shift_all_foster_payloads(p_diff);
            node->set_foster_field(field, payload);
        }
        else if (p_diff != 0) {
            // field will be resized -- shift payloads accordingly
            PayloadPtr p_from = node->get_first_payload();
            PayloadPtr p_to = p_from + p_diff;
            auto p_count = node->get_foster_field_payload(field) - p_from;

            bool success = node->shift_payloads(p_to, p_from, p_count);
            if (!success) { return false; }
            node->shift_foster_payloads(field, p_diff);
        }

        payload = node->get_foster_field_payload(field);
        assert<1>(payload < node->get_payload_end());
        Encoder<T>::encode(reinterpret_cast<char*>(node->get_payload(payload)), new_value);
        return true;
    }

    template <typename N>
    static void unset_foster_field(N node, unsigned field)
    {
        if (node->is_valid_foster_field(field)) {
            auto payload = node->get_foster_field(field);
            auto old_length = Encoder<N>::get_payload_length(node->get_payload(payload));
            node->free_payload(payload, old_length);
            node->unset_foster_field(field);
        }
    }

    template <typename N, typename T>
    static bool get_foster_field(N node, unsigned field, T& value)
    {
        if (!node->is_valid_foster_field(field)) { return false; }
        auto payload = node->get_foster_field(field);
        Encoder<T>::decode(reinterpret_cast<const char*>(node->get_payload(payload)), &value);
        return true;
    }

    template <typename N>
    static bool is_valid_foster_field(N node, unsigned field)
    {
        return node->is_valid_foster_field(field);
    }
};

} // namespace foster

#endif
