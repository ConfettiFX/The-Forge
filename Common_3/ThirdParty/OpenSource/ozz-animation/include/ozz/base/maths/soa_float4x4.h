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

#ifndef OZZ_OZZ_BASE_MATHS_SOA_FLOAT4X4_H_
#define OZZ_OZZ_BASE_MATHS_SOA_FLOAT4X4_H_

#include <cassert>

#include "ozz/base/maths/soa_float.h"
#include "ozz/base/maths/soa_quaternion.h"
#include "ozz/base/platform.h"

namespace ozz {
namespace math {

// Declare the 4x4 soa matrix type. Uses the column major convention where the
// matrix-times-vector is written v'=Mv:
// [ m.cols[0].x m.cols[1].x m.cols[2].x m.cols[3].x ]   {v.x}
// | m.cols[0].y m.cols[1].y m.cols[2].y m.cols[3].y | * {v.y}
// | m.cols[0].z m.cols[1].y m.cols[2].y m.cols[3].y |   {v.z}
// [ m.cols[0].w m.cols[1].w m.cols[2].w m.cols[3].w ]   {v.1}
struct SoaFloat4x4 {
  // Soa matrix columns.
  SoaFloat4 cols[4];

  // Returns the identity matrix.
  static OZZ_INLINE SoaFloat4x4 identity() {
    const SimdFloat4 zero = simd_float4::zero();
    const SimdFloat4 one = simd_float4::one();
    SoaFloat4x4 ret = {{{one, zero, zero, zero},
                        {zero, one, zero, zero},
                        {zero, zero, one, zero},
                        {zero, zero, zero, one}}};
    return ret;
  }

  // Returns a scaling matrix that scales along _v.
  // _v.w is ignored.
  static OZZ_INLINE SoaFloat4x4 Scaling(const SoaFloat4& _v) {
    const SimdFloat4 zero = simd_float4::zero();
    const SimdFloat4 one = simd_float4::one();
    const SoaFloat4x4 ret = {{{_v.x, zero, zero, zero},
                              {zero, _v.y, zero, zero},
                              {zero, zero, _v.z, zero},
                              {zero, zero, zero, one}}};
    return ret;
  }

  // Returns the rotation matrix built from quaternion defined by x, y, z and w
  // components of _v.
  static OZZ_INLINE SoaFloat4x4 FromQuaternion(const SoaQuaternion& _q) {
    assert(AreAllTrue(IsNormalizedEst(_q)));

    const SimdFloat4 zero = simd_float4::zero();
    const SimdFloat4 one = simd_float4::one();
    const SimdFloat4 two = one + one;

    const SimdFloat4 xx = _q.x * _q.x;
    const SimdFloat4 xy = _q.x * _q.y;
    const SimdFloat4 xz = _q.x * _q.z;
    const SimdFloat4 xw = _q.x * _q.w;
    const SimdFloat4 yy = _q.y * _q.y;
    const SimdFloat4 yz = _q.y * _q.z;
    const SimdFloat4 yw = _q.y * _q.w;
    const SimdFloat4 zz = _q.z * _q.z;
    const SimdFloat4 zw = _q.z * _q.w;

    const SoaFloat4x4 ret = {
        {{one - two * (yy + zz), two * (xy + zw), two * (xz - yw), zero},
         {two * (xy - zw), one - two * (xx + zz), two * (yz + xw), zero},
         {two * (xz + yw), two * (yz - xw), one - two * (xx + yy), zero},
         {zero, zero, zero, one}}};
    return ret;
  }

