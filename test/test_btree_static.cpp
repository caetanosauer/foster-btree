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

#define ENABLE_TESTING

#include <gtest/gtest.h>
#include <cstring>

#include "slot_array.h"
#include "encoding.h"
#include "search.h"
#include "node_refactored.h"
#include "node_foster.h"
#include "node_mgr.h"
#include "pointers.h"
#include "btree.h"
#include "latch_mutex.h"

constexpr size_t DftArrayBytes = 4096;
constexpr size_t DftAlignment = 8;

using DftPMNK = uint16_t;

using SArray = foster::SlotArray<
    DftPMNK,
    DftArrayBytes,
    DftAlignment,
    // base classes
    foster::FosterNodePayloads,
    foster::MutexLatch
>;

template<class K, class V>
using Node = foster::Node<
      K, V,
      foster::BinarySearch<SArray>,
      foster::GetEncoder<DftPMNK>::template type,
      void // Logger
>;

template <class K, class V>
using FosterNode = foster::FosterNode<K, V, Node, foster::AssignmentEncoder>;

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

TEST(MainTest, SimpleInsertions)
{
    Btree<string, string> tree;
    tree.put("key", "value");
    tree.put("key2", "value_2");
    tree.put("key0", "value__0");
    tree.put("key1", "value___1");
    tree.put("key3", "value____3");

    // node.print(std::cout);

    string v;
    bool found = tree.get("key0", v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, "value__0");
}

TEST(MainTest, ManyInsertions)
{
    Btree<string, string> tree;
    int max = 10000;

    for (int i = 0; i < max; i++) {
        tree.put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    // list.print(std::cout);

    for (int i = 0; i < max; i++) {
        string expected = "value" + std::to_string(i);
        string delivered;
        bool found = tree.get("key" + std::to_string(i), delivered);
        ASSERT_TRUE(found);
        EXPECT_EQ(expected, delivered);
    }
}

TEST(DeletionTest, ManyDeletions)
{
    Btree<string, string> tree;

    // insert all keys from 0 to max-1
    int max = 1000;
    for (int i = 0; i < max; i++) {
        tree.put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    // delete all even keys
    for (int i = 0; i < max; i += 2) {
        tree.remove("key" + std::to_string(i));
    }

    // verify that only odd keys are present
    for (int i = 0; i < max; i++) {
        string expected = "value" + std::to_string(i);
        string delivered;
        bool found = tree.get("key" + std::to_string(i), delivered);

        if (i % 2 == 1) {
            EXPECT_TRUE(found);
            EXPECT_EQ(expected, delivered);
        }
        else { EXPECT_TRUE(!found); }
    }
}

TEST(IntegerKeyTest, ManyInsertions)
{
    Btree<int, int> tree;
    int max = 10000;

    for (int i = 0; i < max; i++) {
        tree.put(i, i);
    }

    // tree.print(std::cout);

    for (int i = 0; i < max; i++) {
        int delivered;
        bool found = tree.get(i, delivered);
        ASSERT_TRUE(found);
        ASSERT_EQ(i, delivered);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


