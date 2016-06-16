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

#ifndef FOSTER_BTREE_SORTED_LIST_H
#define FOSTER_BTREE_SORTED_LIST_H

/**
 * \file sorted_list.h
 *
 * A simple sorted list data structure build upon B-tree nodes and foster relationships.
 */

#include <memory> // for std::allocator
#include <iostream> // for print

#include "assertions.h"

namespace foster {

template <
    class Node,
    class Allocator = std::allocator<Node>
>
class SortedList {
public:

    using NodePointer = typename Node::NodePointer;
    using KeyType = typename Node::KeyType;
    using ValueType = typename Node::ValueType;

    SortedList(const Allocator& allocator = Allocator{})
        : allocator_(allocator)
    {
        head_ = allocate_node();
    }

    ~SortedList()
    {
        // TODO: destroy all nodes in the linked list
    }

    NodePointer traverse(const KeyType& key) const
    {
        NodePointer p = head_;

        while (p && !p->key_range_contains(key)) {
            p = p->get_foster_child();
        }
        assert<1>(p, "Traversal on sorted list reached null pointer");

        return p;
    }

    // TODO: this method is generic enough to be moved out of this class (it works for other data
    // structures such as B-tree)
    void put(const KeyType& key, const ValueType& value)
    {
        NodePointer node = traverse(key);
        bool inserted = node->insert(key, value);

        while (!inserted) {
            // Node is full -- split required
            // STEP 1: Add a new empty foster child
            NodePointer new_node = allocate_node();
            node->add_foster_child(new_node);
            assert<3>(node->is_sorted());

            // STEP 2: Move records into the new foster child using rebalance operation
            bool rebalanced = node->rebalance_foster_child();
            assert<0>(rebalanced, "Could not rebalance records into new foster child");

            // STEP 3: Decide if insertion should go into old or new node and retry
            if (!node->key_range_contains(key)) {
                node = new_node;
                assert<1>(node->fence_contains(key));
            }
            inserted = node->insert(key, value);
        }
    }

    bool get(const KeyType& key, ValueType& value)
    {
        NodePointer node = traverse(key);
        return node->find(key, &value);
    }

    void print(std::ostream& out)
    {
        NodePointer p = head_;
        while (p) {
            std::cout << "====== NODE " << p->id() << " ======" << std::endl;
            p->print(out);
            p = p->get_foster_child();
        }
    }

protected:
    NodePointer head_;

    Allocator allocator_;

    NodePointer allocate_node()
    {
        // allocate space for node and invoke constructor
        void* addr = allocator_.allocate(1 /* number of nodes to allocate */);
        return NodePointer(new (addr) Node());
    }

};


} // namespace foster

#endif
