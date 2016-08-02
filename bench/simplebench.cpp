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

#include "simplebench.h"

namespace foster {

template<template<class,class,unsigned> class Btree, unsigned Levels, class K, class V>
void compare_with_std_map(int count)
{
    static_assert(Levels > 0, "Levels must be > 0");

    Stopwatch sw;
    std::mt19937 rng;
    std::uniform_int_distribution<int> gen(0, count);

    {
        Btree<K, V, Levels-1> tree;
        for (int i = 0; i < count; i++) {
            Insert<Btree<K, V, Levels-1>, K, V>{}(tree, i);
        }
        sw.dump("foster", "insert", count);

        Lookup<Btree<K, V, Levels-1>, K, V> lookup;
        for (int i = 0; i < count; i++) {
            int k = gen(rng);
            lookup(tree, k);
        }
        sw.dump("foster", "lookup", count);
    }

    {
        std::map<K, V> map;
        for (int i = 0; i < count; i++) {
            Insert<std::map<K,V>, K, V>{}(map, i);
        }
        sw.dump("std::map", "insert", count);

        Lookup<std::map<K,V>, K, V> lookup;
        for (int i = 0; i < count; i++) {
            int k = gen(rng);
            lookup(map, k);
        }
        sw.dump("sdt::map", "lookup", count);
    }
}

template<template<class,class,unsigned> class Btree, unsigned Levels, class K, class V>
void concurrent_test(int num_threads, int count)
{
    Btree<K, V, Levels-1> tree;

    auto f = [&tree,count] (uint32_t mask) {
        int inserted = 0;

        std::mt19937 rng;
        std::uniform_int_distribution<int> gen(0, count);

        Insert<Btree<K, V, Levels-1>, K, V> insert;
        Lookup<Btree<K, V, Levels-1>, K, V> lookup;

        try {
            while (inserted < count) {
                int k = gen(rng);
                // insert/lookup with 50% chance
                if (k % 2 == 0) {
                    insert(tree, inserted | mask);
                    inserted++;
                }
                else {
                    lookup(tree, k | mask);
                }
            }
        }
        catch (std::runtime_error& e) {
            std::cerr << "oops..." << std::endl;
            std::terminate();
        }
    };

    std::vector<std::thread> threads;
    Stopwatch sw;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(f, i << 24);
    }
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    sw.dump("foster_" + std::to_string(num_threads), "operation", count * num_threads);
}


} // namespace foster

int main(int, char**)
{
    int max = 1000000;
    // std::cout << "=== Integer keys, no PMNK ===" << std::endl;
    // foster::compare_with_std_map<foster::SBtreeNoPMNK, 3, int, int>(max);

    // std::cout << "=== String keys, no PMNK ===" << std::endl;
    // foster::compare_with_std_map<foster::SBtreeNoPMNK, 3, string, string>(max);

    // std::cout << "=== String keys, with PMNK ===" << std::endl;
    // foster::compare_with_std_map<foster::SBtree, 3, string, string>(max);

    for (int i = 1; i <= 8; i++) {
        int num_threads = i;
        foster::concurrent_test<foster::SBtreeNoPMNK, 3, int, int>(num_threads, max/num_threads);
    }
}
