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

#ifndef VECTORMATH_SOA_QUATERNION_HPP
#define VECTORMATH_SOA_QUATERNION_HPP

namespace Vectormath
{
namespace Soa
{

//----------------------------------------------------------------------------
// SOA Quaternion
//----------------------------------------------------------------------------

// Loads a quaternion from 4 Vector4 values.
inline SoaQuaternion SoaQuaternion::Load(const Vector4& _x, const Vector4& _y,
                                      const Vector4& _z, const Vector4& _w) {
  const SoaQuaternion r = {_x, _y, _z, _w};
  return r;
}

// Returns the identity SoaQuaternion.
inline SoaQuaternion SoaQuaternion::identity() {
  const SoaQuaternion r = {Vector4::zero(), Vector4::zero(),
                            Vector4::zero(), Vector4::one()};
  return r;
}


// Returns the conjugate of _q. This is the same as the inverse if _q is
// normalized. Otherwise the magnitude of the inverse is 1.f/|_q|.
inline SoaQuaternion Conjugate(const SoaQuaternion& _q) {
  const SoaQuaternion r = {-_q.x, -_q.y, -_q.z, _q.w};
  return r;
}

// Returns the negate of _q. This represent the same rotation as q.
inline SoaQuaternion operator-(const SoaQuaternion& _q) {
  const SoaQuaternion r = {-_q.x, -_q.y, -_q.z, -_q.w};
  return r;
}

// Returns the normalized SoaQuaternion _q.
inline SoaQuaternion Normalize(const SoaQuaternion& _q) {
  const Vector4 len2 = mulPerElem(_q.x, _q.x) + mulPerElem(_q.y, _q.y) + mulPerElem(_q.z, _q.z) + mulPerElem(_q.w, _q.w);
  const Vector4 inv_len = divPerElem(Vector4::one(), sqrtPerElem(len2));
  const SoaQuaternion r = {mulPerElem(_q.x, inv_len), mulPerElem(_q.y, inv_len), mulPerElem(_q.z, inv_len),
                           mulPerElem(_q.w, inv_len)};
  return r;
}

// Returns the estimated normalized SoaQuaternion _q.
inline SoaQuaternion NormalizeEst(const SoaQuaternion& _q) {
  const Vector4 len2 = mulPerElem(_q.x, _q.x) + mulPerElem(_q.y, _q.y) + mulPerElem(_q.z, _q.z) + mulPerElem(_q.w, _q.w);
  // Uses RSqrtEstNR (with one more Newton-Raphson step) as quaternions loose
  // much precision due to normalization.
  const Vector4 inv_len = rSqrtEstNR(len2);
  const SoaQuaternion r = {mulPerElem(_q.x, inv_len), mulPerElem(_q.y, inv_len), mulPerElem(_q.z, inv_len),
                           mulPerElem(_q.w, inv_len)};
  return r;
}

// Test if each quaternion of _q is normalized.
inline Vector4Int IsNormalized(const SoaQuaternion& _q) {
  const Vector4 len2 = mulPerElem(_q.x, _q.x) + mulPerElem(_q.y, _q.y) + mulPerElem(_q.z, _q.z) + mulPerElem(_q.w, _q.w);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceSq));
}

// Test if each quaternion of _q is normalized. using estimated tolerance.
inline Vector4Int IsNormalizedEst(const SoaQuaternion& _q) {
  const Vector4 len2 = mulPerElem(_q.x, _q.x) + mulPerElem(_q.y, _q.y) + mulPerElem(_q.z, _q.z) + mulPerElem(_q.w, _q.w);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceEstSq));
}

// Returns the linear interpolation of SoaQuaternion _a and _b with coefficient
// _f.
inline SoaQuaternion Lerp(const SoaQuaternion& _a, const SoaQuaternion& _b,
                              const Vector4& _f) {
  const SoaQuaternion r = {mulPerElem((_b.x - _a.x), _f) + _a.x, mulPerElem((_b.y - _a.y), _f) + _a.y,
                           mulPerElem((_b.z - _a.z), _f) + _a.z,
                           mulPerElem((_b.w - _a.w), _f) + _a.w};
  return r;
}

