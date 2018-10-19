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

#ifndef OZZ_OZZ_BASE_MATHS_SOA_FLOAT_H_
#define OZZ_OZZ_BASE_MATHS_SOA_FLOAT_H_

#include <cassert>

#include "math_constant.h"
#include "simd_math.h"
#include "../platform.h"

namespace ozz {
namespace math {

struct SoaFloat2 {
  SimdFloat4 x, y;

  static OZZ_INLINE SoaFloat2 Load(_SimdFloat4 _x, _SimdFloat4 _y) {
    const SoaFloat2 r = {_x, _y};
    return r;
  }

  static OZZ_INLINE SoaFloat2 zero() {
    const SoaFloat2 r = {simd_float4::zero(), simd_float4::zero()};
    return r;
  }

  static OZZ_INLINE SoaFloat2 one() {
    const SoaFloat2 r = {simd_float4::one(), simd_float4::one()};
    return r;
  }

  static OZZ_INLINE SoaFloat2 x_axis() {
    const SoaFloat2 r = {simd_float4::one(), simd_float4::zero()};
    return r;
  }

  static OZZ_INLINE SoaFloat2 y_axis() {
    const SoaFloat2 r = {simd_float4::zero(), simd_float4::one()};
    return r;
  }
};

struct SoaFloat3 {
  SimdFloat4 x, y, z;

  static OZZ_INLINE SoaFloat3 Load(_SimdFloat4 _x, _SimdFloat4 _y,
                                   _SimdFloat4 _z) {
    const SoaFloat3 r = {_x, _y, _z};
    return r;
  }

  static OZZ_INLINE SoaFloat3 Load(const SoaFloat2& _v, _SimdFloat4 _z) {
    const SoaFloat3 r = {_v.x, _v.y, _z};
    return r;
  }

  static OZZ_INLINE SoaFloat3 zero() {
    const SoaFloat3 r = {simd_float4::zero(), simd_float4::zero(),
                         simd_float4::zero()};
    return r;
  }

  static OZZ_INLINE SoaFloat3 one() {
    const SoaFloat3 r = {simd_float4::one(), simd_float4::one(),
                         simd_float4::one()};
    return r;
  }

  static OZZ_INLINE SoaFloat3 x_axis() {
    const SoaFloat3 r = {simd_float4::one(), simd_float4::zero(),
                         simd_float4::zero()};
    return r;
  }

  static OZZ_INLINE SoaFloat3 y_axis() {
    const SoaFloat3 r = {simd_float4::zero(), simd_float4::one(),
                         simd_float4::zero()};
    return r;
  }

  static OZZ_INLINE SoaFloat3 z_axis() {
    const SoaFloat3 r = {simd_float4::zero(), simd_float4::zero(),
                         simd_float4::one()};
    return r;
  }
};

struct SoaFloat4 {
  SimdFloat4 x, y, z, w;

  static OZZ_INLINE SoaFloat4 Load(_SimdFloat4 _x, _SimdFloat4 _y,
                                   _SimdFloat4 _z, const SimdFloat4& _w) {
    const SoaFloat4 r = {_x, _y, _z, _w};
    return r;
  }

  static OZZ_INLINE SoaFloat4 Load(const SoaFloat3& _v, _SimdFloat4 _w) {
    const SoaFloat4 r = {_v.x, _v.y, _v.z, _w};
    return r;
  }

  static OZZ_INLINE SoaFloat4 Load(const SoaFloat2& _v, _SimdFloat4 _z,
                                   _SimdFloat4 _w) {
    const SoaFloat4 r = {_v.x, _v.y, _z, _w};
    return r;
  }

  static OZZ_INLINE SoaFloat4 zero() {
    const SimdFloat4 zero = simd_float4::zero();
    const SoaFloat4 r = {zero, zero, zero, zero};
    return r;
  }

  static OZZ_INLINE SoaFloat4 one() {
    const SimdFloat4 one = simd_float4::one();
    const SoaFloat4 r = {one, one, one, one};
    return r;
  }

