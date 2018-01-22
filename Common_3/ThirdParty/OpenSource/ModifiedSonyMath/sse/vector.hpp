/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VECTORMATH_SSE_VECTOR_HPP
#define VECTORMATH_SSE_VECTOR_HPP

namespace Vectormath
{
namespace SSE
{

// ========================================================
// VecIdx
// ========================================================

#ifdef VECTORMATH_NO_SCALAR_CAST
inline VecIdx::operator FloatInVec() const
{
    return FloatInVec(ref, i);
}
inline float VecIdx::getAsFloat() const
#else
inline VecIdx::operator float() const
#endif
{
    return ((float *)&ref)[i];
}

inline float VecIdx::operator = (float scalar)
{
    sseVecSetElement(ref, scalar, i);
    return scalar;
}

inline FloatInVec VecIdx::operator = (const FloatInVec & scalar)
{
    ref = sseVecInsert(ref, scalar.get128(), i);
    return scalar;
}

inline FloatInVec VecIdx::operator = (const VecIdx & scalar)
{
    return *this = FloatInVec(scalar.ref, scalar.i);
}

inline FloatInVec VecIdx::operator *= (float scalar)
{
    return *this *= FloatInVec(scalar);
}

inline FloatInVec VecIdx::operator *= (const FloatInVec & scalar)
{
    return *this = FloatInVec(ref, i) * scalar;
}

inline FloatInVec VecIdx::operator /= (float scalar)
{
    return *this /= FloatInVec(scalar);
}

inline FloatInVec VecIdx::operator /= (const FloatInVec & scalar)
{
    return *this = FloatInVec(ref, i) / scalar;
}

inline FloatInVec VecIdx::operator += (float scalar)
{
    return *this += FloatInVec(scalar);
}

inline FloatInVec VecIdx::operator += (const FloatInVec & scalar)
{
    return *this = FloatInVec(ref, i) + scalar;
}

inline FloatInVec VecIdx::operator -= (float scalar)
{
    return *this -= FloatInVec(scalar);
}

inline FloatInVec VecIdx::operator -= (const FloatInVec & scalar)
{
    return *this = FloatInVec(ref, i) - scalar;
}

// ========================================================
// Vector3
// ========================================================

inline Vector3::Vector3(float _x, float _y, float _z)
{
    mVec128 = _mm_setr_ps(_x, _y, _z, 0.0f);
}

inline Vector3::Vector3(const FloatInVec & _x, const FloatInVec & _y, const FloatInVec & _z)
{
    const __m128 xz = _mm_unpacklo_ps(_x.get128(), _z.get128());
    mVec128 = _mm_unpacklo_ps(xz, _y.get128());
}

inline Vector3::Vector3(const Point3 & pnt)
{
    mVec128 = pnt.get128();
}

inline Vector3::Vector3(float scalar)
{
    mVec128 = FloatInVec(scalar).get128();
}

inline Vector3::Vector3(const FloatInVec & scalar)
{
    mVec128 = scalar.get128();
}

inline Vector3::Vector3(__m128 vf4)
{
    mVec128 = vf4;
}

inline const Vector3 Vector3::xAxis()
{
    return Vector3(sseUnitVec1000());
}

inline const Vector3 Vector3::yAxis()
{
    return Vector3(sseUnitVec0100());
}

inline const Vector3 Vector3::zAxis()
{
    return Vector3(sseUnitVec0010());
}

inline const Vector3 lerp(float t, const Vector3 & vec0, const Vector3 & vec1)
{
    return lerp(FloatInVec(t), vec0, vec1);
}

inline const Vector3 lerp(const FloatInVec & t, const Vector3 & vec0, const Vector3 & vec1)
{
    return (vec0 + ((vec1 - vec0) * t));
}

inline const Vector3 slerp(float t, const Vector3 & unitVec0, const Vector3 & unitVec1)
{
    return slerp(FloatInVec(t), unitVec0, unitVec1);
}

inline const Vector3 slerp(const FloatInVec & t, const Vector3 & unitVec0, const Vector3 & unitVec1)
{
    __m128 scales, scale0, scale1, cosAngle, angle, tttt, oneMinusT, angles, sines;
    cosAngle = sseVecDot3(unitVec0.get128(), unitVec1.get128());
    __m128 selectMask = _mm_cmpgt_ps(_mm_set1_ps(VECTORMATH_SLERP_TOL), cosAngle);
    angle = sseACosf(cosAngle);
    tttt = t.get128();
    oneMinusT = _mm_sub_ps(_mm_set1_ps(1.0f), tttt);
    angles = _mm_unpacklo_ps(_mm_set1_ps(1.0f), tttt); // angles = 1, t, 1, t
    angles = _mm_unpacklo_ps(angles, oneMinusT);       // angles = 1, 1-t, t, 1-t
    angles = _mm_mul_ps(angles, angle);
    sines = sseSinf(angles);
    scales = _mm_div_ps(sines, sseSplat(sines, 0));
    scale0 = sseSelect(oneMinusT, sseSplat(scales, 1), selectMask);
    scale1 = sseSelect(tttt, sseSplat(scales, 2), selectMask);
    return Vector3(sseMAdd(unitVec0.get128(), scale0, _mm_mul_ps(unitVec1.get128(), scale1)));
}

inline __m128 Vector3::get128() const
{
    return mVec128;
}

inline void storeXYZ(const Vector3 & vec, __m128 * quad)
{
    __m128 dstVec = *quad;
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    dstVec = sseSelect(vec.get128(), dstVec, sw);
    *quad = dstVec;
}

inline void loadXYZArray(Vector3 & vec0, Vector3 & vec1, Vector3 & vec2, Vector3 & vec3, const __m128 * threeQuads)
{
    const float * quads = (const float *)threeQuads;
    vec0 = Vector3(_mm_load_ps(quads));
    vec1 = Vector3(_mm_loadu_ps(quads + 3));
    vec2 = Vector3(_mm_loadu_ps(quads + 6));
    vec3 = Vector3(_mm_loadu_ps(quads + 9));
}

inline void storeXYZArray(const Vector3 & vec0, const Vector3 & vec1, const Vector3 & vec2, const Vector3 & vec3, __m128 * threeQuads)
{
    __m128 xxxx = _mm_shuffle_ps(vec1.get128(), vec1.get128(), _MM_SHUFFLE(0, 0, 0, 0));
    __m128 zzzz = _mm_shuffle_ps(vec2.get128(), vec2.get128(), _MM_SHUFFLE(2, 2, 2, 2));
    VECTORMATH_ALIGNED(unsigned int xsw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    VECTORMATH_ALIGNED(unsigned int zsw[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    threeQuads[0] = sseSelect(vec0.get128(), xxxx, xsw);
    threeQuads[1] = _mm_shuffle_ps(vec1.get128(), vec2.get128(), _MM_SHUFFLE(1, 0, 2, 1));
    threeQuads[2] = sseSelect(_mm_shuffle_ps(vec3.get128(), vec3.get128(), _MM_SHUFFLE(2, 1, 0, 3)), zzzz, zsw);
}

inline Vector3 & Vector3::operator = (const Vector3 & vec)
{
    mVec128 = vec.mVec128;
    return *this;
}

inline Vector3 & Vector3::setX(float _x)
{
    sseVecSetElement(mVec128, _x, 0);
    return *this;
}

inline Vector3 & Vector3::setX(const FloatInVec & _x)
{
    mVec128 = sseVecInsert(mVec128, _x.get128(), 0);
    return *this;
}

inline const FloatInVec Vector3::getX() const
{
    return FloatInVec(mVec128, 0);
}

inline Vector3 & Vector3::setY(float _y)
{
    sseVecSetElement(mVec128, _y, 1);
    return *this;
}

inline Vector3 & Vector3::setY(const FloatInVec & _y)
{
    mVec128 = sseVecInsert(mVec128, _y.get128(), 1);
    return *this;
}

inline const FloatInVec Vector3::getY() const
{
    return FloatInVec(mVec128, 1);
}

inline Vector3 & Vector3::setZ(float _z)
{
    sseVecSetElement(mVec128, _z, 2);
    return *this;
}

inline Vector3 & Vector3::setZ(const FloatInVec & _z)
{
    mVec128 = sseVecInsert(mVec128, _z.get128(), 2);
    return *this;
}

inline const FloatInVec Vector3::getZ() const
{
    return FloatInVec(mVec128, 2);
}

inline Vector3 & Vector3::setW(float _w)
{
    sseVecSetElement(mVec128, _w, 3);
    return *this;
}

inline Vector3 & Vector3::setW(const FloatInVec & _w)
{
    mVec128 = sseVecInsert(mVec128, _w.get128(), 3);
    return *this;
}

inline const FloatInVec Vector3::getW() const
{
    return FloatInVec(mVec128, 3);
}

inline Vector3 & Vector3::setElem(int idx, float value)
{
    sseVecSetElement(mVec128, value, idx);
    return *this;
}

inline Vector3 & Vector3::setElem(int idx, const FloatInVec & value)
{
    mVec128 = sseVecInsert(mVec128, value.get128(), idx);
    return *this;
}

inline const FloatInVec Vector3::getElem(int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline VecIdx Vector3::operator[](int idx)
{
    return VecIdx(mVec128, idx);
}

inline const FloatInVec Vector3::operator[](int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline const Vector3 Vector3::operator + (const Vector3 & vec) const
{
    return Vector3(_mm_add_ps(mVec128, vec.mVec128));
}

inline const Vector3 Vector3::operator - (const Vector3 & vec) const
{
    return Vector3(_mm_sub_ps(mVec128, vec.mVec128));
}

inline const Point3 Vector3::operator + (const Point3 & pnt) const
{
    return Point3(_mm_add_ps(mVec128, pnt.get128()));
}

inline const Vector3 Vector3::operator * (float scalar) const
{
    return *this * FloatInVec(scalar);
}

inline const Vector3 Vector3::operator * (const FloatInVec & scalar) const
{
    return Vector3(_mm_mul_ps(mVec128, scalar.get128()));
}

inline Vector3 & Vector3::operator += (const Vector3 & vec)
{
    *this = *this + vec;
    return *this;
}

inline Vector3 & Vector3::operator -= (const Vector3 & vec)
{
    *this = *this - vec;
    return *this;
}

inline Vector3 & Vector3::operator *= (float scalar)
{
    *this = *this * scalar;
    return *this;
}

inline Vector3 & Vector3::operator *= (const FloatInVec & scalar)
{
    *this = *this * scalar;
    return *this;
}

inline const Vector3 Vector3::operator / (float scalar) const
{
    return *this / FloatInVec(scalar);
}

inline const Vector3 Vector3::operator / (const FloatInVec & scalar) const
{
    return Vector3(_mm_div_ps(mVec128, scalar.get128()));
}

inline Vector3 & Vector3::operator /= (float scalar)
{
    *this = *this / scalar;
    return *this;
}

inline Vector3 & Vector3::operator /= (const FloatInVec & scalar)
{
    *this = *this / scalar;
    return *this;
}

inline const Vector3 Vector3::operator - () const
{
    return Vector3(_mm_sub_ps(_mm_setzero_ps(), mVec128));
}

inline const Vector3 operator * (float scalar, const Vector3 & vec)
{
    return FloatInVec(scalar) * vec;
}

inline const Vector3 operator * (const FloatInVec & scalar, const Vector3 & vec)
{
    return vec * scalar;
}

inline const Vector3 mulPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
    return Vector3(_mm_mul_ps(vec0.get128(), vec1.get128()));
}

inline const Vector3 divPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
    return Vector3(_mm_div_ps(vec0.get128(), vec1.get128()));
}

inline const Vector3 recipPerElem(const Vector3 & vec)
{
    return Vector3(_mm_rcp_ps(vec.get128()));
}

inline const Vector3 absPerElem(const Vector3 & vec)
{
    return Vector3(sseFabsf(vec.get128()));
}

inline const Vector3 copySignPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
    const __m128 vmask = sseUintToM128(0x7FFFFFFF);
    return Vector3(_mm_or_ps(
        _mm_and_ps(vmask, vec0.get128()),      // Value
        _mm_andnot_ps(vmask, vec1.get128()))); // Signs
}

inline const Vector3 maxPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
    return Vector3(_mm_max_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec maxElem(const Vector3 & vec)
{
    return FloatInVec(_mm_max_ps(_mm_max_ps(sseSplat(vec.get128(), 0), sseSplat(vec.get128(), 1)), sseSplat(vec.get128(), 2)));
}

inline const Vector3 minPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
    return Vector3(_mm_min_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec minElem(const Vector3 & vec)
{
    return FloatInVec(_mm_min_ps(_mm_min_ps(sseSplat(vec.get128(), 0), sseSplat(vec.get128(), 1)), sseSplat(vec.get128(), 2)));
}

inline const FloatInVec sum(const Vector3 & vec)
{
    return FloatInVec(_mm_add_ps(_mm_add_ps(sseSplat(vec.get128(), 0), sseSplat(vec.get128(), 1)), sseSplat(vec.get128(), 2)));
}

inline const FloatInVec dot(const Vector3 & vec0, const Vector3 & vec1)
{
    return FloatInVec(sseVecDot3(vec0.get128(), vec1.get128()), 0);
}

inline const FloatInVec lengthSqr(const Vector3 & vec)
{
    return FloatInVec(sseVecDot3(vec.get128(), vec.get128()), 0);
}

inline const FloatInVec length(const Vector3 & vec)
{
    return FloatInVec(_mm_sqrt_ps(sseVecDot3(vec.get128(), vec.get128())), 0);
}

inline const Vector3 normalizeApprox(const Vector3 & vec)
{
    return Vector3(_mm_mul_ps(vec.get128(), _mm_rsqrt_ps(sseVecDot3(vec.get128(), vec.get128()))));
}

inline const Vector3 normalize(const Vector3 & vec)
{
    return Vector3(_mm_mul_ps(vec.get128(), sseNewtonrapsonRSqrtf(sseVecDot3(vec.get128(), vec.get128()))));
}

inline const Vector3 cross(const Vector3 & vec0, const Vector3 & vec1)
{
    return Vector3(sseVecCross(vec0.get128(), vec1.get128()));
}

inline const Vector3 select(const Vector3 & vec0, const Vector3 & vec1, bool select1)
{
    return select(vec0, vec1, BoolInVec(select1));
}

inline const Vector3 select(const Vector3 & vec0, const Vector3 & vec1, const BoolInVec & select1)
{
    return Vector3(sseSelect(vec0.get128(), vec1.get128(), select1.get128()));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Vector3 & vec)
{
    SSEFloat tmp;
    tmp.m128 = vec.get128();
    std::printf("( %f %f %f )\n", tmp.f[0], tmp.f[1], tmp.f[2]);
}

inline void print(const Vector3 & vec, const char * name)
{
    SSEFloat tmp;
    tmp.m128 = vec.get128();
    std::printf("%s: ( %f %f %f )\n", name, tmp.f[0], tmp.f[1], tmp.f[2]);
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Vector4
// ========================================================

inline Vector4::Vector4(float _x, float _y, float _z, float _w)
{
    mVec128 = _mm_setr_ps(_x, _y, _z, _w);
}

inline Vector4::Vector4(const FloatInVec & _x, const FloatInVec & _y, const FloatInVec & _z, const FloatInVec & _w)
{
    mVec128 = _mm_unpacklo_ps(
        _mm_unpacklo_ps(_x.get128(), _z.get128()),
        _mm_unpacklo_ps(_y.get128(), _w.get128()));
}

inline Vector4::Vector4(const Vector3 & xyz, float _w)
{
    mVec128 = xyz.get128();
    sseVecSetElement(mVec128, _w, 3);
}

inline Vector4::Vector4(const Vector3 & xyz, const FloatInVec & _w)
{
    mVec128 = xyz.get128();
    mVec128 = sseVecInsert(mVec128, _w.get128(), 3);
}

inline Vector4::Vector4(const Vector3 & vec)
{
    mVec128 = vec.get128();
    mVec128 = sseVecInsert(mVec128, _mm_setzero_ps(), 3);
}

inline Vector4::Vector4(const Point3 & pnt)
{
    mVec128 = pnt.get128();
    mVec128 = sseVecInsert(mVec128, _mm_set1_ps(1.0f), 3);
}

inline Vector4::Vector4(const Quat & quat)
{
    mVec128 = quat.get128();
}

inline Vector4::Vector4(float scalar)
{
    mVec128 = FloatInVec(scalar).get128();
}

inline Vector4::Vector4(const FloatInVec & scalar)
{
    mVec128 = scalar.get128();
}

inline Vector4::Vector4(__m128 vf4)
{
    mVec128 = vf4;
}

inline const Vector4 Vector4::xAxis()
{
    return Vector4(sseUnitVec1000());
}

inline const Vector4 Vector4::yAxis()
{
    return Vector4(sseUnitVec0100());
}

inline const Vector4 Vector4::zAxis()
{
    return Vector4(sseUnitVec0010());
}

inline const Vector4 Vector4::wAxis()
{
    return Vector4(sseUnitVec0001());
}

inline const Vector4 lerp(float t, const Vector4 & vec0, const Vector4 & vec1)
{
    return lerp(FloatInVec(t), vec0, vec1);
}

inline const Vector4 lerp(const FloatInVec & t, const Vector4 & vec0, const Vector4 & vec1)
{
    return (vec0 + ((vec1 - vec0) * t));
}

inline const Vector4 slerp(float t, const Vector4 & unitVec0, const Vector4 & unitVec1)
{
    return slerp(FloatInVec(t), unitVec0, unitVec1);
}

inline const Vector4 slerp(const FloatInVec & t, const Vector4 & unitVec0, const Vector4 & unitVec1)
{
    __m128 scales, scale0, scale1, cosAngle, angle, tttt, oneMinusT, angles, sines;
    cosAngle = sseVecDot4(unitVec0.get128(), unitVec1.get128());
    __m128 selectMask = _mm_cmpgt_ps(_mm_set1_ps(VECTORMATH_SLERP_TOL), cosAngle);
    angle = sseACosf(cosAngle);
    tttt = t.get128();
    oneMinusT = _mm_sub_ps(_mm_set1_ps(1.0f), tttt);
    angles = _mm_unpacklo_ps(_mm_set1_ps(1.0f), tttt); // angles = 1, t, 1, t
    angles = _mm_unpacklo_ps(angles, oneMinusT);       // angles = 1, 1-t, t, 1-t
    angles = _mm_mul_ps(angles, angle);
    sines = sseSinf(angles);
    scales = _mm_div_ps(sines, sseSplat(sines, 0));
    scale0 = sseSelect(oneMinusT, sseSplat(scales, 1), selectMask);
    scale1 = sseSelect(tttt, sseSplat(scales, 2), selectMask);
    return Vector4(sseMAdd(unitVec0.get128(), scale0, _mm_mul_ps(unitVec1.get128(), scale1)));
}

inline __m128 Vector4::get128() const
{
    return mVec128;
}

inline Vector4 & Vector4::operator = (const Vector4 & vec)
{
    mVec128 = vec.mVec128;
    return *this;
}

inline Vector4 & Vector4::setXYZ(const Vector3 & vec)
{
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    mVec128 = sseSelect(vec.get128(), mVec128, sw);
    return *this;
}

inline const Vector3 Vector4::getXYZ() const
{
    return Vector3(mVec128);
}

inline Vector4 & Vector4::setX(float _x)
{
    sseVecSetElement(mVec128, _x, 0);
    return *this;
}

inline Vector4 & Vector4::setX(const FloatInVec & _x)
{
    mVec128 = sseVecInsert(mVec128, _x.get128(), 0);
    return *this;
}

inline const FloatInVec Vector4::getX() const
{
    return FloatInVec(mVec128, 0);
}

inline Vector4 & Vector4::setY(float _y)
{
    sseVecSetElement(mVec128, _y, 1);
    return *this;
}

inline Vector4 & Vector4::setY(const FloatInVec & _y)
{
    mVec128 = sseVecInsert(mVec128, _y.get128(), 1);
    return *this;
}

inline const FloatInVec Vector4::getY() const
{
    return FloatInVec(mVec128, 1);
}

inline Vector4 & Vector4::setZ(float _z)
{
    sseVecSetElement(mVec128, _z, 2);
    return *this;
}

inline Vector4 & Vector4::setZ(const FloatInVec & _z)
{
    mVec128 = sseVecInsert(mVec128, _z.get128(), 2);
    return *this;
}

inline const FloatInVec Vector4::getZ() const
{
    return FloatInVec(mVec128, 2);
}

inline Vector4 & Vector4::setW(float _w)
{
    sseVecSetElement(mVec128, _w, 3);
    return *this;
}

inline Vector4 & Vector4::setW(const FloatInVec & _w)
{
    mVec128 = sseVecInsert(mVec128, _w.get128(), 3);
    return *this;
}

inline const FloatInVec Vector4::getW() const
{
    return FloatInVec(mVec128, 3);
}

inline Vector4 & Vector4::setElem(int idx, float value)
{
    sseVecSetElement(mVec128, value, idx);
    return *this;
}

inline Vector4 & Vector4::setElem(int idx, const FloatInVec & value)
{
    mVec128 = sseVecInsert(mVec128, value.get128(), idx);
    return *this;
}

inline const FloatInVec Vector4::getElem(int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline VecIdx Vector4::operator[](int idx)
{
    return VecIdx(mVec128, idx);
}

inline const FloatInVec Vector4::operator[](int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline const Vector4 Vector4::operator + (const Vector4 & vec) const
{
    return Vector4(_mm_add_ps(mVec128, vec.mVec128));
}

inline const Vector4 Vector4::operator - (const Vector4 & vec) const
{
    return Vector4(_mm_sub_ps(mVec128, vec.mVec128));
}

inline const Vector4 Vector4::operator * (float scalar) const
{
    return *this * FloatInVec(scalar);
}

inline const Vector4 Vector4::operator * (const FloatInVec & scalar) const
{
    return Vector4(_mm_mul_ps(mVec128, scalar.get128()));
}

inline Vector4 & Vector4::operator += (const Vector4 & vec)
{
    *this = *this + vec;
    return *this;
}

inline Vector4 & Vector4::operator -= (const Vector4 & vec)
{
    *this = *this - vec;
    return *this;
}

inline Vector4 & Vector4::operator *= (float scalar)
{
    *this = *this * scalar;
    return *this;
}

inline Vector4 & Vector4::operator *= (const FloatInVec & scalar)
{
    *this = *this * scalar;
    return *this;
}

inline const Vector4 Vector4::operator / (float scalar) const
{
    return *this / FloatInVec(scalar);
}

inline const Vector4 Vector4::operator / (const FloatInVec & scalar) const
{
    return Vector4(_mm_div_ps(mVec128, scalar.get128()));
}

inline Vector4 & Vector4::operator /= (float scalar)
{
    *this = *this / scalar;
    return *this;
}

inline Vector4 & Vector4::operator /= (const FloatInVec & scalar)
{
    *this = *this / scalar;
    return *this;
}

inline const Vector4 Vector4::operator - () const
{
    return Vector4(_mm_sub_ps(_mm_setzero_ps(), mVec128));
}

inline const Vector4 operator * (float scalar, const Vector4 & vec)
{
    return FloatInVec(scalar) * vec;
}

inline const Vector4 operator * (const FloatInVec & scalar, const Vector4 & vec)
{
    return vec * scalar;
}

inline const Vector4 mulPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
    return Vector4(_mm_mul_ps(vec0.get128(), vec1.get128()));
}

inline const Vector4 divPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
    return Vector4(_mm_div_ps(vec0.get128(), vec1.get128()));
}

inline const Vector4 recipPerElem(const Vector4 & vec)
{
    return Vector4(_mm_rcp_ps(vec.get128()));
}

inline const Vector4 absPerElem(const Vector4 & vec)
{
    return Vector4(sseFabsf(vec.get128()));
}

inline const Vector4 copySignPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
    const __m128 vmask = sseUintToM128(0x7FFFFFFF);
    return Vector4(_mm_or_ps(
        _mm_and_ps(vmask, vec0.get128()),      // Value
        _mm_andnot_ps(vmask, vec1.get128()))); // Signs
}

inline const Vector4 maxPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
    return Vector4(_mm_max_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec maxElem(const Vector4 & vec)
{
    return FloatInVec(_mm_max_ps(
        _mm_max_ps(sseSplat(vec.get128(), 0), sseSplat(vec.get128(), 1)),
        _mm_max_ps(sseSplat(vec.get128(), 2), sseSplat(vec.get128(), 3))));
}

inline const Vector4 minPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
    return Vector4(_mm_min_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec minElem(const Vector4 & vec)
{
    return FloatInVec(_mm_min_ps(
        _mm_min_ps(sseSplat(vec.get128(), 0), sseSplat(vec.get128(), 1)),
        _mm_min_ps(sseSplat(vec.get128(), 2), sseSplat(vec.get128(), 3))));
}

inline const FloatInVec sum(const Vector4 & vec)
{
    return FloatInVec(_mm_add_ps(
        _mm_add_ps(sseSplat(vec.get128(), 0), sseSplat(vec.get128(), 1)),
        _mm_add_ps(sseSplat(vec.get128(), 2), sseSplat(vec.get128(), 3))));
}

inline const FloatInVec dot(const Vector4 & vec0, const Vector4 & vec1)
{
    return FloatInVec(sseVecDot4(vec0.get128(), vec1.get128()), 0);
}

inline const FloatInVec lengthSqr(const Vector4 & vec)
{
    return FloatInVec(sseVecDot4(vec.get128(), vec.get128()), 0);
}

inline const FloatInVec length(const Vector4 & vec)
{
    return FloatInVec(_mm_sqrt_ps(sseVecDot4(vec.get128(), vec.get128())), 0);
}

inline const Vector4 normalizeApprox(const Vector4 & vec)
{
    return Vector4(_mm_mul_ps(vec.get128(), _mm_rsqrt_ps(sseVecDot4(vec.get128(), vec.get128()))));
}

inline const Vector4 normalize(const Vector4 & vec)
{
    return Vector4(_mm_mul_ps(vec.get128(), sseNewtonrapsonRSqrtf(sseVecDot4(vec.get128(), vec.get128()))));
}

inline const Vector4 select(const Vector4 & vec0, const Vector4 & vec1, bool select1)
{
    return select(vec0, vec1, BoolInVec(select1));
}

inline const Vector4 select(const Vector4 & vec0, const Vector4 & vec1, const BoolInVec & select1)
{
    return Vector4(sseSelect(vec0.get128(), vec1.get128(), select1.get128()));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Vector4 & vec)
{
    SSEFloat tmp;
    tmp.m128 = vec.get128();
    std::printf("( %f %f %f %f )\n", tmp.f[0], tmp.f[1], tmp.f[2], tmp.f[3]);
}

inline void print(const Vector4 & vec, const char * name)
{
    SSEFloat tmp;
    tmp.m128 = vec.get128();
    std::printf("%s: ( %f %f %f %f )\n", name, tmp.f[0], tmp.f[1], tmp.f[2], tmp.f[3]);
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Point3
// ========================================================

inline Point3::Point3(float _x, float _y, float _z)
{
    mVec128 = _mm_setr_ps(_x, _y, _z, 0.0f);
}

inline Point3::Point3(const FloatInVec & _x, const FloatInVec & _y, const FloatInVec & _z)
{
    mVec128 = _mm_unpacklo_ps(_mm_unpacklo_ps(_x.get128(), _z.get128()), _y.get128());
}

inline Point3::Point3(const Vector3 & vec)
{
    mVec128 = vec.get128();
}

inline Point3::Point3(float scalar)
{
    mVec128 = FloatInVec(scalar).get128();
}

inline Point3::Point3(const FloatInVec & scalar)
{
    mVec128 = scalar.get128();
}

inline Point3::Point3(__m128 vf4)
{
    mVec128 = vf4;
}

inline const Point3 lerp(float t, const Point3 & pnt0, const Point3 & pnt1)
{
    return lerp(FloatInVec(t), pnt0, pnt1);
}

inline const Point3 lerp(const FloatInVec & t, const Point3 & pnt0, const Point3 & pnt1)
{
    return (pnt0 + ((pnt1 - pnt0) * t));
}

inline __m128 Point3::get128() const
{
    return mVec128;
}

inline void storeXYZ(const Point3 & pnt, __m128 * quad)
{
    __m128 dstVec = *quad;
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    dstVec = sseSelect(pnt.get128(), dstVec, sw);
    *quad = dstVec;
}

inline void loadXYZArray(Point3 & pnt0, Point3 & pnt1, Point3 & pnt2, Point3 & pnt3, const __m128 * threeQuads)
{
    const float * quads = (const float *)threeQuads;
    pnt0 = Point3(_mm_load_ps(quads));
    pnt1 = Point3(_mm_loadu_ps(quads + 3));
    pnt2 = Point3(_mm_loadu_ps(quads + 6));
    pnt3 = Point3(_mm_loadu_ps(quads + 9));
}

inline void storeXYZArray(const Point3 & pnt0, const Point3 & pnt1, const Point3 & pnt2, const Point3 & pnt3, __m128 * threeQuads)
{
    __m128 xxxx = _mm_shuffle_ps(pnt1.get128(), pnt1.get128(), _MM_SHUFFLE(0, 0, 0, 0));
    __m128 zzzz = _mm_shuffle_ps(pnt2.get128(), pnt2.get128(), _MM_SHUFFLE(2, 2, 2, 2));
    VECTORMATH_ALIGNED(unsigned int xsw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    VECTORMATH_ALIGNED(unsigned int zsw[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    threeQuads[0] = sseSelect(pnt0.get128(), xxxx, xsw);
    threeQuads[1] = _mm_shuffle_ps(pnt1.get128(), pnt2.get128(), _MM_SHUFFLE(1, 0, 2, 1));
    threeQuads[2] = sseSelect(_mm_shuffle_ps(pnt3.get128(), pnt3.get128(), _MM_SHUFFLE(2, 1, 0, 3)), zzzz, zsw);
}

inline Point3 & Point3::operator = (const Point3 & pnt)
{
    mVec128 = pnt.mVec128;
    return *this;
}

inline Point3 & Point3::setX(float _x)
{
    sseVecSetElement(mVec128, _x, 0);
    return *this;
}

inline Point3 & Point3::setX(const FloatInVec & _x)
{
    mVec128 = sseVecInsert(mVec128, _x.get128(), 0);
    return *this;
}

inline const FloatInVec Point3::getX() const
{
    return FloatInVec(mVec128, 0);
}

inline Point3 & Point3::setY(float _y)
{
    sseVecSetElement(mVec128, _y, 1);
    return *this;
}

inline Point3 & Point3::setY(const FloatInVec & _y)
{
    mVec128 = sseVecInsert(mVec128, _y.get128(), 1);
    return *this;
}

inline const FloatInVec Point3::getY() const
{
    return FloatInVec(mVec128, 1);
}

inline Point3 & Point3::setZ(float _z)
{
    sseVecSetElement(mVec128, _z, 2);
    return *this;
}

inline Point3 & Point3::setZ(const FloatInVec & _z)
{
    mVec128 = sseVecInsert(mVec128, _z.get128(), 2);
    return *this;
}

inline const FloatInVec Point3::getZ() const
{
    return FloatInVec(mVec128, 2);
}

inline Point3 & Point3::setW(float _w)
{
    sseVecSetElement(mVec128, _w, 3);
    return *this;
}

inline Point3 & Point3::setW(const FloatInVec & _w)
{
    mVec128 = sseVecInsert(mVec128, _w.get128(), 3);
    return *this;
}

inline const FloatInVec Point3::getW() const
{
    return FloatInVec(mVec128, 3);
}

inline Point3 & Point3::setElem(int idx, float value)
{
    sseVecSetElement(mVec128, value, idx);
    return *this;
}

inline Point3 & Point3::setElem(int idx, const FloatInVec & value)
{
    mVec128 = sseVecInsert(mVec128, value.get128(), idx);
    return *this;
}

inline const FloatInVec Point3::getElem(int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline VecIdx Point3::operator[](int idx)
{
    return VecIdx(mVec128, idx);
}

inline const FloatInVec Point3::operator[](int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline const Vector3 Point3::operator - (const Point3 & pnt) const
{
    return Vector3(_mm_sub_ps(mVec128, pnt.mVec128));
}

inline const Point3 Point3::operator + (const Vector3 & vec) const
{
    return Point3(_mm_add_ps(mVec128, vec.get128()));
}

inline const Point3 Point3::operator - (const Vector3 & vec) const
{
    return Point3(_mm_sub_ps(mVec128, vec.get128()));
}

inline Point3 & Point3::operator += (const Vector3 & vec)
{
    *this = *this + vec;
    return *this;
}

inline Point3 & Point3::operator -= (const Vector3 & vec)
{
    *this = *this - vec;
    return *this;
}

inline const Point3 mulPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
    return Point3(_mm_mul_ps(pnt0.get128(), pnt1.get128()));
}

inline const Point3 divPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
    return Point3(_mm_div_ps(pnt0.get128(), pnt1.get128()));
}

inline const Point3 recipPerElem(const Point3 & pnt)
{
    return Point3(_mm_rcp_ps(pnt.get128()));
}

inline const Point3 absPerElem(const Point3 & pnt)
{
    return Point3(sseFabsf(pnt.get128()));
}

inline const Point3 copySignPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
    const __m128 vmask = sseUintToM128(0x7FFFFFFF);
    return Point3(_mm_or_ps(
        _mm_and_ps(vmask, pnt0.get128()),      // Value
        _mm_andnot_ps(vmask, pnt1.get128()))); // Signs
}

inline const Point3 maxPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
    return Point3(_mm_max_ps(pnt0.get128(), pnt1.get128()));
}

inline const FloatInVec maxElem(const Point3 & pnt)
{
    return FloatInVec(_mm_max_ps(_mm_max_ps(sseSplat(pnt.get128(), 0), sseSplat(pnt.get128(), 1)), sseSplat(pnt.get128(), 2)));
}

inline const Point3 minPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
    return Point3(_mm_min_ps(pnt0.get128(), pnt1.get128()));
}

inline const FloatInVec minElem(const Point3 & pnt)
{
    return FloatInVec(_mm_min_ps(_mm_min_ps(sseSplat(pnt.get128(), 0), sseSplat(pnt.get128(), 1)), sseSplat(pnt.get128(), 2)));
}

inline const FloatInVec sum(const Point3 & pnt)
{
    return FloatInVec(_mm_add_ps(_mm_add_ps(sseSplat(pnt.get128(), 0), sseSplat(pnt.get128(), 1)), sseSplat(pnt.get128(), 2)));
}

inline const Point3 scale(const Point3 & pnt, float scaleVal)
{
    return scale(pnt, FloatInVec(scaleVal));
}

inline const Point3 scale(const Point3 & pnt, const FloatInVec & scaleVal)
{
    return mulPerElem(pnt, Point3(scaleVal));
}

inline const Point3 scale(const Point3 & pnt, const Vector3 & scaleVec)
{
    return mulPerElem(pnt, Point3(scaleVec));
}

inline const FloatInVec projection(const Point3 & pnt, const Vector3 & unitVec)
{
    return FloatInVec(sseVecDot3(pnt.get128(), unitVec.get128()), 0);
}

inline const FloatInVec distSqrFromOrigin(const Point3 & pnt)
{
    return lengthSqr(Vector3(pnt));
}

inline const FloatInVec distFromOrigin(const Point3 & pnt)
{
    return length(Vector3(pnt));
}

inline const FloatInVec distSqr(const Point3 & pnt0, const Point3 & pnt1)
{
    return lengthSqr((pnt1 - pnt0));
}

inline const FloatInVec dist(const Point3 & pnt0, const Point3 & pnt1)
{
    return length((pnt1 - pnt0));
}

inline const Point3 select(const Point3 & pnt0, const Point3 & pnt1, bool select1)
{
    return select(pnt0, pnt1, BoolInVec(select1));
}

inline const Point3 select(const Point3 & pnt0, const Point3 & pnt1, const BoolInVec & select1)
{
    return Point3(sseSelect(pnt0.get128(), pnt1.get128(), select1.get128()));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Point3 & pnt)
{
    SSEFloat tmp;
    tmp.m128 = pnt.get128();
    std::printf("( %f %f %f )\n", tmp.f[0], tmp.f[1], tmp.f[2]);
}

inline void print(const Point3 & pnt, const char * name)
{
    SSEFloat tmp;
    tmp.m128 = pnt.get128();
    std::printf("%s: ( %f %f %f )\n", name, tmp.f[0], tmp.f[1], tmp.f[2]);
}

#endif // VECTORMATH_DEBUG

} // namespace SSE
} // namespace Vectormath

#endif // VECTORMATH_SSE_VECTOR_HPP
