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
#include "node.h"
#include "node_trunc.h"
#include "pointers.h"

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
using KVArrayNoPMNK = foster::KeyValueArray<K, V,
      SArray<K>,
      foster::BinarySearch<SArray<K>>,
      foster::DefaultEncoder<K, V, K>
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
    unsigned
>;

template<class V>
using BTNodeTrunc = foster::BtreeNodePrefixTrunc<string, V,
    KVArray,
    foster::PlainPtr,
    unsigned
>;


TEST(TestInsertions, SimpleInsertions)
{
    BTNode<string, string> node;
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());
    node.insert("key", "value");
    node.insert("key2", "value_2");
    node.insert("key0", "value__0");
    node.insert("key1", "value___1");
    node.insert("key3", "value____3");
    ASSERT_TRUE(node.is_sorted());

    // node.print(std::cout);

    string v;
    bool found = node.find("key0", &v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, "value__0");
}

TEST(TestInsertions, SimpleInsertionsWithoutPMNK)
{
    BTNodeNoPMNK<int,int> node;
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());
    node.insert(2, 2000);
    node.insert(0, 0);
    node.insert(1, 1000);
    node.insert(3, 3000);
    node.insert(4, 4000);
    node.insert(5, 5000);
    ASSERT_TRUE(node.is_sorted());

    // node.print(std::cout);

    int v;
    bool found = node.find(0, &v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, 0);
}

TEST(TestInsertions, SimpleInsertionsWithoutPMNKStringValue)
{
    BTNodeNoPMNK<int,string> node;
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());
    node.insert(2, "200");
    node.insert(0, "00");
    node.insert(1, "10");
    node.insert(3, "3000");
    node.insert(4, "40000");
    node.insert(5, "500000");
    ASSERT_TRUE(node.is_sorted());

    // node.print(std::cout);

    string v;
    bool found = node.find(3, &v);
    ASSERT_TRUE(found);
    ASSERT_EQ(v, "3000");
}

TEST(TestSplit, SimpleSplit)
{
    using NodePointer = BTNode<string, string>::NodePointer;

    BTNode<string, string> node;
    node.insert("key2", "value_2");
    node.insert("key0", "value__0");
    node.insert("key1", "value___1");
    node.insert("key3", "value____3");
    node.insert("key4", "value_____4");
    node.insert("key5", "value______5");

    BTNode<string, string> node2;

    node.add_foster_child(NodePointer(&node2));
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());
    ASSERT_TRUE(node2.is_low_key_infinity());
    ASSERT_TRUE(node2.is_high_key_infinity());
    ASSERT_TRUE(node.get_foster_child() == NodePointer(&node2));

    node.rebalance_foster_child();
    EXPECT_TRUE(!node.is_foster_empty());
    string key;
    node.get_foster_key(&key);
    EXPECT_EQ(key, "key3");
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());

    node2.get_fence_keys(&key, nullptr);
    ASSERT_TRUE(key == "key3");
    ASSERT_TRUE(node2.is_high_key_infinity());

    node2.insert("key6", "value_______6");
    BTNode<string, string> node3;
    node2.add_foster_child(NodePointer(&node3));
    node2.rebalance_foster_child();

    BTNode<string, string> node4;
    node2.add_foster_child(NodePointer(&node4));
    node2.rebalance_foster_child();
}

TEST(TestSplit, SimpleSplitWithoutPMNK)
{
    using NodePointer = BTNodeNoPMNK<int, int>::NodePointer;

    BTNodeNoPMNK<int, int> node;
    node.insert(2, 2000);
    node.insert(0, 0);
    node.insert(1, 1000);
    node.insert(3, 3000);
    node.insert(4, 4000);
    node.insert(5, 5000);

    BTNodeNoPMNK<int, int> node2;

    node.add_foster_child(NodePointer(&node2));
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());
    ASSERT_TRUE(node2.is_low_key_infinity());
    ASSERT_TRUE(node2.is_high_key_infinity());
    ASSERT_TRUE(node.get_foster_child() == NodePointer(&node2));

    node.rebalance_foster_child();
    EXPECT_TRUE(!node.is_foster_empty());
    int key;
    node.get_foster_key(&key);
    EXPECT_EQ(key, 3);
    ASSERT_TRUE(node.is_low_key_infinity());
    ASSERT_TRUE(node.is_high_key_infinity());

    node2.get_fence_keys(&key, nullptr);
    ASSERT_TRUE(key == 3);
    ASSERT_TRUE(node2.is_high_key_infinity());

    node2.insert(6, 6000);
    BTNodeNoPMNK<int, int> node3;
    node2.add_foster_child(NodePointer(&node3));
    node2.rebalance_foster_child();

    BTNodeNoPMNK<int, int> node4;
    node2.add_foster_child(NodePointer(&node4));
    node2.rebalance_foster_child();
}

