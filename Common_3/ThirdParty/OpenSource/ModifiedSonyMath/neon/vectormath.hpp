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

#ifndef VECTORMATH_NEON_VECTORMATH_HPP
#define VECTORMATH_NEON_VECTORMATH_HPP

#include <cmath>
#include <arm_neon.h>
#include <assert.h>
#include "sse2neon.h"

#ifdef VECTORMATH_DEBUG
    #include <cstdio>
#endif // VECTORMATH_DEBUG

#if defined(_MSC_VER)
    // Visual Studio (MS compiler)
    #define VECTORMATH_ALIGNED(type)      __declspec(align(16)) type
    #define VECTORMATH_ALIGNED_TYPE_PRE   __declspec(align(16))
    #define VECTORMATH_ALIGNED_TYPE_POST  /* nothing */
#elif defined(__GNUC__)
    // GCC or Clang
    #define VECTORMATH_ALIGNED(type)      type __attribute__((aligned(16)))
    #define VECTORMATH_ALIGNED_TYPE_PRE   /* nothing */
    #define VECTORMATH_ALIGNED_TYPE_POST  __attribute__((aligned(16)))
#else
    // Unknown compiler
    #error "Define VECTORMATH_ALIGNED for your compiler or platform!"
#endif

#include "internal.hpp"
#include "floatinvec.hpp"
#include "boolinvec.hpp"
#include "vecidx.hpp"

