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

#include "assertions.h"

namespace foster {

template <
    class ParentNodePointer,
    class ChildNodePointer
>
struct EagerAdoption
{
    static constexpr bool Latching = ParentNodePointer::PointeeType::LatchingEnabled;

    template <class NodeMgr>
    static bool try_adopt(ParentNodePointer& parent, ChildNodePointer child, NodeMgr& node_mgr)
    {
        if (!child) { return false; }

        ChildNodePointer foster = child->get_foster_child();
        if (!foster) { return false; }

        // First attempt latch upgrade on parent and on child (if it's not already latched exclusively)
        bool child_latch_upgraded = false;

        // Parent is always latched in shared mode
        if (!parent->attempt_upgrade()) { return false; }
        if (child->has_reader()) {
            if (!child->attempt_upgrade()) {
                parent->downgrade();
                return false;
            }
            child_latch_upgraded = true;
        }

        // Latching was successful or it's disabled -- proceed with adoption
        bool success = do_adopt(parent, child, foster, node_mgr);

        // Release/downgrade acquired latches
        parent->downgrade();
        if (child_latch_upgraded) { child->downgrade(); }

        return success;
    }

    template <class NodeMgr>
    static bool do_adopt(ParentNodePointer& parent, ChildNodePointer child, ChildNodePointer foster,
            NodeMgr& node_mgr)
    {
        using KeyType = typename NodeMgr::KeyType;

        // Insert separator key and pointer into parent
        KeyType foster_key;
        child->get_foster_key(&foster_key);
        bool inserted = parent->insert(foster_key, foster);

        // Split parent as long as insertion fails
        while (!inserted) {
            auto new_node = node_mgr.construct_node();
            parent = parent->split_for_insertion(foster_key, new_node);
            inserted = parent->insert(foster_key, foster);
        }

        // Clear foster relationship on child
        child->unlink_foster_child();

        return true;
    }
};

} // namespace foster

#endif

