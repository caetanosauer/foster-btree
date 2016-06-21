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

#ifndef FOSTER_BTREE_BTREE_LEVEL_H
#define FOSTER_BTREE_BTREE_LEVEL_H

/**
 * \file btree_level.h
 *
 * Btree logic built on top of a node data structure with support for foster relationships.
 */

#include <memory>
#include <limits>

#include "metaprog.h"
#include "assertions.h"

namespace foster {

namespace internal {

/**
 * \brief Returns the minimum value of a certain key type.
 *
 * This is used to encode the key associated with the pointer to the first child in a B-tree branch
 * node. Because key-value pairs are not stored on branch nodes, it does not matter whether the
 * minimum key actually exists in the B-tree or not. It is just used to guide the traversal without
 * any special case for the first child pointer. This works because the find method of the node
 * implementation (actually in KeyValueArray) returns the slot number where the key was found or
 * where it would be inserted if it was not found.
 */
template <class K> K GetMinimumKeyValue() { return std::numeric_limits<K>::min(); }
template <> string GetMinimumKeyValue() { return ""; }

} // namespace internal

template <unsigned L, class LeafType>
struct LeveledNode {
    using type = typename LeveledNode<L-1, LeafType>::type::ParentType;
};

template <class LeafType>
struct LeveledNode<0, LeafType> {
    using type = LeafType;
};

template <
    class K,
    class V,
    unsigned Level,
    template <class,class> class LeafNode,
    template <class,class> class AdoptionPolicy,
    template <class> class NodeMgr
>
class BtreeLevel
{
public:

    using LeafNodeType = LeafNode<K, V>;
    using ThisNodeType = typename LeveledNode<Level, LeafNodeType>::type;
    using NodePointer = typename ThisNodeType::NodePointer;
    using ChildPointer = typename LeveledNode<Level-1, LeafNodeType>::type::NodePointer;
    using LeafPointer = typename LeveledNode<0, LeafNodeType>::type::NodePointer;

    using ThisType = BtreeLevel<K, V, Level, LeafNode, AdoptionPolicy, NodeMgr>;
    using LowerLevel = BtreeLevel<K, V, Level-1, LeafNode, AdoptionPolicy, NodeMgr>;

    using Adoption = AdoptionPolicy<NodePointer, ChildPointer>;
    using IdType = typename NodeMgr<LeafNodeType>::IdType;

    BtreeLevel() :
        next_level_(new LowerLevel),
        node_mgr_(NodeMgr<ThisNodeType>{})
    {
    }

    static constexpr unsigned level() { return Level; }

    LeafPointer traverse(NodePointer branch, const K& key)
    {
        ChildPointer child {nullptr};

        // Descend into the target child node
        while (branch) {
            // Branch nodes that participate in a traversal must not be empty
            assert<1>(branch->size() > 0 || !branch->is_foster_empty());

            // If current branch does not contain the key, it must be in a foster child
            if (!branch->key_range_contains(key)) {
                assert<1>(branch->get_foster_child());
                branch = branch->get_foster_child();
                continue;
            }

            // find method guarantees that the next child will contain the value (i.e., the branch
            // pointer) associated with the slot where the key would be inserted if not found. The
            // return value (a Boolean "found") can be safely ignored.
            bool found = branch->find(key, &child);
            if (found) Adoption::try_adopt(branch, child, node_mgr_);
            break;
        }

        // Now we found the target child node, but key may be somewhere in the foster chain
        while (child && !child->key_range_contains(key)) {
            child = child->get_foster_child();
        }

        assert<1>(child, "Traversal reached null pointer");

        return next_level_->traverse(child, key);
    }

    NodePointer construct_node()
    {
        return node_mgr_.construct_node();
    }

    LeafPointer construct_leaf()
    {
        return next_level_->template construct_leaf();
    }

    NodePointer construct_recursively()
    {
        NodePointer node = construct_node();

        ChildPointer child = next_level_->construct_recursively();
        bool inserted = node->insert(internal::GetMinimumKeyValue<K>(), child);
        assert<1>(inserted, "Could not build empty root-to-leaf path");

        return node;
    }

private:

    std::unique_ptr<LowerLevel> next_level_;
    NodeMgr<ThisNodeType> node_mgr_;
};

template <
    class K,
    class V,
    template <class,class> class LeafNode,
    template <class,class> class AdoptionPolicy,
    template <class> class NodeMgr
>
class BtreeLevel<K, V, 0, LeafNode, AdoptionPolicy, NodeMgr>
{
public:

    using NodePointer = typename LeafNode<K,V>::NodePointer;

    BtreeLevel() :
        node_mgr_(NodeMgr<LeafNode<K,V>>{})
    {
    }

    static constexpr unsigned level() { return 0; }

    NodePointer traverse(NodePointer n, const K&)
    {
        return n;
    }

    NodePointer construct_node()
    {
        return node_mgr_.construct_node();
    }

    NodePointer construct_leaf()
    {
        return construct_node();
    }

    NodePointer construct_recursively()
    {
        return construct_node();
    }

private:
    NodeMgr<LeafNode<K,V>> node_mgr_;
};

} // namespace foster

#endif

