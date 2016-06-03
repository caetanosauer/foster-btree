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

#ifndef FOSTER_BTREE_SEARCH_H
#define FOSTER_BTREE_SEARCH_H

/**
 * \file search.h
 *
 * Search algorithms for locating a slot in a slot array given a key.
 */

namespace foster {

/**
 * \brief Simple binary search implementation as a function object.
 *
 * Slot array class is given as template argument, which allows reusing a single search
 * implementation for different slot array implementations.
 */
template <class SArray>
class BinarySearch
{
public:

    using SlotNumber = typename SArray::SlotNumber;
    using Key = typename SArray::KeyType;

    /**
     * \brief Binary search implementation.
     *
     * \param[in] array Slot array in which to perform the search.
     * \param[in] key Key to search for. Type is defined by the slot array.
     * \param[out] ret Slot number on which the given key was found, or on which it would be
     *      inserted, if not found.
     * \param[in] begin,end Range on which to restrict the search. This is useful for node
     *      implementations where some slots on the slot array are reserved for metadata (e.g.,
     *      fence keys or compressed key prefix).
     * \returns True if key was found in the array; false otherwise.
     */
    bool operator()(const SArray& array, const Key& key, SlotNumber& ret,
            SlotNumber begin, SlotNumber end)
    {
        while (begin <= end) {
            SlotNumber mid = begin + (end - begin)/2;
            if (mid == array.slot_count()) {
                ret = mid;
                return false;
            }
            const Key& mid_key = array[mid].key;

            if (key == mid_key) {
                ret = mid;
                return true;
            }
            else if (key < mid_key) {
                if (mid == 0) {
                    // handle underflow of "end"
                    ret = 0;
                    return false;
                }
                end = mid - 1;
            }
            else {
                if (begin == end) {
                    ret = mid+1;
                    return false;
                }
                begin = mid + 1;
                ret = mid + 1;
            }
        }

        return false;
    }

};

} // namespace foster

#endif
