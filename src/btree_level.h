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
 * Types and utilities used to represent a single level in a B-tree data structure.
 */

#include <memory>
#include <limits>
#include <iostream> // for print

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

/**
 * \brief Type function to yield the type of nodes of a given level.
 *
 * The types for all levels are derived from the leaf node type, which is given as a template
 * parameter. The other levels are then determined recursively, which is possible because the type
 * of a parent node is the same as its children, except that the value type is a pointer to the
 * child node. The type of the parent is available in Node::ParentType, which easily allows the
 * recursive computation.
 *
 * For level 0, the LeafType is returned, and this is done with the template specialization below.
 *
 * \tparam L Level for which we want the node type.
 * \tparam LeafType Type of the leaf node.
 */
template <unsigned L, class LeafType>
struct LeveledNode {
    using type = typename LeveledNode<L-1, LeafType>::type::ParentType;
};

template <class LeafType>
struct LeveledNode<0, LeafType> {
    using type = LeafType;
};

/**
 * \brief Class that represents a level of a B-tree data structure.
 *
 * The leaf nodes are at level 0, and the number is incremented for each level towards the root.
 * Foster children are considered of the same level as their foster parents.
 *
 * Because we use a strictly statically typed approach in our design (using templates and
 * metaprogramming), this has the implication that each level of a B-tree contains nodes of a
 * different type. Nodes of level L differ from those of level L-1 in which they contain pointers to
 * nodes of level L-1 instead of pointers to nodes of level L-2 or actual data records (for level
 * 0). This makes it more cumbersome to work with nodes than if we used a dynamic polymorphism
 * approach (i.e., derived classes and virtual methods), but this static style of programming has
 * other advantages (see our documentation in the `doc` directory for a discussion on this). Also
 * \see StaticBtree in btree_static.h.
 *
 * A BtreeLevel object is responsible for managing nodes of a given level through its own NodeMgr.
 * However, it does not hold any node per se -- it can be seen rather as a helper class for managing
 * and performing operations on nodes of the same level.  It supports traversal by maintaining a
 * linked list of level objects from the root down to the leaf level. Such level-dependent traversal
 * logic is required because, again, the node types involved are different on each level.
 *
 * \tparam K The key type.
 * \tparam V The value type
 * \tparam Level Level number.
 * \tparam LeadNode Template for the leaf node class. Upper-level node types are determined
 *      recursively from this.
 * \tparam NodeMgr Template for the node manager object, used to construct and destroy nodes of
 *      this level.
 */
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

    // Type aliases for convenience and external access
    using LeafNodeType = LeafNode<K, V>;
    using ThisNodeType = typename LeveledNode<Level, LeafNodeType>::type;
    using NodePointer = typename ThisNodeType::NodePointer;
    using ChildPointer = typename LeveledNode<Level-1, LeafNodeType>::type::NodePointer;
    using LeafPointer = typename LeveledNode<0, LeafNodeType>::type::NodePointer;
    using ThisType = BtreeLevel<K, V, Level, LeafNode, AdoptionPolicy, NodeMgr>;
    using LowerLevel = BtreeLevel<K, V, Level-1, LeafNode, AdoptionPolicy, NodeMgr>;
    using Adoption = AdoptionPolicy<NodePointer, ChildPointer>;
    using IdType = typename NodeMgr<LeafNodeType>::IdType;

    BtreeLevel(unsigned depth = 0) :
        next_level_(new LowerLevel(depth+1)),
        node_mgr_(NodeMgr<ThisNodeType>{}),
        depth_(depth)
    {
    }

    static constexpr unsigned level() { return Level; }

    LeafPointer traverse(NodePointer branch, const K& key, bool for_update)
    {
        // If this is root node, latch it here
        if (depth_ == 0) { branch->acquire_read(); }

        ChildPointer child {nullptr};

        // Descend into the target child node
        while (branch) {
            // Branch nodes that participate in a traversal must not be empty
            assert<1>(branch->size() > 0 || !branch->is_foster_empty());
            assert<1>(branch->has_reader());

            // If current branch does not contain the key, it must be in a foster child
            if (!branch->key_range_contains(key)) {
                NodePointer foster = branch->get_foster_child();
                assert<1>(foster);
                foster->acquire_read();
                branch->release_read();
                branch = foster;
                continue;
            }

            // find method guarantees that the next child will contain the value (i.e., the branch
            // pointer) associated with the slot where the key would be inserted if not found. The
            // return value (a Boolean "found") can be safely ignored.
            branch->find(key, &child);
            assert<1>(child);

            // Latch child node before proceeding
            latch_pointer(child, for_update);

            // Try do adopt child's foster child -- restart traversal if it works
            if (Adoption::try_adopt(branch, child, node_mgr_)) {
                unlatch_pointer(child, for_update);
                continue;
            }

            break;
        }

        // Release latch on parent
        branch->release_read();

        // Now we found the target child node, but key may be somewhere in the foster chain
        assert<1>(child->fence_contains(key));
        while (child && !child->key_range_contains(key)) {
            ChildPointer foster = child->get_foster_child();
            latch_pointer(foster, for_update);
            unlatch_pointer(child, for_update);
            child = foster;
        }

        assert<1>(child, "Traversal reached null pointer");

        return next_level_->traverse(child, key, for_update);
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

    void print(NodePointer node, std::ostream& out, unsigned rootLevel = Level)
    {
        // Print indentation
        for (int i = 0; i < rootLevel - Level; i++) {
            out << "    ";
        }
        out << "Node " << node->id() << " with " << node->size() << " items" << std::endl;

        NodePointer foster_child = node->get_foster_child();
        while (foster_child) {
            for (int i = 0; i < rootLevel - Level; i++) {
                out << "    ";
            }
            out << "Foster child " << foster_child->id() << " with " << foster_child->size()
                << " items" << std::endl;
            foster_child = foster_child->get_foster_child();
        }

        typename ThisNodeType::Iterator iter = node->iterate();
        ChildPointer child;
        while (iter.next(nullptr, &child)) {
            next_level_->print(child, out, rootLevel);
        }
    }

private:

    void latch_pointer(ChildPointer child, bool ex_mode)
    {
        // Exclusive latch is only required at leaf nodes during normal traversal.
        // (If required, splits, merges, and adoptions will attempt upgrade on branch nodes)
        if (Level == 1 && ex_mode) { child->acquire_write(); }
        else { child->acquire_read(); }
    }

    void unlatch_pointer(ChildPointer child, bool ex_mode)
    {
        // Exclusive latch is only required at leaf nodes during normal traversal.
        // (If required, splits, merges, and adoptions will attempt upgrade on branch nodes)
        if (Level == 1 && ex_mode) { child->release_write(); }
        else { child->release_read(); }
    }

    std::unique_ptr<LowerLevel> next_level_;
    NodeMgr<ThisNodeType> node_mgr_;
    const unsigned depth_;
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

    BtreeLevel(unsigned depth = 0) :
        node_mgr_(NodeMgr<LeafNode<K,V>>{}),
        depth_(depth)
    {
    }

    static constexpr unsigned level() { return 0; }

    NodePointer traverse(NodePointer n, const K&, bool)
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
    void print(NodePointer node, std::ostream& out, unsigned rootLevel = 0)
    {
        // Print indentation
        for (int i = 0; i < rootLevel; i++) {
            out << "    ";
        }
        out << "Leaf node " << node->id() << " with " << node->size() << " items" << std::endl;

        NodePointer foster_child = node->get_foster_child();
        while (foster_child) {
            for (int i = 0; i < rootLevel; i++) {
                out << "    ";
            }
            out << "Foster child " << foster_child->id() << " with " << foster_child->size()
                << " items" << std::endl;
            foster_child = foster_child->get_foster_child();
        }
    }

private:
    NodeMgr<LeafNode<K,V>> node_mgr_;
    const unsigned depth_;
};

} // namespace foster

#endif

