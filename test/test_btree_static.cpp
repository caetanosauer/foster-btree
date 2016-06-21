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
#include <chrono>

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

constexpr size_t DftArrayBytes = 8192;
constexpr size_t DftAlignment = 8;

template<class PMNK_Type>
using SArray = foster::SlotArray<PMNK_Type, DftArrayBytes, DftAlignment>;

template<class K, class V>
using KVArray = foster::KeyValueArray<K, V,
      SArray<uint16_t>,
      foster::BinarySearch<SArray<uint16_t>>,
      foster::DefaultEncoder<K, V, uint16_t>
>;

template<class K, class V>
using BTNode = foster::BtreeNode<K, V,
    KVArray,
    foster::PlainPtr,
    unsigned
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
using SBtree = foster::StaticBtree<K, V, L, BTLevel>;


TEST(MainTest, TestFewInsertions)
{
    SBtree<string, string, 2> tree;
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

TEST(MainTest, TestManyInsertions)
{
    SBtree<string, string, 1> tree;
    int max = 10000;

    std::map<string, string> map;

    auto start = std::chrono::system_clock::now();

    for (int i = 0; i < max; i++) {
        tree.put("key" + std::to_string(i), "value" + std::to_string(i));
    }

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Keys inserted: " << max << ". Runtime in us: " << elapsed.count() << std::endl;
    std::cout << "Keys inserted per us: " << (float) elapsed.count() / max << std::endl;

    start = std::chrono::system_clock::now();

    for (int i = 0; i < max; i++) {
        map["key" + std::to_string(i)] =  "value" + std::to_string(i);
    }

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "[map] Keys inserted: " << max << ". Runtime in us: " << elapsed.count() << std::endl;
    std::cout << "[map] Keys inserted per us: " << (float) elapsed.count() / max << std::endl;

    // list.print(std::cout);

    for (int i = 0; i < max; i++) {
        string expected = "value" + std::to_string(i);
        string delivered;
        bool found = tree.get("key" + std::to_string(i), delivered);
        EXPECT_TRUE(found);
        EXPECT_EQ(expected, delivered);
    }
}

// TEST(IntegerKeyTest, TestManyInsertions)
// {
//     SBtree<int, int, 2> tree;
//     int max = 10000;

//     auto start = std::chrono::system_clock::now();

//     for (int i = 0; i < max; i++) {
//         tree.put(i, i);
//     }

//     auto end = std::chrono::system_clock::now();
//     auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
//     std::cout << "Keys inserted: " << max << ". Runtime in us: " << elapsed.count() << std::endl;
//     std::cout << "Keys inserted per us: " << (float) elapsed.count() / max << std::endl;

//     // list.print(std::cout);

//     for (int i = 0; i < max; i++) {
//         int delivered;
//         bool found = tree.get(i, delivered);
//         EXPECT_TRUE(found);
//         EXPECT_EQ(i, delivered);
//     }
// }

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

