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

#ifndef OZZ_OZZ_BASE_MATHS_VEC_FLOAT_H_
#define OZZ_OZZ_BASE_MATHS_VEC_FLOAT_H_

#include <cassert>
#include <cmath>

#include "math_constant.h"
#include "../platform.h"

namespace ozz {
namespace math {

// Declares a 2d float vector.
struct Float2 {
  float x, y;

  // Constructs an uninitialized vector.
  OZZ_INLINE Float2() {}

  // Constructs a vector initialized with _f value.
  explicit OZZ_INLINE Float2(float _f) : x(_f), y(_f) {}

  // Constructs a vector initialized with _x and _y values.
  OZZ_INLINE Float2(float _x, float _y) : x(_x), y(_y) {}

  // Returns a vector with all components set to 0.
  static OZZ_INLINE Float2 zero() { return Float2(0.f, 0.f); }

  // Returns a vector with all components set to 1.
  static OZZ_INLINE Float2 one() { return Float2(1.f, 1.f); }

  // Returns a unitary vector x.
  static OZZ_INLINE Float2 x_axis() { return Float2(1.f, 0.f); }

  // Returns a unitary vector y.
  static OZZ_INLINE Float2 y_axis() { return Float2(0.f, 1.f); }
};

// Declares a 3d float vector.
struct Float3 {
  float x, y, z;

  // Constructs an uninitialized vector.
  OZZ_INLINE Float3() {}

  // Constructs a vector initialized with _f value.
  explicit OZZ_INLINE Float3(float _f) : x(_f), y(_f), z(_f) {}

  // Constructs a vector initialized with _x, _y and _z values.
  OZZ_INLINE Float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

  // Returns a vector initialized with _v.x, _v.y and _z values.
  OZZ_INLINE Float3(Float2 _v, float _z) : x(_v.x), y(_v.y), z(_z) {}

  // Returns a vector with all components set to 0.
  static OZZ_INLINE Float3 zero() { return Float3(0.f, 0.f, 0.f); }

  // Returns a vector with all components set to 1.
  static OZZ_INLINE Float3 one() { return Float3(1.f, 1.f, 1.f); }

  // Returns a unitary vector x.
  static OZZ_INLINE Float3 x_axis() { return Float3(1.f, 0.f, 0.f); }

  // Returns a unitary vector y.
  static OZZ_INLINE Float3 y_axis() { return Float3(0.f, 1.f, 0.f); }

  // Returns a unitary vector z.
  static OZZ_INLINE Float3 z_axis() { return Float3(0.f, 0.f, 1.f); }
};

// Declares a 4d float vector.
struct Float4 {
  float x, y, z, w;

  // Constructs an uninitialized vector.
  OZZ_INLINE Float4() {}

  // Constructs a vector initialized with _f value.
  explicit OZZ_INLINE Float4(float _f) : x(_f), y(_f), z(_f), w(_f) {}

  // Constructs a vector initialized with _x, _y, _z and _w values.
  OZZ_INLINE Float4(float _x, float _y, float _z, float _w)
      : x(_x), y(_y), z(_z), w(_w) {}

  // Constructs a vector initialized with _v.x, _v.y, _v.z and _w values.
  OZZ_INLINE Float4(Float3 _v, float _w) : x(_v.x), y(_v.y), z(_v.z), w(_w) {}

  // Constructs a vector initialized with _v.x, _v.y, _z and _w values.
  OZZ_INLINE Float4(Float2 _v, float _z, float _w)
      : x(_v.x), y(_v.y), z(_z), w(_w) {}

  // Returns a vector with all components set to 0.
  static OZZ_INLINE Float4 zero() { return Float4(0.f, 0.f, 0.f, 0.f); }

  // Returns a vector with all components set to 1.
  static OZZ_INLINE Float4 one() { return Float4(1.f, 1.f, 1.f, 1.f); }

  // Returns a unitary vector x.
  static OZZ_INLINE Float4 x_axis() { return Float4(1.f, 0.f, 0.f, 0.f); }

  // Returns a unitary vector y.
  static OZZ_INLINE Float4 y_axis() { return Float4(0.f, 1.f, 0.f, 0.f); }