namespace Vectormath
{
namespace Neon
{

// ========================================================
// Forward Declarations
// ========================================================

class Vector3;
class Vector4;
class Point3;
class Quat;
class Matrix3;
class Matrix4;
class Transform3;
//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================
class AffineTransform; //CONFFX_TEST_BEGIN

// ========================================================
// A 4-D int vector in array-of-structures format
// (used for math and bitwise operations)
// ========================================================

typedef __m128i Vector4Int;

typedef const __m128i _Vector4Int;

static const float kNormalizationToleranceSq = 1e-6f;
static const float kNormalizationToleranceEstSq = 2e-3f;

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================


// ========================================================
// A 3-D vector in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Vector3
{
    __m128 mVec128;

public:

    // Default constructor; does no initialization
    //
    inline Vector3() { }

    // Construct a 3-D vector from x, y, and z elements
    //
    inline Vector3(float x, float y, float z);

    // Construct a 3-D vector from x, y, and z elements (scalar data contained in vector data type)
    //
    inline Vector3(const FloatInVec & x, const FloatInVec & y, const FloatInVec & z);

    // Copy elements from a 3-D point into a 3-D vector
    //
    explicit inline Vector3(const Point3 & pnt);

    // Set all elements of a 3-D vector to the same scalar value
    //
    explicit inline Vector3(float scalar);

    // Set all elements of a 3-D vector to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Vector3(const FloatInVec & scalar);

    // Set vector float data in a 3-D vector
    //
    explicit inline Vector3(__m128 vf4);

    // Get vector float data from a 3-D vector
    //
    inline __m128 get128() const;

    // Assign one 3-D vector to another
    //
    inline Vector3 & operator = (const Vector3 & vec);

    // Set the x element of a 3-D vector
    //
    inline Vector3 & setX(float x);

    // Set the y element of a 3-D vector
    //
    inline Vector3 & setY(float y);

    // Set the z element of a 3-D vector
    //
    inline Vector3 & setZ(float z);

    // Set the w element of a padded 3-D vector
    // NOTE:
    // You are free to use the additional w component - if never set, it's value is undefined.
    //
    inline Vector3 & setW(float w);

    // Set the x element of a 3-D vector (scalar data contained in vector data type)
    //
    inline Vector3 & setX(const FloatInVec & x);

    // Set the y element of a 3-D vector (scalar data contained in vector data type)
    //
    inline Vector3 & setY(const FloatInVec & y);

    // Set the z element of a 3-D vector (scalar data contained in vector data type)
    //
    inline Vector3 & setZ(const FloatInVec & z);

    // Set the w element of a padded 3-D vector
    // NOTE:
    // You are free to use the additional w component - if never set, it's value is undefined.
    //
    inline Vector3 & setW(const FloatInVec & w);

    // Get the x element of a 3-D vector
    //
    inline const FloatInVec getX() const;

    // Get the y element of a 3-D vector
    //
    inline const FloatInVec getY() const;

    // Get the z element of a 3-D vector
    //
    inline const FloatInVec getZ() const;

    // Get the w element of a padded 3-D vector
    // NOTE:
    // You are free to use the additional w component - if never set, it's value is undefined.
    //
    inline const FloatInVec getW() const;

    // Set an x, y, or z element of a 3-D vector by index
    //
    inline Vector3 & setElem(int idx, float value);

    // Set an x, y, or z element of a 3-D vector by index (scalar data contained in vector data type)
    //
    inline Vector3 & setElem(int idx, const FloatInVec & value);

    // Get an x, y, or z element of a 3-D vector by index
    //
    inline const FloatInVec getElem(int idx) const;

    // Subscripting operator to set or get an element
    //
    inline VecIdx operator[](int idx);

    // Subscripting operator to get an element
    //
    inline const FloatInVec operator[](int idx) const;

    // Add two 3-D vectors
    //
    inline const Vector3 operator + (const Vector3 & vec) const;

    // Subtract a 3-D vector from another 3-D vector
    //
    inline const Vector3 operator - (const Vector3 & vec) const;

    // Add a 3-D vector to a 3-D point
    //
    inline const Point3 operator + (const Point3 & pnt) const;

    // Multiply a 3-D vector by a scalar
    //
    inline const Vector3 operator * (float scalar) const;

    // Divide a 3-D vector by a scalar
    //
    inline const Vector3 operator / (float scalar) const;

    // Multiply a 3-D vector by a scalar (scalar data contained in vector data type)
    //
    inline const Vector3 operator * (const FloatInVec & scalar) const;

    // Divide a 3-D vector by a scalar (scalar data contained in vector data type)
    //
    inline const Vector3 operator / (const FloatInVec & scalar) const;

    // Perform compound assignment and addition with a 3-D vector
    //
    inline Vector3 & operator += (const Vector3 & vec);

    // Perform compound assignment and subtraction by a 3-D vector
    //
    inline Vector3 & operator -= (const Vector3 & vec);

    // Perform compound assignment and multiplication by a scalar
    //
    inline Vector3 & operator *= (float scalar);

    // Perform compound assignment and division by a scalar
    //
    inline Vector3 & operator /= (float scalar);

    // Perform compound assignment and multiplication by a scalar (scalar data contained in vector data type)
    //
    inline Vector3 & operator *= (const FloatInVec & scalar);

    // Perform compound assignment and division by a scalar (scalar data contained in vector data type)
    //
    inline Vector3 & operator /= (const FloatInVec & scalar);

    // Negate all elements of a 3-D vector
    //
    inline const Vector3 operator - () const;

    // Construct x axis
    //
    static inline const Vector3 xAxis();

    // Construct y axis
    //
    static inline const Vector3 yAxis();

    // Construct z axis
    //
    static inline const Vector3 zAxis();

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 3-D vector by a scalar
//
inline const Vector3 operator * (float scalar, const Vector3 & vec);

// Multiply a 3-D vector by a scalar (scalar data contained in vector data type)
//
inline const Vector3 operator * (const FloatInVec & scalar, const Vector3 & vec);

// Multiply two 3-D vectors per element
//
inline const Vector3 mulPerElem(const Vector3 & vec0, const Vector3 & vec1);

// Divide two 3-D vectors per element
// NOTE:
// Floating-point behavior matches standard library function divf4.
//
inline const Vector3 divPerElem(const Vector3 & vec0, const Vector3 & vec1);

// Compute the reciprocal of a 3-D vector per element
// NOTE:
// Floating-point behavior matches standard library function recipf4.
//
inline const Vector3 recipPerElem(const Vector3 & vec);

// Compute the absolute value of a 3-D vector per element
//
inline const Vector3 absPerElem(const Vector3 & vec);

// Copy sign from one 3-D vector to another, per element
//
inline const Vector3 copySignPerElem(const Vector3 & vec0, const Vector3 & vec1);

// Maximum of two 3-D vectors per element
//
inline const Vector3 maxPerElem(const Vector3 & vec0, const Vector3 & vec1);

// Minimum of two 3-D vectors per element
//
inline const Vector3 minPerElem(const Vector3 & vec0, const Vector3 & vec1);

// Maximum element of a 3-D vector
//
inline const FloatInVec maxElem(const Vector3 & vec);

// Minimum element of a 3-D vector
//
inline const FloatInVec minElem(const Vector3 & vec);

// Compute the sum of all elements of a 3-D vector
//
inline const FloatInVec sum(const Vector3 & vec);

// Compute the dot product of two 3-D vectors
//
inline const FloatInVec dot(const Vector3 & vec0, const Vector3 & vec1);

// Compute the square of the length of a 3-D vector
//
inline const FloatInVec lengthSqr(const Vector3 & vec);

// Compute the length of a 3-D vector
//
inline const FloatInVec length(const Vector3 & vec);

// Normalize a 3-D vector
// NOTE:
// The result is unpredictable when all elements of vec are at or near zero.
//
inline const Vector3 normalize(const Vector3 & vec);

// Compute cross product of two 3-D vectors
//
inline const Vector3 cross(const Vector3 & vec0, const Vector3 & vec1);

// Outer product of two 3-D vectors
//
inline const Matrix3 outer(const Vector3 & vec0, const Vector3 & vec1);

// Pre-multiply a row vector by a 3x3 matrix
// NOTE:
// Slower than column post-multiply.
//
inline const Vector3 rowMul(const Vector3 & vec, const Matrix3 & mat);

// Cross-product matrix of a 3-D vector
//
inline const Matrix3 crossMatrix(const Vector3 & vec);

// Create cross-product matrix and multiply
// NOTE:
// Faster than separately creating a cross-product matrix and multiplying.
//
inline const Matrix3 crossMatrixMul(const Vector3 & vec, const Matrix3 & mat);

// Linear interpolation between two 3-D vectors
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Vector3 lerp(float t, const Vector3 & vec0, const Vector3 & vec1);

// Linear interpolation between two 3-D vectors (scalar data contained in vector data type)
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Vector3 lerp(const FloatInVec & t, const Vector3 & vec0, const Vector3 & vec1);

// Spherical linear interpolation between two 3-D vectors
// NOTE:
// The result is unpredictable if the vectors point in opposite directions.
// Does not clamp t between 0 and 1.
//
inline const Vector3 slerp(float t, const Vector3 & unitVec0, const Vector3 & unitVec1);

// Spherical linear interpolation between two 3-D vectors (scalar data contained in vector data type)
// NOTE:
// The result is unpredictable if the vectors point in opposite directions.
// Does not clamp t between 0 and 1.
//
inline const Vector3 slerp(const FloatInVec & t, const Vector3 & unitVec0, const Vector3 & unitVec1);

// Conditionally select between two 3-D vectors
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Vector3 select(const Vector3 & vec0, const Vector3 & vec1, bool select1);

// Conditionally select between two 3-D vectors (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Vector3 select(const Vector3 & vec0, const Vector3 & vec1, const BoolInVec & select1);

// Returns per element binary logical xor operation of _a and _b.
// _v[0...127] = _a[0...127] ^ _b[0...127]
//
inline const Vector3 xorPerElem(const Vector3& a, const FloatInVec b);

// Compute the squareroot of a 3-D vector per element
//
inline const Vector3 sqrtPerElem(const Vector3& vec);

// Returns the per component estimated reciprocal square root of v, where
// approximation is improved with one more new Newton-Raphson step.
//
inline const Vector3 rSqrtEstNR(const Vector3& v);

// Tests if the components x, y and z of _v forms a normalized vector.
//
inline bool isNormalizedEst(const Vector3& v);

// Store x, y, and z elements of 3-D vector in first three words of a quadword, preserving fourth word
//
inline void storeXYZ(const Vector3 & vec, __m128 * quad);

// Load four three-float 3-D vectors, stored in three quadwords
//
inline void loadXYZArray(Vector3 & vec0, Vector3 & vec1, Vector3 & vec2, Vector3 & vec3, const __m128 * threeQuads);

// Store four 3-D vectors in three quadwords
//
inline void storeXYZArray(const Vector3 & vec0, const Vector3 & vec1, const Vector3 & vec2, const Vector3 & vec3, __m128 * threeQuads);

#ifdef VECTORMATH_DEBUG

// Print a 3-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Vector3 & vec);

// Print a 3-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Vector3 & vec, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A 4-D vector in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Vector4
{
    __m128 mVec128;

public:

    // Default constructor; does no initialization
    //
    inline Vector4() { }

    // Construct a 4-D vector from x, y, z, and w elements
    //
    inline Vector4(float x, float y, float z, float w);

    // Construct a 4-D vector from x, y, z, and w elements (scalar data contained in vector data type)
    //
    inline Vector4(const FloatInVec & x, const FloatInVec & y, const FloatInVec & z, const FloatInVec & w);

    // Construct a 4-D vector from a 3-D vector and a scalar
    //
    inline Vector4(const Vector3 & xyz, float w);

    // Construct a 4-D vector from a 3-D vector and a scalar (scalar data contained in vector data type)
    //
    inline Vector4(const Vector3 & xyz, const FloatInVec & w);

    // Copy x, y, and z from a 3-D vector into a 4-D vector, and set w to 0
    //
    explicit inline Vector4(const Vector3 & vec);

    // Copy x, y, and z from a 3-D point into a 4-D vector, and set w to 1
    //
    explicit inline Vector4(const Point3 & pnt);

    // Copy elements from a quaternion into a 4-D vector
    //
    explicit inline Vector4(const Quat & quat);

    // Set all elements of a 4-D vector to the same scalar value
    //
    explicit inline Vector4(float scalar);

    // Set all elements of a 4-D vector to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Vector4(const FloatInVec & scalar);

    // Set vector float data in a 4-D vector
    //
    explicit inline Vector4(__m128 vf4);

    //========================================= #TheForgeMathExtensionsBegin ================================================
    //========================================= #TheForgeAnimationMathExtensionsBegin =======================================

    // Convert from integer vector to float vector.
    //
	static inline const Vector4 fromVector4Int(const Vector4Int vecInt);

    //========================================= #TheForgeAnimationMathExtensionsEnd =======================================
    //========================================= #TheForgeMathExtensionsEnd ================================================

    // Get vector float data from a 4-D vector
    //
    inline __m128 get128() const;

    // Assign one 4-D vector to another
    //
    inline Vector4 & operator = (const Vector4 & vec);

    // Set the x, y, and z elements of a 4-D vector
    // NOTE:
    // This function does not change the w element.
    //
    inline Vector4 & setXYZ(const Vector3 & vec);

    // Get the x, y, and z elements of a 4-D vector
    //
    inline const Vector3 getXYZ() const;

    // Set the x element of a 4-D vector
    //
    inline Vector4 & setX(float x);

    // Set the y element of a 4-D vector
    //
    inline Vector4 & setY(float y);

    // Set the z element of a 4-D vector
    //
    inline Vector4 & setZ(float z);

    // Set the w element of a 4-D vector
    //
    inline Vector4 & setW(float w);

    // Set the x element of a 4-D vector (scalar data contained in vector data type)
    //
    inline Vector4 & setX(const FloatInVec & x);

    // Set the y element of a 4-D vector (scalar data contained in vector data type)
    //
    inline Vector4 & setY(const FloatInVec & y);

    // Set the z element of a 4-D vector (scalar data contained in vector data type)
    //
    inline Vector4 & setZ(const FloatInVec & z);

    // Set the w element of a 4-D vector (scalar data contained in vector data type)
    //
    inline Vector4 & setW(const FloatInVec & w);

    // Get the x element of a 4-D vector
    //
    inline const FloatInVec getX() const;

    // Get the y element of a 4-D vector
    //
    inline const FloatInVec getY() const;

    // Get the z element of a 4-D vector
    //
    inline const FloatInVec getZ() const;

    // Get the w element of a 4-D vector
    //
    inline const FloatInVec getW() const;

    // Set an x, y, z, or w element of a 4-D vector by index
    //
    inline Vector4 & setElem(int idx, float value);

    // Set an x, y, z, or w element of a 4-D vector by index (scalar data contained in vector data type)
    //
    inline Vector4 & setElem(int idx, const FloatInVec & value);

    // Get an x, y, z, or w element of a 4-D vector by index
    //
    inline const FloatInVec getElem(int idx) const;

    // Subscripting operator to set or get an element
    //
    inline VecIdx operator[](int idx);

    // Subscripting operator to get an element
    //
    inline const FloatInVec operator[](int idx) const;

    // Add two 4-D vectors
    //
    inline const Vector4 operator + (const Vector4 & vec) const;

    // Subtract a 4-D vector from another 4-D vector
    //
    inline const Vector4 operator - (const Vector4 & vec) const;

    // Multiply a 4-D vector by a scalar
    //
    inline const Vector4 operator * (float scalar) const;

    // Divide a 4-D vector by a scalar
    //
    inline const Vector4 operator / (float scalar) const;

    // Multiply a 4-D vector by a scalar (scalar data contained in vector data type)
    //
    inline const Vector4 operator * (const FloatInVec & scalar) const;

    // Divide a 4-D vector by a scalar (scalar data contained in vector data type)
    //
    inline const Vector4 operator / (const FloatInVec & scalar) const;

    // Perform compound assignment and addition with a 4-D vector
    //
    inline Vector4 & operator += (const Vector4 & vec);

    // Perform compound assignment and subtraction by a 4-D vector
    //
    inline Vector4 & operator -= (const Vector4 & vec);

    // Perform compound assignment and multiplication by a scalar
    //
    inline Vector4 & operator *= (float scalar);

    // Perform compound assignment and division by a scalar
    //
    inline Vector4 & operator /= (float scalar);

    // Perform compound assignment and multiplication by a scalar (scalar data contained in vector data type)
    //
    inline Vector4 & operator *= (const FloatInVec & scalar);

    // Perform compound assignment and division by a scalar (scalar data contained in vector data type)
    //
    inline Vector4 & operator /= (const FloatInVec & scalar);

    // Negate all elements of a 4-D vector
    //
    inline const Vector4 operator - () const;

    // Construct x axis
    //
    static inline const Vector4 xAxis();

    // Construct y axis
    //
    static inline const Vector4 yAxis();

    // Construct z axis
    //
    static inline const Vector4 zAxis();

    // Construct w axis
    //
    static inline const Vector4 wAxis();

    //========================================= #TheForgeMathExtensionsBegin ================================================
    //========================================= #TheForgeAnimationMathExtensionsBegin =======================================

    // Construct zero vector
    //
    static inline const Vector4 zero();

    // Construct one vector
    //
    static inline const Vector4 one();

    //========================================= #TheForgeAnimationMathExtensionsEnd =======================================
	//========================================= #TheForgeMathExtensionsEnd ================================================

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 4-D vector by a scalar
//
inline const Vector4 operator * (float scalar, const Vector4 & vec);

// Multiply a 4-D vector by a scalar (scalar data contained in vector data type)
//
inline const Vector4 operator * (const FloatInVec & scalar, const Vector4 & vec);

// Multiply two 4-D vectors per element
//
inline const Vector4 mulPerElem(const Vector4 & vec0, const Vector4 & vec1);

// Divide two 4-D vectors per element
// NOTE:
// Floating-point behavior matches standard library function divf4.
//
inline const Vector4 divPerElem(const Vector4 & vec0, const Vector4 & vec1);

// Compute the reciprocal of a 4-D vector per element
// NOTE:
// Floating-point behavior matches standard library function recipf4.
//
inline const Vector4 recipPerElem(const Vector4 & vec);

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

// Compute the squareroot of a 4-D vector per element
// NOTE:
// Floating-point behavior matches standard library function recipf4.
//
inline const Vector4 sqrtPerElem(const Vector4 & vec);

// Compute the reciprocal squareroot of a 4-D vector per element
// NOTE:
// Floating-point behavior matches standard library function recipf4.
//
inline const Vector4 rsqrtPerElem(const Vector4 & vec);

// Returns the per component estimated reciprocal of v.
//
inline const Vector4 rcpEst(const Vector4& v);

// Returns the per component estimated reciprocal square root of v.
//
inline const Vector4 rSqrtEst(const Vector4& v);

// Returns the per component estimated reciprocal square root of v, where
// approximation is improved with one more new Newton-Raphson step.
//
inline const Vector4 rSqrtEstNR(const Vector4& v);

// Computes the per element arccosine
//
inline const Vector4 aCos(const Vector4& arg);

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================

// Compute the absolute value of a 4-D vector per element
//
inline const Vector4 absPerElem(const Vector4 & vec);

// Copy sign from one 4-D vector to another, per element
//
inline const Vector4 copySignPerElem(const Vector4 & vec0, const Vector4 & vec1);

// Maximum of two 4-D vectors per element
//
inline const Vector4 maxPerElem(const Vector4 & vec0, const Vector4 & vec1);

// Minimum of two 4-D vectors per element
//
inline const Vector4 minPerElem(const Vector4 & vec0, const Vector4 & vec1);

// Maximum element of a 4-D vector
//
inline const FloatInVec maxElem(const Vector4 & vec);

// Minimum element of a 4-D vector
//
inline const FloatInVec minElem(const Vector4 & vec);

// Compute the sum of all elements of a 4-D vector
//
inline const FloatInVec sum(const Vector4 & vec);

// Compute the dot product of two 4-D vectors
//
inline const FloatInVec dot(const Vector4 & vec0, const Vector4 & vec1);

// Compute the square of the length of a 4-D vector
//
inline const FloatInVec lengthSqr(const Vector4 & vec);

// Compute the length of a 4-D vector
//
inline const FloatInVec length(const Vector4 & vec);

// Normalize a 4-D vector
// NOTE:
// The result is unpredictable when all elements of vec are at or near zero.
//
inline const Vector4 normalize(const Vector4 & vec);

// Outer product of two 4-D vectors
//
inline const Matrix4 outer(const Vector4 & vec0, const Vector4 & vec1);

// Linear interpolation between two 4-D vectors
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Vector4 lerp(float t, const Vector4 & vec0, const Vector4 & vec1);

// Linear interpolation between two 4-D vectors (scalar data contained in vector data type)
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Vector4 lerp(const FloatInVec & t, const Vector4 & vec0, const Vector4 & vec1);

// Spherical linear interpolation between two 4-D vectors
// NOTE:
// The result is unpredictable if the vectors point in opposite directions.
// Does not clamp t between 0 and 1.
//
inline const Vector4 slerp(float t, const Vector4 & unitVec0, const Vector4 & unitVec1);

// Spherical linear interpolation between two 4-D vectors (scalar data contained in vector data type)
// NOTE:
// The result is unpredictable if the vectors point in opposite directions.
// Does not clamp t between 0 and 1.
//
inline const Vector4 slerp(const FloatInVec & t, const Vector4 & unitVec0, const Vector4 & unitVec1);

// Conditionally select between two 4-D vectors
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Vector4 select(const Vector4 & vec0, const Vector4 & vec1, bool select1);

// Conditionally select between two 4-D vectors (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Vector4 select(const Vector4 & vec0, const Vector4 & vec1, const BoolInVec & select1);

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

// Per element "equal" comparison of _a and _b.
inline const Vector4Int cmpEq(const Vector4& a, const Vector4& b);

// Per element "not equal" comparison of _a and _b.
inline const Vector4Int cmpNotEq(const Vector4& a, const Vector4& b);

// Per element "less than" comparison of _a and _b.
inline const Vector4Int cmpLt(const Vector4& a, const Vector4& b);

// Per element "less than or equal" comparison of _a and _b.
inline const Vector4Int cmpLe(const Vector4& a, const Vector4& b);

// Per element "greater than" comparison of _a and _b.
inline const Vector4Int cmpGt(const Vector4& a, const Vector4& b);

// Per element "greater than or equal" comparison of _a and _b.
inline const Vector4Int cmpGe(const Vector4& a, const Vector4& b);

// Returns the sign bit of v.
inline const Vector4Int signBit(const Vector4& v);

// Returns per element binary logical xor operation of _a and _b.
// _v[0...127] = _a[0...127] ^ _b[0...127]
inline const Vector4 xorPerElem(const Vector4& a, const Vector4Int b);

// Returns per element binary or operation of _a and _b.
// _v[0...127] = _a[0...127] | _b[0...127] 
inline const Vector4 orPerElem(const Vector4& a, const Vector4Int b);

// Returns per element binary or operation of _a and _b.
// _v[0...127] = _a[0...127] | _b[0...127]
inline const Vector4 orPerElem(const Vector4& a, const Vector4& b);

// Returns per element binary and operation of _a and _b.
// _v[0...127] = _a[0...127] & _b[0...127]
inline const Vector4 andPerElem(const Vector4& a, const Vector4Int b);

// Converts from a half to a float.
inline const Vector4 halfToFloat(const Vector4Int vecInt);

// Transposes the 3 Vector4 of in into the x, y and z components of the 4
// Vector4 of out. Remaining w are set to 0.
inline void transpose3x4(const Vector4 in[3], Vector4 out[4]);

// Transposes the 4 Vector4 of in into the 4 Vector4 of out.
inline void transpose4x4(const Vector4 in[4], Vector4 out[4]);

//CONFFX_TEST_BEGIN
// Transposes the 4 Vector4 of in into the 3 Vector4 of out.
inline void transpose4x3(const Vector4 in[4], Vector4 out[4]);
//CONFFX_TEST_END

// Transposes the 16 Vector4 of in into the 16 Vector4 of out.
inline void transpose16x16(const Vector4 in[16], Vector4 out[16]);

// Stores the 4 components of v to the four first floats of f.
// f must be aligned to 4 bytes.
// f[0] = v.x
// f[1] = v.y
// f[2] = v.z
// f[3] = v.w
inline void storePtrU(const Vector4& v, float* f);

// Stores x, y and z components of v to the three first floats of f.
// f must be aligned to 4 bytes.
// f[0] = v.x
// f[1] = v.y
// f[2] = v.z
inline void store3PtrU(const Vector4& v, float* f);

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================

#ifdef VECTORMATH_DEBUG

// Print a 4-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Vector4 & vec);

// Print a 4-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Vector4 & vec, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A 3-D point in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Point3
{
    __m128 mVec128;

public:

    // Default constructor; does no initialization
    //
    inline Point3() { }

    // Construct a 3-D point from x, y, and z elements
    //
    inline Point3(float x, float y, float z);

    // Construct a 3-D point from x, y, and z elements (scalar data contained in vector data type)
    //
    inline Point3(const FloatInVec & x, const FloatInVec & y, const FloatInVec & z);

    // Copy elements from a 3-D vector into a 3-D point
    //
    explicit inline Point3(const Vector3 & vec);

    // Set all elements of a 3-D point to the same scalar value
    //
    explicit inline Point3(float scalar);

    // Set all elements of a 3-D point to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Point3(const FloatInVec & scalar);

    // Set vector float data in a 3-D point
    //
    explicit inline Point3(__m128 vf4);

    // Get vector float data from a 3-D point
    //
    inline __m128 get128() const;

    // Assign one 3-D point to another
    //
    inline Point3 & operator = (const Point3 & pnt);

    // Set the x element of a 3-D point
    //
    inline Point3 & setX(float x);

    // Set the y element of a 3-D point
    //
    inline Point3 & setY(float y);

    // Set the z element of a 3-D point
    //
    inline Point3 & setZ(float z);

    // Set the w element of a padded 3-D point
    // NOTE:
    // You are free to use the additional w component - if never set, it's value is undefined.
    //
    inline Point3 & setW(float w);

    // Set the x element of a 3-D point (scalar data contained in vector data type)
    //
    inline Point3 & setX(const FloatInVec & x);

    // Set the y element of a 3-D point (scalar data contained in vector data type)
    //
    inline Point3 & setY(const FloatInVec & y);

    // Set the z element of a 3-D point (scalar data contained in vector data type)
    //
    inline Point3 & setZ(const FloatInVec & z);

    // Set the w element of a padded 3-D point
    // NOTE:
    // You are free to use the additional w component - if never set, it's value is undefined.
    //
    inline Point3 & setW(const FloatInVec & w);

    // Get the x element of a 3-D point
    //
    inline const FloatInVec getX() const;

    // Get the y element of a 3-D point
    //
    inline const FloatInVec getY() const;

    // Get the z element of a 3-D point
    //
    inline const FloatInVec getZ() const;

    // Get the w element of a padded 3-D point
    // NOTE:
    // You are free to use the additional w component - if never set, it's value is undefined.
    //
    inline const FloatInVec getW() const;

    // Set an x, y, or z element of a 3-D point by index
    //
    inline Point3 & setElem(int idx, float value);

    // Set an x, y, or z element of a 3-D point by index (scalar data contained in vector data type)
    //
    inline Point3 & setElem(int idx, const FloatInVec & value);

    // Get an x, y, or z element of a 3-D point by index
    //
    inline const FloatInVec getElem(int idx) const;

    // Subscripting operator to set or get an element
    //
    inline VecIdx operator[](int idx);

    // Subscripting operator to get an element
    //
    inline const FloatInVec operator[](int idx) const;

    // Subtract a 3-D point from another 3-D point
    //
    inline const Vector3 operator - (const Point3 & pnt) const;

    // Add a 3-D point to a 3-D vector
    //
    inline const Point3 operator + (const Vector3 & vec) const;

    // Subtract a 3-D vector from a 3-D point
    //
    inline const Point3 operator - (const Vector3 & vec) const;

    // Perform compound assignment and addition with a 3-D vector
    //
    inline Point3 & operator += (const Vector3 & vec);

    // Perform compound assignment and subtraction by a 3-D vector
    //
    inline Point3 & operator -= (const Vector3 & vec);

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply two 3-D points per element
//
inline const Point3 mulPerElem(const Point3 & pnt0, const Point3 & pnt1);

// Divide two 3-D points per element
// NOTE:
// Floating-point behavior matches standard library function divf4.
//
inline const Point3 divPerElem(const Point3 & pnt0, const Point3 & pnt1);

// Compute the reciprocal of a 3-D point per element
// NOTE:
// Floating-point behavior matches standard library function recipf4.
//
inline const Point3 recipPerElem(const Point3 & pnt);

// Compute the absolute value of a 3-D point per element
//
inline const Point3 absPerElem(const Point3 & pnt);

// Copy sign from one 3-D point to another, per element
//
inline const Point3 copySignPerElem(const Point3 & pnt0, const Point3 & pnt1);

// Maximum of two 3-D points per element
//
inline const Point3 maxPerElem(const Point3 & pnt0, const Point3 & pnt1);

// Minimum of two 3-D points per element
//
inline const Point3 minPerElem(const Point3 & pnt0, const Point3 & pnt1);

// Maximum element of a 3-D point
//
inline const FloatInVec maxElem(const Point3 & pnt);

// Minimum element of a 3-D point
//
inline const FloatInVec minElem(const Point3 & pnt);

// Compute the sum of all elements of a 3-D point
//
inline const FloatInVec sum(const Point3 & pnt);

// Apply uniform scale to a 3-D point
//
inline const Point3 scale(const Point3 & pnt, float scaleVal);

// Apply uniform scale to a 3-D point (scalar data contained in vector data type)
//
inline const Point3 scale(const Point3 & pnt, const FloatInVec & scaleVal);

// Apply non-uniform scale to a 3-D point
//
inline const Point3 scale(const Point3 & pnt, const Vector3 & scaleVec);

// Scalar projection of a 3-D point on a unit-length 3-D vector
//
inline const FloatInVec projection(const Point3 & pnt, const Vector3 & unitVec);

// Compute the square of the distance of a 3-D point from the coordinate-system origin
//
inline const FloatInVec distSqrFromOrigin(const Point3 & pnt);

// Compute the distance of a 3-D point from the coordinate-system origin
//
inline const FloatInVec distFromOrigin(const Point3 & pnt);

// Compute the square of the distance between two 3-D points
//
inline const FloatInVec distSqr(const Point3 & pnt0, const Point3 & pnt1);

// Compute the distance between two 3-D points
//
inline const FloatInVec dist(const Point3 & pnt0, const Point3 & pnt1);

// Linear interpolation between two 3-D points
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Point3 lerp(float t, const Point3 & pnt0, const Point3 & pnt1);

// Linear interpolation between two 3-D points (scalar data contained in vector data type)
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Point3 lerp(const FloatInVec & t, const Point3 & pnt0, const Point3 & pnt1);

// Conditionally select between two 3-D points
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Point3 select(const Point3 & pnt0, const Point3 & pnt1, bool select1);

// Conditionally select between two 3-D points (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Point3 select(const Point3 & pnt0, const Point3 & pnt1, const BoolInVec & select1);

// Store x, y, and z elements of 3-D point in first three words of a quadword, preserving fourth word
//
inline void storeXYZ(const Point3 & pnt, __m128 * quad);

// Load four three-float 3-D points, stored in three quadwords
//
inline void loadXYZArray(Point3 & pnt0, Point3 & pnt1, Point3 & pnt2, Point3 & pnt3, const __m128 * threeQuads);

// Store four 3-D points in three quadwords
//
inline void storeXYZArray(const Point3 & pnt0, const Point3 & pnt1, const Point3 & pnt2, const Point3 & pnt3, __m128 * threeQuads);

#ifdef VECTORMATH_DEBUG

// Print a 3-D point
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Point3 & pnt);

// Print a 3-D point and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Point3 & pnt, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A quaternion in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Quat
{
    __m128 mVec128;

public:

    // Default constructor; does no initialization
    //
    inline Quat() { }

    // Construct a quaternion from x, y, z, and w elements
    //
    inline Quat(float x, float y, float z, float w);

    // Construct a quaternion from x, y, z, and w elements (scalar data contained in vector data type)
    //
    inline Quat(const FloatInVec & x, const FloatInVec & y, const FloatInVec & z, const FloatInVec & w);

    // Construct a quaternion from a 3-D vector and a scalar
    //
    inline Quat(const Vector3 & xyz, float w);

    // Construct a quaternion from a 3-D vector and a scalar (scalar data contained in vector data type)
    //
    inline Quat(const Vector3 & xyz, const FloatInVec & w);

    // Copy elements from a 4-D vector into a quaternion
    //
    explicit inline Quat(const Vector4 & vec);

    // Convert a rotation matrix to a unit-length quaternion
    //
    explicit inline Quat(const Matrix3 & rotMat);

    // Set all elements of a quaternion to the same scalar value
    //
    explicit inline Quat(float scalar);

    // Set all elements of a quaternion to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Quat(const FloatInVec & scalar);

    // Set vector float data in a quaternion
    //
    explicit inline Quat(__m128 vf4);

    // Get vector float data from a quaternion
    //
    inline __m128 get128() const;

    // Assign one quaternion to another
    //
    inline Quat & operator = (const Quat & quat);

    // Set the x, y, and z elements of a quaternion
    // NOTE:
    // This function does not change the w element.
    //
    inline Quat & setXYZ(const Vector3 & vec);

    // Get the x, y, and z elements of a quaternion
    //
    inline const Vector3 getXYZ() const;

    // Set the x element of a quaternion
    //
    inline Quat & setX(float x);

    // Set the y element of a quaternion
    //
    inline Quat & setY(float y);

    // Set the z element of a quaternion
    //
    inline Quat & setZ(float z);

    // Set the w element of a quaternion
    //
    inline Quat & setW(float w);

    // Set the x element of a quaternion (scalar data contained in vector data type)
    //
    inline Quat & setX(const FloatInVec & x);

    // Set the y element of a quaternion (scalar data contained in vector data type)
    //
    inline Quat & setY(const FloatInVec & y);

    // Set the z element of a quaternion (scalar data contained in vector data type)
    //
    inline Quat & setZ(const FloatInVec & z);

    // Set the w element of a quaternion (scalar data contained in vector data type)
    //
    inline Quat & setW(const FloatInVec & w);

    // Get the x element of a quaternion
    //
    inline const FloatInVec getX() const;

    // Get the y element of a quaternion
    //
    inline const FloatInVec getY() const;

    // Get the z element of a quaternion
    //
    inline const FloatInVec getZ() const;

    // Get the w element of a quaternion
    //
    inline const FloatInVec getW() const;

    // Set an x, y, z, or w element of a quaternion by index
    //
    inline Quat & setElem(int idx, float value);

    // Set an x, y, z, or w element of a quaternion by index (scalar data contained in vector data type)
    //
    inline Quat & setElem(int idx, const FloatInVec & value);

    // Get an x, y, z, or w element of a quaternion by index
    //
    inline const FloatInVec getElem(int idx) const;

    // Subscripting operator to set or get an element
    //
    inline VecIdx operator[](int idx);

    // Subscripting operator to get an element
    //
    inline const FloatInVec operator[](int idx) const;

    // Add two quaternions
    //
    inline const Quat operator + (const Quat & quat) const;

    // Subtract a quaternion from another quaternion
    //
    inline const Quat operator - (const Quat & quat) const;

    // Multiply two quaternions
    //
    inline const Quat operator * (const Quat & quat) const;

    // Multiply a quaternion by a scalar
    //
    inline const Quat operator * (float scalar) const;

    // Divide a quaternion by a scalar
    //
    inline const Quat operator / (float scalar) const;

    // Multiply a quaternion by a scalar (scalar data contained in vector data type)
    //
    inline const Quat operator * (const FloatInVec & scalar) const;

    // Divide a quaternion by a scalar (scalar data contained in vector data type)
    //
    inline const Quat operator / (const FloatInVec & scalar) const;

    // Perform compound assignment and addition with a quaternion
    //
    inline Quat & operator += (const Quat & quat);

    // Perform compound assignment and subtraction by a quaternion
    //
    inline Quat & operator -= (const Quat & quat);

    // Perform compound assignment and multiplication by a quaternion
    //
    inline Quat & operator *= (const Quat & quat);

    // Perform compound assignment and multiplication by a scalar
    //
    inline Quat & operator *= (float scalar);

    // Perform compound assignment and division by a scalar
    //
    inline Quat & operator /= (float scalar);

    // Perform compound assignment and multiplication by a scalar (scalar data contained in vector data type)
    //
    inline Quat & operator *= (const FloatInVec & scalar);

    // Perform compound assignment and division by a scalar (scalar data contained in vector data type)
    //
    inline Quat & operator /= (const FloatInVec & scalar);

    // Negate all elements of a quaternion
    //
    inline const Quat operator - () const;

    // Construct an identity quaternion
    //
    static inline const Quat identity();

    // Construct a quaternion to rotate between two unit-length 3-D vectors
    // NOTE:
    // The result is unpredictable if unitVec0 and unitVec1 point in opposite directions.
    //
    static inline const Quat rotation(const Vector3 & unitVec0, const Vector3 & unitVec1);

    // Construct a quaternion to rotate around a unit-length 3-D vector
    //
    static inline const Quat rotation(float radians, const Vector3 & unitVec);

    // Construct a quaternion to rotate around a unit-length 3-D vector (scalar data contained in vector data type)
    //
    static inline const Quat rotation(const FloatInVec & radians, const Vector3 & unitVec);

    // Construct a quaternion to rotate around the x axis
    //
    static inline const Quat rotationX(float radians);

    // Construct a quaternion to rotate around the y axis
    //
    static inline const Quat rotationY(float radians);

    // Construct a quaternion to rotate around the z axis
    //
    static inline const Quat rotationZ(float radians);

    // Construct a quaternion to rotate around the x axis (scalar data contained in vector data type)
    //
    static inline const Quat rotationX(const FloatInVec & radians);

    // Construct a quaternion to rotate around the y axis (scalar data contained in vector data type)
    //
    static inline const Quat rotationY(const FloatInVec & radians);

    // Construct a quaternion to rotate around the z axis (scalar data contained in vector data type)
    //
    static inline const Quat rotationZ(const FloatInVec & radians);

    static inline const Quat fromVectors(const Vector3& from, const Vector3& to);

    static inline const Quat fromAxisCosAngle(const Vector3& axis, const FloatInVec& cos);

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a quaternion by a scalar
//
inline const Quat operator * (float scalar, const Quat & quat);

// Multiply a quaternion by a scalar (scalar data contained in vector data type)
//
inline const Quat operator * (const FloatInVec & scalar, const Quat & quat);

// Compute the conjugate of a quaternion
//
inline const Quat conj(const Quat & quat);

// Use a unit-length quaternion to rotate a 3-D vector
//
inline const Vector3 rotate(const Quat & unitQuat, const Vector3 & vec);

// Compute the dot product of two quaternions
//
inline const FloatInVec dot(const Quat & quat0, const Quat & quat1);

// Compute the norm of a quaternion
//
inline const FloatInVec norm(const Quat & quat);

// Compute the length of a quaternion
//
inline const FloatInVec length(const Quat & quat);

// Normalize a quaternion
// NOTE:
// The result is unpredictable when all elements of quat are at or near zero.
//
inline const Quat normalize(const Quat & quat);

// Linear interpolation between two quaternions
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Quat lerp(float t, const Quat & quat0, const Quat & quat1);

// Linear interpolation between two quaternions (scalar data contained in vector data type)
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Quat lerp(const FloatInVec & t, const Quat & quat0, const Quat & quat1);

// Spherical linear interpolation between two quaternions
// NOTE:
// Interpolates along the shortest path between orientations.
// Does not clamp t between 0 and 1.
//
inline const Quat slerp(float t, const Quat & unitQuat0, const Quat & unitQuat1);

// Spherical linear interpolation between two quaternions (scalar data contained in vector data type)
// NOTE:
// Interpolates along the shortest path between orientations.
// Does not clamp t between 0 and 1.
//
inline const Quat slerp(const FloatInVec & t, const Quat & unitQuat0, const Quat & unitQuat1);

// Spherical quadrangle interpolation
//
inline const Quat squad(float t, const Quat & unitQuat0, const Quat & unitQuat1, const Quat & unitQuat2, const Quat & unitQuat3);

// Spherical quadrangle interpolation (scalar data contained in vector data type)
//
inline const Quat squad(const FloatInVec & t, const Quat & unitQuat0, const Quat & unitQuat1, const Quat & unitQuat2, const Quat & unitQuat3);

// Conditionally select between two quaternions
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Quat select(const Quat & quat0, const Quat & quat1, bool select1);

// Conditionally select between two quaternions (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Quat select(const Quat & quat0, const Quat & quat1, const BoolInVec & select1);

#ifdef VECTORMATH_DEBUG

// Print a quaternion
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Quat & quat);

// Print a quaternion and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Quat & quat, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A 3x3 matrix in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Matrix3
{
    Vector3 mCol0;
    Vector3 mCol1;
    Vector3 mCol2;

public:

    // Default constructor; does no initialization
    //
    inline Matrix3() { }

    // Copy a 3x3 matrix
    //
    inline Matrix3(const Matrix3 & mat);

    // Construct a 3x3 matrix containing the specified columns
    //
    inline Matrix3(const Vector3 & col0, const Vector3 & col1, const Vector3 & col2);

    // Construct a 3x3 rotation matrix from a unit-length quaternion
    //
    explicit inline Matrix3(const Quat & unitQuat);

    // Set all elements of a 3x3 matrix to the same scalar value
    //
    explicit inline Matrix3(float scalar);

    // Set all elements of a 3x3 matrix to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Matrix3(const FloatInVec & scalar);

    // Assign one 3x3 matrix to another
    //
    inline Matrix3 & operator = (const Matrix3 & mat);

    // Set column 0 of a 3x3 matrix
    //
    inline Matrix3 & setCol0(const Vector3 & col0);

    // Set column 1 of a 3x3 matrix
    //
    inline Matrix3 & setCol1(const Vector3 & col1);

    // Set column 2 of a 3x3 matrix
    //
    inline Matrix3 & setCol2(const Vector3 & col2);

    // Get column 0 of a 3x3 matrix
    //
    inline const Vector3 getCol0() const;

    // Get column 1 of a 3x3 matrix
    //
    inline const Vector3 getCol1() const;

    // Get column 2 of a 3x3 matrix
    //
    inline const Vector3 getCol2() const;

    // Set the column of a 3x3 matrix referred to by the specified index
    //
    inline Matrix3 & setCol(int col, const Vector3 & vec);

    // Set the row of a 3x3 matrix referred to by the specified index
    //
    inline Matrix3 & setRow(int row, const Vector3 & vec);

    // Get the column of a 3x3 matrix referred to by the specified index
    //
    inline const Vector3 getCol(int col) const;

    // Get the row of a 3x3 matrix referred to by the specified index
    //
    inline const Vector3 getRow(int row) const;

    // Subscripting operator to set or get a column
    //
    inline Vector3 & operator[](int col);

    // Subscripting operator to get a column
    //
    inline const Vector3 operator[](int col) const;

    // Set the element of a 3x3 matrix referred to by column and row indices
    //
    inline Matrix3 & setElem(int col, int row, float val);

    // Set the element of a 3x3 matrix referred to by column and row indices (scalar data contained in vector data type)
    //
    inline Matrix3 & setElem(int col, int row, const FloatInVec & val);

    // Get the element of a 3x3 matrix referred to by column and row indices
    //
    inline const FloatInVec getElem(int col, int row) const;

    // Add two 3x3 matrices
    //
    inline const Matrix3 operator + (const Matrix3 & mat) const;

    // Subtract a 3x3 matrix from another 3x3 matrix
    //
    inline const Matrix3 operator - (const Matrix3 & mat) const;

    // Negate all elements of a 3x3 matrix
    //
    inline const Matrix3 operator - () const;

    // Multiply a 3x3 matrix by a scalar
    //
    inline const Matrix3 operator * (float scalar) const;

    // Multiply a 3x3 matrix by a scalar (scalar data contained in vector data type)
    //
    inline const Matrix3 operator * (const FloatInVec & scalar) const;

    // Multiply a 3x3 matrix by a 3-D vector
    //
    inline const Vector3 operator * (const Vector3 & vec) const;

    // Multiply two 3x3 matrices
    //
    inline const Matrix3 operator * (const Matrix3 & mat) const;

    // Perform compound assignment and addition with a 3x3 matrix
    //
    inline Matrix3 & operator += (const Matrix3 & mat);

    // Perform compound assignment and subtraction by a 3x3 matrix
    //
    inline Matrix3 & operator -= (const Matrix3 & mat);

    // Perform compound assignment and multiplication by a scalar
    //
    inline Matrix3 & operator *= (float scalar);

    // Perform compound assignment and multiplication by a scalar (scalar data contained in vector data type)
    //
    inline Matrix3 & operator *= (const FloatInVec & scalar);

    // Perform compound assignment and multiplication by a 3x3 matrix
    //
    inline Matrix3 & operator *= (const Matrix3 & mat);

    // Construct an identity 3x3 matrix
    //
    static inline const Matrix3 identity();

    // Construct a 3x3 matrix to rotate around the x axis
    //
    static inline const Matrix3 rotationX(float radians);

    // Construct a 3x3 matrix to rotate around the y axis
    //
    static inline const Matrix3 rotationY(float radians);

    // Construct a 3x3 matrix to rotate around the z axis
    //
    static inline const Matrix3 rotationZ(float radians);

    // Construct a 3x3 matrix to rotate around the x axis (scalar data contained in vector data type)
    //
    static inline const Matrix3 rotationX(const FloatInVec & radians);

    // Construct a 3x3 matrix to rotate around the y axis (scalar data contained in vector data type)
    //
    static inline const Matrix3 rotationY(const FloatInVec & radians);

    // Construct a 3x3 matrix to rotate around the z axis (scalar data contained in vector data type)
    //
    static inline const Matrix3 rotationZ(const FloatInVec & radians);

    // Construct a 3x3 matrix to rotate around the x, y, and z axes
    //
    static inline const Matrix3 rotationZYX(const Vector3 & radiansXYZ);

    // Construct a 3x3 matrix to rotate around a unit-length 3-D vector
    //
    static inline const Matrix3 rotation(float radians, const Vector3 & unitVec);

    // Construct a 3x3 matrix to rotate around a unit-length 3-D vector (scalar data contained in vector data type)
    //
    static inline const Matrix3 rotation(const FloatInVec & radians, const Vector3 & unitVec);

    // Construct a rotation matrix from a unit-length quaternion
    //
    static inline const Matrix3 rotation(const Quat & unitQuat);

    // Construct a 3x3 matrix to perform scaling
    //
    static inline const Matrix3 scale(const Vector3 & scaleVec);

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 3x3 matrix by a scalar
//
inline const Matrix3 operator * (float scalar, const Matrix3 & mat);

// Multiply a 3x3 matrix by a scalar (scalar data contained in vector data type)
//
inline const Matrix3 operator * (const FloatInVec & scalar, const Matrix3 & mat);

// Append (post-multiply) a scale transformation to a 3x3 matrix
// NOTE:
// Faster than creating and multiplying a scale transformation matrix.
//
inline const Matrix3 appendScale(const Matrix3 & mat, const Vector3 & scaleVec);

// Prepend (pre-multiply) a scale transformation to a 3x3 matrix
// NOTE:
// Faster than creating and multiplying a scale transformation matrix.
//
inline const Matrix3 prependScale(const Vector3 & scaleVec, const Matrix3 & mat);

// Multiply two 3x3 matrices per element
//
inline const Matrix3 mulPerElem(const Matrix3 & mat0, const Matrix3 & mat1);

// Compute the absolute value of a 3x3 matrix per element
//
inline const Matrix3 absPerElem(const Matrix3 & mat);

// Transpose of a 3x3 matrix
//
inline const Matrix3 transpose(const Matrix3 & mat);

// Compute the inverse of a 3x3 matrix
// NOTE:
// Result is unpredictable when the determinant of mat is equal to or near 0.
//
inline const Matrix3 inverse(const Matrix3 & mat);

// Determinant of a 3x3 matrix
//
inline const FloatInVec determinant(const Matrix3 & mat);

// Conditionally select between two 3x3 matrices
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Matrix3 select(const Matrix3 & mat0, const Matrix3 & mat1, bool select1);

// Conditionally select between two 3x3 matrices (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Matrix3 select(const Matrix3 & mat0, const Matrix3 & mat1, const BoolInVec & select1);

#ifdef VECTORMATH_DEBUG

// Print a 3x3 matrix
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Matrix3 & mat);

// Print a 3x3 matrix and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Matrix3 & mat, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A 4x4 matrix in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Matrix4
{
    Vector4 mCol0;
    Vector4 mCol1;
    Vector4 mCol2;
    Vector4 mCol3;

public:

    // Default constructor; does no initialization
    //
    inline Matrix4() { }

    // Copy a 4x4 matrix
    //
    inline Matrix4(const Matrix4 & mat);

    // Construct a 4x4 matrix containing the specified columns
    //
    inline Matrix4(const Vector4 & col0, const Vector4 & col1, const Vector4 & col2, const Vector4 & col3);

    // Construct a 4x4 matrix from a 3x4 transformation matrix
    //
    explicit inline Matrix4(const Transform3 & mat);

    // Construct a 4x4 matrix from a 3x3 matrix and a 3-D vector
    //
    inline Matrix4(const Matrix3 & mat, const Vector3 & translateVec);

    // Construct a 4x4 matrix from a unit-length quaternion and a 3-D vector
    //
    inline Matrix4(const Quat & unitQuat, const Vector3 & translateVec);

    // Set all elements of a 4x4 matrix to the same scalar value
    //
    explicit inline Matrix4(float scalar);

    // Set all elements of a 4x4 matrix to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Matrix4(const FloatInVec & scalar);

    // Assign one 4x4 matrix to another
    //
    inline Matrix4 & operator = (const Matrix4 & mat);

    // Set the upper-left 3x3 submatrix
    // NOTE:
    // This function does not change the bottom row elements.
    //
    inline Matrix4 & setUpper3x3(const Matrix3 & mat3);

    // Get the upper-left 3x3 submatrix of a 4x4 matrix
    //
    inline const Matrix3 getUpper3x3() const;

    // Set translation component
    // NOTE:
    // This function does not change the bottom row elements.
    //
    inline Matrix4 & setTranslation(const Vector3 & translateVec);

    // Get the translation component of a 4x4 matrix
    //
    inline const Vector3 getTranslation() const;

    // Set column 0 of a 4x4 matrix
    //
    inline Matrix4 & setCol0(const Vector4 & col0);

    // Set column 1 of a 4x4 matrix
    //
    inline Matrix4 & setCol1(const Vector4 & col1);

    // Set column 2 of a 4x4 matrix
    //
    inline Matrix4 & setCol2(const Vector4 & col2);

    // Set column 3 of a 4x4 matrix
    //
    inline Matrix4 & setCol3(const Vector4 & col3);

    // Get column 0 of a 4x4 matrix
    //
    inline const Vector4 getCol0() const;

    // Get column 1 of a 4x4 matrix
    //
    inline const Vector4 getCol1() const;

    // Get column 2 of a 4x4 matrix
    //
    inline const Vector4 getCol2() const;

    // Get column 3 of a 4x4 matrix
    //
    inline const Vector4 getCol3() const;

    // Set the column of a 4x4 matrix referred to by the specified index
    //
    inline Matrix4 & setCol(int col, const Vector4 & vec);

    // Set the row of a 4x4 matrix referred to by the specified index
    //
    inline Matrix4 & setRow(int row, const Vector4 & vec);

    // Get the column of a 4x4 matrix referred to by the specified index
    //
    inline const Vector4 getCol(int col) const;

    // Get the row of a 4x4 matrix referred to by the specified index
    //
    inline const Vector4 getRow(int row) const;

    // Subscripting operator to set or get a column
    //
    inline Vector4 & operator[](int col);

    // Subscripting operator to get a column
    //
    inline const Vector4 operator[](int col) const;

    // Set the element of a 4x4 matrix referred to by column and row indices
    //
    inline Matrix4 & setElem(int col, int row, float val);

    // Set the element of a 4x4 matrix referred to by column and row indices (scalar data contained in vector data type)
    //
    inline Matrix4 & setElem(int col, int row, const FloatInVec & val);

    // Get the element of a 4x4 matrix referred to by column and row indices
    //
    inline const FloatInVec getElem(int col, int row) const;

    // Add two 4x4 matrices
    //
    inline const Matrix4 operator + (const Matrix4 & mat) const;

    // Subtract a 4x4 matrix from another 4x4 matrix
    //
    inline const Matrix4 operator - (const Matrix4 & mat) const;

    // Negate all elements of a 4x4 matrix
    //
    inline const Matrix4 operator - () const;

    // Multiply a 4x4 matrix by a scalar
    //
    inline const Matrix4 operator * (float scalar) const;

    // Multiply a 4x4 matrix by a scalar (scalar data contained in vector data type)
    //
    inline const Matrix4 operator * (const FloatInVec & scalar) const;

    // Multiply a 4x4 matrix by a 4-D vector
    //
    inline const Vector4 operator * (const Vector4 & vec) const;

    // Multiply a 4x4 matrix by a 3-D vector
    //
    inline const Vector4 operator * (const Vector3 & vec) const;

    // Multiply a 4x4 matrix by a 3-D point
    //
    inline const Vector4 operator * (const Point3 & pnt) const;

    // Multiply two 4x4 matrices
    //
    inline const Matrix4 operator * (const Matrix4 & mat) const;

    // Multiply a 4x4 matrix by a 3x4 transformation matrix
    //
    inline const Matrix4 operator * (const Transform3 & tfrm) const;

    // Perform compound assignment and addition with a 4x4 matrix
    //
    inline Matrix4 & operator += (const Matrix4 & mat);

    // Perform compound assignment and subtraction by a 4x4 matrix
    //
    inline Matrix4 & operator -= (const Matrix4 & mat);

    // Perform compound assignment and multiplication by a scalar
    //
    inline Matrix4 & operator *= (float scalar);

    // Perform compound assignment and multiplication by a scalar (scalar data contained in vector data type)
    //
    inline Matrix4 & operator *= (const FloatInVec & scalar);

    // Perform compound assignment and multiplication by a 4x4 matrix
    //
    inline Matrix4 & operator *= (const Matrix4 & mat);

    // Perform compound assignment and multiplication by a 3x4 transformation matrix
    //
    inline Matrix4 & operator *= (const Transform3 & tfrm);

    // Construct an identity 4x4 matrix
    //
    static inline const Matrix4 identity();

    // Construct a 4x4 matrix to rotate around the x axis
    //
    static inline const Matrix4 rotationX(float radians);

    // Construct a 4x4 matrix to rotate around the y axis
    //
    static inline const Matrix4 rotationY(float radians);

    // Construct a 4x4 matrix to rotate around the z axis
    //
    static inline const Matrix4 rotationZ(float radians);

    // Construct a 4x4 matrix to rotate around the x axis (scalar data contained in vector data type)
    //
    static inline const Matrix4 rotationX(const FloatInVec & radians);

    // Construct a 4x4 matrix to rotate around the y axis (scalar data contained in vector data type)
    //
    static inline const Matrix4 rotationY(const FloatInVec & radians);

    // Construct a 4x4 matrix to rotate around the z axis (scalar data contained in vector data type)
    //
    static inline const Matrix4 rotationZ(const FloatInVec & radians);

    // Construct a 4x4 matrix to rotate around the x, y, and z axes
    //
    static inline const Matrix4 rotationZYX(const Vector3 & radiansXYZ);

    // Construct a 4x4 matrix to rotate around a unit-length 3-D vector
    //
    static inline const Matrix4 rotation(float radians, const Vector3 & unitVec);

    // Construct a 4x4 matrix to rotate around a unit-length 3-D vector (scalar data contained in vector data type)
    //
    static inline const Matrix4 rotation(const FloatInVec & radians, const Vector3 & unitVec);

    // Construct a rotation matrix from a unit-length quaternion
    //
    static inline const Matrix4 rotation(const Quat & unitQuat);

    // Construct a 4x4 matrix to perform scaling
    //
    static inline const Matrix4 scale(const Vector3 & scaleVec);

    // Construct a 4x4 matrix to perform translation
    //
    static inline const Matrix4 translation(const Vector3 & translateVec);

    // Construct viewing matrix based on eye, position looked at, and up direction
    //
    static inline const Matrix4 lookAt(const Point3 & eyePos, const Point3 & lookAtPos, const Vector3 & upVec);

    // Construct a perspective projection matrix based on frustum
    //
    static inline const Matrix4 frustum(float left, float right, float bottom, float top, float zNear, float zFar);

    // Construct an orthographic projection matrix
    //
    static inline const Matrix4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar);
    static inline const Matrix4 orthographicRH(float left, float right, float bottom, float top, float zNear, float zFar);

	//========================================= #TheForgeMathExtensionsBegin ================================================
	// Construct an reversed orthographic projection matrix
	//
	static inline const Matrix4 orthographicReverseZ(float left, float right, float bottom, float top, float zNear, float zFar);

	// Construct a perspective projection matrix using horizontal fov
	// 
	static inline const Matrix4 perspective(float fovxRadians, float aspectInverse, float zNear, float zFar);
   	static inline const Matrix4 perspectiveRH(float fovxRadians, float aspectInverse, float zNear, float zFar);

	static inline const Matrix4 perspectiveReverseZ(float fovxRadians, float aspectInverse, float zNear, float zFar);
    static inline const Matrix4 perspectiveAsymmetricFov(const float leftDegrees, const float rightDegrees, const float upDegrees, const float downDegrees, const float zNear, const float zFar);

	static inline const Matrix4 rotationYX(const float radiansY, const float radiansX);
	static inline const Matrix4 rotationXY(const float radiansX, const float radiansY);

	static inline const Matrix4 cubeProjection(const float zNear, const float zFar);
    static inline const Matrix4 cubeProjectionRH(const float zNear, const float zFar);
	static inline const Matrix4 cubeView(const unsigned int side);
	static void inline extractFrustumClipPlanes(const Matrix4& vp, Vector4& rcp, Vector4& lcp, Vector4& tcp, Vector4& bcp, Vector4& fcp, Vector4& ncp, bool const normalizePlanes);
	//========================================= #TheForgeMathExtensionsEnd ==================================================
} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 4x4 matrix by a scalar
//
inline const Matrix4 operator * (float scalar, const Matrix4 & mat);

// Multiply a 4x4 matrix by a scalar (scalar data contained in vector data type)
//
inline const Matrix4 operator * (const FloatInVec & scalar, const Matrix4 & mat);

// Append (post-multiply) a scale transformation to a 4x4 matrix
// NOTE:
// Faster than creating and multiplying a scale transformation matrix.
//
inline const Matrix4 appendScale(const Matrix4 & mat, const Vector3 & scaleVec);

// Prepend (pre-multiply) a scale transformation to a 4x4 matrix
// NOTE:
// Faster than creating and multiplying a scale transformation matrix.
//
inline const Matrix4 prependScale(const Vector3 & scaleVec, const Matrix4 & mat);

// Multiply two 4x4 matrices per element
//
inline const Matrix4 mulPerElem(const Matrix4 & mat0, const Matrix4 & mat1);

// Compute the absolute value of a 4x4 matrix per element
//
inline const Matrix4 absPerElem(const Matrix4 & mat);

// Transpose of a 4x4 matrix
//
inline const Matrix4 transpose(const Matrix4 & mat);

// Compute the inverse of a 4x4 matrix
// NOTE:
// Result is unpredictable when the determinant of mat is equal to or near 0.
//
inline const Matrix4 inverse(const Matrix4 & mat);

// Compute the inverse of a 4x4 matrix, which is expected to be an affine matrix
// NOTE:
// This can be used to achieve better performance than a general inverse when the specified 4x4 matrix meets the given restrictions.
// The result is unpredictable when the determinant of mat is equal to or near 0.
//
inline const Matrix4 affineInverse(const Matrix4 & mat);

// Compute the inverse of a 4x4 matrix, which is expected to be an affine matrix with an orthogonal upper-left 3x3 submatrix
// NOTE:
// This can be used to achieve better performance than a general inverse when the specified 4x4 matrix meets the given restrictions.
//
inline const Matrix4 orthoInverse(const Matrix4 & mat);

// Determinant of a 4x4 matrix
//
inline const FloatInVec determinant(const Matrix4 & mat);

// Conditionally select between two 4x4 matrices
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Matrix4 select(const Matrix4 & mat0, const Matrix4 & mat1, bool select1);

// Conditionally select between two 4x4 matrices (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Matrix4 select(const Matrix4 & mat0, const Matrix4 & mat1, const BoolInVec & select1);

#ifdef VECTORMATH_DEBUG

// Print a 4x4 matrix
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Matrix4 & mat);

// Print a 4x4 matrix and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Matrix4 & mat, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A 3x4 transformation matrix in array-of-structs format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class Transform3
{
    Vector3 mCol0;
    Vector3 mCol1;
    Vector3 mCol2;
    Vector3 mCol3;

public:

    // Default constructor; does no initialization
    //
    inline Transform3() { }

    // Copy a 3x4 transformation matrix
    //
    inline Transform3(const Transform3 & tfrm);

    // Construct a 3x4 transformation matrix containing the specified columns
    //
    inline Transform3(const Vector3 & col0, const Vector3 & col1, const Vector3 & col2, const Vector3 & col3);

    // Construct a 3x4 transformation matrix from a 3x3 matrix and a 3-D vector
    //
    inline Transform3(const Matrix3 & tfrm, const Vector3 & translateVec);

    // Construct a 3x4 transformation matrix from a unit-length quaternion and a 3-D vector
    //
    inline Transform3(const Quat & unitQuat, const Vector3 & translateVec);

    // Set all elements of a 3x4 transformation matrix to the same scalar value
    //
    explicit inline Transform3(float scalar);

    // Set all elements of a 3x4 transformation matrix to the same scalar value (scalar data contained in vector data type)
    //
    explicit inline Transform3(const FloatInVec & scalar);

    // Assign one 3x4 transformation matrix to another
    //
    inline Transform3 & operator = (const Transform3 & tfrm);

    // Set the upper-left 3x3 submatrix
    //
    inline Transform3 & setUpper3x3(const Matrix3 & mat3);

    // Get the upper-left 3x3 submatrix of a 3x4 transformation matrix
    //
    inline const Matrix3 getUpper3x3() const;

    // Set translation component
    //
    inline Transform3 & setTranslation(const Vector3 & translateVec);

    // Get the translation component of a 3x4 transformation matrix
    //
    inline const Vector3 getTranslation() const;

    // Set column 0 of a 3x4 transformation matrix
    //
    inline Transform3 & setCol0(const Vector3 & col0);

    // Set column 1 of a 3x4 transformation matrix
    //
    inline Transform3 & setCol1(const Vector3 & col1);

    // Set column 2 of a 3x4 transformation matrix
    //
    inline Transform3 & setCol2(const Vector3 & col2);

    // Set column 3 of a 3x4 transformation matrix
    //
    inline Transform3 & setCol3(const Vector3 & col3);

    // Get column 0 of a 3x4 transformation matrix
    //
    inline const Vector3 getCol0() const;

    // Get column 1 of a 3x4 transformation matrix
    //
    inline const Vector3 getCol1() const;

    // Get column 2 of a 3x4 transformation matrix
    //
    inline const Vector3 getCol2() const;

    // Get column 3 of a 3x4 transformation matrix
    //
    inline const Vector3 getCol3() const;

    // Set the column of a 3x4 transformation matrix referred to by the specified index
    //
    inline Transform3 & setCol(int col, const Vector3 & vec);

    // Set the row of a 3x4 transformation matrix referred to by the specified index
    //
    inline Transform3 & setRow(int row, const Vector4 & vec);

    // Get the column of a 3x4 transformation matrix referred to by the specified index
    //
    inline const Vector3 getCol(int col) const;

    // Get the row of a 3x4 transformation matrix referred to by the specified index
    //
    inline const Vector4 getRow(int row) const;

    // Subscripting operator to set or get a column
    //
    inline Vector3 & operator[](int col);

    // Subscripting operator to get a column
    //
    inline const Vector3 operator[](int col) const;

    // Set the element of a 3x4 transformation matrix referred to by column and row indices
    //
    inline Transform3 & setElem(int col, int row, float val);

    // Set the element of a 3x4 transformation matrix referred to by column and row indices (scalar data contained in vector data type)
    //
    inline Transform3 & setElem(int col, int row, const FloatInVec & val);

    // Get the element of a 3x4 transformation matrix referred to by column and row indices
    //
    inline const FloatInVec getElem(int col, int row) const;

    // Multiply a 3x4 transformation matrix by a 3-D vector
    //
    inline const Vector3 operator * (const Vector3 & vec) const;

    // Multiply a 3x4 transformation matrix by a 3-D point
    //
    inline const Point3 operator * (const Point3 & pnt) const;

    // Multiply two 3x4 transformation matrices
    //
    inline const Transform3 operator * (const Transform3 & tfrm) const;

    // Perform compound assignment and multiplication by a 3x4 transformation matrix
    //
    inline Transform3 & operator *= (const Transform3 & tfrm);

    // Construct an identity 3x4 transformation matrix
    //
    static inline const Transform3 identity();

    // Construct a 3x4 transformation matrix to rotate around the x axis
    //
    static inline const Transform3 rotationX(float radians);

    // Construct a 3x4 transformation matrix to rotate around the y axis
    //
    static inline const Transform3 rotationY(float radians);

    // Construct a 3x4 transformation matrix to rotate around the z axis
    //
    static inline const Transform3 rotationZ(float radians);

    // Construct a 3x4 transformation matrix to rotate around the x axis (scalar data contained in vector data type)
    //
    static inline const Transform3 rotationX(const FloatInVec & radians);

    // Construct a 3x4 transformation matrix to rotate around the y axis (scalar data contained in vector data type)
    //
    static inline const Transform3 rotationY(const FloatInVec & radians);

    // Construct a 3x4 transformation matrix to rotate around the z axis (scalar data contained in vector data type)
    //
    static inline const Transform3 rotationZ(const FloatInVec & radians);

    // Construct a 3x4 transformation matrix to rotate around the x, y, and z axes
    //
    static inline const Transform3 rotationZYX(const Vector3 & radiansXYZ);

    // Construct a 3x4 transformation matrix to rotate around a unit-length 3-D vector
    //
    static inline const Transform3 rotation(float radians, const Vector3 & unitVec);

    // Construct a 3x4 transformation matrix to rotate around a unit-length 3-D vector (scalar data contained in vector data type)
    //
    static inline const Transform3 rotation(const FloatInVec & radians, const Vector3 & unitVec);

    // Construct a rotation matrix from a unit-length quaternion
    //
    static inline const Transform3 rotation(const Quat & unitQuat);

    // Construct a 3x4 transformation matrix to perform scaling
    //
    static inline const Transform3 scale(const Vector3 & scaleVec);

    // Construct a 3x4 transformation matrix to perform translation
    //
    static inline const Transform3 translation(const Vector3 & translateVec);

} VECTORMATH_ALIGNED_TYPE_POST;

// Append (post-multiply) a scale transformation to a 3x4 transformation matrix
// NOTE:
// Faster than creating and multiplying a scale transformation matrix.
//
inline const Transform3 appendScale(const Transform3 & tfrm, const Vector3 & scaleVec);

// Prepend (pre-multiply) a scale transformation to a 3x4 transformation matrix
// NOTE:
// Faster than creating and multiplying a scale transformation matrix.
//
inline const Transform3 prependScale(const Vector3 & scaleVec, const Transform3 & tfrm);

// Multiply two 3x4 transformation matrices per element
//
inline const Transform3 mulPerElem(const Transform3 & tfrm0, const Transform3 & tfrm1);

// Compute the absolute value of a 3x4 transformation matrix per element
//
inline const Transform3 absPerElem(const Transform3 & tfrm);

// Inverse of a 3x4 transformation matrix
// NOTE:
// Result is unpredictable when the determinant of the left 3x3 submatrix is equal to or near 0.
//
inline const Transform3 inverse(const Transform3 & tfrm);

// Compute the inverse of a 3x4 transformation matrix, expected to have an orthogonal upper-left 3x3 submatrix
// NOTE:
// This can be used to achieve better performance than a general inverse when the specified 3x4 transformation matrix meets the given restrictions.
//
inline const Transform3 orthoInverse(const Transform3 & tfrm);

// Conditionally select between two 3x4 transformation matrices
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
// However, the transfer of select1 to a VMX register may use more processing time than a branch.
// Use the BoolInVec version for better performance.
//
inline const Transform3 select(const Transform3 & tfrm0, const Transform3 & tfrm1, bool select1);

// Conditionally select between two 3x4 transformation matrices (scalar data contained in vector data type)
// NOTE:
// This function uses a conditional select instruction to avoid a branch.
//
inline const Transform3 select(const Transform3 & tfrm0, const Transform3 & tfrm1, const BoolInVec & select1);

#ifdef VECTORMATH_DEBUG

// Print a 3x4 transformation matrix
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Transform3 & tfrm);

// Print a 3x4 transformation matrix and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Transform3 & tfrm, const char * name);

#endif // VECTORMATH_DEBUG

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

//CONFFX_TEST_BEGIN
// ========================================================
// Stores an affine transformation with separate translation, 
// rotation and scale attributes.
// ========================================================


VECTORMATH_ALIGNED_TYPE_PRE class AffineTransform
{

public:

	// Translation affine transformation component.
	Vector3 translation;

	// Rotation affine transformation component.
	Quat rotation;

	// Scale affine transformation component.
	Vector3 scale;

	// Builds an identity transform
	//
	static inline const AffineTransform identity();

} VECTORMATH_ALIGNED_TYPE_POST;

#ifdef VECTORMATH_DEBUG

// Print a transform
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const AffineTransform & tfrm);

