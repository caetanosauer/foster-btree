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
#include <iostream>

#include "slot_array.h"
#include "encoding.h"
#include "search.h"
#include "node.h"
#include "node_foster.h"
#include "node_mgr.h"
#include "pointers.h"
#include "btree.h"

constexpr size_t DftArrayBytes = 4096;
constexpr size_t DftAlignment = 8;

using DftPMNK = uint16_t;

using SArray = foster::SlotArray<
    DftPMNK,
    DftArrayBytes,
    DftAlignment,
    foster::FosterNodePayloads
>;

template<class K, class V>
using Node = foster::Node<
      K, V,
      foster::BinarySearch,
      foster::GetEncoder<DftPMNK>::template type
>;

template <class K, class V>
using FosterNode = foster::FosterNode<
    K, V,
    Node,
    foster::InlineEncoder
>;

template <class T>
using NodePointer = foster::PlainPtr<T>;

template<class Node>
using NodeMgr = foster::BtreeNodeManager<NodePointer<SArray>>;

template <class K, class V>
using Btree = foster::GenericBtree<
    K, V,
    SArray,
    FosterNode,
    NodePointer,
    NodeMgr,
    foster::EagerAdoption
>;


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

#endif
