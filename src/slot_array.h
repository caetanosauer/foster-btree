/*
 * MIT License
 *
 * Copyright (c) 2016 Caetano Sauer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FOSTER_BTREE_SLOT_ARRAY_H
#define FOSTER_BTREE_SLOT_ARRAY_H

#include <array>
#include <cstring>
#include <cstdint>
// #include <stdexcept>

using std::size_t;

#include <metaprog.h>

#include <iostream>
using std::cout;
using std::endl;

namespace foster {

template<class Key, size_t ArrayBytes = 8192, size_t Alignment = 8>
class SlotArray {
public:

    static constexpr size_t MaxPayloadCount = ArrayBytes / Alignment;
    static constexpr size_t PayloadPtrSize = meta::get_pointer_size(MaxPayloadCount);
    using PayloadPtr = typename meta::UnsignedInteger<PayloadPtrSize>;
    using PayloadBlock = typename std::array<char, Alignment>;

    struct Slot {
        Key key;
        struct {
            PayloadPtr ptr : (PayloadPtrSize*8)-1;
            bool ghost : 1;
        };
    };

    static constexpr size_t MaxSlotCount = ArrayBytes / sizeof(Slot);
    static constexpr size_t SlotPtrSize = meta::get_pointer_size(MaxSlotCount);
    using SlotNumber = typename meta::UnsignedInteger<PayloadPtrSize>;

protected:

    struct alignas(Alignment) HeaderData {
        SlotNumber slot_end;
        PayloadPtr payload_begin;
    };

    // Adjust number of slots and payloads for the header size
    static constexpr size_t SlotCount = (ArrayBytes - sizeof(HeaderData)) / sizeof(Slot);
    static constexpr size_t PayloadCount = (ArrayBytes - sizeof(HeaderData)) / Alignment;

private:

    HeaderData header_;

    union {
        Slot slots_[SlotCount];
        PayloadBlock payloads_[PayloadCount];
    };

    static_assert(ArrayBytes % Alignment == 0,
            "SlotArray template argument error: ArrayBytes must be a multiple of Aligment");
    static_assert(sizeof(HeaderData) % Alignment == 0,
            "SlotArray::HeaderData is not aligned properly");
    static_assert(sizeof(slots_) % Alignment == 0,
            "SlotArray::slots_ is not aligned properly");
    static_assert(sizeof(payloads_) % Alignment == 0,
            "SlotArray::payloads_ is not aligned properly");
    static_assert(sizeof(payloads_) + sizeof(HeaderData) == ArrayBytes,
            "SlotArray takes more space than the given ArrayBytes");

protected:

    static size_t get_payload_count(size_t length)
    {
        return length / sizeof(PayloadBlock) + (length % sizeof(PayloadBlock) != 0);
    }

public:

    SlotArray() : header_{0, PayloadCount}
    {};

    size_t slot_count()
    {
        return header_.slot_end;
    }

    size_t free_space()
    {
        return header_.payload_begin * sizeof(PayloadBlock)
            - header_.slot_end * sizeof(Slot);
    }

    bool allocate_payload(PayloadPtr& ptr, size_t length)
    {
        // We probably also want space for a new slot, so check it here
        if (free_space() < sizeof(Slot) + length) { return false; }
        header_.payload_begin -= get_payload_count(length);
        ptr = header_.payload_begin;
        return true;
    }

    void free_payload(PayloadPtr ptr, size_t length)
    {
        size_t count = get_payload_count(length);
        size_t shift = sizeof(PayloadBlock) * (ptr - header_.payload_begin);
        memmove(&payloads_[header_.payload_begin + count], &payloads_[header_.payload_begin], shift);

        for (SlotNumber i = 0; i < slot_count(); i++) {
            if (slots_[i].ptr < ptr + count) {
                slots_[i].ptr += count;
            }
        }
        header_.payload_begin += count;
    }

    void* get_payload(PayloadPtr ptr)
    {
        return payloads_[ptr].data();
    }

    bool insert_slot(SlotNumber slot)
    {
        if (free_space() < sizeof(Slot)) { return false; }
        memmove(&slots_[slot+1], &slots_[slot], sizeof(Slot) * (slot_count() - slot));
        header_.slot_end++;
        return true;
    }

    void delete_slot(SlotNumber slot)
    {
        if (slot < slot_count() - 1) {
            memmove(&slots_[slot], &slots_[slot+1], sizeof(Slot) * (slot_count() - slot));
        }
        header_.slot_end--;
    }

    Slot& operator[](SlotNumber slot)
    {
        return slots_[slot];
    }

    void print_info()
    {
        cout << "Key size = " << sizeof(Key) << " bytes" << endl;
        cout << "PayloadPtr size = " << sizeof(PayloadPtr) << " bytes" << endl;
        cout << "PayloadPtr size (actual) = " << PayloadPtrSize << " bytes" << endl;
        cout << "Slot size = " << sizeof(Slot) << " bytes" << endl;
        cout << "Array size = " << sizeof(*this) << " bytes (should be " << ArrayBytes << ")"
            << endl;
        for (SlotNumber i = 0; i < slot_count(); i++) {
            cout << "Slot " << i << ": key = " << slots_[i].key
                << " payloadPtr = " << slots_[i].ptr
                << " payload = " << payloads_[slots_[i].ptr].data() << endl;
        }
        cout << "-----------------------------------" << endl;
    }
};

} // namespace foster

#endif
