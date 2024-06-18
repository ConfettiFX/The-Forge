/*
* Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#ifndef _VULKAN_H
#define _VULKAN_H

#define UINT_MAX 4294967295
#define FLT_MAX  3.402823466e+38F

#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
	#extension GL_EXT_nonuniform_qualifier : enable
#endif

#define f4(X) vec4(X)
#define f3(X) vec3(X)
#define f2(X) vec2(X)
#define u4(X) uvec4(X)
#define u3(X) uvec3(X)
#define u2(X) uvec2(X)
#define i4(X) ivec4(X)
#define i3(X) ivec3(X)
#define i2(X) ivec2(X)

#define h4 f4
#define half4 float4
#define half3 float3
#define half2 float2
#define half  float

#define short4 int4
#define short3 int3
#define short2 int2
#define short  int

#define ushort4 uint4
#define ushort3 uint3
#define ushort2 uint2
#define ushort  uint

#define SV_VERTEXID         gl_VertexIndex
#define SV_INSTANCEID       gl_InstanceIndex
#define SV_ISFRONTFACE      gl_FrontFacing 
#define SV_GROUPID          gl_WorkGroupID
#define SV_DISPATCHTHREADID gl_GlobalInvocationID
#define SV_GROUPTHREADID    gl_LocalInvocationID
#define SV_GROUPINDEX       gl_LocalInvocationIndex
#define SV_SAMPLEINDEX      gl_SampleID
#define SV_PRIMITIVEID      gl_PrimitiveID
#define SV_SHADINGRATE      0u //ShadingRateKHR
#define SV_COVERAGE         gl_SampleMaskIn[0]

#define SV_OUTPUTCONTROLPOINTID gl_InvocationID
#define SV_DOMAINLOCATION gl_TessCoord

#define out_coverage int

#define inout(T) inout T
#define out(T) out T
#define in(T) in T
#define inout_array(T, X) inout(T) X
#define out_array(T, X)   out(T) X
#define in_array(T, X)    in(T) X
#define groupshared inout

#define Get(X) _Get##X

#define _NONE
#define NUM_THREADS(X, Y, Z) layout (local_size_x = X, local_size_y = Y, local_size_z = Z) in(_NONE);

// for preprocessor to evaluate symbols before concatenation
#define CAT(a, b) a ## b
#define XCAT(a, b) CAT(a, b)

#ifndef UPDATE_FREQ_NONE
    #define UPDATE_FREQ_NONE      set = 0
#endif
#ifndef UPDATE_FREQ_PER_FRAME
    #define UPDATE_FREQ_PER_FRAME set = 1
#endif
#ifndef UPDATE_FREQ_PER_BATCH
    #define UPDATE_FREQ_PER_BATCH set = 2
#endif
#ifndef UPDATE_FREQ_PER_DRAW
    #define UPDATE_FREQ_PER_DRAW  set = 3
#endif
#define UPDATE_FREQ_USER      UPDATE_FREQ_NONE
#define space4                UPDATE_FREQ_NONE
#define space5                UPDATE_FREQ_NONE
#define space6                UPDATE_FREQ_NONE
#define space10                UPDATE_FREQ_NONE


#define STRUCT(NAME) struct NAME
#define DATA(TYPE, NAME, SEM) TYPE NAME

// these are handled by vulkan.py
#define FLAT(TYPE) TYPE
#define CENTROID(TYPE) TYPE

#define AnyLessThan(X, Y)         any( LessThan((X), (Y)) )
#define AnyLessThanEqual(X, Y)    any( LessThanEqual((X), (Y)) )
#define AnyGreaterThan(X, Y)      any( GreaterThan((X), (Y)) )
#define AnyGreaterThanEqual(X, Y) any( GreaterThanEqual((X), (Y)) )

#define AllLessThan(X, Y)         all( LessThan((X), (Y)) )
#define AllLessThanEqual(X, Y)    all( LessThanEqual((X), (Y)) )
#define AllGreaterThan(X, Y)      all( GreaterThan((X), (Y)) )
#define AllGreaterThanEqual(X, Y) all( GreaterThanEqual((X), (Y)) )

#define select lerp

bvec3 Equal(vec3 X, float Y) { return equal(X, vec3(Y));}

bvec2 LessThan(in(vec2) a, in(float) b)      { return lessThan(a, vec2(b)); }
bvec2 LessThan(in(vec2) a, in(vec2) b)       { return lessThan(a, b);}
bvec3 LessThan(in(vec3) X, in(float) Y)      { return lessThan(X, f3(Y)); }
bvec3 LessThanEqual(in(vec3) X, in(float) Y) { return lessThanEqual(X, f3(Y)); }
bvec2 LessThanEqual(in(vec2) a, in(vec2) b)  { return lessThanEqual(a, b);}

bvec2 GreaterThan(in(vec2) a, in(float) b)      { return greaterThan(a, vec2(b)); }
bvec3 GreaterThan(in(vec3) a, in(float) b)      { return greaterThan(a, f3(b)); }
bvec2 GreaterThan(in(uvec2) a, in(uint) b)      { return greaterThan(a, uvec2(b)); }
bvec2 GreaterThan(in(vec2) a, in(vec2) b)       { return greaterThan(a, b);}
bvec2 GreaterThanEqual(in(vec2) a, in(vec2) b)  { return greaterThanEqual(a, b);}
bvec4 GreaterThan(in(vec4) a, in(vec4) b)       { return greaterThan(a, b); }
bvec3 GreaterThanEqual(in(vec3) a, in(float) b) { return greaterThanEqual(a, vec3(b)); }

bvec2 And(in(bvec2) a, in(bvec2) b)
{ return bvec2(a.x && b.x, a.y && b.y); }

bool allGreaterThan(const vec4 a, const vec4 b)
{ return all(greaterThan(a, b)); }

#define GroupMemoryBarrier() { \
    groupMemoryBarrier(); \
    barrier(); \
}

#define MemoryBarrier() { \
	memoryBarrier(); \
}

// #define AllMemoryBarrier AllMemoryBarrierWithGroupSync()
#define AllMemoryBarrier() { \
    groupMemoryBarrier(); \
    memoryBarrier(); \
    barrier(); \
}

#define clip(COND) if( (COND) < 0 ) \
{ \
    discard;\
    return;\
}
#if defined(FT_ATOMICS_64)
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable
#extension GL_EXT_shader_image_int64 : enable
#endif

#extension GL_ARB_shader_ballot : enable

#define AtomicAdd(DEST, VALUE, ORIGINAL_VALUE) \
    {ORIGINAL_VALUE = atomicAdd(DEST, VALUE);}
#define AtomicStore(DEST, VALUE) \
    (DEST) = (VALUE)
#define AtomicLoad(SRC) \
    SRC
#define AtomicExchange(DEST, VALUE, ORIGINAL_VALUE) \
    ORIGINAL_VALUE = atomicExchange((DEST), (VALUE))
#define AtomicCompareExchange(DEST, COMPARE_VALUE, VALUE, ORIGINAL_VALUE) \
    ORIGINAL_VALUE = atomicCompSwap((DEST), (COMPARE_VALUE), (VALUE))

#define CBUFFER(NAME, FREQ, REG, BINDING) layout(std140, FREQ, BINDING) uniform NAME
#define PUSH_CONSTANT(NAME, REG) layout(push_constant) uniform NAME##Block

// #define mul(a, b) (a * b)
mat4 mul(mat4 a, mat4 b)  { return a * b; }
vec4 mul(mat4 a, vec4 b)  { return a * b; }
vec4 mul(vec4 a, mat4 b)  { return a * b; }

mat3 mul(mat3 a, mat3 b)  { return a * b; }
vec3 mul(mat3 a, vec3 b)  { return a * b; }
vec3 mul(vec3 a, mat3 b)  { return a * b; }

vec2 mul(mat2 a, vec2 b)  { return a * b; }
vec3 mul(vec3 a, float b) { return a * b; }

#define row_major(X) layout(row_major) X

#define RES(TYPE, NAME, FREQ, REG, BINDING) layout(FREQ, BINDING) uniform TYPE NAME

#define SampleLvlTex2D(NAME, SAMPLER, COORD, LEVEL) \
textureLod(sampler2D(NAME, SAMPLER), COORD, LEVEL)
#define SampleLvlTex3D(NAME, SAMPLER, COORD, LEVEL) \
textureLod(sampler3D(NAME, SAMPLER), COORD, LEVEL)
#define SampleLvlTex2DArray(NAME, SAMPLER, COORD, LEVEL ) \
textureLod(sampler2DArray(NAME, SAMPLER), COORD, LEVEL)
#define SampleLvlTexCube(NAME, SAMPLER, COORD, LEVEL ) \
textureLod(samplerCube(NAME, SAMPLER), COORD, LEVEL)
#define SampleLvlTexCubeArray(NAME, SAMPLER, COORD, LEVEL ) \
textureLod(samplerCubeArray(NAME, SAMPLER), float4(COORD, LEVEL), 0)

// vec4 SampleLvlOffsetTex2D( texture2D NAME, sampler SAMPLER, vec2 COORD, float LEVEL, const in(ivec2) OFFSET )
// { return textureLodOffset(sampler2D(NAME, SAMPLER), COORD, LEVEL, OFFSET); }
#define SampleLvlOffsetTex2D(NAME, SAMPLER, COORD, LEVEL, OFFSET) \
textureLodOffset(sampler2D(NAME, SAMPLER), COORD, LEVEL, OFFSET)
#define SampleLvlOffsetTex2DArray(NAME, SAMPLER, COORD, LEVEL, OFFSET) \
textureLodOffset(sampler2DArray(NAME, SAMPLER), COORD, LEVEL, OFFSET)
#define SampleLvlOffsetTex3D(NAME, SAMPLER, COORD, LEVEL, OFFSET) \
textureLodOffset(sampler3D(NAME, SAMPLER), COORD, LEVEL, OFFSET)

#define LoadByte(BYTE_BUFFER, ADDRESS) ((BYTE_BUFFER)[(ADDRESS)>>2])
#define LoadByte2(BYTE_BUFFER, ADDRESS) uint2(((BYTE_BUFFER)[((ADDRESS) >> 2) + 0]), ((BYTE_BUFFER)[((ADDRESS) >> 2) + 1]))
#define LoadByte3(BYTE_BUFFER, ADDRESS) uint3(((BYTE_BUFFER)[((ADDRESS) >> 2) + 0]), ((BYTE_BUFFER)[((ADDRESS) >> 2) + 1]), ((BYTE_BUFFER)[((ADDRESS) >> 2) + 2]))
#define LoadByte4(BYTE_BUFFER, ADDRESS) uint4(((BYTE_BUFFER)[((ADDRESS) >> 2) + 0]), ((BYTE_BUFFER)[((ADDRESS) >> 2) + 1]), ((BYTE_BUFFER)[((ADDRESS) >> 2) + 2]), ((BYTE_BUFFER)[((ADDRESS) >> 2) + 3]))

#define StoreByte(BYTE_BUFFER, ADDRESS, VALUE)  (BYTE_BUFFER)[((ADDRESS) >> 2) + 0] = VALUE;
#define StoreByte2(BYTE_BUFFER, ADDRESS, VALUE) (BYTE_BUFFER)[((ADDRESS) >> 2) + 0] = VALUE[0]; (BYTE_BUFFER)[((ADDRESS) >> 2) + 1] = VALUE[1];
#define StoreByte3(BYTE_BUFFER, ADDRESS, VALUE) (BYTE_BUFFER)[((ADDRESS) >> 2) + 0] = VALUE[0]; (BYTE_BUFFER)[((ADDRESS) >> 2) + 1] = VALUE[1]; (BYTE_BUFFER)[((ADDRESS) >> 2) + 2] = VALUE[2];
#define StoreByte4(BYTE_BUFFER, ADDRESS, VALUE) (BYTE_BUFFER)[((ADDRESS) >> 2) + 0] = VALUE[0]; (BYTE_BUFFER)[((ADDRESS) >> 2) + 1] = VALUE[1]; (BYTE_BUFFER)[((ADDRESS) >> 2) + 2] = VALUE[2]; (BYTE_BUFFER)[((ADDRESS) >> 2) + 3] = VALUE[3];

// #define LoadLvlTex2D(TEX, SMP, P, L) _LoadLvlTex2D(TEX, SMP, ivec2((P).xy), L)
// vec4 _LoadLvlTex2D(texture2D TEX, sampler SMP, ivec2 P, int L) { return texelFetch(sampler2D(TEX, SMP), P, L); }
// #ifdef GL_EXT_samplerless_texture_functions
// #extension GL_EXT_samplerless_texture_functions : enable
// vec4 _LoadLvlTex2D(texture2D TEX, uint _NO_SAMPLER, ivec2 P, int L) { return texelFetch(TEX, P, L); }
// #endif

// #define LoadTex3D(TEX, SMP, P, LOD) _LoadTex3D()
//  vec4 _LoadTex2D( texture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch( sampler2DArray(TEX, SMP), P, lod); }
// uvec4 _LoadTex2D(utexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(usampler2DArray(TEX, SMP), P, lod); }
// ivec4 _LoadTex2D(itexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(isampler2DArray(TEX, SMP), P, lod); }

#define LoadTex1D(TEX, SMP, P, LOD) _LoadTex1D((TEX), (SMP), int(P), int(LOD))
 vec4 _LoadTex1D( texture1D TEX, sampler SMP, int P, int lod) { return texelFetch( sampler1D(TEX, SMP), P, lod); }
uvec4 _LoadTex1D(utexture1D TEX, sampler SMP, int P, int lod) { return texelFetch(usampler1D(TEX, SMP), P, lod); }
ivec4 _LoadTex1D(itexture1D TEX, sampler SMP, int P, int lod) { return texelFetch(isampler1D(TEX, SMP), P, lod); }

#define LoadTex2D(TEX, SMP, P, LOD) _LoadTex2D((TEX), (SMP), ivec2((P).xy), int(LOD))
 vec4 _LoadTex2D( texture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch( sampler2D(TEX, SMP), P, lod); }
uvec4 _LoadTex2D(utexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(usampler2D(TEX, SMP), P, lod); }
ivec4 _LoadTex2D(itexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(isampler2D(TEX, SMP), P, lod); }

#define LoadRWTex2D(TEX, P) imageLoad(TEX, ivec2(P))
#define LoadRWTex3D(TEX, P) imageLoad(TEX, ivec3(P))

// vec4 _LoadTex2D(writeonly image2D img, sampler SMP, ivec2 P, int lod) { return imageLoad(img, P); }

// #define Load2D( NAME, COORD ) imageLoad( (NAME), ivec2((COORD).xy) )
// #define Load3D( NAME, COORD ) imageLoad( (NAME), ivec3((COORD).xyz) )


#define LoadTex3D(TEX, SMP, P, LOD) _LoadTex3D((TEX), (SMP), ivec3((P).xyz), int(LOD))
 vec4 _LoadTex3D( texture2DArray TEX, sampler SMP, ivec3 P, int lod) { return texelFetch( sampler2DArray(TEX, SMP), P, lod); }
uvec4 _LoadTex3D(utexture2DArray TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(usampler2DArray(TEX, SMP), P, lod); }
ivec4 _LoadTex3D(itexture2DArray TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(isampler2DArray(TEX, SMP), P, lod); }
vec4  _LoadTex3D(texture3D  TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(sampler3D(TEX, SMP),  P, lod); }
uvec4 _LoadTex3D(utexture3D TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(usampler3D(TEX, SMP), P, lod); }
ivec4 _LoadTex3D(itexture3D TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(isampler3D(TEX, SMP), P, lod); }

#ifdef GL_EXT_samplerless_texture_functions
#extension GL_EXT_samplerless_texture_functions : enable
vec4  _LoadTex1D(texture1D TEX,       uint _NO_SAMPLER, int P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex1D(utexture1D TEX,      uint _NO_SAMPLER, int P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex1D(itexture1D TEX,      uint _NO_SAMPLER, int P, int lod) { return texelFetch(TEX, P, lod); }
vec4  _LoadTex2D(texture2D TEX,       uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex2D(utexture2D TEX,      uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex2D(itexture2D TEX,      uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
vec4  _LoadTex2DMS(texture2DMS TEX,   uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex2DMS(utexture2DMS TEX,  uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex2DMS(itexture2DMS TEX,  uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
vec4  _LoadTex3D(texture2DArray TEX,  uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex3D(utexture2DArray TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex3D(itexture2DArray TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
vec4  _LoadTex3D(texture3D  TEX,      uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex3D(utexture3D TEX,      uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex3D(itexture3D TEX,      uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
#endif

#define LoadLvlOffsetTex2D(TEX, SMP, P, L, O) _LoadLvlOffsetTex2D((TEX), (SMP), (P), (L), (O))
vec4 _LoadLvlOffsetTex2D( texture2D TEX, sampler SMP, ivec2 P, int L, ivec2 O)
{
    return texelFetch(sampler2D(TEX, SMP), P+O, L);
}

#define LoadLvlOffsetTex3D(TEX, SMP, P, L, O) _LoadLvlOffsetTex3D((TEX), (SMP), (P), (L), (O))
vec4 _LoadLvlOffsetTex3D( texture2DArray TEX, sampler SMP, ivec3 P, int L, ivec3 O)
{
    return texelFetch(sampler2DArray(TEX, SMP), P+O, L);
}


#define LoadTex2DMS(NAME, SMP, P, S) _LoadTex2DMS((NAME), (SMP), ivec2(P.xy), int(S))
vec4 _LoadTex2DMS(texture2DMS TEX, sampler SMP, ivec2 P, int S) { return texelFetch(sampler2DMS(TEX, SMP), P, S); }
#define LoadTex2DArrayMS(NAME, SMP, P, S) _LoadTex2DArrayMS((NAME), (SMP), ivec3(P.xyz), int(S))
vec4 _LoadTex2DArrayMS(texture2DMSArray TEX, sampler SMP, ivec3 P, int S) { return texelFetch(sampler2DMSArray(TEX, SMP), P, S); }
#ifdef GL_EXT_samplerless_texture_functions
vec4 _LoadTex2DArrayMS(texture2DMSArray TEX, uint _NO_SAMPLER, ivec3 P, int S) { return texelFetch(TEX, P, S); }
#endif

#define SampleGradTex2D(TEX, SMP, P, DX, DY) \
textureGrad(sampler2D(TEX, SMP), P, DX, DY)

#define GatherRedTex2D(TEX, SMP, P) _GatherRedTex2D((TEX), (SMP), vec2(P.xy))
vec4 _GatherRedTex2D(texture2D TEX, sampler SMP, vec2 P) { return textureGather(sampler2D(TEX, SMP), P, 0 ); }

#define GatherRedTex3D(TEX, SMP, P) _GatherRedTex3D((TEX), (SMP), vec3(P.xyz))
vec4 _GatherRedTex3D(texture2DArray TEX, sampler SMP, vec3 P) { return textureGather(sampler2DArray(TEX, SMP), P, 0 ); }

#define GatherRedOffsetTex2D(TEX, SMP, P, O) _GatherRedOffsetTex2D((TEX), (SMP), vec2(P.xy), ivec2(O))
vec4 _GatherRedOffsetTex2D(texture2D TEX, sampler SMP, vec2 P, ivec2 O) { return textureGatherOffset(sampler2D(TEX, SMP), P, O, 0 ); }

#define SampleTexCube(TEX, SMP, P) \
texture(samplerCube(TEX, SMP), P)
#define SampleUTexCube(TEX, SMP, P) \
texture(usamplerCube(TEX, SMP), P)
#define SampleITexCube(TEX, SMP, P) \
texture(isamplerCube(TEX, SMP), P)

#define SampleTex1D(TEX, SMP, P) \
texture(sampler1D(TEX, SMP), P)

#define SampleTex2D(TEX, SMP, P) \
texture(sampler2D(TEX, SMP), P)

#define SampleTex2DArray(TEX, SMP, P) \
texture(sampler2DArray(TEX, SMP), P)

#define SampleTex2DProj(TEX, SMP, P) \
textureProj(sampler2D(TEX, SMP), P)

// #define SampleTex3D(NAME, SAMPLER, COORD)            texture(_getSampler(NAME, SAMPLER), COORD)
#define SampleTex3D(TEX, SMP, P) \
texture(sampler3D(TEX, SMP), P)

/* Doesn't work on steam deck, 09_LightShadowPlayground
#define SampleCmp2D(TEX, SMP, PC) \
texture(sampler2DShadow(TEX, SMP), PC)
#define CompareTex2D(TEX, SMP, PC) \
textureLod(sampler2DShadow(TEX, SMP), PC, 0.0f)
#define CompareTex2DProj(TEX, SMP, PC) \
textureProj(sampler2DShadow(TEX, SMP), PC)*/

