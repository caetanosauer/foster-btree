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

#ifndef FOSTER_BTREE_BTREE_ADOPTION_H
#define FOSTER_BTREE_BTREE_ADOPTION_H

/**
 * \file btree.h
 *
 * Btree logic built on top of a node data structure with support for foster relationships.
 */

#include <memory>

#include "debug_log.h"
#include "move_records.h"

namespace foster {

template <
    class Node,
    class NodeMgr
>
class EagerAdoption
{
public:

    EagerAdoption(std::shared_ptr<NodeMgr> node_mgr)
        : node_mgr_(node_mgr)
    {}

    template <typename N>
    bool try_adopt(N& parent, N child)
    {
        if (!child) { return false; }

        N foster;
        if (!Node::get_foster_child(child, foster)) { return false; }

        return do_adopt(parent, child, foster);
    }

    template <typename N>
    bool do_adopt(N& parent, N child, N foster)
    {
        // Insert separator key and pointer into parent
        typename Node::KeyType foster_key;
        Node::get_foster_key(child, foster_key);
        bool inserted = Node::insert(parent, foster_key, foster);

        // Split parent as long as insertion fails
        // TODO this is the same code as in btree.h -> unify
        while (!inserted) {
            auto new_node = node_mgr_->template construct_node<Node>();
            Node::split(parent, new_node);

            // Decide if insertion should go into old or new node
            if (!Node::key_range_contains(parent, foster_key)) {
                assert<1>(Node::key_range_contains(new_node, foster_key));
                new_node->acquire_write();
                parent->release_write();
                parent = new_node;
            }

            inserted = Node::insert(parent, foster_key, foster);
        }

        // Clear foster relationship on child
        Node::unset_foster_child(child);
        Node::set_high_key(child, foster_key);

        dbg::trace("Adopted {} from {} into parent {}", foster, child, parent);
        // Node::print_node(foster, std::cout, false);
        // Node::print_node(child, std::cout, false);
        // Node::print_node(parent, std::cout, false);
        return true;
    }

    template <typename N>
    bool try_grow(N& root)
    {
        auto new_child = node_mgr_->template construct_node<Node>();
        Node::grow(root, new_child);

        dbg::trace("Grew {} with new child {}", root, new_child);
        root = new_child;
        return true;
    }

private:
    std::shared_ptr<NodeMgr> node_mgr_;
};

} // namespace foster

#endif

