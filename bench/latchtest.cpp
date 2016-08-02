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

#include "latch_mutex.h"

#include <random>

constexpr int num_ops = 1000000;
std::mt19937 rng;

int main(int argc, char** argv)
{
    foster::MutexLatch latch;
    int var = 0;

    unsigned num_threads = 2;
    if (argc > 1) {
        num_threads = atoi(argv[1]);
    }

    auto f = [&var,&latch] {
        int local_var = 0;
        std::uniform_int_distribution<int> should_write(0,1);

        for (int i = 0; i < num_ops; i++) {
            if (should_write(rng)) {
                latch.acquire_write();
                var++;
                latch.release_write();
            } else {
                latch.acquire_read();
                local_var = var;
                local_var++;
                latch.release_read();
            }
        }
    };


    std::vector<std::thread> threads;
    for (unsigned i = 0; i < num_threads; i++) {
        threads.emplace_back(f);
    }
    for (unsigned i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    return 0;
}
