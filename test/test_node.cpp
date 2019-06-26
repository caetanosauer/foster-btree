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
#include "node.h"
#include "node_foster.h"
#include "node_mgr.h"
#include "pointers.h"
#include "adoption.h"
#include "latch_mutex.h"

// TODO improve tests by using generic definitions and better paremetrization

constexpr size_t DftArrayBytes = 8192;
constexpr size_t DftAlignment = 8;

template<class PMNK_Type>
using SArray = foster::SlotArray<
    PMNK_Type,
    DftArrayBytes,
    DftAlignment,
    // base classes
    foster::MutexLatch,
    foster::FosterNodePayloads
>;

template<class K, class V, class PMNK_Type, bool Sorted = true>
using Node = foster::Node<
      K, V,
      foster::BinarySearch,
      foster::GetEncoder<PMNK_Type>::template type,
      Sorted
>;

template <class T>
using NodePointer = foster::PlainPtr<T>;

template <class T>
using NodeMgr = foster::BtreeNodeManager<NodePointer<T>>;

struct DummyAdoption
{
    template <typename T>
    bool try_adopt(T&, T, T) { return false; }

    template <typename T>
    bool try_grow(T&) { return false; }
};

TEST(TestInsertions, SimpleInsertions)
{
    using N = Node<std::string, std::string, uint16_t>;
    SArray<uint16_t> n;
    NodePointer<SArray<uint16_t>> node {&n};

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
    SArray<int> n;
    NodePointer<SArray<int>> node {&n};

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
    SArray<int> n;
    NodePointer<SArray<int>> node {&n};

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

template <class K, class V>
using BaseN = Node<K, V, uint16_t>;

TEST(TestSplit, SimpleSplit)
{
    using N = foster::FosterNode<std::string, std::string, BaseN, foster::InlineEncoder>;
    using S = SArray<uint16_t>;
    S n; NodePointer<S> node {&n};

    bool valid;
    string key;
    NodePointer<S> actual_child;

    N::insert(node, "key2", "value_2");
    N::insert(node, "key0", "value__0");
    N::insert(node, "key1", "value___1");
    N::insert(node, "key3", "value____3");
    N::insert(node, "key4", "value_____4");
    N::insert(node, "key5", "value______5");

    S n2; NodePointer<S> node2 {&n2};
    N::add_foster_child(node, node2);

    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));
    EXPECT_TRUE(N::is_low_key_infinity(node2));
    EXPECT_TRUE(N::is_high_key_infinity(node2));

    N::rebalance(node);
    valid = N::get_foster_child(node, actual_child);
    EXPECT_TRUE(valid);
    EXPECT_EQ(actual_child, node2);
    EXPECT_EQ(node->slot_count(), 3);
    EXPECT_EQ(node2->slot_count(), 3);

    valid = N::get_foster_key(node, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key, "key3");
    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));

    valid = N::get_low_key(node2, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key3");
    EXPECT_TRUE(N::is_high_key_infinity(node2));

    N::insert(node2, "key6", "value_______6");
    EXPECT_EQ(node2->slot_count(), 4);

    S n3; NodePointer<S> node3 {&n3};
    N::add_foster_child(node2, node3);
    N::rebalance(node2);

    EXPECT_EQ(node2->slot_count(), 2);
    EXPECT_EQ(node3->slot_count(), 2);
    valid = N::get_low_key(node2, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key3");
    valid = N::get_foster_key(node2, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key5");
    valid = N::get_low_key(node3, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key5");
    EXPECT_TRUE(N::is_high_key_infinity(node2));
    EXPECT_TRUE(N::is_high_key_infinity(node3));
    valid = N::get_foster_child(node2, actual_child);
    EXPECT_TRUE(valid);
    EXPECT_EQ(actual_child, node3);

    S n4; NodePointer<S> node4 {&n4};
    N::add_foster_child(node2, node4);
    N::rebalance(node2);

    EXPECT_EQ(node2->slot_count(), 1);
    EXPECT_EQ(node4->slot_count(), 1);
    valid = N::get_low_key(node2, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key3");
    valid = N::get_foster_key(node2, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key4");
    valid = N::get_low_key(node4, key);
    EXPECT_TRUE(valid);
    EXPECT_EQ(key,  "key4");
    EXPECT_TRUE(N::is_high_key_infinity(node2));
    EXPECT_TRUE(N::is_high_key_infinity(node4));
    valid = N::get_foster_child(node2, actual_child);
    EXPECT_TRUE(valid);
    EXPECT_EQ(actual_child, node4);
}

TEST(TestSplit, SimpleSplitWithoutPMNK)
{
    using N = foster::FosterNode<uint16_t, uint16_t, BaseN, foster::InlineEncoder>;
    using S = SArray<uint16_t>;

    S n; NodePointer<S> node {&n};
    N::insert(node, 2, 2000);
    N::insert(node, 0, 0);
    N::insert(node, 1, 1000);
    N::insert(node, 3, 3000);
    N::insert(node, 4, 4000);
    N::insert(node, 5, 5000);

    S n2; NodePointer<S> node2 {&n2};
    N::add_foster_child(node, node2);

    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));
    EXPECT_TRUE(N::is_low_key_infinity(node2));
    EXPECT_TRUE(N::is_high_key_infinity(node2));
    NodePointer<S> actual_child;
    N::get_foster_child(node, actual_child);
    EXPECT_EQ(actual_child, node2);

    N::rebalance(node);
    N::get_foster_child(node, actual_child);
    EXPECT_EQ(actual_child, node2);
    EXPECT_EQ(node->slot_count(), 3);
    EXPECT_EQ(node2->slot_count(), 3);

    uint16_t key;
    N::get_foster_key(node, key);
    EXPECT_EQ(key, 3);
    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));

    N::get_low_key(node2, key);
    EXPECT_EQ(key,  3);
    EXPECT_TRUE(N::is_high_key_infinity(node2));

    N::insert(node2, 6, 6000);
    EXPECT_EQ(node2->slot_count(), 4);
    S n3; NodePointer<S> node3 {&n3};
    N::add_foster_child(node2, node3);
    N::rebalance(node2);
    EXPECT_EQ(node2->slot_count(), 2);
    EXPECT_EQ(node3->slot_count(), 2);

    S n4; NodePointer<S> node4 {&n4};
    N::add_foster_child(node2, node4);
    N::rebalance(node2);
    EXPECT_EQ(node2->slot_count(), 1);
    EXPECT_EQ(node4->slot_count(), 1);
}

template <typename N, typename K, typename Ptr>
void verify_foster_chain(Ptr node, int count)
{
    EXPECT_TRUE(N::is_low_key_infinity(node));

    Ptr child;
    for (int i = 0; i < count; i++) {
        bool found = N::get_foster_child(node, child);
        ASSERT_TRUE(found);

        K foster_key, low_key;
        found = N::get_foster_key(node, foster_key);
        ASSERT_TRUE(found);
        found = N::get_low_key(child, low_key);
        ASSERT_TRUE(found);
        EXPECT_TRUE(N::is_high_key_infinity(node));
        EXPECT_TRUE(N::is_high_key_infinity(child));
        EXPECT_EQ(foster_key, low_key);

        node = child;
    }

    // foster chain should be over by now
    bool found = N::get_foster_child(node, child);
    EXPECT_TRUE(!found);
}

TEST(TestFosterChain, ManyInsertions)
{
    using N = foster::FosterNode<std::string, std::string, BaseN, foster::InlineEncoder>;
    using S = SArray<uint16_t>;
    NodeMgr<S> node_mgr;

    S n; NodePointer<S> node {&n};
    N::initialize(node);
    int max = 10000;
    int splits = 0;

    for (int i = 0; i < max; i++) {
        auto key = "key" + std::to_string(i);
        auto value = "value" + std::to_string(i);
        auto target = N::traverse(node, key, true, static_cast<DummyAdoption*>(nullptr));
        bool inserted = N::insert(target, key, value);
        if (!inserted) {
            // TODO: yes, there's a memory leak here
            auto new_node = node_mgr.construct_node<N>();
            N::split(target, new_node);
            splits++;

            verify_foster_chain<N, std::string>(node, splits);

            target = N::traverse(node, key, true, static_cast<DummyAdoption*>(nullptr));
            inserted = N::insert(target, key, value);
            EXPECT_TRUE(inserted);
        }
    }
}

TEST(TestGrowth, SimpleGrowth)
{
    using S = SArray<uint16_t>;
    using N = foster::FosterNode<std::string, std::string, BaseN, foster::InlineEncoder>;
    using Branch = foster::FosterNode<std::string, NodePointer<S>, BaseN, foster::InlineEncoder>;
    S n; NodePointer<S> node {&n};

    string key;
    NodePointer<S> actual_child;

    N::insert(node, "key2", "value_2");
    N::insert(node, "key0", "value__0");
    N::insert(node, "key1", "value___1");
    N::insert(node, "key3", "value____3");
    N::insert(node, "key4", "value_____4");
    N::insert(node, "key5", "value______5");

    S n2; NodePointer<S> node2 {&n2};
    N::add_foster_child(node, node2);
    N::rebalance(node);

    S n3; NodePointer<S> node3 {&n3};
    Branch::grow(node, node3);
    EXPECT_EQ(node->slot_count(), 1);
    EXPECT_EQ(node->level(), 1);
    EXPECT_EQ(node3->level(), 0);

    NodePointer<S> ptr;
    std::string min_key = foster::GetMinimumKeyValue<std::string>();
    bool found = BaseN<std::string, NodePointer<S>>::find(node, min_key, ptr);
    EXPECT_TRUE(found);
    EXPECT_EQ(ptr, node3);
    EXPECT_EQ(ptr->slot_count(), 3);

    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));
    EXPECT_TRUE(!N::get_foster_child(node, ptr));
    EXPECT_TRUE(!N::get_foster_key(node, key));

    EXPECT_TRUE(N::is_low_key_infinity(node3));
    EXPECT_TRUE(N::is_high_key_infinity(node3));
    EXPECT_TRUE(N::get_foster_child(node3, ptr));
    EXPECT_EQ(ptr, node2);
    EXPECT_TRUE(N::get_foster_key(node3, key));
    EXPECT_EQ(key, "key3");
}

