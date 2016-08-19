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
#include <map>

#include "slot_array.h"
#include "encoding.h"
#include "search.h"
#include "kv_array.h"

constexpr size_t DftArrayBytes = 8192;
constexpr size_t DftAlignment = 8;

template<class PMNK_Type>
using SArray = foster::SlotArray<PMNK_Type, DftArrayBytes, DftAlignment>;

template<class K, class V, class PMNK_Type, bool Sorted = true>
using KVArray = foster::KeyValueArray<K, V,
      SArray<PMNK_Type>,
      foster::BinarySearch<SArray<PMNK_Type>>,
      foster::DefaultEncoder<K, V, PMNK_Type>,
      Sorted
>;


template<class K, class V, class PMNK_Type>
class KVArrayValidator
{
public:

    void insert(const K& key, const V& value)
    {
        map_[key] = value;
        kv_.insert(key, value);
        validate();
    }

    void remove(const K& key)
    {
        map_.erase(key);
        kv_.remove(key);
        validate();
    }

    void validate()
    {
        EXPECT_EQ(kv_.size(), map_.size());

        bool found;
        V value;
        for (auto e : map_) {
            found = kv_.find(e.first, &value);
            ASSERT_TRUE(found);
            EXPECT_EQ(e.second, value);
        }

        EXPECT_TRUE(kv_.is_sorted());
    }

    KVArray<K, V, PMNK_Type>& get_kv() { return kv_; }
    std::map<K, V>& get_map() { return map_; }

private:

    std::map<K, V> map_;
    KVArray<K, V, PMNK_Type> kv_;
};

TEST(TestInsertions, SimpleInsertions)
{
    KVArrayValidator<string, string, uint16_t> kv;
    kv.insert("hello", "world");
    kv.insert("abc", "The quick brown fox jumps over the lazy dog!");
    kv.insert("cde", "TXT");
    kv.insert("Zero Dark Thirty is a movie that starts with Z", "OK");
    kv.insert("empty value", "");
    kv.insert("heyoh", "world");
    kv.insert("hey", "world");
    kv.insert("hallo", "welt");
    kv.insert("Hallo", "Welt");
    kv.insert("Hallo!", "Welt!");
    kv.insert("hb", "world");
}

TEST(TestInsertionsFixedLength, SimpleInsertionsWithPMNK)
{
    KVArrayValidator<uint32_t, uint64_t, uint16_t> kv;
    kv.insert(3, 3000);
    kv.insert(1, 1000);
    kv.insert(2, 2000);
    kv.insert(5, 5000);
    kv.insert(6, 6000);
    kv.insert(4, 4000);
}

TEST(TestInsertionsFixedLength, SimpleInsertionsWithoutPMNK)
{
    KVArrayValidator<uint32_t, uint64_t, uint32_t> kv;
    kv.insert(3, 3000);
    kv.insert(1, 1000);
    kv.insert(2, 2000);
    kv.insert(5, 5000);
    kv.insert(6, 6000);
    kv.insert(4, 4000);
}

TEST(TestDeletions, SimpleDeletions)
{
    KVArrayValidator<string, string, uint16_t> kv;
    kv.insert("a", "value1");
    kv.insert("b", "value2");
    kv.insert("c", "value3");
    kv.insert("d", "value4");
    kv.insert("e", "value5");

    // delete first
    kv.remove("a");

    // delete last
    kv.remove("e");

    // delete middle
    kv.remove("c");

    // delete remaining
    kv.remove("b");
    kv.remove("d");
}

TEST(TestMovement, SimpleMovement)
{
    using namespace foster;

    KVArrayValidator<string, string, uint16_t> kv;
    kv.insert("a", "value1");
    kv.insert("b", "value2");
    kv.insert("c", "value3");
    kv.insert("d", "value4");
    kv.insert("e", "value5");

    KVArrayValidator<string, string, uint16_t> kv2;

    // move d and e
    internal::move_kv_records(kv2.get_kv(), 0, kv.get_kv(), 3, 2);
    kv.get_map().erase("d");
    kv.get_map().erase("e");
    kv.validate();
    kv2.get_map()["d"] = "value4";
    kv2.get_map()["e"] = "value5";
    kv2.validate();

    // move b
    internal::move_kv_records(kv2.get_kv(), 0, kv.get_kv(), 1, 1);
    kv.get_map().erase("b");
    kv.validate();
    kv2.get_map()["b"] = "value2";
    kv2.validate();

    // move a
    internal::move_kv_records(kv2.get_kv(), 0, kv.get_kv(), 0, 1);
    kv.get_map().erase("a");
    kv.validate();
    kv2.get_map()["a"] = "value1";
    kv2.validate();
}

TEST(TestMovement, SimpleMovementWithoutPMNK)
{
    using namespace foster;

    KVArrayValidator<int, int, int> kv;
    kv.insert(1, 1000);
    kv.insert(2, 2000);
    kv.insert(3, 3000);
    kv.insert(4, 4000);
    kv.insert(5, 5000);
    kv.insert(6, 6000);

    KVArrayValidator<int, int, int> kv2;

    // move d and e
    internal::move_kv_records(kv2.get_kv(), 0, kv.get_kv(), 3, 2);
    kv.get_map().erase(4);
    kv.get_map().erase(5);
    kv.validate();
    kv2.get_map()[4] = 4000;
    kv2.get_map()[5] = 5000;
    kv2.validate();

    // move b
    internal::move_kv_records(kv2.get_kv(), 0, kv.get_kv(), 1, 1);
    kv.get_map().erase(2);
    kv.validate();
    kv2.get_map()[2] = 2000;
    kv2.validate();

    // move a
    internal::move_kv_records(kv2.get_kv(), 0, kv.get_kv(), 0, 1);
    kv.get_map().erase(1);
    kv.validate();
    kv2.get_map()[1] = 1000;
    kv2.validate();
}

TEST(TestUnsorted, Sortedness)
{
    KVArray<string, string, uint16_t, false> kv;
    kv.insert("b", "value2");
    kv.insert("e", "value5");
    kv.insert("d", "value4");
    kv.insert("a", "value1");
    kv.insert("c", "value3");

    {
        auto iter = kv.iterate();
        string key, value;
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "b");
        EXPECT_EQ(value, "value2");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "e");
        EXPECT_EQ(value, "value5");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "d");
        EXPECT_EQ(value, "value4");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "a");
        EXPECT_EQ(value, "value1");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "c");
        EXPECT_EQ(value, "value3");
    }

    {
        auto kv_sorted = *kv.convert_to_sorted();
        auto iter = kv_sorted.iterate();
        string key, value;
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "a");
        EXPECT_EQ(value, "value1");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "b");
        EXPECT_EQ(value, "value2");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "c");
        EXPECT_EQ(value, "value3");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "d");
        EXPECT_EQ(value, "value4");
        EXPECT_TRUE(iter.next(&key, &value));
        EXPECT_EQ(key, "e");
        EXPECT_EQ(value, "value5");
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
