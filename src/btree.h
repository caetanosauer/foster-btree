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

#ifndef FOSTER_BTREE_BTREE_H
#define FOSTER_BTREE_BTREE_H

#include <memory>

#include "assertions.h"

namespace foster {

template <
    class K,
    class V,
    class SArray,
    template <class,class> class Node,
    template <class> class Pointer,
    template <class> class NodeMgr,
    template <class,class> class Adoption
>
class GenericBtree
{
public:

    using NodeMgrType = NodeMgr<SArray>;
    using BranchNode = Node<K, Pointer<SArray>>;
    using LeafNode = Node<K, V>;
    using AdoptionType = Adoption<BranchNode, NodeMgrType>;

    GenericBtree()
    {
        node_mgr_ = std::make_shared<NodeMgrType>();
        adoption_ = std::make_shared<AdoptionType>(node_mgr_);
        root_ = node_mgr_->construct_node();
        BranchNode::initialize(root_);
    }

    ~GenericBtree()
    {
        // TODO free/destroy all nodes
    }

    void put(const K& key, const V& value, bool upsert = false)
    {
        auto leaf = BranchNode::traverse(root_, key, true /* for_update */, adoption_.get());
        assert<1>(LeafNode::is_latched(leaf, true));
        if (upsert) {
            // insert node if not existing, otherwise just update it
            // TODO implement update/overwrite method
            if (LeafNode::find(leaf, key)) { LeafNode::remove(leaf, key); }
        }
        // TODO: Upsert logic should be in Node
        bool inserted = LeafNode::insert(leaf, key, value);
        assert<1>(LeafNode::is_latched(leaf, true));

        while (!inserted) {
            // Node is full -- split required
            assert<1>(LeafNode::is_latched(leaf, true));
            auto new_node = node_mgr_->construct_node();
            LeafNode::split(leaf, new_node);

            // Decide if insertion should go into old or new node
            if (!LeafNode::key_range_contains(leaf, key)) {
                assert<1>(LeafNode::key_range_contains(new_node, key));
                assert<1>(LeafNode::is_latched(leaf, true));
                LeafNode::latch_pointer(new_node, true);
                LeafNode::unlatch_pointer(leaf, true);
                leaf = new_node;
            }

            assert<1>(LeafNode::is_latched(leaf, true));
            inserted = LeafNode::insert(leaf, key, value);
        }

        // TODO latch should be released when pointer is destroyed
        LeafNode::unlatch_pointer(leaf, true);
    }

    bool get(const K& key, V& value)
    {
        auto leaf = BranchNode::traverse(root_, key, false /* for_update */, adoption_.get());
        bool res = LeafNode::find(leaf, key, value);
        LeafNode::unlatch_pointer(leaf, false);
        return res;
    }

    bool remove(const K& key)
    {
        auto leaf = BranchNode::traverse(root_, key, true /* for_update */, adoption_.get());
        bool res = LeafNode::remove(leaf, key);
        LeafNode::unlatch_pointer(leaf, true);
        return res;
    }

private:

    Pointer<SArray> root_;
    std::shared_ptr<NodeMgrType> node_mgr_;
    std::shared_ptr<AdoptionType> adoption_;
};

} // namespace foster

#endif
