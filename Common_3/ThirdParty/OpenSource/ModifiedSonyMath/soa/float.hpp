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

#ifndef VECTORMATH_SOA_FLOAT_HPP
#define VECTORMATH_SOA_FLOAT_HPP

namespace Vectormath
{
namespace Soa
{

//----------------------------------------------------------------------------
// SoaFloat2
//----------------------------------------------------------------------------

inline SoaFloat2 SoaFloat2::Load(const Vector4& _x, const Vector4& _y) {
  const SoaFloat2 r = {_x, _y};
  return r;
}

inline SoaFloat2 SoaFloat2::zero() {
  const SoaFloat2 r = {Vector4::zero(), Vector4::zero()};
  return r;
}

inline SoaFloat2 SoaFloat2::one() {
  const SoaFloat2 r = {Vector4::one(), Vector4::one()};
  return r;
}

inline SoaFloat2 SoaFloat2::x_axis() {
  const SoaFloat2 r = {Vector4::one(), Vector4::zero()};
  return r;
}

inline SoaFloat2 SoaFloat2::y_axis() {
  const SoaFloat2 r = {Vector4::zero(), Vector4::one()};
  return r;
}

//----------------------------------------------------------------------------
// SoaFloat3
//----------------------------------------------------------------------------

inline SoaFloat3 SoaFloat3::Load(const Vector4& _x, const Vector4& _y,
                                  const Vector4& _z) {
  const SoaFloat3 r = {_x, _y, _z};
  return r;
}

inline SoaFloat3 SoaFloat3::Load(const SoaFloat2& _v, const Vector4& _z) {
  const SoaFloat3 r = {_v.x, _v.y, _z};
  return r;
}

inline SoaFloat3 SoaFloat3::zero() {
  const SoaFloat3 r = {Vector4::zero(), Vector4::zero(),
                        Vector4::zero()};
  return r;
}

inline SoaFloat3 SoaFloat3::one() {
  const SoaFloat3 r = {Vector4::one(), Vector4::one(),
                        Vector4::one()};
  return r;
}

inline SoaFloat3 SoaFloat3::x_axis() {
  const SoaFloat3 r = {Vector4::one(), Vector4::zero(),
                        Vector4::zero()};
  return r;
}

inline SoaFloat3 SoaFloat3::y_axis() {
  const SoaFloat3 r = {Vector4::zero(), Vector4::one(),
                        Vector4::zero()};
  return r;
}

inline SoaFloat3 SoaFloat3::z_axis() {
  const SoaFloat3 r = {Vector4::zero(), Vector4::zero(),
                        Vector4::one()};
  return r;
}

//----------------------------------------------------------------------------
// SoaFloat4
//----------------------------------------------------------------------------

inline SoaFloat4 SoaFloat4::Load(const Vector4& _x, const Vector4& _y,
                                  const Vector4& _z, const Vector4& _w) {
  const SoaFloat4 r = {_x, _y, _z, _w};
  return r;
}

inline SoaFloat4 SoaFloat4::Load(const SoaFloat3& _v, const Vector4& _w) {
  const SoaFloat4 r = {_v.x, _v.y, _v.z, _w};
  return r;
}

inline SoaFloat4 SoaFloat4::Load(const SoaFloat2& _v, const Vector4& _z,
                                  const Vector4& _w) {
  const SoaFloat4 r = {_v.x, _v.y, _z, _w};
  return r;
}

inline SoaFloat4 SoaFloat4::zero() {
  const Vector4 zero = Vector4::zero();
  const SoaFloat4 r = {zero, zero, zero, zero};
  return r;
}

inline SoaFloat4 SoaFloat4::one() {
  const Vector4 one = Vector4::one();
  const SoaFloat4 r = {one, one, one, one};
  return r;
}

inline SoaFloat4 SoaFloat4::x_axis() {
  const Vector4 zero = Vector4::zero();
  const SoaFloat4 r = {Vector4::one(), zero, zero, zero};
  return r;
}

inline SoaFloat4 SoaFloat4::y_axis() {
  const Vector4 zero = Vector4::zero();
  const SoaFloat4 r = {zero, Vector4::one(), zero, zero};
  return r;
}

inline SoaFloat4 SoaFloat4::z_axis() {
  const Vector4 zero = Vector4::zero();
  const SoaFloat4 r = {zero, zero, Vector4::one(), zero};
  return r;
}

inline SoaFloat4 SoaFloat4::w_axis() {
  const Vector4 zero = Vector4::zero();
  const SoaFloat4 r = {zero, zero, zero, Vector4::one()};
  return r;
}

//----------------------------------------------------------------------------
// SoaFloat 2, 3, 4 Methods
//----------------------------------------------------------------------------


// Returns per element addition of _a and _b using operator +.
inline SoaFloat4 operator+(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const SoaFloat4 r = {_a.x + _b.x, _a.y + _b.y, _a.z + _b.z,
                                  _a.w + _b.w};
  return r;
}
inline SoaFloat3 operator+(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const SoaFloat3 r = {_a.x + _b.x, _a.y + _b.y, _a.z + _b.z};
  return r;
}
inline SoaFloat2 operator+(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const SoaFloat2 r = {_a.x + _b.x, _a.y + _b.y};
  return r;
}

// Returns per element subtraction of _a and _b using operator -.
inline SoaFloat4 operator-(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const SoaFloat4 r = {_a.x - _b.x, _a.y - _b.y, _a.z - _b.z,
                                  _a.w - _b.w};
  return r;
}
inline SoaFloat3 operator-(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const SoaFloat3 r = {_a.x - _b.x, _a.y - _b.y, _a.z - _b.z};
  return r;
}
inline SoaFloat2 operator-(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const SoaFloat2 r = {_a.x - _b.x, _a.y - _b.y};
  return r;
}

// Returns per element negative value of _v.
inline SoaFloat4 operator-(const SoaFloat4& _v) {
  const SoaFloat4 r = {-_v.x, -_v.y, -_v.z, -_v.w};
  return r;
}
inline SoaFloat3 operator-(const SoaFloat3& _v) {
  const SoaFloat3 r = {-_v.x, -_v.y, -_v.z};
  return r;
}
inline SoaFloat2 operator-(const SoaFloat2& _v) {
  const SoaFloat2 r = {-_v.x, -_v.y};
  return r;
}

// Returns per element multiplication of _a and _b using operator *.
inline SoaFloat4 operator*(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const SoaFloat4 r = {mulPerElem(_a.x, _b.x), mulPerElem(_a.y, _b.y), mulPerElem(_a.z, _b.z),
                                  mulPerElem(_a.w, _b.w)};
  return r;
}
inline SoaFloat3 operator*(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const SoaFloat3 r = {mulPerElem(_a.x, _b.x), mulPerElem(_a.y, _b.y), mulPerElem(_a.z, _b.z)};
  return r;
}
inline SoaFloat2 operator*(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const SoaFloat2 r = {mulPerElem(_a.x, _b.x), mulPerElem(_a.y, _b.y)};
  return r;
}

// Returns per element multiplication of _a and scalar value _f using
// operator *.
inline SoaFloat4 operator*(const SoaFloat4& _a,
                                          const Vector4& _f) {
  const SoaFloat4 r = {mulPerElem(_a.x, _f), mulPerElem(_a.y, _f), mulPerElem(_a.z, _f), mulPerElem(_a.w, _f)};
  return r;
}
inline SoaFloat3 operator*(const SoaFloat3& _a,
                                          const Vector4& _f) {
  const SoaFloat3 r = {mulPerElem(_a.x, _f), mulPerElem(_a.y, _f), mulPerElem(_a.z, _f)};
  return r;
}
inline SoaFloat2 operator*(const SoaFloat2& _a,
                                          const Vector4& _f) {
  const SoaFloat2 r = {mulPerElem(_a.x, _f), mulPerElem(_a.y, _f)};
  return r;
}

// Returns per element division of _a and _b using operator /.
inline SoaFloat4 operator/(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const SoaFloat4 r = {divPerElem(_a.x, _b.x), divPerElem(_a.y, _b.y), divPerElem(_a.z, _b.z),
                                  divPerElem(_a.w, _b.w)};
  return r;
}
inline SoaFloat3 operator/(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const SoaFloat3 r = {divPerElem(_a.x, _b.x), divPerElem(_a.y, _b.y), divPerElem(_a.z, _b.z)};
  return r;
}
inline SoaFloat2 operator/(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const SoaFloat2 r = {divPerElem(_a.x, _b.x), divPerElem(_a.y, _b.y)};
  return r;
}

// Returns per element division of _a and scalar value _f using operator/.
inline SoaFloat4 operator/(const SoaFloat4& _a,
                                          const Vector4& _f) {
  const SoaFloat4 r = {divPerElem(_a.x, _f), divPerElem(_a.y, _f), divPerElem(_a.z, _f), divPerElem(_a.w, _f)};
  return r;
}
inline SoaFloat3 operator/(const SoaFloat3& _a,
                                          const Vector4& _f) {
  const SoaFloat3 r = {divPerElem(_a.x, _f), divPerElem(_a.y, _f), divPerElem(_a.z, _f)};
  return r;
}
inline SoaFloat2 operator/(const SoaFloat2& _a,
                                          const Vector4& _f) {
  const SoaFloat2 r = {divPerElem(_a.x, _f), divPerElem(_a.y, _f)};
  return r;
}

// Returns true if each element of a is less than each element of _b.
inline Vector4Int operator<(const SoaFloat4& _a,
                                         const SoaFloat4& _b) {
  const Vector4Int x = cmpLt(_a.x, _b.x);
  const Vector4Int y = cmpLt(_a.y, _b.y);
  const Vector4Int z = cmpLt(_a.z, _b.z);
  const Vector4Int w = cmpLt(_a.w, _b.w);
  return And(And(And(x, y), z), w);
}
inline Vector4Int operator<(const SoaFloat3& _a,
                                         const SoaFloat3& _b) {
  const Vector4Int x = cmpLt(_a.x, _b.x);
  const Vector4Int y = cmpLt(_a.y, _b.y);
  const Vector4Int z = cmpLt(_a.z, _b.z);
  return And(And(x, y), z);
}
inline Vector4Int operator<(const SoaFloat2& _a,
                                         const SoaFloat2& _b) {
  const Vector4Int x = cmpLt(_a.x, _b.x);
  const Vector4Int y = cmpLt(_a.y, _b.y);
  return And(x, y);
}

// Returns true if each element of a is less or equal to each element of _b.
inline Vector4Int operator<=(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const Vector4Int x = cmpLe(_a.x, _b.x);
  const Vector4Int y = cmpLe(_a.y, _b.y);
  const Vector4Int z = cmpLe(_a.z, _b.z);
  const Vector4Int w = cmpLe(_a.w, _b.w);
  return And(And(And(x, y), z), w);
}
inline Vector4Int operator<=(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const Vector4Int x = cmpLe(_a.x, _b.x);
  const Vector4Int y = cmpLe(_a.y, _b.y);
  const Vector4Int z = cmpLe(_a.z, _b.z);
  return And(And(x, y), z);
}
inline Vector4Int operator<=(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const Vector4Int x = cmpLe(_a.x, _b.x);
  const Vector4Int y = cmpLe(_a.y, _b.y);
  return And(x, y);
}

// Returns true if each element of a is greater than each element of _b.
inline Vector4Int operator>(const SoaFloat4& _a,
                                         const SoaFloat4& _b) {
  const Vector4Int y = cmpGt(_a.y, _b.y);
  const Vector4Int z = cmpGt(_a.z, _b.z);
  const Vector4Int w = cmpGt(_a.w, _b.w);
  const Vector4Int x = cmpGt(_a.x, _b.x);
  return And(And(And(x, y), z), w);
}
inline Vector4Int operator>(const SoaFloat3& _a,
                                         const SoaFloat3& _b) {
  const Vector4Int x = cmpGt(_a.x, _b.x);
  const Vector4Int y = cmpGt(_a.y, _b.y);
  const Vector4Int z = cmpGt(_a.z, _b.z);
  return And(And(x, y), z);
}
inline Vector4Int operator>(const SoaFloat2& _a,
                                         const SoaFloat2& _b) {
  const Vector4Int x = cmpGt(_a.x, _b.x);
  const Vector4Int y = cmpGt(_a.y, _b.y);
  return And(x, y);
}

// Returns true if each element of a is greater or equal to each element of _b.
inline Vector4Int operator>=(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const Vector4Int x = cmpGe(_a.x, _b.x);
  const Vector4Int y = cmpGe(_a.y, _b.y);
  const Vector4Int z = cmpGe(_a.z, _b.z);
  const Vector4Int w = cmpGe(_a.w, _b.w);
  return And(And(And(x, y), z), w);
}
inline Vector4Int operator>=(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const Vector4Int x = cmpGe(_a.x, _b.x);
  const Vector4Int y = cmpGe(_a.y, _b.y);
  const Vector4Int z = cmpGe(_a.z, _b.z);
  return And(And(x, y), z);
}
inline Vector4Int operator>=(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const Vector4Int x = cmpGe(_a.x, _b.x);
  const Vector4Int y = cmpGe(_a.y, _b.y);
  return And(x, y);
}

// Returns true if each element of _a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
inline Vector4Int operator==(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const Vector4Int x = cmpEq(_a.x, _b.x);
  const Vector4Int y = cmpEq(_a.y, _b.y);
  const Vector4Int z = cmpEq(_a.z, _b.z);
  const Vector4Int w = cmpEq(_a.w, _b.w);
  return And(And(And(x, y), z), w);
}
inline Vector4Int operator==(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const Vector4Int x = cmpEq(_a.x, _b.x);
  const Vector4Int y = cmpEq(_a.y, _b.y);
  const Vector4Int z = cmpEq(_a.z, _b.z);
  return And(And(x, y), z);
}
inline Vector4Int operator==(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const Vector4Int x = cmpEq(_a.x, _b.x);
  const Vector4Int y = cmpEq(_a.y, _b.y);
  return And(x, y);
}

// Returns true if each element of a is different from each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
inline Vector4Int operator!=(const SoaFloat4& _a,
                                          const SoaFloat4& _b) {
  const Vector4Int x = cmpNotEq(_a.x, _b.x);
  const Vector4Int y = cmpNotEq(_a.y, _b.y);
  const Vector4Int z = cmpNotEq(_a.z, _b.z);
  const Vector4Int w = cmpNotEq(_a.w, _b.w);
  return Or(Or(Or(x, y), z), w);
}
inline Vector4Int operator!=(const SoaFloat3& _a,
                                          const SoaFloat3& _b) {
  const Vector4Int x = cmpNotEq(_a.x, _b.x);
  const Vector4Int y = cmpNotEq(_a.y, _b.y);
  const Vector4Int z = cmpNotEq(_a.z, _b.z);
  return Or(Or(x, y), z);
}
inline Vector4Int operator!=(const SoaFloat2& _a,
                                          const SoaFloat2& _b) {
  const Vector4Int x = cmpNotEq(_a.x, _b.x);
  const Vector4Int y = cmpNotEq(_a.y, _b.y);
  return Or(x, y);
}

// Returns the (horizontal) addition of each element of _v.
inline Vector4 HAdd(const SoaFloat4& _v) {
  return _v.x + _v.y + _v.z + _v.w;
}
inline Vector4 HAdd(const SoaFloat3& _v) { return _v.x + _v.y + _v.z; }
inline Vector4 HAdd(const SoaFloat2& _v) { return _v.x + _v.y; }

// Returns the dot product of _a and _b.
inline Vector4 Dot(const SoaFloat4& _a, const SoaFloat4& _b) {
  return mulPerElem(_a.x, _b.x) + mulPerElem(_a.y, _b.y) + mulPerElem(_a.z, _b.z) + mulPerElem(_a.w, _b.w);
}
inline Vector4 Dot(const SoaFloat3& _a, const SoaFloat3& _b) {
  return mulPerElem(_a.x, _b.x) + mulPerElem(_a.y, _b.y) + mulPerElem(_a.z, _b.z);
}
inline Vector4 Dot(const SoaFloat2& _a, const SoaFloat2& _b) {
  return mulPerElem(_a.x, _b.x) + mulPerElem(_a.y, _b.y);
}

// Returns the cross product of _a and _b.
inline SoaFloat3 CrossProduct(const SoaFloat3& _a, const SoaFloat3& _b) {
  const SoaFloat3 r = {mulPerElem(_a.y, _b.z) - mulPerElem(_b.y, _a.z), mulPerElem(_a.z, _b.x) - mulPerElem(_b.z, _a.x),
                       mulPerElem(_a.x, _b.y) - mulPerElem(_b.x, _a.y)};
  return r;
}

// Returns the length |_v| of _v.
inline Vector4 Length(const SoaFloat4& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z) + mulPerElem(_v.w, _v.w);
  return sqrtPerElem(len2);
}
inline Vector4 Length(const SoaFloat3& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z);
  return sqrtPerElem(len2);
}
inline Vector4 Length(const SoaFloat2& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y);
  return sqrtPerElem(len2);
}

