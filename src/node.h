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

/**
 * \file node.h
 *
 * Classes used to represent a single B-tree node.
 */

#include <string>
using std::string;

#include "assertions.h"
#include "exceptions.h"

// TODO: Fenster object and move_kv_records function are currently not parametrized
#include "fenster.h"
#include "kv_array.h"

namespace foster {

/**
 * \brief Basic class that represents a node of a Foster B-tree.
 *
 * Uses a key-value array (e.g., KeyValueArray) to store key value pairs in the underlying slot
 * array (e.g., SlotArray). The latter is not completely abstracted by the latter, since we
 * need access to the lower-level slot array to encode fence keys, compression prefix, and foster
 * relationships.
 *
 * A note on the use of template arguments: TODO
 */
template <
    class IdType,
    template <class> class PointerType,
    class KeyValueArray,
    template <class,class,class> class Encoder
>
class BtreeNode : public KeyValueArray
{
public:

    using ThisType = BtreeNode<IdType, PointerType, KeyValueArray, Encoder>;
    using NodePointer = PointerType<ThisType>;

    using KeyType = typename KeyValueArray::KeyType;
    using ValueType = typename KeyValueArray::ValueType;
    using PMNK_Type = typename KeyValueArray::PMNK_Type;
    using RecordEncoder = Encoder<KeyType, ValueType, PMNK_Type>;
    using FenceKeyEncoder = Encoder<KeyType, KeyType, PMNK_Type>;
    using ChildPtrEncoder = Encoder<KeyType, NodePointer, PMNK_Type>;
    using SlotNumber = typename KeyValueArray::SlotNumber;
    using PayloadPtr = typename KeyValueArray::PayloadPtr;
    using FensterType = Fenster<KeyType, NodePointer>;

    // Adding "0+" to get around bug in gcc versions < 4.9
    struct alignas(0+KeyValueArray::Alignment) HeaderData
    {
        IdType id;
        PayloadPtr fenster_ptr;

        HeaderData() : fenster_ptr(0) {}
    };

    BtreeNode()
    {
        // Allocate payload for header (see header()) and initialize it
        PayloadPtr ptr;
        bool allocated = this->allocate_payload(ptr, sizeof(HeaderData));
        assert<1>(allocated);

        void* hdr_location = this->get_payload(ptr);
        new (hdr_location) HeaderData;
        assert<1>(hdr_location == header_ptr());

        // Fence and foster keys and foster child are initially empty (i.e., nullptr)
        size_t fenster_length = FensterType::compute_size(nullptr, nullptr, nullptr);
        allocated = this->allocate_payload(ptr, fenster_length);
        assert<1>(allocated);
        header().fenster_ptr = ptr;
        new (this->get_payload(ptr)) FensterType {nullptr, nullptr, nullptr, NodePointer{nullptr}};

        // TODO assign ID
        header().id = IdType(0);
    }

    void add_foster_child(NodePointer child)
    {
        assert<1>(child, "Invalid node pointer in set_foster_child");

        // Foster key is equal to high key of parent, which is equal to both fence keys on child
        KeyType low_key, high_key;
        get_fence_keys(&low_key, &high_key);

        if (child->slot_count() > 0) {
            throw InvalidFosterChildException<KeyType, IdType>(high_key, child->id(), id(),
                "Only an empty node can be added as a foster child");
        }

        // Initialize new child's fenster with equal fence keys and move foster pointer & key
        KeyType old_foster_key;
        get_foster_key(&old_foster_key);
        NodePointer old_foster_child = get_foster_child();
        bool success = child->update_fenster(&high_key, &high_key, &old_foster_key, old_foster_child);
        assert<1>(success, "Keys will not fit into new empty foster child");

        // A newly inserted foster child is always empty, which means the foster key is the same as
        // the high key. In that case, we use an empty key (nullptr). This allows adding an empty
        // foster child without having to allocate more space for the foster key. This way, we make
        // sure that an empty foster child can always be added to an overflown node.
        success = update_fenster(&low_key, &high_key, nullptr, child);
        assert<1>(success, "No space left to add foster child");
    }