#define SampleCmp2D(TEX, SMP, PC) _SampleCmp2D((TEX), (SMP), vec3((PC).xyz))
float _SampleCmp2D(texture2D TEX, sampler SMP, vec3 PC)
{ return texture(sampler2DShadow(TEX, SMP), PC); }

#define CompareTex2D(TEX, SMP, PC) _CompareTex2D((TEX), (SMP), vec3((PC).xyz))
float _CompareTex2D(texture2D TEX, sampler SMP, vec3 PC)
{ return textureLod(sampler2DShadow(TEX, SMP), PC, 0.0f); }

#define CompareTex2DProj(TEX, SMP, PC) _CompareTex2DProj((TEX), (SMP), vec4((PC).xyzw))
float _CompareTex2DProj(texture2D TEX, sampler SMP, vec4 PC)
{ return textureProj(sampler2DShadow(TEX, SMP), PC); }

#define SHADER_CONSTANT(INDEX, TYPE, NAME, VALUE) layout (constant_id = INDEX) const TYPE NAME = VALUE

#define FSL_CONST(TYPE, NAME) const TYPE NAME
#define STATIC
#define INLINE

// #define WRITE2D(NAME, COORD, VAL) imageStore(NAME, int2(COORD.xy), VAL)
// #define WRITE3D(NAME, COORD, VAL) imageStore(NAME, int3(COORD.xyz), VAL)

