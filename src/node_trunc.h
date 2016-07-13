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

#ifndef FOSTER_BTREE_NODE_TRUNC_H
#define FOSTER_BTREE_NODE_TRUNC_H

/**
 * \file node_trunc.h
 *
 * B-tree node class template supporting prefix truncation.
 */

#include "node.h"

namespace foster {


template <
    class K,
    class V,
    template <class,class> class KeyValueArray,
    template <class> class Pointer,
    class IdType
>
class BtreeNodePrefixTrunc;

template <
    class V,
    template <class,class> class KeyValueArray,
    template <class> class Pointer,
    class IdType
>
class BtreeNodePrefixTrunc<string, V, KeyValueArray, Pointer, IdType> :
    public BtreeNode<string, V, KeyValueArray, Pointer, IdType>
{
public:

    // Type aliases for convenience and external access
    using SuperType = BtreeNode<string, V, KeyValueArray, Pointer, IdType>;
    using ThisType = BtreeNodePrefixTrunc<string, V, KeyValueArray, Pointer, IdType>;
    using NodePointer = Pointer<ThisType>;

    bool insert(const string& key, const V& value)
    {
        return SuperType::insert(truncate(key), value);
    }

    bool find(const string& key, V* value = nullptr)
    {
        return SuperType::find(truncate(key), value);
    }

    void remove(const string& key)
    {
        return SuperType::remove(truncate(key));
    }

    void add_foster_child(NodePointer child)
    {
        SuperType::add_foster_child(Pointer<SuperType>::static_pointer_cast(child));
    }

    /**
     * \brief Simple iterator class to support sequentially reading all key-value pairs
     */
    class Iterator
    {
    public:
        Iterator(ThisType* kv) :
            current_slot_{0}, kv_{kv}
        {}

        bool next(string* key, V* value)
        {
            if (current_slot_ >= kv_->size()) { return false; }

            kv_->read_slot(current_slot_, key, value);
            kv_->add_prefix(*key);
            current_slot_++;

            return true;
        }

    private:
        typename SuperType::SlotNumber current_slot_;
        ThisType* kv_;
    };

    /// \brief Yields an iterator instance to sequentially read all key-value pairs
    Iterator iterate()
    {
        return Iterator{this};
    }


    // TODO this method is basically a copy of unlink_foster_child() of the super-type. We need to
    // do this to make sure that update_fenster is invoked here and not on the super-type. This is
    // one of the disadvantages of template metaprogramming in comparison with dynamic polymorphism
    // (i.e., virtual methods).
    void unlink_foster_child()
    {
        if (!this->get_foster_child()) { return; }

        string low_key, high_key, foster_key;
        this->get_fence_keys(&low_key, &high_key);
        this->get_foster_key(&foster_key);
        string* low_ptr = this->is_low_key_infinity() ? nullptr : &low_key;
        string* high_ptr = this->is_high_key_infinity() ? nullptr : &high_key;
        // If foster child was empty, foster key is not stored and is equal to the high fence key
        string* foster_key_ptr = this->is_foster_empty() ? high_ptr : &foster_key;

        size_t prev_len = this->get_fenster()->get_prefix_size();

        // The old foster key becomes the new high fence key
        bool success = SuperType::update_fenster(low_ptr, foster_key_ptr, nullptr,
               Pointer<SuperType>{nullptr});
        assert<1>(success && !this->get_foster_child(), "Unable to unlink foster child");

        // recompress existing keys if the prefix changed
        size_t new_len = this->get_fenster()->get_prefix_size();

        // prefix may only increase when unlinking foster child
        assert<1>(new_len >= prev_len);
        if (new_len > prev_len) { this->truncate_keys(new_len - prev_len); }
    }

protected:

    const string truncate(const string& key)
    {
        size_t prefix_len = this->get_fenster()->get_prefix_size();
        if (prefix_len == 0) { return key; }
        else { return key.substr(prefix_len); }
    }

    void add_prefix(string& key)
    {
        // TODO this could be optimized to avoid extra copies using string::iterator
        string prefix;
        this->get_fenster()->get_prefix(prefix);
        key.insert(0, prefix);
    }
};

} // namespace foster

#endif
