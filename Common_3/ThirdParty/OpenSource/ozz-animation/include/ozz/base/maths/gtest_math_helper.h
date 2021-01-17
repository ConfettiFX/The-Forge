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

#ifndef OZZ_OZZ_BASE_MATHS_GTEST_MATH_HELPER_H_
#define OZZ_OZZ_BASE_MATHS_GTEST_MATH_HELPER_H_

static const float kFloatNearTolerance = 1e-5f;
static const float kFloatNearEstTolerance = 1e-3f;

// Implements "float near" test as a function. Avoids overloading compiler
// optimizer when too much EXPECT_NEAR are used in a single compilation unit.
inline void ExpectFloatNear(float _a, float _b,
                            float _tol = kFloatNearTolerance) {
  EXPECT_NEAR(_a, _b, _tol);
}

// Implements "int equality" test as a function. Avoids overloading compiler
// optimizer when too much EXPECT_TRUE are used in a single compilation unit.
inline void ExpectIntEq(int _a, int _b) { EXPECT_EQ(_a, _b); }

// Implements "bool equality" test as a function. Avoids overloading compiler
// optimizer when too much EXPECT_TRUE are used in a single compilation unit.
inline void ExpectTrue(bool _b) { EXPECT_TRUE(_b); }

// Macro for testing floats, dedicated to estimated functions with a lower
// precision.
#define EXPECT_FLOAT_EQ_EST(_expected, _x) \
  EXPECT_NEAR(_expected, _x, kFloatNearEstTolerance)

// Macro for testing ozz::math::Float4 members with x, y, z, w float values,
// using EXPECT_FLOAT_EQ internally.
#define EXPECT_FLOAT4_EQ(_expected, _x, _y, _z, _w) \
                                                    \
  do {                                              \
    SCOPED_TRACE("");                               \
    const ozz::math::Float4 expected(_expected);    \
    ExpectFloatNear(expected.x, _x);                \
    ExpectFloatNear(expected.y, _y);                \
    ExpectFloatNear(expected.z, _z);                \
    ExpectFloatNear(expected.w, _w);                \
                                                    \
  } while (void(0), 0)

// Macro for testing ozz::math::Float3 members with x, y, z float values,
// using EXPECT_FLOAT_EQ internally.
#define EXPECT_FLOAT3_EQ(_expected, _x, _y, _z)  \
                                                 \
  do {                                           \
    SCOPED_TRACE("");                            \
    const ozz::math::Float3 expected(_expected); \
    ExpectFloatNear(expected.x, _x);             \
    ExpectFloatNear(expected.y, _y);             \
    ExpectFloatNear(expected.z, _z);             \
                                                 \
  } while (void(0), 0)

// Macro for testing ozz::math::Float2 members with x, y float values,
// using EXPECT_NEAR internally.
#define EXPECT_FLOAT2_EQ(_expected, _x, _y)      \
                                                 \
  do {                                           \
    SCOPED_TRACE("");                            \
    const ozz::math::Float2 expected(_expected); \
    ExpectFloatNear(expected.x, _x);             \
    ExpectFloatNear(expected.y, _y);             \
                                                 \
  } while (void(0), 0)

// Macro for testing ozz::math::Quaternion members with x, y, z, w float value.
#define EXPECT_QUATERNION_EQ(_expected, _x, _y, _z, _w) \
                                                        \
  do {                                                  \
    SCOPED_TRACE("");                                   \
    const ozz::math::Quaternion expected(_expected);    \
    ExpectFloatNear(expected.x, _x);                    \
    ExpectFloatNear(expected.y, _y);                    \
    ExpectFloatNear(expected.z, _z);                    \
    ExpectFloatNear(expected.w, _w);                    \
                                                        \
  } while (void(0), 0)

#define _IMPL_EXPECT_SIMDFLOAT_EQ_TOL(_expected, _x, _y, _z, _w, _tol) \
                                                                       \
  do {                                                                 \
    union {                                                            \
      ozz::math::SimdFloat4 ret;                                       \
      float af[4];                                                     \
    } u = {_expected};                                                 \
    ExpectFloatNear(u.af[0], _x, _tol);                                \
    ExpectFloatNear(u.af[1], _y, _tol);                                \
    ExpectFloatNear(u.af[2], _z, _tol);                                \
    ExpectFloatNear(u.af[3], _w, _tol);                                \
                                                                       \
  } while (void(0), 0)

#define _IMPL_EXPECT_SIMDFLOAT_EQ(_expected, _x, _y, _z, _w) \
  _IMPL_EXPECT_SIMDFLOAT_EQ_TOL(_expected, _x, _y, _z, _w, kFloatNearTolerance)