// void write3D()

// void write3D( layout(rgba32f) image2DArray dst, ivec3 coord, vec4 value) { imageStore(dst, coord, value); }


// #define Write2D(TEX, P, V) _Write2D((TEX), ivec2((P).xy), (V))
// void _Write2D(layout(rg32f) image2D TEX, ivec2 P, vec2 V) { imageStore(TEX, P, vec4(V, 0, 0)); }

vec4  _to4(in(vec4)  x)  { return x; }
vec4  _to4(in(vec3)  x)  { return vec4(x, 0); }
vec4  _to4(in(vec2)  x)  { return vec4(x, 0, 0); }
vec4  _to4(in(float) x)  { return vec4(x, 0, 0, 0); }
uvec4 _to4(in(uvec4) x)  { return x; }
uvec4 _to4(in(uvec3) x)  { return uvec4(x, 0); }
uvec4 _to4(in(uvec2) x)  { return uvec4(x, 0, 0); }
uvec4 _to4(in(uint)  x)  { return uvec4(x, 0, 0, 0); }
ivec4 _to4(in(ivec4) x)  { return x; }
ivec4 _to4(in(ivec3) x)  { return ivec4(x, 0); }
ivec4 _to4(in(ivec2) x)  { return ivec4(x, 0, 0); }
ivec4 _to4(in(int)   x)  { return ivec4(x, 0, 0, 0); }