// Print a transform and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const AffineTransform & tfrm, const char * name);

#endif // VECTORMATH_DEBUG

//CONFFX_TEST_END

// ========================================================
// A 4-D int vector in array-of-structures format
// (used for math and bitwise operations)
// ========================================================

namespace vector4int
{

    // Returns a Vector4Int vector with all components set to 0.
    inline Vector4Int zero();

    // Returns a Vector4Int vector with all components set to 1.
    inline Vector4Int one();

    // Returns a Vector4Int vector with the x component set to 1 and all the others
    // to 0.
    inline Vector4Int x_axis();

    // Returns a Vector4Int vector with the y component set to 1 and all the others
    // to 0.
    inline Vector4Int y_axis();

    // Returns a Vector4Int vector with the z component set to 1 and all the others
    // to 0.
    inline Vector4Int z_axis();

    // Returns a Vector4Int vector with the w component set to 1 and all the others
    // to 0.
    inline Vector4Int w_axis();

    // Returns a Vector4Int vector with all components set to true (0xffffffff).
    inline Vector4Int all_true();

    // Returns a Vector4Int vector with all components set to false (0).
    inline Vector4Int all_false();

    // Returns a Vector4Int vector with sign bits set to 1.
    inline Vector4Int mask_sign();

    // Returns a Vector4Int vector with all bits set to 1 except sign.
    inline Vector4Int mask_not_sign();