TEST(TestPrefixTruncation, SimpleTruncation)
{
    using NodePointer = BTNodeTrunc<int>::NodePointer;

    BTNodeTrunc<int> node;
    node.insert("longkeyprefix0", 0);
    node.insert("longkeyprefix1", 1);
    node.insert("longkeyprefix2", 2);
    node.insert("longkeyprefix3", 3);
    node.insert("longkeyprefix4", 4);
    node.insert("longkeyprefixF", 95);
    node.insert("longkeyprefixG", 96);
    node.insert("longkeyprefixH", 97);
    node.insert("longkeyprefixI", 98);
    node.insert("longkeyprefixJ", 99);

    BTNodeTrunc<int> node2;
    node.add_foster_child(NodePointer{&node2});
    node.rebalance_foster_child();

    node2.insert("longkeyprefixV", 90);
    node2.insert("longkeyprefixW", 91);
    node2.insert("longkeyprefixX", 92);
    node2.insert("longkeyprefixY", 93);
    node2.insert("longkeyprefixZ", 94);

    BTNodeTrunc<int> node3;
    node2.add_foster_child(NodePointer{&node3});
    node2.rebalance_foster_child();

    // At this point, unlinking the foster child of node 2 should adjust its fence keys to:
    // low = longkeyprefix5 and high = longkeyprefix90
    // Which means that only the values A, B, C, ... will be stored
    node2.unlink_foster_child();

    node2.insert("longkeyprefixA", 90);
    node2.insert("longkeyprefixB", 91);
    node2.insert("longkeyprefixC", 92);
    node2.insert("longkeyprefixD", 93);
    node2.insert("longkeyprefixE", 94);

    // 1) Make sure that a query with the long key returns the expected value
    int v;
    bool found;
    found = node2.find("longkeyprefixA", &v);
    EXPECT_EQ(v, 90); EXPECT_TRUE(found);
    found = node2.find("longkeyprefixB", &v);
    EXPECT_EQ(v, 91); EXPECT_TRUE(found);
    found = node2.find("longkeyprefixC", &v);
    EXPECT_EQ(v, 92); EXPECT_TRUE(found);
    found = node2.find("longkeyprefixD", &v);
    EXPECT_EQ(v, 93); EXPECT_TRUE(found);
    found = node2.find("longkeyprefixE", &v);
    EXPECT_EQ(v, 94); EXPECT_TRUE(found);

    // 2) Make sure that the actual keys stored do not contain the prefix
    auto node2_p = BTNode<string, int>::NodePointer::static_pointer_cast(NodePointer{&node2});
    found = node2_p->find("A", &v);
    EXPECT_EQ(v, 90); EXPECT_TRUE(found);
    found = node2_p->find("B", &v);
    EXPECT_EQ(v, 91); EXPECT_TRUE(found);
    found = node2_p->find("C", &v);
    EXPECT_EQ(v, 92); EXPECT_TRUE(found);
    found = node2_p->find("D", &v);
    EXPECT_EQ(v, 93); EXPECT_TRUE(found);
    found = node2_p->find("E", &v);
    EXPECT_EQ(v, 94); EXPECT_TRUE(found);

    // 3) Test iterator
    auto iter = node2.iterate();
    string k;
    char first_char = 'A';
    int first_v = 90;
    int i = 0;
    while (iter.next(&k, &v)) {
        std::stringstream ss;
        ss << "longkeyprefix" << static_cast<char>(first_char + i);
        EXPECT_EQ(k, ss.str());
        EXPECT_EQ(v, first_v + i);
        i++;
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

