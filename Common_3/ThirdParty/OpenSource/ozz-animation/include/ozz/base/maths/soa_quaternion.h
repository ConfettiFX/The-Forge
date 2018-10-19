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

#ifndef OZZ_OZZ_BASE_MATHS_SOA_QUATERNION_H_
#define OZZ_OZZ_BASE_MATHS_SOA_QUATERNION_H_

#include <cassert>

#include "soa_float.h"
#include "../platform.h"

namespace ozz {
namespace math {

struct SoaQuaternion {
  SimdFloat4 x, y, z, w;

  // Loads a quaternion from 4 SimdFloat4 values.
  static OZZ_INLINE SoaQuaternion Load(_SimdFloat4 _x, _SimdFloat4 _y,
                                       _SimdFloat4 _z, const SimdFloat4& _w) {
    const SoaQuaternion r = {_x, _y, _z, _w};
    return r;
  }

  // Returns the identity SoaQuaternion.
  static OZZ_INLINE SoaQuaternion identity() {
    const SoaQuaternion r = {simd_float4::zero(), simd_float4::zero(),
                             simd_float4::zero(), simd_float4::one()};
    return r;
  }
};

// Returns the conjugate of _q. This is the same as the inverse if _q is
// normalized. Otherwise the magnitude of the inverse is 1.f/|_q|.
OZZ_INLINE SoaQuaternion Conjugate(const SoaQuaternion& _q) {
  const SoaQuaternion r = {-_q.x, -_q.y, -_q.z, _q.w};
  return r;
}

// Returns the negate of _q. This represent the same rotation as q.
OZZ_INLINE SoaQuaternion operator-(const SoaQuaternion& _q) {
  const SoaQuaternion r = {-_q.x, -_q.y, -_q.z, -_q.w};
  return r;
}

// Returns the normalized SoaQuaternion _q.
OZZ_INLINE SoaQuaternion Normalize(const SoaQuaternion& _q) {
  const SimdFloat4 len2 = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaQuaternion r = {_q.x * inv_len, _q.y * inv_len, _q.z * inv_len,
                           _q.w * inv_len};
  return r;
}

// Returns the estimated normalized SoaQuaternion _q.
OZZ_INLINE SoaQuaternion NormalizeEst(const SoaQuaternion& _q) {
  const SimdFloat4 len2 = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  // Uses RSqrtEstNR (with one more Newton-Raphson step) as quaternions loose
  // much precision due to normalization.
  const SimdFloat4 inv_len = RSqrtEstNR(len2);
  const SoaQuaternion r = {_q.x * inv_len, _q.y * inv_len, _q.z * inv_len,
                           _q.w * inv_len};
  return r;
}

// Test if each quaternion of _q is normalized.
OZZ_INLINE SimdInt4 IsNormalized(const SoaQuaternion& _q) {
  const SimdFloat4 len2 = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceSq));
}

// Test if each quaternion of _q is normalized. using estimated tolerance.
OZZ_INLINE SimdInt4 IsNormalizedEst(const SoaQuaternion& _q) {
  const SimdFloat4 len2 = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  return CmpLt(Abs(len2 - math::simd_float4::one()),
               simd_float4::Load1(kNormalizationToleranceEstSq));
}

