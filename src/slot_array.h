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

#ifndef FOSTER_BTREE_SLOT_ARRAY_H
#define FOSTER_BTREE_SLOT_ARRAY_H

/**
 * \file slot_array.h
 *
 * Implementation of a basic slot array to support B-tree nodes
 */

#include <array>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <algorithm>
#include <ostream>

// TODO
#include <spdlog/fmt/ostr.h> // required to overload operator<<

#include "assertions.h"
#include "metaprog.h"

using std::size_t;

namespace foster {

/**
 * \brief A container for fixed-length keys and associated, uninterpreted, binary payloads.
 *
 * A slot array is the basic data structure underlying a B-tree node. It can be seen as two
 * mostly independent data structures that share the same fixed-length memory space. These are:
 *
 * 1. A vector of fixed-length *slots*, which contain a key, a ghost bit, and a pointer to a
 *    payload block. This vector starts at the beginning of the allocated space (after the header)
 *    and grows towards the end (i.e., "left to right").
 * 2. A sequence of arbitrary *payloads*, which are uninterpreted byte arrays that are aligned
 *    to boundaries of fixed-length payload blocks. These blocks are allocated sequentially,
 *    growing from the end of the allocated space towards the beginning (i.e., "right to left").
 *
 * A slot array alone is not a suitable data structure to store arbitrary key-value pairs or records
 * (as expected from a general purpose B-tree), because it only supports fixed-length numeric keys
 * and the payloads are not interpreted in any way (for instance, the data structure does not keep
 * track of the payload length associated with each key). It does however provide basic memory
 * management, slot ordering, ghost bits, and key-payload association. For details of how
 * higher-level functionality is implemented based on this class, see, e.g., kv_array.h.
 *
 * The template parameters allow the definition of different slot array layouts.
 * \tparam Key The type used for keys. Must be integral or floating-point, e.g., uint32_t or double.
 * \tparam ArrayBytes The total size of the array in bytes. This will be the exact amount of memory
 *                    occupied by the slot array (i.e., slot vector and payloads) throughout its
 *                    entire life span.
 * \tparam Alignment The size of a payload block. This determines not only the block size used to
 *                   manage payloads, but also the size of a payload pointer that is stored in each
 *                   slot. Therefore, a higher value yields small slots and thus more cache
 *                   efficiency, but it increases wasted space due to fragmentation of payloads. A
 *                   good rule of thumb is 0.001 of the total array size.
 */
template<
    class Key,
    size_t TotalSize = 8192,
    size_t Alignment = 8,
    typename... SuperTypes
>
class alignas(Alignment) SlotArray : public SuperTypes...
{
public:

    using ThisType = SlotArray<Key, TotalSize, Alignment, SuperTypes...>;

    /** @name Compile-time constants and types **/
    /**@{**/
    /** Usable portion of TotalSize that will be used by slots and payloads */
    static constexpr size_t ArrayBytes =
        ((TotalSize - meta::SizeOfTypePack<SuperTypes...>()) / Alignment) * Alignment;
    /** Number of payload blocks that fit into the allocated memory */
    static constexpr size_t MaxPayloadCount = ArrayBytes / Alignment;
    /** Pointer size (in bytes) required to address all possible payload blocks */
    static constexpr size_t PayloadPtrSize = meta::get_pointer_size(MaxPayloadCount);
    /** Type of payload block pointers */
    using PayloadPtr = typename meta::UnsignedInteger<PayloadPtrSize>;
    /** Type of payload blocks (a fixed-length byte array) */
    using PayloadBlock = typename std::array<char, Alignment>;
    /** For acessing the alignment size externally **/
    static constexpr size_t AlignmentSize = Alignment;
    /**@}**/

    /**
     * \brief Type of individual slots in the slot vector.
     *
     * Contains key and a payload pointer, where the last bit is the ghost bit (uses C++ bit fields).
     * Ghost bits are not used internally by this class, but managed exclusively by the caller for
     * its own purposes.
     */
    struct Slot {
        Key key;
        struct {
            PayloadPtr ptr : (PayloadPtrSize*8)-1; /// Uses all bits but one
            bool ghost : 1;
        };
    };


    /** @name Compile-time constants and types **/
    /**@{**/
    /** Number of slots that fit into the allocated memory */
    static constexpr size_t MaxSlotCount = ArrayBytes / sizeof(Slot);
    /** Size (in bytes) of integer variable required to address all slots */
    static constexpr size_t SlotPtrSize = meta::get_pointer_size(MaxSlotCount);
    /** Slot number type (derived from SlotPtrSize */
    using SlotNumber = typename meta::UnsignedInteger<SlotPtrSize>;
    /**@}**/

    /** Type alias for convenience of objects using this class **/
    using KeyType = Key;

protected:

    /**
     * \brief Header object containing metadata about the slot array
     *
     * Manages free space information by keeping a pointer to the first used payload block and the
     * first slot beyond the last used one. Amount of free space is given by converting them to byte
     * offsets and computing the difference (see SlotArray::free_space()).
     */
    struct alignas(Alignment) HeaderData {
        SlotNumber slot_end;
        PayloadPtr payload_begin;
    };

    /** @name Compile-time constants and types **/
    /**@{**/
    /** Actual maximum number of slots, taking space occupied by header into account */
    static constexpr size_t SlotCount = (ArrayBytes - sizeof(HeaderData)) / sizeof(Slot);
    /** Actual maximum number of payload blocks, taking space occupied by header into account */
    static constexpr size_t PayloadCount = (ArrayBytes - sizeof(HeaderData)) / Alignment;
    /**@}**/

private:

    HeaderData header_;

    union {
        Slot slots_[SlotCount];
        PayloadBlock payloads_[PayloadCount];
    };


    /*
     * Compile-time assertions to verify integrity of the template parameters and the resulting
     * class.
     */
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

public:

    /** \brief Default constructor. No arguments required */
    SlotArray() : header_{0, PayloadCount}
    {};

    ~SlotArray() {};

    /** \brief Amount of free space (in bytes) between end of slot vector and begin of payloads. */
    size_t free_space()
    {
        return header_.payload_begin * sizeof(PayloadBlock)
            - header_.slot_end * sizeof(Slot);
    }

    /**
     * @name Payload management methods
     */
    /**@{**/

    /** \brief Helper function to get number of payload blocks required to store 'length' bytes */
    static size_t get_payload_count(size_t length)
    {
        return length / sizeof(PayloadBlock) + (length % sizeof(PayloadBlock) != 0);
    }

    /**
     * \brief Allocates contiguous payload blocks to store a given number of bytes.
     * \param[out] ptr Internal pointer to first allocated payload block.
     * \param[in] length Number of bytes to allocate.
     * \returns true if allocation successfull, false if no free space available.
     */
    bool allocate_payload(PayloadPtr& ptr, size_t length)
    {
        assert<1>(header_.payload_begin <= get_payload_end());

        size_t space_needed = get_payload_count(length) * Alignment;
        if (free_space() < space_needed) { return false; }
        header_.payload_begin -= get_payload_count(length);
        ptr = header_.payload_begin;

        sanity_check();
        return true;
    }

    bool allocate_end_payload(PayloadPtr& ptr, size_t length)
    {
        size_t p_count = get_payload_count(length);
        size_t space_needed = p_count * Alignment;
        if (free_space() < space_needed) { return false; }

        PayloadPtr last_p = get_payload_end();
        PayloadPtr first_p = get_first_payload();
        shift_payloads(first_p - p_count, first_p, last_p - first_p);
        ptr = last_p - p_count;

        sanity_check();
        return true;
    }

    /**
     * \brief Frees contiguous blocks occupied by a given payload.
     * \param[in] ptr Internal pointer to first payload block to be freed.
     * \param[in] length Number of bytes to free (converted internally into number of blocks).
     */
    void free_payload(PayloadPtr ptr, size_t length)
    {
        assert<1>(length <= ArrayBytes);
        assert<1>(ptr >= header_.payload_begin, DBGINFO, "Invalid payload pointer");

        // Move all payloads behind me forward, essentially deleting my blocks.
        size_t shift = get_payload_count(length);
        size_t count = ptr - header_.payload_begin;

        shift_payloads(header_.payload_begin + shift, header_.payload_begin, count);
    }

    /**
     * \brief Shifts payloads analogously to a memmove operation
     * \param[in] to Payload blocks will be moved into this position
     * \param[in} from ... from this position
     * \param[in] count Number of blocks to move
     * \returns If move goes beyond the free space area (which can only happen when to < from), false is
     * returned. Otherwise, move succeeds and true is returned.
     *
     * If movement affects the amount of free space by relocating the first used payload, the amount
     * of free space is adjusted accordingly by de- or incrementing payload_begin.
     */
    bool shift_payloads(PayloadPtr to, PayloadPtr from, size_t count)
    {
        assert<1>(to <= get_payload_end());
        assert<1>(from <= get_payload_end());

        PayloadPtr first_affected = std::min(from, to);
        PayloadPtr last_affected = std::max(from, to) + count - 1;
        int shift = to - from;

        // A negative shift implies growth of the payload area, which means we must have space.
        if (shift < 0 && free_space() < sizeof(PayloadBlock) * (-shift)) {
            return false;
        }

        // Perform the actual movement
        memmove(&payloads_[to], &payloads_[from], count * sizeof(PayloadBlock));
        sanity_check();

        // Adjust pointers in the slot vector to account for the movement above.
        if (count > 0) {
            for (SlotNumber i = 0; i < slot_count(); i++) {
                if (slots_[i].ptr >= first_affected && slots_[i].ptr <= last_affected) {
                    slots_[i].ptr += shift;
                }
            }
            sanity_check();
        }

        // Adjust payload_begin if there was growth or shrinkage
        if (first_affected <= header_.payload_begin) {
            header_.payload_begin += shift;
        }

        sanity_check();
        return true;
    }