#define Write2D(TEX, P, V) imageStore(TEX, ivec2((P).xy),  _to4(V))
#define Write3D(TEX, P, V) imageStore(TEX, ivec3((P.xyz)), _to4(V))
#define Write2DArray(TEX, P, I, V) imageStore(TEX, ivec3(P, I), _to4(V))

// void _Write2D(layout(rg32f) image2D TEX, ivec2 P, vec2 V) { imageStore(TEX, P, vec4(V, 0, 0)); }
// void _Write2D(image2D TEX, ivec2 P, vec2 V) { imageStore(TEX, P, vec4(V, 0, 0)); }

// #define Write2D( NAME, COORD, VALUE ) imageStore( (NAME), ivec2((COORD).xy), vec4(VALUE, 0, 0) )
// void Write2D(image2D img, )
// #define Write3D( NAME, COORD, VALUE ) imageStore( (NAME), ivec3((COORD).xyz), (VALUE) )

#define Load2D( NAME, COORD ) imageLoad( (NAME), ivec2((COORD).xy) )
#define Load3D( NAME, COORD ) imageLoad( (NAME), ivec3((COORD).xyz) )

#define AtomicMin3D( DST, COORD, VALUE, ORIGINAL_VALUE ) ((ORIGINAL_VALUE) = imageAtomicMin((DST), ivec3((COORD).xyz), (VALUE)))
#define AtomicMax3D( DST, COORD, VALUE, ORIGINAL_VALUE ) ((ORIGINAL_VALUE) = imageAtomicMax((DST), ivec3((COORD).xyz), (VALUE)))
#define AtomicMin2D( DST, COORD, VALUE, ORIGINAL_VALUE ) ((ORIGINAL_VALUE) = imageAtomicMin((DST), ivec2((COORD).xy), (VALUE)))
#define AtomicMax2D( DST, COORD, VALUE, ORIGINAL_VALUE ) ((ORIGINAL_VALUE) = imageAtomicMax((DST), ivec2((COORD).xy), (VALUE)))
#define AtomicMin(DST, VALUE) atomicMin(DST, VALUE)
#define AtomicMax(DST, VALUE) atomicMax(DST, VALUE)
#define AtomicOr(DST, VALUE, ORIGINAL_VALUE) atomicOr(DST, VALUE)
#define AtomicAnd(DST, VALUE, ORIGINAL_VALUE) atomicAnd(DST, VALUE)
#define AtomicXor(DST, VALUE, ORIGINAL_VALUE) atomicXor(DST, VALUE)