    // Returns a Vector4Int vector with all bits set to 1.
    inline Vector4Int mask_ffff();

    // Returns a Vector4Int vector with all bits set to 0.
    inline Vector4Int mask_0000();

    // Returns a Vector4Int vector with all the bits of the x, y, z components set to
    // 1, while z is set to 0.
    inline Vector4Int mask_fff0();

    // Returns a Vector4Int vector with all the bits of the x component set to 1,
    // while the others are set to 0.
    inline Vector4Int mask_f000();

    // Returns a Vector4Int vector with all the bits of the y component set to 1,
    // while the others are set to 0.
    inline Vector4Int mask_0f00();

    // Returns a Vector4Int vector with all the bits of the z component set to 1,
    // while the others are set to 0.
    inline Vector4Int mask_00f0();

    // Returns a Vector4Int vector with all the bits of the w component set to 1,
    // while the others are set to 0.
    inline Vector4Int mask_000f();

    // Loads _x, _y, _z, _w to the returned vector.
    // r.x = _x
    // r.y = _y
    // r.z = _z
    // r.w = _w
    inline Vector4Int Load(int _x, int _y, int _z, int _w);

    // Loads _x, _y, _z, _w to the returned vector using the following conversion
    // rule.
    // r.x = _x ? 0xffffffff:0
    // r.y = _y ? 0xffffffff:0
    // r.z = _z ? 0xffffffff:0
    // r.w = _w ? 0xffffffff:0
    inline Vector4Int Load(bool _x, bool _y, bool _z, bool _w);

