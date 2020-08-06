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

#ifndef VECTORMATH_SOA_HPP
#define VECTORMATH_SOA_HPP

namespace Vectormath
{
namespace Soa
{

// ========================================================
// Constants
// ========================================================

// Defines the square normalization tolerance value.
static const float kNormalizationToleranceSq = 1e-6f;
static const float kNormalizationToleranceEstSq = 2e-3f;

// ========================================================
// Forward Declarations
// ========================================================

class SoaFloat2;
class SoaFloat3;
class SoaFloat4;
class SoaFloat4x4;
class SoaQuaternion;
class SoaTransform;


//----------------------------------------------------------------------------
// SoaFloat2
//----------------------------------------------------------------------------

class SoaFloat2 {

public:

  Vector4 x, y;

  static inline SoaFloat2 Load(const Vector4& _x, const Vector4& _y);

  static inline SoaFloat2 zero();

  static inline SoaFloat2 one();

  static inline SoaFloat2 x_axis();

  static inline SoaFloat2 y_axis();

};

//----------------------------------------------------------------------------
// SoaFloat3
//----------------------------------------------------------------------------

class SoaFloat3 {

public:

  Vector4 x, y, z;
  
  static inline SoaFloat3 Load(const Vector4& _x, const Vector4& _y,
                                   const Vector4& _z);

  static inline SoaFloat3 Load(const SoaFloat2& _v, const Vector4& _z);

  static inline SoaFloat3 zero();

  static inline SoaFloat3 one();

  static inline SoaFloat3 x_axis();

  static inline SoaFloat3 y_axis();

  static inline SoaFloat3 z_axis();

};

//----------------------------------------------------------------------------
// SoaFloat4
//----------------------------------------------------------------------------

class SoaFloat4 {

public:

  Vector4 x, y, z, w;

  static inline SoaFloat4 Load(const Vector4& _x, const Vector4& _y,
                                   const Vector4& _z, const Vector4& _w);

  static inline SoaFloat4 Load(const SoaFloat3& _v, const Vector4& _w);

  static inline SoaFloat4 Load(const SoaFloat2& _v, const Vector4& _z,
                                   const Vector4& _w);

  static inline SoaFloat4 zero();

  static inline SoaFloat4 one();

  static inline SoaFloat4 x_axis();

  static inline SoaFloat4 y_axis();

  static inline SoaFloat4 z_axis();