// Returns the linear interpolation of SoaQuaternion _a and _b with coefficient
// _f.
inline SoaQuaternion NLerp(const SoaQuaternion& _a, const SoaQuaternion& _b,
                               const Vector4& _f) {
  const SoaFloat4 lerp = {mulPerElem((_b.x - _a.x), _f) + _a.x, mulPerElem((_b.y - _a.y), _f) + _a.y,
                          mulPerElem((_b.z - _a.z), _f) + _a.z, mulPerElem((_b.w - _a.w), _f) + _a.w};
  const Vector4 len2 =
      mulPerElem(lerp.x, lerp.x) + mulPerElem(lerp.y, lerp.y) + mulPerElem(lerp.z, lerp.z) + mulPerElem(lerp.w, lerp.w);
  const Vector4 inv_len = divPerElem(Vector4::one(), sqrtPerElem(len2));
  const SoaQuaternion r = {mulPerElem(lerp.x, inv_len), mulPerElem(lerp.y, inv_len), mulPerElem(lerp.z, inv_len),
                           mulPerElem(lerp.w, inv_len)};
  return r;
}

// Returns the estimated linear interpolation of SoaQuaternion _a and _b with
// coefficient _f.
inline SoaQuaternion NLerpEst(const SoaQuaternion& _a,
                                  const SoaQuaternion& _b, const Vector4& _f) {
  const SoaFloat4 lerp = {mulPerElem((_b.x - _a.x), _f) + _a.x, mulPerElem((_b.y - _a.y), _f) + _a.y,
                          mulPerElem((_b.z - _a.z), _f) + _a.z, mulPerElem((_b.w - _a.w), _f) + _a.w};
  const Vector4 len2 =
      mulPerElem(lerp.x, lerp.x) + mulPerElem(lerp.y, lerp.y) + mulPerElem(lerp.z, lerp.z) + mulPerElem(lerp.w, lerp.w);
  // Uses RSqrtEstNR (with one more Newton-Raphson step) as quaternions loose
  // much precision due to normalization.
  const Vector4 inv_len = rSqrtEstNR(len2);
  const SoaQuaternion r = {mulPerElem(lerp.x, inv_len), mulPerElem(lerp.y, inv_len), mulPerElem(lerp.z, inv_len),
                           mulPerElem(lerp.w, inv_len)};
  return r;
}
//}  // namespace math
//}  // namespace ozz

// Returns the addition of _a and _b.
inline SoaQuaternion operator+(
    const SoaQuaternion& _a, const SoaQuaternion& _b) {
  const SoaQuaternion r = {_a.x + _b.x, _a.y + _b.y, _a.z + _b.z,
                                      _a.w + _b.w};
  return r;
}

// Returns the multiplication of _q and scalar value _f.
inline SoaQuaternion operator*(
    const SoaQuaternion& _q, const Vector4& _f) {
  const SoaQuaternion r = {mulPerElem(_q.x, _f), mulPerElem(_q.y, _f), mulPerElem(_q.z, _f),
                                      mulPerElem(_q.w, _f)};
  return r;
}

// Returns the multiplication of _a and _b. If both _a and _b are normalized,
// then the result is normalized.
inline SoaQuaternion operator*(
    const SoaQuaternion& _a, const SoaQuaternion& _b) {
  const SoaQuaternion r = {
      mulPerElem(_a.w, _b.x) + mulPerElem(_a.x, _b.w) + mulPerElem(_a.y, _b.z) - mulPerElem(_a.z, _b.y),
      mulPerElem(_a.w, _b.y) + mulPerElem(_a.y, _b.w) + mulPerElem(_a.z, _b.x) - mulPerElem(_a.x, _b.z),
      mulPerElem(_a.w, _b.z) + mulPerElem(_a.z, _b.w) + mulPerElem(_a.x, _b.y) - mulPerElem(_a.y, _b.x),
      mulPerElem(_a.w, _b.w) - mulPerElem(_a.x, _b.x) - mulPerElem(_a.y, _b.y) - mulPerElem(_a.z, _b.z)};
  return r;
}

// Returns true if each element of _a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
inline Vector4Int operator==(const SoaQuaternion& _a,
                                          const SoaQuaternion& _b) {
  const Vector4Int x = cmpEq(_a.x, _b.x);
  const Vector4Int y = cmpEq(_a.y, _b.y);
  const Vector4Int z = cmpEq(_a.z, _b.z);
  const Vector4Int w = cmpEq(_a.w, _b.w);
  return And(And(And(x, y), z), w);
}

} // namespace Soa
} // namespace Vectormath

#endif // VECTORMATH_SOA_QUATERNION_HPP

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================