// Returns the linear interpolation of SoaQuaternion _a and _b with coefficient
// _f.
OZZ_INLINE SoaQuaternion Lerp(const SoaQuaternion& _a, const SoaQuaternion& _b,
                              _SimdFloat4 _f) {
  const SoaQuaternion r = {(_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                           (_b.z - _a.z) * _f + _a.z,
                           (_b.w - _a.w) * _f + _a.w};
  return r;
}

// Returns the linear interpolation of SoaQuaternion _a and _b with coefficient
// _f.
OZZ_INLINE SoaQuaternion NLerp(const SoaQuaternion& _a, const SoaQuaternion& _b,
                               _SimdFloat4 _f) {
  const SoaFloat4 lerp = {(_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                          (_b.z - _a.z) * _f + _a.z, (_b.w - _a.w) * _f + _a.w};
  const SimdFloat4 len2 =
      lerp.x * lerp.x + lerp.y * lerp.y + lerp.z * lerp.z + lerp.w * lerp.w;
  const SimdFloat4 inv_len = math::simd_float4::one() / Sqrt(len2);
  const SoaQuaternion r = {lerp.x * inv_len, lerp.y * inv_len, lerp.z * inv_len,
                           lerp.w * inv_len};
  return r;
}

// Returns the estimated linear interpolation of SoaQuaternion _a and _b with
// coefficient _f.
OZZ_INLINE SoaQuaternion NLerpEst(const SoaQuaternion& _a,
                                  const SoaQuaternion& _b, _SimdFloat4 _f) {
  const SoaFloat4 lerp = {(_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                          (_b.z - _a.z) * _f + _a.z, (_b.w - _a.w) * _f + _a.w};
  const SimdFloat4 len2 =
      lerp.x * lerp.x + lerp.y * lerp.y + lerp.z * lerp.z + lerp.w * lerp.w;
  // Uses RSqrtEstNR (with one more Newton-Raphson step) as quaternions loose
  // much precision due to normalization.
  const SimdFloat4 inv_len = RSqrtEstNR(len2);
  const SoaQuaternion r = {lerp.x * inv_len, lerp.y * inv_len, lerp.z * inv_len,
                           lerp.w * inv_len};
  return r;
}
}  // namespace math
}  // namespace ozz

// Returns the addition of _a and _b.
OZZ_INLINE ozz::math::SoaQuaternion operator+(
    const ozz::math::SoaQuaternion& _a, const ozz::math::SoaQuaternion& _b) {
  const ozz::math::SoaQuaternion r = {_a.x + _b.x, _a.y + _b.y, _a.z + _b.z,
                                      _a.w + _b.w};
  return r;
}

// Returns the multiplication of _q and scalar value _f.
OZZ_INLINE ozz::math::SoaQuaternion operator*(
    const ozz::math::SoaQuaternion& _q, const ozz::math::SimdFloat4& _f) {
  const ozz::math::SoaQuaternion r = {_q.x * _f, _q.y * _f, _q.z * _f,
                                      _q.w * _f};
  return r;
}

// Returns the multiplication of _a and _b. If both _a and _b are normalized,
// then the result is normalized.
OZZ_INLINE ozz::math::SoaQuaternion operator*(
    const ozz::math::SoaQuaternion& _a, const ozz::math::SoaQuaternion& _b) {
  const ozz::math::SoaQuaternion r = {
      _a.w * _b.x + _a.x * _b.w + _a.y * _b.z - _a.z * _b.y,
      _a.w * _b.y + _a.y * _b.w + _a.z * _b.x - _a.x * _b.z,
      _a.w * _b.z + _a.z * _b.w + _a.x * _b.y - _a.y * _b.x,
      _a.w * _b.w - _a.x * _b.x - _a.y * _b.y - _a.z * _b.z};
  return r;
}

// Returns true if each element of _a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
OZZ_INLINE ozz::math::SimdInt4 operator==(const ozz::math::SoaQuaternion& _a,
                                          const ozz::math::SoaQuaternion& _b) {
  const ozz::math::SimdInt4 x = ozz::math::CmpEq(_a.x, _b.x);
  const ozz::math::SimdInt4 y = ozz::math::CmpEq(_a.y, _b.y);
  const ozz::math::SimdInt4 z = ozz::math::CmpEq(_a.z, _b.z);
  const ozz::math::SimdInt4 w = ozz::math::CmpEq(_a.w, _b.w);
  return ozz::math::And(ozz::math::And(ozz::math::And(x, y), z), w);
}
#endif  // OZZ_OZZ_BASE_MATHS_SOA_QUATERNION_H_