    // Loads _x to the x component of the returned vector using the following
    // conversion rule, and sets y, z and w to 0.
    // r.x = _x ? 0xffffffff:0
    // r.y = 0
    // r.z = 0
    // r.w = 0
    inline Vector4Int LoadX(bool _x);

    // Loads _x to the all the components of the returned vector using the following
    // conversion rule.
    // r.x = _x ? 0xffffffff:0
    // r.y = _x ? 0xffffffff:0
    // r.z = _x ? 0xffffffff:0
    // r.w = _x ? 0xffffffff:0
    inline Vector4Int Load1(bool _x);

    // Loads the 4 values of _f to the returned vector.
    // _i must be aligned to 16 bytes.
    // r.x = _i[0]
    // r.y = _i[1]
    // r.z = _i[2]
    // r.w = _i[3]
    inline Vector4Int LoadPtr(const int* _i);

    // Loads _i[0] to the x component of the returned vector, and sets y, z and w
    // to 0.
    // _i must be aligned to 16 bytes.
    // r.x = _i[0]
    // r.y = 0
    // r.z = 0
    // r.w = 0
    inline Vector4Int LoadXPtr(const int* _i);

    // Loads _i[0] to all the components of the returned vector.
    // _i must be aligned to 16 bytes.
    // r.x = _i[0]
    // r.y = _i[0]
    // r.z = _i[0]
    // r.w = _i[0]
    inline Vector4Int Load1Ptr(const int* _i);