#if defined(FT_ATOMICS_64)
#define AtomicMinU64(DST, VALUE) atomicMin(DST, VALUE)
#define AtomicMaxU64(DST, VALUE) atomicMax(DST, VALUE)
#endif

#define UNROLL_N(X)
#define UNROLL
#define LOOP
#define FLATTEN

#define sincos(angle, s, c) { (s) = sin(angle); (c) = cos(angle); }

/* Matrix */

#define to_f3x3(M) mat3(M)

#define f2x2 mat2x2
#define f2x3 mat2x3
#define f2x4 mat2x4
#define f3x2 mat3x2
#define f3x3 mat3x3
#define f3x4 mat3x4
#define f4x2 mat4x2
#define f4x3 mat4x3
#define f4x4 mat4x4

#define make_f2x2_cols(C0, C1) f2x2(C0, C1)
#define make_f2x2_rows(R0, R1) transpose(f2x2(R0, R1))
#define make_f2x2_col_elems f2x2
#define make_f2x3_cols(C0, C1) f2x3(C0, C1)
#define make_f2x3_rows(R0, R1, R2) transpose(f3x2(R0, R1, R2))
#define make_f2x3_col_elems f2x3
#define make_f2x4_cols(C0, C1) f2x4(C0, C1)
#define make_f2x4_rows(R0, R1, R2, R3) transpose(f4x2(R0, R1, R2, R3))
#define make_f2x4_col_elems f2x4

#define make_f3x2_cols(C0, C1, C2) f3x2(C0, C1, C2)
#define make_f3x2_rows(R0, R1) transpose(f2x3(R0, R1))
#define make_f3x2_col_elems f3x2
#define make_f3x3_cols(C0, C1, C2) f3x3(C0, C1,C2)
#define make_f3x3_rows(R0, R1, R2) transpose(f3x3(R0, R1, R2))
#define make_f3x3_col_elems f3x3
#define make_f3x3_row_elems(E00, E01, E02, E10, E11, E12, E20, E21, E22) \
    f3x3(E00, E10, E20, E01, E11, E21, E02, E12, E22)
#define f3x4_cols(C0, C1, C2) f3x4(C0, C1,C2)
#define f3x4_rows(R0, R1, R2, R3) transpose(f4x3(R0, R1, R2, R3))
#define f3x4_col_elems f3x4

#define make_f4x2_cols(C0, C1, C2, C3) f4x2(C0, C1, C2, C3)
#define make_f4x2_rows(R0, R1) transpose(f2x4(R0, R1))
#define make_f4x2_col_elems f4x2
#define make_f4x3_cols(C0, C1, C2, C3) f4x3(C0, C1, C2, C3)
#define make_f4x3_rows(R0, R1, R2) transpose(f3x4(R0, R1, R2))
#define make_f4x3_col_elems f4x3
#define make_f4x4_cols(C0, C1, C2, C3) f4x4(C0, C1, C2, C3)
#define make_f4x4_rows(R0, R1, R2, R3) transpose(f4x4(R0, R1, R2, R3))
#define make_f4x4_col_elems f4x4
#define make_f4x4_row_elems(E00, E01, E02, E03, E10, E11, E12, E13, E20, E21, E22, E23, E30, E31, E32, E33) \
f4x4(E00, E10, E20, E30, E01, E11, E21, E31, E02, E12, E22, E32, E03, E13, E23, E33)

f4x4 Identity() { return f4x4(1.0f); }

#define setElem(M, I, J, V) {M[I][J] = V;}
#define getElem(M, I, J) (M[I][J])

