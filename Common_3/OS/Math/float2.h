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
//----------------------------------------------------------------------------
// float2
// A simple structure containing 2 floating point values.
// float2 is always guaranteed to be 2 floats in size. Only use when a very
// specific size is required (like defining structures that need to be the
// same accross platforms, or the same on CPU and GPU (like constant and
// structured buffers) In all other cases you should opt to use vec2, since
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

	//vec2 toVec2() const { return vec2(x, y); }
	
	float x, y;
};
inline float2 operator+(const float2& a, const float2& b)
{ return float2(a.x + b.x, a.y + b.y); }
inline const float2& operator+=(float2& a, const float2& b)
{ return a = a + b; }
inline float2 operator-(const float2& a, const float2& b)
{ return float2(a.x - b.x, a.y - b.y); }
inline const float2& operator-=(float2& a, const float2& b)
{ return a = a - b; }
inline float2 operator-(const float2& a)
{ return float2(-a.x, -a.y); }

inline float2 operator*(const float2& a, float b)
{ return float2(a.x * b, a.y * b); }
inline const float2& operator*=(float2&a, float b)
{ return a = a * b; }
inline float2 operator*(float a, const float2& b)
{ return b * a; }
inline float2 operator*(const float2& a, const float2& b)
{ return float2(a.x * b.x, a.y * b.y); }
inline const float2& operator*=(float2&a, float2& b)
{ return a = a * b; }

inline float2 operator/(const float2& a, float b)
{ return float2(a.x / b, a.y / b); }
inline const float2& operator/=(float2& a, float b)
{ return a = a / b; }
inline float2 operator/(float a, const float2& b)
{ return b / a; }
inline float2 operator/(const float2& a, const float2& b)
{ return float2(a.x / b.x, a.y / b.y); }
inline const float2& operator/=(float2&a, const float2& b)
{ return a = a / b; }
