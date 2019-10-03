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

#ifndef VECTORMATH_NEON_QUATERNION_HPP
#define VECTORMATH_NEON_QUATERNION_HPP

namespace Vectormath
{
namespace Neon
{

// ========================================================
// Quat
// ========================================================

inline Quat::Quat(float _x, float _y, float _z, float _w)
{
    mVec128 = _mm_setr_ps(_x, _y, _z, _w);
}

inline Quat::Quat(const FloatInVec & _x, const FloatInVec & _y, const FloatInVec & _z, const FloatInVec & _w)
{
    mVec128 = _mm_unpacklo_ps(
        _mm_unpacklo_ps(_x.get128(), _z.get128()),
        _mm_unpacklo_ps(_y.get128(), _w.get128()));
}

inline Quat::Quat(const Vector3 & xyz, float _w)
{
    mVec128 = xyz.get128();
    sseVecSetElement(mVec128, _w, 3);
}

inline Quat::Quat(const Vector3 & xyz, const FloatInVec & _w)
{
    mVec128 = xyz.get128();
    mVec128 = sseVecInsert(mVec128, _w.get128(), 3);
}

inline Quat::Quat(const Vector4 & vec)
{
    mVec128 = vec.get128();
}

inline Quat::Quat(float scalar)
{
    mVec128 = FloatInVec(scalar).get128();
}

inline Quat::Quat(const FloatInVec & scalar)
{
    mVec128 = scalar.get128();
}

inline Quat::Quat(__m128 vf4)
{
    mVec128 = vf4;
}

inline const Quat Quat::identity()
{
    return Quat(sseUnitVec0001());
}

inline const Quat lerp(float t, const Quat & quat0, const Quat & quat1)
{
    return lerp(FloatInVec(t), quat0, quat1);
}

inline const Quat lerp(const FloatInVec & t, const Quat & quat0, const Quat & quat1)
{
    return (quat0 + ((quat1 - quat0) * t));
}

inline const Quat slerp(float t, const Quat & unitQuat0, const Quat & unitQuat1)
{
    return slerp(FloatInVec(t), unitQuat0, unitQuat1);
}

inline const Quat slerp(const FloatInVec & t, const Quat & unitQuat0, const Quat & unitQuat1)
{
    Quat start;
    SSEFloat4V scales, scale0, scale1, cosAngle, angle, tttt, oneMinusT, angles, sines;
    SSEUint4V selectMask;
    cosAngle = sseVecDot4(unitQuat0.get128(), unitQuat1.get128());
    selectMask = (SSEUint4V)_mm_cmpgt_ps(_mm_setzero_ps(), cosAngle);
    cosAngle = sseSelect(cosAngle, sseNegatef(cosAngle), selectMask);
    start = Quat(sseSelect(unitQuat0.get128(), sseNegatef(unitQuat0.get128()), selectMask));
    selectMask = (SSEUint4V)_mm_cmpgt_ps(_mm_set1_ps(VECTORMATH_SLERP_TOL), cosAngle);
    angle = sseACosf(cosAngle);
    tttt = t.get128();
    oneMinusT = _mm_sub_ps(_mm_set1_ps(1.0f), tttt);
    angles = sseMergeH(_mm_set1_ps(1.0f), tttt);
    angles = sseMergeH(angles, oneMinusT);
    angles = sseMAdd(angles, angle, _mm_setzero_ps());
    sines = sseSinf(angles);
    scales = _mm_div_ps(sines, sseSplat(sines, 0));
    scale0 = sseSelect(oneMinusT, sseSplat(scales, 1), selectMask);
    scale1 = sseSelect(tttt, sseSplat(scales, 2), selectMask);
    return Quat(sseMAdd(start.get128(), scale0, _mm_mul_ps(unitQuat1.get128(), scale1)));
}

inline const Quat squad(float t, const Quat & unitQuat0, const Quat & unitQuat1, const Quat & unitQuat2, const Quat & unitQuat3)
{
    return squad(FloatInVec(t), unitQuat0, unitQuat1, unitQuat2, unitQuat3);
}

inline const Quat squad(const FloatInVec & t, const Quat & unitQuat0, const Quat & unitQuat1, const Quat & unitQuat2, const Quat & unitQuat3)
{
    return slerp(((FloatInVec(2.0f) * t) * (FloatInVec(1.0f) - t)), slerp(t, unitQuat0, unitQuat3), slerp(t, unitQuat1, unitQuat2));
}

inline __m128 Quat::get128() const
{
    return mVec128;
}

inline Quat & Quat::operator = (const Quat & quat)
{
    mVec128 = quat.mVec128;
    return *this;
}

inline Quat & Quat::setXYZ(const Vector3 & vec)
{
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    mVec128 = sseSelect(vec.get128(), mVec128, sw);
    return *this;
}

inline const Vector3 Quat::getXYZ() const
{
    return Vector3(mVec128);
}

inline Quat & Quat::setX(float _x)
{
    sseVecSetElement(mVec128, _x, 0);
    return *this;
}

inline Quat & Quat::setX(const FloatInVec & _x)
{
    mVec128 = sseVecInsert(mVec128, _x.get128(), 0);
    return *this;
}

inline const FloatInVec Quat::getX() const
{
    return FloatInVec(mVec128, 0);
}

inline Quat & Quat::setY(float _y)
{
    sseVecSetElement(mVec128, _y, 1);
    return *this;
}

inline Quat & Quat::setY(const FloatInVec & _y)
{
    mVec128 = sseVecInsert(mVec128, _y.get128(), 1);
    return *this;
}

inline const FloatInVec Quat::getY() const
{
    return FloatInVec(mVec128, 1);
}

inline Quat & Quat::setZ(float _z)
{
    sseVecSetElement(mVec128, _z, 2);
    return *this;
}

inline Quat & Quat::setZ(const FloatInVec & _z)
{
    mVec128 = sseVecInsert(mVec128, _z.get128(), 2);
    return *this;
}

inline const FloatInVec Quat::getZ() const
{
    return FloatInVec(mVec128, 2);
}

inline Quat & Quat::setW(float _w)
{
    sseVecSetElement(mVec128, _w, 3);
    return *this;
}

inline Quat & Quat::setW(const FloatInVec & _w)
{
    mVec128 = sseVecInsert(mVec128, _w.get128(), 3);
    return *this;
}

inline const FloatInVec Quat::getW() const
{
    return FloatInVec(mVec128, 3);
}

inline Quat & Quat::setElem(int idx, float value)
{
    sseVecSetElement(mVec128, value, idx);
    return *this;
}

inline Quat & Quat::setElem(int idx, const FloatInVec & value)
{
    mVec128 = sseVecInsert(mVec128, value.get128(), idx);
    return *this;
}

inline const FloatInVec Quat::getElem(int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline VecIdx Quat::operator[](int idx)
{
    return VecIdx(mVec128, idx);
}

inline const FloatInVec Quat::operator[](int idx) const
{
    return FloatInVec(mVec128, idx);
}

inline const Quat Quat::operator + (const Quat & quat) const
{
    return Quat(_mm_add_ps(mVec128, quat.mVec128));
}

inline const Quat Quat::operator - (const Quat & quat) const
{
    return Quat(_mm_sub_ps(mVec128, quat.mVec128));
}

inline const Quat Quat::operator * (float scalar) const
{
    return *this * FloatInVec(scalar);
}

inline const Quat Quat::operator * (const FloatInVec & scalar) const
{
    return Quat(_mm_mul_ps(mVec128, scalar.get128()));
}

inline Quat & Quat::operator += (const Quat & quat)
{
    *this = *this + quat;
    return *this;
}

inline Quat & Quat::operator -= (const Quat & quat)
{
    *this = *this - quat;
    return *this;
}

inline Quat & Quat::operator *= (float scalar)
{
    *this = *this * scalar;
    return *this;
}

inline Quat & Quat::operator *= (const FloatInVec & scalar)
{
    *this = *this * scalar;
    return *this;
}

inline const Quat Quat::operator / (float scalar) const
{
    return *this / FloatInVec(scalar);
}

inline const Quat Quat::operator / (const FloatInVec & scalar) const
{
    return Quat(_mm_div_ps(mVec128, scalar.get128()));
}

inline Quat & Quat::operator /= (float scalar)
{
    *this = *this / scalar;
    return *this;
}

inline Quat & Quat::operator /= (const FloatInVec & scalar)
{
    *this = *this / scalar;
    return *this;
}

inline const Quat Quat::operator - () const
{
    return Quat(_mm_sub_ps(_mm_setzero_ps(), mVec128));
}

inline const Quat operator * (float scalar, const Quat & quat)
{
    return FloatInVec(scalar) * quat;
}

inline const Quat operator * (const FloatInVec & scalar, const Quat & quat)
{
    return quat * scalar;
}

inline const FloatInVec dot(const Quat & quat0, const Quat & quat1)
{
    return FloatInVec(sseVecDot4(quat0.get128(), quat1.get128()), 0);
}

inline const FloatInVec norm(const Quat & quat)
{
    return FloatInVec(sseVecDot4(quat.get128(), quat.get128()), 0);
}

inline const FloatInVec length(const Quat & quat)
{
    return FloatInVec(_mm_sqrt_ps(sseVecDot4(quat.get128(), quat.get128())), 0);
}

inline const Quat normalize(const Quat & quat)
{
    return Quat(_mm_mul_ps(quat.get128(), _mm_rsqrt_ps(sseVecDot4(quat.get128(), quat.get128()))));
}

inline const Quat Quat::rotation(const Vector3 & unitVec0, const Vector3 & unitVec1)
{
    Vector3 crossVec;
    __m128 cosAngle, cosAngleX2Plus2, recipCosHalfAngleX2, cosHalfAngleX2, res;
    cosAngle = sseVecDot3(unitVec0.get128(), unitVec1.get128());
    cosAngleX2Plus2 = sseMAdd(cosAngle, _mm_set1_ps(2.0f), _mm_set1_ps(2.0f));
    recipCosHalfAngleX2 = _mm_rsqrt_ps(cosAngleX2Plus2);
    cosHalfAngleX2 = _mm_mul_ps(recipCosHalfAngleX2, cosAngleX2Plus2);
    crossVec = cross(unitVec0, unitVec1);
    res = _mm_mul_ps(crossVec.get128(), recipCosHalfAngleX2);
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    res = sseSelect(res, _mm_mul_ps(cosHalfAngleX2, _mm_set1_ps(0.5f)), sw);
    return Quat(res);
}

inline const Quat Quat::rotation(float radians, const Vector3 & unitVec)
{
    return rotation(FloatInVec(radians), unitVec);
}

inline const Quat Quat::rotation(const FloatInVec & radians, const Vector3 & unitVec)
{
    __m128 s, c, angle, res;
    angle = _mm_mul_ps(radians.get128(), _mm_set1_ps(0.5f));
    sseSinfCosf(angle, &s, &c);
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    res = sseSelect(_mm_mul_ps(unitVec.get128(), s), c, sw);
    return Quat(res);
}

inline const Quat Quat::rotationX(float radians)
{
    return rotationX(FloatInVec(radians));
}

inline const Quat Quat::rotationX(const FloatInVec & radians)
{
    __m128 s, c, angle, res;
    angle = _mm_mul_ps(radians.get128(), _mm_set1_ps(0.5f));
    sseSinfCosf(angle, &s, &c);
    VECTORMATH_ALIGNED(unsigned int xsw[4]) = { 0xFFFFFFFF, 0, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int wsw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    res = sseSelect(_mm_setzero_ps(), s, xsw);
    res = sseSelect(res, c, wsw);
    return Quat(res);
}

inline const Quat Quat::rotationY(float radians)
{
    return rotationY(FloatInVec(radians));
}

inline const Quat Quat::rotationY(const FloatInVec & radians)
{
    __m128 s, c, angle, res;
    angle = _mm_mul_ps(radians.get128(), _mm_set1_ps(0.5f));
    sseSinfCosf(angle, &s, &c);
    VECTORMATH_ALIGNED(unsigned int ysw[4]) = { 0, 0xFFFFFFFF, 0, 0 };
    VECTORMATH_ALIGNED(unsigned int wsw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    res = sseSelect(_mm_setzero_ps(), s, ysw);
    res = sseSelect(res, c, wsw);
    return Quat(res);
}

inline const Quat Quat::rotationZ(float radians)
{
    return rotationZ(FloatInVec(radians));
}

inline const Quat Quat::rotationZ(const FloatInVec & radians)
{
    __m128 s, c, angle, res;
    angle = _mm_mul_ps(radians.get128(), _mm_set1_ps(0.5f));
    sseSinfCosf(angle, &s, &c);
    VECTORMATH_ALIGNED(unsigned int zsw[4]) = { 0, 0, 0xFFFFFFFF, 0 };
    VECTORMATH_ALIGNED(unsigned int wsw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    res = sseSelect(_mm_setzero_ps(), s, zsw);
    res = sseSelect(res, c, wsw);
    return Quat(res);
}

inline const Quat Quat::fromVectors(const Vector3& from, const Vector3& to)
{
    const __m128 from_128 = from.get128();
    const __m128 to_128 = to.get128();

    const __m128 norm_from_norm_to =
        _mm_sqrt_ss(_mm_mul_ps(sseVecDot3(from_128, from_128), sseVecDot3(to_128, to_128)));
    const float norm_from_norm_to_x = _mm_cvtss_f32(norm_from_norm_to);
    if (norm_from_norm_to_x < kNormalizationToleranceSq) {
        return Quat::identity();
    }

    const __m128 real_part = _mm_add_ss(norm_from_norm_to, dot(from, to).get128());
    __m128 quat;
    if (_mm_cvtss_f32(real_part) < kNormalizationToleranceSq * norm_from_norm_to_x) {
        // If _from and _to are exactly opposite, rotate 180 degrees around an
        // arbitrary orthogonal axis. Axis normalization can happen later, when we
        // normalize the quaternion.

        float from_f[4];
        _mm_storeu_ps(from_f, from_128);
        quat = std::abs(from_f[0]) > std::abs(from_f[2])
            ? _mm_set_ps(0.0f, 0.0f, from_f[0], -from_f[1])
            : _mm_set_ps(0.0f, from_f[1], -from_f[2], 0.0f);
    }
    else {
        // This is the general code path.
        quat = sseVecCross(from_128, to_128);
        quat = _mm_shuffle_ps(quat, _mm_shuffle_ps(real_part, quat, _MM_SHUFFLE(2, 2, 0, 0)), _MM_SHUFFLE(0, 2, 1, 0));
    }

    return Quat(normalize(Vector4(quat)));
}

inline const Quat Quat::fromAxisCosAngle(const Vector3 &axis, const FloatInVec &cos)
{

    const __m128 one = _mm_set1_ps(1.0f);
    const __m128 half = _mm_set1_ps(0.5f);

    const __m128 half_cos2 = _mm_mul_ps(_mm_add_ps(one, cos.get128()), half);
    const __m128 half_sin2 = _mm_sub_ps(one, half_cos2);
    const __m128 half_sincos2 =
          _mm_shuffle_ps(_mm_unpacklo_ps(half_cos2, half_sin2), half_cos2,
                     _MM_SHUFFLE(3, 2, 1, 0));
    const __m128 half_sincos = _mm_sqrt_ps(half_sincos2);
    const __m128 half_sin = sseSplat(half_sincos, 1);

    const __m128 vec = _mm_mul_ps(axis.get128(), half_sin);

    return Quat(_mm_shuffle_ps(
          vec, _mm_shuffle_ps(half_sincos, vec, _MM_SHUFFLE(2, 2, 0, 0)),
          _MM_SHUFFLE(0, 2, 1, 0)));
}

inline const Quat Quat::operator * (const Quat & quat) const
{
    __m128 ldata, rdata, qv, tmp0, tmp1, tmp2, tmp3;
    __m128 product, l_wxyz, r_wxyz, xy, qw;
    ldata = mVec128;
    rdata = quat.mVec128;
    tmp0 = _mm_shuffle_ps(ldata, ldata, _MM_SHUFFLE(3, 0, 2, 1));
    tmp1 = _mm_shuffle_ps(rdata, rdata, _MM_SHUFFLE(3, 1, 0, 2));
    tmp2 = _mm_shuffle_ps(ldata, ldata, _MM_SHUFFLE(3, 1, 0, 2));
    tmp3 = _mm_shuffle_ps(rdata, rdata, _MM_SHUFFLE(3, 0, 2, 1));
    qv = _mm_mul_ps(sseSplat(ldata, 3), rdata);
    qv = sseMAdd(sseSplat(rdata, 3), ldata, qv);
    qv = sseMAdd(tmp0, tmp1, qv);
    qv = sseMSub(tmp2, tmp3, qv);
    product = _mm_mul_ps(ldata, rdata);
    l_wxyz = sseSld(ldata, ldata, 12);
    r_wxyz = sseSld(rdata, rdata, 12);
    qw = sseMSub(l_wxyz, r_wxyz, product);
    xy = sseMAdd(l_wxyz, r_wxyz, product);
    qw = _mm_sub_ps(qw, sseSld(xy, xy, 8));
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0, 0, 0, 0xFFFFFFFF };
    return Quat(sseSelect(qv, qw, sw));
}

inline Quat & Quat::operator *= (const Quat & quat)
{
    *this = *this * quat;
    return *this;
}

inline const Vector3 rotate(const Quat & quat, const Vector3 & vec)
{
    __m128 qdata, vdata, product, tmp0, tmp1, tmp2, tmp3, wwww, qv, qw, res;
    qdata = quat.get128();
    vdata = vec.get128();
    tmp0 = _mm_shuffle_ps(qdata, qdata, _MM_SHUFFLE(3, 0, 2, 1));
    tmp1 = _mm_shuffle_ps(vdata, vdata, _MM_SHUFFLE(3, 1, 0, 2));
    tmp2 = _mm_shuffle_ps(qdata, qdata, _MM_SHUFFLE(3, 1, 0, 2));
    tmp3 = _mm_shuffle_ps(vdata, vdata, _MM_SHUFFLE(3, 0, 2, 1));
    wwww = sseSplat(qdata, 3);
    qv = _mm_mul_ps(wwww, vdata);
    qv = sseMAdd(tmp0, tmp1, qv);
    qv = sseMSub(tmp2, tmp3, qv);
    product = _mm_mul_ps(qdata, vdata);
    qw = sseMAdd(sseSld(qdata, qdata, 4), sseSld(vdata, vdata, 4), product);
    qw = _mm_add_ps(sseSld(product, product, 8), qw);
    tmp1 = _mm_shuffle_ps(qv, qv, _MM_SHUFFLE(3, 1, 0, 2));
    tmp3 = _mm_shuffle_ps(qv, qv, _MM_SHUFFLE(3, 0, 2, 1));
    res = _mm_mul_ps(sseSplat(qw, 0), qdata);
    res = sseMAdd(wwww, qv, res);
    res = sseMAdd(tmp0, tmp1, res);
    res = sseMSub(tmp2, tmp3, res);
    return Vector3(res);
}

inline const Quat conj(const Quat & quat)
{
    VECTORMATH_ALIGNED(unsigned int sw[4]) = { 0x80000000, 0x80000000, 0x80000000, 0 };
    return Quat(_mm_xor_ps(quat.get128(), _mm_load_ps((float *)sw)));
}

inline const Quat inverse(const Quat & quat)
{
	Quat con = conj(quat);
	//vec4 q = vec4(quat.getXYZ(), quat.getW());
	float d = 1.0f / dot(quat, quat);
	return con * d;
}

inline const Quat select(const Quat & quat0, const Quat & quat1, bool select1)
{
    return select(quat0, quat1, BoolInVec(select1));
}

inline const Quat select(const Quat & quat0, const Quat & quat1, const BoolInVec & select1)
{
    return Quat(sseSelect(quat0.get128(), quat1.get128(), select1.get128()));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Quat & quat)
{
    SSEFloat tmp;
    tmp.m128 = quat.get128();
    std::printf("( %f %f %f %f )\n", tmp.f[0], tmp.f[1], tmp.f[2], tmp.f[3]);
}

inline void print(const Quat & quat, const char * name)
{
    SSEFloat tmp;
    tmp.m128 = quat.get128();
    std::printf("%s: ( %f %f %f %f )\n", name, tmp.f[0], tmp.f[1], tmp.f[2], tmp.f[3]);
}

#endif // VECTORMATH_DEBUG

} // namespace Neon
} // namespace Vectormath

#endif // VECTORMATH_NEON_QUATERNION_HPP