#define getCol(M, I) M[I]
#define getCol0(M) getCol(M, 0)
#define getCol1(M) getCol(M, 1)
#define getCol2(M) getCol(M, 2)
#define getCol3(M) getCol(M, 3)

vec4 getRow(in(mat4)   M, uint i) { return vec4(M[0][i], M[1][i], M[2][i], M[3][i]); }
vec4 getRow(in(mat4x3) M, uint i) { return vec4(M[0][i], M[1][i], M[2][i], M[3][i]); }
vec4 getRow(in(mat4x2) M, uint i) { return vec4(M[0][i], M[1][i], M[2][i], M[3][i]); }

vec3 getRow(in(mat3x4) M, uint i) { return vec3(M[0][i], M[1][i], M[2][i]); }
vec3 getRow(in(mat3)   M, uint i) { return vec3(M[0][i], M[1][i], M[2][i]); }
vec3 getRow(in(mat3x2) M, uint i) { return vec3(M[0][i], M[1][i], M[2][i]); }

vec2 getRow(in(mat2x4) M, uint i) { return vec2(M[0][i], M[1][i]); }
vec2 getRow(in(mat2x3) M, uint i) { return vec2(M[0][i], M[1][i]); }
vec2 getRow(in(mat2)   M, uint i) { return vec2(M[0][i], M[1][i]); }

#define getRow0(M) getRow(M, 0)
#define getRow1(M) getRow(M, 1)
#define getRow2(M) getRow(M, 2)
#define getRow3(M) getRow(M, 3)


f4x4 setCol(inout(f4x4) M, in(vec4) col, const uint i) { M[i] = col; return M; }
f4x3 setCol(inout(f4x3) M, in(vec3) col, const uint i) { M[i] = col; return M; }
f4x2 setCol(inout(f4x2) M, in(vec2) col, const uint i) { M[i] = col; return M; }

f3x4 setCol(inout(f3x4) M, in(vec4) col, const uint i) { M[i] = col; return M; }
f3x3 setCol(inout(f3x3) M, in(vec3) col, const uint i) { M[i] = col; return M; }
f3x2 setCol(inout(f3x2) M, in(vec2) col, const uint i) { M[i] = col; return M; }

f2x4 setCol(inout(f2x4) M, in(vec4) col, const uint i) { M[i] = col; return M; }
f2x3 setCol(inout(f2x3) M, in(vec3) col, const uint i) { M[i] = col; return M; }
f2x2 setCol(inout(f2x2) M, in(vec2) col, const uint i) { M[i] = col; return M; }

#define setCol0(M, C) setCol(M, C, 0)
#define setCol1(M, C) setCol(M, C, 1)
#define setCol2(M, C) setCol(M, C, 2)
#define setCol3(M, C) setCol(M, C, 3)


f4x4 setRow(inout(f4x4) M, in(vec4) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; M[3][i] = row[3]; return M; }
f4x3 setRow(inout(f4x3) M, in(vec4) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; M[3][i] = row[3]; return M; }
f4x2 setRow(inout(f4x2) M, in(vec4) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; M[3][i] = row[3]; return M; }

f3x4 setRow(inout(f3x4) M, in(vec3) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; return M; }
f3x3 setRow(inout(f3x3) M, in(vec3) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; return M; }
f3x2 setRow(inout(f3x2) M, in(vec3) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; return M; }

f2x4 setRow(inout(f2x4) M, in(vec2) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; return M; }
f2x3 setRow(inout(f2x3) M, in(vec2) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; return M; }
f2x2 setRow(inout(f2x2) M, in(vec2) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; return M; }

#define setRow0(M, R) setRow(M, R, 0)
#define setRow1(M, R) setRow(M, R, 1)
#define setRow2(M, R) setRow(M, R, 2)
#define setRow3(M, R) setRow(M, R, 3)

#define packed_float3 vec3

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define float float
#define float2 vec2
#define float3 vec3
#define packed_float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define float3x2 mat3x2
#define float2x3 mat2x3
#define float4x4 mat4

#define double2 dvec2
#define double3 dvec3
#define double4 dvec4
#define double2x2 dmat2
#define double3x3 dmat3
#define double4x4 dmat4

#define min16float  mediump float
#define min16float2 mediump float2
#define min16float3 mediump float3
#define min16float4 mediump float4

#define SamplerState sampler
#define SamplerComparisonState sampler
#define NO_SAMPLER 0u

// #define GetDimensions(TEXTURE, SAMPLER) textureSize(TEXTURE, 0)
// int2 GetDimensions(texture2D t, sampler s)
//     { return textureSize(sampler2D(t, s), 0); }
// int2 GetDimensions(textureCube t, sampler s)
//     { return textureSize(samplerCube(t, s), 0); }

#ifdef GL_EXT_samplerless_texture_functions
#extension GL_EXT_samplerless_texture_functions : enable
// int2 GetDimensions( texture2D t, uint _NO_SAMPLER) { return textureSize(t, 0); }
// int2 GetDimensions(utexture2D t, uint _NO_SAMPLER) { return textureSize(t, 0); }
// int2 GetDimensions(itexture2D t, uint _NO_SAMPLER) { return textureSize(t, 0); }
#endif

// #ifdef GL_EXT_samplerless_texture_functions
// #extension GL_EXT_samplerless_texture_functions : enable
//  vec4 _LoadTex2D( texture2D TEX, uint _NO_SAMPLER, ivec2 P) { return texelFetch(TEX, P, 0); }
// uvec4 _LoadTex2D(utexture2D TEX, uint _NO_SAMPLER, ivec2 P) { return texelFetch(TEX, P, 0); }
// ivec4 _LoadTex2D(itexture2D TEX, uint _NO_SAMPLER, ivec2 P) { return texelFetch(TEX, P, 0); }
// #endif