  static inline SoaFloat4 w_axis();


};

//----------------------------------------------------------------------------
// SoaFloat 2, 3, 4 Methods
//----------------------------------------------------------------------------

// Returns per element addition of _a and _b using operator +.
inline SoaFloat4 operator+(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline SoaFloat3 operator+(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline SoaFloat2 operator+(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns per element subtraction of _a and _b using operator -.
inline SoaFloat4 operator-(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline SoaFloat3 operator-(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline SoaFloat2 operator-(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns per element negative value of _v.
inline SoaFloat4 operator-(const SoaFloat4& _v);
inline SoaFloat3 operator-(const SoaFloat3& _v);
inline SoaFloat2 operator-(const SoaFloat2& _v);

// Returns per element multiplication of _a and _b using operator *.
inline SoaFloat4 operator*(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline SoaFloat3 operator*(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline SoaFloat2 operator*(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns per element multiplication of _a and scalar value _f using
// operator *.
inline SoaFloat4 operator*(const SoaFloat4& _a,
                                          const Vector4& _f);
inline SoaFloat3 operator*(const SoaFloat3& _a,
                                          const Vector4& _f);
inline SoaFloat2 operator*(const SoaFloat2& _a,
                                          const Vector4& _f);

// Returns per element division of _a and _b using operator /.
inline SoaFloat4 operator/(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline SoaFloat3 operator/(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline SoaFloat2 operator/(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns per element division of _a and scalar value _f using operator/.
inline SoaFloat4 operator/(const SoaFloat4& _a,
                                          const Vector4& _f);
inline SoaFloat3 operator/(const SoaFloat3& _a,
                                          const Vector4& _f);
inline SoaFloat2 operator/(const SoaFloat2& _a,
                                          const Vector4& _f);

// Returns true if each element of a is less than each element of _b.
inline Vector4Int operator<(const SoaFloat4& _a,
                                         const SoaFloat4& _b);
inline Vector4Int operator<(const SoaFloat3& _a,
                                         const SoaFloat3& _b);
inline Vector4Int operator<(const SoaFloat2& _a,
                                         const SoaFloat2& _b);

// Returns true if each element of a is less or equal to each element of _b.
inline Vector4Int operator<=(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline Vector4Int operator<=(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline Vector4Int operator<=(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns true if each element of a is greater than each element of _b.
inline Vector4Int operator>(const SoaFloat4& _a,
                                         const SoaFloat4& _b);
inline Vector4Int operator>(const SoaFloat3& _a,
                                         const SoaFloat3& _b);
inline Vector4Int operator>(const SoaFloat2& _a,
                                         const SoaFloat2& _b);

// Returns true if each element of a is greater or equal to each element of _b.
inline Vector4Int operator>=(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline Vector4Int operator>=(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline Vector4Int operator>=(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns true if each element of _a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
inline Vector4Int operator==(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline Vector4Int operator==(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline Vector4Int operator==(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns true if each element of a is different from each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
inline Vector4Int operator!=(const SoaFloat4& _a,
                                          const SoaFloat4& _b);
inline Vector4Int operator!=(const SoaFloat3& _a,
                                          const SoaFloat3& _b);
inline Vector4Int operator!=(const SoaFloat2& _a,
                                          const SoaFloat2& _b);

// Returns the (horizontal) addition of each element of _v.
inline Vector4 HAdd(const SoaFloat4& _v);
inline Vector4 HAdd(const SoaFloat3& _v);
inline Vector4 HAdd(const SoaFloat2& _v);

// Returns the dot product of _a and _b.
inline Vector4 Dot(const SoaFloat4& _a, const SoaFloat4& _b);
inline Vector4 Dot(const SoaFloat3& _a, const SoaFloat3& _b);
inline Vector4 Dot(const SoaFloat2& _a, const SoaFloat2& _b);

// Returns the cross product of _a and _b.
inline SoaFloat3 CrossProduct(const SoaFloat3& _a, const SoaFloat3& _b);

// Returns the length |_v| of _v.
inline Vector4 Length(const SoaFloat4& _v);
inline Vector4 Length(const SoaFloat3& _v);
inline Vector4 Length(const SoaFloat2& _v);

// Returns the square length |_v|^2 of _v.
inline Vector4 LengthSqr(const SoaFloat4& _v);
inline Vector4 LengthSqr(const SoaFloat3& _v);
inline Vector4 LengthSqr(const SoaFloat2& _v);

// Returns the normalized vector _v.
inline SoaFloat4 Normalize(const SoaFloat4& _v);
inline SoaFloat3 Normalize(const SoaFloat3& _v);
inline SoaFloat2 Normalize(const SoaFloat2& _v);

// Test if each vector _v is normalized.
inline Vector4Int IsNormalized(const SoaFloat4& _v);
inline Vector4Int IsNormalized(const SoaFloat3& _v);
inline Vector4Int IsNormalized(const SoaFloat2& _v);

// Test if each vector _v is normalized using estimated tolerance.
inline Vector4Int IsNormalizedEst(const SoaFloat4& _v);
inline Vector4Int IsNormalizedEst(const SoaFloat3& _v);
inline Vector4Int IsNormalizedEst(const SoaFloat2& _v);

// Returns the linear interpolation of _a and _b with coefficient _f.
// _f is not limited to range [0,1].
inline SoaFloat4 Lerp(const SoaFloat4& _a, const SoaFloat4& _b,
                          const Vector4& _f);
inline SoaFloat3 Lerp(const SoaFloat3& _a, const SoaFloat3& _b,
                          const Vector4& _f);
inline SoaFloat2 Lerp(const SoaFloat2& _a, const SoaFloat2& _b,
                          const Vector4& _f);

// Returns the minimum of each element of _a and _b.
inline SoaFloat4 Min(const SoaFloat4& _a, const SoaFloat4& _b);

inline SoaFloat3 Min(const SoaFloat3& _a, const SoaFloat3& _b);
inline SoaFloat2 Min(const SoaFloat2& _a, const SoaFloat2& _b);

// Returns the maximum of each element of _a and _b.
inline SoaFloat4 Max(const SoaFloat4& _a, const SoaFloat4& _b);
inline SoaFloat3 Max(const SoaFloat3& _a, const SoaFloat3& _b);
inline SoaFloat2 Max(const SoaFloat2& _a, const SoaFloat2& _b);

// Clamps each element of _x between _a and _b.
// _a must be less or equal to b;
inline SoaFloat4 Clamp(const SoaFloat4& _a, const SoaFloat4& _v,
                           const SoaFloat4& _b);
inline SoaFloat3 Clamp(const SoaFloat3& _a, const SoaFloat3& _v,
                           const SoaFloat3& _b);
inline SoaFloat2 Clamp(const SoaFloat2& _a, const SoaFloat2& _v,
                           const SoaFloat2& _b);


//----------------------------------------------------------------------------
// SoaFloat4x4
//----------------------------------------------------------------------------

// Declare the 4x4 soa matrix type. Uses the column major convention where the
// matrix-times-vector is written v'=Mv:
// [ m.cols[0].x m.cols[1].x m.cols[2].x m.cols[3].x ]   {v.x}
// | m.cols[0].y m.cols[1].y m.cols[2].y m.cols[3].y | * {v.y}
// | m.cols[0].z m.cols[1].y m.cols[2].y m.cols[3].y |   {v.z}
// [ m.cols[0].w m.cols[1].w m.cols[2].w m.cols[3].w ]   {v.1}
class SoaFloat4x4 {

public:

  // Soa matrix columns.
  SoaFloat4 cols[4];

  // Returns the identity matrix.
  static inline SoaFloat4x4 identity();

  // Returns a scaling matrix that scales along _v.
  // _v.w is ignored.
  static inline SoaFloat4x4 Scaling(const SoaFloat4& _v);

  // Returns the rotation matrix built from quaternion defined by x, y, z and w
  // components of _v.
  static inline SoaFloat4x4 FromQuaternion(const SoaQuaternion& _q);

  // Returns the affine transformation matrix built from split translation,
  // rotation (quaternion) and scale.
  static inline SoaFloat4x4 FromAffine(const SoaFloat3& _translation,
                                           const SoaQuaternion& _quaternion,
                                           const SoaFloat3& _scale);
};

// Returns the transpose of matrix _m.
inline SoaFloat4x4 Transpose(const SoaFloat4x4& _m);

// Returns the inverse of matrix _m.
inline SoaFloat4x4 Invert(const SoaFloat4x4& _m);

// Scales matrix _m along the axis defined by _v components.
// _v.w is ignored.
inline SoaFloat4x4 Scale(const SoaFloat4x4& _m, const SoaFloat4& _v);

// Computes the multiplication of matrix Float4x4 and vector  _v.
inline SoaFloat4 operator*(const SoaFloat4x4& _m,
                                          const SoaFloat4& _v);

// Computes the multiplication of two matrices _a and _b.
inline SoaFloat4x4 operator*(const SoaFloat4x4& _a,
                                            const SoaFloat4x4& _b);

// Computes the per element addition of two matrices _a and _b.
inline SoaFloat4x4 operator+(const SoaFloat4x4& _a,
                                            const SoaFloat4x4& _b);

// Computes the per element subtraction of two matrices _a and _b.
inline SoaFloat4x4 operator-(const SoaFloat4x4& _a,
                                            const SoaFloat4x4& _b);


//----------------------------------------------------------------------------
// SOA Quaternion
//----------------------------------------------------------------------------

class SoaQuaternion {

public:

  Vector4 x, y, z, w;

// Loads a quaternion from 4 Vector4 values.
  static inline SoaQuaternion Load(const Vector4& _x, const Vector4& _y,
                                       const Vector4& _z, const Vector4& _w);

  // Returns the identity SoaQuaternion.
  static inline SoaQuaternion identity();
};

// Returns the conjugate of _q. This is the same as the inverse if _q is
// normalized. Otherwise the magnitude of the inverse is 1.f/|_q|.
inline SoaQuaternion Conjugate(const SoaQuaternion& _q);

// Returns the negate of _q. This represent the same rotation as q.
inline SoaQuaternion operator-(const SoaQuaternion& _q);

// Returns the normalized SoaQuaternion _q.
inline SoaQuaternion Normalize(const SoaQuaternion& _q);

// Returns the estimated normalized SoaQuaternion _q.
inline SoaQuaternion NormalizeEst(const SoaQuaternion& _q);

// Test if each quaternion of _q is normalized.
inline Vector4Int IsNormalized(const SoaQuaternion& _q);

// Test if each quaternion of _q is normalized. using estimated tolerance.
inline Vector4Int IsNormalizedEst(const SoaQuaternion& _q);

// Returns the linear interpolation of SoaQuaternion _a and _b with coefficient
// _f.
inline SoaQuaternion Lerp(const SoaQuaternion& _a, const SoaQuaternion& _b,
                              const Vector4& _f);

// Returns the linear interpolation of SoaQuaternion _a and _b with coefficient
// _f.
inline SoaQuaternion NLerp(const SoaQuaternion& _a, const SoaQuaternion& _b,
                               const Vector4& _f);

// Returns the estimated linear interpolation of SoaQuaternion _a and _b with
// coefficient _f.
inline SoaQuaternion NLerpEst(const SoaQuaternion& _a,
                                  const SoaQuaternion& _b, const Vector4& _f);

// Returns the addition of _a and _b.
inline SoaQuaternion operator+(
    const SoaQuaternion& _a, const SoaQuaternion& _b);

// Returns the multiplication of _q and scalar value _f.
inline SoaQuaternion operator*(
    const SoaQuaternion& _q, const Vector4& _f);

// Returns the multiplication of _a and _b. If both _a and _b are normalized,
// then the result is normalized.
inline SoaQuaternion operator*(
    const SoaQuaternion& _a, const SoaQuaternion& _b);

// Returns true if each element of _a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
inline Vector4Int operator==(const SoaQuaternion& _a,
                                          const SoaQuaternion& _b);

//----------------------------------------------------------------------------
// SOA Transform
//----------------------------------------------------------------------------

// Stores an affine transformation with separate translation, rotation and scale
// attributes.
class SoaTransform {

public:

  SoaFloat3 translation;
  SoaQuaternion rotation;
  SoaFloat3 scale;
  
  static inline SoaTransform identity();

};

} // namespace Soa
} // namespace Vectormath

// Inline implementations:
#include "float.hpp"
#include "float4x4.hpp"
#include "quaternion.hpp"
#include "transform.hpp"


#endif // VECTORMATH_SOA_HPP

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================

