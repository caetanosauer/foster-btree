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

#ifndef FOSTER_BTREE_NODE_MGR_H
#define FOSTER_BTREE_NODE_MGR_H

/**
 * \file node_mgr.h
 *
 * Management of leaf and branch B-tree nodes
 */

#include <memory> // for std::allocator
#include <type_traits>
#include <atomic>

#include "assertions.h"

namespace foster {

template <class IdType>
class AtomicCounterIdGenerator
{
public:
    using Type = IdType;

    IdType generate()
    {
        // Counter is a local static variable instead of a static member because then it does not
        // have to be defined in a translation unit (i.e., cpp file). This is the only way to
        // guarantee that only one counter will be used, no matter how many units include this
        // header.
        static std::atomic<IdType> counter{0};
        return ++counter;
    }
};

template <
    class Node,
    class IdGenerator,
    class Allocator = std::allocator<Node>
>
class BtreeNodeManager
{
public:

    using IdType = typename IdGenerator::Type;
    using NodePointer = typename Node::NodePointer;
    using KeyType = typename Node::KeyType;

    NodePointer construct_node()
    {
        // allocate space for node and invoke constructor
        void* addr = allocator_.allocate(1 /* number of nodes to allocate */);
        IdType id = idgen_.generate();
        return NodePointer {new (addr) Node(id)};
    }

protected:

    Allocator allocator_;
    IdGenerator idgen_;

};


} // namsepace foster

#endif