// Returns the square length |_v|^2 of _v.
inline Vector4 LengthSqr(const SoaFloat4& _v) {
  return mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z) + mulPerElem(_v.w, _v.w);
}
inline Vector4 LengthSqr(const SoaFloat3& _v) {
  return mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z);
}
inline Vector4 LengthSqr(const SoaFloat2& _v) {
  return mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y);
}

// Returns the normalized vector _v.
inline SoaFloat4 Normalize(const SoaFloat4& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z) + mulPerElem(_v.w, _v.w);
  const Vector4 inv_len = divPerElem(Vector4::one(), sqrtPerElem(len2));
  const SoaFloat4 r = {mulPerElem(_v.x, inv_len), mulPerElem(_v.y, inv_len), mulPerElem(_v.z, inv_len),
                       mulPerElem(_v.w, inv_len)};
  return r;
}
inline SoaFloat3 Normalize(const SoaFloat3& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z);
  const Vector4 inv_len = divPerElem(Vector4::one(), sqrtPerElem(len2));
  const SoaFloat3 r = {mulPerElem(_v.x, inv_len), mulPerElem(_v.y, inv_len), mulPerElem(_v.z, inv_len)};
  return r;
}
inline SoaFloat2 Normalize(const SoaFloat2& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y);
  const Vector4 inv_len = divPerElem(Vector4::one(), sqrtPerElem(len2));
  const SoaFloat2 r = {mulPerElem(_v.x, inv_len), mulPerElem(_v.y, inv_len)};
  return r;
}

