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

#ifndef FOSTER_BTREE_BTREE_STATIC_H
#define FOSTER_BTREE_BTREE_STATIC_H

/**
 * \file btree_level.h
 *
 * Btree logic built on top of a node data structure with support for foster relationships.
 */

#include <memory>
#include <iostream> // for print

#include "assertions.h"

namespace foster {

template <
    class K,
    class V,
    unsigned Level,
    template <class,class,unsigned> class BtreeLevel
>
class StaticBtree
{
public:

    template <unsigned L>
    using BtreeLevelType = BtreeLevel<K, V, L>;
    using NodePointer = typename BtreeLevelType<Level>::NodePointer;
    using LeafPointer = typename BtreeLevelType<0>::NodePointer;

    StaticBtree() :
        root_level_(new BtreeLevelType<Level>),
        root_(root_level_->construct_recursively())
    {
    }

    void put(const K& key, const V& value)
    {
        LeafPointer node = root_level_->traverse(root_, key);
        bool inserted = node->insert(key, value);

        while (!inserted) {
            // Node is full -- split required
            LeafPointer new_node = root_level_->construct_leaf();
            node = node->split_for_insertion(key, new_node);
            inserted = node->insert(key, value);
        }
    }

    bool get(const K& key, V& value)
    {
        LeafPointer node = root_level_->traverse(root_, key);
        return node->find(key, &value);
    }

    bool remove(const K& key)
    {
        LeafPointer node = root_level_->traverse(root_, key);
        return node->template remove<false>(key);
    }

    void print(std::ostream& out)
    {
        root_level_->print(root_, out);
    }

private:

    std::unique_ptr<BtreeLevelType<Level>> root_level_;
    NodePointer root_;
};

} // namespace foster

#endif
