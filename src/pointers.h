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

#ifndef FOSTER_BTREE_POINTERS_H
#define FOSTER_BTREE_POINTERS_H

/**
 * \file pointers.h
 *
 * Defines handler classes that can be used as smart (or dumb) pointers to objects, similar to
 * shared_ptr and unique_ptr.
 */

#include "assertions.h"

namespace foster {

template <class T>
class PlainPtr {
public:

    using PointeeType = T;

    explicit PlainPtr(T* p = nullptr) : ptr_(p) {}

    template <typename OtherT, typename = typename
        std::enable_if<std::is_convertible<OtherT*, T*>::value>::type>
    PlainPtr(const PlainPtr<OtherT>& other)
        : PlainPtr<T>(other)
    {}

    template <typename OtherT, typename = typename
        std::enable_if<std::is_convertible<OtherT*, T*>::value>::type>
    PlainPtr(const PlainPtr<OtherT>&& other)
        : PlainPtr<T>(std::move(other))
    {}

    template <typename OtherT>
    PlainPtr& operator=(const PlainPtr<OtherT>& other)
    {
        this->ptr_ = other.ptr_;
        return *this;
    }

    template <typename OtherT>
    PlainPtr& operator=(const PlainPtr<OtherT>&& other)
    {
        this->ptr_ = std::move(other.ptr_);
        return *this;
    }

    PlainPtr& operator=(void* raw_ptr)
    {
        this->ptr_ = reinterpret_cast<T*>(raw_ptr);
        return *this;
    }

    operator bool() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    bool operator!() const { return !ptr_; }
    bool operator==(const PlainPtr& other) const { return other.ptr_ == ptr_; }

    operator void*() { return ptr_; }

    friend std::ostream& operator<< (std::ostream& out, const PlainPtr<T>& ptr)
    {
        return out << ptr.ptr_;
    }

    template <class OtherT>
    static PlainPtr<T> static_pointer_cast(PlainPtr<OtherT> other)
    {
        return PlainPtr<T>(static_cast<T*>(other.operator void*()));
    }

private:
    T* ptr_;
};

} // namespace foster

#endif
