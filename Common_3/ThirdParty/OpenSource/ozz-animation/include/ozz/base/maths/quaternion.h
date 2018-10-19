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

#ifndef OZZ_OZZ_BASE_MATHS_QUATERNION_H_
#define OZZ_OZZ_BASE_MATHS_QUATERNION_H_

#include <cassert>

#include "math_constant.h"
#include "math_ex.h"
#include "vec_float.h"
#include "../platform.h"

namespace ozz {
namespace math {

struct Quaternion {
  float x, y, z, w;

  // Constructs an uninitialized quaternion.
  OZZ_INLINE Quaternion() {}

  // Constructs a quaternion from 4 floating point values.
  OZZ_INLINE Quaternion(float _x, float _y, float _z, float _w)
      : x(_x), y(_y), z(_z), w(_w) {}

  // Returns a normalized quaternion initialized from an axis angle
  // representation.
  // Assumes the axis part (x, y, z) of _axis_angle is normalized.
  // _axis_angle.w is the angle in radian.
  static OZZ_INLINE Quaternion FromAxisAngle(const Float4& _axis_angle);

  // Returns a normalized quaternion initialized from an Euler representation.
  // Euler angles are ordered Heading, Elevation and Bank, or Yaw, Pitch and
  // Roll.
  static OZZ_INLINE Quaternion FromEuler(const Float3& _euler);

