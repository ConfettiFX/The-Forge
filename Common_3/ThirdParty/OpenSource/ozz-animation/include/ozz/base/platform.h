//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2017 Guillaume Blanc                                         //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#ifndef OZZ_OZZ_BASE_PLATFORM_H_
#define OZZ_OZZ_BASE_PLATFORM_H_

#include <stdint.h>
#include <cassert>
#include <cstddef>

namespace ozz {

// Compile time string concatenation.
#define OZZ_JOIN(_a, _b) _OZZ_JOIN(_a, _b)
// Compile time string concatenation implementation details.
#define _OZZ_JOIN(_a, _b) _OZZ_JOIN2(_a, _b)
#define _OZZ_JOIN2(_a, _b) _a##_b

// Compile time assertion. Breaks compiling if _condition is false.
// Defines an array with a negative number of elements if _condition is false,
// which generates a compiler error.
#define OZZ_STATIC_ASSERT(_condition)           \
  struct OZZ_JOIN(_StaticAssert, __COUNTER__) { \
    char x[(_condition) ? 1 : -1];              \
  }

// Gets alignment in bytes required for any instance of the given type.
// Usage is OZZ_ALIGN_OF(MyStruct).
namespace internal {
template <typename _Ty>
struct AlignOf {
  static const size_t value = sizeof(_Ty) ^ (sizeof(_Ty) & (sizeof(_Ty) - 1));
};
}  // namespace internal
#define OZZ_ALIGN_OF(_Ty) ozz::internal::AlignOf<_Ty>::value

// Finds the number of elements of a statically allocated array.
#define OZZ_ARRAY_SIZE(_array) (sizeof(_array) / sizeof(_array[0]))

// Specifies a minimum alignment (in bytes) for variables.
// Syntax is: "OZZ_ALIGN(16) int i;" which aligns "i" variable address to 16
// bytes.
#if defined(_MSC_VER)
#define OZZ_ALIGN(_alignment) __declspec(align(_alignment))
#else
#define OZZ_ALIGN(_alignment) __attribute__((aligned(_alignment)))
#endif

// Instructs the compiler to try to inline a function, regardless cost/benefit
// compiler analysis.
// Syntax is: "OZZ_INLINE void function();"
#if defined(_MSC_VER)
#define OZZ_INLINE __forceinline
#else
#define OZZ_INLINE inline __attribute__((always_inline))
#endif

// Tells the compiler to never inline a function.
// Syntax is: "OZZ_NO_INLINE void function();"
#if defined(_MSC_VER)
#define OZZ_NOINLINE __declspec(noinline)
#else
#define OZZ_NOINLINE __attribute__((noinline))
#endif

// Tells the compiler that the memory addressed by the restrict -qualified
// pointer is not aliased, aka no other pointer will access that same memory.
// Syntax is: void function(int* OZZ_RESTRICT _p);"
#define OZZ_RESTRICT __restrict

// Defines macro to help with DEBUG/NDEBUG syntax.
#if defined(NDEBUG)
#define OZZ_IF_DEBUG(...)
#define OZZ_IF_NDEBUG(...) __VA_ARGS__
#else  // NDEBUG
#define OZZ_IF_DEBUG(...) __VA_ARGS__
#define OZZ_IF_NDEBUG(...)
#endif  // NDEBUG

// Case sensitive wildcard string matching:
// - a ? sign matches any character, except an empty string.
// - a * sign matches any string, including an empty string.
bool strmatch(const char* _str, const char* _pattern);

// Offset a pointer from a given number of bytes.
template <typename _Ty>
_Ty* PointerStride(_Ty* _ty, size_t _stride) {
  return reinterpret_cast<_Ty*>(reinterpret_cast<uintptr_t>(_ty) + _stride);
}

// Defines a range [begin,end[ of objects ot type _Ty.
template <typename _Ty>
struct Range {
  // Default constructor initializes range to empty.
  Range() : begin(NULL), end(NULL) {}

  // Constructs a range from its extreme values.
  Range(_Ty* _begin, const _Ty* _end) : begin(_begin), end(_end) {
    assert(_begin <= _end && "Invalid range.");
  }

  // Construct a range from a pointer to a buffer and its size, ie its number of
  // elements.
  Range(_Ty* _begin, size_t _size) : begin(_begin), end(_begin + _size) {}

  // Construct a range from a single element.
  Range(_Ty& _element) : begin(&_element), end((&_element) + 1) {}

  // Construct a range from an array, its size is automatically deduced.
  // It isn't declared explicit as conversion is free and safe.
  template <size_t _size>
  Range(_Ty (&_array)[_size]) : begin(_array), end(_array + _size) {}

  // Reset range to empty.
  void Clear() {
    begin = NULL;
    end = NULL;
  }

  // Reinitialized from an array, its size is automatically deduced.
  template <size_t _size>
  void operator=(_Ty (&_array)[_size]) {
    begin = _array;
    end = _array + _size;
  }

  // Implement cast operator to allow conversions to Range<const _Ty>.
  operator Range<const _Ty>() const { return Range<const _Ty>(begin, end); }

  // Returns a const reference to element _i of range [begin,end[.
  const _Ty& operator[](size_t _i) const {
    assert(begin != NULL && begin + _i < end && "Index out of range.");
    return begin[_i];
  }

  // Returns a reference to element _i of range [begin,end[.
  _Ty& operator[](size_t _i) {
    assert(begin != NULL && begin + _i < end && "Index out of range.");
    return begin[_i];
  }

  // Gets the number of elements of the range.
  // This size isn't stored but computed from begin and end pointers.
  size_t count() const {
    assert(begin <= end && "Invalid range.");
    return static_cast<size_t>(end - begin);
  }

  // Gets the size in byte of the range.
  size_t size() const {
    assert(begin <= end && "Invalid range.");
    return static_cast<size_t>(reinterpret_cast<uintptr_t>(end) -
                               reinterpret_cast<uintptr_t>(begin));
  }

  // Range begin pointer.
  _Ty* begin;

  // Range end pointer, declared as const as it should never be dereferenced.
  const _Ty* end;
};
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_PLATFORM_H_
