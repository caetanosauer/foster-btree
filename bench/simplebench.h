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

#ifndef FOSTER_SIMPLEBENCH_H
#define FOSTER_SIMPLEBENCH_H

/**
 * \file node.h
 *
 * Classes used to represent a single B-tree node.
 */

#include <cstring>
#include <chrono>
#include <map>
#include <random>
#include <thread>
#include <vector>
#include <future>

#include "slot_array.h"
#include "encoding.h"
#include "search.h"
#include "kv_array.h"
#include "node.h"
#include "node_mgr.h"
#include "pointers.h"
#include "btree_level.h"
#include "btree_static.h"
#include "btree_adoption.h"
#include "latch_mutex.h"

namespace foster {

constexpr size_t DftArrayBytes = 4096;
constexpr size_t DftAlignment = 8;

template<class PMNK_Type>
using SArray = foster::SlotArray<PMNK_Type, DftArrayBytes, DftAlignment>;

template<class K, class V>
using KVArray = foster::KeyValueArray<K, V,
      SArray<uint16_t>,
      foster::BinarySearch<SArray<uint16_t>>,
      foster::CompoundEncoder<foster::AssignmentEncoder<K>, foster::AssignmentEncoder<V>, uint16_t>
      // foster::DefaultEncoder<K, V, uint16_t>
>;

template<class K, class V>
using KVArrayNoPMNK = foster::KeyValueArray<K, V,
      SArray<K>,
      foster::BinarySearch<SArray<K>>,
      foster::CompoundEncoder<foster::AssignmentEncoder<K>, foster::AssignmentEncoder<V>, K>
      // foster::DefaultEncoder<K, V, K>
>;

template<class K, class V>
using BTNode = foster::BtreeNode<K, V,
    KVArray,
    foster::PlainPtr,
    unsigned
>;

template<class K, class V>
using BTNodeNoPMNK = foster::BtreeNode<K, V,
    KVArrayNoPMNK,
    foster::PlainPtr,
    unsigned,
    foster::MutexLatch
>;

template<class Node>
using NodeMgr = foster::BtreeNodeManager<Node, foster::AtomicCounterIdGenerator<unsigned>>;

template<class K, class V, unsigned L>
using BTLevel = foster::BtreeLevel<
    K, V, L,
    BTNode,
    foster::EagerAdoption,
    NodeMgr
>;

template<class K, class V, unsigned L>
using BTLevelNoPMNK = foster::BtreeLevel<
    K, V, L,
    BTNodeNoPMNK,
    foster::EagerAdoption,
    NodeMgr
>;

template<class K, class V, unsigned L>
using SBtree = foster::StaticBtree<K, V, L, BTLevel>;

template<class K, class V, unsigned L>
using SBtreeNoPMNK = foster::StaticBtree<K, V, L, BTLevelNoPMNK>;

template<class T> T convert(int n) { return static_cast<T>(n); }

template<> string convert(int n)
{
    return "keyvalue_" + std::to_string(n);
}

class Stopwatch
{
public:
    Stopwatch() { reset(); }

    void reset()
    {
        start = std::chrono::system_clock::now();
    }

    void dump(string name, string op, int count)
    {
        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "[" << name << "]\t" << op << "s: " << count
            << "\truntime_in_sec: " << elapsed.count() / 1000000.0
            << "\tusec_per_" << op << ": " << (float) elapsed.count() / count
            << std::endl;

        reset();
    }

private:
    std::chrono::system_clock::time_point start;
};

template<class Tree, class K, class V>
struct Insert {
    void operator()(Tree& tree, int i)
    {
        tree.put(convert<K>(i), convert<V>(i));
    }
};

template<class K, class V>
struct Insert<std::map<K,V>, K, V> {
    void operator()(std::map<K, V>& map, int i)
    {
        map[convert<K>(i)] =  convert<V>(i);
    }
};

template<class Tree, class K, class V>
struct Lookup {
    V operator()(Tree& tree, int i)
    {
        V value;
        tree.get(convert<K>(i), value);
        return value;
    }
};

template<class K, class V>
struct Lookup<std::map<K,V>, K, V> {
    V operator()(std::map<K, V>& map, int i)
    {
        return map[convert<K>(i)];
    }
};

} // namespace foster

#endif