    // Loads the 2 first value of _i to the x and y components of the returned
    // vector. The remaining components are set to 0.
    // _f must be aligned to 4 bytes.
    // r.x = _i[0]
    // r.y = _i[1]
    // r.z = 0
    // r.w = 0
    inline Vector4Int Load2Ptr(const int* _i);

    // Loads the 3 first value of _i to the x, y and z components of the returned
    // vector. The remaining components are set to 0.
    // _f must be aligned to 16 bytes.
    // r.x = _i[0]
    // r.y = _i[1]
    // r.z = _i[2]
    // r.w = 0
    inline Vector4Int Load3Ptr(const int* _i);

    // Loads the 4 values of _f to the returned vector.
    // _i must be aligned to 16 bytes.
    // r.x = _i[0]
    // r.y = _i[1]
    // r.z = _i[2]
    // r.w = _i[3]
    inline Vector4Int LoadPtrU(const int* _i);

    // Loads _i[0] to the x component of the returned vector, and sets y, z and w
    // to 0.
    // _f must be aligned to 4 bytes.
    // r.x = _i[0]
    // r.y = 0
    // r.z = 0
    // r.w = 0
    inline Vector4Int LoadXPtrU(const int* _i);

    // Loads the 4 values of _i to the returned vector.
    // _i must be aligned to 4 bytes.
    // r.x = _i[0]
    // r.y = _i[0]
    // r.z = _i[0]
    // r.w = _i[0]
    inline Vector4Int Load1PtrU(const int* _i);

