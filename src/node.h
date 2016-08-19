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
#include "dummies.h"

// TODO: Fenster node currently not parametrized
#include "fenster_node.h"

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
 * \tparam Pointer Class template for the type of the foster child pointer (\see pointers.h)
 */
template <
    class K,
    class V,
    template <class,class> class KeyValueArray,
    template <class> class Pointer,
    class Latch = DummyLatch
>
class BtreeNode : public FensterNode<KeyValueArray<K, V>>, public Latch
{
public:

    // Type aliases for convenience and external access
    using ThisType = BtreeNode<K, V, KeyValueArray, Pointer, Latch>;
    using NodePointer = Pointer<ThisType>;
    using ParentType = BtreeNode<K, NodePointer, KeyValueArray, Pointer, Latch>;
    using ParentPointer = Pointer<ParentType>;
    using KeyType = K;
    using ValueType = V;
    using SlotNumber = typename KeyValueArray<K, V>::SlotNumber;
    using PayloadPtr = typename KeyValueArray<K, V>::PayloadPtr;
    template <class T> using PointerType = Pointer<T>;

    static constexpr bool LatchingEnabled = !std::is_same<Latch, DummyLatch>::value;

    template <class Ptr>
    Ptr split_for_insertion(const KeyType& new_key, Ptr new_node)
    {
        new_node->acquire_write();

        this->split(new_node);

        // Decide if insertion should go into old or new node and return it
        if (!this->key_range_contains(new_key)) {
            assert<1>(new_node->key_range_contains(new_key));
            this->release_write();
            return new_node;
        }
        else {
            new_node->release_write();
            return Ptr{this};
        }
    }

    /// Debugging/testing method to verify node's state
    bool is_consistent()
    {
        return this->is_sorted() && this->all_keys_in_range();
    }
};

} // namespace foster

#endif
