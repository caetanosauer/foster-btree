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

constexpr size_t DftArrayBytes = 8192;
constexpr size_t DftAlignment = 8;

template<class K, class V, class PMNK_Type>
using KVArray = foster::KeyValueArray<
        K, V, PMNK_Type,
        DftArrayBytes,
        DftAlignment,
        foster::SlotArray,
        foster::BinarySearch,
        foster::DefaultEncoder
    >;


TEST(TestInsertions, MainTest)
{
    KVArray<string, string, uint16_t> kv;
    kv.insert("hello", "world");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("abc", "The quick brown fox jumps over the lazy dog!");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("cde", "TXT");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("Zero Dark Thirty is a movie that starts with Z", "OK");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("empty value", "");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("heyoh", "world");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("hey", "world");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("hallo", "welt");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("Hallo", "Welt");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("Hallo!", "Welt!");
    ASSERT_TRUE(kv.is_sorted());
    kv.insert("hb", "world");
    ASSERT_TRUE(kv.is_sorted());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
