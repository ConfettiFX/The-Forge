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

#ifndef VECTORMATH_SSE_DOUBLEINVEC_HPP
#define VECTORMATH_SSE_DOUBLEINVEC_HPP

#include "internal.hpp"
namespace Vectormath
{
namespace SSE
{

class BoolInVec;
//typedef __m128i Vector4Int;

// ========================================================
// DoubleInVec
// ========================================================

// Vectorized scalar float.
VECTORMATH_ALIGNED_TYPE_PRE class DoubleInVec
{
    DSSEVec4 mData;

public:

    // Default constructor; does no initialization
    //
    inline DoubleInVec() { } //-V730

    inline DoubleInVec(DSSEVec4 vec);

    // matches standard type conversions
    //
    inline DoubleInVec(const BoolInVec & vec);

    // construct from a slot of __m128
    //
    inline DoubleInVec(DSSEVec4 vec, int slot);

    // explicit cast from double
    //
    explicit inline DoubleInVec(double scalar);

#ifdef VECTORMATH_NO_SCALAR_CAST
    // explicit cast to float
    inline double getAsDouble() const;
#else // !VECTORMATH_NO_SCALAR_CAST
    // implicit cast to float
    inline operator double() const;
#endif // VECTORMATH_NO_SCALAR_CAST

    // get vector data
    // double value is splatted across all word slots of vector
    //
    inline DSSEVec4 get256() const;

    // operators
    //
    inline const DoubleInVec operator ++ (int);
    inline const DoubleInVec operator -- (int);
    inline DoubleInVec & operator ++ ();
    inline DoubleInVec & operator -- ();
    inline const DoubleInVec operator - () const;
    inline DoubleInVec & operator =  (const DoubleInVec & vec);
    inline DoubleInVec & operator *= (const DoubleInVec & vec);
    inline DoubleInVec & operator /= (const DoubleInVec & vec);
    inline DoubleInVec & operator += (const DoubleInVec & vec);
    inline DoubleInVec & operator -= (const DoubleInVec & vec);

    // friend functions
    //
    friend inline const DoubleInVec operator * (const DoubleInVec & vec0, const DoubleInVec & vec1);
    friend inline const DoubleInVec operator / (const DoubleInVec & vec0, const DoubleInVec & vec1);
    friend inline const DoubleInVec operator + (const DoubleInVec & vec0, const DoubleInVec & vec1);
    friend inline const DoubleInVec operator - (const DoubleInVec & vec0, const DoubleInVec & vec1);
    friend inline const DoubleInVec select(const DoubleInVec & vec0, const DoubleInVec & vec1, const BoolInVec & select_vec1);

} VECTORMATH_ALIGNED_TYPE_POST;

// ========================================================
// FloatInVec functions
// ========================================================

// operators
//

inline const DoubleInVec operator *  (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const DoubleInVec operator /  (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const DoubleInVec operator +  (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const DoubleInVec operator -  (const DoubleInVec & vec0, const DoubleInVec & vec1);

inline const BoolInVec operator <  (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const BoolInVec operator <= (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const BoolInVec operator >  (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const BoolInVec operator >= (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const BoolInVec operator == (const DoubleInVec & vec0, const DoubleInVec & vec1);
inline const BoolInVec operator != (const DoubleInVec & vec0, const DoubleInVec & vec1);

// select between vec0 and vec1 using BoolInVec.
// false selects vec0, true selects vec1
//
inline const DoubleInVec select(const DoubleInVec & vec0, const DoubleInVec & vec1, const BoolInVec & select_vec1);

inline const DoubleInVec rcpEst(const DoubleInVec& v);
inline const DoubleInVec rSqrtEstNR(const DoubleInVec& v);
inline const DoubleInVec sqrt(const DoubleInVec& v);
inline const DoubleInVec xorPerElem(const DoubleInVec& a, const Vector4Int b);
inline const DoubleInVec andPerElem(const DoubleInVec& a, const Vector4Int b);
inline const DoubleInVec andNotPerElem(const DoubleInVec& a, const Vector4Int b);

} // namespace SSE
} // namespace Vectormath

// ========================================================
// DoubleInVec implementation
// ========================================================

#include "boolinvec.hpp"

namespace Vectormath
{
namespace SSE
{

inline DoubleInVec::DoubleInVec(DSSEVec4 vec)
{
    mData = vec;
}

inline DoubleInVec::DoubleInVec(const BoolInVec & vec)
{
	mData = dsseSelect(dsseSetZero(), dsseSet1(1.0), dsseFromBool(vec));
}

inline DoubleInVec::DoubleInVec(DSSEVec4 vec, int slot)
{
	DSSEDouble v;
    v.m256 = vec;
	mData = dsseSet1(v.d[slot]);
}

inline DoubleInVec::DoubleInVec(double scalar)
{
    mData = dsseSet1(scalar);
}

#ifdef VECTORMATH_NO_SCALAR_CAST
inline float DoubleInVec::getAsDouble() const
#else
inline DoubleInVec::operator double() const
#endif
{
    return *((double *)&mData);
}

inline DSSEVec4 DoubleInVec::get256() const
{
    return mData;
}

inline const DoubleInVec DoubleInVec::operator ++ (int)
{
	DSSEVec4 olddata = mData;
    operator++();
    return DoubleInVec(olddata);
}

inline const DoubleInVec DoubleInVec::operator -- (int)
{
	DSSEVec4 olddata = mData;
    operator--();
    return DoubleInVec(olddata);
}

inline DoubleInVec & DoubleInVec::operator ++ ()
{
    *this += DoubleInVec(dsseSet1(1.0));
    return *this;
}

inline DoubleInVec & DoubleInVec::operator -- ()
{
    *this -= DoubleInVec(dsseSet1(1.0));
    return *this;
}

inline const DoubleInVec DoubleInVec::operator - () const
{
    return DoubleInVec(dsseSub(dsseSetZero(), mData));
}

inline DoubleInVec & DoubleInVec::operator = (const DoubleInVec & vec)
{
    mData = vec.mData;
    return *this;
}

inline DoubleInVec & DoubleInVec::operator *= (const DoubleInVec & vec)
{
    *this = *this * vec;
    return *this;
}

inline DoubleInVec & DoubleInVec::operator /= (const DoubleInVec & vec)
{
    *this = *this / vec;
    return *this;
}

inline DoubleInVec & DoubleInVec::operator += (const DoubleInVec & vec)
{
    *this = *this + vec;
    return *this;
}

inline DoubleInVec & DoubleInVec::operator -= (const DoubleInVec & vec)
{
    *this = *this - vec;
    return *this;
}

inline const DoubleInVec operator * (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
    return DoubleInVec(dsseMul(vec0.get256(), vec1.get256()));
}

inline const DoubleInVec operator / (const DoubleInVec & num, const DoubleInVec & den)
{
    return DoubleInVec(dsseDiv(num.get256(), den.get256()));
}

inline const DoubleInVec operator + (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
    return DoubleInVec(dsseAdd(vec0.get256(), vec1.get256()));
}

inline const DoubleInVec operator - (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
    return DoubleInVec(dsseSub(vec0.get256(), vec1.get256()));
}

inline const BoolInVec operator < (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
    return BoolInVec(dsseLt(vec0.get256(), vec1.get256()));
}

inline const BoolInVec operator <= (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
	return BoolInVec(dsseLe(vec0.get256(), vec1.get256()));
}

inline const BoolInVec operator > (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
	return BoolInVec(dsseGt(vec0.get256(), vec1.get256()));
}

inline const BoolInVec operator >= (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
	return BoolInVec(dsseGe(vec0.get256(), vec1.get256()));
}

inline const BoolInVec operator == (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
	return BoolInVec(dsseEq(vec0.get256(), vec1.get256()));
}

inline const BoolInVec operator != (const DoubleInVec & vec0, const DoubleInVec & vec1)
{
	return BoolInVec(dsseNe(vec0.get256(), vec1.get256()));
}

inline const DoubleInVec select(const DoubleInVec & vec0, const DoubleInVec & vec1, const BoolInVec & select_vec1)
{
    return DoubleInVec(dsseSelect(vec0.get256(), vec1.get256(), dsseFromBool(select_vec1)));
}

inline const DoubleInVec rcpEst(const DoubleInVec& v)
{
    return DoubleInVec(dsseRecipf(v.get256()));
}

inline const DoubleInVec rSqrtEstNR(const DoubleInVec& v)
{
	return DoubleInVec(dsseNewtonrapsonRSqrtf(v.get256()));
}

inline const DoubleInVec sqrt(const DoubleInVec& v)
{
	return DoubleInVec(dsseSqrtf(v.get256()));
}

inline const DoubleInVec xorPerElem(const DoubleInVec &a, const Vector4Int b)
{
	SSEInt c;
	c.m128 = _mm_xor_si128(dsseToIVec4(a.get256()), b);
	return DoubleInVec(dsseFromIVec4(c));
}

inline const DoubleInVec andPerElem(const DoubleInVec& a, const Vector4Int b)
{
	SSEInt c;
	c.m128 = _mm_and_si128(dsseToIVec4(a.get256()), b);
	return DoubleInVec(dsseFromIVec4(c));
}

inline const DoubleInVec andNotPerElem(const DoubleInVec& a, const Vector4Int b)
{
	SSEInt c;
	c.m128 = _mm_andnot_si128(dsseToIVec4(a.get256()), b);
	return DoubleInVec(dsseFromIVec4(c));
}

} // namespace SSE
} // namespace Vectormath

#endif // VECTORMATH_SSE_DOUBLEINVEC_HPP
