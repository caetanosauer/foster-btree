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

#ifndef FOSTER_LATCH_MUTEX_H
#define FOSTER_LATCH_MUTEX_H

/**
 * \file latch_mutex.h
 *
 * Latch implementation that uses a mutex to syhchronize writers and waiting readers.
 */

#include <atomic>
#include <mutex>
#include <thread>

#include "assertions.h"

namespace foster {

/**
 * \brief Latch implementation that uses a mutex as underlying synchronization primitive.
 *
 * This is borrowed from Shore-MT's w_pthread_lock_t and mcs_lock.
 */
class MutexLatch {
public:

    MutexLatch()
        : counter_(0)
    {}

    bool attempt_read()
    {
        unsigned old_value = counter_;
        if (has_writer(old_value)) { return false; }
        if (!counter_.compare_exchange_weak(old_value, old_value + READER_MASK)) { return false; }

        std::atomic_thread_fence(std::memory_order_acquire);
        return true;
    }

    void acquire_read()
    {
        if(!attempt_read()) {
            {
                std::unique_lock<std::mutex>(mutex_);
                add_when_writer_leaves(READER_MASK);
            }
            std::atomic_thread_fence(std::memory_order_acquire);
        }
    }

    void release_read()
    {
        assert<1>(has_reader());
        std::atomic_thread_fence(std::memory_order_release);
        counter_ -= READER_MASK;
    }

    bool attempt_write(unsigned expected_previous = 0)
    {
        if (counter_ != expected_previous) { return false; }
        if (!mutex_.try_lock()) { return false; }

        bool success = counter_.compare_exchange_weak(expected_previous, WRITER_MASK);
        mutex_.unlock();
        std::atomic_thread_fence(std::memory_order_acquire);

        return success;
    }

    void acquire_write()
    {
        std::unique_lock<std::mutex>(mutex_);
        add_when_writer_leaves(WRITER_MASK);
        assert<1>(has_writer());

        // now spin until all readers leave
        while (has_reader());

        std::atomic_thread_fence(std::memory_order_acquire);
    }

    void release_write()
    {
        std::atomic_thread_fence(std::memory_order_release);
        assert<1>(counter_ == WRITER_MASK);
        counter_ = 0;
    }

    bool attempt_upgrade()
    {
        assert<1>(has_reader());
        return attempt_write(READER_MASK);
    }

    void downgrade()
    {
        std::atomic_thread_fence(std::memory_order_release);
        assert<1>(counter_ == WRITER_MASK);
        counter_ = READER_MASK;
        std::atomic_thread_fence(std::memory_order_acquire);
    }

    bool has_reader()
    {
        return counter_ & ~WRITER_MASK;
    }

    bool has_writer()
    {
        return counter_ & WRITER_MASK;
    }

private:
    std::mutex mutex_;
    std::atomic<unsigned> counter_;

    static constexpr unsigned WRITER_MASK = 0x01;
    static constexpr unsigned READER_MASK = 0x02;

    bool has_writer(unsigned c)
    {
        return c & WRITER_MASK;
    }

    void add_when_writer_leaves(int c)
    {
        // caller must hold mutex!
        // spin on writer and then add given value to counter
        while(has_writer());
        counter_ += c;
    }
};

} // namespace foster

#endif
