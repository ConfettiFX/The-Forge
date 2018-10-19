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

#ifndef OZZ_OZZ_BASE_MATHS_MATH_EX_H_
#define OZZ_OZZ_BASE_MATHS_MATH_EX_H_

#include <cassert>
#include <cmath>

#include "../platform.h"

namespace ozz {
namespace math {

// Returns the linear interpolation of _a and _b with coefficient _f.
// _f is not limited to range [0,1].
OZZ_INLINE float Lerp(float _a, float _b, float _f) {
  return (_b - _a) * _f + _a;
}

// Returns the minimum of _a and _b. Comparison's based on operator <.
template <typename _Ty>
OZZ_INLINE _Ty Min(_Ty _a, _Ty _b) {
  return (_a < _b) ? _a : _b;
}

// Returns the maximum of _a and _b. Comparison's based on operator <.
template <typename _Ty>
OZZ_INLINE _Ty Max(_Ty _a, _Ty _b) {
  return (_b < _a) ? _a : _b;
}

// Clamps _x between _a and _b. Comparison's based on operator <.
// Result is unknown if _a is not less or equal to _b.
template <typename _Ty>
OZZ_INLINE _Ty Clamp(_Ty _a, _Ty _x, _Ty _b) {
  const _Ty min = _x < _b ? _x : _b;
  return min < _a ? _a : min;
}

// Implements int selection, avoiding branching.
OZZ_INLINE int Select(bool _b, int _true, int _false) {
  return _false ^ (-static_cast<int>(_b) & (_true ^ _false));
}

// Implements float selection, avoiding branching.
OZZ_INLINE float Select(bool _b, float _true, float _false) {
  union {
    float f;
    int32_t i;
  } t = {_true};
  union {
    float f;
    int32_t i;
  } f = {_false};
  union {
    int32_t i;
    float f;
  } r = {f.i ^ (-static_cast<int32_t>(_b) & (t.i ^ f.i))};
  return r.f;
}

// Implements pointer selection, avoiding branching.
template <typename _Ty>
OZZ_INLINE _Ty* Select(bool _b, _Ty* _true, _Ty* _false) {
  union {
    _Ty* p;
    intptr_t i;
  } t = {_true};
  union {
    _Ty* p;
    intptr_t i;
  } f = {_false};
  union {
    intptr_t i;
    _Ty* p;
  } r = {f.i ^ (-static_cast<intptr_t>(_b) & (t.i ^ f.i))};
  return r.p;
}

// Implements const pointer selection, avoiding branching.
template <typename _Ty>
OZZ_INLINE const _Ty* Select(bool _b, const _Ty* _true, const _Ty* _false) {
  union {
    const _Ty* p;
    intptr_t i;
  } t = {_true};
  union {
    const _Ty* p;
    intptr_t i;
  } f = {_false};
  union {
    intptr_t i;
    const _Ty* p;
  } r = {f.i ^ (-static_cast<intptr_t>(_b) & (t.i ^ f.i))};
  return r.p;
}

// Tests whether _block is aligned to _alignment boundary.
template <typename _Ty>
OZZ_INLINE bool IsAligned(_Ty _value, size_t _alignment) {
  return (_value & (_alignment - 1)) == 0;
}
template <typename _Ty>
OZZ_INLINE bool IsAligned(_Ty* _address, size_t _alignment) {
  return (reinterpret_cast<uintptr_t>(_address) & (_alignment - 1)) == 0;
}

// Aligns _block address to the first greater address that is aligned to
// _alignment boundaries.
template <typename _Ty>
OZZ_INLINE _Ty Align(_Ty _value, size_t _alignment) {
  return static_cast<_Ty>(_value + (_alignment - 1)) & (0 - _alignment);
}
template <typename _Ty>
OZZ_INLINE _Ty* Align(_Ty* _address, size_t _alignment) {
  return reinterpret_cast<_Ty*>(
      (reinterpret_cast<uintptr_t>(_address) + (_alignment - 1)) &
      (0 - _alignment));
}

// Strides a pointer of _stride bytes.
template <typename _Ty>
OZZ_INLINE _Ty* Stride(_Ty* _value, intptr_t _stride) {
  return reinterpret_cast<const _Ty*>(reinterpret_cast<uintptr_t>(_value) +
                                      _stride);
}
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_MATH_EX_H_
