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

#ifndef VECTORMATH_SSE_FLOATINVEC_HPP
#define VECTORMATH_SSE_FLOATINVEC_HPP

namespace Vectormath
{
namespace SSE
{

class BoolInVec;
typedef __m128i Vector4Int;

// ========================================================
// FloatInVec
// ========================================================

// Vectorized scalar float.
VECTORMATH_ALIGNED_TYPE_PRE class FloatInVec
{
    __m128 mData;

public:

    inline FloatInVec() { }
    inline FloatInVec(__m128 vec);

    // matches standard type conversions
    //
    inline FloatInVec(const BoolInVec & vec);

    // construct from a slot of __m128
    //
    inline FloatInVec(__m128 vec, int slot);

    // explicit cast from float
    //
    explicit inline FloatInVec(float scalar);

#ifdef VECTORMATH_NO_SCALAR_CAST
    // explicit cast to float
    inline float getAsFloat() const;
#else // !VECTORMATH_NO_SCALAR_CAST
    // implicit cast to float
    inline operator float() const;
#endif // VECTORMATH_NO_SCALAR_CAST

    // get vector data
    // float value is splatted across all word slots of vector
    //
    inline __m128 get128() const;

    // operators
    //
    inline const FloatInVec operator ++ (int);
    inline const FloatInVec operator -- (int);
    inline FloatInVec & operator ++ ();
    inline FloatInVec & operator -- ();
    inline const FloatInVec operator - () const;
    inline FloatInVec & operator =  (const FloatInVec & vec);
    inline FloatInVec & operator *= (const FloatInVec & vec);
    inline FloatInVec & operator /= (const FloatInVec & vec);
    inline FloatInVec & operator += (const FloatInVec & vec);
    inline FloatInVec & operator -= (const FloatInVec & vec);

    // friend functions
    //
    friend inline const FloatInVec operator * (const FloatInVec & vec0, const FloatInVec & vec1);
    friend inline const FloatInVec operator / (const FloatInVec & vec0, const FloatInVec & vec1);
    friend inline const FloatInVec operator + (const FloatInVec & vec0, const FloatInVec & vec1);
    friend inline const FloatInVec operator - (const FloatInVec & vec0, const FloatInVec & vec1);
    friend inline const FloatInVec select(const FloatInVec & vec0, const FloatInVec & vec1, BoolInVec select_vec1);

} VECTORMATH_ALIGNED_TYPE_POST;

// ========================================================
// FloatInVec functions
// ========================================================

// operators
//
inline const FloatInVec operator *  (const FloatInVec & vec0, const FloatInVec & vec1);
inline const FloatInVec operator /  (const FloatInVec & vec0, const FloatInVec & vec1);
inline const FloatInVec operator +  (const FloatInVec & vec0, const FloatInVec & vec1);
inline const FloatInVec operator -  (const FloatInVec & vec0, const FloatInVec & vec1);
inline const BoolInVec  operator <  (const FloatInVec & vec0, const FloatInVec & vec1);
inline const BoolInVec  operator <= (const FloatInVec & vec0, const FloatInVec & vec1);
inline const BoolInVec  operator >  (const FloatInVec & vec0, const FloatInVec & vec1);
inline const BoolInVec  operator >= (const FloatInVec & vec0, const FloatInVec & vec1);
inline const BoolInVec  operator == (const FloatInVec & vec0, const FloatInVec & vec1);
inline const BoolInVec  operator != (const FloatInVec & vec0, const FloatInVec & vec1);

// select between vec0 and vec1 using BoolInVec.
// false selects vec0, true selects vec1
//
inline const FloatInVec select(const FloatInVec & vec0, const FloatInVec & vec1, const BoolInVec & select_vec1);

inline const FloatInVec rcpEst(const FloatInVec& v);
inline const FloatInVec rSqrtEstNR(const FloatInVec& v);
inline const FloatInVec sqrt(const FloatInVec& v);
inline const FloatInVec xorPerElem(const FloatInVec& a, const Vector4Int b);
inline const FloatInVec andPerElem(const FloatInVec& a, const Vector4Int b);
inline const FloatInVec andNotPerElem(const FloatInVec& a, const Vector4Int b);

} // namespace SSE
} // namespace Vectormath

// ========================================================
// FloatInVec implementation
// ========================================================

#include "boolinvec.hpp"