    // Loads the 2 first value of _i to the x and y components of the returned
    // vector. The remaining components are set to 0.
    // _f must be aligned to 4 bytes.
    // r.x = _i[0]
    // r.y = _i[1]
    // r.z = 0
    // r.w = 0
    inline Vector4Int Load2PtrU(const int* _i);

    // Loads the 3 first value of _i to the x, y and z components of the returned
    // vector. The remaining components are set to 0.
    // _f must be aligned to 4 bytes.
    // r.x = _i[0]
    // r.y = _i[1]
    // r.z = _i[2]
    // r.w = 0
    inline Vector4Int Load3PtrU(const int* _i);

    // Convert from float to integer by rounding the nearest value.
    inline Vector4Int FromFloatRound(const Vector4& _f);

    // Convert from float to integer by truncating.
    inline Vector4Int FromFloatTrunc(const Vector4& _f);

} //namespace vector4int

// Returns the x component of _v as an integer.
inline int GetX(const Vector4Int _v);

// Returns the y component of _v as a integer.
inline int GetY(const Vector4Int _v);

// Returns the z component of _v as a integer.
inline int GetZ(const Vector4Int _v);

// Returns the w component of _v as a integer.
inline int GetW(const Vector4Int _v);

// Returns _v with the x component set to _i.
inline Vector4Int SetX(const Vector4Int _v, int _i);

// Returns _v with the y component set to _i.
inline Vector4Int SetY(const Vector4Int _v, int _i);

// Returns _v with the z component set to _i.
inline Vector4Int SetZ(const Vector4Int _v, int _i);

// Returns _v with the w component set to _i.
inline Vector4Int SetW(const Vector4Int _v, int _i);

// Returns _v with the _ith component set to _i.
// _i must be in range [0,3]
inline Vector4Int SetI(const Vector4Int _v, int _ith, int _i);

// Stores the 4 components of _v to the four first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
// _i[3] = _v.w
inline void StorePtr(const Vector4Int _v, int* _i);

// Stores the x component of _v to the first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
inline void Store1Ptr(const Vector4Int _v, int* _i);

// Stores x and y components of _v to the two first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
inline void Store2Ptr(const Vector4Int _v, int* _i);

// Stores x, y and z components of _v to the three first integers of _i.
// _i must be aligned to 16 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
inline void Store3Ptr(const Vector4Int _v, int* _i);

// Stores the 4 components of _v to the four first integers of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
// _i[3] = _v.w
inline void StorePtrU(const Vector4Int _v, int* _i);

// Stores the x component of _v to the first float of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
inline void Store1PtrU(const Vector4Int _v, int* _i);

// Stores x and y components of _v to the two first integers of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
inline void Store2PtrU(const Vector4Int _v, int* _i);

// Stores x, y and z components of _v to the three first integers of _i.
// _i must be aligned to 4 bytes.
// _i[0] = _v.x
// _i[1] = _v.y
// _i[2] = _v.z
inline void Store3PtrU(const Vector4Int _v, int* _i);

// Replicates x of _a to all the components of the returned vector.
inline Vector4Int SplatX(const Vector4Int _v);

// Replicates y of _a to all the components of the returned vector.
inline Vector4Int SplatY(const Vector4Int _v);

// Replicates z of _a to all the components of the returned vector.
inline Vector4Int SplatZ(const Vector4Int _v);

// Replicates w of _a to all the components of the returned vector.
inline Vector4Int SplatW(const Vector4Int _v);

// Creates a 4-bit mask from the most significant bits of each component of _v.
// i := sign(a3)<<3 | sign(a2)<<2 | sign(a1)<<1 | sign(a0)
inline int MoveMask(const Vector4Int _v);

// Returns true if all the components of _v are not 0.
inline bool AreAllTrue(const Vector4Int _v);

// Returns true if x, y and z components of _v are not 0.
inline bool AreAllTrue3(const Vector4Int _v);

// Returns true if x and y components of _v are not 0.
inline bool AreAllTrue2(const Vector4Int _v);

// Returns true if x component of _v is not 0.
inline bool AreAllTrue1(const Vector4Int _v);

// Returns true if all the components of _v are 0.
inline bool AreAllFalse(const Vector4Int _v);

// Returns true if x, y and z components of _v are 0.
inline bool AreAllFalse3(const Vector4Int _v);

// Returns true if x and y components of _v are 0.
inline bool AreAllFalse2(const Vector4Int _v);

// Returns true if x component of _v is 0.
inline bool AreAllFalse1(const Vector4Int _v);

// Computes the (horizontal) addition of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
inline Vector4Int HAdd2(const Vector4Int _v);

// Computes the (horizontal) addition of x, y and z components of _v. The result
// is stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y + _a.z
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
inline Vector4Int HAdd3(const Vector4Int _v);

// Computes the (horizontal) addition of x and y components of _v. The result is
// stored in the x component of the returned value. y, z, w of the returned
// vector are the same as their respective components in _v.
// r.x = _a.x + _a.y + _a.z + _a.w
// r.y = _a.y
// r.z = _a.z
// r.w = _a.w
inline Vector4Int HAdd4(const Vector4Int _v);

// Returns the per element absolute value of _v.
inline Vector4Int Abs(const Vector4Int _v);

// Returns the sign bit of _v.
inline Vector4Int Sign(const Vector4Int _v);

// Returns the per component minimum of _a and _b.
inline Vector4Int Min(const Vector4Int _a, const Vector4Int _b);

// Returns the per component maximum of _a and _b.
inline Vector4Int Max(const Vector4Int _a, const Vector4Int _b);

// Returns the per component minimum of _v and 0.
inline Vector4Int Min0(const Vector4Int _v);

// Returns the per component maximum of _v and 0.
inline Vector4Int Max0(const Vector4Int _v);

// Clamps each element of _x between _a and _b.
// Result is unknown if _a is not less or equal to _b.
inline Vector4Int Clamp(const Vector4Int _a, const Vector4Int _v, const Vector4Int _b);

// Returns Per bit selection of vectors _true and _false according to _b.
// _v[0...127] = _b[0...127] ? _true[0...127]:_false[0...127]
inline Vector4Int Select(const Vector4Int _b, const Vector4Int _true, const Vector4Int _false);

// Returns per element binary and operation of _a and _b.
// _v[0...127] = _a[0...127] & _b[0...127]
inline Vector4Int And(const Vector4Int _a, const Vector4Int _b);

// Returns per element binary and operation of _a and _b.
// _v[0...127] = _a[0...127] & _b[0...127]
inline Vector4Int And(const Vector4Int& a, const BoolInVec b);

// Returns per element binary or operation of _a and _b.
// _v[0...127] = _a[0...127] | _b[0...127]
inline Vector4Int Or(const Vector4Int _a, const Vector4Int _b);

// Returns per element binary logical xor operation of _a and _b.
// _v[0...127] = _a[0...127] ^ _b[0...127]
inline Vector4Int Xor(const Vector4Int _a, const Vector4Int _b);

// Returns per element binary complement of _v.
// _v[0...127] = ~_b[0...127]
inline Vector4Int Not(const Vector4Int _v);

// Shifts the 4 signed or unsigned 32-bit integers in a left by count _bits
// while shifting in zeros.
inline Vector4Int ShiftL(const Vector4Int _v, int _bits);

// Shifts the 4 signed 32-bit integers in a right by count bits while shifting
// in the sign bit.
inline Vector4Int ShiftR(const Vector4Int _v, int _bits);

// Shifts the 4 signed or unsigned 32-bit integers in a right by count bits
// while shifting in zeros.
inline Vector4Int ShiftRu(const Vector4Int _v, int _bits);

// Per element "equal" comparison of _a and _b.
inline Vector4Int CmpEq(const Vector4Int _a, const Vector4Int _b);

// Per element "not equal" comparison of _a and _b.
inline Vector4Int CmpNe(const Vector4Int _a, const Vector4Int _b);

// Per element "less than" comparison of _a and _b.
inline Vector4Int CmpLt(const Vector4Int _a, const Vector4Int _b);

// Per element "less than or equal" comparison of _a and _b.
inline Vector4Int CmpLe(const Vector4Int _a, const Vector4Int _b);

// Per element "greater than" comparison of _a and _b.
inline Vector4Int CmpGt(const Vector4Int _a, const Vector4Int _b);

