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

#ifndef OZZ_OZZ_BASE_MATHS_INTERNAL_SIMD_MATH_SSE_INL_H_
#define OZZ_OZZ_BASE_MATHS_INTERNAL_SIMD_MATH_SSE_INL_H_

// SIMD SSE2+ implementation, based on scalar floats.

#include <stdint.h>
#include <cassert>

// Temporarly needed while trigonometric functions aren't implemented.
#include <cmath>

#include "../math_constant.h"

namespace ozz {
namespace math {

namespace simd_float4 {

// clang-format off

// Internal macros.
// Unused components of the result vector are replicated from the first input
// argument.

#define OZZ_SSE_SPLAT_F(_v, _i) \
  _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(_i, _i, _i, _i))

// _v.x + _v.y, _v.y, _v.z, _v.w
#define OZZ_SSE_HADD2_F(_v) _mm_add_ss(_v, OZZ_SSE_SPLAT_F(_v, 1))

// _v.x + _v.y + _v.z, _v.y, _v.z, _v.w
#define OZZ_SSE_HADD3_F(_v) \
  _mm_add_ss(_mm_add_ss(_v, _mm_unpackhi_ps(_v, _v)), OZZ_SSE_SPLAT_F(_v, 1))

// _v.x + _v.y + _v.z, ?, ?, ?
#define OZZ_SSE_HADD4_F(_v, _r)                                    \
do {                                                               \
    const __m128 haddxyzw = _mm_add_ps(_v, _mm_movehl_ps(_v, _v)); \
    _r = _mm_add_ss(haddxyzw, OZZ_SSE_SPLAT_F(haddxyzw, 1));       \
} while (void(0), 0)

// dot2, ?, ?, ?
#define OZZ_SSE_DOT2_F(_a, _b, _r)               \
do {                                             \
    const __m128 ab = _mm_mul_ps(_a, _b);        \
    _r = _mm_add_ss(ab, OZZ_SSE_SPLAT_F(ab, 1)); \
  \
} while (void(0), 0)

// dot3, ?, ?, ?
#define OZZ_SSE_DOT3_F(_a, _b, _r)        \
do {                                      \
    const __m128 ab = _mm_mul_ps(_a, _b); \
    _r = OZZ_SSE_HADD3_F(ab);             \
} while (void(0), 0)

// dot4, ?, ?, ?
#define OZZ_SSE_DOT4_F(_a, _b, _r)        \
do {                                      \
    const __m128 ab = _mm_mul_ps(_a, _b); \
    OZZ_SSE_HADD4_F(ab, _r);              \
  \
} while (void(0), 0)

#define OZZ_SSE_SELECT_F(_b, _true, _false) \
  _mm_xor_ps(_false,                        \
             _mm_and_ps(_mm_castsi128_ps(_b), _mm_xor_ps(_true, _false)))

#define OZZ_SSE_SPLAT_I(_v, _i)                                               \
  _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(_v), _mm_castsi128_ps(_v), \
                                  _MM_SHUFFLE(_i, _i, _i, _i)))

#define OZZ_SSE_SELECT_I(_b, _true, _false) \
  _mm_xor_si128(_false, _mm_and_si128(_b, _mm_xor_si128(_true, _false)))

// clang-format on

OZZ_INLINE SimdFloat4 zero() { return _mm_setzero_ps(); }

OZZ_INLINE SimdFloat4 one() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2));
}

OZZ_INLINE SimdFloat4 x_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i one = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  return _mm_castsi128_ps(_mm_srli_si128(one, 12));
}

OZZ_INLINE SimdFloat4 y_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i one = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  return _mm_castsi128_ps(_mm_slli_si128(_mm_srli_si128(one, 12), 4));
}

OZZ_INLINE SimdFloat4 z_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i one = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  return _mm_castsi128_ps(_mm_slli_si128(_mm_srli_si128(one, 12), 8));
}

OZZ_INLINE SimdFloat4 w_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i one = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  return _mm_castsi128_ps(_mm_slli_si128(one, 12));
}

OZZ_INLINE SimdFloat4 Load(float _x, float _y, float _z, float _w) {
  return _mm_set_ps(_w, _z, _y, _x);
}

OZZ_INLINE SimdFloat4 LoadX(float _x) { return _mm_set_ss(_x); }

OZZ_INLINE SimdFloat4 Load1(float _x) { return _mm_set_ps1(_x); }

OZZ_INLINE SimdFloat4 LoadPtr(const float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0xf) && "Invalid alignment");
  return _mm_load_ps(_f);
}

OZZ_INLINE SimdFloat4 LoadPtrU(const float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  return _mm_loadu_ps(_f);
}

OZZ_INLINE SimdFloat4 LoadXPtrU(const float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  return _mm_load_ss(_f);
}

OZZ_INLINE SimdFloat4 Load1PtrU(const float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  return _mm_load_ps1(_f);
}

OZZ_INLINE SimdFloat4 Load2PtrU(const float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  return _mm_unpacklo_ps(_mm_load_ss(_f + 0), _mm_load_ss(_f + 1));
}

OZZ_INLINE SimdFloat4 Load3PtrU(const float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  return _mm_movelh_ps(
      _mm_unpacklo_ps(_mm_load_ss(_f + 0), _mm_load_ss(_f + 1)),
      _mm_load_ss(_f + 2));
}

OZZ_INLINE SimdFloat4 FromInt(_SimdInt4 _i) { return _mm_cvtepi32_ps(_i); }
}  // ozz::math::simd_float4

OZZ_INLINE float GetX(_SimdFloat4 _v) { return _mm_cvtss_f32(_v); }

OZZ_INLINE float GetY(_SimdFloat4 _v) {
  return _mm_cvtss_f32(OZZ_SSE_SPLAT_F(_v, 1));
}

OZZ_INLINE float GetZ(_SimdFloat4 _v) {
  return _mm_cvtss_f32(_mm_movehl_ps(_v, _v));
}

OZZ_INLINE float GetW(_SimdFloat4 _v) {
  return _mm_cvtss_f32(OZZ_SSE_SPLAT_F(_v, 3));
}

OZZ_INLINE SimdFloat4 SetX(_SimdFloat4 _v, float _f) {
  return _mm_move_ss(_v, _mm_set_ss(_f));
}

OZZ_INLINE SimdFloat4 SetY(_SimdFloat4 _v, float _f) {
  const __m128 yxzw = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(3, 2, 0, 1));
  const __m128 fxzw = _mm_move_ss(yxzw, _mm_set_ss(_f));
  return _mm_shuffle_ps(fxzw, fxzw, _MM_SHUFFLE(3, 2, 0, 1));
}

OZZ_INLINE SimdFloat4 SetZ(_SimdFloat4 _v, float _f) {
  const __m128 yxzw = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(3, 0, 1, 2));
  const __m128 fxzw = _mm_move_ss(yxzw, _mm_set_ss(_f));
  return _mm_shuffle_ps(fxzw, fxzw, _MM_SHUFFLE(3, 0, 1, 2));
}

OZZ_INLINE SimdFloat4 SetW(_SimdFloat4 _v, float _f) {
  const __m128 yxzw = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(0, 2, 1, 3));
  const __m128 fxzw = _mm_move_ss(yxzw, _mm_set_ss(_f));
  return _mm_shuffle_ps(fxzw, fxzw, _MM_SHUFFLE(0, 2, 1, 3));
}

OZZ_INLINE SimdFloat4 SetI(_SimdFloat4 _v, int _ith, float _f) {
  assert(_ith >= 0 && _ith <= 3 && "Invalid index ranges");
  union {
    SimdFloat4 ret;
    float af[4];
  } u = {_v};
  u.af[_ith] = _f;
  return u.ret;
}

OZZ_INLINE void StorePtr(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0xf) && "Invalid alignment");
  _mm_store_ps(_f, _v);
}

OZZ_INLINE void Store1Ptr(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0xf) && "Invalid alignment");
  _mm_store_ss(_f, _v);
}

OZZ_INLINE void Store2Ptr(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0xf) && "Invalid alignment");
  _mm_storel_pi(reinterpret_cast<__m64*>(_f), _v);
}

OZZ_INLINE void Store3Ptr(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0xf) && "Invalid alignment");
  _mm_storel_pi(reinterpret_cast<__m64*>(_f), _v);
  _mm_store_ss(_f + 2, _mm_movehl_ps(_v, _v));
}

OZZ_INLINE void StorePtrU(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  _mm_storeu_ps(_f, _v);
}

OZZ_INLINE void Store1PtrU(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  _mm_store_ss(_f, _v);
}

OZZ_INLINE void Store2PtrU(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  _mm_store_ss(_f + 0, _v);
  _mm_store_ss(_f + 1, OZZ_SSE_SPLAT_F(_v, 1));
}

OZZ_INLINE void Store3PtrU(_SimdFloat4 _v, float* _f) {
  assert(!(reinterpret_cast<uintptr_t>(_f) & 0x3) && "Invalid alignment");
  _mm_store_ss(_f + 0, _v);
  _mm_store_ss(_f + 1, OZZ_SSE_SPLAT_F(_v, 1));
  _mm_store_ss(_f + 2, _mm_movehl_ps(_v, _v));
}

OZZ_INLINE SimdFloat4 SplatX(_SimdFloat4 _v) { return OZZ_SSE_SPLAT_F(_v, 0); }

OZZ_INLINE SimdFloat4 SplatY(_SimdFloat4 _v) { return OZZ_SSE_SPLAT_F(_v, 1); }

OZZ_INLINE SimdFloat4 SplatZ(_SimdFloat4 _v) { return OZZ_SSE_SPLAT_F(_v, 2); }

OZZ_INLINE SimdFloat4 SplatW(_SimdFloat4 _v) { return OZZ_SSE_SPLAT_F(_v, 3); }

OZZ_INLINE void Transpose4x1(const SimdFloat4 _in[4], SimdFloat4 _out[1]) {
  const __m128 xz = _mm_unpacklo_ps(_in[0], _in[2]);
  const __m128 yw = _mm_unpacklo_ps(_in[1], _in[3]);
  _out[0] = _mm_unpacklo_ps(xz, yw);
}

OZZ_INLINE void Transpose1x4(const SimdFloat4 _in[1], SimdFloat4 _out[4]) {
  const __m128 zwzw = _mm_movehl_ps(_in[0], _in[0]);
  const __m128 yyyy = OZZ_SSE_SPLAT_F(_in[0], 1);
  const __m128 wwww = OZZ_SSE_SPLAT_F(_in[0], 3);
  const __m128 zero = _mm_setzero_ps();
  _out[0] = _mm_move_ss(zero, _in[0]);
  _out[1] = _mm_move_ss(zero, yyyy);
  _out[2] = _mm_move_ss(zero, zwzw);
  _out[3] = _mm_move_ss(zero, wwww);
}

