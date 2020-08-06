//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

/*
* Copyright (c) 2018-2020 The Forge Interactive Inc.
*
* This file is part of The-Forge
* (see https://github.com/ConfettiFX/The-Forge).
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/

#ifndef VECTORMATH_SOA_FLOAT4X4_HPP
#define VECTORMATH_SOA_FLOAT4X4_HPP

namespace Vectormath
{
namespace Soa
{

//----------------------------------------------------------------------------
// SoaFloat4x4
//----------------------------------------------------------------------------

// Returns the identity matrix.
inline SoaFloat4x4 SoaFloat4x4::identity() {
  const Vector4 zero = Vector4::zero();
  const Vector4 one = Vector4::one();
  SoaFloat4x4 ret = {{{one, zero, zero, zero},
                      {zero, one, zero, zero},
                      {zero, zero, one, zero},
                      {zero, zero, zero, one}}};
  return ret;
}

// Returns a scaling matrix that scales along _v.
// _v.w is ignored.
inline SoaFloat4x4 SoaFloat4x4::Scaling(const SoaFloat4& _v) {
  const Vector4 zero = Vector4::zero();
  const Vector4 one = Vector4::one();
  const SoaFloat4x4 ret = {{{_v.x, zero, zero, zero},
                            {zero, _v.y, zero, zero},
                            {zero, zero, _v.z, zero},
                            {zero, zero, zero, one}}};
  return ret;
}

// Returns the rotation matrix built from quaternion defined by x, y, z and w
// components of _v.
inline SoaFloat4x4 SoaFloat4x4::FromQuaternion(const SoaQuaternion& _q) {

  const Vector4 zero = Vector4::zero();
  const Vector4 one = Vector4::one();
  const Vector4 two = one + one;

  const Vector4 xx = mulPerElem(_q.x, _q.x);
  const Vector4 xy = mulPerElem(_q.x, _q.y);
  const Vector4 xz = mulPerElem(_q.x, _q.z);
  const Vector4 xw = mulPerElem(_q.x, _q.w);
  const Vector4 yy = mulPerElem(_q.y, _q.y);
  const Vector4 yz = mulPerElem(_q.y, _q.z);
  const Vector4 yw = mulPerElem(_q.y, _q.w);
  const Vector4 zz = mulPerElem(_q.z, _q.z);
  const Vector4 zw = mulPerElem(_q.z, _q.w);

  const SoaFloat4x4 ret = {
		{ { one - mulPerElem(two, (yy + zz)), mulPerElem(two, (xy + zw)), mulPerElem(two, (xz - yw)), zero },
		{ mulPerElem(two, (xy - zw)),one - mulPerElem(two, (xx + zz)), mulPerElem(two, (yz + xw)), zero },
		{ mulPerElem(two, (xz + yw)), mulPerElem(two, (yz - xw)), one - mulPerElem(two, (xx + yy)), zero },
		{ zero, zero, zero, one } } };
  
  return ret;
}

// Returns the affine transformation matrix built from split translation,
// rotation (quaternion) and scale.
inline SoaFloat4x4 SoaFloat4x4::FromAffine(const SoaFloat3& _translation,
                                          const SoaQuaternion& _quaternion,
                                          const SoaFloat3& _scale) {

  const Vector4 zero = Vector4::zero();
  const Vector4 one = Vector4::one();
  const Vector4 two = one + one;

  const Vector4 xx = mulPerElem(_quaternion.x, _quaternion.x);
  const Vector4 xy = mulPerElem(_quaternion.x, _quaternion.y);
  const Vector4 xz = mulPerElem(_quaternion.x, _quaternion.z);
  const Vector4 xw = mulPerElem(_quaternion.x, _quaternion.w);
  const Vector4 yy = mulPerElem(_quaternion.y, _quaternion.y);
  const Vector4 yz = mulPerElem(_quaternion.y, _quaternion.z);
  const Vector4 yw = mulPerElem(_quaternion.y, _quaternion.w);
  const Vector4 zz = mulPerElem(_quaternion.z, _quaternion.z);
  const Vector4 zw = mulPerElem(_quaternion.z, _quaternion.w);


  const SoaFloat4x4 ret = {
		{ { mulPerElem(_scale.x,one - (mulPerElem(two, (yy + zz)))), mulPerElem(mulPerElem(_scale.x, two), (xy + zw)),
		mulPerElem(mulPerElem(_scale.x, two), (xz - yw)), zero },
		{ mulPerElem(mulPerElem(_scale.y, two), (xy - zw)), mulPerElem(_scale.y,one - (mulPerElem(two, (xx + zz)))),
		mulPerElem(mulPerElem(_scale.y, two), (yz + xw)), zero },
		{ mulPerElem(mulPerElem(_scale.z, two), (xz + yw)), mulPerElem(mulPerElem(_scale.z, two), (yz - xw)),
		mulPerElem(_scale.z,one - (mulPerElem(two, (xx + yy)))), zero },
		{ _translation.x, _translation.y, _translation.z, one } } };

  return ret;
}

//----------------------------------------------------------------------------
// SoaFloat4x4 Methods
//----------------------------------------------------------------------------

// Returns the transpose of matrix _m.
inline SoaFloat4x4 Transpose(const SoaFloat4x4& _m) {
  const SoaFloat4x4 ret = {
      {{_m.cols[0].x, _m.cols[1].x, _m.cols[2].x, _m.cols[3].x},
       {_m.cols[0].y, _m.cols[1].y, _m.cols[2].y, _m.cols[3].y},
       {_m.cols[0].z, _m.cols[1].z, _m.cols[2].z, _m.cols[3].z},
       {_m.cols[0].w, _m.cols[1].w, _m.cols[2].w, _m.cols[3].w}}};
  return ret;
}

// Returns the inverse of matrix _m.
inline SoaFloat4x4 Invert(const SoaFloat4x4& _m) {
  const SoaFloat4* cols = _m.cols;
  const Vector4 a00 = mulPerElem(cols[2].z, cols[3].w) - mulPerElem(cols[3].z, cols[2].w);
  const Vector4 a01 = mulPerElem(cols[2].y, cols[3].w) - mulPerElem(cols[3].y, cols[2].w);
  const Vector4 a02 = mulPerElem(cols[2].y, cols[3].z) - mulPerElem(cols[3].y, cols[2].z);
  const Vector4 a03 = mulPerElem(cols[2].x, cols[3].w) - mulPerElem(cols[3].x, cols[2].w);
  const Vector4 a04 = mulPerElem(cols[2].x, cols[3].z) - mulPerElem(cols[3].x, cols[2].z);
  const Vector4 a05 = mulPerElem(cols[2].x, cols[3].y) - mulPerElem(cols[3].x, cols[2].y);
  const Vector4 a06 = mulPerElem(cols[1].z, cols[3].w) - mulPerElem(cols[3].z, cols[1].w);
  const Vector4 a07 = mulPerElem(cols[1].y, cols[3].w) - mulPerElem(cols[3].y, cols[1].w);
  const Vector4 a08 = mulPerElem(cols[1].y, cols[3].z) - mulPerElem(cols[3].y, cols[1].z);
  const Vector4 a09 = mulPerElem(cols[1].x, cols[3].w) - mulPerElem(cols[3].x, cols[1].w);
  const Vector4 a10 = mulPerElem(cols[1].x, cols[3].z) - mulPerElem(cols[3].x, cols[1].z);
  const Vector4 a11 = mulPerElem(cols[1].y, cols[3].w) - mulPerElem(cols[3].y, cols[1].w);
  const Vector4 a12 = mulPerElem(cols[1].x, cols[3].y) - mulPerElem(cols[3].x, cols[1].y);
  const Vector4 a13 = mulPerElem(cols[1].z, cols[2].w) - mulPerElem(cols[2].z, cols[1].w);
  const Vector4 a14 = mulPerElem(cols[1].y, cols[2].w) - mulPerElem(cols[2].y, cols[1].w);
  const Vector4 a15 = mulPerElem(cols[1].y, cols[2].z) - mulPerElem(cols[2].y, cols[1].z);
  const Vector4 a16 = mulPerElem(cols[1].x, cols[2].w) - mulPerElem(cols[2].x, cols[1].w);
  const Vector4 a17 = mulPerElem(cols[1].x, cols[2].z) - mulPerElem(cols[2].x, cols[1].z);
  const Vector4 a18 = mulPerElem(cols[1].x, cols[2].y) - mulPerElem(cols[2].x, cols[1].y);

  const Vector4 b0x = mulPerElem(cols[1].y, a00) - mulPerElem(cols[1].z, a01) + mulPerElem(cols[1].w, a02);
  const Vector4 b1x = mulPerElem(-cols[1].x, a00) + mulPerElem(cols[1].z, a03) - mulPerElem(cols[1].w, a04);
  const Vector4 b2x = mulPerElem(cols[1].x, a01) - mulPerElem(cols[1].y, a03) + mulPerElem(cols[1].w, a05);
  const Vector4 b3x = mulPerElem(-cols[1].x, a02) + mulPerElem(cols[1].y, a04) - mulPerElem(cols[1].z, a05);

  const Vector4 b0y = mulPerElem(-cols[0].y, a00) + mulPerElem(cols[0].z, a01) - mulPerElem(cols[0].w, a02);
  const Vector4 b1y = mulPerElem(cols[0].x, a00) - mulPerElem(cols[0].z, a03) + mulPerElem(cols[0].w, a04);
  const Vector4 b2y = mulPerElem(-cols[0].x, a01) + mulPerElem(cols[0].y, a03) - mulPerElem(cols[0].w, a05);
  const Vector4 b3y = mulPerElem(cols[0].x, a02) - mulPerElem(cols[0].y, a04) + mulPerElem(cols[0].z, a05);

  const Vector4 b0z = mulPerElem(cols[0].y, a06) - mulPerElem(cols[0].z, a07) + mulPerElem(cols[0].w, a08);
  const Vector4 b1z = mulPerElem(-cols[0].x, a06) + mulPerElem(cols[0].z, a09) - mulPerElem(cols[0].w, a10);
  const Vector4 b2z = mulPerElem(cols[0].x, a11) - mulPerElem(cols[0].y, a09) + mulPerElem(cols[0].w, a12);
  const Vector4 b3z = mulPerElem(-cols[0].x, a08) + mulPerElem(cols[0].y, a10) - mulPerElem(cols[0].z, a12);

  const Vector4 b0w = mulPerElem(-cols[0].y, a13) + mulPerElem(cols[0].z, a14) - mulPerElem(cols[0].w, a15);
  const Vector4 b1w = mulPerElem(cols[0].x, a13) - mulPerElem(cols[0].z, a16) + mulPerElem(cols[0].w, a17);
  const Vector4 b2w = mulPerElem(-cols[0].x, a14) + mulPerElem(cols[0].y, a16) - mulPerElem(cols[0].w, a18);
  const Vector4 b3w = mulPerElem(cols[0].x, a15) - mulPerElem(cols[0].y, a17) + mulPerElem(cols[0].z, a18);

  const Vector4 det =
      mulPerElem(cols[0].x, b0x) + mulPerElem(cols[0].y, b1x) + mulPerElem(cols[0].z, b2x) + mulPerElem(cols[0].w, b3x);
  const Vector4 inv_det = divPerElem(Vector4::one(), det);

  const SoaFloat4x4 ret = {
      {{mulPerElem(b0x, inv_det), mulPerElem(b0y, inv_det), mulPerElem(b0z, inv_det), mulPerElem(b0w, inv_det)},
       {mulPerElem(b1x, inv_det), mulPerElem(b1y, inv_det), mulPerElem(b1z, inv_det), mulPerElem(b1w, inv_det)},
       {mulPerElem(b2x, inv_det), mulPerElem(b2y, inv_det), mulPerElem(b2z, inv_det), mulPerElem(b2w, inv_det)},
       {mulPerElem(b3x, inv_det), mulPerElem(b3y, inv_det), mulPerElem(b3z, inv_det), mulPerElem(b3w, inv_det)}}};
  return ret;
}

// Scales matrix _m along the axis defined by _v components.
// _v.w is ignored.
inline SoaFloat4x4 Scale(const SoaFloat4x4& _m, const SoaFloat4& _v) {
  const SoaFloat4x4 ret = {{{mulPerElem(_m.cols[0].x, _v.x), mulPerElem(_m.cols[0].y, _v.x),
                             mulPerElem(_m.cols[0].z, _v.x), mulPerElem(_m.cols[0].w, _v.x)},
                            {mulPerElem(_m.cols[1].x, _v.y), mulPerElem(_m.cols[1].y, _v.y),
                             mulPerElem(_m.cols[1].z, _v.y), mulPerElem(_m.cols[1].w, _v.y)},
                            {mulPerElem(_m.cols[2].x, _v.z), mulPerElem(_m.cols[2].y, _v.z),
                             mulPerElem(_m.cols[2].z, _v.z), mulPerElem(_m.cols[2].w, _v.z)},
                            _m.cols[3]}};
  return ret;
}

// Computes the multiplication of matrix Float4x4 and vector  _v.
inline SoaFloat4 operator*(const SoaFloat4x4& _m,
                                          const SoaFloat4& _v) {
  const SoaFloat4 ret = {
      mulPerElem(_m.cols[0].x, _v.x) + mulPerElem(_m.cols[1].x, _v.y) + mulPerElem(_m.cols[2].x, _v.z) +
          mulPerElem(_m.cols[3].x, _v.w),
      mulPerElem(_m.cols[0].y, _v.x) + mulPerElem(_m.cols[1].y, _v.y) + mulPerElem(_m.cols[2].y, _v.z) +
          mulPerElem(_m.cols[3].y, _v.w),
      mulPerElem(_m.cols[0].z, _v.x) + mulPerElem(_m.cols[1].z, _v.y) + mulPerElem(_m.cols[2].z, _v.z) +
          mulPerElem(_m.cols[3].z, _v.w),
      mulPerElem(_m.cols[0].w, _v.x) + mulPerElem(_m.cols[1].w, _v.y) + mulPerElem(_m.cols[2].w, _v.z) +
          mulPerElem(_m.cols[3].w, _v.w)};
  return ret;
}

// Computes the multiplication of two matrices _a and _b.
inline SoaFloat4x4 operator*(const SoaFloat4x4& _a,
                                            const SoaFloat4x4& _b) {
  const SoaFloat4x4 ret = {
      {_a * _b.cols[0], _a * _b.cols[1], _a * _b.cols[2], _a * _b.cols[3]}};
  return ret;
}

// Computes the per element addition of two matrices _a and _b.
inline SoaFloat4x4 operator+(const SoaFloat4x4& _a,
                                            const SoaFloat4x4& _b) {
  const SoaFloat4x4 ret = {
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
inline SoaFloat4x4 operator-(const SoaFloat4x4& _a,
                                            const SoaFloat4x4& _b) {
  const SoaFloat4x4 ret = {
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

} // namespace Soa
} // namespace Vectormath

#endif // VECTORMATH_SOA_FLOAT4X4_HPP

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================