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
#include "kv_array.h"
#include "node_refactored.h"
#include "node_foster.h"
#include "pointers.h"

constexpr size_t DftArrayBytes = 8192;
constexpr size_t DftAlignment = 8;

template<class K, class PMNK_Type>
using SArray = foster::SlotArray<
    PMNK_Type,
    DftArrayBytes,
    DftAlignment,
    // base classes
    foster::FosterNodePayloads
>;

template<class K, class V, class PMNK_Type, bool Sorted = true>
using Node = foster::Node<
      K, V,
      foster::BinarySearch<SArray<K, PMNK_Type>>,
      foster::GetEncoder<PMNK_Type>::template type,
      void, // Logger
      Sorted
>;

template <class T>
using NodePointer = foster::PlainPtr<T>;

TEST(TestInsertions, SimpleInsertions)
{
    using N = Node<std::string, std::string, uint16_t>;
    SArray<std::string, uint16_t> node;

    N::insert(node, "key", "value");
    N::insert(node, "key2", "value_2");
    N::insert(node, "key0", "value__0");
    N::insert(node, "key1", "value___1");
    N::insert(node, "key3", "value____3");
    ASSERT_TRUE(N::is_sorted(node));

    // node.print(std::cout);

    string v;
    bool found = N::find(node, "key0", v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, "value__0");
}

TEST(TestInsertions, SimpleInsertionsWithoutPMNK)
{
    using N = Node<int, int, int>;
    SArray<int, int> node;

    N::insert(node, 2, 2000);
    N::insert(node, 0, 0);
    N::insert(node, 1, 1000);
    N::insert(node, 3, 3000);
    N::insert(node, 4, 4000);
    N::insert(node, 5, 5000);
    ASSERT_TRUE(N::is_sorted(node));

    // node.print(std::cout);

    int v;
    bool found = N::find(node, 0, v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, 0);
}

TEST(TestInsertions, SimpleInsertionsWithoutPMNKStringValue)
{
    using N = Node<int, std::string, int>;
    SArray<int, int> node;

    N::insert(node, 2, "200");
    N::insert(node, 0, "00");
    N::insert(node, 1, "10");
    N::insert(node, 3, "3000");
    N::insert(node, 4, "40000");
    N::insert(node, 5, "500000");
    ASSERT_TRUE(N::is_sorted(node));

    // node.print(std::cout);

    string v;
    bool found = N::find(node, 3, v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, "3000");
}

TEST(TestSplit, SimpleSplit)
{
    using BaseN = Node<std::string, std::string, uint16_t>;
    using N = foster::FosterNode<BaseN, NodePointer, foster::AssignmentEncoder>;
    using S = SArray<std::string, uint16_t>;
    S node;

    N::insert(node, "key2", "value_2");
    N::insert(node, "key0", "value__0");
    N::insert(node, "key1", "value___1");
    N::insert(node, "key3", "value____3");
    N::insert(node, "key4", "value_____4");
    N::insert(node, "key5", "value______5");

    S node2;
    N::add_foster_child(node, NodePointer<S>(&node2));

    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));
    EXPECT_TRUE(N::is_low_key_infinity(node2));
    EXPECT_TRUE(N::is_high_key_infinity(node2));
    NodePointer<S> actual_child;
    N::get_foster_child(node, actual_child);
    EXPECT_EQ(actual_child, NodePointer<S>(&node2));

    N::rebalance(node);
    N::get_foster_child(node, actual_child);
    EXPECT_EQ(actual_child, NodePointer<S>(&node2));
    EXPECT_EQ(node.slot_count(), 3);
    EXPECT_EQ(node2.slot_count(), 3);

    string key;
    N::get_foster_key(node, key);
    EXPECT_EQ(key, "key3");
    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));

    N::get_low_key(node2, key);
    EXPECT_EQ(key,  "key3");
    EXPECT_TRUE(N::is_high_key_infinity(node2));

    N::insert(node2, "key6", "value_______6");
    EXPECT_EQ(node2.slot_count(), 4);
    S node3;
    N::add_foster_child(node2, NodePointer<S>(&node3));
    N::rebalance(node2);
    EXPECT_EQ(node2.slot_count(), 2);
    EXPECT_EQ(node3.slot_count(), 2);

    S node4;
    N::add_foster_child(node2, NodePointer<S>(&node4));
    N::rebalance(node2);
    EXPECT_EQ(node2.slot_count(), 1);
    EXPECT_EQ(node4.slot_count(), 1);
}

// TEST(TestSplit, SimpleSplitWithoutPMNK)
// {
//     using N = Node<std::string, std::string, uint16_t>;
//     SArray<std::string, int> node;

//     using NodePointer = BTNodeNoPMNK<int, int>::NodePointer;

//     BTNodeNoPMNK<int, int> node;
//     N::insert(node, 2, 2000);
//     N::insert(node, 0, 0);
//     N::insert(node, 1, 1000);
//     N::insert(node, 3, 3000);
//     N::insert(node, 4, 4000);
//     N::insert(node, 5, 5000);

//     BTNodeNoPMNK<int, int> node2;