OZZ_INLINE void Transpose4x2(const SimdFloat4 _in[4], SimdFloat4 _out[2]) {
  const __m128 tmp0 = _mm_unpacklo_ps(_in[0], _in[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_in[1], _in[3]);
  _out[0] = _mm_unpacklo_ps(tmp0, tmp1);
  _out[1] = _mm_unpackhi_ps(tmp0, tmp1);
}

OZZ_INLINE void Transpose2x4(const SimdFloat4 _in[2], SimdFloat4 _out[4]) {
  const __m128 tmp0 = _mm_unpacklo_ps(_in[0], _in[1]);
  const __m128 tmp1 = _mm_unpackhi_ps(_in[0], _in[1]);
  const __m128 zero = _mm_setzero_ps();
  _out[0] = _mm_movelh_ps(tmp0, zero);
  _out[1] = _mm_movehl_ps(zero, tmp0);
  _out[2] = _mm_movelh_ps(tmp1, zero);
  _out[3] = _mm_movehl_ps(zero, tmp1);
}

OZZ_INLINE void Transpose4x3(const SimdFloat4 _in[4], SimdFloat4 _out[3]) {
  const __m128 tmp0 = _mm_unpacklo_ps(_in[0], _in[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_in[1], _in[3]);
  const __m128 tmp2 = _mm_unpackhi_ps(_in[0], _in[2]);
  const __m128 tmp3 = _mm_unpackhi_ps(_in[1], _in[3]);
  _out[0] = _mm_unpacklo_ps(tmp0, tmp1);
  _out[1] = _mm_unpackhi_ps(tmp0, tmp1);
  _out[2] = _mm_unpacklo_ps(tmp2, tmp3);
}

OZZ_INLINE void Transpose3x4(const SimdFloat4 _in[3], SimdFloat4 _out[4]) {
  const __m128 zero = _mm_setzero_ps();
  const __m128 temp0 = _mm_unpacklo_ps(_in[0], _in[1]);
  const __m128 temp1 = _mm_unpacklo_ps(_in[2], zero);
  const __m128 temp2 = _mm_unpackhi_ps(_in[0], _in[1]);
  const __m128 temp3 = _mm_unpackhi_ps(_in[2], zero);
  _out[0] = _mm_movelh_ps(temp0, temp1);
  _out[1] = _mm_movehl_ps(temp1, temp0);
  _out[2] = _mm_movelh_ps(temp2, temp3);
  _out[3] = _mm_movehl_ps(temp3, temp2);
}

OZZ_INLINE void Transpose4x4(const SimdFloat4 _in[4], SimdFloat4 _out[4]) {
  const __m128 tmp0 = _mm_unpacklo_ps(_in[0], _in[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_in[1], _in[3]);
  const __m128 tmp2 = _mm_unpackhi_ps(_in[0], _in[2]);
  const __m128 tmp3 = _mm_unpackhi_ps(_in[1], _in[3]);
  _out[0] = _mm_unpacklo_ps(tmp0, tmp1);
  _out[1] = _mm_unpackhi_ps(tmp0, tmp1);
  _out[2] = _mm_unpacklo_ps(tmp2, tmp3);
  _out[3] = _mm_unpackhi_ps(tmp2, tmp3);
}

OZZ_INLINE void Transpose16x16(const SimdFloat4 _in[16], SimdFloat4 _out[16]) {
  const __m128 tmp0 = _mm_unpacklo_ps(_in[0], _in[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_in[1], _in[3]);
  const __m128 tmp2 = _mm_unpackhi_ps(_in[0], _in[2]);
  const __m128 tmp3 = _mm_unpackhi_ps(_in[1], _in[3]);
  const __m128 tmp4 = _mm_unpacklo_ps(_in[4], _in[6]);
  const __m128 tmp5 = _mm_unpacklo_ps(_in[5], _in[7]);
  const __m128 tmp6 = _mm_unpackhi_ps(_in[4], _in[6]);
  const __m128 tmp7 = _mm_unpackhi_ps(_in[5], _in[7]);
  const __m128 tmp8 = _mm_unpacklo_ps(_in[8], _in[10]);
  const __m128 tmp9 = _mm_unpacklo_ps(_in[9], _in[11]);
  const __m128 tmp10 = _mm_unpackhi_ps(_in[8], _in[10]);
  const __m128 tmp11 = _mm_unpackhi_ps(_in[9], _in[11]);
  const __m128 tmp12 = _mm_unpacklo_ps(_in[12], _in[14]);
  const __m128 tmp13 = _mm_unpacklo_ps(_in[13], _in[15]);
  const __m128 tmp14 = _mm_unpackhi_ps(_in[12], _in[14]);
  const __m128 tmp15 = _mm_unpackhi_ps(_in[13], _in[15]);
  _out[0] = _mm_unpacklo_ps(tmp0, tmp1);
  _out[1] = _mm_unpacklo_ps(tmp4, tmp5);
  _out[2] = _mm_unpacklo_ps(tmp8, tmp9);
  _out[3] = _mm_unpacklo_ps(tmp12, tmp13);
  _out[4] = _mm_unpackhi_ps(tmp0, tmp1);
  _out[5] = _mm_unpackhi_ps(tmp4, tmp5);
  _out[6] = _mm_unpackhi_ps(tmp8, tmp9);
  _out[7] = _mm_unpackhi_ps(tmp12, tmp13);
  _out[8] = _mm_unpacklo_ps(tmp2, tmp3);
  _out[9] = _mm_unpacklo_ps(tmp6, tmp7);
  _out[10] = _mm_unpacklo_ps(tmp10, tmp11);
  _out[11] = _mm_unpacklo_ps(tmp14, tmp15);
  _out[12] = _mm_unpackhi_ps(tmp2, tmp3);
  _out[13] = _mm_unpackhi_ps(tmp6, tmp7);
  _out[14] = _mm_unpackhi_ps(tmp10, tmp11);
  _out[15] = _mm_unpackhi_ps(tmp14, tmp15);
}

OZZ_INLINE SimdFloat4 MAdd(_SimdFloat4 _a, _SimdFloat4 _b,
                           _SimdFloat4 _addend) {
  return _mm_add_ps(_mm_mul_ps(_a, _b), _addend);
}

OZZ_INLINE SimdFloat4 DivX(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_div_ss(_a, _b);
}

OZZ_INLINE SimdFloat4 HAdd2(_SimdFloat4 _v) { return OZZ_SSE_HADD2_F(_v); }

OZZ_INLINE SimdFloat4 HAdd3(_SimdFloat4 _v) { return OZZ_SSE_HADD3_F(_v); }

OZZ_INLINE SimdFloat4 HAdd4(_SimdFloat4 _v) {
  __m128 hadd4;
  OZZ_SSE_HADD4_F(_v, hadd4);
  return _mm_move_ss(_v, hadd4);
}

OZZ_INLINE SimdFloat4 Dot2(_SimdFloat4 _a, _SimdFloat4 _b) {
  __m128 dot2;
  OZZ_SSE_DOT2_F(_a, _b, dot2);
  return _mm_move_ss(_a, dot2);
}

OZZ_INLINE SimdFloat4 Dot3(_SimdFloat4 _a, _SimdFloat4 _b) {
  __m128 dot3;
  OZZ_SSE_DOT3_F(_a, _b, dot3);
  return _mm_move_ss(_a, dot3);
}

OZZ_INLINE SimdFloat4 Dot4(_SimdFloat4 _a, _SimdFloat4 _b) {
  __m128 dot4;
  OZZ_SSE_DOT4_F(_a, _b, dot4);
  return _mm_move_ss(_a, dot4);
}

OZZ_INLINE SimdFloat4 Cross3(_SimdFloat4 _a, _SimdFloat4 _b) {
  const __m128 left0 = _mm_shuffle_ps(_a, _a, _MM_SHUFFLE(3, 0, 2, 1));
  const __m128 left1 = _mm_shuffle_ps(_b, _b, _MM_SHUFFLE(3, 0, 2, 1));
  const __m128 right0 = _mm_shuffle_ps(_a, _a, _MM_SHUFFLE(3, 1, 0, 2));
  const __m128 right1 = _mm_shuffle_ps(_b, _b, _MM_SHUFFLE(3, 1, 0, 2));
  return _mm_sub_ps(_mm_mul_ps(left0, right1), _mm_mul_ps(left1, right0));
}

OZZ_INLINE SimdFloat4 RcpEst(_SimdFloat4 _v) { return _mm_rcp_ps(_v); }

OZZ_INLINE SimdFloat4 RcpEstNR(_SimdFloat4 _v) {
  const __m128 nr = _mm_rcp_ps(_v);
  // Do one more Newton-Raphson step to improve precision.
  return _mm_sub_ps(_mm_add_ps(nr, nr), _mm_mul_ps(_mm_mul_ps(nr, nr), _v));
}

OZZ_INLINE SimdFloat4 RcpEstX(_SimdFloat4 _v) { return _mm_rcp_ss(_v); }

OZZ_INLINE SimdFloat4 Sqrt(_SimdFloat4 _v) { return _mm_sqrt_ps(_v); }

OZZ_INLINE SimdFloat4 SqrtX(_SimdFloat4 _v) { return _mm_sqrt_ss(_v); }

OZZ_INLINE SimdFloat4 RSqrtEst(_SimdFloat4 _v) { return _mm_rsqrt_ps(_v); }

OZZ_INLINE SimdFloat4 RSqrtEstNR(_SimdFloat4 _v) {
  const __m128 nr = _mm_rsqrt_ps(_v);
  // Do one more Newton-Raphson step to improve precision.
  const __m128 muls = _mm_mul_ps(_mm_mul_ps(_v, nr), nr);
  return _mm_mul_ps(_mm_mul_ps(_mm_set_ps1(.5f), nr),
                    _mm_sub_ps(_mm_set_ps1(3.f), muls));
}

OZZ_INLINE SimdFloat4 RSqrtEstX(_SimdFloat4 _v) { return _mm_rsqrt_ss(_v); }

OZZ_INLINE SimdFloat4 Abs(_SimdFloat4 _v) {
  return _mm_and_ps(_mm_castsi128_ps(simd_int4::mask_not_sign()), _v);
}

OZZ_INLINE SimdInt4 Sign(_SimdFloat4 _v) {
  return _mm_slli_epi32(_mm_srli_epi32(_mm_castps_si128(_v), 31), 31);
}

OZZ_INLINE SimdFloat4 Length2(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT2_F(_v, _v, sq_len);
  return _mm_move_ss(_v, _mm_sqrt_ss(sq_len));
}

OZZ_INLINE SimdFloat4 Length3(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT3_F(_v, _v, sq_len);
  return _mm_move_ss(_v, _mm_sqrt_ss(sq_len));
}

OZZ_INLINE SimdFloat4 Length4(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT4_F(_v, _v, sq_len);
  return _mm_move_ss(_v, _mm_sqrt_ss(sq_len));
}

OZZ_INLINE SimdFloat4 Length2Sqr(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT2_F(_v, _v, sq_len);
  return _mm_move_ss(_v, sq_len);
}

OZZ_INLINE SimdFloat4 Length3Sqr(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT3_F(_v, _v, sq_len);
  return _mm_move_ss(_v, sq_len);
}

OZZ_INLINE SimdFloat4 Length4Sqr(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT4_F(_v, _v, sq_len);
  return _mm_move_ss(_v, sq_len);
}

OZZ_INLINE SimdFloat4 Normalize2(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT2_F(_v, _v, sq_len);
  assert(_mm_cvtss_f32(sq_len) != 0.f && "_v is not normalizable");
  const __m128 inv_len = _mm_div_ss(simd_float4::one(), _mm_sqrt_ss(sq_len));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 norm = _mm_mul_ps(_v, inv_lenxxxx);
  return _mm_movelh_ps(norm, _mm_movehl_ps(_v, _v));
}

OZZ_INLINE SimdFloat4 Normalize3(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT3_F(_v, _v, sq_len);
  assert(_mm_cvtss_f32(sq_len) != 0.f && "_v is not normalizable");
  const __m128 inv_len = _mm_div_ss(simd_float4::one(), _mm_sqrt_ss(sq_len));
  const __m128 vwxyz = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(0, 1, 2, 3));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 normwxyz = _mm_move_ss(_mm_mul_ps(vwxyz, inv_lenxxxx), vwxyz);
  return _mm_shuffle_ps(normwxyz, normwxyz, _MM_SHUFFLE(0, 1, 2, 3));
}

OZZ_INLINE SimdFloat4 Normalize4(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT4_F(_v, _v, sq_len);
  assert(_mm_cvtss_f32(sq_len) != 0.f && "_v is not normalizable");
  const __m128 inv_len = _mm_div_ss(simd_float4::one(), _mm_sqrt_ss(sq_len));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  return _mm_mul_ps(_v, inv_lenxxxx);
}

OZZ_INLINE SimdFloat4 NormalizeEst2(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT2_F(_v, _v, sq_len);
  assert(_mm_cvtss_f32(sq_len) != 0.f && "_v is not normalizable");
  const __m128 inv_len = _mm_rsqrt_ss(sq_len);
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 norm = _mm_mul_ps(_v, inv_lenxxxx);
  return _mm_movelh_ps(norm, _mm_movehl_ps(_v, _v));
}

OZZ_INLINE SimdFloat4 NormalizeEst3(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT3_F(_v, _v, sq_len);
  assert(_mm_cvtss_f32(sq_len) != 0.f && "_v is not normalizable");
  const __m128 inv_len = _mm_rsqrt_ss(sq_len);
  const __m128 vwxyz = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(0, 1, 2, 3));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 normwxyz = _mm_move_ss(_mm_mul_ps(vwxyz, inv_lenxxxx), vwxyz);
  return _mm_shuffle_ps(normwxyz, normwxyz, _MM_SHUFFLE(0, 1, 2, 3));
}

OZZ_INLINE SimdFloat4 NormalizeEst4(_SimdFloat4 _v) {
  __m128 sq_len;
  OZZ_SSE_DOT4_F(_v, _v, sq_len);
  assert(_mm_cvtss_f32(sq_len) != 0.f && "_v is not normalizable");
  const __m128 inv_len = _mm_rsqrt_ss(sq_len);
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  return _mm_mul_ps(_v, inv_lenxxxx);
}

OZZ_INLINE SimdInt4 IsNormalized2(_SimdFloat4 _v) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceSq);
  __m128 dot;
  OZZ_SSE_DOT2_F(_v, _v, dot);
  __m128 dotx000 = _mm_move_ss(_mm_setzero_ps(), dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdInt4 IsNormalized3(_SimdFloat4 _v) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceSq);
  __m128 dot;
  OZZ_SSE_DOT3_F(_v, _v, dot);
  __m128 dotx000 = _mm_move_ss(_mm_setzero_ps(), dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdInt4 IsNormalized4(_SimdFloat4 _v) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceSq);
  __m128 dot;
  OZZ_SSE_DOT4_F(_v, _v, dot);
  __m128 dotx000 = _mm_move_ss(_mm_setzero_ps(), dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdInt4 IsNormalizedEst2(_SimdFloat4 _v) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceEstSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceEstSq);
  __m128 dot;
  OZZ_SSE_DOT2_F(_v, _v, dot);
  __m128 dotx000 = _mm_move_ss(_mm_setzero_ps(), dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdInt4 IsNormalizedEst3(_SimdFloat4 _v) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceEstSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceEstSq);
  __m128 dot;
  OZZ_SSE_DOT3_F(_v, _v, dot);
  __m128 dotx000 = _mm_move_ss(_mm_setzero_ps(), dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdInt4 IsNormalizedEst4(_SimdFloat4 _v) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceEstSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceEstSq);
  __m128 dot;
  OZZ_SSE_DOT4_F(_v, _v, dot);
  __m128 dotx000 = _mm_move_ss(_mm_setzero_ps(), dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdFloat4 NormalizeSafe2(_SimdFloat4 _v, _SimdFloat4 _safe) {
  __m128 sq_len;
  OZZ_SSE_DOT2_F(_v, _v, sq_len);
  const __m128 inv_len = _mm_div_ss(simd_float4::one(), _mm_sqrt_ss(sq_len));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 norm = _mm_mul_ps(_v, inv_lenxxxx);
  const __m128i cond = _mm_castps_si128(
      _mm_cmple_ps(OZZ_SSE_SPLAT_F(sq_len, 0), _mm_setzero_ps()));
  const __m128 cfalse = _mm_movelh_ps(norm, _mm_movehl_ps(_v, _v));
  return OZZ_SSE_SELECT_F(cond, _safe, cfalse);
}

OZZ_INLINE SimdFloat4 NormalizeSafe3(_SimdFloat4 _v, _SimdFloat4 _safe) {
  __m128 sq_len;
  OZZ_SSE_DOT3_F(_v, _v, sq_len);
  const __m128 inv_len = _mm_div_ss(simd_float4::one(), _mm_sqrt_ss(sq_len));
  const __m128 vwxyz = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(0, 1, 2, 3));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 normwxyz = _mm_move_ss(_mm_mul_ps(vwxyz, inv_lenxxxx), vwxyz);
  const __m128i cond = _mm_castps_si128(
      _mm_cmple_ps(OZZ_SSE_SPLAT_F(sq_len, 0), _mm_setzero_ps()));
  const __m128 cfalse =
      _mm_shuffle_ps(normwxyz, normwxyz, _MM_SHUFFLE(0, 1, 2, 3));
  return OZZ_SSE_SELECT_F(cond, _safe, cfalse);
}

OZZ_INLINE SimdFloat4 NormalizeSafe4(_SimdFloat4 _v, _SimdFloat4 _safe) {
  __m128 sq_len;
  OZZ_SSE_DOT4_F(_v, _v, sq_len);
  const __m128 inv_len = _mm_div_ss(simd_float4::one(), _mm_sqrt_ss(sq_len));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128i cond = _mm_castps_si128(
      _mm_cmple_ps(OZZ_SSE_SPLAT_F(sq_len, 0), _mm_setzero_ps()));
  const __m128 cfalse = _mm_mul_ps(_v, inv_lenxxxx);
  return OZZ_SSE_SELECT_F(cond, _safe, cfalse);
}

OZZ_INLINE SimdFloat4 NormalizeSafeEst2(_SimdFloat4 _v, _SimdFloat4 _safe) {
  __m128 sq_len;
  OZZ_SSE_DOT2_F(_v, _v, sq_len);
  const __m128 inv_len = _mm_rsqrt_ss(sq_len);
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 norm = _mm_mul_ps(_v, inv_lenxxxx);
  const __m128i cond = _mm_castps_si128(
      _mm_cmple_ps(OZZ_SSE_SPLAT_F(sq_len, 0), _mm_setzero_ps()));
  const __m128 cfalse = _mm_movelh_ps(norm, _mm_movehl_ps(_v, _v));
  return OZZ_SSE_SELECT_F(cond, _safe, cfalse);
}

OZZ_INLINE SimdFloat4 NormalizeSafeEst3(_SimdFloat4 _v, _SimdFloat4 _safe) {
  __m128 sq_len;
  OZZ_SSE_DOT3_F(_v, _v, sq_len);
  const __m128 inv_len = _mm_rsqrt_ss(sq_len);
  const __m128 vwxyz = _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(0, 1, 2, 3));
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128 normwxyz = _mm_move_ss(_mm_mul_ps(vwxyz, inv_lenxxxx), vwxyz);
  const __m128i cond = _mm_castps_si128(
      _mm_cmple_ps(OZZ_SSE_SPLAT_F(sq_len, 0), _mm_setzero_ps()));
  const __m128 cfalse =
      _mm_shuffle_ps(normwxyz, normwxyz, _MM_SHUFFLE(0, 1, 2, 3));
  return OZZ_SSE_SELECT_F(cond, _safe, cfalse);
}

