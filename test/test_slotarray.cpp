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

#include <gtest/gtest.h>
#include <cstring>

#include "slot_array.h"

// Adding a \0 at the end means we don't have to keep track of length to read the payload
char data[6] = {'d', 'a', 't', 'a', '0', '\0'};

template<class T> T get_key(unsigned n) { return static_cast<T>(n); }

template<> string get_key<string>(unsigned n)
{
    return std::to_string(n);
}

template<class T>
void sequential_insertions(T& slots, bool forward, int count = -1)
{
    size_t initial_free_space = slots.free_space();
    size_t one_record_space = 0;

    using PayloadPtr = typename T::PayloadPtr;
    using SlotNumber = typename T::SlotNumber;
    using KeyType = typename T::KeyType;

    PayloadPtr ptr = 0;
    SlotNumber i = 0;

    // Pick increment function depending on forward parameter. Increment if true; do nothing if false.
    auto incr = forward ? [](SlotNumber& i){ i++; } : [](SlotNumber&){};
    unsigned j = 0;

    while (true) {
        bool allocated = slots.allocate_payload(ptr, sizeof(data));
        if (!allocated) { break; }
        data[4] = '0' + (i % 10);
        memcpy(slots.get_payload(ptr), data, sizeof(data));

        bool inserted = slots.insert_slot(i);
        // Array full -- stop
        if (!inserted) {
            slots.free_payload(ptr, sizeof(data));
            break;
        }
        slots[i].ghost = false;
        slots[i].key = get_key<KeyType>(100 + i);
        slots[i].ptr = ptr;

        // EXPECT_EQ(100 + i, slots[i].key);
        EXPECT_TRUE(strncmp(data, (char*) slots.get_payload(ptr), 6) == 0);
        EXPECT_EQ(i + 1, slots.slot_count());

        if (one_record_space == 0) {
            one_record_space = initial_free_space - slots.free_space();
        }
        else {
            EXPECT_TRUE(slots.free_space() == initial_free_space - (i+1) * one_record_space);
        }

        incr(i);
        j++;

        if ((int) j == count) { break; }
    }

    if (count < 0) {
        EXPECT_TRUE(slots.free_space() < one_record_space);
    }
}

template<class T>
void sequential_deletions(T& slots, bool forward, int count = -1)
{
    size_t initial_free_space = slots.free_space();
    size_t one_record_space = 0;
    size_t initial_slot_count = slots.slot_count();

    using SlotNumber = typename T::SlotNumber;
    using KeyType = typename T::KeyType;

    // Pick increment function depending on forward parameter. Increment if false; do nothing if true.
    auto incr = !forward ? [](SlotNumber& i){ i++; } : [](SlotNumber&){};

    SlotNumber i = 0;
    unsigned j = 0;
    while (true) {
        size_t free_space = slots.free_space();
        slots.free_payload(slots[i].ptr, sizeof(data));
        EXPECT_EQ(free_space + slots.get_payload_count(sizeof(data)) * sizeof(typename T::PayloadBlock),
                slots.free_space());

        free_space = slots.free_space();
        slots.delete_slot(i);
        EXPECT_EQ(slots.slot_count(), initial_slot_count - (j + 1));
        EXPECT_EQ(free_space + sizeof(typename T::Slot), slots.free_space());

        if (slots.slot_count() == 0) { break; }
        EXPECT_EQ(slots[i].key, get_key<KeyType>(100 + j + 1));
        data[4] = '0' + ((j + 1) % 10);
        EXPECT_TRUE(strncmp(data, (char*) slots.get_payload(slots[i].ptr), 6) == 0);

        if (one_record_space == 0) {
            one_record_space = slots.free_space() - initial_free_space;
            EXPECT_TRUE(one_record_space >= sizeof(data) + sizeof(typename T::Slot));
        }
        else {
            EXPECT_TRUE(slots.free_space() == initial_free_space + (j+1) * one_record_space);
        }

        incr(i);
        j++;
        if ((int) j == count) { break; }
    }
}

template<class T>
void test()
{
    T slots;
    size_t initial_free_space = slots.free_space();

    sequential_insertions(slots, true);
    sequential_deletions(slots, true);
    EXPECT_EQ(initial_free_space, slots.free_space());
}

TEST(TestSlotArray, MainTest) {
    test<foster::SlotArray<uint16_t>>();
    test<foster::SlotArray<string>>();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
