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
#include "pointers.h"

constexpr size_t DftArrayBytes = 8192;
constexpr size_t DftAlignment = 8;

template<class PMNK_Type>
using SArray = foster::SlotArray<PMNK_Type, DftArrayBytes, DftAlignment>;

template<class K, class V, class PMNK_Type>
using KVArray = foster::KeyValueArray<K, V,
      SArray<PMNK_Type>,
      foster::BinarySearch<SArray<PMNK_Type>>,
      foster::DefaultEncoder<K, V, PMNK_Type>
>;

template<class K, class V, class PMNK_Type>
using BTNode = foster::BtreeNode<
    unsigned,
    foster::PlainPtr,
    KVArray<K, V, PMNK_Type>,
    foster::DefaultEncoder
>;


TEST(TestInsertions, MainTest)
{
    BTNode<string, string, uint16_t> node;
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

TEST(TestSplit, MainTest)
{
    using NodePointer = BTNode<string, string, uint16_t>::NodePointer;

    BTNode<string, string, uint16_t> n1;
    BTNode<string, string, uint16_t> n2;
    BTNode<string, string, uint16_t> n3;
    n1.add_foster_child(NodePointer(&n2));
    n1.add_foster_child(NodePointer(&n3));

    BTNode<string, string, uint16_t> node;
    node.insert("key2", "value_2");
    node.insert("key0", "value__0");
    node.insert("key1", "value___1");
    node.insert("key3", "value____3");
    node.insert("key4", "value_____4");
    node.insert("key5", "value______5");

    BTNode<string, string, uint16_t> node2;

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
    BTNode<string, string, uint16_t> node3;
    node2.add_foster_child(NodePointer(&node3));
    node2.rebalance_foster_child();

    BTNode<string, string, uint16_t> node4;
    node2.add_foster_child(NodePointer(&node4));
    node2.rebalance_foster_child();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