  // Returns a unitary vector z.
  static OZZ_INLINE Float4 z_axis() { return Float4(0.f, 0.f, 1.f, 0.f); }

  // Returns a unitary vector w.
  static OZZ_INLINE Float4 w_axis() { return Float4(0.f, 0.f, 0.f, 1.f); }
};

// Returns per element addition of _a and _b using operator +.
OZZ_INLINE Float4 operator+(const Float4& _a, const Float4& _b) {
  return Float4(_a.x + _b.x, _a.y + _b.y, _a.z + _b.z, _a.w + _b.w);
}
OZZ_INLINE Float3 operator+(const Float3& _a, const Float3& _b) {
  return Float3(_a.x + _b.x, _a.y + _b.y, _a.z + _b.z);
}
OZZ_INLINE Float2 operator+(const Float2& _a, const Float2& _b) {
  return Float2(_a.x + _b.x, _a.y + _b.y);
}

// Returns per element subtraction of _a and _b using operator -.
OZZ_INLINE Float4 operator-(const Float4& _a, const Float4& _b) {
  return Float4(_a.x - _b.x, _a.y - _b.y, _a.z - _b.z, _a.w - _b.w);
}
OZZ_INLINE Float3 operator-(const Float3& _a, const Float3& _b) {
  return Float3(_a.x - _b.x, _a.y - _b.y, _a.z - _b.z);
}
OZZ_INLINE Float2 operator-(const Float2& _a, const Float2& _b) {
  return Float2(_a.x - _b.x, _a.y - _b.y);
}

// Returns per element negative value of _v.
OZZ_INLINE Float4 operator-(const Float4& _v) {
  return Float4(-_v.x, -_v.y, -_v.z, -_v.w);
}
OZZ_INLINE Float3 operator-(const Float3& _v) {
  return Float3(-_v.x, -_v.y, -_v.z);
}
OZZ_INLINE Float2 operator-(const Float2& _v) { return Float2(-_v.x, -_v.y); }

// Returns per element multiplication of _a and _b using operator *.
OZZ_INLINE Float4 operator*(const Float4& _a, const Float4& _b) {
  return Float4(_a.x * _b.x, _a.y * _b.y, _a.z * _b.z, _a.w * _b.w);
}
OZZ_INLINE Float3 operator*(const Float3& _a, const Float3& _b) {
  return Float3(_a.x * _b.x, _a.y * _b.y, _a.z * _b.z);
}
OZZ_INLINE Float2 operator*(const Float2& _a, const Float2& _b) {
  return Float2(_a.x * _b.x, _a.y * _b.y);
}

// Returns per element multiplication of _a and scalar value _f using
// operator *.
OZZ_INLINE Float4 operator*(const Float4& _a, float _f) {
  return Float4(_a.x * _f, _a.y * _f, _a.z * _f, _a.w * _f);
}
OZZ_INLINE Float3 operator*(const Float3& _a, float _f) {
  return Float3(_a.x * _f, _a.y * _f, _a.z * _f);
}
OZZ_INLINE Float2 operator*(const Float2& _a, float _f) {
  return Float2(_a.x * _f, _a.y * _f);
}

// Returns per element division of _a and _b using operator /.
OZZ_INLINE Float4 operator/(const Float4& _a, const Float4& _b) {
  return Float4(_a.x / _b.x, _a.y / _b.y, _a.z / _b.z, _a.w / _b.w);
}
OZZ_INLINE Float3 operator/(const Float3& _a, const Float3& _b) {
  return Float3(_a.x / _b.x, _a.y / _b.y, _a.z / _b.z);
}
OZZ_INLINE Float2 operator/(const Float2& _a, const Float2& _b) {
  return Float2(_a.x / _b.x, _a.y / _b.y);
}

// Returns per element division of _a and scalar value _f using operator/.
OZZ_INLINE Float4 operator/(const Float4& _a, float _f) {
  return Float4(_a.x / _f, _a.y / _f, _a.z / _f, _a.w / _f);
}
OZZ_INLINE Float3 operator/(const Float3& _a, float _f) {
  return Float3(_a.x / _f, _a.y / _f, _a.z / _f);
}
OZZ_INLINE Float2 operator/(const Float2& _a, float _f) {
  return Float2(_a.x / _f, _a.y / _f);
}

// Returns the (horizontal) addition of each element of _v.
OZZ_INLINE float HAdd(const Float4& _v) { return _v.x + _v.y + _v.z + _v.w; }
OZZ_INLINE float HAdd(const Float3& _v) { return _v.x + _v.y + _v.z; }
OZZ_INLINE float HAdd(const Float2& _v) { return _v.x + _v.y; }

// Returns the dot product of _a and _b.
OZZ_INLINE float Dot(const Float4& _a, const Float4& _b) {
  return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w;
}
OZZ_INLINE float Dot(const Float3& _a, const Float3& _b) {
  return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z;
}
OZZ_INLINE float Dot(const Float2& _a, const Float2& _b) {
  return _a.x * _b.x + _a.y * _b.y;
}

// Returns the cross product of _a and _b.
OZZ_INLINE Float3 Cross(const Float3& _a, const Float3& _b) {
  return Float3(_a.y * _b.z - _b.y * _a.z, _a.z * _b.x - _b.z * _a.x,
                _a.x * _b.y - _b.x * _a.y);
}

// Returns the length |_v| of _v.
OZZ_INLINE float Length(const Float4& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  return std::sqrt(len2);
}
OZZ_INLINE float Length(const Float3& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  return std::sqrt(len2);
}
OZZ_INLINE float Length(const Float2& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y;
  return std::sqrt(len2);
}

// Returns the square length |_v|^2 of _v.
OZZ_INLINE float LengthSqr(const Float4& _v) {
  return _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
}
OZZ_INLINE float LengthSqr(const Float3& _v) {
  return _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
}
OZZ_INLINE float LengthSqr(const Float2& _v) {
  return _v.x * _v.x + _v.y * _v.y;
}

// Returns the normalized vector _v.
OZZ_INLINE Float4 Normalize(const Float4& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  assert(len2 != 0.f && "_v is not normalizable");
  const float len = std::sqrt(len2);
  return Float4(_v.x / len, _v.y / len, _v.z / len, _v.w / len);
}
OZZ_INLINE Float3 Normalize(const Float3& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  assert(len2 != 0.f && "_v is not normalizable");
  const float len = std::sqrt(len2);
  return Float3(_v.x / len, _v.y / len, _v.z / len);
}
OZZ_INLINE Float2 Normalize(const Float2& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y;
  assert(len2 != 0.f && "_v is not normalizable");
  const float len = std::sqrt(len2);
  return Float2(_v.x / len, _v.y / len);
}

// Returns true if _v is normalized.
OZZ_INLINE bool IsNormalized(const Float4& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  return std::abs(len2 - 1.f) < kNormalizationToleranceSq;
}
OZZ_INLINE bool IsNormalized(const Float3& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  return std::abs(len2 - 1.f) < kNormalizationToleranceSq;
}
OZZ_INLINE bool IsNormalized(const Float2& _v) {
  const float len2 = _v.x * _v.x + _v.y * _v.y;
  return std::abs(len2 - 1.f) < kNormalizationToleranceSq;
}

// Returns the normalized vector _v if the norm of _v is not 0.
// Otherwise returns _safer.
OZZ_INLINE Float4 NormalizeSafe(const Float4& _v, const Float4& _safer) {
  assert(IsNormalized(_safer) && "_safer is not normalized");
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z + _v.w * _v.w;
  if (len2 <= 0.f) {
    return _safer;
  }
  const float len = std::sqrt(len2);
  return Float4(_v.x / len, _v.y / len, _v.z / len, _v.w / len);
}
OZZ_INLINE Float3 NormalizeSafe(const Float3& _v, const Float3& _safer) {
  assert(IsNormalized(_safer) && "_safer is not normalized");
  const float len2 = _v.x * _v.x + _v.y * _v.y + _v.z * _v.z;
  if (len2 <= 0.f) {
    return _safer;
  }
  const float len = std::sqrt(len2);
  return Float3(_v.x / len, _v.y / len, _v.z / len);
}
OZZ_INLINE Float2 NormalizeSafe(const Float2& _v, const Float2& _safer) {
  assert(IsNormalized(_safer) && "_safer is not normalized");
  const float len2 = _v.x * _v.x + _v.y * _v.y;
  if (len2 <= 0.f) {
    return _safer;
  }
  const float len = std::sqrt(len2);
  return Float2(_v.x / len, _v.y / len);
}

// Returns the linear interpolation of _a and _b with coefficient _f.
// _f is not limited to range [0,1].
OZZ_INLINE Float4 Lerp(const Float4& _a, const Float4& _b, float _f) {
  return Float4((_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                (_b.z - _a.z) * _f + _a.z, (_b.w - _a.w) * _f + _a.w);
}
OZZ_INLINE Float3 Lerp(const Float3& _a, const Float3& _b, float _f) {
  return Float3((_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y,
                (_b.z - _a.z) * _f + _a.z);
}
OZZ_INLINE Float2 Lerp(const Float2& _a, const Float2& _b, float _f) {
  return Float2((_b.x - _a.x) * _f + _a.x, (_b.y - _a.y) * _f + _a.y);
}

// Returns true if the distance between _a and _b is less than _tolerance.
OZZ_INLINE bool Compare(const Float4& _a, const Float4& _b, float _tolerance) {
  const math::Float4 diff = _a - _b;
  return Dot(diff, diff) <= _tolerance * _tolerance;
}
OZZ_INLINE bool Compare(const Float3& _a, const Float3& _b, float _tolerance) {
  const math::Float3 diff = _a - _b;
  return Dot(diff, diff) <= _tolerance * _tolerance;
}
OZZ_INLINE bool Compare(const Float2& _a, const Float2& _b, float _tolerance) {
  const math::Float2 diff = _a - _b;
  return Dot(diff, diff) <= _tolerance * _tolerance;
}

// Returns true if each element of a is less than each element of _b.
OZZ_INLINE bool operator<(const Float4& _a, const Float4& _b) {
  return _a.x < _b.x && _a.y < _b.y && _a.z < _b.z && _a.w < _b.w;
}
OZZ_INLINE bool operator<(const Float3& _a, const Float3& _b) {
  return _a.x < _b.x && _a.y < _b.y && _a.z < _b.z;
}
OZZ_INLINE bool operator<(const Float2& _a, const Float2& _b) {
  return _a.x < _b.x && _a.y < _b.y;
}

// Returns true if each element of a is less or equal to each element of _b.
OZZ_INLINE bool operator<=(const Float4& _a, const Float4& _b) {
  return _a.x <= _b.x && _a.y <= _b.y && _a.z <= _b.z && _a.w <= _b.w;
}
OZZ_INLINE bool operator<=(const Float3& _a, const Float3& _b) {
  return _a.x <= _b.x && _a.y <= _b.y && _a.z <= _b.z;
}
OZZ_INLINE bool operator<=(const Float2& _a, const Float2& _b) {
  return _a.x <= _b.x && _a.y <= _b.y;
}

// Returns true if each element of a is greater than each element of _b.
OZZ_INLINE bool operator>(const Float4& _a, const Float4& _b) {
  return _a.x > _b.x && _a.y > _b.y && _a.z > _b.z && _a.w > _b.w;
}
OZZ_INLINE bool operator>(const Float3& _a, const Float3& _b) {
  return _a.x > _b.x && _a.y > _b.y && _a.z > _b.z;
}
OZZ_INLINE bool operator>(const Float2& _a, const Float2& _b) {
  return _a.x > _b.x && _a.y > _b.y;
}

// Returns true if each element of a is greater or equal to each element of _b.
OZZ_INLINE bool operator>=(const Float4& _a, const Float4& _b) {
  return _a.x >= _b.x && _a.y >= _b.y && _a.z >= _b.z && _a.w >= _b.w;
}
OZZ_INLINE bool operator>=(const Float3& _a, const Float3& _b) {
  return _a.x >= _b.x && _a.y >= _b.y && _a.z >= _b.z;
}
OZZ_INLINE bool operator>=(const Float2& _a, const Float2& _b) {
  return _a.x >= _b.x && _a.y >= _b.y;
}

// Returns true if each element of a is equal to each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
OZZ_INLINE bool operator==(const Float4& _a, const Float4& _b) {
  return _a.x == _b.x && _a.y == _b.y && _a.z == _b.z && _a.w == _b.w;
}
OZZ_INLINE bool operator==(const Float3& _a, const Float3& _b) {
  return _a.x == _b.x && _a.y == _b.y && _a.z == _b.z;
}
OZZ_INLINE bool operator==(const Float2& _a, const Float2& _b) {
  return _a.x == _b.x && _a.y == _b.y;
}

// Returns true if each element of a is different from each element of _b.
// Uses a bitwise comparison of _a and _b, no tolerance is applied.
OZZ_INLINE bool operator!=(const Float4& _a, const Float4& _b) {
  return _a.x != _b.x || _a.y != _b.y || _a.z != _b.z || _a.w != _b.w;
}
OZZ_INLINE bool operator!=(const Float3& _a, const Float3& _b) {
  return _a.x != _b.x || _a.y != _b.y || _a.z != _b.z;
}
OZZ_INLINE bool operator!=(const Float2& _a, const Float2& _b) {
  return _a.x != _b.x || _a.y != _b.y;
}

// Returns the minimum of each element of _a and _b.
OZZ_INLINE Float4 Min(const Float4& _a, const Float4& _b) {
  return Float4(_a.x < _b.x ? _a.x : _b.x, _a.y < _b.y ? _a.y : _b.y,
                _a.z < _b.z ? _a.z : _b.z, _a.w < _b.w ? _a.w : _b.w);
}
OZZ_INLINE Float3 Min(const Float3& _a, const Float3& _b) {
  return Float3(_a.x < _b.x ? _a.x : _b.x, _a.y < _b.y ? _a.y : _b.y,
                _a.z < _b.z ? _a.z : _b.z);
}
OZZ_INLINE Float2 Min(const Float2& _a, const Float2& _b) {
  return Float2(_a.x < _b.x ? _a.x : _b.x, _a.y < _b.y ? _a.y : _b.y);
}

// Returns the maximum of each element of _a and _b.
OZZ_INLINE Float4 Max(const Float4& _a, const Float4& _b) {
  return Float4(_a.x > _b.x ? _a.x : _b.x, _a.y > _b.y ? _a.y : _b.y,
                _a.z > _b.z ? _a.z : _b.z, _a.w > _b.w ? _a.w : _b.w);
}
OZZ_INLINE Float3 Max(const Float3& _a, const Float3& _b) {
  return Float3(_a.x > _b.x ? _a.x : _b.x, _a.y > _b.y ? _a.y : _b.y,
                _a.z > _b.z ? _a.z : _b.z);
}
OZZ_INLINE Float2 Max(const Float2& _a, const Float2& _b) {
  return Float2(_a.x > _b.x ? _a.x : _b.x, _a.y > _b.y ? _a.y : _b.y);
}

// Clamps each element of _x between _a and _b.
// _a must be less or equal to b;
OZZ_INLINE Float4 Clamp(const Float4& _a, const Float4& _v, const Float4& _b) {
  const Float4 min(_v.x < _b.x ? _v.x : _b.x, _v.y < _b.y ? _v.y : _b.y,
                   _v.z < _b.z ? _v.z : _b.z, _v.w < _b.w ? _v.w : _b.w);
  return Float4(_a.x > min.x ? _a.x : min.x, _a.y > min.y ? _a.y : min.y,
                _a.z > min.z ? _a.z : min.z, _a.w > min.w ? _a.w : min.w);
}
OZZ_INLINE Float3 Clamp(const Float3& _a, const Float3& _v, const Float3& _b) {
  const Float3 min(_v.x < _b.x ? _v.x : _b.x, _v.y < _b.y ? _v.y : _b.y,
                   _v.z < _b.z ? _v.z : _b.z);
  return Float3(_a.x > min.x ? _a.x : min.x, _a.y > min.y ? _a.y : min.y,
                _a.z > min.z ? _a.z : min.z);
}
OZZ_INLINE Float2 Clamp(const Float2& _a, const Float2& _v, const Float2& _b) {
  const Float2 min(_v.x < _b.x ? _v.x : _b.x, _v.y < _b.y ? _v.y : _b.y);
  return Float2(_a.x > min.x ? _a.x : min.x, _a.y > min.y ? _a.y : min.y);
}
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_VEC_FLOAT_H_