#define _IMPL_EXPECT_SIMDFLOAT_EQ_EST(_expected, _x, _y, _z, _w) \
  _IMPL_EXPECT_SIMDFLOAT_EQ_TOL(_expected, _x, _y, _z, _w,       \
                                kFloatNearEstTolerance)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z, w values.
#define EXPECT_SIMDFLOAT_EQ(_expected, _x, _y, _z, _w)    \
                                                          \
  do {                                                    \
    SCOPED_TRACE("");                                     \
    _IMPL_EXPECT_SIMDFLOAT_EQ(_expected, _x, _y, _z, _w); \
                                                          \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z, w values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SIMDFLOAT_EQ_EST(_expected, _x, _y, _z, _w)    \
                                                              \
  do {                                                        \
    SCOPED_TRACE("");                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(_expected, _x, _y, _z, _w); \
                                                              \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y values with
// a user defined precision.
#define _IMPL_EXPECT_SIMDFLOAT2_EQ_TOL(_expected, _x, _y, _tol) \
                                                                \
  do {                                                          \
    union {                                                     \
      ozz::math::SimdFloat4 ret;                                \
      float af[4];                                              \
    } u = {_expected};                                          \
    ExpectFloatNear(u.af[0], _x, _tol);                         \
    ExpectFloatNear(u.af[1], _y, _tol);                         \
                                                                \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y values.
#define EXPECT_SIMDFLOAT2_EQ(_expected, _x, _y)                             \
                                                                            \
  do {                                                                      \
    SCOPED_TRACE("");                                                       \
    _IMPL_EXPECT_SIMDFLOAT2_EQ_TOL(_expected, _x, _y, kFloatNearTolerance); \
                                                                            \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SIMDFLOAT2_EQ_EST(_expected, _x, _y)                            \
                                                                               \
  do {                                                                         \
    SCOPED_TRACE("");                                                          \
    _IMPL_EXPECT_SIMDFLOAT2_EQ_TOL(_expected, _x, _y, kFloatNearEstTolerance); \
                                                                               \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z values.
// Dedicated to estimated functions with a user defined precision.
#define EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z, _tol)    \
                                                                 \
  do {                                                           \
    SCOPED_TRACE("");                                            \
    _IMPL_EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z, _tol); \
                                                                 \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z values with
// a user defined precision.
#define _IMPL_EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z, _tol) \
                                                                    \
  do {                                                              \
    union {                                                         \
      ozz::math::SimdFloat4 ret;                                    \
      float af[4];                                                  \
    } u = {_expected};                                              \
    ExpectFloatNear(u.af[0], _x, _tol);                             \
    ExpectFloatNear(u.af[1], _y, _tol);                             \
    ExpectFloatNear(u.af[2], _z, _tol);                             \
                                                                    \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z values.
#define EXPECT_SIMDFLOAT3_EQ(_expected, _x, _y, _z)       \
                                                          \
  do {                                                    \
    SCOPED_TRACE("");                                     \
    _IMPL_EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z, \
                                   kFloatNearTolerance);  \
                                                          \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SIMDFLOAT3_EQ_EST(_expected, _x, _y, _z)     \
                                                            \
  do {                                                      \
    SCOPED_TRACE("");                                       \
    _IMPL_EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z,   \
                                   kFloatNearEstTolerance); \
                                                            \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdFloat members with x, y, z values.
// Dedicated to estimated functions with a user defined precision.
#define EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z, _tol)    \
                                                                 \
  do {                                                           \
    SCOPED_TRACE("");                                            \
    _IMPL_EXPECT_SIMDFLOAT3_EQ_TOL(_expected, _x, _y, _z, _tol); \
                                                                 \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdInt members with x, y, z, w values.
#define EXPECT_SIMDINT_EQ(_expected, _x, _y, _z, _w) \
                                                     \
  do {                                               \
    SCOPED_TRACE("");                                \
    union {                                          \
      ozz::math::SimdInt4 ret;                       \
      int ai[4];                                     \
    } u = {_expected};                               \
    ExpectIntEq(u.ai[0], static_cast<int>(_x));      \
    ExpectIntEq(u.ai[1], static_cast<int>(_y));      \
    ExpectIntEq(u.ai[2], static_cast<int>(_z));      \
    ExpectIntEq(u.ai[3], static_cast<int>(_w));      \
                                                     \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat4 members with x, y, z, w float values.
#define EXPECT_FLOAT4x4_EQ(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, _y3, \
                           _z0, _z1, _z2, _z3, _w0, _w1, _w2, _w3)            \
                                                                              \
  do {                                                                        \
    SCOPED_TRACE("");                                                         \
    const ozz::math::Float4x4 expected(_expected);                            \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[0], _x0, _x1, _x2, _x3);          \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[1], _y0, _y1, _y2, _y3);          \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[2], _z0, _z1, _z2, _z3);          \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[3], _w0, _w1, _w2, _w3);          \
                                                                              \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdQuaternion members with x, y, z, w
// values.
#define EXPECT_SIMDQUATERNION_EQ(_expected, _x, _y, _z, _w)    \
                                                               \
  do {                                                         \
    SCOPED_TRACE("");                                          \
    _IMPL_EXPECT_SIMDFLOAT_EQ(_expected.xyzw, _x, _y, _z, _w); \
                                                               \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdQuaternion members with x, y, z, w
// values.
#define EXPECT_SIMDQUATERNION_EQ_EST(_expected, _x, _y, _z, _w)    \
                                                                   \
  do {                                                             \
    SCOPED_TRACE("");                                              \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(_expected.xyzw, _x, _y, _z, _w); \
                                                                   \
  } while (void(0), 0)

// Macro for testing ozz::math::simd::SimdQuaternion members with x, y, z, w
// values.
#define EXPECT_SIMDQUATERNION_EQ_TOL(_expected, _x, _y, _z, _w, _tol)    \
                                                                         \
  do {                                                                   \
    SCOPED_TRACE("");                                                    \
    _IMPL_EXPECT_SIMDFLOAT_EQ_TOL(_expected.xyzw, _x, _y, _z, _w, _tol); \
                                                                         \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat4 members with x, y, z, w float values.
#define EXPECT_SOAFLOAT4_EQ(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, _y3, \
                            _z0, _z1, _z2, _z3, _w0, _w1, _w2, _w3)            \
                                                                               \
  do {                                                                         \
    SCOPED_TRACE("");                                                          \
    const ozz::math::SoaFloat4 expected(_expected);                            \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.x, _x0, _x1, _x2, _x3);                 \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.y, _y0, _y1, _y2, _y3);                 \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.z, _z0, _z1, _z2, _z3);                 \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.w, _w0, _w1, _w2, _w3);                 \
                                                                               \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat4 members with x, y, z, w float values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SOAFLOAT4_EQ_EST(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, \
                                _y3, _z0, _z1, _z2, _z3, _w0, _w1, _w2, _w3)  \
                                                                              \
  do {                                                                        \
    SCOPED_TRACE("");                                                         \
    const ozz::math::SoaFloat4 expected(_expected);                           \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.x, _x0, _x1, _x2, _x3);            \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.y, _y0, _y1, _y2, _y3);            \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.z, _z0, _z1, _z2, _z3);            \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.w, _w0, _w1, _w2, _w3);            \
                                                                              \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat3 members with x, y, z float values.