namespace Vectormath
{
namespace SSE
{

inline FloatInVec::FloatInVec(__m128 vec)
{
    mData = vec;
}

inline FloatInVec::FloatInVec(const BoolInVec & vec)
{
    mData = sseSelect(_mm_setzero_ps(), _mm_set1_ps(1.0f), vec.get128());
}

inline FloatInVec::FloatInVec(__m128 vec, int slot)
{
    SSEFloat v;
    v.m128 = vec;
    mData = _mm_set1_ps(v.f[slot]);
}

inline FloatInVec::FloatInVec(float scalar)
{
    mData = _mm_set1_ps(scalar);
}

#ifdef VECTORMATH_NO_SCALAR_CAST
inline float FloatInVec::getAsFloat() const
#else
inline FloatInVec::operator float() const
#endif
{
    return *((float *)&mData);
}

inline __m128 FloatInVec::get128() const
{
    return mData;
}

inline const FloatInVec FloatInVec::operator ++ (int)
{
    __m128 olddata = mData;
    operator++();
    return FloatInVec(olddata);
}

inline const FloatInVec FloatInVec::operator -- (int)
{
    __m128 olddata = mData;
    operator--();
    return FloatInVec(olddata);
}

inline FloatInVec & FloatInVec::operator ++ ()
{
    *this += FloatInVec(_mm_set1_ps(1.0f));
    return *this;
}

inline FloatInVec & FloatInVec::operator -- ()
{
    *this -= FloatInVec(_mm_set1_ps(1.0f));
    return *this;
}

inline const FloatInVec FloatInVec::operator - () const
{
    return FloatInVec(_mm_sub_ps(_mm_setzero_ps(), mData));
}

inline FloatInVec & FloatInVec::operator = (const FloatInVec & vec)
{
    mData = vec.mData;
    return *this;
}

inline FloatInVec & FloatInVec::operator *= (const FloatInVec & vec)
{
    *this = *this * vec;
    return *this;
}

inline FloatInVec & FloatInVec::operator /= (const FloatInVec & vec)
{
    *this = *this / vec;
    return *this;
}

inline FloatInVec & FloatInVec::operator += (const FloatInVec & vec)
{
    *this = *this + vec;
    return *this;
}

inline FloatInVec & FloatInVec::operator -= (const FloatInVec & vec)
{
    *this = *this - vec;
    return *this;
}

inline const FloatInVec operator * (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return FloatInVec(_mm_mul_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec operator / (const FloatInVec & num, const FloatInVec & den)
{
    return FloatInVec(_mm_div_ps(num.get128(), den.get128()));
}

inline const FloatInVec operator + (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return FloatInVec(_mm_add_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec operator - (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return FloatInVec(_mm_sub_ps(vec0.get128(), vec1.get128()));
}

inline const BoolInVec operator < (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return BoolInVec(_mm_cmpgt_ps(vec1.get128(), vec0.get128()));
}

inline const BoolInVec operator <= (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return BoolInVec(_mm_cmpge_ps(vec1.get128(), vec0.get128()));
}

inline const BoolInVec operator > (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return BoolInVec(_mm_cmpgt_ps(vec0.get128(), vec1.get128()));
}

inline const BoolInVec operator >= (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return BoolInVec(_mm_cmpge_ps(vec0.get128(), vec1.get128()));
}

inline const BoolInVec operator == (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return BoolInVec(_mm_cmpeq_ps(vec0.get128(), vec1.get128()));
}

inline const BoolInVec operator != (const FloatInVec & vec0, const FloatInVec & vec1)
{
    return BoolInVec(_mm_cmpneq_ps(vec0.get128(), vec1.get128()));
}

inline const FloatInVec select(const FloatInVec & vec0, const FloatInVec & vec1, const BoolInVec & select_vec1)
{
    return FloatInVec(sseSelect(vec0.get128(), vec1.get128(), select_vec1.get128()));
}

inline const FloatInVec rcpEst(const FloatInVec& v)
{
    return FloatInVec(_mm_rcp_ps(v.get128()));
}

inline const FloatInVec rSqrtEstNR(const FloatInVec& v)
{
    const __m128 nr = _mm_rsqrt_ps(v.get128());
    // Do one more Newton-Raphson step to improve precision.
    const __m128 muls = _mm_mul_ps(_mm_mul_ps(v.get128(), nr), nr);
    return FloatInVec(_mm_mul_ps(_mm_mul_ps(_mm_set_ps1(.5f), nr), _mm_sub_ps(_mm_set_ps1(3.f), muls)));
}

inline const FloatInVec sqrt(const FloatInVec& v)
{
    return FloatInVec(_mm_sqrt_ps(v.get128()));
}

inline const FloatInVec xorPerElem(const FloatInVec &a, const Vector4Int b)
{
    return FloatInVec(_mm_xor_ps(a.get128(), _mm_castsi128_ps(b)));
}

inline const FloatInVec andPerElem(const FloatInVec& a, const Vector4Int b)
{
    return FloatInVec(_mm_and_ps(a.get128(), _mm_castsi128_ps(b)));
}

inline const FloatInVec andNotPerElem(const FloatInVec& a, const Vector4Int b)
{
    return FloatInVec(_mm_andnot_ps(a.get128(), _mm_castsi128_ps(b)));
}

} // namespace SSE
} // namespace Vectormath

#endif // VECTORMATH_SSE_FLOATINVEC_HPP
