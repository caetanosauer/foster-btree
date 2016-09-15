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

#ifndef FOSTER_BTREE_METAPROG_H
#define FOSTER_BTREE_METAPROG_H

/**
 * \file metaprog.h
 *
 * Defines meta-programming utilities such as constexpr functions, type traits, and type functions.
 * These are useful for writing generic and reusable template classes such as SlotArray.
 *
 * These utilities are all defined in the namespace \ref foster::meta.
 */

#include <type_traits>
#include <cstdint>
#include <array>

namespace foster { namespace meta {

/**
 * \brief Calculates number of bytes required to address n elements.
 *
 * Helper constexpr function (to use only at compile time) to calculate the pointer size (in bytes)
 * required to address n elements. Because this is a compile-time function, it must use recursion
 * and a single return statement.
 **/
constexpr size_t get_pointer_size(size_t n, size_t iter = 0)
{
    /*
     * Writing the function with recursion in functional style makes it a bit difficult to
     * understand, but here's how to break it down:
     * 1) The first line computes log2(N) by getting the position of the highest 1-bit. This is done
     * by shifting N until it reaches zero.  The number of shifts performed is then the result.
     * 2) The second line computes ceil(log2(N)), which is the pointer size required to address N
     * elements. This is done by adding 1 to the result if any bit other than the highest 1-bit is
     * also set. The result is finally divided by 8 to get the number of bytes instead of bits.
     */
    return ((n >> iter) > 0) ? get_pointer_size(n, iter+1)
        : iter / 8 + (iter % 8 != 0);
}

/**
 * \typedef SelectType
 * \brief Selects among multiple types using an enumerator.
 *
 * Type function to allow choosing among multiple types depending on the value of an integer, like a
 * switch statement. The recursive function SelectTypeImpl performs the type choice.
 *
 * For an example, see UnsignedInteger below.
 * (stolen from Stroustroup's book)
 */
template<unsigned N, typename ... Cases> // general case; never instantiated
struct SelectTypeImpl;

template<unsigned N, typename T, typename ... Cases> // recursion step: N > 0
struct SelectTypeImpl<N, T, Cases...> : SelectTypeImpl<N-1, Cases...> { };

template<typename T, typename ... Cases> // final case: N==0
struct SelectTypeImpl<0, T, Cases...> {
    using type = T;
};

template<unsigned N, typename... Cases>
using SelectType = typename SelectTypeImpl<N, Cases...>::type;

/**
 * \typedef MetaIntegerImpl
 * \brief Type function that yields a signed or unsigned integer of N bytes.
 *
 * Instead of using this directly, use the aliases SignedInteger and UnsignedInteger. Example:
 *
 *     UnsignedInteger<8> myLongInt = 4711;
 *
 * The point of using this type function is that it allows choosing among integer types at compile
 * time, using metaprogramming. The argument 8 on the example above, for instance, could be derived
 * from a template parameter or a constexpr expression.
 *
 * Only integers of 1, 2, 4, and 8 bytes are supported. If N = 6, for example, an 8-byte integer is
 * returned.
 *
 * (partially stolen from Stroustroup's book)
 **/
template<bool Signed, int N>
struct MetaIntegerImpl {
    using type = typename std::conditional<Signed,
      SelectType<N, void, int8_t, int16_t, int32_t, int32_t, int64_t, int64_t, int64_t, int64_t>,
      SelectType<N, void, uint8_t, uint16_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t,
        uint64_t>>::type;
};

template<int N>
using UnsignedInteger = typename MetaIntegerImpl<false, N>::type;

template<int N>
using SignedInteger = typename MetaIntegerImpl<true, N>::type;

/**
 * Declare the given type T only if the passed condition is true.
 */
template<bool Condition, typename T = void>
using EnableIf = typename std::enable_if<Condition, T>::type;

/**
 * Return size of the given object types aligned to the given block size.
 */
template<unsigned Alignment, typename ... T>
constexpr unsigned AlignedSizeOf() {
    return Alignment *
            (sizeof...(T) / Alignment + (sizeof...(T) % Alignment > 0 ? 1 : 0));
}

/**
 * Declare a char array to fill data structures into a given alignment.
 *
 * Example:
 * struct MySizeIs16 {
 *      SomeClass c;
 *      Padding<16 - sizeof(SomeClass)> _fill;
 * };
 *
 * In the example above, we make sure that objects of type MySizeIs16 are exactly
 * 16 bytes long, regardless of the size of SomeClass. If the given N is negative,
 * an error will be thrown when instantiating std::array.
 */
template<unsigned N>
struct PaddingImpl {
    using type = meta::EnableIf<N != 0, std::array<char, N>>;
};

template<unsigned N>
using Padding = typename PaddingImpl<N>::type;

/**
 * Variadic version of Padding that uses AlignedSizeOf
 */
template<unsigned N, typename ... T>
using TypePadding = typename PaddingImpl<AlignedSizeOf<N, T...>() - sizeof...(T)>::type;

template<size_t N, typename... Types> // general case; never instantiated
struct SizeOfTypePackImpl;

template<size_t N, typename Head, typename... Tail>
struct SizeOfTypePackImpl<N, Head, Tail...> {
    static constexpr size_t size = sizeof(Head) + SizeOfTypePackImpl<N-1, Tail...>::size;
};

template<typename... Tail>
struct SizeOfTypePackImpl<0, Tail...> {
    static constexpr size_t size = 0;
};

template <typename... Types>
constexpr size_t SizeOfTypePack()
{
    return SizeOfTypePackImpl<sizeof...(Types), Types...>::size;
}

}} // namespace foster::meta

#endif