int2 imageSize(utexture2D TEX) { return textureSize(TEX, 0); }
int2 imageSize(texture2D TEX) { return textureSize(TEX, 0); }
int2 imageSize(texture2DMS TEX) { return textureSize(TEX); }
int2 imageSize(textureCube TEX) { return textureSize(TEX, 0); }
int3 imageSize(texture2DArray TEX) { return textureSize(TEX, 0); }
int3 imageSize(utexture2DArray TEX) { return textureSize(TEX, 0); }

#define GetDimensions(TEX, SMP) imageSize(TEX)
#define GetDimensionsMS(TEX, DIM) int2 DIM; { DIM = imageSize(TEX); }

// int2 GetDimensions(writeonly iimage2D t, uint _NO_SAMPLER) { return imageSize(t); }
// int2 GetDimensions(writeonly uimage2D t, uint _NO_SAMPLER) { return imageSize(t); }
// int2 GetDimensions(writeonly image2D t, uint _NO_SAMPLER) { return imageSize(t); }

#ifdef FT_ATOMICS_64
#define VK_T_uint64_t(T)   u64 ## T
#endif

#define VK_T_uint(T)   u ## T
#define VK_T_uint2(T)  u ## T
#define VK_T_uint4(T)  u ## T
#define VK_T_int(T)    i ## T
#define VK_T_int2(T)   i ## T
#define VK_T_int4(T)   i ## T
#define VK_T_float(T)       T
#define VK_T_float2(T)      T
#define VK_T_float4(T)      T
#define VK_T_half(T)       T
#define VK_T_half2(T)      T
#define VK_T_half4(T)      T

// TODO: get rid of these
#define VK_T_uint3(T)  u ## T
#define VK_T_int3(T)   i ## T
#define VK_T_float3(T)      T
#define VK_T_half3(T)      T

#define TexCube(T)           VK_T_##T(textureCube)
#define TexCubeArray(T)      VK_T_##T(textureCubeArray)

#define Tex1D(T)             VK_T_##T(texture1D)
#define Tex2D(T)             VK_T_##T(texture2D)
#define Tex3D(T)             VK_T_##T(texture3D)

#define Tex2DMS(T, S)        VK_T_##T(texture2DMS)

#define Tex1DArray(T)        VK_T_##T(texture1DArray)
#define Tex2DArray(T)        VK_T_##T(texture2DArray)
#define Tex2DArrayMS(T, S)   VK_T_##T(texture2DMSArray)

#define RWTexCube(T)         VK_T_##T(imageCube)
#define RWTexCubeArray(T)    VK_T_##T(imageCubeArray)

#define RWTex1D(T)           VK_T_##T(image1D)
#define RWTex2D(T)           VK_T_##T(image2D)
#define RWTex3D(T)           VK_T_##T(image3D)

#define RWTex2DMS(T, S)      VK_T_##T(image2DMS)

#define RWTex1DArray(T)      VK_T_##T(image1DArray)
#define RWTex2DArray(T)      VK_T_##T(image2DArray)

#define WTex1D RWTex1D
#define WTex2D RWTex2D
#define WTex3D RWTex3D
#define WTex1DArray RWTex1DArray
#define WTex2DArray RWTex2DArray

#define RTex1D RWTex1D
#define RTex2D RWTex2D
#define RTex3D RWTex3D
#define RTex1DArray RWTex1DArray
#define RTex2DArray RWTex2DArray

#define RWTex2DArrayMS(T, S) VK_T_##T(image2DMSArray)

#define RasterizerOrderedTex2D(ELEM_TYPE, GROUP_INDEX) coherent RWTex2D(ELEM_TYPE)
#define RasterizerOrderedTex2DArray(ELEM_TYPE, GROUP_INDEX) coherent RWTex2DArray(ELEM_TYPE)

#define Depth2D(T)              VK_T_##T(texture2D)
#define Depth2DMS(T, S)         VK_T_##T(texture2DMS)
#define Depth2DArray(T)         VK_T_##T(texture2DArray)
#define Depth2DArrayMS(T, S)    VK_T_##T(texture2DMSArray)

// matching hlsl semantics, glsl mod preserves sign(Y)
#define fmod(X, Y)           (abs(mod(X, Y))*sign(X))

#define fast_min min
#define fast_max max
#define isordered(X, Y) ( ((X)==(X)) && ((Y)==(Y)) )
#define isunordered(X, Y) (isnan(X) || isnan(Y))

#define extract_bits bitfieldExtract
#define insert_bits bitfieldInsert

