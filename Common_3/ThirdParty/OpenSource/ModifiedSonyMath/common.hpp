
// ================================================================================================
// -*- C++ -*-
// File: vectormath/common.hpp
// Author: Guilherme R. Lampert
// Created on: 30/12/16
// Brief: Extra helper functions added to the Vectormath library.
// ================================================================================================

#ifndef VECTORMATH_COMMON_HPP
#define VECTORMATH_COMMON_HPP

#ifndef conf_malloc
extern void* conf_malloc(size_t size);
#endif
#ifndef conf_free
extern void conf_free(void* ptr);
#endif

namespace Vectormath
{

inline float * toFloatPtr(Point2  & p)    { return reinterpret_cast<float *>(&p); } //  2 floats - default alignment
inline float * toFloatPtr(Point3  & p)    { return reinterpret_cast<float *>(&p); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Vector2 & v)    { return reinterpret_cast<float *>(&v); } //  2 floats - default alignment
inline float * toFloatPtr(Vector3 & v)    { return reinterpret_cast<float *>(&v); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Vector4 & v)    { return reinterpret_cast<float *>(&v); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Quat    & q)    { return reinterpret_cast<float *>(&q); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Matrix3 & m)    { return reinterpret_cast<float *>(&m); } // 12 floats - 16 bytes aligned
inline float * toFloatPtr(Matrix4 & m)    { return reinterpret_cast<float *>(&m); } // 16 floats - 16 bytes aligned
inline float * toFloatPtr(Transform3 & t) { return reinterpret_cast<float *>(&t); } // 16 floats - 16 bytes aligned

inline const float * toFloatPtr(const Point2  & p)    { return reinterpret_cast<const float *>(&p); }
inline const float * toFloatPtr(const Point3  & p)    { return reinterpret_cast<const float *>(&p); }
inline const float * toFloatPtr(const Vector2 & v)    { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Vector3 & v)    { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Vector4 & v)    { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Quat    & q)    { return reinterpret_cast<const float *>(&q); }
inline const float * toFloatPtr(const Matrix3 & m)    { return reinterpret_cast<const float *>(&m); }
inline const float * toFloatPtr(const Matrix4 & m)    { return reinterpret_cast<const float *>(&m); }
inline const float * toFloatPtr(const Transform3 & t) { return reinterpret_cast<const float *>(&t); }

// Shorthand to discard the last element of a Vector4 and get a Point3.
inline Point3 toPoint3(const Vector4 & v4)
{
    return Point3(v4[0], v4[1], v4[2]);
}

// Convert from world (global) coordinates to local model coordinates.
// Input matrix must be the inverse of the model matrix, e.g.: 'inverse(modelMatrix)'.
inline Point3 worldPointToModel(const Matrix4 & invModelToWorldMatrix, const Point3 & point)
{
    return toPoint3(invModelToWorldMatrix * point);
}

// Makes a plane projection matrix that can be used for simple object shadow effects.
// The W component of the light position vector should be 1 for a point light and 0 for directional.
inline Matrix4 makeShadowMatrix(const Vector4 & plane, const Vector4 & light)
{
    Matrix4 shadowMat;
    const auto dot = (plane[0] * light[0]) +
                     (plane[1] * light[1]) +
                     (plane[2] * light[2]) +
                     (plane[3] * light[3]);

    shadowMat[0][0] = dot - (light[0] * plane[0]);
    shadowMat[1][0] =     - (light[0] * plane[1]);
    shadowMat[2][0] =     - (light[0] * plane[2]);
    shadowMat[3][0] =     - (light[0] * plane[3]);

    shadowMat[0][1] =     - (light[1] * plane[0]);
    shadowMat[1][1] = dot - (light[1] * plane[1]);
    shadowMat[2][1] =     - (light[1] * plane[2]);
    shadowMat[3][1] =     - (light[1] * plane[3]);

    shadowMat[0][2] =     - (light[2] * plane[0]);
    shadowMat[1][2] =     - (light[2] * plane[1]);
    shadowMat[2][2] = dot - (light[2] * plane[2]);
    shadowMat[3][2] =     - (light[2] * plane[3]);

    shadowMat[0][3] =     - (light[3] * plane[0]);
    shadowMat[1][3] =     - (light[3] * plane[1]);
    shadowMat[2][3] =     - (light[3] * plane[2]);
    shadowMat[3][3] = dot - (light[3] * plane[3]);

    return shadowMat;
}

} // namespace Vectormath

//========================================= #ConfettiMathExtensionsBegin ================================================

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _ANDROID
#include "../../Common_2/Code/Renderer/Android/AndroidDefines.h"
#endif

#ifndef TARGET_IOS
#include <immintrin.h>
#endif

#include "../../../OS/Core/Compiler.h"

