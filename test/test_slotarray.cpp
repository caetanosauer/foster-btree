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

#include <slot_array.h>

void test()
{
    foster::SlotArray<uint16_t> test1;
    foster::SlotArray<uint64_t> test2;
    foster::SlotArray<uint64_t, 1048576, 2> test3;
    foster::SlotArray<uint16_t, 1048576, 8> test4;
    foster::SlotArray<uint16_t, 139276, 4> test5;
    foster::SlotArray<uint16_t, 1020, 4> test6;

    char data[6] = {'d', 'a', 't', 'a', '0', '\0'};

    foster::SlotArray<uint16_t>::PayloadPtr ptr;
    for (int i = 0; i < 10; i++) {
        std::cout << "free_space = " << test1.free_space() << endl;
        test1.allocate_payload(ptr, sizeof(data));
        data[4] = '0' + i;
        memcpy(test1.get_payload(ptr), data, sizeof(data));
        test1.insert_slot(i);
        test1[i].ghost = false;
        test1[i].key = 100 + i;
        test1[i].ptr = ptr;
    }
    std::cout << "free_space = " << test1.free_space() << endl;
    test1.print_info();
    test1.free_payload(test1[1].ptr, sizeof(data));
    test1.delete_slot(1);

    EXPECT_EQ(102, test1[1].key);

    std::cout << "free_space = " << test1.free_space() << endl;
    test1.free_payload(test1[2].ptr, sizeof(data));
    test1.delete_slot(2);
    std::cout << "free_space = " << test1.free_space() << endl;
    test1.allocate_payload(ptr, sizeof(data));
    data[4] = 'Z';
    memcpy(test1.get_payload(ptr), data, sizeof(data));
    test1.insert_slot(1);
    test1[1].ghost = false;
    test1[1].key = 666;
    test1[1].ptr = ptr;
    std::cout << "free_space = " << test1.free_space() << endl;

    test1.print_info();
    test2.print_info();
    test3.print_info();
    test4.print_info();
    test5.print_info();
    test6.print_info();
}

TEST(TestSlotArray, MainTest) {
    test();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