  // Returns the affine transformation matrix built from split translation,
  // rotation (quaternion) and scale.
  static OZZ_INLINE SoaFloat4x4 FromAffine(const SoaFloat3& _translation,
                                           const SoaQuaternion& _quaternion,
                                           const SoaFloat3& _scale) {
    assert(AreAllTrue(IsNormalizedEst(_quaternion)));

    const SimdFloat4 zero = simd_float4::zero();
    const SimdFloat4 one = simd_float4::one();
    const SimdFloat4 two = one + one;

    const SimdFloat4 xx = _quaternion.x * _quaternion.x;
    const SimdFloat4 xy = _quaternion.x * _quaternion.y;
    const SimdFloat4 xz = _quaternion.x * _quaternion.z;
    const SimdFloat4 xw = _quaternion.x * _quaternion.w;
    const SimdFloat4 yy = _quaternion.y * _quaternion.y;
    const SimdFloat4 yz = _quaternion.y * _quaternion.z;
    const SimdFloat4 yw = _quaternion.y * _quaternion.w;
    const SimdFloat4 zz = _quaternion.z * _quaternion.z;
    const SimdFloat4 zw = _quaternion.z * _quaternion.w;

    const SoaFloat4x4 ret = {
        {{_scale.x * (one - two * (yy + zz)), _scale.x * two * (xy + zw),
          _scale.x * two * (xz - yw), zero},
         {_scale.y * two * (xy - zw), _scale.y * (one - two * (xx + zz)),
          _scale.y * two * (yz + xw), zero},
         {_scale.z * two * (xz + yw), _scale.z * two * (yz - xw),
          _scale.z * (one - two * (xx + yy)), zero},
         {_translation.x, _translation.y, _translation.z, one}}};
    return ret;
  }
};

// Returns the transpose of matrix _m.
OZZ_INLINE SoaFloat4x4 Transpose(const SoaFloat4x4& _m) {
  const SoaFloat4x4 ret = {
      {{_m.cols[0].x, _m.cols[1].x, _m.cols[2].x, _m.cols[3].x},
       {_m.cols[0].y, _m.cols[1].y, _m.cols[2].y, _m.cols[3].y},
       {_m.cols[0].z, _m.cols[1].z, _m.cols[2].z, _m.cols[3].z},
       {_m.cols[0].w, _m.cols[1].w, _m.cols[2].w, _m.cols[3].w}}};
  return ret;
}

// Returns the inverse of matrix _m.
OZZ_INLINE SoaFloat4x4 Invert(const SoaFloat4x4& _m) {
  const SoaFloat4* cols = _m.cols;
  const SimdFloat4 a00 = cols[2].z * cols[3].w - cols[3].z * cols[2].w;
  const SimdFloat4 a01 = cols[2].y * cols[3].w - cols[3].y * cols[2].w;
  const SimdFloat4 a02 = cols[2].y * cols[3].z - cols[3].y * cols[2].z;
  const SimdFloat4 a03 = cols[2].x * cols[3].w - cols[3].x * cols[2].w;
  const SimdFloat4 a04 = cols[2].x * cols[3].z - cols[3].x * cols[2].z;
  const SimdFloat4 a05 = cols[2].x * cols[3].y - cols[3].x * cols[2].y;
  const SimdFloat4 a06 = cols[1].z * cols[3].w - cols[3].z * cols[1].w;
  const SimdFloat4 a07 = cols[1].y * cols[3].w - cols[3].y * cols[1].w;
  const SimdFloat4 a08 = cols[1].y * cols[3].z - cols[3].y * cols[1].z;
  const SimdFloat4 a09 = cols[1].x * cols[3].w - cols[3].x * cols[1].w;
  const SimdFloat4 a10 = cols[1].x * cols[3].z - cols[3].x * cols[1].z;
  const SimdFloat4 a11 = cols[1].y * cols[3].w - cols[3].y * cols[1].w;
  const SimdFloat4 a12 = cols[1].x * cols[3].y - cols[3].x * cols[1].y;
  const SimdFloat4 a13 = cols[1].z * cols[2].w - cols[2].z * cols[1].w;
  const SimdFloat4 a14 = cols[1].y * cols[2].w - cols[2].y * cols[1].w;
  const SimdFloat4 a15 = cols[1].y * cols[2].z - cols[2].y * cols[1].z;
  const SimdFloat4 a16 = cols[1].x * cols[2].w - cols[2].x * cols[1].w;
  const SimdFloat4 a17 = cols[1].x * cols[2].z - cols[2].x * cols[1].z;
  const SimdFloat4 a18 = cols[1].x * cols[2].y - cols[2].x * cols[1].y;

  const SimdFloat4 b0x = cols[1].y * a00 - cols[1].z * a01 + cols[1].w * a02;
  const SimdFloat4 b1x = -cols[1].x * a00 + cols[1].z * a03 - cols[1].w * a04;
  const SimdFloat4 b2x = cols[1].x * a01 - cols[1].y * a03 + cols[1].w * a05;
  const SimdFloat4 b3x = -cols[1].x * a02 + cols[1].y * a04 - cols[1].z * a05;

  const SimdFloat4 b0y = -cols[0].y * a00 + cols[0].z * a01 - cols[0].w * a02;
  const SimdFloat4 b1y = cols[0].x * a00 - cols[0].z * a03 + cols[0].w * a04;
  const SimdFloat4 b2y = -cols[0].x * a01 + cols[0].y * a03 - cols[0].w * a05;
  const SimdFloat4 b3y = cols[0].x * a02 - cols[0].y * a04 + cols[0].z * a05;

  const SimdFloat4 b0z = cols[0].y * a06 - cols[0].z * a07 + cols[0].w * a08;
  const SimdFloat4 b1z = -cols[0].x * a06 + cols[0].z * a09 - cols[0].w * a10;
  const SimdFloat4 b2z = cols[0].x * a11 - cols[0].y * a09 + cols[0].w * a12;
  const SimdFloat4 b3z = -cols[0].x * a08 + cols[0].y * a10 - cols[0].z * a12;

  const SimdFloat4 b0w = -cols[0].y * a13 + cols[0].z * a14 - cols[0].w * a15;
  const SimdFloat4 b1w = cols[0].x * a13 - cols[0].z * a16 + cols[0].w * a17;
  const SimdFloat4 b2w = -cols[0].x * a14 + cols[0].y * a16 - cols[0].w * a18;
  const SimdFloat4 b3w = cols[0].x * a15 - cols[0].y * a17 + cols[0].z * a18;

  const SimdFloat4 det =
      cols[0].x * b0x + cols[0].y * b1x + cols[0].z * b2x + cols[0].w * b3x;
  assert(AreAllTrue(CmpNe(det, simd_float4::zero())) &&
         "Matrix is not invertible");
  const SimdFloat4 inv_det = simd_float4::one() / det;

  const SoaFloat4x4 ret = {
      {{b0x * inv_det, b0y * inv_det, b0z * inv_det, b0w * inv_det},
       {b1x * inv_det, b1y * inv_det, b1z * inv_det, b1w * inv_det},
       {b2x * inv_det, b2y * inv_det, b2z * inv_det, b2w * inv_det},
       {b3x * inv_det, b3y * inv_det, b3z * inv_det, b3w * inv_det}}};
  return ret;
}

// Scales matrix _m along the axis defined by _v components.
// _v.w is ignored.
OZZ_INLINE SoaFloat4x4 Scale(const SoaFloat4x4& _m, const SoaFloat4& _v) {
  const SoaFloat4x4 ret = {{{_m.cols[0].x * _v.x, _m.cols[0].y * _v.x,
                             _m.cols[0].z * _v.x, _m.cols[0].w * _v.x},
                            {_m.cols[1].x * _v.y, _m.cols[1].y * _v.y,
                             _m.cols[1].z * _v.y, _m.cols[1].w * _v.y},
                            {_m.cols[2].x * _v.z, _m.cols[2].y * _v.z,
                             _m.cols[2].z * _v.z, _m.cols[2].w * _v.z},
                            _m.cols[3]}};
  return ret;
}
}  // namespace math
}  // namespace ozz

