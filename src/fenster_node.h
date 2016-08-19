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

#ifndef FOSTER_BTREE_FENSTER_NODE_H
#define FOSTER_BTREE_FENSTER_NODE_H

/**
 * \file node.h
 *
 * B-tree node with support for a fenster object (fence keys + foster relationship)
 */

#include <string>
using std::string;

#include "assertions.h"
#include "exceptions.h"
#include "dummies.h"

// TODO: Fenster object and move_kv_records function are currently not parametrized, so we must
// include the headers here.
#include "fenster.h"
#include "kv_array.h"

namespace foster {


/**
 * \brief Basic class that represents a node of a Foster B-tree.
 *
 * Uses a key-value array (e.g., KeyValueArray) to store key value pairs in the underlying slot
 * array (e.g., SlotArray). The latter is not completely abstracted by the latter, since we
 * need access to the lower-level slot array to store header data and encode the fenster object.
 * The fenster object encapsulates the encoding of a triplet of keys (low fence, high fence, and
 * foster) as well as a foster child pointer (\see fenster.h).
 *
 * This class reserves some payloads in the end of the slot array to store its header data (\see
 * HeaderData) and the fenster object. Whenever the latter grows in size, payloads are shifted to
 * maintain the invariant that such "metadata" is always at the end of the slot array. If this
 * invariant is broken, shifting other payloads (e.g., during insertion of a key-value pair) would
 * invalidate the pointer to the fenster object payload, which is kept in HeaderData.
 *
 * Other than maintaining a fenster object, this class provides methods to manage foster
 * relationships, including adding or removind an empty foster, rebalancing records between a node
 * and its foster child (for split or merge). Furthermore, it supports traversal by allowing to test
 * if a given key is within the range of a node or its foster child.
 *
 * \tparam K Key type
 * \tparam V Value type
 * \tparam KeyValueArray Key-value array clas template, parametrized only by key and value types
 */
template <class KeyValueArray>
class FensterNode : public KeyValueArray
{
public:

    // Type aliases for convenience and external access
    using ThisType = FensterNode<KeyValueArray>;
    using KeyType = typename KeyValueArray::KeyType;
    using SlotNumber = typename KeyValueArray::SlotNumber;
    using PayloadPtr = typename KeyValueArray::PayloadPtr;
    using FensterType = Fenster<KeyType>;
    using Logger = typename KeyValueArray::LoggerType;

    /**
     * \brief Constructs an empty node with a given ID
     *
     * Initializes header data and the fenster object with empty keys. All this data is kept on
     * payloads in the end of the underlying slot array. Such payloads are allocated only once,
     * right here, and any further changes require invoking shift_payloads.
     *
     * A destructor is not necessary because this class does not have any explicit member variables,
     * i.e., it does not occupy any extra space other than the underlying KeyValueArray, which, in
     * turn, only occupies the space of its underlying SlotArray.
     */
    FensterNode()
    {
        PayloadPtr ptr {0};
        // Fence and foster keys and foster child are initially empty (i.e., nullptr)
        size_t fenster_length = FensterType::compute_size(nullptr, nullptr, nullptr);
        bool allocated = this->allocate_payload(ptr, fenster_length);
        assert<1>(allocated);

        fenster_ptr_ = ptr;
        new (this->get_payload(ptr)) FensterType {nullptr, nullptr, nullptr, nullptr};
    }

    /** @name Methods for management of foster relationship **/
    /**@{**/

    /**
     * \brief Adds the given node as a foster child.
     *
     * If this node already has a foster child, given node adopts it as its own foster child, so
     * that the new node is inserted in-between the current node and the old foster child.
     *
     * The given node must be empty, i.e., contain zero slots and have no foster child of its own.
     */
    template <class Ptr>
    void add_foster_child(Ptr child)
    {
        assert<1>(child, "Invalid node pointer in set_foster_child");

        // Foster key is equal to high key of parent, which is equal to both fence keys on child
        KeyType low_key, high_key;
        get_fence_keys(&low_key, &high_key);
        KeyType* low_ptr = is_low_key_infinity() ? nullptr : &low_key;
        KeyType* high_ptr = is_high_key_infinity() ? nullptr : &high_key;

        // Initialize new child's fenster with equal fence keys and move foster pointer & key
        KeyType old_foster_key;
        get_foster_key(&old_foster_key);
        KeyType* old_foster_key_ptr = is_foster_empty() ? nullptr : &old_foster_key;
        Ptr old_foster_child;
        get_foster_child(old_foster_child);
        bool success = child->update_fenster(high_ptr, high_ptr, old_foster_key_ptr, old_foster_child);
        assert<1>(success, "Keys will not fit into new empty foster child");

        // A newly inserted foster child is always empty, which means the foster key is the same as
        // the high key. In that case, we use an empty key (nullptr). This allows adding an empty
        // foster child without having to allocate more space for the foster key. This way, we make
        // sure that an empty foster child can always be added to an overflown node.
        success = update_fenster(low_ptr, high_ptr, nullptr, child);
        assert<1>(success, "No space left to add foster child");
    }