// Test if each vector _v is normalized.
inline Vector4Int IsNormalized(const SoaFloat4& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z) + mulPerElem(_v.w, _v.w);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceSq));
}
inline Vector4Int IsNormalized(const SoaFloat3& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceSq));
}
inline Vector4Int IsNormalized(const SoaFloat2& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceSq));
}

// Test if each vector _v is normalized using estimated tolerance.
inline Vector4Int IsNormalizedEst(const SoaFloat4& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z) + mulPerElem(_v.w, _v.w);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceEstSq));
}
inline Vector4Int IsNormalizedEst(const SoaFloat3& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y) + mulPerElem(_v.z, _v.z);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceEstSq));
}
inline Vector4Int IsNormalizedEst(const SoaFloat2& _v) {
  const Vector4 len2 = mulPerElem(_v.x, _v.x) + mulPerElem(_v.y, _v.y);
  return cmpLt(absPerElem(len2 - Vector4::one()),
               Vector4(kNormalizationToleranceEstSq));
}

// Returns the linear interpolation of _a and _b with coefficient _f.
// _f is not limited to range [0,1].
inline SoaFloat4 Lerp(const SoaFloat4& _a, const SoaFloat4& _b,
                          const Vector4& _f) {
  const SoaFloat4 r = {mulPerElem((_b.x - _a.x), _f) + _a.x, mulPerElem((_b.y - _a.y), _f) + _a.y,
                       mulPerElem((_b.z - _a.z), _f) + _a.z, mulPerElem((_b.w - _a.w), _f) + _a.w};
  return r;
}
inline SoaFloat3 Lerp(const SoaFloat3& _a, const SoaFloat3& _b,
                          const Vector4& _f) {
  const SoaFloat3 r = {mulPerElem((_b.x - _a.x), _f) + _a.x, mulPerElem((_b.y - _a.y), _f) + _a.y,
                       mulPerElem((_b.z - _a.z), _f) + _a.z};
  return r;
}
inline SoaFloat2 Lerp(const SoaFloat2& _a, const SoaFloat2& _b,
                          const Vector4& _f) {
  const SoaFloat2 r = {mulPerElem((_b.x - _a.x), _f) + _a.x, mulPerElem((_b.y - _a.y), _f) + _a.y};
  return r;
}

