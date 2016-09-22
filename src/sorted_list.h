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
    class K,
    class V,
    class SArray,
    template <class,class> class Node,
    template <class> class Pointer,
    template <class> class NodeMgr
>
class SortedList {
public:

    using NodeMgrType = NodeMgr<SArray>;
    using NodePointer = Pointer<SArray>;
    using N = Node<K, V>;

    /**
     * \brief Constructs an empty list, with a single empty node.
     */
    SortedList()
    {
        node_mgr_ = std::make_shared<NodeMgrType>();
        head_ = node_mgr_->construct_node();
        N::initialize(head_);
    }

    ~SortedList()
    {
        // TODO: destroy all nodes in the linked list
    }

    /**
     * \brief Traversal method: finds node whose key range contains the given key
     */
    NodePointer traverse(const K& key) const
    {
        NodePointer p = head_;

        while (p && !N::key_range_contains(p, key)) {
            bool has_foster = N::get_foster_child(p, p);
            if (!has_foster) { p = nullptr; }
        }
        assert<1>(p, "Traversal on sorted list reached null pointer");

        return p;
    }

    /**
     * \brief Insert new key-value pair, splitting the target node if necessary.
     */
    void put(const K& key, const V& value)
    {
        NodePointer node = traverse(key);
        bool inserted = N::insert(node, key, value);

        while (!inserted) {
            // Node is full -- split required
            auto new_node = node_mgr_->construct_node();
            N::split(node, new_node);

            // Decide if insertion should go into old or new node
            if (!N::key_range_contains(node, key)) {
                assert<1>(N::key_range_contains(new_node, key));
                node = new_node;
            }

            inserted = N::insert(node, key, value);
        }
    }

    /**
     * \brief Retrieve a value given its associated key.
     */
    bool get(const K& key, V& value)
    {
        NodePointer node = traverse(key);
        return N::find(node, key, value);
    }

    /// Print method for debugging and testing
    void print(std::ostream& out)
    {
        NodePointer p = head_;
        while (p) {
            std::cout << "====== NODE " << p->id() << " ======" << std::endl;
            N::print(p, out);
            bool has_foster = N::get_foster_child(p);
            if (!has_foster) { p = nullptr; }
        }
    }

protected:

    /// \brief Pointer to the head node, i.e., the first node in the linked list.
    NodePointer head_;

    std::shared_ptr<NodeMgrType> node_mgr_;
};


} // namespace foster

#endif