    /**
     * \brief Resets the foster child pointer to null.
     */
    void unlink_foster_child()
    {
        if (!get_foster_child()) { return; }

        KeyType low_key, high_key, foster_key;
        get_fence_keys(&low_key, &high_key);
        get_foster_key(&foster_key);
        KeyType* low_ptr = is_low_key_infinity() ? nullptr : &low_key;
        KeyType* high_ptr = is_high_key_infinity() ? nullptr : &high_key;
        // If foster child was empty, foster key is not stored and is equal to the high fence key
        KeyType* foster_key_ptr = is_foster_empty() ? high_ptr : &foster_key;

        // The old foster key becomes the new high fence key
        bool success = update_fenster(low_ptr, foster_key_ptr, nullptr, nullptr);
        assert<1>(success && !get_foster_child(), "Unable to unlink foster child");
    }

    /**
     * \brief Rebalances records between this node and its foster child.
     *
     * This operation supports a node split by moving (roughly) half of the records from this node
     * into its foster child. It is the first step of a complete node split -- the adoption by the
     * parent being the second one.
     *
     * This method uses the move_kv_records function template to move key-value pairs from one
     * key-value array to another.
     */
    template <class Ptr>
    bool rebalance_foster_child()
    {
        // TODO support different policies for picking the split key
        // (pick middle key, divide by total payload size, suffix compression, etc.)
        // e.g., template <class RebalancePolicy = void>

        // TODO support merge and node deletion, i.e., rebalance in the opposite direction

        // STEP 1: determine split key
        SlotNumber slot_count = this->slot_count();
        SlotNumber split_slot = slot_count / 2;
        KeyType split_key;
        this->read_slot(split_slot, &split_key, nullptr);

        // TODO for redo, key should be an argument of the method
        // TODO there should be an argument for "direction" of rebalance
        Logger::log(LRType::Rebalance, *this, split_key);

        // STEP 2: move records
        Ptr child;
        get_foster_child(child);
        bool moved = internal::move_kv_records(*child, SlotNumber(0), *this, split_slot,
                slot_count - split_slot);
        if (!moved) { return false; }

        // STEP 3: Adjust foster key
        KeyType low_key, high_key;
        get_fence_keys(&low_key, &high_key);
        KeyType* low_ptr = is_low_key_infinity() ? nullptr : &low_key;
        KeyType* high_ptr = is_high_key_infinity() ? nullptr : &high_key;

        bool success = update_fenster(low_ptr, high_ptr, &split_key, child);
        if (!success) { return false; }

        // STEP 4: Adjust fence keys on foster child
        KeyType childs_foster_key;
        child->get_foster_key(&childs_foster_key);
        KeyType* childs_foster_key_ptr = child->is_foster_empty() ? nullptr : &childs_foster_key;
        success = child->update_fenster(&split_key, high_ptr, childs_foster_key_ptr,
                child->get_foster_child());
        if (!success) { return false; }

        assert<3>(this->all_keys_in_range());
        assert<3>(child->all_keys_in_range());

        return true;
    }

