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

/**
 * \brief Ordered map data structure that stores key-value pairs in a chain of nodes.
 *
 * Key-value pairs are stored in nodes linked by foster relationships. Its organization is a hybrid
 * between a C++ vector and a list, as elements are stored contiguously within nodes that are
 * linked. Since the list is kept ordered, insertion, deletion, and lookup all require a traversal,
 * which is linear in the number of nodes. After a traversal, a logarithmic search is performed
 * within a node.
 *
 * \tparam Node The node class to use.
 * \tparam Allocator The allocator used to manage memory for nodes.
 */
template <
    class Node,
    class Allocator = std::allocator<Node>
>
class SortedList {
public:

    using NodePointer = typename Node::NodePointer;
    using KeyType = typename Node::KeyType;
    using ValueType = typename Node::ValueType;

    /**
     * \brief Constructs an empty list, with a single empty node.
     * \oaram[in] allocator Allocator object used to allocate and free nodes
     */
    SortedList(const Allocator& allocator = Allocator{})
        : allocator_(allocator)
    {
        head_ = allocate_node();
    }

    ~SortedList()
    {
        // TODO: destroy all nodes in the linked list
    }

    /**
     * \brief Traversal method: finds node whose key range contains the given key
     */
    NodePointer traverse(const KeyType& key) const
    {
        NodePointer p = head_;

        while (p && !p->key_range_contains(key)) {
            p = p->get_foster_child();
        }
        assert<1>(p, "Traversal on sorted list reached null pointer");

        return p;
    }

    /**
     * \brief Insert new key-value pair, splitting the target node if necessary.
     */
    void put(const KeyType& key, const ValueType& value)
    {
        NodePointer node = traverse(key);
        bool inserted = node->insert(key, value);

        while (!inserted) {
            // Node is full -- split required
            NodePointer new_node = allocate_node();
            node = node->split_for_insertion(key, new_node);
            inserted = node->insert(key, value);
        }
    }

    /**
     * \brief Retrieve a value given its associated key.
     */
    bool get(const KeyType& key, ValueType& value)
    {
        NodePointer node = traverse(key);
        return node->find(key, &value);
    }

    /// Print method for debugging and testing
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

    /// \brief Pointer to the head node, i.e., the first node in the linked list.
    NodePointer head_;

    /// \brief Allocator object used to allocate memory for new nodes.
    Allocator allocator_;

    /// \brief: Allocate and construct a new node to be added to the linked list.
    NodePointer allocate_node()
    {
        // allocate space for node and invoke constructor
        void* addr = allocator_.allocate(1 /* number of nodes to allocate */);
        return NodePointer(new (addr) Node());
    }

    // TODO implement destroy_node

};


} // namespace foster

#endif