OZZ_INLINE SimdFloat4 NormalizeSafeEst4(_SimdFloat4 _v, _SimdFloat4 _safe) {
  __m128 sq_len;
  OZZ_SSE_DOT4_F(_v, _v, sq_len);
  const __m128 inv_len = _mm_rsqrt_ss(sq_len);
  const __m128 inv_lenxxxx = OZZ_SSE_SPLAT_F(inv_len, 0);
  const __m128i cond = _mm_castps_si128(
      _mm_cmple_ps(OZZ_SSE_SPLAT_F(sq_len, 0), _mm_setzero_ps()));
  const __m128 cfalse = _mm_mul_ps(_v, inv_lenxxxx);
  return OZZ_SSE_SELECT_F(cond, _safe, cfalse);
}

OZZ_INLINE SimdFloat4 Lerp(_SimdFloat4 _a, _SimdFloat4 _b, _SimdFloat4 _alpha) {
  return _mm_add_ps(_mm_mul_ps(_alpha, _mm_sub_ps(_b, _a)), _a);
}

OZZ_INLINE SimdFloat4 Min(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_min_ps(_a, _b);
}

OZZ_INLINE SimdFloat4 Max(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_max_ps(_a, _b);
}

OZZ_INLINE SimdFloat4 Min0(_SimdFloat4 _v) {
  return _mm_min_ps(_mm_setzero_ps(), _v);
}

OZZ_INLINE SimdFloat4 Max0(_SimdFloat4 _v) {
  return _mm_max_ps(_mm_setzero_ps(), _v);
}

OZZ_INLINE SimdFloat4 Clamp(_SimdFloat4 _a, _SimdFloat4 _v, _SimdFloat4 _b) {
  return _mm_max_ps(_a, _mm_min_ps(_v, _b));
}

OZZ_INLINE SimdFloat4 Select(_SimdInt4 _b, _SimdFloat4 _true,
                             _SimdFloat4 _false) {
  return OZZ_SSE_SELECT_F(_b, _true, _false);
}

OZZ_INLINE SimdInt4 CmpEq(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_castps_si128(_mm_cmpeq_ps(_a, _b));
}

OZZ_INLINE SimdInt4 CmpNe(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_castps_si128(_mm_cmpneq_ps(_a, _b));
}

OZZ_INLINE SimdInt4 CmpLt(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_castps_si128(_mm_cmplt_ps(_a, _b));
}

OZZ_INLINE SimdInt4 CmpLe(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_castps_si128(_mm_cmple_ps(_a, _b));
}

OZZ_INLINE SimdInt4 CmpGt(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_castps_si128(_mm_cmpgt_ps(_a, _b));
}

OZZ_INLINE SimdInt4 CmpGe(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_castps_si128(_mm_cmpge_ps(_a, _b));
}

OZZ_INLINE SimdFloat4 And(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_and_ps(_a, _b);
}

OZZ_INLINE SimdFloat4 Or(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_or_ps(_a, _b);
}

OZZ_INLINE SimdFloat4 Xor(_SimdFloat4 _a, _SimdFloat4 _b) {
  return _mm_xor_ps(_a, _b);
}

OZZ_INLINE SimdFloat4 And(_SimdFloat4 _a, _SimdInt4 _b) {
  return _mm_and_ps(_a, _mm_castsi128_ps(_b));
}

OZZ_INLINE SimdFloat4 Or(_SimdFloat4 _a, _SimdInt4 _b) {
  return _mm_or_ps(_a, _mm_castsi128_ps(_b));
}

OZZ_INLINE SimdFloat4 Xor(_SimdFloat4 _a, _SimdInt4 _b) {
  return _mm_xor_ps(_a, _mm_castsi128_ps(_b));
}

OZZ_INLINE SimdFloat4 Cos(_SimdFloat4 _v) {
  return _mm_set_ps(std::cos(GetW(_v)), std::cos(GetZ(_v)), std::cos(GetY(_v)),
                    std::cos(GetX(_v)));
}

OZZ_INLINE SimdFloat4 CosX(_SimdFloat4 _v) {
  return _mm_set_ps(GetW(_v), GetZ(_v), GetY(_v), std::cos(GetX(_v)));
}

OZZ_INLINE SimdFloat4 ACos(_SimdFloat4 _v) {
  return _mm_set_ps(std::acos(GetW(_v)), std::acos(GetZ(_v)),
                    std::acos(GetY(_v)), std::acos(GetX(_v)));
}

OZZ_INLINE SimdFloat4 ACosX(_SimdFloat4 _v) {
  return _mm_set_ps(GetW(_v), GetZ(_v), GetY(_v), std::acos(GetX(_v)));
}

OZZ_INLINE SimdFloat4 Sin(_SimdFloat4 _v) {
  return _mm_set_ps(std::sin(GetW(_v)), std::sin(GetZ(_v)), std::sin(GetY(_v)),
                    std::sin(GetX(_v)));
}

OZZ_INLINE SimdFloat4 SinX(_SimdFloat4 _v) {
  return _mm_set_ps(GetW(_v), GetZ(_v), GetY(_v), std::sin(GetX(_v)));
}

OZZ_INLINE SimdFloat4 ASin(_SimdFloat4 _v) {
  return _mm_set_ps(std::asin(GetW(_v)), std::asin(GetZ(_v)),
                    std::asin(GetY(_v)), std::asin(GetX(_v)));
}

OZZ_INLINE SimdFloat4 ASinX(_SimdFloat4 _v) {
  return _mm_set_ps(GetW(_v), GetZ(_v), GetY(_v), std::asin(GetX(_v)));
}

OZZ_INLINE SimdFloat4 Tan(_SimdFloat4 _v) {
  return _mm_set_ps(std::tan(GetW(_v)), std::tan(GetZ(_v)), std::tan(GetY(_v)),
                    std::tan(GetX(_v)));
}

OZZ_INLINE SimdFloat4 TanX(_SimdFloat4 _v) {
  return _mm_set_ps(GetW(_v), GetZ(_v), GetY(_v), std::tan(GetX(_v)));
}

OZZ_INLINE SimdFloat4 ATan(_SimdFloat4 _v) {
  return _mm_set_ps(std::atan(GetW(_v)), std::atan(GetZ(_v)),
                    std::atan(GetY(_v)), std::atan(GetX(_v)));
}

OZZ_INLINE SimdFloat4 ATanX(_SimdFloat4 _v) {
  return _mm_set_ps(GetW(_v), GetZ(_v), GetY(_v), std::atan(GetX(_v)));
}

namespace simd_int4 {

OZZ_INLINE SimdInt4 zero() { return _mm_setzero_si128(); }

OZZ_INLINE SimdInt4 one() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_srli_epi32(ffff, 31);
}

OZZ_INLINE SimdInt4 x_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_srli_si128(_mm_srli_epi32(ffff, 31), 12);
}

OZZ_INLINE SimdInt4 y_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_slli_si128(_mm_srli_si128(_mm_srli_epi32(ffff, 31), 12), 4);
}