#define frac(VALUE)			fract(VALUE)
#define lerp mix
// #define lerp(X, Y, VALUE)	mix(X, Y, VALUE)
// vec4 lerp(vec4 X, vec4 Y, float VALUE) { return mix(X, Y, VALUE); }
// vec3 lerp(vec3 X, vec3 Y, float VALUE) { return mix(X, Y, VALUE); }
// vec2 lerp(vec2 X, vec2 Y, float VALUE) { return mix(X, Y, VALUE); }
// float lerp(float X, float Y, float VALUE) { return mix(X, Y, VALUE); }
#define rsqrt(VALUE)		inversesqrt(VALUE)
float saturate(float VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
vec2 saturate(vec2 VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
vec3 saturate(vec3 VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
vec4 saturate(vec4 VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
// #define saturate(VALUE)		clamp(VALUE, 0.0f, 1.0f)
#define ddx(VALUE)			dFdx(VALUE)
#define ddy(VALUE)			dFdy(VALUE)
#define ddx_fine(VALUE)		dFdx(VALUE) // fallback
#define ddy_fine(VALUE)		dFdy(VALUE) // dFdxFine/dFdyFine available in opengl 4.5
#define rcp(VALUE)			(1.0f / (VALUE))
#define atan2(X, Y)			atan(X, Y)
#define reversebits(X)		bitfieldReverse(X)
#define asuint(X)			floatBitsToUint(X)
#define asfloat(X)			uintBitsToFloat(X)
#define mad(a, b, c)		(a) * (b) + (c)

// case list
#define REPEAT_TEN(base) CASE(base) CASE(base+1) CASE(base+2) CASE(base+3) CASE(base+4) CASE(base+5) CASE(base+6) CASE(base+7) CASE(base+8) CASE(base+9)
#define REPEAT_HUNDRED(base)	REPEAT_TEN(base) REPEAT_TEN(base+10) REPEAT_TEN(base+20) REPEAT_TEN(base+30) REPEAT_TEN(base+40) REPEAT_TEN(base+50) \
                                REPEAT_TEN(base+60) REPEAT_TEN(base+70) REPEAT_TEN(base+80) REPEAT_TEN(base+90)

#define CASE_LIST_256 CASE(0)	REPEAT_HUNDRED(1) REPEAT_HUNDRED(101) \
                            REPEAT_TEN(201) REPEAT_TEN(211) REPEAT_TEN(221) REPEAT_TEN(231) REPEAT_TEN(241) \
                            CASE(251) CASE(252) CASE(253) CASE(254) CASE(255)

#define EARLY_FRAGMENT_TESTS layout(early_fragment_tests) in;

bool any(vec2 x) { return any(notEqual(x, vec2(0))); }
bool any(vec3 x) { return any(notEqual(x, vec3(0))); }

//  tessellation
#define INPUT_PATCH(T, NC) T[NC]
#define OUTPUT_PATCH(T, NC) T[NC]
#define TESS_VS_SHADER(X)

#define PATCH_CONSTANT_FUNC(F)     //[patchconstantfunc(F)]
#define OUTPUT_CONTROL_POINTS(P)   //[outputcontrolpoints(P)]
#define MAX_TESS_FACTOR(F)         //[maxtessfactor(F)]
#define FSL_DomainPartitioning(X, Y) //[domain(X)] [partitioning(Y)]
#define FSL_OutputTopology(T)        //[outputtopology(T)]

#define VK_TESS_quad quads
#define VK_TESS_integer equal_spacing
#define VK_TESS_triangle_ccw ccw

#ifdef STAGE_TESE
#define TESS_LAYOUT(D, P, T) layout( VK_TESS_##D, VK_TESS_##P, VK_TESS_##T) in(_NONE);
#else
#define TESS_LAYOUT(D, P, T)
#endif

#ifdef ENABLE_WAVEOPS

#define ballot_t uvec4

#define CountBallot(X) (bitCount(X.x) + bitCount(X.y) + bitCount(X.z) + bitCount(X.w))

#ifdef WAVE_OPS_BASIC_BIT
    #extension GL_KHR_shader_subgroup_basic: enable
#endif

#ifdef TARGET_SWITCH
    #extension GL_ARB_shader_ballot: enable
    #extension GL_ARB_gpu_shader_int64: enable
    bool WaveIsFirstLane() { return gl_SubGroupInvocationARB == 0; }
    #define WaveGetLaneIndex() gl_SubGroupInvocationARB
    #define WaveGetMaxActiveIndex() (gl_SubGroupSizeARB - 1)
    #define WaveReadLaneFirst readFirstInvocationARB
#else
    #ifdef WAVE_OPS_VOTE_BIT
        #extension GL_KHR_shader_subgroup_vote: enable
    #endif
    #ifdef WAVE_OPS_ARITHMETIC_BIT
        #extension GL_KHR_shader_subgroup_arithmetic: enable
    #endif
    #ifdef WAVE_OPS_BALLOT_BIT
        #extension GL_KHR_shader_subgroup_ballot: enable
    #endif
    #ifdef WAVE_OPS_QUAD_BIT
        #extension GL_KHR_shader_subgroup_quad: enable
    #endif
    #ifdef WAVE_OPS_SHUFFLE_BIT
        #extension GL_KHR_shader_subgroup_shuffle: enable
    #endif
    
    #ifdef WAVE_OPS_BASIC_BIT
        #define WaveIsFirstLane        subgroupElect
        #define WaveGetLaneIndex() gl_SubgroupInvocationID
		#define WaveIsHelperLane() gl_HelperInvocation
    #endif
    
    #ifdef WAVE_OPS_ARITHMETIC_BIT
        #define WaveGetMaxActiveIndex() subgroupMax(gl_SubgroupInvocationID)
    #endif

    #ifdef WAVE_OPS_BALLOT_BIT
        #define WaveReadLaneFirst subgroupBroadcastFirst
    #endif
#endif

    #ifdef WAVE_OPS_VOTE_BIT
        #define WaveActiveAnyTrue      subgroupAny
        #define WaveActiveAllTrue      subgroupAll
    #endif

    #ifdef WAVE_OPS_ARITHMETIC_BIT
        #define WaveActiveMax subgroupMax
        #define WaveActiveMin subgroupMin
        #define WaveActiveSum subgroupAdd
        #define WavePrefixSum subgroupExclusiveAdd
    #endif

    #ifdef WAVE_OPS_BALLOT_BIT
        #define WavePrefixCountBits(X) subgroupBallotExclusiveBitCount(subgroupBallot(X))
        #define WaveActiveCountBits(X) subgroupBallotBitCount(subgroupBallot(X))
        
        #ifdef TARGET_ANDROID
            #define WaveActiveBallot(X) subgroupBallot(false)
        #else
            #define WaveActiveBallot subgroupBallot
        #endif
    #endif

    #ifdef WAVE_OPS_QUAD_BIT
        #define QuadReadAcrossX subgroupQuadSwapHorizontal
        #define QuadReadAcrossY subgroupQuadSwapVertical
    #endif

    #ifdef WAVE_OPS_SHUFFLE_BIT
        #define WaveReadLaneAt(X, Y) subgroupShuffle(X, Y)
    #endif

    #define countbits bitCount
    
#endif

#ifdef GL_ARB_fragment_shader_interlock
#extension GL_ARB_fragment_shader_interlock : enable
#define BeginPSInterlock() beginInvocationInterlockARB()
#define EndPSInterlock() endInvocationInterlockARB()
#else
#define EnablePSInterlock()
#define BeginPSInterlock()
#define EndPSInterlock()
#endif

#define SET_OUTPUT_FORMAT(target, fmt)
#define PS_ZORDER_EARLYZ()


#endif // _VULKAN_H