TEST(TestGrowth, Adoption)
{
    using S = SArray<uint16_t>;
    using N = foster::FosterNode<std::string, std::string,
          BaseN,
          foster::InlineEncoder,
          foster::MutexLatch>;
    using Branch = foster::FosterNode<std::string, NodePointer<S>, BaseN, foster::InlineEncoder>;
    using Adoption = foster::EagerAdoption<Branch, NodeMgr<S>>;
    S n; NodePointer<S> node {&n};

    string key;
    NodePointer<S> actual_child;

    N::insert(node, "key2", "value_2");
    N::insert(node, "key0", "value__0");
    N::insert(node, "key1", "value___1");
    N::insert(node, "key3", "value____3");
    N::insert(node, "key4", "value_____4");
    N::insert(node, "key5", "value______5");

    S n2; NodePointer<S> node2 {&n2};
    N::add_foster_child(node, node2);
    N::rebalance(node);
    S n3; NodePointer<S> node3 {&n3};
    Branch::grow(node, node3);

    NodePointer<S> ptr;
    std::string min_key = foster::GetMinimumKeyValue<std::string>();

    // TODO: must acquire latches so that adoption assertions don't complain
    node->acquire_read();
    node3->acquire_read();
    Adoption adoption {std::make_shared<NodeMgr<S>>()};
    bool adopted = adoption.try_adopt(node, node3);
    EXPECT_TRUE(adopted);

    EXPECT_TRUE(N::is_low_key_infinity(node));
    EXPECT_TRUE(N::is_high_key_infinity(node));
    EXPECT_TRUE(!N::get_foster_child(node, ptr));
    EXPECT_TRUE(!N::get_foster_key(node, key));
    EXPECT_EQ(node->slot_count(), 2);

    EXPECT_TRUE(N::is_low_key_infinity(node3));
    EXPECT_TRUE(!N::get_low_key(node3, key));
    EXPECT_TRUE(!N::is_high_key_infinity(node3));
    EXPECT_TRUE(N::get_high_key(node3, key));
    EXPECT_EQ(key, "key3");
    EXPECT_TRUE(!N::get_foster_child(node3, ptr));
    EXPECT_TRUE(!N::get_foster_key(node3, key));
}