OZZ_INLINE SimdInt4 z_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_slli_si128(_mm_srli_si128(_mm_srli_epi32(ffff, 31), 12), 8);
}

OZZ_INLINE SimdInt4 w_axis() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_slli_si128(_mm_srli_epi32(ffff, 31), 12);
}

OZZ_INLINE SimdInt4 all_true() {
  return _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
}

OZZ_INLINE SimdInt4 all_false() { return _mm_setzero_si128(); }

OZZ_INLINE SimdInt4 mask_sign() {
  const __m128i ffff =
      _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
  return _mm_slli_epi32(ffff, 31);
}

OZZ_INLINE SimdInt4 mask_not_sign() {
  const __m128i ffff =
      _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
  return _mm_srli_epi32(ffff, 1);
}

OZZ_INLINE SimdInt4 mask_ffff() {
  return _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128());
}
OZZ_INLINE SimdInt4 mask_0000() { return _mm_setzero_si128(); }

OZZ_INLINE SimdInt4 mask_fff0() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_srli_si128(ffff, 4);
}

OZZ_INLINE SimdInt4 mask_f000() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_srli_si128(ffff, 12);
}

OZZ_INLINE SimdInt4 mask_0f00() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_srli_si128(_mm_slli_si128(ffff, 12), 8);
}

OZZ_INLINE SimdInt4 mask_00f0() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_srli_si128(_mm_slli_si128(ffff, 12), 4);
}

OZZ_INLINE SimdInt4 mask_000f() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  return _mm_slli_si128(ffff, 12);
}

OZZ_INLINE SimdInt4 Load(int _x, int _y, int _z, int _w) {
  return _mm_set_epi32(_w, _z, _y, _x);
}

OZZ_INLINE SimdInt4 LoadX(int _x) { return _mm_set_epi32(0, 0, 0, _x); }

OZZ_INLINE SimdInt4 Load1(int _x) { return _mm_set1_epi32(_x); }

OZZ_INLINE SimdInt4 Load(bool _x, bool _y, bool _z, bool _w) {
  return _mm_sub_epi32(_mm_setzero_si128(), _mm_set_epi32(_w, _z, _y, _x));
}

OZZ_INLINE SimdInt4 LoadX(bool _x) {
  return _mm_sub_epi32(_mm_setzero_si128(), _mm_set_epi32(0, 0, 0, _x));
}

OZZ_INLINE SimdInt4 Load1(bool _x) {
  return _mm_sub_epi32(_mm_setzero_si128(), _mm_set1_epi32(_x));
}

OZZ_INLINE SimdInt4 LoadPtr(const int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  return _mm_load_si128(reinterpret_cast<const __m128i*>(_i));
}

OZZ_INLINE SimdInt4 LoadXPtr(const int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  return _mm_cvtsi32_si128(*_i);
}

OZZ_INLINE SimdInt4 Load1Ptr(const int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  return _mm_shuffle_epi32(
      _mm_loadl_epi64(reinterpret_cast<const __m128i*>(_i)),
      _MM_SHUFFLE(0, 0, 0, 0));
}

OZZ_INLINE SimdInt4 Load2Ptr(const int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  return _mm_loadl_epi64(reinterpret_cast<const __m128i*>(_i));
}

OZZ_INLINE SimdInt4 Load3Ptr(const int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  return _mm_set_epi32(0, _i[2], _i[1], _i[0]);
}

OZZ_INLINE SimdInt4 LoadPtrU(const int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  return _mm_loadu_si128(reinterpret_cast<const __m128i*>(_i));
}

OZZ_INLINE SimdInt4 LoadXPtrU(const int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  return _mm_cvtsi32_si128(*_i);
}

OZZ_INLINE SimdInt4 Load1PtrU(const int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  return _mm_set1_epi32(*_i);
}

OZZ_INLINE SimdInt4 Load2PtrU(const int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  return _mm_set_epi32(0, 0, _i[1], _i[0]);
}

OZZ_INLINE SimdInt4 Load3PtrU(const int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  return _mm_set_epi32(0, _i[2], _i[1], _i[0]);
}

OZZ_INLINE SimdInt4 FromFloatRound(_SimdFloat4 _f) {
  return _mm_cvtps_epi32(_f);
}

OZZ_INLINE SimdInt4 FromFloatTrunc(_SimdFloat4 _f) {
  return _mm_cvttps_epi32(_f);
}
}  // namespace simd_int4

OZZ_INLINE int GetX(_SimdInt4 _v) { return _mm_cvtsi128_si32(_v); }

OZZ_INLINE int GetY(_SimdInt4 _v) {
  return _mm_cvtsi128_si32(OZZ_SSE_SPLAT_I(_v, 1));
}

OZZ_INLINE int GetZ(_SimdInt4 _v) {
  return _mm_cvtsi128_si32(_mm_unpackhi_epi32(_v, _v));
}

OZZ_INLINE int GetW(_SimdInt4 _v) {
  return _mm_cvtsi128_si32(OZZ_SSE_SPLAT_I(_v, 3));
}

OZZ_INLINE SimdInt4 SetX(_SimdInt4 _v, int _i) {
  return _mm_castps_si128(
      _mm_move_ss(_mm_castsi128_ps(_v), _mm_castsi128_ps(_mm_set1_epi32(_i))));
}

OZZ_INLINE SimdInt4 SetY(_SimdInt4 _v, int _i) {
  const __m128 i = _mm_castsi128_ps(_mm_set1_epi32(_i));
  const __m128 v = _mm_castsi128_ps(_v);
  const __m128 yxzw = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 2, 0, 1));
  const __m128 fxzw = _mm_move_ss(yxzw, i);
  return _mm_castps_si128(_mm_shuffle_ps(fxzw, fxzw, _MM_SHUFFLE(3, 2, 0, 1)));
}

OZZ_INLINE SimdInt4 SetZ(_SimdInt4 _v, int _i) {
  const __m128 i = _mm_castsi128_ps(_mm_set1_epi32(_i));
  const __m128 v = _mm_castsi128_ps(_v);
  const __m128 yxzw = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 0, 1, 2));
  const __m128 fxzw = _mm_move_ss(yxzw, i);
  return _mm_castps_si128(_mm_shuffle_ps(fxzw, fxzw, _MM_SHUFFLE(3, 0, 1, 2)));
}

OZZ_INLINE SimdInt4 SetW(_SimdInt4 _v, int _i) {
  const __m128 i = _mm_castsi128_ps(_mm_set1_epi32(_i));
  const __m128 v = _mm_castsi128_ps(_v);
  const __m128 yxzw = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 2, 1, 3));
  const __m128 fxzw = _mm_move_ss(yxzw, i);
  return _mm_castps_si128(_mm_shuffle_ps(fxzw, fxzw, _MM_SHUFFLE(0, 2, 1, 3)));
}

OZZ_INLINE SimdInt4 SetI(_SimdInt4 _v, int _ith, int _i) {
  assert(_ith >= 0 && _ith <= 3 && "Invalid index ranges");
  union {
    SimdInt4 ret;
    int af[4];
  } u = {_v};
  u.af[_ith] = _i;
  return u.ret;
}

OZZ_INLINE void StorePtr(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  _mm_store_si128(reinterpret_cast<__m128i*>(_i), _v);
}

OZZ_INLINE void Store1Ptr(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  *_i = _mm_cvtsi128_si32(_v);
}

OZZ_INLINE void Store2Ptr(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  _i[0] = _mm_cvtsi128_si32(_v);
  _i[1] = _mm_cvtsi128_si32(OZZ_SSE_SPLAT_I(_v, 1));
}

OZZ_INLINE void Store3Ptr(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0xf) && "Invalid alignment");
  _i[0] = _mm_cvtsi128_si32(_v);
  _i[1] = _mm_cvtsi128_si32(OZZ_SSE_SPLAT_I(_v, 1));
  _i[2] = _mm_cvtsi128_si32(_mm_unpackhi_epi32(_v, _v));
}

OZZ_INLINE void StorePtrU(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  _mm_storeu_si128(reinterpret_cast<__m128i*>(_i), _v);
}

OZZ_INLINE void Store1PtrU(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  *_i = _mm_cvtsi128_si32(_v);
}

OZZ_INLINE void Store2PtrU(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  _i[0] = _mm_cvtsi128_si32(_v);
  _i[1] = _mm_cvtsi128_si32(OZZ_SSE_SPLAT_I(_v, 1));
}

OZZ_INLINE void Store3PtrU(_SimdInt4 _v, int* _i) {
  assert(!(uintptr_t(_i) & 0x3) && "Invalid alignment");
  _i[0] = _mm_cvtsi128_si32(_v);
  _i[1] = _mm_cvtsi128_si32(OZZ_SSE_SPLAT_I(_v, 1));
  _i[2] = _mm_cvtsi128_si32(_mm_unpackhi_epi32(_v, _v));
}

OZZ_INLINE SimdInt4 SplatX(_SimdInt4 _a) { return OZZ_SSE_SPLAT_I(_a, 0); }

OZZ_INLINE SimdInt4 SplatY(_SimdInt4 _a) { return OZZ_SSE_SPLAT_I(_a, 1); }

OZZ_INLINE SimdInt4 SplatZ(_SimdInt4 _a) { return OZZ_SSE_SPLAT_I(_a, 2); }

OZZ_INLINE SimdInt4 SplatW(_SimdInt4 _a) { return OZZ_SSE_SPLAT_I(_a, 3); }

OZZ_INLINE int MoveMask(_SimdInt4 _v) {
  return _mm_movemask_ps(_mm_castsi128_ps(_v));
}

OZZ_INLINE bool AreAllTrue(_SimdInt4 _v) {
  return _mm_movemask_ps(_mm_castsi128_ps(_v)) == 0xf;
}

OZZ_INLINE bool AreAllTrue3(_SimdInt4 _v) {
  return (_mm_movemask_ps(_mm_castsi128_ps(_v)) & 0x7) == 0x7;
}

OZZ_INLINE bool AreAllTrue2(_SimdInt4 _v) {
  return (_mm_movemask_ps(_mm_castsi128_ps(_v)) & 0x3) == 0x3;
}

OZZ_INLINE bool AreAllTrue1(_SimdInt4 _v) {
  return (_mm_movemask_ps(_mm_castsi128_ps(_v)) & 0x1) == 0x1;
}

OZZ_INLINE bool AreAllFalse(_SimdInt4 _v) {
  return _mm_movemask_ps(_mm_castsi128_ps(_v)) == 0;
}

OZZ_INLINE bool AreAllFalse3(_SimdInt4 _v) {
  return (_mm_movemask_ps(_mm_castsi128_ps(_v)) & 0x7) == 0;
}

OZZ_INLINE bool AreAllFalse2(_SimdInt4 _v) {
  return (_mm_movemask_ps(_mm_castsi128_ps(_v)) & 0x3) == 0;
}

OZZ_INLINE bool AreAllFalse1(_SimdInt4 _v) {
  return (_mm_movemask_ps(_mm_castsi128_ps(_v)) & 0x1) == 0;
}

OZZ_INLINE SimdInt4 HAdd2(_SimdInt4 _v) {
  const __m128i hadd = _mm_add_epi32(_v, OZZ_SSE_SPLAT_I(_v, 1));
  return _mm_castps_si128(
      _mm_move_ss(_mm_castsi128_ps(_v), _mm_castsi128_ps(hadd)));
}

OZZ_INLINE SimdInt4 HAdd3(_SimdInt4 _v) {
  const __m128i hadd = _mm_add_epi32(_mm_add_epi32(_v, OZZ_SSE_SPLAT_I(_v, 1)),
                                     _mm_unpackhi_epi32(_v, _v));
  return _mm_castps_si128(
      _mm_move_ss(_mm_castsi128_ps(_v), _mm_castsi128_ps(hadd)));
}

OZZ_INLINE SimdInt4 HAdd4(_SimdInt4 _v) {
  const __m128 v = _mm_castsi128_ps(_v);
  const __m128i haddxyzw =
      _mm_add_epi32(_v, _mm_castps_si128(_mm_movehl_ps(v, v)));
  return _mm_castps_si128(_mm_move_ss(
      v,
      _mm_castsi128_ps(_mm_add_epi32(haddxyzw, OZZ_SSE_SPLAT_I(haddxyzw, 1)))));
}

OZZ_INLINE SimdInt4 Abs(_SimdInt4 _v) {
  const __m128i zero = _mm_setzero_si128();
  return OZZ_SSE_SELECT_I(_mm_cmplt_epi32(_v, zero), _mm_sub_epi32(zero, _v),
                          _v);
}

