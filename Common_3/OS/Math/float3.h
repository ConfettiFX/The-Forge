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

#pragma once

#include "float2.h"

//----------------------------------------------------------------------------
// float3
// A simple structure containing 3 floating point values.
// float3 is always guaranteed to be 3 floats in size. Only use when a very
// specific size is required (like defining structures that need to be the
// same accross platforms, or the same on CPU and GPU (like constant and
// structured buffers) In all other cases you should opt to use vec3, since
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
  
	//vec3 toVec3() const { return vec3(x, y, z); }
	
	void setX(float x_) { x = x_; }
	void setY(float y_) { y = y_; }
	void setZ(float z_) { z = z_; }

	float x, y, z;
};
inline float3 operator+(const float3& a, const float3& b)
{ return float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline const float3& operator+=(float3& a, const float3& b)
{ return a = a + b; }
inline float3 operator-(const float3& a, const float3& b)
{ return float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline const float3& operator-=(float3& a, const float3& b)
{ return a = a - b; }
inline float3 operator-(const float3& a)
{ return float3(-a.x, -a.y, -a.z); }

inline float3 operator*(const float3& a, float b)
{ return float3(a.x * b, a.y * b, a.z * b); }
inline const float3& operator*=(float3&a, float b)
{ return a = a * b; }
inline float3 operator*(float a, const float3& b)
{ return b * a; }
inline float3 operator*(const float3& a, const float3& b)
{ return float3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline const float3& operator*=(float3&a, float3& b)
{ return a = a * b; }

inline float3 operator/(const float3& a, float b)
{ return float3(a.x / b, a.y / b, a.z / b); }
inline const float3& operator/=(float3& a, float b)
{ return a = a / b; }
inline float3 operator/(float a, const float3& b)
{ return b / a; }
inline float3 operator/(const float3& a, const float3& b)
{ return float3(a.x / b.x, a.y / b.y, a.z / b.z); }
inline const float3& operator/=(float3&a, const float3& b)
{ return a = a / b; }