  static OZZ_INLINE SoaFloat4 x_axis() {
    const SimdFloat4 zero = simd_float4::zero();
    const SoaFloat4 r = {simd_float4::one(), zero, zero, zero};
    return r;
  }

  static OZZ_INLINE SoaFloat4 y_axis() {
    const SimdFloat4 zero = simd_float4::zero();
    const SoaFloat4 r = {zero, simd_float4::one(), zero, zero};
    return r;
  }

  static OZZ_INLINE SoaFloat4 z_axis() {
    const SimdFloat4 zero = simd_float4::zero();
    const SoaFloat4 r = {zero, zero, simd_float4::one(), zero};
    return r;
  }

  static OZZ_INLINE SoaFloat4 w_axis() {
    const SimdFloat4 zero = simd_float4::zero();
    const SoaFloat4 r = {zero, zero, zero, simd_float4::one()};
    return r;
  }
};
}  // namespace math
}  // namespace ozz

// Returns per element addition of _a and _b using operator +.
OZZ_INLINE ozz::math::SoaFloat4 operator+(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SoaFloat4 r = {_a.x + _b.x, _a.y + _b.y, _a.z + _b.z,
                                  _a.w + _b.w};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator+(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SoaFloat3 r = {_a.x + _b.x, _a.y + _b.y, _a.z + _b.z};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator+(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SoaFloat2 r = {_a.x + _b.x, _a.y + _b.y};
  return r;
}

// Returns per element subtraction of _a and _b using operator -.
OZZ_INLINE ozz::math::SoaFloat4 operator-(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SoaFloat4 r = {_a.x - _b.x, _a.y - _b.y, _a.z - _b.z,
                                  _a.w - _b.w};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator-(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SoaFloat3 r = {_a.x - _b.x, _a.y - _b.y, _a.z - _b.z};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator-(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SoaFloat2 r = {_a.x - _b.x, _a.y - _b.y};
  return r;
}

// Returns per element negative value of _v.
OZZ_INLINE ozz::math::SoaFloat4 operator-(const ozz::math::SoaFloat4& _v) {
  const ozz::math::SoaFloat4 r = {-_v.x, -_v.y, -_v.z, -_v.w};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator-(const ozz::math::SoaFloat3& _v) {
  const ozz::math::SoaFloat3 r = {-_v.x, -_v.y, -_v.z};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator-(const ozz::math::SoaFloat2& _v) {
  const ozz::math::SoaFloat2 r = {-_v.x, -_v.y};
  return r;
}

// Returns per element multiplication of _a and _b using operator *.
OZZ_INLINE ozz::math::SoaFloat4 operator*(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SoaFloat4 r = {_a.x * _b.x, _a.y * _b.y, _a.z * _b.z,
                                  _a.w * _b.w};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator*(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SoaFloat3 r = {_a.x * _b.x, _a.y * _b.y, _a.z * _b.z};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator*(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SoaFloat2 r = {_a.x * _b.x, _a.y * _b.y};
  return r;
}

// Returns per element multiplication of _a and scalar value _f using
// operator *.
OZZ_INLINE ozz::math::SoaFloat4 operator*(const ozz::math::SoaFloat4& _a,
                                          ozz::math::_SimdFloat4 _f) {
  const ozz::math::SoaFloat4 r = {_a.x * _f, _a.y * _f, _a.z * _f, _a.w * _f};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator*(const ozz::math::SoaFloat3& _a,
                                          ozz::math::_SimdFloat4 _f) {
  const ozz::math::SoaFloat3 r = {_a.x * _f, _a.y * _f, _a.z * _f};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator*(const ozz::math::SoaFloat2& _a,
                                          ozz::math::_SimdFloat4 _f) {
  const ozz::math::SoaFloat2 r = {_a.x * _f, _a.y * _f};
  return r;
}

// Multiplies _a and _b, then adds _addend.
// v = (_a * _b) + _addend
OZZ_INLINE ozz::math::SoaFloat2 MAdd(const ozz::math::SoaFloat2& _a,
                                     const ozz::math::SoaFloat2& _b,
                                     const ozz::math::SoaFloat2& _addend) {
  const ozz::math::SoaFloat2 r = {ozz::math::MAdd(_a.x, _b.x, _addend.x),
                                  ozz::math::MAdd(_a.y, _b.y, _addend.y)};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 MAdd(const ozz::math::SoaFloat3& _a,
                                     const ozz::math::SoaFloat3& _b,
                                     const ozz::math::SoaFloat3& _addend) {
  const ozz::math::SoaFloat3 r = {ozz::math::MAdd(_a.x, _b.x, _addend.x),
                                  ozz::math::MAdd(_a.y, _b.y, _addend.y),
                                  ozz::math::MAdd(_a.z, _b.z, _addend.z)};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat4 MAdd(const ozz::math::SoaFloat4& _a,
                                     const ozz::math::SoaFloat4& _b,
                                     const ozz::math::SoaFloat4& _addend) {
  const ozz::math::SoaFloat4 r = {ozz::math::MAdd(_a.x, _b.x, _addend.x),
                                  ozz::math::MAdd(_a.y, _b.y, _addend.y),
                                  ozz::math::MAdd(_a.z, _b.z, _addend.z),
                                  ozz::math::MAdd(_a.w, _b.w, _addend.w)};
  return r;
}

// Returns per element division of _a and _b using operator /.
OZZ_INLINE ozz::math::SoaFloat4 operator/(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SoaFloat4 r = {_a.x / _b.x, _a.y / _b.y, _a.z / _b.z,
                                  _a.w / _b.w};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator/(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SoaFloat3 r = {_a.x / _b.x, _a.y / _b.y, _a.z / _b.z};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator/(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SoaFloat2 r = {_a.x / _b.x, _a.y / _b.y};
  return r;
}

// Returns per element division of _a and scalar value _f using operator/.
OZZ_INLINE ozz::math::SoaFloat4 operator/(const ozz::math::SoaFloat4& _a,
                                          ozz::math::_SimdFloat4 _f) {
  const ozz::math::SoaFloat4 r = {_a.x / _f, _a.y / _f, _a.z / _f, _a.w / _f};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat3 operator/(const ozz::math::SoaFloat3& _a,
                                          ozz::math::_SimdFloat4 _f) {
  const ozz::math::SoaFloat3 r = {_a.x / _f, _a.y / _f, _a.z / _f};
  return r;
}
OZZ_INLINE ozz::math::SoaFloat2 operator/(const ozz::math::SoaFloat2& _a,
                                          ozz::math::_SimdFloat4 _f) {
  const ozz::math::SoaFloat2 r = {_a.x / _f, _a.y / _f};
  return r;
}

// Returns true if each element of a is less than each element of _b.
OZZ_INLINE ozz::math::SimdInt4 operator<(const ozz::math::SoaFloat4& _a,
                                         const ozz::math::SoaFloat4& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpLt(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpLt(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpLt(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpLt(_a.w, _b.w);
  return ozz::math::And(ozz::math::And(ozz::math::And(x, y), z), w);
}
OZZ_INLINE ozz::math::SimdInt4 operator<(const ozz::math::SoaFloat3& _a,
                                         const ozz::math::SoaFloat3& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpLt(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpLt(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpLt(_a.z, _b.z);
  return ozz::math::And(ozz::math::And(x, y), z);
}
OZZ_INLINE ozz::math::SimdInt4 operator<(const ozz::math::SoaFloat2& _a,
                                         const ozz::math::SoaFloat2& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpLt(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpLt(_a.y, _b.y);
  return ozz::math::And(x, y);
}

// Returns true if each element of a is less or equal to each element of _b.
OZZ_INLINE ozz::math::SimdInt4 operator<=(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpLe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpLe(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpLe(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpLe(_a.w, _b.w);
  return ozz::math::And(ozz::math::And(ozz::math::And(x, y), z), w);
}
OZZ_INLINE ozz::math::SimdInt4 operator<=(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpLe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpLe(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpLe(_a.z, _b.z);
  return ozz::math::And(ozz::math::And(x, y), z);
}
OZZ_INLINE ozz::math::SimdInt4 operator<=(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpLe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpLe(_a.y, _b.y);
  return ozz::math::And(x, y);
}

// Returns true if each element of a is greater than each element of _b.
OZZ_INLINE ozz::math::SimdInt4 operator>(const ozz::math::SoaFloat4& _a,
                                         const ozz::math::SoaFloat4& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpGt(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpGt(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpGt(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpGt(_a.w, _b.w);
  return ozz::math::And(ozz::math::And(ozz::math::And(x, y), z), w);
}
OZZ_INLINE ozz::math::SimdInt4 operator>(const ozz::math::SoaFloat3& _a,
                                         const ozz::math::SoaFloat3& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpGt(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpGt(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpGt(_a.z, _b.z);
  return ozz::math::And(ozz::math::And(x, y), z);
}
OZZ_INLINE ozz::math::SimdInt4 operator>(const ozz::math::SoaFloat2& _a,
                                         const ozz::math::SoaFloat2& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpGt(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpGt(_a.y, _b.y);
  return ozz::math::And(x, y);
}

// Returns true if each element of a is greater or equal to each element of _b.
OZZ_INLINE ozz::math::SimdInt4 operator>=(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpGe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpGe(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpGe(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpGe(_a.w, _b.w);
  return ozz::math::And(ozz::math::And(ozz::math::And(x, y), z), w);
}
OZZ_INLINE ozz::math::SimdInt4 operator>=(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpGe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpGe(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpGe(_a.z, _b.z);
  return ozz::math::And(ozz::math::And(x, y), z);
}
OZZ_INLINE ozz::math::SimdInt4 operator>=(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpGe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpGe(_a.y, _b.y);
  return ozz::math::And(x, y);
}

// Returns true if each element of _a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
OZZ_INLINE ozz::math::SimdInt4 operator==(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpEq(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpEq(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpEq(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpEq(_a.w, _b.w);
  return ozz::math::And(ozz::math::And(ozz::math::And(x, y), z), w);
}
OZZ_INLINE ozz::math::SimdInt4 operator==(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpEq(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpEq(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpEq(_a.z, _b.z);
  return ozz::math::And(ozz::math::And(x, y), z);
}
OZZ_INLINE ozz::math::SimdInt4 operator==(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpEq(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpEq(_a.y, _b.y);
  return ozz::math::And(x, y);
}

// Returns true if each element of a is different from each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
OZZ_INLINE ozz::math::SimdInt4 operator!=(const ozz::math::SoaFloat4& _a,
                                          const ozz::math::SoaFloat4& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpNe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpNe(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpNe(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpNe(_a.w, _b.w);
  return ozz::math::Or(ozz::math::Or(ozz::math::Or(x, y), z), w);
}
OZZ_INLINE ozz::math::SimdInt4 operator!=(const ozz::math::SoaFloat3& _a,
                                          const ozz::math::SoaFloat3& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpNe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpNe(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpNe(_a.z, _b.z);
  return ozz::math::Or(ozz::math::Or(x, y), z);
}
OZZ_INLINE ozz::math::SimdInt4 operator!=(const ozz::math::SoaFloat2& _a,
                                          const ozz::math::SoaFloat2& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpNe(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpNe(_a.y, _b.y);
  return ozz::math::Or(x, y);
}

namespace ozz {
namespace math {

// Returns the (horizontal) addition of each element of _v.
OZZ_INLINE SimdFloat4 HAdd(const SoaFloat4& _v) {
  return _v.x + _v.y + _v.z + _v.w;
}
OZZ_INLINE SimdFloat4 HAdd(const SoaFloat3& _v) { return _v.x + _v.y + _v.z; }
OZZ_INLINE SimdFloat4 HAdd(const SoaFloat2& _v) { return _v.x + _v.y; }

// Returns the dot product of _a and _b.
OZZ_INLINE SimdFloat4 Dot(const SoaFloat4& _a, const SoaFloat4& _b) {
  return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
}
OZZ_INLINE SimdFloat4 Dot(const SoaFloat3& _a, const SoaFloat3& _b) {
  return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z;
}
OZZ_INLINE SimdFloat4 Dot(const SoaFloat2& _a, const SoaFloat2& _b) {
  return _a.x * _b.x + _a.y * _b.y;
}

// Returns the cross product of _a and _b.
OZZ_INLINE SoaFloat3 CrossProduct(const SoaFloat3& _a, const SoaFloat3& _b) {
  const SoaFloat3 r = {_a.y * _b.z - _b.y * _a.z, _a.z * _b.x - _b.z * _a.x,
                       _a.x * _b.y - _b.x * _a.y};
  return r;
}

// Returns the length |_v| of _v.
OZZ_INLINE SimdFloat4 Length(const SoaFloat4& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  return Sqrt(len2);
}
OZZ_INLINE SimdFloat4 Length(const SoaFloat3& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  return Sqrt(len2);
}
OZZ_INLINE SimdFloat4 Length(const SoaFloat2& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y;
  return Sqrt(len2);
}

// Returns the square length |_v|^2 of _v.
OZZ_INLINE SimdFloat4 LengthSqr(const SoaFloat4& _v) {
  return _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
}
OZZ_INLINE SimdFloat4 LengthSqr(const SoaFloat3& _v) {
  return _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
}
OZZ_INLINE SimdFloat4 LengthSqr(const SoaFloat2& _v) {
  return _v.x * _v.x + _v.y * _v.y;
}

// Returns the normalized vector _v.
OZZ_INLINE SoaFloat4 Normalize(const SoaFloat4& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  assert(AreAllTrue(CmpNe(len2, simd_float4::zero())) &&
         "_v is not normalizable");
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaFloat4 r = {_v.x * inv_len, _v.y * inv_len, _v.z * inv_len,
                       _v.w * inv_len};
  return r;
}
OZZ_INLINE SoaFloat3 Normalize(const SoaFloat3& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  assert(AreAllTrue(CmpNe(len2, simd_float4::zero())) &&
         "_v is not normalizable");
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaFloat3 r = {_v.x * inv_len, _v.y * inv_len, _v.z * inv_len};
  return r;
}
OZZ_INLINE SoaFloat2 Normalize(const SoaFloat2& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y;
  assert(AreAllTrue(CmpNe(len2, simd_float4::zero())) &&
         "_v is not normalizable");
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaFloat2 r = {_v.x * inv_len, _v.y * inv_len};
  return r;
}

// Test if each vector _v is normalized.
OZZ_INLINE math::SimdInt4 IsNormalized(const SoaFloat4& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceSq));
}
OZZ_INLINE math::SimdInt4 IsNormalized(const SoaFloat3& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceSq));
}
OZZ_INLINE math::SimdInt4 IsNormalized(const SoaFloat2& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceSq));
}

// Test if each vector _v is normalized using estimated tolerance.
OZZ_INLINE math::SimdInt4 IsNormalizedEst(const SoaFloat4& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceEstSq));
}
OZZ_INLINE math::SimdInt4 IsNormalizedEst(const SoaFloat3& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceEstSq));
}
OZZ_INLINE math::SimdInt4 IsNormalizedEst(const SoaFloat2& _v) {
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceEstSq));
}

// Returns the normalized vector _v if the norm of _v is not 0.
// Otherwise returns _safer.
OZZ_INLINE SoaFloat4 NormalizeSafe(const SoaFloat4& _v,
                                   const SoaFloat4& _safer) {
  assert(AreAllTrue(IsNormalizedEst(_safer)) && "_safer is not normalized");
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  const math::SimdInt4 b = CmpNe(len2, math::simd_float4::zero());
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaFloat4 r = {
      Select(b, _v.x * inv_len, _safer.x), Select(b, _v.y * inv_len, _safer.y),
      Select(b, _v.z * inv_len, _safer.z), Select(b, _v.w * inv_len, _safer.w)};
  return r;
}
OZZ_INLINE SoaFloat3 NormalizeSafe(const SoaFloat3& _v,
                                   const SoaFloat3& _safer) {
  assert(AreAllTrue(IsNormalizedEst(_safer)) && "_safer is not normalized");
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  const math::SimdInt4 b = CmpNe(len2, math::simd_float4::zero());
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaFloat3 r = {Select(b, _v.x * inv_len, _safer.x),
                       Select(b, _v.y * inv_len, _safer.y),
                       Select(b, _v.z * inv_len, _safer.z)};
  return r;
}
OZZ_INLINE SoaFloat2 NormalizeSafe(const SoaFloat2& _v,
                                   const SoaFloat2& _safer) {
  assert(AreAllTrue(IsNormalizedEst(_safer)) && "_safer is not normalized");
  const SimdFloat4 len2 = _v.x * _v.x + _v.y * _v.y;
  const math::SimdInt4 b = CmpNe(len2, math::simd_float4::zero());
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaFloat2 r = {Select(b, _v.x * inv_len, _safer.x),
                       Select(b, _v.y * inv_len, _safer.y)};
  return r;
}

// Returns the linear interpolation of _a and _b with coefficient _f.
// _f is not limited to range [0,1].
OZZ_INLINE SoaFloat4 Lerp(const SoaFloat4& _a, const SoaFloat4& _b,
                          _SimdFloat4 _f) {
  const SoaFloat4 r = {(_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                       (_b.z - _a.z) * _f + _a.z, (_b.w - _a.w) * _f + _a.w};
  return r;
}
OZZ_INLINE SoaFloat3 Lerp(const SoaFloat3& _a, const SoaFloat3& _b,
                          _SimdFloat4 _f) {
  const SoaFloat3 r = {(_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                       (_b.z - _a.z) * _f + _a.z};
  return r;
}
OZZ_INLINE SoaFloat2 Lerp(const SoaFloat2& _a, const SoaFloat2& _b,
                          _SimdFloat4 _f) {
  const SoaFloat2 r = {(_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y};
  return r;
}

// Returns the minimum of each element of _a and _b.
OZZ_INLINE SoaFloat4 Min(const SoaFloat4& _a, const SoaFloat4& _b) {
  const SoaFloat4 r = {Min(_a.x, _b.x), Min(_a.y, _b.y), Min(_a.z, _b.z),
                       Min(_a.w, _b.w)};
  return r;
}
OZZ_INLINE SoaFloat3 Min(const SoaFloat3& _a, const SoaFloat3& _b) {
  const SoaFloat3 r = {Min(_a.x, _b.x), Min(_a.y, _b.y), Min(_a.z, _b.z)};
  return r;
}
OZZ_INLINE SoaFloat2 Min(const SoaFloat2& _a, const SoaFloat2& _b) {
  const SoaFloat2 r = {Min(_a.x, _b.x), Min(_a.y, _b.y)};
  return r;
}

// Returns the maximum of each element of _a and _b.
OZZ_INLINE SoaFloat4 Max(const SoaFloat4& _a, const SoaFloat4& _b) {
  const SoaFloat4 r = {Max(_a.x, _b.x), Max(_a.y, _b.y), Max(_a.z, _b.z),
                       Max(_a.w, _b.w)};
  return r;
}
OZZ_INLINE SoaFloat3 Max(const SoaFloat3& _a, const SoaFloat3& _b) {
  const SoaFloat3 r = {Max(_a.x, _b.x), Max(_a.y, _b.y), Max(_a.z, _b.z)};
  return r;
}
OZZ_INLINE SoaFloat2 Max(const SoaFloat2& _a, const SoaFloat2& _b) {
  const SoaFloat2 r = {Max(_a.x, _b.x), Max(_a.y, _b.y)};
  return r;
}

// Clamps each element of _x between _a and _b.
// _a must be less or equal to b;
OZZ_INLINE SoaFloat4 Clamp(const SoaFloat4& _a, const SoaFloat4& _v,
                           const SoaFloat4& _b) {
  return Max(_a, Min(_v, _b));
}
OZZ_INLINE SoaFloat3 Clamp(const SoaFloat3& _a, const SoaFloat3& _v,
                           const SoaFloat3& _b) {
  return Max(_a, Min(_v, _b));
}
OZZ_INLINE SoaFloat2 Clamp(const SoaFloat2& _a, const SoaFloat2& _v,
                           const SoaFloat2& _b) {
  return Max(_a, Min(_v, _b));
}
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_SOA_FLOAT_H_