#define EXPECT_SOAFLOAT3_EQ(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, _y3, \
                            _z0, _z1, _z2, _z3)                                \
                                                                               \
  do {                                                                         \
    SCOPED_TRACE("");                                                          \
    const ozz::math::SoaFloat3 expected(_expected);                            \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.x, _x0, _x1, _x2, _x3);                 \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.y, _y0, _y1, _y2, _y3);                 \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.z, _z0, _z1, _z2, _z3);                 \
                                                                               \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat3 members with x, y, z float values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SOAFLOAT3_EQ_EST(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, \
                                _y3, _z0, _z1, _z2, _z3)                      \
                                                                              \
  do {                                                                        \
    SCOPED_TRACE("");                                                         \
    const ozz::math::SoaFloat3 expected(_expected);                           \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.x, _x0, _x1, _x2, _x3);            \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.y, _y0, _y1, _y2, _y3);            \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.z, _z0, _z1, _z2, _z3);            \
                                                                              \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat2 members with x, y float values.
#define EXPECT_SOAFLOAT2_EQ(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, _y3) \
                                                                               \
  do {                                                                         \
    SCOPED_TRACE("");                                                          \
    const ozz::math::SoaFloat2 expected(_expected);                            \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.x, _x0, _x1, _x2, _x3);                 \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.y, _y0, _y1, _y2, _y3);                 \
                                                                               \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat2 members with x, y float values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SOAFLOAT2_EQ_EST(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, \
                                _y3)                                          \
                                                                              \
  do {                                                                        \
    SCOPED_TRACE("");                                                         \
    const ozz::math::SoaFloat2 expected(_expected);                           \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.x, _x0, _x1, _x2, _x3);            \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.y, _y0, _y1, _y2, _y3);            \
                                                                              \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaFloat2 members with x, y float values.
#define EXPECT_SOAFLOAT1_EQ(_expected, _x0, _x1, _x2, _x3) \
  _IMPL_EXPECT_SIMDFLOAT_EQ(_expected, _x0, _x1, _x2, _x3);