OZZ_INLINE SimdInt4 Sign(_SimdInt4 _v) {
  return _mm_slli_epi32(_mm_srli_epi32(_v, 31), 31);
}

OZZ_INLINE SimdInt4 Min(_SimdInt4 _a, _SimdInt4 _b) {
  // SSE4 _mm_min_epi32
  return OZZ_SSE_SELECT_I(_mm_cmplt_epi32(_a, _b), _a, _b);
}

OZZ_INLINE SimdInt4 Max(_SimdInt4 _a, _SimdInt4 _b) {
  // SSE4 _mm_max_epi32
  return OZZ_SSE_SELECT_I(_mm_cmpgt_epi32(_a, _b), _a, _b);
}

OZZ_INLINE SimdInt4 Min0(_SimdInt4 _v) {
  // SSE4 _mm_min_epi32
  const __m128i zero = _mm_setzero_si128();
  return OZZ_SSE_SELECT_I(_mm_cmplt_epi32(zero, _v), zero, _v);
}

OZZ_INLINE SimdInt4 Max0(_SimdInt4 _v) {
  // SSE4 _mm_max_epi32
  const __m128i zero = _mm_setzero_si128();
  return OZZ_SSE_SELECT_I(_mm_cmpgt_epi32(zero, _v), zero, _v);
}

OZZ_INLINE SimdInt4 Clamp(_SimdInt4 _a, _SimdInt4 _v, _SimdInt4 _b) {
  // SSE4 _mm_min_epi32/_mm_max_epi32
  const __m128i min = OZZ_SSE_SELECT_I(_mm_cmplt_epi32(_v, _b), _v, _b);
  return OZZ_SSE_SELECT_I(_mm_cmpgt_epi32(_a, min), _a, min);
}

OZZ_INLINE SimdInt4 Select(_SimdInt4 _b, _SimdInt4 _true, _SimdInt4 _false) {
  return OZZ_SSE_SELECT_I(_b, _true, _false);
}

OZZ_INLINE SimdInt4 And(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_and_si128(_a, _b);
}

OZZ_INLINE SimdInt4 Or(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_or_si128(_a, _b);
}

OZZ_INLINE SimdInt4 Xor(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_xor_si128(_a, _b);
}

