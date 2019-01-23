/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

// Definitions that are meant to be shared between GLSL/Host

// Below are the GLSL buffer definitions
#ifndef NO_GLSL_DEFINITIONS

float F16toF32(uint val)
{
	return ((val & 0x8000) << 16)
	 | (((val & 0x7c00) + 0x1C000) << 13)
	 | ((val & 0x03FF) << 13);
}

uint F32toF16(float val)
{
	uint f32 = floatBitsToUint(val);
	return ((f32 >> 16) & 0x8000) |
	((((f32 & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) |
	((f32 >> 13) & 0x03ff);
}

vec2 OctWrap( vec2 v )
{
	return ( 1.0 - abs( v.yx ) ) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0 );
}

vec3 decodeDir(vec2 encN)
{
	encN = encN * 2.0 - 1.0;

	vec3 n;
	n.z = 1.0 - abs( encN.x ) - abs( encN.y );
	n.xy = n.z >= 0.0 ? encN.xy : OctWrap( encN.xy );
	n = normalize( n );
	return n;
}

vec2 encodeDir(vec3 n)
{
	n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
	n.xy = n.z >= 0.0 ? n.xy : OctWrap( n.xy );
	n.xy = n.xy * 0.5 + 0.5;
	return n.xy;
}

vec2 sign_not_zero(vec2 v)
{
	return step(0.0, v) * 2.0 - vec2(1, 1);
}


uint pack2Floats(float low, float high)
{
	return packHalf2x16(vec2(low, high));
}

vec2 unpack2Floats(uint p)
{
	return unpackHalf2x16(p);
}

uint pack2Snorms(float low, float high)
{
	return packSnorm2x16(vec2(low, high));
}

vec2 unpack2Snorms(uint p)
{
	return vec2(unpackSnorm2x16(p));
}

uint pack2Unorms(float low, float high)
{
	return packUnorm2x16(vec2(low, high));
}

vec2 unpack2Unorms(uint p)
{
	return vec2(unpackUnorm2x16(p));
}

#endif