//     node.add_foster_child(NodePointer(&node2));
//     ASSERT_TRUE(node.is_low_key_infinity());
//     ASSERT_TRUE(node.is_high_key_infinity());
//     ASSERT_TRUE(node2.is_low_key_infinity());
//     ASSERT_TRUE(node2.is_high_key_infinity());
//     ASSERT_TRUE(node.get_foster_child() == NodePointer(&node2));

//     node.rebalance_foster_child<NodePointer>();
//     EXPECT_TRUE(!node.is_foster_empty());
//     int key;
//     node.get_foster_key(&key);
//     EXPECT_EQ(key, 3);
//     ASSERT_TRUE(node.is_low_key_infinity());
//     ASSERT_TRUE(node.is_high_key_infinity());

//     node2.get_fence_keys(&key, nullptr);
//     ASSERT_TRUE(key == 3);
//     ASSERT_TRUE(node2.is_high_key_infinity());

//     N::insert(node2, 6, 6000);
//     BTNodeNoPMNK<int, int> node3;
//     node2.add_foster_child(NodePointer(&node3));
//     node2.rebalance_foster_child<NodePointer>();

//     BTNodeNoPMNK<int, int> node4;
//     node2.add_foster_child(NodePointer(&node4));
//     node2.rebalance_foster_child<NodePointer>();
// }

// TEST(TestPrefixTruncation, SimpleTruncation)
// {
//     using N = Node<std::string, std::string, uint16_t>;
//     SArray<std::string, int> node;

//     using NodePointer = BTNodeTrunc<int>::NodePointer;

//     BTNodeTrunc<int> node;
//     N::insert(node, "longkeyprefix0", 0);
//     N::insert(node, "longkeyprefix1", 1);
//     N::insert(node, "longkeyprefix2", 2);
//     N::insert(node, "longkeyprefix3", 3);
//     N::insert(node, "longkeyprefix4", 4);
//     N::insert(node, "longkeyprefixF", 95);
//     N::insert(node, "longkeyprefixG", 96);
//     N::insert(node, "longkeyprefixH", 97);
//     N::insert(node, "longkeyprefixI", 98);
//     N::insert(node, "longkeyprefixJ", 99);

//     BTNodeTrunc<int> node2;
//     node.add_foster_child(NodePointer{&node2});
//     node.rebalance_foster_child<NodePointer>();

//     N::insert(node2, "longkeyprefixV", 90);
//     N::insert(node2, "longkeyprefixW", 91);
//     N::insert(node2, "longkeyprefixX", 92);
//     N::insert(node2, "longkeyprefixY", 93);
//     N::insert(node2, "longkeyprefixZ", 94);

//     BTNodeTrunc<int> node3;
//     node2.add_foster_child(NodePointer{&node3});
//     node2.rebalance_foster_child<NodePointer>();

//     // At this point, unlinking the foster child of node 2 should adjust its fence keys to:
//     // low = longkeyprefix5 and high = longkeyprefix90
//     // Which means that only the values A, B, C, ... will be stored
//     node2.unlink_foster_child();

//     N::insert(node2, "longkeyprefixA", 90);
//     N::insert(node2, "longkeyprefixB", 91);
//     N::insert(node2, "longkeyprefixC", 92);
//     N::insert(node2, "longkeyprefixD", 93);
//     N::insert(node2, "longkeyprefixE", 94);

//     // 1) Make sure that a query with the long key returns the expected value
//     int v;
//     bool found;
//     found = N::find(node2, "longkeyprefixA", v);
//     EXPECT_EQ(v, 90); EXPECT_TRUE(found);
//     found = N::find(node2, "longkeyprefixB", v);
//     EXPECT_EQ(v, 91); EXPECT_TRUE(found);
//     found = N::find(node2, "longkeyprefixC", v);
//     EXPECT_EQ(v, 92); EXPECT_TRUE(found);
//     found = N::find(node2, "longkeyprefixD", v);
//     EXPECT_EQ(v, 93); EXPECT_TRUE(found);
//     found = N::find(node2, "longkeyprefixE", v);
//     EXPECT_EQ(v, 94); EXPECT_TRUE(found);

//     // 2) Make sure that the actual keys stored do not contain the prefix
//     auto node2_p = BTNode<string, int>::NodePointer::static_pointer_cast(NodePointer{&node2});
//     found = N::find(*node2_p, "A", v);
//     EXPECT_EQ(v, 90); EXPECT_TRUE(found);
//     found = N::find(*node2_p, "B", v);
//     EXPECT_EQ(v, 91); EXPECT_TRUE(found);
//     found = N::find(*node2_p, "C", v);
//     EXPECT_EQ(v, 92); EXPECT_TRUE(found);
//     found = N::find(*node2_p, "D", v);
//     EXPECT_EQ(v, 93); EXPECT_TRUE(found);
//     found = N::find(*node2_p, "E", v);
//     EXPECT_EQ(v, 94); EXPECT_TRUE(found);

//     // 3) Test iterator
//     auto iter = node2.iterate();
//     string k;
//     char first_char = 'A';
//     int first_v = 90;
//     int i = 0;
//     while (iter.next(&k, &v)) {
//         std::stringstream ss;
//         ss << "longkeyprefix" << static_cast<char>(first_char + i);
//         EXPECT_EQ(k, ss.str());
//         EXPECT_EQ(v, first_v + i);
//         i++;
//     }
// }

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