TEST(TestPrefixTruncation, SimpleTruncation)
{
    // using N = Node<std::string, std::string, uint16_t>;
    // using S = SArray<int>;

    // S node;
    // N::insert(node, "longkeyprefix0", 0);
    // N::insert(node, "longkeyprefix1", 1);
    // N::insert(node, "longkeyprefix2", 2);
    // N::insert(node, "longkeyprefix3", 3);
    // N::insert(node, "longkeyprefix4", 4);
    // N::insert(node, "longkeyprefixF", 95);
    // N::insert(node, "longkeyprefixG", 96);
    // N::insert(node, "longkeyprefixH", 97);
    // N::insert(node, "longkeyprefixI", 98);
    // N::insert(node, "longkeyprefixJ", 99);

    // S node2;
    // N::add_foster_child(node, NodePointer{&node2});
    // N::rebalance(node);

    // N::insert(node2, "longkeyprefixV", 90);
    // N::insert(node2, "longkeyprefixW", 91);
    // N::insert(node2, "longkeyprefixX", 92);
    // N::insert(node2, "longkeyprefixY", 93);
    // N::insert(node2, "longkeyprefixZ", 94);

    // S node3;
    // N::add_foster_child(node2, NodePointer{&node3});
    // N::rebalance(node2);

    // // At this point, unlinking the foster child of node 2 should adjust its fence keys to:
    // // low = longkeyprefix5 and high = longkeyprefix90
    // // Which means that only the values A, B, C, ... will be stored
    // N::unset_foster_child(node2);

    // N::insert(node2, "longkeyprefixA", 90);
    // N::insert(node2, "longkeyprefixB", 91);
    // N::insert(node2, "longkeyprefixC", 92);
    // N::insert(node2, "longkeyprefixD", 93);
    // N::insert(node2, "longkeyprefixE", 94);

    // // 1) Make sure that a query with the long key returns the expected value
    // int v;
    // bool found;
    // found = N::find(node2, "longkeyprefixA", v);
    // EXPECT_EQ(v, 90); EXPECT_TRUE(found);
    // found = N::find(node2, "longkeyprefixB", v);
    // EXPECT_EQ(v, 91); EXPECT_TRUE(found);
    // found = N::find(node2, "longkeyprefixC", v);
    // EXPECT_EQ(v, 92); EXPECT_TRUE(found);
    // found = N::find(node2, "longkeyprefixD", v);
    // EXPECT_EQ(v, 93); EXPECT_TRUE(found);
    // found = N::find(node2, "longkeyprefixE", v);
    // EXPECT_EQ(v, 94); EXPECT_TRUE(found);

    // // 2) Make sure that the actual keys stored do not contain the prefix
    // auto node2_p = BTNode<string, int>::NodePointer::static_pointer_cast(NodePointer{&node2});
    // found = N::find(*node2_p, "A", v);
    // EXPECT_EQ(v, 90); EXPECT_TRUE(found);
    // found = N::find(*node2_p, "B", v);
    // EXPECT_EQ(v, 91); EXPECT_TRUE(found);
    // found = N::find(*node2_p, "C", v);
    // EXPECT_EQ(v, 92); EXPECT_TRUE(found);
    // found = N::find(*node2_p, "D", v);
    // EXPECT_EQ(v, 93); EXPECT_TRUE(found);
    // found = N::find(*node2_p, "E", v);
    // EXPECT_EQ(v, 94); EXPECT_TRUE(found);

    // // 3) Test iterator
    // auto iter = node2.iterate();
    // string k;
    // char first_char = 'A';
    // int first_v = 90;
    // int i = 0;
    // while (iter.next(&k, &v)) {
    //     std::stringstream ss;
    //     ss << "longkeyprefix" << static_cast<char>(first_char + i);
    //     EXPECT_EQ(k, ss.str());
    //     EXPECT_EQ(v, first_v + i);
    //     i++;
    // }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

