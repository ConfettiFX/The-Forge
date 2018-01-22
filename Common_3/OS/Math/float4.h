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

	vec4 toVec4() const { return vec4(x, y, z, w); }

	void setX(float x_) { x = x_; }
	void setY(float y_) { y = y_; }
	void setZ(float z_) { z = z_; }
	void setW(float w_) { w = w_; }

	float x, y, z, w;
};
inline float4 operator+(const float4& a, const float4& b)
{
	return float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}
inline const float4& operator+=(float4& a, const float4& b)
{
	return a = a + b;
}
inline float4 operator-(const float4& a, const float4& b)
{
	return float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}
inline const float4& operator-=(float4& a, const float4& b)
{
	return a = a - b;
}
inline float4 operator-(const float4& a)
{
	return float4(-a.x, -a.y, -a.z, -a.w);
}

inline float4 operator*(const float4& a, float b)
{
	return float4(a.x * b, a.y * b, a.z * b, a.w * b);
}
inline const float4& operator*=(float4&a, float b)
{
	return a = a * b;
}
inline float4 operator*(float a, const float4& b)
{
	return b * a;
}
inline float4 operator*(const float4& a, const float4& b)
{
	return float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}
inline const float4& operator*=(float4&a, float4& b)
{
	return a = a * b;
}

inline float4 operator/(const float4& a, float b)
{
	return float4(a.x / b, a.y / b, a.z / b, a.w / b);
}
inline const float4& operator/=(float4& a, float b)
{
	return a = a / b;
}
inline float4 operator/(float a, const float4& b)
{
	return b / a;
}
inline float4 operator/(const float4& a, const float4& b)
{
	return float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
}
inline const float4& operator/=(float4&a, const float4& b)
{
	return a = a / b;
}
