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

#ifndef __VM_INCLUDE_H
#define __VM_INCLUDE_H

//#define __SSE__
#if defined(ORBIS)
#include "../../../../CommonPS4_2/Modified_Sony/vectormath/cpp/vectormath_aos.h"

typedef sce::Vectormath::Simd::Aos::Vector2 vec2;
typedef sce::Vectormath::Simd::Aos::Vector3 vec3;
typedef sce::Vectormath::Simd::Aos::Vector4 vec4;

typedef sce::VectorMath::Simd::Aos::Matrix3 mat3;
typedef sce::VectorMath::Simd::Aos::Matrix4 mat4;

#else
// ModifiedSonyMath Readme:
// - Removed the Aos/Soa sub-namespaces, since the Soa implementations were only available for SPU.
// - All you need to do is include the public header file vectormath.hpp. It will expose the relevant parts of the library for you and try to select the SSE implementation if supported.
#include "../../ThirdParty/OpenSource/ModifiedSonyMath/vectormath.hpp"

typedef Vector2 vec2;
typedef Vector3 vec3;
typedef Vector4 vec4;


typedef Matrix3 mat3;
typedef Matrix4 mat4;


#endif



#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif


//Comparison operators for vec2
inline bool operator == (const vec2 &u, const vec2 &v)
{ return (u.getX() == v.getX() && u.getY() == v.getY()); }
inline bool operator != (const vec2 &u, const vec2 &v) { return !(u == v); }

//Comparison operators for vec3
inline bool operator == (const vec3 &u, const vec3 &v)
{
	//Compare the X, Y, and Z values
	//(We actually compare X, Y, Z, and W, but mask away the result of W
#if VECTORMATH_MODE_SCALAR
  return
    u.getX() == v.getX() &&
    u.getY() == v.getY() &&
    u.getZ() == v.getZ();
#else
	return (_mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) & 0x7) == 0x7;
#endif
}
inline bool operator != (const vec3 &u, const vec3 &v)
{
#if VECTORMATH_MODE_SCALAR
  return !operator==(u, v);
#else
	//Compare the X, Y, and Z values
	//(We actually compare X, Y, Z, and W, but mask away the result of W
	return (_mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) & 0x7) != 0x7;
#endif
}

//Comparison operators for vec4
inline bool operator == (const vec4 &u, const vec4 &v)
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
inline bool operator != (const vec4 &u, const vec4 &v)
{
#if VECTORMATH_MODE_SCALAR
  return !operator==(u, v);
#else
	//Compare the X, Y, Z, and W values
	return _mm_movemask_ps(_mm_cmpeq_ps(u.get128(), v.get128())) != 0xf;
#endif
}




#endif //__VM_INCLUDE_H