// Macro for testing ozz::math::SoaFloat2 members with x, y float values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SOAFLOAT1_EQ_EST(_expected, _x0, _x1, _x2, _x3) \
  _IMPL_EXPECT_SIMDFLOAT_EQ_EST(_expected, _x0, _x1, _x2, _x3);

// Macro for testing ozz::math::SoaQuaternion members with x, y, z, w float
// values.
#define EXPECT_SOAQUATERNION_EQ(_expected, _x0, _x1, _x2, _x3, _y0, _y1, _y2, \
                                _y3, _z0, _z1, _z2, _z3, _w0, _w1, _w2, _w3)  \
                                                                              \
  do {                                                                        \
    SCOPED_TRACE("");                                                         \
    const ozz::math::SoaQuaternion expected(_expected);                       \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.x, _x0, _x1, _x2, _x3);                \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.y, _y0, _y1, _y2, _y3);                \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.z, _z0, _z1, _z2, _z3);                \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.w, _w0, _w1, _w2, _w3);                \
                                                                              \
  } while (void(0), 0)

// Macro for testing ozz::math::SoaQuaternion members with x, y, z, w float
// values.
// Dedicated to estimated functions with a lower precision.
#define EXPECT_SOAQUATERNION_EQ_EST(_expected, _x0, _x1, _x2, _x3, _y0, _y1, \
                                    _y2, _y3, _z0, _z1, _z2, _z3, _w0, _w1,  \
                                    _w2, _w3)                                \
                                                                             \
  do {                                                                       \
    SCOPED_TRACE("");                                                        \
    const ozz::math::SoaQuaternion expected(_expected);                      \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.x, _x0, _x1, _x2, _x3);           \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.y, _y0, _y1, _y2, _y3);           \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.z, _z0, _z1, _z2, _z3);           \
    _IMPL_EXPECT_SIMDFLOAT_EQ_EST(expected.w, _w0, _w1, _w2, _w3);           \
                                                                             \
  } while (void(0), 0)

#define EXPECT_SOAFLOAT4x4_EQ(                                                 \
    _expected, col0xx, col0xy, col0xz, col0xw, col0yx, col0yy, col0yz, col0yw, \
    col0zx, col0zy, col0zz, col0zw, col0wx, col0wy, col0wz, col0ww, col1xx,    \
    col1xy, col1xz, col1xw, col1yx, col1yy, col1yz, col1yw, col1zx, col1zy,    \
    col1zz, col1zw, col1wx, col1wy, col1wz, col1ww, col2xx, col2xy, col2xz,    \
    col2xw, col2yx, col2yy, col2yz, col2yw, col2zx, col2zy, col2zz, col2zw,    \
    col2wx, col2wy, col2wz, col2ww, col3xx, col3xy, col3xz, col3xw, col3yx,    \
    col3yy, col3yz, col3yw, col3zx, col3zy, col3zz, col3zw, col3wx, col3wy,    \
    col3wz, col3ww)                                                            \
                                                                               \
  do {                                                                         \
    SCOPED_TRACE("");                                                          \
    const ozz::math::SoaFloat4x4 expected(_expected);                          \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[0].x, col0xx, col0xy, col0xz,      \
                              col0xw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[0].y, col0yx, col0yy, col0yz,      \
                              col0yw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[0].z, col0zx, col0zy, col0zz,      \
                              col0zw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[0].w, col0wx, col0wy, col0wz,      \
                              col0ww);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[1].x, col1xx, col1xy, col1xz,      \
                              col1xw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[1].y, col1yx, col1yy, col1yz,      \
                              col1yw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[1].z, col1zx, col1zy, col1zz,      \
                              col1zw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[1].w, col1wx, col1wy, col1wz,      \
                              col1ww);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[2].x, col2xx, col2xy, col2xz,      \
                              col2xw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[2].y, col2yx, col2yy, col2yz,      \
                              col2yw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[2].z, col2zx, col2zy, col2zz,      \
                              col2zw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[2].w, col2wx, col2wy, col2wz,      \
                              col2ww);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[3].x, col3xx, col3xy, col3xz,      \
                              col3xw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[3].y, col3yx, col3yy, col3yz,      \
                              col3yw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[3].z, col3zx, col3zy, col3zz,      \
                              col3zw);                                         \
    _IMPL_EXPECT_SIMDFLOAT_EQ(expected.cols[3].w, col3wx, col3wy, col3wz,      \
                              col3ww);                                         \
                                                                               \
  } while (void(0), 0)
#endif  // OZZ_OZZ_BASE_MATHS_GTEST_MATH_HELPER_H_
