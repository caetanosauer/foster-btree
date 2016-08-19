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

#include "encoding.h"

char TEST_PAGE[8192];

TEST(TestStringPMNK, TestPMNK)
{
    foster::PMNKEncoder<string, uint16_t> enc;

    EXPECT_TRUE(enc.get_pmnk("abc") < enc.get_pmnk("acb"));
    EXPECT_TRUE(enc.get_pmnk("abc") < enc.get_pmnk("cba"));
    EXPECT_TRUE(enc.get_pmnk("acb") < enc.get_pmnk("cba"));
}

TEST(TestTupleInline, TestVariadic)
{
    std::tuple<int, string, double, string> t =
        std::make_tuple(4711, "second field", 3.14, "fourth element");
    foster::InlineEncoder<decltype(t)> enc;

    std::cout << "Tuple encoded size = " << enc.get_payload_length(t) << std::endl;
    enc.encode(TEST_PAGE, t);

    decltype(t) decoded_t;
    enc.decode(TEST_PAGE, &decoded_t);

    EXPECT_EQ(decoded_t, t);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