    bool rebalance_foster_child()
    {
        // TODO support different policies for picking the split key
        // (pick middle key, divide by total payload size, suffix compression, etc.)
        // e.g., template <class RebalancePolicy = void>

        // STEP 1: determine split key
        SlotNumber slot_count = this->slot_count();
        SlotNumber split_slot = slot_count / 2;
        KeyType split_key;
        RecordEncoder::decode(this->get_payload_for_slot(split_slot), &split_key);

        // STEP 2: move records
        NodePointer child = get_foster_child();
        bool moved = internal::move_kv_records(*child, SlotNumber(0), *this, split_slot,
                slot_count - split_slot);
        if (!moved) { return false; }

        // STEP 3: Adjust foster key
        KeyType low_key, high_key;
        get_fence_keys(&low_key, &high_key);
        bool success = update_fenster(&low_key, &high_key, &split_key, child);
        if (!success) { return false; }

        // STEP 4: Adjust fence keys on foster child
        KeyType childs_foster_key;
        child->get_foster_key(&childs_foster_key);
        success = child->update_fenster(&split_key, &high_key, &childs_foster_key,
                child->get_foster_child());
        if (!success) { return false; }

        assert<3>(this->is_consistent());
        assert<3>(child->is_consistent());

        return true;
    }

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

    /*
     * Since this class is not supposed to have any member variables (so that the space it occupies
     * is exactly what the user specifies in TotalBytes), header data is stored in the payload
     * region of the underlying slot array (which is hidden behind KeyValueArray).
     */
    HeaderData& header() const { return *(header_ptr()); }

    /// \brief Convenience method to get node ID from the header
    IdType id() const { return header().id; }

    void get_fence_keys(KeyType* low, KeyType* high) const
    {
        get_fenster()->get_keys(low, high, nullptr);
    }

    void get_foster_key(KeyType* key) const
    {
        get_fenster()->get_keys(nullptr, nullptr, key);
    }

    NodePointer get_foster_child() const
    {
        return get_fenster()->get_foster_ptr();
    }

    NodePointer get_child(const SlotNumber& slot) const
    {
        // The decoder is optimized for the case where PMNK and the actual key are the same type. In
        // that case, the key doesn't have to be encoded in the payload. Furthermore, since the
        // value type is not string, it is encoded simply by copying the contents.
        // However (TODO), if we check for these conditions here already, we might eliminate some
        // overhead that a not-so-clever compiler would not optimize out. Since the checking is
        // purely static, the code inside the if statement is generated at compile time and no
        // checking overhead is incurred at runtime.
        NodePointer ret;
        ChildPtrEncoder::decode(this->get_payload_for_slot(slot), nullptr, &ret,
                this->get_slot(slot).key);

        return ret;
    }

    bool is_consistent()
    {
        if (!this->is_sorted()) { return false; }

        // TODO implement a KeyValueArray iterator
        KeyType key;
        for (SlotNumber i = 0; i < this->slot_count(); i++) {
           RecordEncoder::decode(this->get_payload_for_slot(i), &key, nullptr, &this->get_slot(i).key);
           if (!key_range_contains(key)) { return false; }
        }

        return true;
    }

    bool is_low_key_infinity() const { return get_fenster()->is_low_key_infinity(); }
    bool is_high_key_infinity() const { return get_fenster()->is_high_key_infinity(); }
    bool is_foster_empty() const { return get_fenster()->is_foster_empty(); }

protected:

    FensterType* get_fenster()
    {
        return reinterpret_cast<FensterType*>(this->get_payload(header().fenster_ptr));
    }

    const FensterType* get_fenster() const
    {
        return reinterpret_cast<const FensterType*>(this->get_payload(header().fenster_ptr));
    }

    bool update_fenster(KeyType* low, KeyType* high, KeyType* foster, NodePointer foster_ptr)
    {
        size_t current_length = this->get_payload_count(get_fenster()->get_size());
        size_t new_length = this->get_payload_count(FensterType::compute_size(low, high, foster));

        PayloadPtr& ptr = header().fenster_ptr;
        if (new_length != current_length) {
            int diff = current_length - new_length;
            PayloadPtr first = this->get_first_payload();
            bool shifted = this->shift_payloads(first + diff, first, ptr - first);
            if (!shifted) { return false; }
            ptr = ptr + diff;
        }

        new (this->get_payload(ptr)) FensterType {low, high, foster, foster_ptr};
        return true;
    }

private:

    /// Helper method for header()
    HeaderData* header_ptr() const
    {
        // Yeah, it's ugly, but it's low-level code.
        // The assertion in the constuctor makes sure it works.
        char* this_addr = const_cast<char*>(reinterpret_cast<const char*>(this));
        void* hdr_location = this_addr + sizeof(*this) - sizeof(HeaderData);
        return reinterpret_cast<HeaderData*>(hdr_location);
    }
};

} // namespace foster

#endif