// Computes the multiplication of matrix Float4x4 and vector  _v.
OZZ_INLINE ozz::math::SoaFloat4 operator*(const ozz::math::SoaFloat4x4& _m,
                                          const ozz::math::SoaFloat4& _v) {
  const ozz::math::SoaFloat4 ret = {
      _m.cols[0].x * _v.x + _m.cols[1].x * _v.y + _m.cols[2].x * _v.z +
          _m.cols[3].x * _v.w,
      _m.cols[0].y * _v.x + _m.cols[1].y * _v.y + _m.cols[2].y * _v.z +
          _m.cols[3].y * _v.w,
      _m.cols[0].z * _v.x + _m.cols[1].z * _v.y + _m.cols[2].z * _v.z +
          _m.cols[3].z * _v.w,
      _m.cols[0].w * _v.x + _m.cols[1].w * _v.y + _m.cols[2].w * _v.z +
          _m.cols[3].w * _v.w};
  return ret;
}

// Computes the multiplication of two matrices _a and _b.
OZZ_INLINE ozz::math::SoaFloat4x4 operator*(const ozz::math::SoaFloat4x4& _a,
                                            const ozz::math::SoaFloat4x4& _b) {
  const ozz::math::SoaFloat4x4 ret = {
      {_a * _b.cols[0], _a * _b.cols[1], _a * _b.cols[2], _a * _b.cols[3]}};
  return ret;
}