OZZ_INLINE SimdInt4 Not(_SimdInt4 _v) {
  return _mm_andnot_si128(
      _v, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
}

OZZ_INLINE SimdInt4 ShiftL(_SimdInt4 _v, int _bits) {
  return _mm_slli_epi32(_v, _bits);
}

OZZ_INLINE SimdInt4 ShiftR(_SimdInt4 _v, int _bits) {
  return _mm_srai_epi32(_v, _bits);
}

OZZ_INLINE SimdInt4 ShiftRu(_SimdInt4 _v, int _bits) {
  return _mm_srli_epi32(_v, _bits);
}

OZZ_INLINE SimdInt4 CmpEq(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_cmpeq_epi32(_a, _b);
}

OZZ_INLINE SimdInt4 CmpNe(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_castps_si128(
      _mm_cmpneq_ps(_mm_castsi128_ps(_a), _mm_castsi128_ps(_b)));
}

OZZ_INLINE SimdInt4 CmpLt(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_cmplt_epi32(_a, _b);
}

OZZ_INLINE SimdInt4 CmpLe(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_andnot_si128(
      _mm_cmpgt_epi32(_a, _b),
      _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
}

OZZ_INLINE SimdInt4 CmpGt(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_cmpgt_epi32(_a, _b);
}

OZZ_INLINE SimdInt4 CmpGe(_SimdInt4 _a, _SimdInt4 _b) {
  return _mm_andnot_si128(
      _mm_cmplt_epi32(_a, _b),
      _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
}

OZZ_INLINE Float4x4 Float4x4::identity() {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i one = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  const __m128i x = _mm_srli_si128(one, 12);

  const Float4x4 ret = {{_mm_castsi128_ps(x),
                         _mm_castsi128_ps(_mm_slli_si128(x, 4)),
                         _mm_castsi128_ps(_mm_slli_si128(x, 8)),
                         _mm_castsi128_ps(_mm_slli_si128(one, 12))}};
  return ret;
}

OZZ_INLINE Float4x4 Transpose(const Float4x4& _m) {
  const __m128 tmp0 = _mm_unpacklo_ps(_m.cols[0], _m.cols[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_m.cols[1], _m.cols[3]);
  const __m128 tmp2 = _mm_unpackhi_ps(_m.cols[0], _m.cols[2]);
  const __m128 tmp3 = _mm_unpackhi_ps(_m.cols[1], _m.cols[3]);
  const Float4x4 ret = {
      {_mm_unpacklo_ps(tmp0, tmp1), _mm_unpackhi_ps(tmp0, tmp1),
       _mm_unpacklo_ps(tmp2, tmp3), _mm_unpackhi_ps(tmp2, tmp3)}};
  return ret;
}

OZZ_INLINE Float4x4 Invert(const Float4x4& _m) {
  const __m128 _t0 =
      _mm_shuffle_ps(_m.cols[0], _m.cols[1], _MM_SHUFFLE(1, 0, 1, 0));
  const __m128 _t1 =
      _mm_shuffle_ps(_m.cols[2], _m.cols[3], _MM_SHUFFLE(1, 0, 1, 0));
  const __m128 _t2 =
      _mm_shuffle_ps(_m.cols[0], _m.cols[1], _MM_SHUFFLE(3, 2, 3, 2));
  const __m128 _t3 =
      _mm_shuffle_ps(_m.cols[2], _m.cols[3], _MM_SHUFFLE(3, 2, 3, 2));
  const __m128 c0 = _mm_shuffle_ps(_t0, _t1, _MM_SHUFFLE(2, 0, 2, 0));
  const __m128 c1 = _mm_shuffle_ps(_t1, _t0, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128 c2 = _mm_shuffle_ps(_t2, _t3, _MM_SHUFFLE(2, 0, 2, 0));
  const __m128 c3 = _mm_shuffle_ps(_t3, _t2, _MM_SHUFFLE(3, 1, 3, 1));

  __m128 minor0, minor1, minor2, minor3, tmp1, tmp2;
  tmp1 = _mm_mul_ps(c2, c3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
  minor0 = _mm_mul_ps(c1, tmp1);
  minor1 = _mm_mul_ps(c0, tmp1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
  minor0 = _mm_sub_ps(_mm_mul_ps(c1, tmp1), minor0);
  minor1 = _mm_sub_ps(_mm_mul_ps(c0, tmp1), minor1);
  minor1 = _mm_shuffle_ps(minor1, minor1, 0x4E);

  tmp1 = _mm_mul_ps(c1, c2);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
  minor0 = _mm_add_ps(_mm_mul_ps(c3, tmp1), minor0);
  minor3 = _mm_mul_ps(c0, tmp1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
  minor0 = _mm_sub_ps(minor0, _mm_mul_ps(c3, tmp1));
  minor3 = _mm_sub_ps(_mm_mul_ps(c0, tmp1), minor3);
  minor3 = _mm_shuffle_ps(minor3, minor3, 0x4E);

  tmp1 = _mm_mul_ps(_mm_shuffle_ps(c1, c1, 0x4E), c3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
  tmp2 = _mm_shuffle_ps(c2, c2, 0x4E);
  minor0 = _mm_add_ps(_mm_mul_ps(tmp2, tmp1), minor0);
  minor2 = _mm_mul_ps(c0, tmp1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
  minor0 = _mm_sub_ps(minor0, _mm_mul_ps(tmp2, tmp1));
  minor2 = _mm_sub_ps(_mm_mul_ps(c0, tmp1), minor2);
  minor2 = _mm_shuffle_ps(minor2, minor2, 0x4E);

  tmp1 = _mm_mul_ps(c0, c1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
  minor2 = _mm_add_ps(_mm_mul_ps(c3, tmp1), minor2);
  minor3 = _mm_sub_ps(_mm_mul_ps(tmp2, tmp1), minor3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
  minor2 = _mm_sub_ps(_mm_mul_ps(c3, tmp1), minor2);
  minor3 = _mm_sub_ps(minor3, _mm_mul_ps(tmp2, tmp1));

  tmp1 = _mm_mul_ps(c0, c3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
  minor1 = _mm_sub_ps(minor1, _mm_mul_ps(tmp2, tmp1));
  minor2 = _mm_add_ps(_mm_mul_ps(c1, tmp1), minor2);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
  minor1 = _mm_add_ps(_mm_mul_ps(tmp2, tmp1), minor1);
  minor2 = _mm_sub_ps(minor2, _mm_mul_ps(c1, tmp1));

  tmp1 = _mm_mul_ps(c0, tmp2);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
  minor1 = _mm_add_ps(_mm_mul_ps(c3, tmp1), minor1);
  minor3 = _mm_sub_ps(minor3, _mm_mul_ps(c1, tmp1));
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
  minor1 = _mm_sub_ps(minor1, _mm_mul_ps(c3, tmp1));
  minor3 = _mm_add_ps(_mm_mul_ps(c1, tmp1), minor3);

  __m128 det;
  det = _mm_mul_ps(c0, minor0);
  det = _mm_add_ps(_mm_shuffle_ps(det, det, 0x4E), det);
  det = _mm_add_ss(_mm_shuffle_ps(det, det, 0xB1), det);
  tmp1 = _mm_rcp_ss(det);
  det = _mm_sub_ss(_mm_add_ss(tmp1, tmp1),
                   _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
  det = _mm_shuffle_ps(det, det, 0x00);

  // Copy the final columns
  const Float4x4 ret = {{_mm_mul_ps(det, minor0), _mm_mul_ps(det, minor1),
                         _mm_mul_ps(det, minor2), _mm_mul_ps(det, minor3)}};
  return ret;
}

Float4x4 Float4x4::Translation(_SimdFloat4 _v) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i mask000f = _mm_slli_si128(ffff, 12);
  const __m128i one = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  const __m128i x = _mm_srli_si128(one, 12);
  const Float4x4 ret = {
      {_mm_castsi128_ps(x), _mm_castsi128_ps(_mm_slli_si128(x, 4)),
       _mm_castsi128_ps(_mm_slli_si128(x, 8)),
       OZZ_SSE_SELECT_F(mask000f, _mm_castsi128_ps(one), _v)}};
  return ret;
}  // math

Float4x4 Float4x4::Scaling(_SimdFloat4 _v) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i if000 = _mm_srli_si128(ffff, 12);
  const __m128i ione = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  const Float4x4 ret = {
      {_mm_and_ps(_v, _mm_castsi128_ps(if000)),
       _mm_and_ps(_v, _mm_castsi128_ps(_mm_slli_si128(if000, 4))),
       _mm_and_ps(_v, _mm_castsi128_ps(_mm_slli_si128(if000, 8))),
       _mm_castsi128_ps(_mm_slli_si128(ione, 12))}};
  return ret;
}  // math

OZZ_INLINE Float4x4 Translate(const Float4x4& _m, _SimdFloat4 _v) {
  const __m128 vxxxx = OZZ_SSE_SPLAT_F(_v, 0);
  const __m128 vyyyy = OZZ_SSE_SPLAT_F(_v, 1);
  const __m128 m0 = _mm_mul_ps(_m.cols[0], vxxxx);
  const __m128 m1 = _mm_mul_ps(_m.cols[1], vyyyy);
  const __m128 vzzzz = OZZ_SSE_SPLAT_F(_v, 2);
  const __m128 a01 = _mm_add_ps(m0, m1);
  const __m128 m2 = _mm_mul_ps(_m.cols[2], vzzzz);
  const __m128 m3 = _mm_add_ps(m2, _m.cols[3]);
  const __m128 col3 = _mm_add_ps(a01, m3);

  const Float4x4 ret = {{_m.cols[0], _m.cols[1], _m.cols[2], col3}};
  return ret;
}

OZZ_INLINE Float4x4 Scale(const Float4x4& _m, _SimdFloat4 _v) {
  const Float4x4 ret = {{_mm_mul_ps(_m.cols[0], OZZ_SSE_SPLAT_F(_v, 0)),
                         _mm_mul_ps(_m.cols[1], OZZ_SSE_SPLAT_F(_v, 1)),
                         _mm_mul_ps(_m.cols[2], OZZ_SSE_SPLAT_F(_v, 2)),
                         _m.cols[3]}};
  return ret;
}

OZZ_INLINE Float4x4 ColumnMultiply(const Float4x4& _m, _SimdFloat4 _v) {
  const Float4x4 ret = {{_mm_mul_ps(_m.cols[0], _v), _mm_mul_ps(_m.cols[1], _v),
                         _mm_mul_ps(_m.cols[2], _v),
                         _mm_mul_ps(_m.cols[3], _v)}};
  return ret;
}

OZZ_INLINE SimdInt4 IsNormalized(const Float4x4& _m) {
  const __m128 max = _mm_set_ps1(1.f + kNormalizationToleranceSq);
  const __m128 min = _mm_set_ps1(1.f - kNormalizationToleranceSq);

  const __m128 tmp0 = _mm_unpacklo_ps(_m.cols[0], _m.cols[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_m.cols[1], _m.cols[3]);
  const __m128 tmp2 = _mm_unpackhi_ps(_m.cols[0], _m.cols[2]);
  const __m128 tmp3 = _mm_unpackhi_ps(_m.cols[1], _m.cols[3]);
  const __m128 row0 = _mm_unpacklo_ps(tmp0, tmp1);
  const __m128 row1 = _mm_unpackhi_ps(tmp0, tmp1);
  const __m128 row2 = _mm_unpacklo_ps(tmp2, tmp3);

  const __m128 dot =
      _mm_add_ps(_mm_add_ps(_mm_mul_ps(row0, row0), _mm_mul_ps(row1, row1)),
                 _mm_mul_ps(row2, row2));
  const __m128 normalized =
      _mm_and_ps(_mm_cmplt_ps(dot, max), _mm_cmpgt_ps(dot, min));
  return _mm_castps_si128(
      _mm_and_ps(normalized, _mm_castsi128_ps(simd_int4::mask_fff0())));
}

OZZ_INLINE SimdInt4 IsNormalizedEst(const Float4x4& _m) {
  const __m128 max = _mm_set_ps1(1.f + kNormalizationToleranceEstSq);
  const __m128 min = _mm_set_ps1(1.f - kNormalizationToleranceEstSq);

  const __m128 tmp0 = _mm_unpacklo_ps(_m.cols[0], _m.cols[2]);
  const __m128 tmp1 = _mm_unpacklo_ps(_m.cols[1], _m.cols[3]);
  const __m128 tmp2 = _mm_unpackhi_ps(_m.cols[0], _m.cols[2]);
  const __m128 tmp3 = _mm_unpackhi_ps(_m.cols[1], _m.cols[3]);
  const __m128 row0 = _mm_unpacklo_ps(tmp0, tmp1);
  const __m128 row1 = _mm_unpackhi_ps(tmp0, tmp1);
  const __m128 row2 = _mm_unpacklo_ps(tmp2, tmp3);

  const __m128 dot =
      _mm_add_ps(_mm_add_ps(_mm_mul_ps(row0, row0), _mm_mul_ps(row1, row1)),
                 _mm_mul_ps(row2, row2));

  const __m128 normalized =
      _mm_and_ps(_mm_cmplt_ps(dot, max), _mm_cmpgt_ps(dot, min));

  return _mm_castps_si128(
      _mm_and_ps(normalized, _mm_castsi128_ps(simd_int4::mask_fff0())));
}

OZZ_INLINE SimdInt4 IsOrthogonal(const Float4x4& _m) {
  const __m128 max = _mm_set_ss(1.f + kNormalizationToleranceSq);
  const __m128 min = _mm_set_ss(1.f - kNormalizationToleranceSq);
  const __m128 zero = _mm_setzero_ps();

  // Use simd_float4::zero() if one of the normalization fails. _m will then be
  // considered not orthogonal.
  const SimdFloat4 cross = NormalizeSafe3(Cross3(_m.cols[0], _m.cols[1]), zero);
  const SimdFloat4 at = NormalizeSafe3(_m.cols[2], zero);

  SimdFloat4 dot;
  OZZ_SSE_DOT3_F(cross, at, dot);
  __m128 dotx000 = _mm_move_ss(zero, dot);
  return _mm_castps_si128(
      _mm_and_ps(_mm_cmplt_ss(dotx000, max), _mm_cmpgt_ss(dotx000, min)));
}

OZZ_INLINE SimdFloat4 ToQuaternion(const Float4x4& _m) {
  assert(AreAllTrue3(IsNormalizedEst(_m)));
  assert(AreAllTrue1(IsOrthogonal(_m)));

  // Prepares constants.
  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128 half = _mm_set1_ps(0.5f);
  const __m128i mask_f000 = _mm_srli_si128(ffff, 12);
  const __m128i mask_000f = _mm_slli_si128(ffff, 12);
  const __m128 one =
      _mm_castsi128_ps(_mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2));
  const __m128i mask_0f00 = _mm_slli_si128(mask_f000, 4);
  const __m128i mask_00f0 = _mm_slli_si128(mask_f000, 8);

  const __m128 xx_yy = OZZ_SSE_SELECT_F(mask_0f00, _m.cols[1], _m.cols[0]);
  const __m128 xx_yy_0010 =
      _mm_shuffle_ps(xx_yy, xx_yy, _MM_SHUFFLE(0, 0, 1, 0));
  const __m128 xx_yy_zz_xx =
      OZZ_SSE_SELECT_F(mask_00f0, _m.cols[2], xx_yy_0010);
  const __m128 yy_zz_xx_yy =
      _mm_shuffle_ps(xx_yy_zz_xx, xx_yy_zz_xx, _MM_SHUFFLE(1, 0, 2, 1));
  const __m128 zz_xx_yy_zz =
      _mm_shuffle_ps(xx_yy_zz_xx, xx_yy_zz_xx, _MM_SHUFFLE(2, 1, 0, 2));

  const __m128 diag_sum =
      _mm_add_ps(_mm_add_ps(xx_yy_zz_xx, yy_zz_xx_yy), zz_xx_yy_zz);
  const __m128 diag_diff =
      _mm_sub_ps(_mm_sub_ps(xx_yy_zz_xx, yy_zz_xx_yy), zz_xx_yy_zz);
  const __m128 radicand =
      _mm_add_ps(OZZ_SSE_SELECT_F(mask_000f, diag_sum, diag_diff), one);
  const __m128 invSqrt = one / _mm_sqrt_ps(radicand);

  __m128 zy_xz_yx = OZZ_SSE_SELECT_F(mask_00f0, _m.cols[1], _m.cols[0]);
  zy_xz_yx = _mm_shuffle_ps(zy_xz_yx, zy_xz_yx, _MM_SHUFFLE(0, 1, 2, 2));
  zy_xz_yx =
      OZZ_SSE_SELECT_F(mask_0f00, OZZ_SSE_SPLAT_F(_m.cols[2], 0), zy_xz_yx);
  __m128 yz_zx_xy = OZZ_SSE_SELECT_F(mask_f000, _m.cols[1], _m.cols[0]);
  yz_zx_xy = _mm_shuffle_ps(yz_zx_xy, yz_zx_xy, _MM_SHUFFLE(0, 0, 2, 0));
  yz_zx_xy =
      OZZ_SSE_SELECT_F(mask_f000, OZZ_SSE_SPLAT_F(_m.cols[2], 1), yz_zx_xy);
  const __m128 sum = _mm_add_ps(zy_xz_yx, yz_zx_xy);
  const __m128 diff = _mm_sub_ps(zy_xz_yx, yz_zx_xy);
  const __m128 scale = _mm_mul_ps(invSqrt, half);

  const __m128 sum0 = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 1, 2, 0));
  const __m128 sum1 = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 0, 0, 2));
  const __m128 sum2 = _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(0, 0, 0, 1));
  __m128 res0 = OZZ_SSE_SELECT_F(mask_000f, OZZ_SSE_SPLAT_F(diff, 0), sum0);
  __m128 res1 = OZZ_SSE_SELECT_F(mask_000f, OZZ_SSE_SPLAT_F(diff, 1), sum1);
  __m128 res2 = OZZ_SSE_SELECT_F(mask_000f, OZZ_SSE_SPLAT_F(diff, 2), sum2);
  res0 = _mm_mul_ps(OZZ_SSE_SELECT_F(mask_f000, radicand, res0),
                    OZZ_SSE_SPLAT_F(scale, 0));
  res1 = _mm_mul_ps(OZZ_SSE_SELECT_F(mask_0f00, radicand, res1),
                    OZZ_SSE_SPLAT_F(scale, 1));
  res2 = _mm_mul_ps(OZZ_SSE_SELECT_F(mask_00f0, radicand, res2),
                    OZZ_SSE_SPLAT_F(scale, 2));
  __m128 res3 = _mm_mul_ps(OZZ_SSE_SELECT_F(mask_000f, radicand, diff),
                           OZZ_SSE_SPLAT_F(scale, 3));

  const __m128 xx = OZZ_SSE_SPLAT_F(_m.cols[0], 0);
  const __m128 yy = OZZ_SSE_SPLAT_F(_m.cols[1], 1);
  const __m128 zz = OZZ_SSE_SPLAT_F(_m.cols[2], 2);
  const __m128i cond0 = _mm_castps_si128(_mm_cmpgt_ps(yy, xx));
  const __m128i cond1 =
      _mm_castps_si128(_mm_and_ps(_mm_cmpgt_ps(zz, xx), _mm_cmpgt_ps(zz, yy)));
  const __m128i cond2 = _mm_castps_si128(
      _mm_cmpgt_ps(OZZ_SSE_SPLAT_F(diag_sum, 0), _mm_castsi128_ps(zero)));
  __m128 res = OZZ_SSE_SELECT_F(cond0, res1, res0);
  res = OZZ_SSE_SELECT_F(cond1, res2, res);
  res = OZZ_SSE_SELECT_F(cond2, res3, res);

  assert(AreAllTrue1(IsNormalizedEst4(res)));
  return res;
}

OZZ_INLINE bool ToAffine(const Float4x4& _m, SimdFloat4* _translation,
                         SimdFloat4* _quaternion, SimdFloat4* _scale) {
  const __m128 zero = _mm_setzero_ps();
  const __m128 one = simd_float4::one();
  const __m128i fff0 = simd_int4::mask_fff0();
  const __m128 max = _mm_set_ps1(kOrthogonalisationToleranceSq);
  const __m128 min = _mm_set_ps1(-kOrthogonalisationToleranceSq);

  // Extracts translation.
  *_translation = OZZ_SSE_SELECT_F(fff0, _m.cols[3], one);

  // Extracts scale.
  const __m128 m_tmp0 = _mm_unpacklo_ps(_m.cols[0], _m.cols[2]);
  const __m128 m_tmp1 = _mm_unpacklo_ps(_m.cols[1], _m.cols[3]);
  const __m128 m_tmp2 = _mm_unpackhi_ps(_m.cols[0], _m.cols[2]);
  const __m128 m_tmp3 = _mm_unpackhi_ps(_m.cols[1], _m.cols[3]);
  const __m128 m_row0 = _mm_unpacklo_ps(m_tmp0, m_tmp1);
  const __m128 m_row1 = _mm_unpackhi_ps(m_tmp0, m_tmp1);
  const __m128 m_row2 = _mm_unpacklo_ps(m_tmp2, m_tmp3);

  const __m128 dot = _mm_add_ps(
      _mm_add_ps(_mm_mul_ps(m_row0, m_row0), _mm_mul_ps(m_row1, m_row1)),
      _mm_mul_ps(m_row2, m_row2));
  const __m128 abs_scale = _mm_sqrt_ps(dot);

  const __m128 zero_axis =
      _mm_and_ps(_mm_cmplt_ps(dot, max), _mm_cmpgt_ps(dot, min));

  // Builds an orthonormal matrix in order to support quaternion extraction.
  Float4x4 orthonormal;
  int mask = _mm_movemask_ps(zero_axis);
  if (mask & 1) {
    if (mask & 6) {
      return false;
    }
    orthonormal.cols[1] = _mm_div_ps(_m.cols[1], OZZ_SSE_SPLAT_F(abs_scale, 1));
    orthonormal.cols[0] = Normalize3(Cross3(orthonormal.cols[1], _m.cols[2]));
    orthonormal.cols[2] =
        Normalize3(Cross3(orthonormal.cols[0], orthonormal.cols[1]));
  } else if (mask & 4) {
    if (mask & 3) {
      return false;
    }
    orthonormal.cols[0] = _mm_div_ps(_m.cols[0], OZZ_SSE_SPLAT_F(abs_scale, 0));
    orthonormal.cols[2] = Normalize3(Cross3(orthonormal.cols[0], _m.cols[1]));
    orthonormal.cols[1] =
        Normalize3(Cross3(orthonormal.cols[2], orthonormal.cols[0]));
  } else {  // Favor z axis in the default case
    if (mask & 5) {
      return false;
    }
    orthonormal.cols[2] = _mm_div_ps(_m.cols[2], OZZ_SSE_SPLAT_F(abs_scale, 2));
    orthonormal.cols[1] = Normalize3(Cross3(orthonormal.cols[2], _m.cols[0]));
    orthonormal.cols[0] =
        Normalize3(Cross3(orthonormal.cols[1], orthonormal.cols[2]));
  }
  orthonormal.cols[3] = simd_float4::w_axis();

  // Get back scale signs in case of reflexions
  const __m128 o_tmp0 =
      _mm_unpacklo_ps(orthonormal.cols[0], orthonormal.cols[2]);
  const __m128 o_tmp1 =
      _mm_unpacklo_ps(orthonormal.cols[1], orthonormal.cols[3]);
  const __m128 o_tmp2 =
      _mm_unpackhi_ps(orthonormal.cols[0], orthonormal.cols[2]);
  const __m128 o_tmp3 =
      _mm_unpackhi_ps(orthonormal.cols[1], orthonormal.cols[3]);
  const __m128 o_row0 = _mm_unpacklo_ps(o_tmp0, o_tmp1);
  const __m128 o_row1 = _mm_unpackhi_ps(o_tmp0, o_tmp1);
  const __m128 o_row2 = _mm_unpacklo_ps(o_tmp2, o_tmp3);

  const __m128 scale_dot = _mm_add_ps(
      _mm_add_ps(_mm_mul_ps(o_row0, m_row0), _mm_mul_ps(o_row1, m_row1)),
      _mm_mul_ps(o_row2, m_row2));

  const __m128i cond = _mm_castps_si128(_mm_cmpgt_ps(scale_dot, zero));
  const __m128 cfalse = _mm_sub_ps(zero, abs_scale);
  const __m128 scale = OZZ_SSE_SELECT_F(cond, abs_scale, cfalse);
  *_scale = OZZ_SSE_SELECT_F(fff0, scale, one);

  // Extracts quaternion.
  *_quaternion = ToQuaternion(orthonormal);
  return true;
}

OZZ_INLINE Float4x4 Float4x4::FromEuler(_SimdFloat4 _v) {
  const __m128 cos = Cos(_v);
  const __m128 sin = Sin(_v);

  const float cx = GetX(cos);
  const float sx = GetX(sin);
  const float cy = GetY(cos);
  const float sy = GetY(sin);
  const float cz = GetZ(cos);
  const float sz = GetZ(sin);

  const float sycz = sy * cz;
  const float sysz = sy * sz;

  const Float4x4 ret = {{simd_float4::Load(cx * cy, sx * sz - cx * sycz,
                                           cx * sysz + sx * cz, 0.f),
                         simd_float4::Load(sy, cy * cz, -cy * sz, 0.f),
                         simd_float4::Load(-sx * cy, sx * sycz + cx * sz,
                                           -sx * sysz + cx * cz, 0.f),
                         simd_float4::w_axis()}};
  return ret;
}

OZZ_INLINE Float4x4 Float4x4::FromAxisAngle(_SimdFloat4 _v) {
  assert(AreAllTrue1(IsNormalizedEst3(_v)));

  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i ione = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  const __m128 fff0 = _mm_castsi128_ps(_mm_srli_si128(ffff, 4));
  const __m128 one = _mm_castsi128_ps(ione);
  const __m128 w_axis = _mm_castsi128_ps(_mm_slli_si128(ione, 12));

  const __m128 angle = SplatW(_v);
  const __m128 sin = SplatX(SinX(angle));
  const __m128 cos = SplatX(CosX(angle));
  const __m128 one_minus_cos = _mm_sub_ps(one, cos);

  const __m128 v0 =
      _mm_mul_ps(_mm_mul_ps(one_minus_cos,
                            _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(3, 0, 2, 1))),
                 _mm_shuffle_ps(_v, _v, _MM_SHUFFLE(3, 1, 0, 2)));
  const __m128 r0 =
      _mm_add_ps(_mm_mul_ps(_mm_mul_ps(one_minus_cos, _v), _v), cos);
  const __m128 r1 = _mm_add_ps(_mm_mul_ps(sin, _v), v0);
  const __m128 r2 = _mm_sub_ps(v0, _mm_mul_ps(sin, _v));
  const __m128 r0fff0 = _mm_and_ps(r0, fff0);
  const __m128 r1r22120 = _mm_shuffle_ps(r1, r2, _MM_SHUFFLE(2, 1, 2, 0));
  const __m128 v1 = _mm_shuffle_ps(r1r22120, r1r22120, _MM_SHUFFLE(0, 3, 2, 1));
  const __m128 r1r20011 = _mm_shuffle_ps(r1, r2, _MM_SHUFFLE(0, 0, 1, 1));
  const __m128 v2 = _mm_shuffle_ps(r1r20011, r1r20011, _MM_SHUFFLE(2, 0, 2, 0));

  const __m128 t0 = _mm_shuffle_ps(r0fff0, v1, _MM_SHUFFLE(1, 0, 3, 0));
  const __m128 t1 = _mm_shuffle_ps(r0fff0, v1, _MM_SHUFFLE(3, 2, 3, 1));
  const Float4x4 ret = {{_mm_shuffle_ps(t0, t0, _MM_SHUFFLE(1, 3, 2, 0)),
                         _mm_shuffle_ps(t1, t1, _MM_SHUFFLE(1, 3, 0, 2)),
                         _mm_shuffle_ps(v2, r0fff0, _MM_SHUFFLE(3, 2, 1, 0)),
                         w_axis}};
  return ret;
}

OZZ_INLINE Float4x4 Float4x4::FromQuaternion(_SimdFloat4 _quaternion) {
  assert(AreAllTrue1(IsNormalizedEst4(_quaternion)));

  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i ione = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  const __m128 fff0 = _mm_castsi128_ps(_mm_srli_si128(ffff, 4));
  const __m128 c1110 = _mm_castsi128_ps(_mm_srli_si128(ione, 4));
  const __m128 w_axis = _mm_castsi128_ps(_mm_slli_si128(ione, 12));

  const __m128 vsum = _mm_add_ps(_quaternion, _quaternion);
  const __m128 vms = _mm_mul_ps(_quaternion, vsum);

  const __m128 r0 = _mm_sub_ps(
      _mm_sub_ps(
          c1110,
          _mm_and_ps(_mm_shuffle_ps(vms, vms, _MM_SHUFFLE(3, 0, 0, 1)), fff0)),
      _mm_and_ps(_mm_shuffle_ps(vms, vms, _MM_SHUFFLE(3, 1, 2, 2)), fff0));
  const __m128 v0 = _mm_mul_ps(
      _mm_shuffle_ps(_quaternion, _quaternion, _MM_SHUFFLE(3, 1, 0, 0)),
      _mm_shuffle_ps(vsum, vsum, _MM_SHUFFLE(3, 2, 1, 2)));
  const __m128 v1 = _mm_mul_ps(
      _mm_shuffle_ps(_quaternion, _quaternion, _MM_SHUFFLE(3, 3, 3, 3)),
      _mm_shuffle_ps(vsum, vsum, _MM_SHUFFLE(3, 0, 2, 1)));

  const __m128 r1 = _mm_add_ps(v0, v1);
  const __m128 r2 = _mm_sub_ps(v0, v1);

  const __m128 r1r21021 = _mm_shuffle_ps(r1, r2, _MM_SHUFFLE(1, 0, 2, 1));
  const __m128 v2 = _mm_shuffle_ps(r1r21021, r1r21021, _MM_SHUFFLE(1, 3, 2, 0));
  const __m128 r1r22200 = _mm_shuffle_ps(r1, r2, _MM_SHUFFLE(2, 2, 0, 0));
  const __m128 v3 = _mm_shuffle_ps(r1r22200, r1r22200, _MM_SHUFFLE(2, 0, 2, 0));

  const __m128 q0 = _mm_shuffle_ps(r0, v2, _MM_SHUFFLE(1, 0, 3, 0));
  const __m128 q1 = _mm_shuffle_ps(r0, v2, _MM_SHUFFLE(3, 2, 3, 1));
  const Float4x4 ret = {{_mm_shuffle_ps(q0, q0, _MM_SHUFFLE(1, 3, 2, 0)),
                         _mm_shuffle_ps(q1, q1, _MM_SHUFFLE(1, 3, 0, 2)),
                         _mm_shuffle_ps(v3, r0, _MM_SHUFFLE(3, 2, 1, 0)),
                         w_axis}};
  return ret;
}

OZZ_INLINE Float4x4 Float4x4::FromAffine(_SimdFloat4 _translation,
                                         _SimdFloat4 _quaternion,
                                         _SimdFloat4 _scale) {
  assert(AreAllTrue1(IsNormalizedEst4(_quaternion)));

  const __m128i zero = _mm_setzero_si128();
  const __m128i ffff = _mm_cmpeq_epi32(zero, zero);
  const __m128i ione = _mm_srli_epi32(_mm_slli_epi32(ffff, 25), 2);
  const __m128 fff0 = _mm_castsi128_ps(_mm_srli_si128(ffff, 4));
  const __m128 c1110 = _mm_castsi128_ps(_mm_srli_si128(ione, 4));

  const __m128 vsum = _mm_add_ps(_quaternion, _quaternion);
  const __m128 vms = _mm_mul_ps(_quaternion, vsum);

  const __m128 r0 = _mm_sub_ps(
      _mm_sub_ps(
          c1110,
          _mm_and_ps(_mm_shuffle_ps(vms, vms, _MM_SHUFFLE(3, 0, 0, 1)), fff0)),
      _mm_and_ps(_mm_shuffle_ps(vms, vms, _MM_SHUFFLE(3, 1, 2, 2)), fff0));
  const __m128 v0 = _mm_mul_ps(
      _mm_shuffle_ps(_quaternion, _quaternion, _MM_SHUFFLE(3, 1, 0, 0)),
      _mm_shuffle_ps(vsum, vsum, _MM_SHUFFLE(3, 2, 1, 2)));
  const __m128 v1 = _mm_mul_ps(
      _mm_shuffle_ps(_quaternion, _quaternion, _MM_SHUFFLE(3, 3, 3, 3)),
      _mm_shuffle_ps(vsum, vsum, _MM_SHUFFLE(3, 0, 2, 1)));

  const __m128 r1 = _mm_add_ps(v0, v1);
  const __m128 r2 = _mm_sub_ps(v0, v1);

  const __m128 r1r21021 = _mm_shuffle_ps(r1, r2, _MM_SHUFFLE(1, 0, 2, 1));
  const __m128 v2 = _mm_shuffle_ps(r1r21021, r1r21021, _MM_SHUFFLE(1, 3, 2, 0));
  const __m128 r1r22200 = _mm_shuffle_ps(r1, r2, _MM_SHUFFLE(2, 2, 0, 0));
  const __m128 v3 = _mm_shuffle_ps(r1r22200, r1r22200, _MM_SHUFFLE(2, 0, 2, 0));

  const __m128 q0 = _mm_shuffle_ps(r0, v2, _MM_SHUFFLE(1, 0, 3, 0));
  const __m128 q1 = _mm_shuffle_ps(r0, v2, _MM_SHUFFLE(3, 2, 3, 1));

  const Float4x4 ret = {
      {_mm_mul_ps(_mm_shuffle_ps(q0, q0, _MM_SHUFFLE(1, 3, 2, 0)),
                  OZZ_SSE_SPLAT_F(_scale, 0)),
       _mm_mul_ps(_mm_shuffle_ps(q1, q1, _MM_SHUFFLE(1, 3, 0, 2)),
                  OZZ_SSE_SPLAT_F(_scale, 1)),
       _mm_mul_ps(_mm_shuffle_ps(v3, r0, _MM_SHUFFLE(3, 2, 1, 0)),
                  OZZ_SSE_SPLAT_F(_scale, 2)),
       _mm_movelh_ps(_translation, _mm_unpackhi_ps(_translation, c1110))}};
  return ret;
}

OZZ_INLINE ozz::math::SimdFloat4 TransformPoint(const ozz::math::Float4x4& _m,
                                                ozz::math::_SimdFloat4 _v) {
  const __m128 vxxxx = OZZ_SSE_SPLAT_F(_v, 0);
  const __m128 vyyyy = OZZ_SSE_SPLAT_F(_v, 1);
  const __m128 vzzzz = OZZ_SSE_SPLAT_F(_v, 2);
  const __m128 m0 = _mm_mul_ps(_m.cols[0], vxxxx);
  const __m128 m1 = _mm_mul_ps(_m.cols[1], vyyyy);
  const __m128 m2 = _mm_mul_ps(_m.cols[2], vzzzz);
  const __m128 a01 = _mm_add_ps(m0, m1);
  const __m128 a23 = _mm_add_ps(m2, _m.cols[3]);
  return _mm_add_ps(a01, a23);
}

OZZ_INLINE ozz::math::SimdFloat4 TransformVector(const ozz::math::Float4x4& _m,
                                                 ozz::math::_SimdFloat4 _v) {
  const __m128 vxxxx = OZZ_SSE_SPLAT_F(_v, 0);
  const __m128 vyyyy = OZZ_SSE_SPLAT_F(_v, 1);
  const __m128 vzzzz = OZZ_SSE_SPLAT_F(_v, 2);
  const __m128 m0 = _mm_mul_ps(_m.cols[0], vxxxx);
  const __m128 m1 = _mm_mul_ps(_m.cols[1], vyyyy);
  const __m128 m2 = _mm_mul_ps(_m.cols[2], vzzzz);
  const __m128 a01 = _mm_add_ps(m0, m1);
  return _mm_add_ps(a01, m2);
}
}  // namespace math
}  // namespace ozz

#if !defined(__GNUC__)
OZZ_INLINE ozz::math::SimdFloat4 operator+(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b) {
  return _mm_add_ps(_a, _b);
}

OZZ_INLINE ozz::math::SimdFloat4 operator-(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b) {
  return _mm_sub_ps(_a, _b);
}

OZZ_INLINE ozz::math::SimdFloat4 operator-(ozz::math::_SimdFloat4 _v) {
  return _mm_sub_ps(_mm_setzero_ps(), _v);
}

OZZ_INLINE ozz::math::SimdFloat4 operator*(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b) {
  return _mm_mul_ps(_a, _b);
}

OZZ_INLINE ozz::math::SimdFloat4 operator/(ozz::math::_SimdFloat4 _a,
                                           ozz::math::_SimdFloat4 _b) {
  return _mm_div_ps(_a, _b);
}
#endif  // !defined(__GNUC__)

OZZ_INLINE ozz::math::SimdFloat4 operator*(const ozz::math::Float4x4& _m,
                                           ozz::math::_SimdFloat4 _v) {
  const __m128 vxxxx = OZZ_SSE_SPLAT_F(_v, 0);
  const __m128 vyyyy = OZZ_SSE_SPLAT_F(_v, 1);
  const __m128 m0 = _mm_mul_ps(_m.cols[0], vxxxx);
  const __m128 m1 = _mm_mul_ps(_m.cols[1], vyyyy);
  const __m128 vzzzz = OZZ_SSE_SPLAT_F(_v, 2);
  const __m128 vwwww = OZZ_SSE_SPLAT_F(_v, 3);
  const __m128 a01 = _mm_add_ps(m0, m1);
  const __m128 m2 = _mm_mul_ps(_m.cols[2], vzzzz);
  const __m128 m3 = _mm_mul_ps(_m.cols[3], vwwww);
  const __m128 a012 = _mm_add_ps(a01, m2);
  return _mm_add_ps(a012, m3);
}

OZZ_INLINE ozz::math::Float4x4 operator*(const ozz::math::Float4x4& _a,
                                         const ozz::math::Float4x4& _b) {
  ozz::math::Float4x4 ret;
  {
    const __m128 vxxxx = OZZ_SSE_SPLAT_F(_b.cols[0], 0);
    const __m128 vyyyy = OZZ_SSE_SPLAT_F(_b.cols[0], 1);
    const __m128 m0 = _mm_mul_ps(_a.cols[0], vxxxx);
    const __m128 m1 = _mm_mul_ps(_a.cols[1], vyyyy);
    const __m128 vzzzz = OZZ_SSE_SPLAT_F(_b.cols[0], 2);
    const __m128 vwwww = OZZ_SSE_SPLAT_F(_b.cols[0], 3);
    const __m128 a01 = _mm_add_ps(m0, m1);
    const __m128 m2 = _mm_mul_ps(_a.cols[2], vzzzz);
    const __m128 m3 = _mm_mul_ps(_a.cols[3], vwwww);
    const __m128 a012 = _mm_add_ps(a01, m2);
    ret.cols[0] = _mm_add_ps(a012, m3);
  }
  {
    const __m128 vxxxx = OZZ_SSE_SPLAT_F(_b.cols[1], 0);
    const __m128 vyyyy = OZZ_SSE_SPLAT_F(_b.cols[1], 1);
    const __m128 m0 = _mm_mul_ps(_a.cols[0], vxxxx);
    const __m128 m1 = _mm_mul_ps(_a.cols[1], vyyyy);
    const __m128 vzzzz = OZZ_SSE_SPLAT_F(_b.cols[1], 2);
    const __m128 vwwww = OZZ_SSE_SPLAT_F(_b.cols[1], 3);
    const __m128 a01 = _mm_add_ps(m0, m1);
    const __m128 m2 = _mm_mul_ps(_a.cols[2], vzzzz);
    const __m128 m3 = _mm_mul_ps(_a.cols[3], vwwww);
    const __m128 a012 = _mm_add_ps(a01, m2);
    ret.cols[1] = _mm_add_ps(a012, m3);
  }
  {
    const __m128 vxxxx = OZZ_SSE_SPLAT_F(_b.cols[2], 0);
    const __m128 vyyyy = OZZ_SSE_SPLAT_F(_b.cols[2], 1);
    const __m128 m0 = _mm_mul_ps(_a.cols[0], vxxxx);
    const __m128 m1 = _mm_mul_ps(_a.cols[1], vyyyy);
    const __m128 vzzzz = OZZ_SSE_SPLAT_F(_b.cols[2], 2);
    const __m128 vwwww = OZZ_SSE_SPLAT_F(_b.cols[2], 3);
    const __m128 a01 = _mm_add_ps(m0, m1);
    const __m128 m2 = _mm_mul_ps(_a.cols[2], vzzzz);
    const __m128 m3 = _mm_mul_ps(_a.cols[3], vwwww);
    const __m128 a012 = _mm_add_ps(a01, m2);
    ret.cols[2] = _mm_add_ps(a012, m3);
  }
  {
    const __m128 vxxxx = OZZ_SSE_SPLAT_F(_b.cols[3], 0);
    const __m128 vyyyy = OZZ_SSE_SPLAT_F(_b.cols[3], 1);
    const __m128 m0 = _mm_mul_ps(_a.cols[0], vxxxx);
    const __m128 m1 = _mm_mul_ps(_a.cols[1], vyyyy);
    const __m128 vzzzz = OZZ_SSE_SPLAT_F(_b.cols[3], 2);
    const __m128 vwwww = OZZ_SSE_SPLAT_F(_b.cols[3], 3);
    const __m128 a01 = _mm_add_ps(m0, m1);
    const __m128 m2 = _mm_mul_ps(_a.cols[2], vzzzz);
    const __m128 m3 = _mm_mul_ps(_a.cols[3], vwwww);
    const __m128 a012 = _mm_add_ps(a01, m2);
    ret.cols[3] = _mm_add_ps(a012, m3);
  }
  return ret;
}

OZZ_INLINE ozz::math::Float4x4 operator+(const ozz::math::Float4x4& _a,
                                         const ozz::math::Float4x4& _b) {
  const ozz::math::Float4x4 ret = {
      {_mm_add_ps(_a.cols[0], _b.cols[0]), _mm_add_ps(_a.cols[1], _b.cols[1]),
       _mm_add_ps(_a.cols[2], _b.cols[2]), _mm_add_ps(_a.cols[3], _b.cols[3])}};
  return ret;
}

OZZ_INLINE ozz::math::Float4x4 operator-(const ozz::math::Float4x4& _a,
                                         const ozz::math::Float4x4& _b) {
  const ozz::math::Float4x4 ret = {
      {_mm_sub_ps(_a.cols[0], _b.cols[0]), _mm_sub_ps(_a.cols[1], _b.cols[1]),
       _mm_sub_ps(_a.cols[2], _b.cols[2]), _mm_sub_ps(_a.cols[3], _b.cols[3])}};
  return ret;
}

namespace ozz {
namespace math {
OZZ_INLINE uint16_t FloatToHalf(float _f) {
  const int h = _mm_cvtsi128_si32(FloatToHalf(_mm_set1_ps(_f)));
  return static_cast<uint16_t>(h);
}

OZZ_INLINE float HalfToFloat(uint16_t _h) {
  return _mm_cvtss_f32(HalfToFloat(_mm_set1_epi32(_h)));
}

// Half <-> Float implementation is based on:
// http://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/.
OZZ_INLINE SimdInt4 FloatToHalf(_SimdFloat4 _f) {
  const __m128i mask_sign = _mm_set1_epi32(0x80000000u);
  const __m128i mask_round = _mm_set1_epi32(~0xfffu);
  const __m128i f32infty = _mm_set1_epi32(255 << 23);
  const __m128 magic = _mm_castsi128_ps(_mm_set1_epi32(15 << 23));
  const __m128i nanbit = _mm_set1_epi32(0x200);
  const __m128i infty_as_fp16 = _mm_set1_epi32(0x7c00);
  const __m128 clamp = _mm_castsi128_ps(_mm_set1_epi32((31 << 23) - 0x1000));

  const __m128 msign = _mm_castsi128_ps(mask_sign);
  const __m128 justsign = _mm_and_ps(msign, _f);
  const __m128 absf = _mm_xor_ps(_f, justsign);
  const __m128 mround = _mm_castsi128_ps(mask_round);
  const __m128i absf_int = _mm_castps_si128(absf);
  const __m128i b_isnan = _mm_cmpgt_epi32(absf_int, f32infty);
  const __m128i b_isnormal = _mm_cmpgt_epi32(f32infty, _mm_castps_si128(absf));
  const __m128i inf_or_nan =
      _mm_or_si128(_mm_and_si128(b_isnan, nanbit), infty_as_fp16);
  const __m128 fnosticky = _mm_and_ps(absf, mround);
  const __m128 scaled = _mm_mul_ps(fnosticky, magic);
  // Logically, we want PMINSD on "biased", but this should gen better code
  const __m128 clamped = _mm_min_ps(scaled, clamp);
  const __m128i biased =
      _mm_sub_epi32(_mm_castps_si128(clamped), _mm_castps_si128(mround));
  const __m128i shifted = _mm_srli_epi32(biased, 13);
  const __m128i normal = _mm_and_si128(shifted, b_isnormal);
  const __m128i not_normal = _mm_andnot_si128(b_isnormal, inf_or_nan);
  const __m128i joined = _mm_or_si128(normal, not_normal);

  const __m128i sign_shift = _mm_srli_epi32(_mm_castps_si128(justsign), 16);
  return _mm_or_si128(joined, sign_shift);
}

OZZ_INLINE SimdFloat4 HalfToFloat(_SimdInt4 _h) {
  const __m128i mask_nosign = _mm_set1_epi32(0x7fff);
  const __m128 magic = _mm_castsi128_ps(_mm_set1_epi32((254 - 15) << 23));
  const __m128i was_infnan = _mm_set1_epi32(0x7bff);
  const __m128 exp_infnan = _mm_castsi128_ps(_mm_set1_epi32(255 << 23));

  const __m128i expmant = _mm_and_si128(mask_nosign, _h);
  const __m128i shifted = _mm_slli_epi32(expmant, 13);
  const __m128 scaled = _mm_mul_ps(_mm_castsi128_ps(shifted), magic);
  const __m128i b_wasinfnan = _mm_cmpgt_epi32(expmant, was_infnan);
  const __m128i sign = _mm_slli_epi32(_mm_xor_si128(_h, expmant), 16);
  const __m128 infnanexp =
      _mm_and_ps(_mm_castsi128_ps(b_wasinfnan), exp_infnan);
  const __m128 sign_inf = _mm_or_ps(_mm_castsi128_ps(sign), infnanexp);
  return _mm_or_ps(scaled, sign_inf);
}
}  // namespace math
}  // namespace ozz

#undef OZZ_SSE_SPLAT_F
#undef OZZ_SSE_HADD2_F
#undef OZZ_SSE_HADD3_F
#undef OZZ_SSE_HADD4_F
#undef OZZ_SSE_DOT2_F
#undef OZZ_SSE_DOT3_F
#undef OZZ_SSE_DOT4_F
#undef OZZ_SSE_SELECT_F
#undef OZZ_SSE_SPLAT_I
#undef OZZ_SSE_SELECT_I
#endif  // OZZ_OZZ_BASE_MATHS_INTERNAL_SIMD_MATH_SSE_INL_H_
