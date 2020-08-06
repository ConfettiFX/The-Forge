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

#ifndef VECTORMATH_SSE_VECIDX_HPP
#define VECTORMATH_SSE_VECIDX_HPP

namespace Vectormath
{
namespace SSE
{

// ========================================================
// VecIdx
// ========================================================

// Used in setting elements of Vector3, Vector4, Point3, or Quat
// with the subscripting operator [].
VECTORMATH_ALIGNED_TYPE_PRE class VecIdx
{
    __m128 & ref;
    int i;

public:

    inline VecIdx(__m128 & vec, int idx) : ref(vec), i(idx) { }

    //
    // implicitly casts to float unless VECTORMATH_NO_SCALAR_CAST defined
    // in which case, implicitly casts to FloatInVec, and one must call
    // getAsFloat() to convert to float.
    //
#ifdef VECTORMATH_NO_SCALAR_CAST
    inline operator FloatInVec() const;
    inline float getAsFloat() const;
#else // !VECTORMATH_NO_SCALAR_CAST
    inline operator float() const;
#endif // VECTORMATH_NO_SCALAR_CAST

    inline float operator = (float scalar);
    inline FloatInVec operator =  (const FloatInVec & scalar);
    inline FloatInVec operator =  (const VecIdx & scalar);
    inline FloatInVec operator *= (float scalar);
    inline FloatInVec operator *= (const FloatInVec & scalar);
    inline FloatInVec operator /= (float scalar);
    inline FloatInVec operator /= (const FloatInVec & scalar);
    inline FloatInVec operator += (float scalar);
    inline FloatInVec operator += (const FloatInVec & scalar);
    inline FloatInVec operator -= (float scalar);
    inline FloatInVec operator -= (const FloatInVec & scalar);

} VECTORMATH_ALIGNED_TYPE_POST;


//========================================= #TheForgeMathExtensionsStart ================================================

// ========================================================
// IVecIdx
// ========================================================

// Used in setting elements of IVector3, IVector4
// with the subscripting operator [].
VECTORMATH_ALIGNED_TYPE_PRE class IVecIdx
{
	__m128i & ref;
	int i;

public:

	inline IVecIdx(__m128i & vec, int idx) : ref(vec), i(idx) {}

	//
	// implicitly casts to int unless VECTORMATH_NO_SCALAR_CAST defined
	//
#ifdef VECTORMATH_NO_SCALAR_CAST
	inline int getAsInt() const;
#else // !VECTORMATH_NO_SCALAR_CAST
	inline operator int() const;
#endif // VECTORMATH_NO_SCALAR_CAST

	inline int operator = (int scalar);
	inline int operator =  (const IVecIdx & scalar);
	inline int operator *= (int scalar);
	inline int operator /= (int scalar);
	inline int operator += (int scalar);
	inline int operator -= (int scalar);

} VECTORMATH_ALIGNED_TYPE_POST;
//========================================= #TheForgeMathExtensionsEnd ================================================

} // namespace SSE
} // namespace Vectormath

#endif // VECTORMATH_SSE_VECIDX_HPP