    /**
     * \brief Returns position of first payload -- useful for calling shift_payloads()
     */
    PayloadPtr get_first_payload() const
    {
        return header_.payload_begin;
    }

    /**
     * \brief Returns position beyond last payload
     */
    PayloadPtr get_payload_end() const
    {
        return PayloadCount;
    }

    /**
     * \brief Returns a virtual-memory address of a given internal payload pointer.
     *
     * This method is used to copy payload data from the caller's memory into some allocated
     * payload blocks previously allocated.
     *
     * \param[in] ptr Internal payload pointer.
     * \returns Pointer to the payload memory region (as void*)
     */
    void* get_payload(PayloadPtr ptr) { return payloads_[ptr].data(); }
    const void* get_payload(PayloadPtr ptr) const { return payloads_[ptr].data(); }
    void* get_payload_for_slot(SlotNumber slot) { return get_payload(get_slot(slot).ptr); }
    const void* get_payload_for_slot(SlotNumber slot) const { return get_payload(get_slot(slot).ptr); }

    /**@}**/

    /**
     * @name Slot-vector management methods
     */
    /**@{**/

    /** \brief Number of slots currently stored in the slot vector */
    size_t slot_count() const
    {
        return header_.slot_end;
    }

    /**
     * \brief Insert a new empty slot into a given position, shifting other slots to make room.
     * \param[in] slot Position in which to insert the new slot.
     * \returns true if insertion succeeded; false if no free space available for a new slot.
     * \throws AssertionFailure if slot number is greater than the slot count (if it's equal, then
     *      it's an append)
     */
    bool insert_slot(SlotNumber slot)
    {
        assert(slot <= slot_count(), DBGINFO, "Slot number out of bounds");

        if (free_space() < sizeof(Slot)) { return false; }
        memmove(&slots_[slot+1], &slots_[slot], sizeof(Slot) * (slot_count() - slot));
        header_.slot_end++;

        // Non-numeric keys should be empty-constructed
        if (!std::is_integral<Key>::value && !std::is_floating_point<Key>::value) {
            new (&slots_[slot]) Key;
        }

        sanity_check();
        return true;
    }

    /**
     * \brief Deletes a slot from a given position, shitfing other slots if necessary.
     * \param[in] slot Position of slot to be deleted.
     */
    void delete_slot(SlotNumber slot)
    {
        if (slot < slot_count() - 1) {
            memmove(&slots_[slot], &slots_[slot+1], sizeof(Slot) * (slot_count() - slot));
        }
        header_.slot_end--;

        sanity_check();
    }

    bool slots_are_sorted() const
    {
        Key prev;
        for (SlotNumber i = 1; i < slot_count(); i++) {
            if (i == 1) { prev = slots_[0].key; }
            if (slots_[i].key < prev) { return false; }
            prev = slots_[i].key;
        }
        sanity_check();
        return true;
    }

    /** \brief Provides access to slot in the given position.  */
    Slot& get_slot(SlotNumber slot) { return slots_[slot]; }
    const Slot& get_slot(SlotNumber slot) const { return slots_[slot]; }
    /** \brief Convenience alias for slot() **/
    Slot& operator[](SlotNumber slot) { return get_slot(slot); }
    const Slot& operator[](SlotNumber slot) const { return get_slot(slot); }

    /**@}**/

    /** \brief Print method used for debugging */
    template <typename OS>
    friend OS& operator<<(OS& out, const ThisType& s)
    {
        using std::endl;
        out << "Key size = " << sizeof(Key) << " bytes" << endl;
        out << "PayloadPtr size = " << sizeof(PayloadPtr) << " bytes" << endl;
        out << "PayloadPtr size (actual) = " << PayloadPtrSize << " bytes" << endl;
        out << "Slot size = " << sizeof(Slot) << " bytes" << endl;
        out << "Array size = " << sizeof(s) << " bytes (should be " << ArrayBytes << ")"
            << endl;
        for (SlotNumber i = 0; i < s.slot_count(); i++) {
            out << "Slot " << i << ": key = " << s.slots_[i].key
                << " payloadPtr = " << s.slots_[i].ptr
                << " payload = " << s.payloads_[s.slots_[i].ptr].data() << endl;
        }
        out << "-----------------------------------" << endl;
        return out;
    }

    void sort_slots()
    {
        auto cmp = [](const Slot& a, const Slot& b) { return a.key < b.key; };
        std::stable_sort(slots_, slots_ + slot_count(), cmp);

        sanity_check();
    }

    void sanity_check() const
    {
        assert<1>(header_.payload_begin <= get_payload_end());
    }

};

} // namespace foster

#endif