// Computes the per element addition of two matrices _a and _b.
OZZ_INLINE ozz::math::SoaFloat4x4 operator+(const ozz::math::SoaFloat4x4& _a,
                                            const ozz::math::SoaFloat4x4& _b) {
  const ozz::math::SoaFloat4x4 ret = {
      {{_a.cols[0].x + _b.cols[0].x, _a.cols[0].y + _b.cols[0].y,
        _a.cols[0].z + _b.cols[0].z, _a.cols[0].w + _b.cols[0].w},
       {_a.cols[1].x + _b.cols[1].x, _a.cols[1].y + _b.cols[1].y,
        _a.cols[1].z + _b.cols[1].z, _a.cols[1].w + _b.cols[1].w},
       {_a.cols[2].x + _b.cols[2].x, _a.cols[2].y + _b.cols[2].y,
        _a.cols[2].z + _b.cols[2].z, _a.cols[2].w + _b.cols[2].w},
       {_a.cols[3].x + _b.cols[3].x, _a.cols[3].y + _b.cols[3].y,
        _a.cols[3].z + _b.cols[3].z, _a.cols[3].w + _b.cols[3].w}}};
  return ret;
}

// Computes the per element subtraction of two matrices _a and _b.
OZZ_INLINE ozz::math::SoaFloat4x4 operator-(const ozz::math::SoaFloat4x4& _a,
                                            const ozz::math::SoaFloat4x4& _b) {
  const ozz::math::SoaFloat4x4 ret = {
      {{_a.cols[0].x - _b.cols[0].x, _a.cols[0].y - _b.cols[0].y,
        _a.cols[0].z - _b.cols[0].z, _a.cols[0].w - _b.cols[0].w},
       {_a.cols[1].x - _b.cols[1].x, _a.cols[1].y - _b.cols[1].y,
        _a.cols[1].z - _b.cols[1].z, _a.cols[1].w - _b.cols[1].w},
       {_a.cols[2].x - _b.cols[2].x, _a.cols[2].y - _b.cols[2].y,
        _a.cols[2].z - _b.cols[2].z, _a.cols[2].w - _b.cols[2].w},
       {_a.cols[3].x - _b.cols[3].x, _a.cols[3].y - _b.cols[3].y,
        _a.cols[3].z - _b.cols[3].z, _a.cols[3].w - _b.cols[3].w}}};
  return ret;
}
#endif  // OZZ_OZZ_BASE_MATHS_SOA_FLOAT4X4_H_