  // Returns the identity quaternion.
  static OZZ_INLINE Quaternion identity() {
    return Quaternion(0.f, 0.f, 0.f, 1.f);
  }
};

// Returns true if each element of a is equal to each element of _b.
OZZ_INLINE bool operator==(const Quaternion& _a, const Quaternion& _b) {
  return _a.x == _b.x && _a.y == _b.y && _a.z == _b.z && _a.w == _b.w;
}

// Returns true if one element of a differs from one element of _b.
OZZ_INLINE bool operator!=(const Quaternion& _a, const Quaternion& _b) {
  return _a.x != _b.x || _a.y != _b.y || _a.z != _b.z || _a.w != _b.w;
}

// Returns the conjugate of _q. This is the same as the inverse if _q is
// normalized. Otherwise the magnitude of the inverse is 1.f/|_q|.
OZZ_INLINE Quaternion Conjugate(const Quaternion& _q) {
  return Quaternion(-_q.x, -_q.y, -_q.z, _q.w);
}

// Returns the addition of _a and _b.
OZZ_INLINE Quaternion operator+(const Quaternion& _a, const Quaternion& _b) {
  return Quaternion(_a.x + _b.x, _a.y + _b.y, _a.z + _b.z, _a.w + _b.w);
}

// Returns the multiplication of _q and a scalar _f.
OZZ_INLINE Quaternion operator*(const Quaternion& _q, float _f) {
  return Quaternion(_q.x * _f, _q.y * _f, _q.z * _f, _q.w * _f);
}

// Returns the multiplication of _a and _b. If both _a and _b are normalized,
// then the result is normalized.
OZZ_INLINE Quaternion operator*(const Quaternion& _a, const Quaternion& _b) {
  return Quaternion(_a.w * _b.x + _a.x * _b.w + _a.y * _b.z - _a.z * _b.y,
                    _a.w * _b.y + _a.y * _b.w + _a.z * _b.x - _a.x * _b.z,
                    _a.w * _b.z + _a.z * _b.w + _a.x * _b.y - _a.y * _b.x,
                    _a.w * _b.w - _a.x * _b.x - _a.y * _b.y - _a.z * _b.z);
}

// Returns the negate of _q. This represent the same rotation as q.
OZZ_INLINE Quaternion operator-(const Quaternion& _q) {
  return Quaternion(-_q.x, -_q.y, -_q.z, -_q.w);
}

// Returns true if the angle between _a and _b is less than _tolerance.
OZZ_INLINE bool Compare(const math::Quaternion& _a, const math::Quaternion& _b,
                        float _tolerance) {
  // Computes w component of a-1 * b.
  const float diff_w = _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
  // Converts w back to an angle.
  const float angle = 2.f * std::acos(Min(std::abs(diff_w), 1.f));
  return std::abs(angle) <= _tolerance;
}

// Returns true if _q is a normalized quaternion.
OZZ_INLINE bool IsNormalized(const Quaternion& _q) {
  const float sq_len = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  return std::abs(sq_len - 1.f) < kNormalizationToleranceSq;
}

// Returns the normalized quaternion _q.
OZZ_INLINE Quaternion Normalize(const Quaternion& _q) {
  const float sq_len = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  assert(sq_len != 0.f && "_q is not normalizable");
  const float inv_len = 1.f / std::sqrt(sq_len);
  return Quaternion(_q.x * inv_len, _q.y * inv_len, _q.z * inv_len,
                    _q.w * inv_len);
}

// Returns the normalized quaternion _q if the norm of _q is not 0.
// Otherwise returns _safer.
OZZ_INLINE Quaternion NormalizeSafe(const Quaternion& _q,
                                    const Quaternion& _safer) {
  assert(IsNormalized(_safer) && "_safer is not normalized");
  const float sq_len = _q.x * _q.x + _q.y * _q.y + _q.z * _q.z + _q.w * _q.w;
  if (sq_len == 0) {
    return _safer;
  }
  const float inv_len = 1.f / std::sqrt(sq_len);
  return Quaternion(_q.x * inv_len, _q.y * inv_len, _q.z * inv_len,
                    _q.w * inv_len);
}

OZZ_INLINE Quaternion Quaternion::FromAxisAngle(const Float4& _axis_angle) {
  assert(IsNormalized(Float3(_axis_angle.x, _axis_angle.y, _axis_angle.z)));
  const float half_angle = _axis_angle.w * .5f;
  const float sin_half = std::sin(half_angle);
  const float cos_half = std::cos(half_angle);
  return Quaternion(_axis_angle.x * sin_half, _axis_angle.y * sin_half,
                    _axis_angle.z * sin_half, cos_half);
}

// Returns to an axis angle representation of quaternion _q.
// Assumes quaternion _q is normalized.
OZZ_INLINE Float4 ToAxisAngle(const Quaternion& _q) {
  assert(IsNormalized(_q));
  const float clamped_w = Clamp(-1.f, _q.w, 1.f);
  const float angle = 2.f * std::acos(clamped_w);
  const float s = std::sqrt(1.f - clamped_w * clamped_w);

  // Assuming quaternion normalized then s always positive.
  if (s < .001f) {  // Tests to avoid divide by zero.
    // If s close to zero then direction of axis is not important.
    return Float4(1.f, 0.f, 0.f, angle);
  } else {
    // Normalize axis
    const float inv_s = 1.f / s;
    return Float4(_q.x * inv_s, _q.y * inv_s, _q.z * inv_s, angle);
  }
}

OZZ_INLINE Quaternion Quaternion::FromEuler(const Float3& _euler) {
  const Float3 half_euler = _euler * .5f;
  const float c1 = std::cos(half_euler.x);
  const float s1 = std::sin(half_euler.x);
  const float c2 = std::cos(half_euler.y);
  const float s2 = std::sin(half_euler.y);
  const float c3 = std::cos(half_euler.z);
  const float s3 = std::sin(half_euler.z);
  const float c1c2 = c1 * c2;
  const float s1s2 = s1 * s2;
  return Quaternion(c1c2 * s3 + s1s2 * c3, s1 * c2 * c3 + c1 * s2 * s3,
                    c1 * s2 * c3 - s1 * c2 * s3, c1c2 * c3 - s1s2 * s3);
}

// Returns to an Euler representation of quaternion _q.
// Quaternion _q does not require to be normalized.
OZZ_INLINE Float3 ToEuler(const Quaternion& _q) {
  const float sqw = _q.w * _q.w;
  const float sqx = _q.x * _q.x;
  const float sqy = _q.y * _q.y;
  const float sqz = _q.z * _q.z;
  // If normalized is one, otherwise is correction factor.
  const float unit = sqx + sqy + sqz + sqw;
  const float test = _q.x * _q.y + _q.z * _q.w;
  Float3 euler;
  if (test > .499f * unit) {  // Singularity at north pole
    euler.x = 2.f * std::atan2(_q.x, _q.w);
    euler.y = ozz::math::kPi_2;
    euler.z = 0;
  } else if (test < -.499f * unit) {  // Singularity at south pole
    euler.x = -2 * std::atan2(_q.x, _q.w);
    euler.y = -kPi_2;
    euler.z = 0;
  } else {
    euler.x = std::atan2(2.f * _q.y * _q.w - 2.f * _q.x * _q.z,
                         sqx - sqy - sqz + sqw);
    euler.y = std::asin(2.f * test / unit);
    euler.z = std::atan2(2.f * _q.x * _q.w - 2.f * _q.y * _q.z,
                         -sqx + sqy - sqz + sqw);
  }
  return euler;
}

// Returns the linear interpolation of quaternion _a and _b with coefficient _f.
OZZ_INLINE Quaternion Lerp(const Quaternion& _a, const Quaternion& _b,
                           float _f) {
  return Quaternion((_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                    (_b.z - _a.z) * _f + _a.z, (_b.w - _a.w) * _f + _a.w);
}

// Returns the linear interpolation of quaternion _a and _b with coefficient _f.
OZZ_INLINE Quaternion NLerp(const Quaternion& _a, const Quaternion& _b,
                            float _f) {
  const Float4 lerp((_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                    (_b.z - _a.z) * _f + _a.z, (_b.w - _a.w) * _f + _a.w);
  const float sq_len =
      lerp.x * lerp.x + lerp.y * lerp.y + lerp.z * lerp.z + lerp.w * lerp.w;
  const float inv_len = 1.f / std::sqrt(sq_len);
  return Quaternion(lerp.x * inv_len, lerp.y * inv_len, lerp.z * inv_len,
                    lerp.w * inv_len);
}

// Returns the spherical interpolation of quaternion _a and _b with
// coefficient _f.
OZZ_INLINE Quaternion SLerp(const Quaternion& _a, const Quaternion& _b,
                            float _f) {
  assert(IsNormalized(_a));
  assert(IsNormalized(_b));
  // Calculate angle between them.
  float cos_half_theta = _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;

  // If _a=_b or _a=-_b then theta = 0 and we can return _a.
  if (std::abs(cos_half_theta) >= .999f) {
    return _a;
  }

  // Calculate temporary values.
  const float half_theta = std::acos(cos_half_theta);
  const float sin_half_theta = std::sqrt(1.f - cos_half_theta * cos_half_theta);

  // If theta = pi then result is not fully defined, we could rotate around any
  // axis normal to _a or _b.
  if (sin_half_theta < .001f) {
    return Quaternion((_a.x + _b.x) * .5f, (_a.y + _b.y) * .5f,
                      (_a.z + _b.z) * .5f, (_a.w + _b.w) * .5f);
  }

  const float ratio_a = std::sin((1.f - _f) * half_theta) / sin_half_theta;
  const float ratio_b = std::sin(_f * half_theta) / sin_half_theta;

  // Calculate Quaternion.
  return Quaternion(
      ratio_a * _a.x + ratio_b * _b.x, ratio_a * _a.y + ratio_b * _b.y,
      ratio_a * _a.z + ratio_b * _b.z, ratio_a * _a.w + ratio_b * _b.w);
}
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_QUATERNION_H_
