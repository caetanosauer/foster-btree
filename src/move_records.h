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

#ifndef FOSTER_BTREE_MOVE_RECORDS_H
#define FOSTER_BTREE_MOVE_RECORDS_H

#include "exceptions.h"
#include "assertions.h"

namespace foster {

/**
 * \brief Function template to move key-value pairs between containers.
 *
 * \param[in] dest Key-value array into which pairs will be moved.
 * \param[in] dest_slot Slot position in which pairs will be inserted in the destination array.
 * \param[in] src Source array from which pairs will be moved.
 * \param[in] src_slot First slot to be moved in the source array.
 * \param[in] slot_count Number of slots to move
 * \returns true if movement succeeds; false otherwise
 *
 * One important aspect of this function is that the movement is atomic; i.e., if any of the
 * individual movements fails (possibliy due to lack of free space on destination) then the whole
 * movement is "rolled back" by reinserting the already-moved pairs into the source array.
 */
template <class Encoder>
class RecordMover
{
public:

    template <class Dest, class Src, class SlotNumber = typename Dest::SlotNumber>
    static bool move_records(
            Dest& dest, SlotNumber dest_slot,
            Src& src, SlotNumber src_slot,
            size_t slot_count,
            bool move = true)
    {
        using PayloadPtr = typename Dest::PayloadPtr;

        SlotNumber last_slot = src_slot + slot_count - 1;
        assert<1>(last_slot < src.slot_count());

        bool success = true;
        SlotNumber i = src_slot, j = dest_slot;

        // First copy slots from the source array into the destination one. If an insertion or payload
        // allocation fails in the destination, we break out of the loop and success is set to false.
        while (i <= last_slot) {
            // 1. Insert slot
            success = dest.insert_slot(j);
            if (!success) { break; }

            // 2. Allocate payload
            void* payload_src = src.get_payload_for_slot(i);
            size_t length = Encoder::get_payload_length(payload_src);

            PayloadPtr payload_dest_ptr;
            success = dest.allocate_payload(payload_dest_ptr, length);
            if (!success) {
                dest.delete_slot(j);
                break;
            }

            // 3. Copy slot and payload data into the reserved space.
            dest.get_slot(j) = { src.get_slot(i).key, { payload_dest_ptr, src.get_slot(i).ghost } };
            memcpy(dest.get_payload(payload_dest_ptr), payload_src, length);

            i++;
            j++;
        }

        // If copy failed above, we need to roll back by removing the entries that were already copied
        if (!success) {
            while (j > dest_slot) {
                j--;
                dest.free_payload(dest.get_slot(j).ptr,
                        Encoder::get_payload_length(dest.get_payload_for_slot(j)));
                dest.delete_slot(j);
            }
        }
        // Otherwise, we delete the copied slots on the source array, thus completing the move operation
        else if (move) {
            assert<1>(i == last_slot + 1);
            while (i > src_slot) {
                i--;
                src.free_payload(src.get_slot(i).ptr,
                        Encoder::get_payload_length(src.get_payload_for_slot(i)));
                src.delete_slot(i);
            }
        }

        return success;
    }

};

template <
class Dest,
      class Src,
      class SlotNumber = typename Dest::SlotNumber,
      class PayloadPtr = typename Dest::PayloadPtr
    >
static void copy_records_prealloc(
        Dest& dest, SlotNumber dest_slot, PayloadPtr dest_payload,
        const Src& src, SlotNumber src_slot,
        size_t slot_count)
{
    constexpr size_t PayloadSize = sizeof(typename Dest::PayloadBlock);
    SlotNumber last_slot = src_slot + slot_count - 1;
    assert<1>(last_slot < src.slot_count());

    SlotNumber i = src_slot, j = dest_slot;
    PayloadPtr payload_dest_ptr = dest_payload;

    while (i <= last_slot) {
        const void* payload_src = src.get_payload_for_slot(i);
        size_t length = src.get_payload_length(i);

        dest.get_slot(j) = { src.get_slot(i).key, payload_dest_ptr, src.get_slot(i).ghost };
        memcpy(dest.get_payload(payload_dest_ptr), payload_src, length);

        payload_dest_ptr += length / PayloadSize;
        if (length % PayloadSize != 0) { payload_dest_ptr++; }
        i++;
        j++;
    }
}

template <
      class SlotArray,
      class SlotNumber = typename SlotArray::SlotNumber,
      class PayloadPtr = typename SlotArray::PayloadPtr
    >
static bool preallocate_slots(
        SlotArray& sarray, size_t slot_count, size_t payload_count,
        SlotNumber& slot_dest, PayloadPtr& dest_ptr)
{
    constexpr size_t PayloadSize = sizeof(typename SlotArray::PayloadBlock);
    slot_dest = sarray.slot_count();

    bool success = true;
    SlotNumber inserted = 0;
    while (inserted < slot_count) {
        success = sarray.insert_slot(sarray.slot_count());
        if (!success) { break; }
        inserted++;
    }

    if (success) {
        success = sarray.allocate_payload(dest_ptr, payload_count * PayloadSize);
    }

    if (!success) {
        for (SlotNumber i = 0; i < inserted; i++) {
            sarray.delete_slot(slot_dest);
        }
    }

    return success;
}

} // namespace foster

#endif