// Returns the minimum of each element of _a and _b.
inline SoaFloat4 Min(const SoaFloat4& _a, const SoaFloat4& _b) {
  const SoaFloat4 r = {minPerElem(_a.x, _b.x), minPerElem(_a.y, _b.y), minPerElem(_a.z, _b.z),
                       minPerElem(_a.w, _b.w)};
  return r;
}

inline SoaFloat3 Min(const SoaFloat3& _a, const SoaFloat3& _b) {
  const SoaFloat3 r = {minPerElem(_a.x, _b.x), minPerElem(_a.y, _b.y), minPerElem(_a.z, _b.z)};
  return r;
}
inline SoaFloat2 Min(const SoaFloat2& _a, const SoaFloat2& _b) {
  const SoaFloat2 r = {minPerElem(_a.x, _b.x), minPerElem(_a.y, _b.y)};
  return r;
}

// Returns the maximum of each element of _a and _b.
inline SoaFloat4 Max(const SoaFloat4& _a, const SoaFloat4& _b) {
  const SoaFloat4 r = {maxPerElem(_a.x, _b.x), maxPerElem(_a.y, _b.y), maxPerElem(_a.z, _b.z),
                       maxPerElem(_a.w, _b.w)};
  return r;
}
inline SoaFloat3 Max(const SoaFloat3& _a, const SoaFloat3& _b) {
  const SoaFloat3 r = {maxPerElem(_a.x, _b.x), maxPerElem(_a.y, _b.y), maxPerElem(_a.z, _b.z)};
  return r;
}
inline SoaFloat2 Max(const SoaFloat2& _a, const SoaFloat2& _b) {
  const SoaFloat2 r = {maxPerElem(_a.x, _b.x), maxPerElem(_a.y, _b.y)};
  return r;
}

// Clamps each element of _x between _a and _b.
// _a must be less or equal to b;
inline SoaFloat4 Clamp(const SoaFloat4& _a, const SoaFloat4& _v,
                           const SoaFloat4& _b) {
  return Max(_a, Min(_v, _b));
}
inline SoaFloat3 Clamp(const SoaFloat3& _a, const SoaFloat3& _v,
                           const SoaFloat3& _b) {
  return Max(_a, Min(_v, _b));
}
inline SoaFloat2 Clamp(const SoaFloat2& _a, const SoaFloat2& _v,
                           const SoaFloat2& _b) {
  return Max(_a, Min(_v, _b));
}

} // namespace Soa
} // namespace Vectormath


#endif // VECTORMATH_SOA_FLOAT_HPP

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================