    /**
     * \brief Perform a rebalance (split) operation to enable an insertion when the node is full.
     *
     * This method is invoked whenever a regular insertion returns false, indicating that there is
     * no space left to insert the given record. It should be invoked in a while loop, because
     * multiple splits may be required to enable a single insertion.
     *
     * This method is here just for convenience, since all it does is invoke add_foster_child
     * followed by rebalance_foster_child. After that, it checks the key ranges to determine if
     * insertion should be retried on this node or its (new) foster child -- this check is the
     * reason why the key must be given as argument.
     *
     * \param[in] key Key which we are trying to insert.
     * \param[in] neW_node New node to be added as foster child.
     * \returns Pointer to node on which insertion should be retried (either this node or the new
     *       foster child).
     */
    template <class Ptr>
    void split(Ptr new_node)
    {
        add_foster_child(new_node);
        bool rebalanced = rebalance_foster_child<Ptr>();
        assert<1>(rebalanced, "Could not rebalance records into new foster child");
    }

    /**@}**/

    /** @name Methods for keys against fence and foster keys **/
    /**@{**/

    /// \brief Checks if given key is within the fence borders
    bool fence_contains(const KeyType& key) const
    {
        KeyType low, high;
        get_fence_keys(&low, &high);

        if (key >= low && key <= high) { return true; }

        // comparison failed, but key may still be contained if one of the keys is infinity
        if (key >= low && is_high_key_infinity()) { return true; }
        if (key <= high && is_low_key_infinity()) { return true; }
        if (is_low_key_infinity() && is_high_key_infinity()) {
            return true;
        }

        return false;
    }

    /**
     * \brief Checks if given key is contained in this node.
     *
     * This is stricter than fence_contains, because it returns false if the key is not in this node
     * but in its foster child, whereas the former returns true.
     */
    bool key_range_contains(const KeyType& key) const
    {
        if (!fence_contains(key)) { return false; }
        if (!is_foster_empty()) {
            KeyType foster;
            get_foster_key(&foster);
            return key < foster;
        }
        return true;
    }

    /**@}**/

    /** @name Convenience methods to access header and fenster data **/
    /**@{**/

    /// \brief Convenience method to get node ID
    void get_fence_keys(KeyType* low, KeyType* high) const
    {
        get_fenster()->get_keys(low, high, nullptr);
    }

    void get_foster_key(KeyType* key) const
    {
        get_fenster()->get_keys(nullptr, nullptr, key);
    }

    void get_prefix(KeyType* key) const
    {
        get_fenster()->get_keys(nullptr, nullptr, key);
    }

    template <class Ptr>
    void get_foster_child(Ptr& ptr) const
    {
        get_fenster()->get_foster_ptr(ptr);
    }

    void* get_foster_child() const { return get_fenster()->get_foster_ptr(); }

    bool is_low_key_infinity() const { return get_fenster()->is_low_key_infinity(); }
    bool is_high_key_infinity() const { return get_fenster()->is_high_key_infinity(); }
    bool is_foster_empty() const { return get_fenster()->is_foster_empty(); }

    /**@}**/

    /// Debugging/testing method to verify node's state
    bool all_keys_in_range()
    {
        KeyType key;
        auto iter = this->iterate();
        while (iter.next(&key, nullptr)) {
            if (!key_range_contains(key)) { return false; }
        }
        return true;
    }

protected:

    FensterType* get_fenster()
    {
        return reinterpret_cast<FensterType*>(this->get_payload(fenster_ptr_));
    }

    const FensterType* get_fenster() const
    {
        return reinterpret_cast<const FensterType*>(this->get_payload(fenster_ptr_));
    }

    /**
     * Method used to update fenster data by constructing a new fenster object in the reserved
     * payload area. If growing or shrinking is required, SlotArray::shift_payloads is invoked
     * accordingly and fenster_ptr is updated in the header.
     */
    bool update_fenster(KeyType* low, KeyType* high, KeyType* foster, void* foster_ptr)
    {
        size_t current_length = this->get_payload_count(get_fenster()->get_size());
        size_t new_length = this->get_payload_count(FensterType::compute_size(low, high, foster));

        PayloadPtr& ptr = fenster_ptr_;
        if (new_length != current_length) {
            int diff = current_length - new_length;
            PayloadPtr first = this->get_first_payload();
            bool shifted = this->shift_payloads(first + diff, first, ptr - first);
            if (!shifted) { return false; }
            ptr = ptr + diff;
        }

        new (this->get_payload(ptr)) FensterType {low, high, foster, foster_ptr};
        assert<3>(this->all_keys_in_range());
        return true;
    }

private:

    PayloadPtr fenster_ptr_;
};


} // namespace foster

#endif