/*
* Copyright (c) 2018 Confetti Interactive Inc.
*
* This file is part of The-Forge
* (see https://github.com/ConfettiFX/The-Forge).
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/

//****************************************************************************
// List of added functionalities:
// - Vector2/3/4 comparison operators
// - half float
// - float2/3/4
// - float* <-> vec* conversions
// - float operations (lerp, saturate, utils, etc.)
// - Mesh Generator
// - Intersection helpers
// - Noise
//****************************************************************************

namespace Vectormath
{
// constants
#define PI 3.14159265358979323846f
static const float piDivTwo = 1.570796326794896619231f;        //!< pi/2 constant

//----------------------------------------------------------------------------
// Comparison operators for Vector2/3/
//----------------------------------------------------------------------------
inline bool operator == (const Vector2 &u, const Vector2 &v) { return (u.getX() == v.getX() && u.getY() == v.getY()); }
inline bool operator != (const Vector2 &u, const Vector2 &v) { return !(u == v); }
inline bool operator == (const Vector3 &u, const Vector3 &v) 
{
	//Compare the X, Y, and Z values
#if VECTORMATH_MODE_SCALAR
	return
		u.getX() == v.getX() &&
		u.getY() == v.getY() &&
		u.getZ() == v.getZ();
#else
	//(We actually compare X, Y, Z, and W, but mask away the result of W
	return (_mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) & 0x7) == 0x7;
#endif
}
inline bool operator != (const Vector3 &u, const Vector3 &v)
{
#if VECTORMATH_MODE_SCALAR
	return !operator==(u, v);
#else
	//Compare the X, Y, and Z values
	//(We actually compare X, Y, Z, and W, but mask away the result of W
	return (_mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) & 0x7) != 0x7;
#endif
}
inline bool operator == (const Vector4 &u, const Vector4 &v)
{
#if VECTORMATH_MODE_SCALAR
	return
		u.getX() == v.getX() &&
		u.getY() == v.getY() &&
		u.getZ() == v.getZ() &&
		u.getW() == v.getW();
#else
	//Compare the X, Y, Z, and W values
	return _mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) == 0xf;
#endif
}
inline bool operator != (const Vector4 &u, const Vector4 &v)
{
#if VECTORMATH_MODE_SCALAR
	return !operator==(u, v);
#else
	//Compare the X, Y, Z, and W values
	return _mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) != 0xf;
#endif
}

//----------------------------------------------------------------------------
// half float
//----------------------------------------------------------------------------
struct half
{
	unsigned short sh;

	half() {};
	inline half(const float x)
	{
		union {
			float floatI;
			unsigned int i;
		};
		floatI = x;

		//	unsigned int i = *((unsigned int *) &mX);
		int e = ((i >> 23) & 0xFF) - 112;
		int m = i & 0x007FFFFF;

		sh = (i >> 16) & 0x8000;
		if (e <= 0) {
			// Denorm
			m = ((m | 0x00800000) >> (1 - e)) + 0x1000;
			sh |= (m >> 13);
		}
		else if (e == 143) {
			sh |= 0x7C00;
			if (m != 0) {
				// NAN
				m >>= 13;
				sh |= m | (m == 0);
			}
		}
		else {
			m += 0x1000;
			if (m & 0x00800000) {
				// Mantissa overflow
				m = 0;
				e++;
			}
			if (e >= 31) {
				// Exponent overflow
				sh |= 0x7C00;
			}
			else {
				sh |= (e << 10) | (m >> 13);
			}
		}
	}

	inline operator float() const
	{
		union {
			unsigned int s;
			float result;
		};

		s = (sh & 0x8000) << 16;
		unsigned int e = (sh >> 10) & 0x1F;
		unsigned int m = sh & 0x03FF;

		if (e == 0) {
			// +/- 0
			if (m == 0) return result;

			// Denorm
			while ((m & 0x0400) == 0) {
				m += m;
				e--;
			}
			e++;
			m &= ~0x0400;
		}
		else if (e == 31) {
			// INF / NAN
			s |= 0x7F800000 | (m << 13);
			return result;
		}

		s |= ((e + 112) << 23) | (m << 13);

		return result;
	}
};

#define DEFINE_FLOAT_X
#ifdef DEFINE_FLOAT_X
//----------------------------------------------------------------------------
// float2
//----------------------------------------------------------------------------
// A simple structure containing 2 floating point values.
// float2 is always guaranteed to be 2 floats in size. Only use when a very
// specific size is required (like defining structures that need to be the
// same across platforms, or the same on CPU and GPU (like constant and
// structured buffers) In all other cases you should opt to use Vector2, since
// it uses SIMD optimizations whenever possible. float2 does not.
//----------------------------------------------------------------------------
struct float2
{
	float2() = default;
	float2(float x, float y) : x(x), y(y) {}
	float2(float x) : x(x), y(x) {}
	float2(const float2& f) : x(f.x), y(f.y) {}

	float& operator[](int i) { return (&x)[i]; }
	float operator[](int i) const { return (&x)[i]; }

	float getX() const { return x; }
	float getY() const { return y; }

	void setX(float x_) { x = x_; }
	void setY(float y_) { y = y_; }

	//Vector2 toVec2() const { return Vector2(x, y); }

	float x, y;
};

inline float2 operator+(const float2& a, const float2& b) { return float2(a.x + b.x, a.y + b.y); }
inline float2 operator-(const float2& a, const float2& b) { return float2(a.x - b.x, a.y - b.y); }
inline float2 operator-(const float2& a) { return float2(-a.x, -a.y); }
inline float2 operator*(const float2& a, float b) { return float2(a.x * b, a.y * b); }
inline float2 operator*(float a, const float2& b) { return b * a; }
inline float2 operator*(const float2& a, const float2& b) { return float2(a.x * b.x, a.y * b.y); }
inline float2 operator/(const float2& a, float b) { return float2(a.x / b, a.y / b); }
inline float2 operator/(float a, const float2& b) { return b / a; }
inline float2 operator/(const float2& a, const float2& b) { return float2(a.x / b.x, a.y / b.y); }

inline const float2& operator+=(float2& a, const float2& b) { return a = a + b; }
inline const float2& operator-=(float2& a, const float2& b) { return a = a - b; }
inline const float2& operator*=(float2&a, float b) { return a = a * b; }
inline const float2& operator*=(float2&a, float2& b) { return a = a * b; }
inline const float2& operator/=(float2& a, float b) { return a = a / b; }
inline const float2& operator/=(float2&a, const float2& b) { return a = a / b; }


//----------------------------------------------------------------------------
// float3
//----------------------------------------------------------------------------
// A simple structure containing 3 floating point values.
// float3 is always guaranteed to be 3 floats in size. Only use when a very
// specific size is required (like defining structures that need to be the
// same across platforms, or the same on CPU and GPU (like constant and
// structured buffers) In all other cases you should opt to use Vector3, since
// it uses SIMD optimizations whenever possible. float3 does not.
//----------------------------------------------------------------------------
struct float3
{
	float3() = default;
	float3(float x, float y, float z) : x(x), y(y), z(z) {}
	float3(float x) : x(x), y(x), z(x) {}
	float3(const float3& f) : x(f.x), y(f.y), z(f.z) {}

	float& operator[](int i) { return (&x)[i]; }
	float operator[](int i) const { return (&x)[i]; }

	float getX() const { return x; }
	float getY() const { return y; }
	float getZ() const { return z; }
	float2 getXY() const { return float2(x, y); }

	//Vector3 toVec3() const { return Vector3(x, y, z); }

	void setX(float x_) { x = x_; }
	void setY(float y_) { y = y_; }
	void setZ(float z_) { z = z_; }

	float x, y, z;
};

inline float3 operator+(const float3& a, const float3& b) { return float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline float3 operator-(const float3& a, const float3& b) { return float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline float3 operator-(const float3& a) { return float3(-a.x, -a.y, -a.z); }
inline float3 operator*(const float3& a, float b) { return float3(a.x * b, a.y * b, a.z * b); }
inline float3 operator*(float a, const float3& b) { return b * a; }
inline float3 operator*(const float3& a, const float3& b) { return float3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline float3 operator/(const float3& a, float b) { return float3(a.x / b, a.y / b, a.z / b); }
inline float3 operator/(float a, const float3& b) { return b / a; }
inline float3 operator/(const float3& a, const float3& b) { return float3(a.x / b.x, a.y / b.y, a.z / b.z); }

inline const float3& operator+=(float3& a, const float3& b) { return a = a + b; }
inline const float3& operator-=(float3& a, const float3& b) { return a = a - b; }
inline const float3& operator*=(float3&a, float b) { return a = a * b; }
inline const float3& operator*=(float3&a, float3& b) { return a = a * b; }
inline const float3& operator/=(float3& a, float b) { return a = a / b; }
inline const float3& operator/=(float3&a, const float3& b) { return a = a / b; }

//----------------------------------------------------------------------------
// float4
//----------------------------------------------------------------------------
struct float4
{
	float4() = default;
	float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
	float4(float x) : x(x), y(x), z(x), w(x) {}
	float4(const float3& f, float w) : x(f.x), y(f.y), z(f.z), w(w) {}
	float4(const float4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {}

	float& operator[](int i) { return (&x)[i]; }
	float operator[](int i) const { return (&x)[i]; }

	float getX() const { return x; }
	float getY() const { return y; }
	float getZ() const { return z; }
	float getW() const { return w; }
	float2 getXY() const { return float2(x, y); }
	float3 getXYZ() const { return float3(x, y, z); }

	Vector4 toVec4() const { return Vector4(x, y, z, w); }

	void setX(float x_) { x = x_; }
	void setY(float y_) { y = y_; }
	void setZ(float z_) { z = z_; }
	void setW(float w_) { w = w_; }

	float x, y, z, w;
};

inline float4 operator+(const float4& a, const float4& b) { return float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline float4 operator-(const float4& a, const float4& b) { return float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
inline float4 operator-(const float4& a) { return float4(-a.x, -a.y, -a.z, -a.w); }
inline float4 operator*(const float4& a, float b) { return float4(a.x * b, a.y * b, a.z * b, a.w * b); }
inline float4 operator*(float a, const float4& b) { return b * a; }
inline float4 operator*(const float4& a, const float4& b) { return float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
inline float4 operator/(const float4& a, float b) { return float4(a.x / b, a.y / b, a.z / b, a.w / b); }
inline float4 operator/(float a, const float4& b) { return b / a; }
inline float4 operator/(const float4& a, const float4& b) { return float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }

inline const float4& operator+=(float4& a, const float4& b) { return a = a + b; }
inline const float4& operator-=(float4& a, const float4& b) { return a = a - b; }
inline const float4& operator*=(float4&a, float b) { return a = a * b; }
inline const float4& operator*=(float4&a, float4& b) { return a = a * b; }
inline const float4& operator/=(float4&a, const float4& b) { return a = a / b; }
inline const float4& operator/=(float4& a, float b) { return a = a / b; }



//----------------------------------------------------------------------------
// float* to vec* conversions
//----------------------------------------------------------------------------
inline float2 v2ToF2(const Vector2& v2) { return float2(v2.getX(), v2.getY()); }
inline float3 v3ToF3(const Vector3& v3)
{
#if VECTORMATH_MODE_SCALAR
	return float3(v3.getX(), v3.getY(), v3.getZ());
#else
	DEFINE_ALIGNED(float array[4], 16);
	_mm_store_ps(array, v3.get128());
	return float3(array[0], array[1], array[2]);
#endif
}
inline float4 v4ToF4(const Vector4& v4)
{
#if VECTORMATH_MODE_SCALAR
	return float4(v4.getX(), v4.getY(), v4.getZ(), v4.getW());
#else
	DEFINE_ALIGNED(float4 result, 16);
	_mm_store_ps(&result.x, v4.get128());
	return result;
#endif
}

//----------------------------------------------------------------------------
// vec* to float* conversions
//----------------------------------------------------------------------------
inline Vector2 f2Tov2(const float2& f2) { return Vector2(f2.x, f2.y); }
inline Vector3 f3Tov3(const float3& f3) { return Vector3(f3.x, f3.y, f3.z); }
inline Vector4 f4Tov4(const float4& f4) { return Vector4(f4.x, f4.y, f4.z, f4.w); }
#endif

//----------------------------------------------------------------------------
// Float operations.
//----------------------------------------------------------------------------
//	doing this to be able to instantiate vector min/max later
#undef min
#undef max
//	use reference as argument since this function will be inlined anyway
template <class T>
constexpr T min(const T &x, const T &y) { return (x < y) ? x : y; }
template <class T>
constexpr T max(const T &x, const T &y) { return (x > y) ? x : y; }


inline float lerp(const float u, const float v, const float x) { return u + x * (v - u); }
inline float cerp(const float u0, const float u1, const float u2, const float u3, float x)
{
	float p = (u3 - u2) - (u0 - u1);
	float q = (u0 - u1) - p;
	float r = u2 - u0;
	return x * (x * (x * p + q) + r) + u1;
}
inline float sign(const float v) { return (v > 0) ? 1.0f : (v < 0) ? -1.0f : 0.0f; }
inline float clamp(const float v, const float c0, const float c1) { return min(max(v, c0), c1); }
inline float saturate(const float x) { return clamp(x, 0, 1); }
inline float sCurve(const float t) { return t * t * (3 - 2 * t); }

inline float smoothstep(float edge0, float edge1, float x)
{
	if (x < edge0)
		return 0.0f;
	if (x >= edge1)
		return 1.0f;
	// Scale/bias into [0..1] range
	x = (x - edge0) / (edge1 - edge0);
	return x * x * (3 - 2 * x);
}

inline float rsqrtf(const float v) {
#if VECTORMATH_MODE_SCALAR
	union {
		float vh;
		int i0;
	};

	union {
		float vr;
		int i1;
	};

	vh = v * 0.5f;
	i1 = 0x5f3759df - (i0 >> 1);
	return vr * (1.5f - vh * vr * vr);

	//Let's write code to run on modern hardware :)
#else
	//TODO: Add a NEON implementation for ARM
	return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set1_ps(v)));
#endif
}

inline float sqrf(const float x) {
	return x * x;
}

inline float sincf(const float x) {
	return (x == 0) ? 1 : sinf(x) / x;
}

//inline float roundf(float x) { return floorf((x)+0.5f); }

template <class T> 
inline T clamp(const T& value, const T& minV, const T& maxV){ return min(max(value, minV), maxV); }

inline float intAdjustf(const float x, const float diff = 0.01f) 
{
	float f = roundf(x);
	return (fabsf(f - x) < diff) ? f : x;
}

inline float degToRad(float degrees) { 	return (degrees * PI / 180.0f); }
inline float radToDeg(float radians) { 	return (radians * 180.0f / PI); }
inline bool isPowerOf2(const int x) { return (x & (x - 1)) == 0; } // Note: returns true for 0

inline unsigned int getClosestPowerOfTwo(const unsigned int x) 
{
	unsigned int i = 1;
	while (i < x) i += i;

	if (4 * x < 3 * i) i >>= 1;
	return i;
}

inline unsigned int getUpperPowerOfTwo(const unsigned int x) 
{
	unsigned int i = 1;
	while (i < x) i += i;

	return i;
}

inline unsigned int getLowerPowerOfTwo(const unsigned int x) 
{
	unsigned int i = 1;
	while (i <= x) i += i;

	return i >> 1;
}

static inline unsigned int round_up(unsigned int value, unsigned int multiple) { return ((value + multiple - 1) / multiple) * multiple; }
static inline uint64_t round_up_64(uint64_t value, uint64_t multiple) { return ((value + multiple - 1) / multiple) * multiple; }

//Output format is B8G8R8A8
static inline uint32_t packColorU32(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	return
		((r & 0xff) << 24) |
		((g & 0xff) << 16) |
		((b & 0xff) << 8) |
		((a & 0xff) << 0);
}

//Output format is R8G8B8A8
static inline uint32_t packColorF32(float r, float g, float b, float a)
{
	return packColorU32(
		(uint32_t)(clamp(r, 0.0f, 1.0f) * 255),
		(uint32_t)(clamp(g, 0.0f, 1.0f) * 255),
		(uint32_t)(clamp(b, 0.0f, 1.0f) * 255),
		(uint32_t)(clamp(a, 0.0f, 1.0f) * 255));
}

static inline uint32_t packColorF32_4(float4 rgba) { return packColorF32(rgba.x, rgba.y, rgba.z, rgba.w); }
inline float lineProjection(const Vector3 &line0, const Vector3 &line1, const Vector3 &point)
{
	Vector3 v = line1 - line0;
	return dot(v, point - line0) / dot(v, v);
}

inline Vector3 rgbeToRGB(unsigned char *rgbe)
{
	if (rgbe[3]) 
	{
		return Vector3(rgbe[0], rgbe[1], rgbe[2]) * ldexpf(1.0f, rgbe[3] - (int)(128 + 8));
	}
	return Vector3(0, 0, 0);
}
inline unsigned int rgbToRGBE8(const Vector3 &rgb)
{
	//This is bad usage of Vector3, causing movement of data between registers
	float v = max(rgb.getX(), rgb.getY());
	v = max(v, (float)rgb.getZ());

	if (v < 1e-32f) {
		return 0;
	}
	else {
		int ex;
		float m = frexpf(v, &ex) * 256.0f / v;

		unsigned int r = (unsigned int)(m * rgb.getX());
		unsigned int g = (unsigned int)(m * rgb.getY());
		unsigned int b = (unsigned int)(m * rgb.getZ());
		unsigned int e = (unsigned int)(ex + 128);

		return r | (g << 8) | (b << 16) | (e << 24);
	}
}
inline unsigned int rgbToRGB9E5(const Vector3 &rgb)
{
	//This is bad usage of Vector3, causing movement of data between registers
	float v = max(rgb.getX(), rgb.getY());
	v = max(v, (float)rgb.getZ());

	if (v < 1.52587890625e-5f) {
		return 0;
	}
	else if (v < 65536) {
		int ex;
		float m = frexpf(v, &ex) * 512.0f / v;

		unsigned int r = (unsigned int)(m * rgb.getX());
		unsigned int g = (unsigned int)(m * rgb.getY());
		unsigned int b = (unsigned int)(m * rgb.getZ());
		unsigned int e = (unsigned int)(ex + 15);

		return r | (g << 9) | (b << 18) | (e << 27);
	}
	else {
		unsigned int r = (rgb.getX() < 65536) ? (unsigned int)(rgb.getX() * (1.0f / 128.0f)) : 0x1FF;
		unsigned int g = (rgb.getY() < 65536) ? (unsigned int)(rgb.getY() * (1.0f / 128.0f)) : 0x1FF;
		unsigned int b = (rgb.getZ() < 65536) ? (unsigned int)(rgb.getZ() * (1.0f / 128.0f)) : 0x1FF;
		unsigned int e = 31;

		return r | (g << 9) | (b << 18) | (e << 27);
	}
}


inline Vector3 min(const Vector3 &a, const Vector3 &b)
{
#if VECTORMATH_MODE_SCALAR
	return Vector3(
		min(a.getX(), b.getX()),
		min(a.getY(), b.getY()),
		min(a.getZ(), b.getZ()));
#else
	return Vector3(_mm_min_ps(a.get128(), b.get128()));
#endif
}
inline Vector3 max(const Vector3 &a, const Vector3 &b)
{
#if VECTORMATH_MODE_SCALAR
	return Vector3(
		max(a.getX(), b.getX()),
		max(a.getY(), b.getY()),
		max(a.getZ(), b.getZ()));
#else
	return Vector3(_mm_max_ps(a.get128(), b.get128()));
#endif
}

inline Vector3 lerp(const Vector3 &u, const Vector3 &v, const float x) { return u + x * (v - u); }
inline Vector3 clamp(const Vector3 &v, const Vector3 &c0, const Vector3 &c1) { 	return min(max(v, c0), c1); }

inline float planeDistance(const Vector4 &plane, const Vector3 &point)
{
#if VECTORMATH_MODE_SCALAR
	return
		point.getX() * plane.getX() +
		point.getY() * plane.getY() +
		point.getZ() * plane.getZ() +
		plane.getW();
#else
	static const __m128 maskxyz = _mm_castsi128_ps(_mm_set_epi32(0, ~0u, ~0u, ~0u));
	//static const __m128 maskw = _mm_castsi128_ps(_mm_set_epi32(~0u, 0, 0, 0));

	//a = Vector4(point.xyz * plane.xyz, 0);
	const __m128 a = _mm_and_ps(
		_mm_mul_ps(point.get128(), plane.get128()),
		maskxyz);
	//b = Vector4(0,0,0,plane.w)
	const __m128 b = _mm_and_ps(plane.get128(), b);

	//c = Vector4(plane.xyz, plane.w);
	__m128 c = _mm_or_ps(a, b);


	//result = c.x + c.y + c.z + d.w
	c = _mm_hadd_ps(c, c);
	c = _mm_hadd_ps(c, c);
	float result;
	_mm_store1_ps(&result, c);


	return result;
#endif

}
inline unsigned int toRGBA(const Vector4 &u)
{
#if VECTORMATH_MODE_SCALAR
	return
		(int(u.getX() * 255) << 0) |
		(int(u.getY() * 255) << 8) |
		(int(u.getZ() * 255) << 16) |
		(int(u.getW() * 255) << 24);
#else
	__m128 scaled = _mm_mul_ps(u.get128(), _mm_set1_ps(255));
	__m128i scaledi = _mm_castps_si128(scaled);

	DEFINE_ALIGNED(uint32_t values[4], 16);
	_mm_store_si128((__m128i*)values, scaledi);

	return
		(values[0] << 0) |
		(values[1] << 8) |
		(values[2] << 16) |
		(values[3] << 24);
#endif
}
inline unsigned int toBGRA(const Vector4 &u)
{
#if VECTORMATH_MODE_SCALAR
	return
		(int(u.getZ() * 255) << 0) |
		(int(u.getY() * 255) << 8) |
		(int(u.getX() * 255) << 16) |
		(int(u.getW() * 255) << 24);
#else
	__m128 scaled = _mm_mul_ps(u.get128(), _mm_set1_ps(255));
	__m128i scaledi = _mm_castps_si128(scaled);


	DEFINE_ALIGNED(uint32_t values[4], 16);
	_mm_store_si128((__m128i*)values, scaledi);

	return
		(values[0] << 16) |
		(values[1] << 8) |
		(values[2] << 0) |
		(values[3] << 24);
#endif

}
//----------------------------------------------------------------------------
// Mesh generation helpers
//----------------------------------------------------------------------------
// Generates an array of vertices and normals for a sphere
inline void generateSpherePoints(float **ppPoints, int *pNumberOfPoints, int numberOfDivisions, float radius = 1.0f)
{
	float numStacks = (float)numberOfDivisions;
	float numSlices = (float)numberOfDivisions;

	uint32_t numberOfPoints = numberOfDivisions * numberOfDivisions * 6;
	float3* pPoints = (float3*)conf_malloc(numberOfPoints * sizeof(float3) * 2);
	uint32_t vertexCounter = 0;

	for (int i = 0; i < numberOfDivisions; i++)
	{
		for (int j = 0; j < numberOfDivisions; j++)
		{
			// Sectioned into quads, utilizing two triangles
			Vector3 topLeftPoint = Vector3{ (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)),
				(float)(-cos(PI * (j + 1.0f) / numSlices)),
				(float)(sin(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)) } * radius;
			Vector3 topRightPoint = Vector3{ (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)),
				(float)(-cos(PI * (j + 1.0) / numSlices)),
				(float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)) } * radius;
			Vector3 botLeftPoint = Vector3{ (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)),
				(float)(-cos(PI * j / numSlices)),
				(float)(sin(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)) } * radius;
			Vector3 botRightPoint = Vector3{ (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)),
				(float)(-cos(PI * j / numSlices)),
				(float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)) } * radius;

			// Top right triangle
			pPoints[vertexCounter++] = v3ToF3(topLeftPoint);
			pPoints[vertexCounter++] = v3ToF3(normalize(topLeftPoint));
			pPoints[vertexCounter++] = v3ToF3(botRightPoint);
			pPoints[vertexCounter++] = v3ToF3(normalize(botRightPoint));
			pPoints[vertexCounter++] = v3ToF3(topRightPoint);
			pPoints[vertexCounter++] = v3ToF3(normalize(topRightPoint));

			// Bot left triangle
			pPoints[vertexCounter++] = v3ToF3(topLeftPoint);
			pPoints[vertexCounter++] = v3ToF3(normalize(topLeftPoint));
			pPoints[vertexCounter++] = v3ToF3(botLeftPoint);
			pPoints[vertexCounter++] = v3ToF3(normalize(botLeftPoint));
			pPoints[vertexCounter++] = v3ToF3(botRightPoint);
			pPoints[vertexCounter++] = v3ToF3(normalize(botRightPoint));
		}
	}

	*pNumberOfPoints = numberOfPoints * 3 * 2;
	(*ppPoints) = (float*)pPoints;
}

#define MAKEQUAD(x0, y0, x1, y1, o)\
	float2(x0 + o, y0 + o),\
	float2(x0 + o, y1 - o),\
	float2(x1 - o, y0 + o),\
	float2(x1 - o, y1 - o),

#define MAKETEXQUAD(x0, y0, x1, y1, o)\
	TexVertex(float2(x0 + o, y0 + o), float2(0, 0)),\
	TexVertex(float2(x0 + o, y1 - o), float2(0, 1)),\
	TexVertex(float2(x1 - o, y0 + o), float2(1, 0)),\
	TexVertex(float2(x1 - o, y1 - o), float2(1, 1)),




//----------------------------------------------------------------------------
// Intersection Helpers
//----------------------------------------------------------------------------
struct AABB
{	// Bounding box 
	AABB() 
	{
		minBounds = Vector3(-0.001f, -0.001f, -0.001f);
		maxBounds = Vector3(0.001f, 0.001f, 0.001f);
	}
	inline void Transform(Matrix4 const& mat)
	{
		minBounds = (mat * Vector4(minBounds.getX(), minBounds.getY(), minBounds.getZ(), 1.0f)).getXYZ();
		maxBounds = (mat * Vector4(maxBounds.getX(), maxBounds.getY(), maxBounds.getZ(), 1.0f)).getXYZ();
	}

	Vector3 minBounds, maxBounds;
};

struct Frustum
{
	Vector4 nearPlane, farPlane, topPlane, bottomPlane, leftPlane, rightPlane;
	Vector4 nearTopLeftVert, nearTopRightVert, nearBottomLeftVert, nearBottomRightVert;
	Vector4 farTopLeftVert, farTopRightVert, farBottomLeftVert, farBottomRightVert;

	inline void InitFrustumVerts(Matrix4 const& mvp)
	{
		Matrix4 invMvp = inverse(mvp);

		nearTopLeftVert = invMvp * Vector4(-1, 1, -1, 1);
		nearTopRightVert = invMvp * Vector4(1, 1, -1, 1);
		nearBottomLeftVert = invMvp * Vector4(-1, -1, -1, 1);
		nearBottomRightVert = invMvp * Vector4(1, -1, -1, 1);
		farTopLeftVert = invMvp * Vector4(-1, 1, 1, 1);
		farTopRightVert = invMvp * Vector4(1, 1, 1, 1);
		farBottomLeftVert = invMvp * Vector4(-1, -1, 1, 1);
		farBottomRightVert = invMvp * Vector4(1, -1, 1, 1);

		nearTopLeftVert /= nearTopLeftVert.getW();
		nearTopRightVert /= nearTopRightVert.getW();
		nearBottomLeftVert /= nearBottomLeftVert.getW();
		nearBottomRightVert /= nearBottomRightVert.getW();
		farTopLeftVert /= farTopLeftVert.getW();
		farTopRightVert /= farTopRightVert.getW();
		farBottomLeftVert /= farBottomLeftVert.getW();
		farBottomRightVert /= farBottomRightVert.getW();

		nearTopLeftVert.setW(1.f);
		nearTopRightVert.setW(1.f);
		nearBottomLeftVert.setW(1.f);
		nearBottomRightVert.setW(1.f);
		farTopLeftVert.setW(1.f);
		farTopRightVert.setW(1.f);
		farBottomLeftVert.setW(1.f);
		farBottomRightVert.setW(1.f);
	}
};

// Frustum to AABB intersection
// false if aabb is completely outside frustum, true otherwise
// Based on Íñigo Quílez' "Correct Frustum Culling" article
// http://www.iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
// If fast is true, function will do extra frustum-in-box checks using the frustum's corner vertices.
inline bool aabbInsideOrIntersectsFrustum(AABB const& aabb, const Frustum& frustum, bool const& fast = false)
{
	Vector4 frus_planes[6] = {
		frustum.bottomPlane,
		frustum.topPlane,
		frustum.leftPlane,
		frustum.rightPlane,
		frustum.nearPlane,
		frustum.farPlane
	};
	Vector4 frus_pnts[8] = {
		frustum.nearBottomLeftVert,
		frustum.nearBottomRightVert,
		frustum.nearTopLeftVert,
		frustum.nearTopRightVert,
		frustum.farBottomLeftVert,
		frustum.farBottomRightVert,
		frustum.farTopLeftVert,
		frustum.farTopRightVert,
	};


	// Fast check (aabb vs frustum)
	for (int i = 0; i < 6; i++)
	{
		int out = 0;
		out += ((dot(frus_planes[i], Vector4(aabb.minBounds.getX(), aabb.minBounds.getY(), aabb.minBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.maxBounds.getX(), aabb.minBounds.getY(), aabb.minBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.minBounds.getX(), aabb.maxBounds.getY(), aabb.minBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.maxBounds.getX(), aabb.maxBounds.getY(), aabb.minBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.minBounds.getX(), aabb.minBounds.getY(), aabb.maxBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.maxBounds.getX(), aabb.minBounds.getY(), aabb.maxBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.minBounds.getX(), aabb.maxBounds.getY(), aabb.maxBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);
		out += ((dot(frus_planes[i], Vector4(aabb.maxBounds.getX(), aabb.maxBounds.getY(), aabb.maxBounds.getZ(), 1.0f)) < 0.0) ? 1 : 0);

		if (out == 8)
			return false;
	}

	// Slow check (frustum vs aabb)
	if (!fast)
	{
		int out;

		out = 0;
		for (int i = 0; i < 8; i++)
			out += ((frus_pnts[i].getX() > aabb.maxBounds.getX()) ? 1 : 0);
		if (out == 8)
			return false;

		out = 0;
		for (int i = 0; i < 8; i++)
			out += ((frus_pnts[i].getX() < aabb.minBounds.getX()) ? 1 : 0);
		if (out == 8)
			return false;

		out = 0;
		for (int i = 0; i < 8; i++)
			out += ((frus_pnts[i].getY() > aabb.maxBounds.getY()) ? 1 : 0);
		if (out == 8)
			return false;

		out = 0;
		for (int i = 0; i < 8; i++)
			out += ((frus_pnts[i].getY() < aabb.minBounds.getY()) ? 1 : 0);
		if (out == 8)
			return false;

		out = 0;
		for (int i = 0; i < 8; i++)
			out += ((frus_pnts[i].getZ() > aabb.maxBounds.getZ()) ? 1 : 0);
		if (out == 8)
			return false;

		out = 0;
		for (int i = 0; i < 8; i++)
			out += ((frus_pnts[i].getZ() < aabb.minBounds.getZ()) ? 1 : 0);
		if (out == 8)
			return false;

	}

	return true;
}


//----------------------------------------------------------------------------
// Noise
//----------------------------------------------------------------------------
#define Noise_B 0x1000
#define Noise_BM 0xff

#define Noise_N 0x1000
#define Noise_NP 12
#define Noise_NM 0xfff

#define setup(i,b0,b1,r0,r1)\
	t = i + Noise_N;\
	b0 = ((int) t) & Noise_BM;\
	b1 = (b0 + 1) & Noise_BM;\
	r0 = t - (int) t;\
	r1 = r0 - 1;

static int Noise_p   [Noise_B + Noise_B + 2];
static float Noise_g3[Noise_B + Noise_B + 2][3];
static float Noise_g2[Noise_B + Noise_B + 2][2];
static float Noise_g1[Noise_B + Noise_B + 2];

static void normalize2(float v[2])
{
	float s = 1.0f / sqrtf(v[0] * v[0] + v[1] * v[1]);
	v[0] *= s;
	v[1] *= s;
}

static void normalize3(float v[3])
{
	float s = 1.0f / sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] *= s;
	v[1] *= s;
	v[2] *= s;
}

inline float noise1(const float x)
{
	int bx0, bx1;
	float rx0, rx1, sx, t, u, v;

	setup(x, bx0, bx1, rx0, rx1);

	sx = sCurve(rx0);

	u = rx0 * Noise_g1[Noise_p[bx0]];
	v = rx1 * Noise_g1[Noise_p[bx1]];

	return lerp(sx, u, v);
}

#define at2(rx,ry) (rx * q[0] + ry * q[1])
inline float noise2(const float x, const float y)
{
	int bx0, bx1, by0, by1, b00, b10, b01, b11;
	float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
	int i, j;

	setup(x, bx0, bx1, rx0, rx1);
	setup(y, by0, by1, ry0, ry1);

	i = Noise_p[bx0];
	j = Noise_p[bx1];

	b00 = Noise_p[i + by0];
	b10 = Noise_p[j + by0];
	b01 = Noise_p[i + by1];
	b11 = Noise_p[j + by1];

	sx = sCurve(rx0);
	sy = sCurve(ry0);

	q = Noise_g2[b00]; u = at2(rx0, ry0);
	q = Noise_g2[b10]; v = at2(rx1, ry0);
	a = lerp(sx, u, v);

	q = Noise_g2[b01]; u = at2(rx0, ry1);
	q = Noise_g2[b11]; v = at2(rx1, ry1);
	b = lerp(sx, u, v);

	return lerp(sy, a, b);
}
#undef at2

#define at3(rx,ry,rz) (rx * q[0] + ry * q[1] + rz * q[2])
inline float noise3(const float x, const float y, const float z)
{
	int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
	float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
	int i, j;

	setup(x, bx0, bx1, rx0, rx1);
	setup(y, by0, by1, ry0, ry1);
	setup(z, bz0, bz1, rz0, rz1);

	i = Noise_p[bx0];
	j = Noise_p[bx1];

	b00 = Noise_p[i + by0];
	b10 = Noise_p[j + by0];
	b01 = Noise_p[i + by1];
	b11 = Noise_p[j + by1];

	t = sCurve(rx0);
	sy = sCurve(ry0);
	sz = sCurve(rz0);

	q = Noise_g3[b00 + bz0]; u = at3(rx0, ry0, rz0);
	q = Noise_g3[b10 + bz0]; v = at3(rx1, ry0, rz0);
	a = lerp(t, u, v);

	q = Noise_g3[b01 + bz0]; u = at3(rx0, ry1, rz0);
	q = Noise_g3[b11 + bz0]; v = at3(rx1, ry1, rz0);
	b = lerp(t, u, v);

	c = lerp(sy, a, b);

	q = Noise_g3[b00 + bz1]; u = at3(rx0, ry0, rz1);
	q = Noise_g3[b10 + bz1]; v = at3(rx1, ry0, rz1);
	a = lerp(t, u, v);

	q = Noise_g3[b01 + bz1]; u = at3(rx0, ry1, rz1);
	q = Noise_g3[b11 + bz1]; v = at3(rx1, ry1, rz1);
	b = lerp(t, u, v);

	d = lerp(sy, a, b);

	return lerp(sz, c, d);
}
#undef at3

inline float turbulence2(const float x, const float y, float freq)
{
	float t = 0.0f;

	do {
		t += noise2(freq * x, freq * y) / freq;
		freq *= 0.5f;
	} while (freq >= 1.0f);

	return t;
}

inline float turbulence3(const float x, const float y, const float z, float freq)
{
	float t = 0.0f;

	do {
		t += noise3(freq * x, freq * y, freq * z) / freq;
		freq *= 0.5f;
	} while (freq >= 1.0f);

	return t;
}

inline float tileableNoise1(const float x, const float w)
{
	return (noise1(x)     * (w - x) +
		noise1(x - w) *      x) / w;
}
inline float tileableNoise2(const float x, const float y, const float w, const float h)
{
	return (noise2(x, y)     * (w - x) * (h - y) +
		noise2(x - w, y)     *      x  * (h - y) +
		noise2(x, y - h) * (w - x) *      y +
		noise2(x - w, y - h) *      x  *      y) / (w * h);
}
inline float tileableNoise3(const float x, const float y, const float z, const float w, const float h, const float d)
{
	return (noise3(x, y, z)     * (w - x) * (h - y) * (d - z) +
		noise3(x - w, y, z)     *      x  * (h - y) * (d - z) +
		noise3(x, y - h, z)     * (w - x) *      y  * (d - z) +
		noise3(x - w, y - h, z)     *      x  *      y  * (d - z) +
		noise3(x, y, z - d) * (w - x) * (h - y) *      z +
		noise3(x - w, y, z - d) *      x  * (h - y) *      z +
		noise3(x, y - h, z - d) * (w - x) *      y  *      z +
		noise3(x - w, y - h, z - d) *      x  *      y  *      z) / (w * h * d);
}

inline float tileableTurbulence2(const float x, const float y, const float w, const float h, float freq)
{
	float t = 0.0f;

	do {
		t += tileableNoise2(freq * x, freq * y, w * freq, h * freq) / freq;
		freq *= 0.5f;
	} while (freq >= 1.0f);

	return t;
}
inline float tileableTurbulence3(const float x, const float y, const float z, const float w, const float h, const float d, float freq)
{
	float t = 0.0f;

	do {
		t += tileableNoise3(freq * x, freq * y, freq * z, w * freq, h * freq, d * freq) / freq;
		freq *= 0.5f;
	} while (freq >= 1.0f);

	return t;
}

inline void initNoise()
{
	int i, j, k;

	for (i = 0; i < Noise_B; i++) {
		Noise_p[i] = i;

		Noise_g1[i] = (float)((rand() % (Noise_B + Noise_B)) - Noise_B) / Noise_B;

		for (j = 0; j < 2; j++)
			Noise_g2[i][j] = (float)((rand() % (Noise_B + Noise_B)) - Noise_B) / Noise_B;
		normalize2(Noise_g2[i]);

		for (j = 0; j < 3; j++)
			Noise_g3[i][j] = (float)((rand() % (Noise_B + Noise_B)) - Noise_B) / Noise_B;
		normalize3(Noise_g3[i]);
	}

	while (--i) {
		k = Noise_p[i];
		Noise_p[i] = Noise_p[j = rand() % Noise_B];
		Noise_p[j] = k;
	}

	for (i = 0; i < Noise_B + 2; i++) {
		Noise_p[Noise_B + i] = Noise_p[i];
		Noise_g1[Noise_B + i] = Noise_g1[i];
		for (j = 0; j < 2; j++)
			Noise_g2[Noise_B + i][j] = Noise_g2[i][j];
		for (j = 0; j < 3; j++)
			Noise_g3[Noise_B + i][j] = Noise_g3[i][j];
	}
}

#undef Noise_B
#undef Noise_BM
#undef Noise_NP
#undef Noise_NM
#undef setup

} // namespace Vectormath
//========================================= #ConfettiMathExtensionsEnd ================================================

#endif // VECTORMATH_COMMON_HPP
