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

#ifndef _CFX_FLOAT_UTIL_
#define _CFX_FLOAT_UTIL_

#include <math.h>
#include <stdint.h>
#ifdef _ANDROID
#include "../../Common_2/Code/Renderer/Android/AndroidDefines.h"
#endif
#include "vmInclude.h"

#include "float2.h"
#include "float3.h"
#include "float4.h"

#ifndef TARGET_IOS
#include <immintrin.h>
#endif

#include "../Core/Compiler.h"


//floatx to vecx conversion functions
inline float2 v2ToF2(const vec2& v2) { return float2(v2.getX(), v2.getY()); }
inline float3 v3ToF3(const vec3& v3)
{
#if VECTORMATH_MODE_SCALAR
  return float3(v3.getX(), v3.getY(), v3.getZ());
#else
    DEFINE_ALIGNED(float array[4], 16);
	_mm_store_ps(array, v3.get128());
	return float3(array[0], array[1], array[2]);
#endif
}
inline float4 v4ToF4(const vec4& v4)
{
#if VECTORMATH_MODE_SCALAR
  return float4(v4.getX(), v4.getY(), v4.getZ(), v4.getW());
#else
    DEFINE_ALIGNED(float4 result, 16);
	_mm_store_ps(&result.x, v4.get128());
	return result;
#endif
}

//vecx to floatx conversion functions
inline vec2 f2Tov2(const float2& f2) { return vec2(f2.x, f2.y); }
inline vec3 f3Tov3(const float3& f3) { return vec3(f3.x, f3.y, f3.z); }
inline vec4 f4Tov4(const float4& f4) { return vec4(f4.x, f4.y, f4.z, f4.w); }


//----------------------------------------------------------------------------
// Float operations.
//----------------------------------------------------------------------------
float lerp(const float u, const float v, const float x);
float cerp(const float u0, const float u1, const float u2, const float u3, float x);
float sign(const float v);
float clamp(const float v, const float c0, const float c1);
float saturate(const float x);
float sCurve(const float t);

#define PI 3.14159265358979323846f
static const float piDivTwo = 1.570796326794896619231f;        //!< pi/2 constant

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

inline float roundf(float x) { return floorf((x)+0.5f); }

//	doing this to be able to instantiate vector min/max later
#undef min
#undef max

//	use reference as argument since this function will be inlined anyway
template <class T>
constexpr T min(const T &x, const T &y) { return (x < y) ? x : y; }
template <class T>
constexpr T max(const T &x, const T &y) { return (x > y) ? x : y; }
template <class T>
T clamp(const T& value, const T& minV, const T& maxV)
{ return min(max(value, minV), maxV); }

inline float intAdjustf(const float x, const float diff = 0.01f) {
	float f = roundf(x);

	return (fabsf(f - x) < diff) ? f : x;
}

inline float degToRad(float degrees)
{
	return (degrees * PI / 180.0f);
}

inline float radToDeg(float radians)
{
	return (radians * 180.0f / PI);
}

// Note: returns true for 0
inline bool isPowerOf2(const int x) {
	return (x & (x - 1)) == 0;
}

inline unsigned int getClosestPowerOfTwo(const unsigned int x) {
	unsigned int i = 1;
	while (i < x) i += i;

	if (4 * x < 3 * i) i >>= 1;
	return i;
}

inline unsigned int getUpperPowerOfTwo(const unsigned int x) {
	unsigned int i = 1;
	while (i < x) i += i;

	return i;
}

inline unsigned int getLowerPowerOfTwo(const unsigned int x) {
	unsigned int i = 1;
	while (i <= x) i += i;

	return i >> 1;
}

static inline unsigned int round_up(unsigned int value, unsigned int multiple)
{
	return ((value + multiple - 1) / multiple) * multiple;
}

static inline uint64_t round_up_64(uint64_t value, uint64_t multiple)
{
	return ((value + multiple - 1) / multiple) * multiple;
}

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

static inline uint32_t packColorF32_4(float4 rgba)
{
	return packColorF32(rgba.x, rgba.y, rgba.z, rgba.w);
}



inline float lineProjection(const vec3 &line0, const vec3 &line1, const vec3 &point)
{
	vec3 v = line1 - line0;
	return dot(v, point - line0) / dot(v, v);
}

inline vec3 rgbeToRGB(unsigned char *rgbe)
{
	if (rgbe[3]) {
		return vec3(rgbe[0], rgbe[1], rgbe[2]) * ldexpf(1.0f, rgbe[3] - (int)(128 + 8));
	}
	else return vec3(0, 0, 0);
}
inline unsigned int rgbToRGBE8(const vec3 &rgb)
{
	//This is bad usage of vec3, causing movement of data between registers
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
inline unsigned int rgbToRGB9E5(const vec3 &rgb)
{
	//This is bad usage of vec3, causing movement of data between registers
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


inline vec3 min(const vec3 &a, const vec3 &b)
{
#if VECTORMATH_MODE_SCALAR
  return vec3(
    min(a.getX(), b.getX()),
    min(a.getY(), b.getY()),
    min(a.getZ(), b.getZ()));
#else
	return vec3(_mm_min_ps(a.get128(), b.get128()));
#endif
}
inline vec3 max(const vec3 &a, const vec3 &b)
{
#if VECTORMATH_MODE_SCALAR
  return vec3(
    max(a.getX(), b.getX()),
    max(a.getY(), b.getY()),
    max(a.getZ(), b.getZ()));
#else
	return vec3(_mm_max_ps(a.get128(), b.get128()));
#endif
}

inline vec3 lerp(const vec3 &u, const vec3 &v, const float x)
{
	return u + x * (v - u);
}
inline vec3 clamp(const vec3 &v, const vec3 &c0, const vec3 &c1)
{
	return min(max(v, c0), c1);
}

inline float planeDistance(const vec4 &plane, const vec3 &point)
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

	//a = vec4(point.xyz * plane.xyz, 0);
	const __m128 a = _mm_and_ps(
		_mm_mul_ps(point.get128(), plane.get128()),
		maskxyz);
	//b = vec4(0,0,0,plane.w)
	const __m128 b = _mm_and_ps(plane.get128(), b);

	//c = vec4(plane.xyz, plane.w);
	__m128 c = _mm_or_ps(a, b);


	//result = c.x + c.y + c.z + d.w
	c = _mm_hadd_ps(c, c);
	c = _mm_hadd_ps(c, c);
	float result;
	_mm_store1_ps(&result, c);
	

	return result;
#endif

}
inline unsigned int toRGBA(const vec4 &u)
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
inline unsigned int toBGRA(const vec4 &u)
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
/************************************************************************/
// Mesh generation helpers
/************************************************************************/
// Generates an array of vertices and normals for a sphere
void generateSpherePoints(float **ppPoints, int *pNumberOfPoints, int numberOfDivisions);

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
/************************************************************************/
/************************************************************************/
#endif
