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

#ifndef OZZ_OZZ_BASE_MATHS_SIMD_MATH_H_
#define OZZ_OZZ_BASE_MATHS_SIMD_MATH_H_

#include "internal/simd_math_config.h"
#include "../platform.h"

namespace ozz {
namespace math {

// Returns SIMDimplementation name has decided at library build time.
const char* SimdImplementationName();

namespace simd_float4 {
// Returns a SimdFloat4 vector with all components set to 0.
OZZ_INLINE SimdFloat4 zero();

// Returns a SimdFloat4 vector with all components set to 1.
OZZ_INLINE SimdFloat4 one();

// Returns a SimdFloat4 vector with the x component set to 1 and all the others
// to 0.
OZZ_INLINE SimdFloat4 x_axis();

// Returns a SimdFloat4 vector with the y component set to 1 and all the others
// to 0.
OZZ_INLINE SimdFloat4 y_axis();

// Returns a SimdFloat4 vector with the z component set to 1 and all the others
// to 0.
OZZ_INLINE SimdFloat4 z_axis();

// Returns a SimdFloat4 vector with the w component set to 1 and all the others
// to 0.
OZZ_INLINE SimdFloat4 w_axis();

// Loads _x, _y, _z, _w to the returned vector.
// r.x = _x
// r.y = _y
// r.z = _z
// r.w = _w
OZZ_INLINE SimdFloat4 Load(float _x, float _y, float _z, float _w);

// Loads _x to the x component of the returned vector, and sets y, z and w to 0.
// r.x = _x
// r.y = 0
// r.z = 0
// r.w = 0
OZZ_INLINE SimdFloat4 LoadX(float _x);

// Loads _x to the all the components of the returned vector.
// r.x = _x
// r.y = _x
// r.z = _x
// r.w = _x
OZZ_INLINE SimdFloat4 Load1(float _x);

// Loads the 4 values of _f to the returned vector.
// _f must be aligned to 16 bytes.
// r.x = _f[0]
// r.y = _f[1]
// r.z = _f[2]
// r.w = _f[3]
OZZ_INLINE SimdFloat4 LoadPtr(const float* _f);

// Loads the 4 values of _f to the returned vector.
// _f must be aligned to 4 bytes.
// r.x = _f[0]
// r.y = _f[1]
// r.z = _f[2]
// r.w = _f[3]
OZZ_INLINE SimdFloat4 LoadPtrU(const float* _f);

// Loads _f[0] to the x component of the returned vector, and sets y, z and w
// to 0.
// _f must be aligned to 4 bytes.
// r.x = _f[0]
// r.y = 0
// r.z = 0
// r.w = 0
OZZ_INLINE SimdFloat4 LoadXPtrU(const float* _f);

// Loads _f[0] to all the components of the returned vector.
// _f must be aligned to 4 bytes.
// r.x = _f[0]
// r.y = _f[0]
// r.z = _f[0]
// r.w = _f[0]
OZZ_INLINE SimdFloat4 Load1PtrU(const float* _f);

// Loads the 2 first value of _f to the x and y components of the returned
// vector. The remaining components are set to 0.
// _f must be aligned to 4 bytes.
// r.x = _f[0]
// r.y = _f[1]
// r.z = 0
// r.w = 0
OZZ_INLINE SimdFloat4 Load2PtrU(const float* _f);

// Loads the 3 first value of _f to the x, y and z components of the returned
// vector. The remaining components are set to 0.
// _f must be aligned to 4 bytes.
// r.x = _f[0]
// r.y = _f[1]
// r.z = _f[2]
// r.w = 0
OZZ_INLINE SimdFloat4 Load3PtrU(const float* _f);

// Convert from integer to float.
OZZ_INLINE SimdFloat4 FromInt(_SimdInt4 _i);
}  // namespace simd_float4

// Returns the x component of _v as a float.
OZZ_INLINE float GetX(_SimdFloat4 _v);

// Returns the y component of _v as a float.
OZZ_INLINE float GetY(_SimdFloat4 _v);

// Returns the z component of _v as a float.
OZZ_INLINE float GetZ(_SimdFloat4 _v);

// Returns the w component of _v as a float.
OZZ_INLINE float GetW(_SimdFloat4 _v);

// Returns _v with the x component set to _f.
OZZ_INLINE SimdFloat4 SetX(_SimdFloat4 _v, float _f);

// Returns _v with the y component set to _f.
OZZ_INLINE SimdFloat4 SetY(_SimdFloat4 _v, float _f);

// Returns _v with the z component set to _f.
OZZ_INLINE SimdFloat4 SetZ(_SimdFloat4 _v, float _f);

// Returns _v with the w component set to _f.
OZZ_INLINE SimdFloat4 SetW(_SimdFloat4 _v, float _f);

// Returns _v with the _i th component set to _f.
// _i must be in range [0,3]
OZZ_INLINE SimdFloat4 SetI(_SimdFloat4 _v, int _i, float _f);

// Stores the 4 components of _v to the four first floats of _f.
// _f must be aligned to 16 bytes.
// _f[0] = _v.x
// _f[1] = _v.y
// _f[2] = _v.z
// _f[3] = _v.w
OZZ_INLINE void StorePtr(_SimdFloat4 _v, float* _f);

// Stores the x component of _v to the first float of _f.
// _f must be aligned to 16 bytes.
// _f[0] = _v.x
OZZ_INLINE void Store1Ptr(_SimdFloat4 _v, float* _f);

// Stores x and y components of _v to the two first floats of _f.
// _f must be aligned to 16 bytes.
// _f[0] = _v.x
// _f[1] = _v.y
OZZ_INLINE void Store2Ptr(_SimdFloat4 _v, float* _f);

// Stores x, y and z components of _v to the three first floats of _f.
// _f must be aligned to 16 bytes.
// _f[0] = _v.x
// _f[1] = _v.y
// _f[2] = _v.z
OZZ_INLINE void Store3Ptr(_SimdFloat4 _v, float* _f);

// Stores the 4 components of _v to the four first floats of _f.
// _f must be aligned to 4 bytes.
// _f[0] = _v.x
// _f[1] = _v.y
// _f[2] = _v.z
// _f[3] = _v.w
OZZ_INLINE void StorePtrU(_SimdFloat4 _v, float* _f);

// Stores the x component of _v to the first float of _f.
// _f must be aligned to 4 bytes.
// _f[0] = _v.x
OZZ_INLINE void Store1PtrU(_SimdFloat4 _v, float* _f);

// Stores x and y components of _v to the two first floats of _f.
// _f must be aligned to 4 bytes.
// _f[0] = _v.x
// _f[1] = _v.y
OZZ_INLINE void Store2PtrU(_SimdFloat4 _v, float* _f);

// Stores x, y and z components of _v to the three first floats of _f.
// _f must be aligned to 4 bytes.
// _f[0] = _v.x
// _f[1] = _v.y
// _f[2] = _v.z
OZZ_INLINE void Store3PtrU(_SimdFloat4 _v, float* _f);

// Replicates x of _a to all the components of the returned vector.
OZZ_INLINE SimdFloat4 SplatX(_SimdFloat4 _v);

// Replicates y of _a to all the components of the returned vector.
OZZ_INLINE SimdFloat4 SplatY(_SimdFloat4 _v);

// Replicates z of _a to all the components of the returned vector.
OZZ_INLINE SimdFloat4 SplatZ(_SimdFloat4 _v);

// Replicates w of _a to all the components of the returned vector.
OZZ_INLINE SimdFloat4 SplatW(_SimdFloat4 _v);

// Transposes the x components of the 4 SimdFloat4 of _in into the 1
// SimdFloat4 of _out.
OZZ_INLINE void Transpose4x1(const SimdFloat4 _in[4], SimdFloat4 _out[1]);

// Transposes x, y, z and w components of _in to the x components of _out.
// Remaining y, z and w are set to 0.
OZZ_INLINE void Transpose1x4(const SimdFloat4 _in[1], SimdFloat4 _out[4]);

// Transposes the 1 SimdFloat4 of _in into the x components of the 4
// SimdFloat4 of _out. Remaining y, z and w are set to 0.
OZZ_INLINE void Transpose2x4(const SimdFloat4 _in[2], SimdFloat4 _out[4]);

// Transposes the x and y components of the 4 SimdFloat4 of _in into the 2
// SimdFloat4 of _out.
OZZ_INLINE void Transpose4x2(const SimdFloat4 _in[4], SimdFloat4 _out[2]);

// Transposes the 2 SimdFloat4 of _in into the x and y components of the 4
// SimdFloat4 of _out. Remaining z and w are set to 0.
OZZ_INLINE void Transpose2x4(const SimdFloat4 _in[2], SimdFloat4 _out[4]);

// Transposes the x, y and z components of the 4 SimdFloat4 of _in into the 3
// SimdFloat4 of _out.
OZZ_INLINE void Transpose4x3(const SimdFloat4 _in[4], SimdFloat4 _out[3]);

// Transposes the 3 SimdFloat4 of _in into the x, y and z components of the 4
// SimdFloat4 of _out. Remaining w are set to 0.
OZZ_INLINE void Transpose3x4(const SimdFloat4 _in[3], SimdFloat4 _out[4]);

// Transposes the 4 SimdFloat4 of _in into the 4 SimdFloat4 of _out.
OZZ_INLINE void Transpose4x4(const SimdFloat4 _in[4], SimdFloat4 _out[4]);

// Transposes the 16 SimdFloat4 of _in into the 16 SimdFloat4 of _out.
OZZ_INLINE void Transpose16x16(const SimdFloat4 _in[16], SimdFloat4 _out[16]);

// Multiplies _a and _b, then adds _addend.
// v = (_a * _b) + _addend
OZZ_INLINE SimdFloat4 MAdd(_SimdFloat4 _a, _SimdFloat4 _b, _SimdFloat4 _addend);

// Divides the x component of _a by the _x component of _b and stores it in the
// x component of the returned vector. y, z, w of the returned vector are the
// same as _a respective components.
// r.x = _a.x / _b.x
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 DivX(_SimdFloat4 _a, _SimdFloat4 _b);

// Computes the (horizontal) addition of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 HAdd2(_SimdFloat4 _v);

// Computes the (horizontal) addition of x, y and z components of _v. The result
// is stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y + _a.z
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 HAdd3(_SimdFloat4 _v);

// Computes the (horizontal) addition of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y + _a.z + _a.w
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 HAdd4(_SimdFloat4 _v);

// Computes the dot product of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x * _a.x + _a.y * _a.y
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 Dot2(_SimdFloat4 _a, _SimdFloat4 _b);

// Computes the dot product of x, y and z components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _a.
// r.x = _a.x * _a.x + _a.y * _a.y + _a.z * _a.z
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 Dot3(_SimdFloat4 _a, _SimdFloat4 _b);

// Computes the dot product of x, y, z and w components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _a.
// r.x = _a.x * _a.x + _a.y * _a.y + _a.z * _a.z + _a.w * _a.w
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdFloat4 Dot4(_SimdFloat4 _a, _SimdFloat4 _b);

// Computes the cross product of x, y and z components of _v. The result is
// stored in the x, y and z components of the returned value. w of the returned
// vector is set to 0.
// r.x = _a.y * _b.z - _a.z * _b.y
// r.y = _a.z * _b.x - _a.x * _b.z
// r.z = _a.x * _b.y - _a.y * _b.x
// r.w = _a.w
OZZ_INLINE SimdFloat4 Cross3(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns the per component estimated reciprocal of _v.
OZZ_INLINE SimdFloat4 RcpEst(_SimdFloat4 _v);

// Returns the per component estimated reciprocal of _v, where approximation is
// improved with one more new Newton-Raphson step.
OZZ_INLINE SimdFloat4 RcpEstNR(_SimdFloat4 _v);

// Returns the estimated reciprocal of the x component of _v and stores it in
// the x component of the returned vector. y, z, w of the returned vector are
// the same as their respective components in _v.
OZZ_INLINE SimdFloat4 RcpEstX(_SimdFloat4 _v);

// Returns the per component square root of _v.
OZZ_INLINE SimdFloat4 Sqrt(_SimdFloat4 _v);

// Returns the square root of the x component of _v and stores it in the x
// component of the returned vector. y, z, w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 SqrtX(_SimdFloat4 _v);

// Returns the per component estimated reciprocal square root of _v.
OZZ_INLINE SimdFloat4 RSqrtEst(_SimdFloat4 _v);

// Returns the per component estimated reciprocal square root of _v, where
// approximation is improved with one more new Newton-Raphson step.
OZZ_INLINE SimdFloat4 RSqrtEstNR(_SimdFloat4 _v);

// Returns the estimated reciprocal square root of the x component of _v and
// stores it in the x component of the returned vector. y, z, w of the returned
// vector are the same as their respective components in _v.
OZZ_INLINE SimdFloat4 RSqrtEstX(_SimdFloat4 _v);

// Returns the per element absolute value of _v.
OZZ_INLINE SimdFloat4 Abs(_SimdFloat4 _v);

// Returns the sign bit of _v.
OZZ_INLINE SimdInt4 Sign(_SimdFloat4 _v);

// Returns the per component minimum of _a and _b.
OZZ_INLINE SimdFloat4 Min(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns the per component maximum of _a and _b.
OZZ_INLINE SimdFloat4 Max(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns the per component minimum of _v and 0.
OZZ_INLINE SimdFloat4 Min(_SimdFloat4 _v);

// Returns the per component maximum of _v and 0.
OZZ_INLINE SimdFloat4 Max0(_SimdFloat4 _v);

// Clamps each element of _x between _a and _b.
// Result is unknown if _a is not less or equal to _b.
OZZ_INLINE SimdFloat4 Clamp(_SimdFloat4 _a, _SimdFloat4 _v, _SimdFloat4 _b);

// Computes the length of the components x and y of _v, and stores it in the x
// component of the returned vector. y, z, w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 Length2(_SimdFloat4 _v);

// Computes the length of the components x, y and z of _v, and stores it in the
// x component of the returned vector. y, z, w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 Length3(_SimdFloat4 _v);

// Computes the length of _v, and stores it in the x component of the returned
// vector. y, z, w of the returned vector are the same as their respective
// components in _v.
OZZ_INLINE SimdFloat4 Length4(_SimdFloat4 _v);

// Computes the square length of the components x and y of _v, and stores it
// in the x component of the returned vector. y, z, w of the returned vector are
// the same as their respective components in _v.
OZZ_INLINE SimdFloat4 Length2Sqr(_SimdFloat4 _v);

// Computes the square length of the components x, y and z of _v, and stores it
// in the x component of the returned vector. y, z, w of the returned vector are
// the same as their respective components in _v.
OZZ_INLINE SimdFloat4 Length3Sqr(_SimdFloat4 _v);

// Computes the square length of the components x, y, z and w of _v, and stores
// it in the x component of the returned vector. y, z, w of the returned vector
// are the same as their respective components in _v.
OZZ_INLINE SimdFloat4 Length4Sqr(_SimdFloat4 _v);

// Returns the normalized vector of the components x and y of _v, and stores
// it in the x and y components of the returned vector. z and w of the returned
// vector are the same as their respective components in _v.
OZZ_INLINE SimdFloat4 Normalize2(_SimdFloat4 _v);

// Returns the normalized vector of the components x, y and z of _v, and stores
// it in the x, y and z components of the returned vector. w of the returned
// vector is the same as its respective component in _v.
OZZ_INLINE SimdFloat4 Normalize3(_SimdFloat4 _v);

// Returns the normalized vector _v.
OZZ_INLINE SimdFloat4 Normalize4(_SimdFloat4 _v);

// Returns the estimated normalized vector of the components x and y of _v, and
// stores it in the x and y components of the returned vector. z and w of the
// returned vector are the same as their respective components in _v.
OZZ_INLINE SimdFloat4 NormalizeEst2(_SimdFloat4 _v);

// Returns the estimated normalized vector of the components x, y and z of _v,
// and stores it in the x, y and z components of the returned vector. w of the
// returned vector is the same as its respective component in _v.
OZZ_INLINE SimdFloat4 NormalizeEst3(_SimdFloat4 _v);

// Returns the estimated normalized vector _v.
OZZ_INLINE SimdFloat4 NormalizeEst4(_SimdFloat4 _v);

// Tests if the components x and y of _v forms a normalized vector.
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsNormalized2(_SimdFloat4 _v);

// Tests if the components x, y and z of _v forms a normalized vector.
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsNormalized3(_SimdFloat4 _v);

// Tests if the _v is a normalized vector.
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsNormalized4(_SimdFloat4 _v);

// Tests if the components x and y of _v forms a normalized vector.
// Uses the estimated normalization coefficient, that matches estimated math
// functions (RecpEst, MormalizeEst...).
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsNormalizedEst2(_SimdFloat4 _v);

// Tests if the components x, y and z of _v forms a normalized vector.
// Uses the estimated normalization coefficient, that matches estimated math
// functions (RecpEst, MormalizeEst...).
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsNormalizedEst3(_SimdFloat4 _v);

// Tests if the _v is a normalized vector.
// Uses the estimated normalization coefficient, that matches estimated math
// functions (RecpEst, MormalizeEst...).
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsNormalizedEst4(_SimdFloat4 _v);

// Returns the normalized vector of the components x and y of _v if it is
// normalizable, otherwise returns _safe. z and w of the returned vector are
// the same as their respective components in _v.
OZZ_INLINE SimdFloat4 NormalizeSafe2(_SimdFloat4 _v, _SimdFloat4 _safe);

// Returns the normalized vector of the components x, y, z and w of _v if it is
// normalizable, otherwise returns _safe. w of the returned vector is the same
// as its respective components in _v.
OZZ_INLINE SimdFloat4 NormalizeSafe3(_SimdFloat4 _v, _SimdFloat4 _safe);

// Returns the normalized vector _v if it is normalizable, otherwise returns
// _safe.
OZZ_INLINE SimdFloat4 NormalizeSafe4(_SimdFloat4 _v, _SimdFloat4 _safe);

// Returns the estimated normalized vector of the components x and y of _v if it
// is normalizable, otherwise returns _safe. z and w of the returned vector are
// the same as their respective components in _v.
OZZ_INLINE SimdFloat4 NormalizeSafeEst2(_SimdFloat4 _v, _SimdFloat4 _safe);

// Returns the estimated normalized vector of the components x, y, z and w of _v
// if it is normalizable, otherwise returns _safe. w of the returned vector is
// the same as its respective components in _v.
OZZ_INLINE SimdFloat4 NormalizeSafeEst3(_SimdFloat4 _v, _SimdFloat4 _safe);

// Returns the estimated normalized vector _v if it is normalizable, otherwise
// returns _safe.
OZZ_INLINE SimdFloat4 NormalizeSafeEst4(_SimdFloat4 _v, _SimdFloat4 _safe);

// Computes the per element linear interpolation of _a and _b, where _alpha is
// not bound to range [0,1].
OZZ_INLINE SimdFloat4 Lerp(_SimdFloat4 _a, _SimdFloat4 _b, _SimdFloat4 _alpha);

// Computes the per element cosine of _v.
OZZ_INLINE SimdFloat4 Cos(_SimdFloat4 _v);

// Computes the cosine of the x component of _v and stores it in the x
// component of the returned vector. y, z and w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 CosX(_SimdFloat4 _v);

// Computes the per element arccosine of _v.
OZZ_INLINE SimdFloat4 ACos(_SimdFloat4 _v);

// Computes the arccosine of the x component of _v and stores it in the x
// component of the returned vector. y, z and w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 ACosX(_SimdFloat4 _v);

// Computes the per element sines of _v.
OZZ_INLINE SimdFloat4 Sin(_SimdFloat4 _v);

// Computes the sines of the x component of _v and stores it in the x
// component of the returned vector. y, z and w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 SinX(_SimdFloat4 _v);

// Computes the per element arcsine of _v.
OZZ_INLINE SimdFloat4 ASin(_SimdFloat4 _v);

// Computes the arcsine of the x component of _v and stores it in the x
// component of the returned vector. y, z and w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 ASinX(_SimdFloat4 _v);

// Computes the per element tangent of _v.
OZZ_INLINE SimdFloat4 Tan(_SimdFloat4 _v);

// Computes the tangent of the x component of _v and stores it in the x
// component of the returned vector. y, z and w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 TanX(_SimdFloat4 _v);

// Computes the per element arctangent of _v.
OZZ_INLINE SimdFloat4 ATan(_SimdFloat4 _v);

// Computes the arctangent of the x component of _v and stores it in the x
// component of the returned vector. y, z and w of the returned vector are the
// same as their respective components in _v.
OZZ_INLINE SimdFloat4 ATanX(_SimdFloat4 _v);

// Returns Per bit selection of vectors _true and _false according to _b.
// _v[0...127] = _b[0...127] ? _true[0...127]:_false[0...127]
OZZ_INLINE SimdFloat4 Select(_SimdInt4 _b, _SimdFloat4 _true,
                             _SimdFloat4 _false);

// Per element "equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpEq(_SimdFloat4 _a, _SimdFloat4 _b);

// Per element "not equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpNe(_SimdFloat4 _a, _SimdFloat4 _b);

// Per element "less than" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpLt(_SimdFloat4 _a, _SimdFloat4 _b);

// Per element "less than or equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpLe(_SimdFloat4 _a, _SimdFloat4 _b);

// Per element "greater than" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpGt(_SimdFloat4 _a, _SimdFloat4 _b);

// Per element "greater than or equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpGe(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns per element binary and operation of _a and _b.
// _v[0...127] = _a[0...127] & _b[0...127]
OZZ_INLINE SimdFloat4 And(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns per element binary or operation of _a and _b.
// _v[0...127] = _a[0...127] | _b[0...127]
OZZ_INLINE SimdFloat4 Or(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns per element binary logical xor operation of _a and _b.
// _v[0...127] = _a[0...127] ^ _b[0...127]
OZZ_INLINE SimdFloat4 Xor(_SimdFloat4 _a, _SimdFloat4 _b);

// Returns per element binary and operation of _a and _b.
// _v[0...127] = _a[0...127] & _b[0...127]
OZZ_INLINE SimdFloat4 And(_SimdFloat4 _a, _SimdInt4 _b);

// Returns per element binary or operation of _a and _b.
// _v[0...127] = _a[0...127] | _b[0...127]
OZZ_INLINE SimdFloat4 Or(_SimdFloat4 _a, _SimdInt4 _b);

// Returns per element binary logical xor operation of _a and _b.
// _v[0...127] = _a[0...127] ^ _b[0...127]
OZZ_INLINE SimdFloat4 Xor(_SimdFloat4 _a, _SimdInt4 _b);

namespace simd_int4 {
// Returns a SimdInt4 vector with all components set to 0.
OZZ_INLINE SimdInt4 zero();

// Returns a SimdInt4 vector with all components set to 1.
OZZ_INLINE SimdInt4 one();

// Returns a SimdInt4 vector with the x component set to 1 and all the others
// to 0.
OZZ_INLINE SimdInt4 x_axis();

// Returns a SimdInt4 vector with the y component set to 1 and all the others
// to 0.
OZZ_INLINE SimdInt4 y_axis();

// Returns a SimdInt4 vector with the z component set to 1 and all the others
// to 0.
OZZ_INLINE SimdInt4 z_axis();

// Returns a SimdInt4 vector with the w component set to 1 and all the others
// to 0.
OZZ_INLINE SimdInt4 w_axis();

// Returns a SimdInt4 vector with all components set to true (0xffffffff).
OZZ_INLINE SimdInt4 all_true();

// Returns a SimdInt4 vector with all components set to false (0).
OZZ_INLINE SimdInt4 all_false();

// Returns a SimdInt4 vector with sign bits set to 1.
OZZ_INLINE SimdInt4 mask_sign();

// Returns a SimdInt4 vector with all bits set to 1 except sign.
OZZ_INLINE SimdInt4 mask_not_sign();

// Returns a SimdInt4 vector with all bits set to 1.
OZZ_INLINE SimdInt4 mask_ffff();

// Returns a SimdInt4 vector with all bits set to 0.
OZZ_INLINE SimdInt4 mask_0000();

// Returns a SimdInt4 vector with all the bits of the x, y, z components set to
// 1, while z is set to 0.
OZZ_INLINE SimdInt4 mask_fff0();

// Returns a SimdInt4 vector with all the bits of the x component set to 1,
// while the others are set to 0.
OZZ_INLINE SimdInt4 mask_f000();

// Returns a SimdInt4 vector with all the bits of the y component set to 1,
// while the others are set to 0.
OZZ_INLINE SimdInt4 mask_0f00();

// Returns a SimdInt4 vector with all the bits of the z component set to 1,
// while the others are set to 0.
OZZ_INLINE SimdInt4 mask_00f0();

// Returns a SimdInt4 vector with all the bits of the w component set to 1,
// while the others are set to 0.
OZZ_INLINE SimdInt4 mask_000f();

// Loads _x, _y, _z, _w to the returned vector.
// r.x = _x
// r.y = _y
// r.z = _z
// r.w = _w
OZZ_INLINE SimdInt4 Load(int _x, int _y, int _z, int _w);

// Loads _x, _y, _z, _w to the returned vector using the following conversion
// rule.
// r.x = _x ? 0xffffffff:0
// r.y = _y ? 0xffffffff:0
// r.z = _z ? 0xffffffff:0
// r.w = _w ? 0xffffffff:0
OZZ_INLINE SimdInt4 Load(bool _x, bool _y, bool _z, bool _w);

// Loads _x to the x component of the returned vector using the following
// conversion rule, and sets y, z and w to 0.
// r.x = _x ? 0xffffffff:0
// r.y = 0
// r.z = 0
// r.w = 0
OZZ_INLINE SimdInt4 LoadX(bool _x);

// Loads _x to the all the components of the returned vector using the following
// conversion rule.
// r.x = _x ? 0xffffffff:0
// r.y = _x ? 0xffffffff:0
// r.z = _x ? 0xffffffff:0
// r.w = _x ? 0xffffffff:0
OZZ_INLINE SimdInt4 Load1(bool _x);

// Loads the 4 values of _f to the returned vector.
// _i must be aligned to 16 bytes.
// r.x = _i[0]
// r.y = _i[1]
// r.z = _i[2]
// r.w = _i[3]
OZZ_INLINE SimdInt4 LoadPtr(const int* _i);

// Loads _i[0] to the x component of the returned vector, and sets y, z and w
// to 0.
// _i must be aligned to 16 bytes.
// r.x = _i[0]
// r.y = 0
// r.z = 0
// r.w = 0
OZZ_INLINE SimdInt4 LoadXPtr(const int* _i);

// Loads _i[0] to all the components of the returned vector.
// _i must be aligned to 16 bytes.
// r.x = _i[0]
// r.y = _i[0]
// r.z = _i[0]
// r.w = _i[0]
OZZ_INLINE SimdInt4 Load1Ptr(const int* _i);

// Loads the 2 first value of _i to the x and y components of the returned
// vector. The remaining components are set to 0.
// _f must be aligned to 4 bytes.
// r.x = _i[0]
// r.y = _i[1]
// r.z = 0
// r.w = 0
OZZ_INLINE SimdInt4 Load2Ptr(const int* _i);

// Loads the 3 first value of _i to the x, y and z components of the returned
// vector. The remaining components are set to 0.
// _f must be aligned to 16 bytes.
// r.x = _i[0]
// r.y = _i[1]
// r.z = _i[2]
// r.w = 0
OZZ_INLINE SimdInt4 Load3Ptr(const int* _i);

// Loads the 4 values of _f to the returned vector.
// _i must be aligned to 16 bytes.
// r.x = _i[0]
// r.y = _i[1]
// r.z = _i[2]
// r.w = _i[3]
OZZ_INLINE SimdInt4 LoadPtrU(const int* _i);

// Loads _i[0] to the x component of the returned vector, and sets y, z and w
// to 0.
// _f must be aligned to 4 bytes.
// r.x = _i[0]
// r.y = 0
// r.z = 0
// r.w = 0
OZZ_INLINE SimdInt4 LoadXPtrU(const int* _i);

// Loads the 4 values of _i to the returned vector.
// _i must be aligned to 4 bytes.
// r.x = _i[0]
// r.y = _i[0]
// r.z = _i[0]
// r.w = _i[0]
OZZ_INLINE SimdInt4 Load1PtrU(const int* _i);

// Loads the 2 first value of _i to the x and y components of the returned
// vector. The remaining components are set to 0.
// _f must be aligned to 4 bytes.
// r.x = _i[0]
// r.y = _i[1]
// r.z = 0
// r.w = 0
OZZ_INLINE SimdInt4 Load2PtrU(const int* _i);

// Loads the 3 first value of _i to the x, y and z components of the returned
// vector. The remaining components are set to 0.
// _f must be aligned to 4 bytes.
// r.x = _i[0]
// r.y = _i[1]
// r.z = _i[2]
// r.w = 0
OZZ_INLINE SimdInt4 Load3PtrU(const int* _i);

// Convert from float to integer by rounding the nearest value.
OZZ_INLINE SimdInt4 FromFloatRound(_SimdFloat4 _f);

// Convert from float to integer by truncating.
OZZ_INLINE SimdInt4 FromFloatTrunc(_SimdFloat4 _f);
}  // namespace simd_int4

// Returns the x component of _v as an integer.
OZZ_INLINE int GetX(_SimdInt4 _v);

// Returns the y component of _v as a integer.
OZZ_INLINE int GetY(_SimdInt4 _v);

// Returns the z component of _v as a integer.
OZZ_INLINE int GetZ(_SimdInt4 _v);

// Returns the w component of _v as a integer.
OZZ_INLINE int GetW(_SimdInt4 _v);

// Returns _v with the x component set to _i.
OZZ_INLINE SimdInt4 SetX(_SimdInt4 _v, int _i);

// Returns _v with the y component set to _i.
OZZ_INLINE SimdInt4 SetY(_SimdInt4 _v, int _i);

// Returns _v with the z component set to _i.
OZZ_INLINE SimdInt4 SetZ(_SimdInt4 _v, int _i);

// Returns _v with the w component set to _i.
OZZ_INLINE SimdInt4 SetW(_SimdInt4 _v, int _i);

// Returns _v with the _ith component set to _i.
// _i must be in range [0,3]
OZZ_INLINE SimdInt4 SetI(_SimdInt4 _v, int _ith, int _i);

// Stores the 4 components of _v to the four first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
// _i[3] = _v.w
OZZ_INLINE void StorePtr(_SimdInt4 _v, int* _i);

// Stores the x component of _v to the first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
OZZ_INLINE void Store1Ptr(_SimdInt4 _v, int* _i);

// Stores x and y components of _v to the two first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
OZZ_INLINE void Store2Ptr(_SimdInt4 _v, int* _i);

// Stores x, y and z components of _v to the three first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
OZZ_INLINE void Store3Ptr(_SimdInt4 _v, int* _i);

// Stores the 4 components of _v to the four first integers of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
// _i[3] = _v.w
OZZ_INLINE void StorePtrU(_SimdInt4 _v, int* _i);

// Stores the x component of _v to the first float of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
OZZ_INLINE void Store1PtrU(_SimdInt4 _v, int* _i);

// Stores x and y components of _v to the two first integers of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
OZZ_INLINE void Store2PtrU(_SimdInt4 _v, int* _i);

// Stores x, y and z components of _v to the three first integers of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
OZZ_INLINE void Store3PtrU(_SimdInt4 _v, int* _i);

// Replicates x of _a to all the components of the returned vector.
OZZ_INLINE SimdInt4 SplatX(_SimdInt4 _v);

// Replicates y of _a to all the components of the returned vector.
OZZ_INLINE SimdInt4 SplatY(_SimdInt4 _v);

// Replicates z of _a to all the components of the returned vector.
OZZ_INLINE SimdInt4 SplatZ(_SimdInt4 _v);

// Replicates w of _a to all the components of the returned vector.
OZZ_INLINE SimdInt4 SplatW(_SimdInt4 _v);

// Creates a 4-bit mask from the most significant bits of each component of _v.
// i := sign(a3)<<3 | sign(a2)<<2 | sign(a1)<<1 | sign(a0)
OZZ_INLINE int MoveMask(_SimdInt4 _v);

// Returns true if all the components of _v are not 0.
OZZ_INLINE bool AreAllTrue(_SimdInt4 _v);

// Returns true if x, y and z components of _v are not 0.
OZZ_INLINE bool AreAllTrue3(_SimdInt4 _v);

// Returns true if x and y components of _v are not 0.
OZZ_INLINE bool AreAllTrue2(_SimdInt4 _v);

// Returns true if x component of _v is not 0.
OZZ_INLINE bool AreAllTrue1(_SimdInt4 _v);

// Returns true if all the components of _v are 0.
OZZ_INLINE bool AreAllFalse(_SimdInt4 _v);

// Returns true if x, y and z components of _v are 0.
OZZ_INLINE bool AreAllFalse3(_SimdInt4 _v);

// Returns true if x and y components of _v are 0.
OZZ_INLINE bool AreAllFalse2(_SimdInt4 _v);

// Returns true if x component of _v is 0.
OZZ_INLINE bool AreAllFalse1(_SimdInt4 _v);

// Computes the (horizontal) addition of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdInt4 HAdd2(_SimdInt4 _v);

// Computes the (horizontal) addition of x, y and z components of _v. The result
// is stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y + _a.z
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdInt4 HAdd3(_SimdInt4 _v);

// Computes the (horizontal) addition of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y + _a.z + _a.w
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
OZZ_INLINE SimdInt4 HAdd4(_SimdInt4 _v);

// Returns the per element absolute value of _v.
OZZ_INLINE SimdInt4 Abs(_SimdInt4 _v);

// Returns the sign bit of _v.
OZZ_INLINE SimdInt4 Sign(_SimdInt4 _v);

// Returns the per component minimum of _a and _b.
OZZ_INLINE SimdInt4 Min(_SimdInt4 _a, _SimdInt4 _b);

// Returns the per component maximum of _a and _b.
OZZ_INLINE SimdInt4 Max(_SimdInt4 _a, _SimdInt4 _b);

// Returns the per component minimum of _v and 0.
OZZ_INLINE SimdInt4 Min0(_SimdInt4 _v);

// Returns the per component maximum of _v and 0.
OZZ_INLINE SimdInt4 Max0(_SimdInt4 _v);

// Clamps each element of _x between _a and _b.
// Result is unknown if _a is not less or equal to _b.
OZZ_INLINE SimdInt4 Clamp(_SimdInt4 _a, _SimdInt4 _v, _SimdInt4 _b);

// Returns Per bit selection of vectors _true and _false according to _b.
// _v[0...127] = _b[0...127] ? _true[0...127]:_false[0...127]
OZZ_INLINE SimdInt4 Select(_SimdInt4 _b, _SimdInt4 _true, _SimdInt4 _false);

// Returns per element binary and operation of _a and _b.
// _v[0...127] = _a[0...127] & _b[0...127]
OZZ_INLINE SimdInt4 And(_SimdInt4 _a, _SimdInt4 _b);

// Returns per element binary or operation of _a and _b.
// _v[0...127] = _a[0...127] | _b[0...127]
OZZ_INLINE SimdInt4 Or(_SimdInt4 _a, _SimdInt4 _b);

// Returns per element binary logical xor operation of _a and _b.
// _v[0...127] = _a[0...127] ^ _b[0...127]
OZZ_INLINE SimdInt4 Xor(_SimdInt4 _a, _SimdInt4 _b);

// Returns per element binary complement of _v.
// _v[0...127] = ~_b[0...127]
OZZ_INLINE SimdInt4 Not(_SimdInt4 _v);

// Shifts the 4 signed or unsigned 32-bit integers in a left by count _bits
// while shifting in zeros.
OZZ_INLINE SimdInt4 ShiftL(_SimdInt4 _v, int _bits);

// Shifts the 4 signed 32-bit integers in a right by count bits while shifting
// in the sign bit.
OZZ_INLINE SimdInt4 ShiftR(_SimdInt4 _v, int _bits);

// Shifts the 4 signed or unsigned 32-bit integers in a right by count bits
// while shifting in zeros.
OZZ_INLINE SimdInt4 ShiftRu(_SimdInt4 _v, int _bits);

// Per element "equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpEq(_SimdInt4 _a, _SimdInt4 _b);

// Per element "not equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpNe(_SimdInt4 _a, _SimdInt4 _b);

// Per element "less than" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpLt(_SimdInt4 _a, _SimdInt4 _b);

// Per element "less than or equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpLe(_SimdInt4 _a, _SimdInt4 _b);

// Per element "greater than" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpGt(_SimdInt4 _a, _SimdInt4 _b);

// Per element "greater than or equal" comparison of _a and _b.
OZZ_INLINE SimdInt4 CmpGe(_SimdInt4 _a, _SimdInt4 _b);

// Declare the 4x4 matrix type. Uses the column major convention where the
// matrix-times-vector is written v'=Mv:
// [ m.cols[0].x m.cols[1].x m.cols[2].x m.cols[3].x ]   {v.x}
// | m.cols[0].y m.cols[1].y m.cols[2].y m.cols[3].y | * {v.y}
// | m.cols[0].z m.cols[1].y m.cols[2].y m.cols[3].y |   {v.z}
// [ m.cols[0].w m.cols[1].w m.cols[2].w m.cols[3].w ]   {v.1}
struct Float4x4 {
  // Matrix columns.
  SimdFloat4 cols[4];

  // Returns the identity matrix.
  static OZZ_INLINE Float4x4 identity();

  // Returns a translation matrix.
  // _v.w is ignored.
  static OZZ_INLINE Float4x4 Translation(_SimdFloat4 _v);

  // Returns a scaling matrix that scales along _v.
  // _v.w is ignored.
  static OZZ_INLINE Float4x4 Scaling(_SimdFloat4 _v);

  // Returns the rotation matrix built from Euler angles defined by x, y and z
  // components of _v. Euler angles are ordered Heading, Elevation and Bank, or
  // Yaw, Pitch and Roll. _v.w is ignored.
  static OZZ_INLINE Float4x4 FromEuler(_SimdFloat4 _v);

  // Returns the rotation matrix built from axis defined by x, y, z and angle w
  // of _v.
  static OZZ_INLINE Float4x4 FromAxisAngle(_SimdFloat4 _v);

  // Returns the rotation matrix built from quaternion defined by x, y, z and w
  // components of _v.
  static OZZ_INLINE Float4x4 FromQuaternion(_SimdFloat4 _v);

  // Returns the affine transformation matrix built from split translation,
  // rotation (quaternion) and scale.
  static OZZ_INLINE Float4x4 FromAffine(_SimdFloat4 _translation,
                                        _SimdFloat4 _quaternion,
                                        _SimdFloat4 _scale);
};

// Returns the transpose of matrix _m.
OZZ_INLINE Float4x4 Transpose(const Float4x4& _m);

// Returns the inverse of matrix _m.
OZZ_INLINE Float4x4 Invert(const Float4x4& _m);

// Translates matrix _m along the axis defined by _v components.
// _v.w is ignored.
OZZ_INLINE Float4x4 Translate(const Float4x4& _m, _SimdFloat4 _v);

// Scales matrix _m along each axis with x, y, z components of _v.
// _v.w is ignored.
OZZ_INLINE Float4x4 Scale(const Float4x4& _m, _SimdFloat4 _v);

// Multiply each column of matrix _m with vector _v.
OZZ_INLINE Float4x4 ColumnMultiply(const Float4x4& _m, _SimdFloat4 _v);

// Tests if each 3 column of upper 3x3 matrix of _m is a normal matrix.
// Returns the result in the x, y and z component of the returned vector. w is
// set to 0.
OZZ_INLINE SimdInt4 IsNormalized(const Float4x4& _m);

// Tests if each 3 column of upper 3x3 matrix of _m is a normal matrix.
// Uses the estimated tolerance
// Returns the result in the x, y and z component of the returned vector. w is
// set to 0.
OZZ_INLINE SimdInt4 IsNormalizedEst(const Float4x4& _m);

// Tests if the upper 3x3 matrix of _m is an orthogonal matrix.
// A matrix that contains a reflexion cannot be considered orthogonal.
// Returns the result in the x component of the returned vector. y, z and w are
// set to 0.
OZZ_INLINE SimdInt4 IsOrthogonal(const Float4x4& _m);

// Returns the quaternion that represent the rotation of matrix _m.
// _m must be normalized and orthogonal.
// the return quaternion is normalized.
OZZ_INLINE SimdFloat4 ToQuaternion(const Float4x4& _m);

// Decompose a general 3D transformation matrix _m into its scalar, rotational
// and translational components.
// Returns false if it was not possible to decompose the matrix. This would be
// because more than 1 of the 3 first column of _m are scaled to 0.
OZZ_INLINE bool ToAffine(const Float4x4& _m, SimdFloat4* _translation,
                         SimdFloat4* _quaternion, SimdFloat4* _scale);

// Computes the transformation of a Float4x4 matrix and a point _p.
// This is equivalent to multiplying a matrix by a SimdFloat4 with a w component
// of 1.
OZZ_INLINE ozz::math::SimdFloat4 TransformPoint(const ozz::math::Float4x4& _m,
                                                ozz::math::_SimdFloat4 _v);

// Computes the transformation of a Float4x4 matrix and a vector _v.
// This is equivalent to multiplying a matrix by a SimdFloat4 with a w component
// of 0.
OZZ_INLINE ozz::math::SimdFloat4 TransformVector(const ozz::math::Float4x4& _m,
                                                 ozz::math::_SimdFloat4 _v);
}  // namespace math
}  // namespace ozz

#if !defined(__GNUC__) || defined(OZZ_SIMD_REF)
// Returns per element addition of _a and _b.
OZZ_INLINE ozz::math::SimdFloat4 operator+(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b);

// Returns per element subtraction of _a and _b.
OZZ_INLINE ozz::math::SimdFloat4 operator-(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b);

// Returns per element negation of _v.
OZZ_INLINE ozz::math::SimdFloat4 operator-(ozz::math::_SimdFloat4 _v);

// Returns per element multiplication of _a and _b.
OZZ_INLINE ozz::math::SimdFloat4 operator*(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b);

// Returns per element division of _a and _b.
OZZ_INLINE ozz::math::SimdFloat4 operator/(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b);
#endif  // !defined(__GNUC__) || defined(OZZ_SIMD_REF)

// Computes the multiplication of matrix Float4x4 and vector _v.
OZZ_INLINE ozz::math::SimdFloat4 operator*(const ozz::math::Float4x4& _m,
                                           ozz::math::_SimdFloat4 _v);

// Computes the multiplication of two matrices _a and _b.
OZZ_INLINE ozz::math::Float4x4 operator*(const ozz::math::Float4x4& _a,
                                         const ozz::math::Float4x4& _b);

// Computes the per element addition of two matrices _a and _b.
OZZ_INLINE ozz::math::Float4x4 operator+(const ozz::math::Float4x4& _a,
                                         const ozz::math::Float4x4& _b);

// Computes the per element subtraction of two matrices _a and _b.
OZZ_INLINE ozz::math::Float4x4 operator-(const ozz::math::Float4x4& _a,
                                         const ozz::math::Float4x4& _b);

// Implement format conversions.
namespace ozz {
namespace math {
// Converts from a float to a half.
OZZ_INLINE uint16_t FloatToHalf(float _f);

// Converts from a half to a float.
OZZ_INLINE float HalfToFloat(uint16_t _h);

// Converts from a float to a half.
OZZ_INLINE SimdInt4 FloatToHalf(_SimdFloat4 _f);

// Converts from a half to a float.
OZZ_INLINE SimdFloat4 HalfToFloat(_SimdInt4 _h);
}  // namespace math
}  // namespace ozz

#if defined(OZZ_SIMD_SSEx)
#include "../maths/internal/simd_math_sse-inl.h"
#elif defined(OZZ_SIMD_REF)
#include "../maths/internal/simd_math_ref-inl.h"
#else
#error No simd_math implementation detected
#endif
#endif  // OZZ_OZZ_BASE_MATHS_SIMD_MATH_H_