// Per element "greater than or equal" comparison of _a and _b.
inline Vector4Int CmpGe(const Vector4Int _a, const Vector4Int _b);

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
// ========================================================
// A 3-D vector in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class IVector3
{
	__m128i mVec128;

public:

	// Default constructor; does no initialization
	//
	inline IVector3() {}

	// Construct a 3-D vector from x, y, and z elements
	//
	inline IVector3(int x, int y, int z);

	// Set all elements of a 3-D vector to the same scalar value
	//
	explicit inline IVector3(int scalar);

	// Set vector int data in a 3-D vector
	//
	explicit inline IVector3(__m128i vi4);

	// Get vector int data from a 3-D vector
	//
	inline __m128i get128() const;

	// Assign one 3-D vector to another
	//
	inline IVector3 & operator = (const IVector3 & vec);

	// Set the x element of a 3-D vector
	//
	inline IVector3 & setX(int x);

	// Set the y element of a 3-D vector
	//
	inline IVector3 & setY(int y);

	// Set the z element of a 3-D vector
	//
	inline IVector3 & setZ(int z);

	// Set the w element of a padded 3-D vector
	// NOTE:
	// You are free to use the additional w component - if never set, it's value is undefined.
	//
	inline IVector3 & setW(int w);

	// Get the x element of a 3-D vector
	//
	inline const int getX() const;

	// Get the y element of a 3-D vector
	//
	inline const int getY() const;

	// Get the z element of a 3-D vector
	//
	inline const int getZ() const;

	// Get the w element of a padded 3-D vector
	// NOTE:
	// You are free to use the additional w component - if never set, it's value is undefined.
	//
	inline const int getW() const;

	// Set an x, y, or z element of a 3-D vector by index
	//
	inline IVector3 & setElem(int idx, int value);

	// Get an x, y, or z element of a 3-D vector by index
	//
	inline const int getElem(int idx) const;

	// Subscripting operator to set or get an element
	//
	inline IVecIdx operator[](int idx);

	// Subscripting operator to get an element
	//
	inline const int operator[](int idx) const;

	// Add two 3-D vectors
	//
	inline const IVector3 operator + (const IVector3 & vec) const;

	// Subtract a 3-D vector from another 3-D vector
	//
	inline const IVector3 operator - (const IVector3 & vec) const;

	// Multiply a 3-D vector by a scalar
	//
	inline const IVector3 operator * (int scalar) const;

	// Divide a 3-D vector by a scalar
	//
	inline const IVector3 operator / (int scalar) const;

	// Perform compound assignment and addition with a 3-D vector
	//
	inline IVector3 & operator += (const IVector3 & vec);

	// Perform compound assignment and subtraction by a 3-D vector
	//
	inline IVector3 & operator -= (const IVector3 & vec);

	// Perform compound assignment and multiplication by a scalar
	//
	inline IVector3 & operator *= (int scalar);

	// Perform compound assignment and division by a scalar
	//
	inline IVector3 & operator /= (int scalar);

	// Negate all elements of a 3-D vector
	//
	inline const IVector3 operator - () const;

	// Construct x axis
	//
	static inline const IVector3 xAxis();

	// Construct y axis
	//
	static inline const IVector3 yAxis();

	// Construct z axis
	//
	static inline const IVector3 zAxis();

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 3-D vector by a scalar
//
inline const IVector3 operator * (int scalar, const IVector3 & vec);

// Multiply two 3-D vectors per element
//
inline const IVector3 mulPerElem(const IVector3 & vec0, const IVector3 & vec1);

// Divide two 3-D vectors per element
//
inline const IVector3 divPerElem(const IVector3 & vec0, const IVector3 & vec1);

// Compute the absolute value of a 3-D vector per element
//
inline const IVector3 absPerElem(const IVector3 & vec);

// Copy sign from one 3-D vector to another, per element
//
inline const IVector3 copySignPerElem(const IVector3 & vec0, const IVector3 & vec1);

// Maximum of two 3-D vectors per element
//
inline const IVector3 maxPerElem(const IVector3 & vec0, const IVector3 & vec1);

// Minimum of two 3-D vectors per element
//
inline const IVector3 minPerElem(const IVector3 & vec0, const IVector3 & vec1);

// Maximum element of a 3-D vector
//
inline const int maxElem(const IVector3 & vec);

// Minimum element of a 3-D vector
//
inline const int minElem(const IVector3 & vec);

// Compute the sum of all elements of a 3-D vector
//
inline const int sum(const IVector3 & vec);

#ifdef VECTORMATH_DEBUG

// Print a 3-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const IVector3 & vec);

// Print a 3-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const IVector3 & vec, const char * name);

#endif // VECTORMATH_DEBUG

typedef unsigned int uint;
// ========================================================
// A 3-D vector in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class UVector3
{
	__m128i mVec128;

public:

	// Default constructor; does no initialization
	//
	inline UVector3() {}

	// Construct a 3-D vector from x, y, and z elements
	//
	inline UVector3(uint x, uint y, uint z);

	// Set all elements of a 3-D vector to the same scalar value
	//
	explicit inline UVector3(uint scalar);

	// Set vector uint data in a 3-D vector
	//
	explicit inline UVector3(__m128i vu4);

	// Get vector uint data from a 3-D vector
	//
	inline __m128i get128() const;

	// Assign one 3-D vector to another
	//
	inline UVector3 & operator = (const UVector3 & vec);

	// Set the x element of a 3-D vector
	//
	inline UVector3 & setX(uint x);

	// Set the y element of a 3-D vector
	//
	inline UVector3 & setY(uint y);

	// Set the z element of a 3-D vector
	//
	inline UVector3 & setZ(uint z);

	// Set the w element of a padded 3-D vector
	// NOTE:
	// You are free to use the additional w component - if never set, it's value is undefined.
	//
	inline UVector3 & setW(uint w);

	// Get the x element of a 3-D vector
	//
	inline const uint getX() const;

	// Get the y element of a 3-D vector
	//
	inline const uint getY() const;

	// Get the z element of a 3-D vector
	//
	inline const uint getZ() const;

	// Get the w element of a padded 3-D vector
	// NOTE:
	// You are free to use the additional w component - if never set, it's value is undefined.
	//
	inline const uint getW() const;

	// Set an x, y, or z element of a 3-D vector by index
	//
	inline UVector3 & setElem(uint idx, uint value);

	// Get an x, y, or z element of a 3-D vector by index
	//
	inline const uint getElem(uint idx) const;

	// Subscripting operator to set or get an element
	//
	inline IVecIdx operator[](uint idx);

	// Subscripting operator to get an element
	//
	inline const uint operator[](uint idx) const;

	// Add two 3-D vectors
	//
	inline const UVector3 operator + (const UVector3 & vec) const;

	// Subtract a 3-D vector from another 3-D vector
	//
	inline const UVector3 operator - (const UVector3 & vec) const;

	// Multiply a 3-D vector by a scalar
	//
	inline const UVector3 operator * (uint scalar) const;

	// Divide a 3-D vector by a scalar
	//
	inline const UVector3 operator / (uint scalar) const;

	// Perform compound assignment and addition with a 3-D vector
	//
	inline UVector3 & operator += (const UVector3 & vec);

	// Perform compound assignment and subtraction by a 3-D vector
	//
	inline UVector3 & operator -= (const UVector3 & vec);

	// Perform compound assignment and multiplication by a scalar
	//
	inline UVector3 & operator *= (uint scalar);

	// Perform compound assignment and division by a scalar
	//
	inline UVector3 & operator /= (uint scalar);

	// Negate all elements of a 3-D vector
	//
	inline const UVector3 operator - () const;

	// Construct x axis
	//
	static inline const UVector3 xAxis();

	// Construct y axis
	//
	static inline const UVector3 yAxis();

	// Construct z axis
	//
	static inline const UVector3 zAxis();

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 3-D vector by a scalar
//
inline const UVector3 operator * (uint scalar, const UVector3 & vec);

// Multiply two 3-D vectors per element
//
inline const UVector3 mulPerElem(const UVector3 & vec0, const UVector3 & vec1);

// Divide two 3-D vectors per element
//
inline const UVector3 divPerElem(const UVector3 & vec0, const UVector3 & vec1);

// Maximum of two 3-D vectors per element
//
inline const UVector3 maxPerElem(const UVector3 & vec0, const UVector3 & vec1);

// Minimum of two 3-D vectors per element
//
inline const UVector3 minPerElem(const UVector3 & vec0, const UVector3 & vec1);

// Maximum element of a 3-D vector
//
inline const uint maxElem(const UVector3 & vec);

// Minimum element of a 3-D vector
//
inline const uint minElem(const UVector3 & vec);

// Compute the sum of all elements of a 3-D vector
//
inline const uint sum(const UVector3 & vec);

#ifdef VECTORMATH_DEBUG

// Print a 3-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const UVector3 & vec);

// Print a 3-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const UVector3 & vec, const char * name);

#endif // VECTORMATH_DEBUG


// ========================================================
// A 4-D vector in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class IVector4
{
	__m128i mVec128;

public:

	// Default constructor; does no initialization
	//
	inline IVector4() {}

	// Construct a 4-D vector from x, y, z and w elements
	//
	inline IVector4(int x, int y, int z, int w);

	// Set all elements of a 4-D vector to the same scalar value
	//
	explicit inline IVector4(int scalar);

	// Set vector int data in a 4-D vector
	//
	explicit inline IVector4(__m128i vi4);

	// Get vector int data from a 4-D vector
	//
	inline __m128i get128() const;

	// Assign one 4-D vector to another
	//
	inline IVector4 & operator = (const IVector4 & vec);

	// Set the x element of a 4-D vector
	//
	inline IVector4 & setX(int x);

	// Set the y element of a 4-D vector
	//
	inline IVector4 & setY(int y);

	// Set the z element of a 4-D vector
	//
	inline IVector4 & setZ(int z);

	// Set the w element of a padded 4-D vector
	//
	inline IVector4 & setW(int w);

	// Get the x element of a 4-D vector
	//
	inline const int getX() const;

	// Get the y element of a 4-D vector
	//
	inline const int getY() const;

	// Get the z element of a 4-D vector
	//
	inline const int getZ() const;

	// Get the w element of a padded 4-D vector
	//
	inline const int getW() const;

	// Set an x, y, or z element of a 4-D vector by index
	//
	inline IVector4 & setElem(int idx, int value);

	// Get an x, y, or z element of a 4-D vector by index
	//
	inline const int getElem(int idx) const;

	// Subscripting operator to set or get an element
	//
	inline IVecIdx operator[](int idx);

	// Subscripting operator to get an element
	//
	inline const int operator[](int idx) const;

	// Add two 4-D vectors
	//
	inline const IVector4 operator + (const IVector4 & vec) const;

	// Subtract a 4-D vector from another 4-D vector
	//
	inline const IVector4 operator - (const IVector4 & vec) const;

	// Multiply a 4-D vector by a scalar
	//
	inline const IVector4 operator * (int scalar) const;

	// Divide a 4-D vector by a scalar
	//
	inline const IVector4 operator / (int scalar) const;

	// Perform compound assignment and addition with a 4-D vector
	//
	inline IVector4 & operator += (const IVector4 & vec);

	// Perform compound assignment and subtraction by a 4-D vector
	//
	inline IVector4 & operator -= (const IVector4 & vec);

	// Perform compound assignment and multiplication by a scalar
	//
	inline IVector4 & operator *= (int scalar);

	// Perform compound assignment and division by a scalar
	//
	inline IVector4 & operator /= (int scalar);

	// Negate all elements of a 4-D vector
	//
	inline const IVector4 operator - () const;

	// Construct x axis
	//
	static inline const IVector4 xAxis();

	// Construct y axis
	//
	static inline const IVector4 yAxis();

	// Construct z axis
	//
	static inline const IVector4 zAxis();

	// Construct w axis
	//
	static inline const IVector4 wAxis();

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 4-D vector by a scalar
//
inline const IVector4 operator * (int scalar, const IVector4 & vec);

// Multiply two 4-D vectors per element
//
inline const IVector4 mulPerElem(const IVector4 & vec0, const IVector4 & vec1);

// Divide two 4-D vectors per element
//
inline const IVector4 divPerElem(const IVector4 & vec0, const IVector4 & vec1);

// Compute the absolute value of a 4-D vector per element
//
inline const IVector4 absPerElem(const IVector4 & vec);

// Copy sign from one 4-D vector to another, per element
//
inline const IVector4 copySignPerElem(const IVector4 & vec0, const IVector4 & vec1);

// Maximum of two 4-D vectors per element
//
inline const IVector4 maxPerElem(const IVector4 & vec0, const IVector4 & vec1);

// Minimum of two 4-D vectors per element
//
inline const IVector4 minPerElem(const IVector4 & vec0, const IVector4 & vec1);

// Maximum element of a 4-D vector
//
inline const int maxElem(const IVector4 & vec);

// Minimum element of a 4-D vector
//
inline const int minElem(const IVector4 & vec);

// Compute the sum of all elements of a 4-D vector
//
inline const int sum(const IVector4 & vec);

#ifdef VECTORMATH_DEBUG

// Print a 4-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const IVector4 & vec);

// Print a 4-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const IVector4 & vec, const char * name);

#endif // VECTORMATH_DEBUG


// ========================================================
// A 4-D vector in array-of-structures format
// ========================================================

VECTORMATH_ALIGNED_TYPE_PRE class UVector4
{
	__m128i mVec128;

public:

	// Default constructor; does no initialization
	//
	inline UVector4() {}

	// Construct a 4-D vector from x, y, z and welements
	//
	inline UVector4(uint x, uint y, uint z, uint w);

	// Set all elements of a 4-D vector to the same scalar value
	//
	explicit inline UVector4(uint scalar);

	// Set vector uint data in a 4-D vector
	//
	explicit inline UVector4(__m128i vu4);

	// Get vector uint data from a 4-D vector
	//
	inline __m128i get128() const;

	// Assign one 4-D vector to another
	//
	inline UVector4 & operator = (const UVector4 & vec);

	// Set the x element of a 4-D vector
	//
	inline UVector4 & setX(uint x);

	// Set the y element of a 4-D vector
	//
	inline UVector4 & setY(uint y);

	// Set the z element of a 4-D vector
	//
	inline UVector4 & setZ(uint z);

	// Set the w element of a padded 4-D vector
	//
	inline UVector4 & setW(uint w);

	// Get the x element of a 4-D vector
	//
	inline const uint getX() const;

	// Get the y element of a 4-D vector
	//
	inline const uint getY() const;

	// Get the z element of a 4-D vector
	//
	inline const uint getZ() const;

	// Get the w element of a padded 4-D vector
	//
	inline const uint getW() const;

	// Set an x, y, or z element of a 4-D vector by index
	//
	inline UVector4 & setElem(uint idx, uint value);

	// Get an x, y, or z element of a 4-D vector by index
	//
	inline const uint getElem(uint idx) const;

	// Subscripting operator to set or get an element
	//
	inline IVecIdx operator[](uint idx);

	// Subscripting operator to get an element
	//
	inline const uint operator[](uint idx) const;

	// Add two 4-D vectors
	//
	inline const UVector4 operator + (const UVector4 & vec) const;

	// Subtract a 4-D vector from another 4-D vector
	//
	inline const UVector4 operator - (const UVector4 & vec) const;

	// Multiply a 4-D vector by a scalar
	//
	inline const UVector4 operator * (uint scalar) const;

	// Divide a 4-D vector by a scalar
	//
	inline const UVector4 operator / (uint scalar) const;

	// Perform compound assignment and addition with a 4-D vector
	//
	inline UVector4 & operator += (const UVector4 & vec);

	// Perform compound assignment and subtraction by a 4-D vector
	//
	inline UVector4 & operator -= (const UVector4 & vec);

	// Perform compound assignment and multiplication by a scalar
	//
	inline UVector4 & operator *= (uint scalar);

	// Perform compound assignment and division by a scalar
	//
	inline UVector4 & operator /= (uint scalar);

	// Negate all elements of a 4-D vector
	//
	inline const UVector4 operator - () const;

	// Construct x axis
	//
	static inline const UVector4 xAxis();

	// Construct y axis
	//
	static inline const UVector4 yAxis();

	// Construct z axis
	//
	static inline const UVector4 zAxis();

	// Construct w axis
	//
	static inline const UVector4 wAxis();

} VECTORMATH_ALIGNED_TYPE_POST;

// Multiply a 4-D vector by a scalar
//
inline const UVector4 operator * (uint scalar, const UVector4 & vec);

// Multiply two 4-D vectors per element
//
inline const UVector4 mulPerElem(const UVector4 & vec0, const UVector4 & vec1);

// Divide two 4-D vectors per element
//
inline const UVector4 divPerElem(const UVector4 & vec0, const UVector4 & vec1);

// Maximum of two 4-D vectors per element
//
inline const UVector4 maxPerElem(const UVector4 & vec0, const UVector4 & vec1);

// Minimum of two 4-D vectors per element
//
inline const UVector4 minPerElem(const UVector4 & vec0, const UVector4 & vec1);

// Maximum element of a 4-D vector
//
inline const uint maxElem(const UVector4 & vec);

// Minimum element of a 4-D vector
//
inline const uint minElem(const UVector4 & vec);

// Compute the sum of all elements of a 4-D vector
//
inline const uint sum(const UVector4 & vec);

#ifdef VECTORMATH_DEBUG

// Print a 4-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const UVector4 & vec);

// Print a 4-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const UVector4 & vec, const char * name);

#endif // VECTORMATH_DEBUG

//========================================= #TheForgeMathExtensionsEnd ================================================

} // namespace Neon
} // namespace Vectormath

// Inline implementations:
#include "vector.hpp"
#include "quaternion.hpp"
#include "matrix.hpp"

#endif // VECTORMATH_NEON_VECTORMATH_HPP
