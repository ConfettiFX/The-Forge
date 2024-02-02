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

#pragma once

// Most functions in this file are also available from shaders

// Define FSL_SHADER_LIB when including this file from a shader
#if defined(FSL_SHADER_LIB)
#define inline
#define CONST_REF(T) T
#else
#include "MathTypes.h"

// PVS-Studio reports 'error V813: Decreased performance.' when passing parameters bigger than 8 bytes by value
#define CONST_REF(T) const T&

#define float4x4     mat4
#define float3x3     mat3
#define to_f3x3(M)   mat3(M.getUpper3x3())

inline float2 saturate(float2 f) { return float2(saturate(f.x), saturate(f.y)); }

inline float3 saturate(CONST_REF(float3) f) { return float3(saturate(f.x), saturate(f.y), saturate(f.z)); }

#endif

// Source: Inigo Quilez - https://www.shadertoy.com/view/Wsc3z2
inline float3x3 adjoint(CONST_REF(float3x3) m)
{
    // Computes the lower 3x3 part of the adjoint.
    // Use to transform normals with arbitrary
    // matrices.The code assumes the last column of m is
    // [0,0,0,1]. More info here:
    // https://github.com/graphitemaster/normals_revisited

    return float3x3(cross(m[1], m[2]), cross(m[2], m[0]), cross(m[0], m[1]));
}

// Utility to transform normals from model to world.
// Adjoint matrix is the same as transpose(inverse(m)) as long as the model matrix doesn't have shears.
inline float3x3 adjoint_float4x4(CONST_REF(float4x4) m) { return adjoint(to_f3x3(m)); }

inline float linearizeDepth(float depth, float nearPlane, float farPlane)
{
    return (nearPlane * farPlane) / (farPlane + depth * (nearPlane - farPlane));
}

inline float linearizeDepthReverseZ(float depth, float nearPlane, float farPlane)
{
    return (nearPlane * farPlane) / (nearPlane + depth * (farPlane - nearPlane));
}

inline float2 octWrap_float2(float2 v)
{
    return (float2(1.f, 1.f) - float2(abs(v.y), abs(v.x))) * float2(v.x >= 0.f ? 1.f : -1.f, v.y >= 0.f ? 1.f : -1.f);
}

inline float octWrap(float v, float w) { return ((1.0f - abs((w))) * ((v) >= 0.0f ? 1.0f : -1.0f)); }

inline float2 encodeDir(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));

    float2 result = float2(n.x, n.y);
    if (n.z < 0.f)
        result = octWrap_float2(result);
    result = result * 0.5f + float2(0.5f, 0.5f);
    return result;
}

inline float3 decodeDir(float2 encN)
{
    encN = encN * 2.0f - float2(1.0f, 1.f);

    float3 n;
    n.z = 1.0f - abs(encN.x) - abs(encN.y);
#if defined(FSL_SHADER_LIB)
    n.xy = n.z >= 0.0f ? encN.xy : octWrap_float2(encN.xy);
    n = normalize(n);
#else
    if (n.z >= 0.0f)
    {
        n.x = encN.x;
        n.y = encN.y;
    }
    else
    {
        float2 wrap = octWrap_float2(float2(encN.x, encN.y));
        n.x = wrap.x;
        n.y = wrap.y;
    }
    n /= sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
#endif
    return n;
}

inline uint float2ToUnorm2x16(float x, float y)
{
    uint xx = uint(round(clamp(x, 0.f, 1.f) * 65535.0f));
    uint yy = uint(round(clamp(y, 0.f, 1.f) * 65535.0f));
    return (uint(0x0000FFFF) & xx) | ((yy << 16) & uint(0xFFFF0000));
}

inline uint packFloat3DirectionToHalf2(CONST_REF(float3) dir)
{
    float absLength = abs(dir.x) + abs(dir.y) + abs(dir.z);
    if (absLength != 0.f)
    {
        float3 enc;
        enc.x = dir.x / absLength;
        enc.y = dir.y / absLength;
        enc.z = dir.z / absLength;
        if (enc.z < 0)
        {
            float oldX = enc.x;
            enc.x = octWrap(enc.x, enc.y);
            enc.y = octWrap(enc.y, oldX);
        }
        enc.x = enc.x * 0.5f + 0.5f;
        enc.y = enc.y * 0.5f + 0.5f;
        return float2ToUnorm2x16(enc.x, enc.y);
    }

    return 0;
}

//------------------------------------------------------------------------------
//	Pack three positive normalized numbers between 0.0 and 1.0 into a 32-bit fp
// channel of a render target.  The bits given to each member are:
//
//		x = 16 bits
//		y = 8 bits
//		z = 8 bits
//
inline float pack3PNForFP32(float3 channel)
{
    // prevent unnormalized values from bleeding between channels
    channel = saturate(channel);

    // layout of a 32-bit fp register
    // SEEEEEEEEMMMMMMMMMMMMMMMMMMMMMMM
    // 1 sign bit; 8 bits for the exponent and 23 bits for the mantissa
    uint uValue;
    //	uint uEncodedb;

    // pack x
    uValue = (uint(channel.x * 65535.0f + 0.5f)); // goes from bit 0 to 7

    // pack y in EMMMMMMM
    uValue |= (uint(channel.y * 255.0f + 0.5f)) << 16; // goes from bit 16 to 23

    // pack z in SEEEEEEE
    // the last E will never be 1b because the upper value is 254
    // max value is 11111110 == 254
    // this prevents the bits of the exponents to become all 1
    // range is 1.. 254
    // to prevent an exponent that is 0 we can check
    // check if SEEEEEEE is all 0
    // only check the highest 7-bit 0111 1111 0000 0000 0000 0000 0000 0000 == 0x7F000000
    // to prevent all 7-bits from going zero we add 0000 0001 0000 0000 0000 0000 because we assume that
    // all are zero :-;
    uValue |= (uint(channel.z * 253.0f + 1.5f)) << 24; // goes from bit 24 to 31

#if defined(FSL_SHADER_LIB)
    return asfloat(uValue);
#else
    union Cast
    {
        uint  u;
        float f;
    };

    Cast cst;
    cst.u = uValue;
    return cst.f;
#endif
}

//------------------------------------------------------------------------------
// Unpack function
//
inline float3 unpack3PNFromFP32(float fFloatFromFP32)
{
    float a, b, c;

#if defined(FSL_SHADER_LIB)
    uint uInputFloat = asuint(fFloatFromFP32);
#else
    union Cast
    {
        uint  u;
        float f;
    };

    Cast cst;
    cst.f = fFloatFromFP32;
    uint uInputFloat = cst.u;
#endif

    // unpack a
    // mask out all the stuff above 8-bit with 0xFF == 255
    a = ((uInputFloat)&0xFFFF) / 65535.0f;

    b = ((uInputFloat >> 16) & 0xFF) / 255.0f;

    // extract the 1..254 value range and subtract 1
    // ending up with 0..253
    c = (((uInputFloat >> 24) & 0xFF) - 1.0f) / 253.0f;

    return float3(a, b, c);
}

inline uint packUintFromUint2x16(uint2 v) { return (uint(0x0000FFFF) & v.x) | ((v.y << 16) & uint(0xFFFF0000)); }

inline uint2 unpackUint2x16FromUint32(uint v) { return uint2(v & uint(0x0000FFFF), (v >> 16) & uint(0x0000FFFF)); }

inline uint2 packUint4x16FromUint2x32(CONST_REF(uint4) v)
{
    return uint2((uint(0x0000FFFF) & v.x) | ((v.y << 16) & uint(0xFFFF0000)), (uint(0x0000FFFF) & v.z) | ((v.w << 16) & uint(0xFFFF0000)));
}

inline uint4 unpackUint4x16FromUint2x32(uint2 v)
{
    return uint4(uint(0x0000FFFF) & v.x, (v.x >> 16) & uint(0x0000FFFF), uint(0x0000FFFF) & v.y, (v.y >> 16) & uint(0x0000FFFF));
}

// Functions that have their own implementation on Graphics/ShaderUtilities.h.fsl because they use shader intrinsics
// Implementation here is just to expose them to C++
#if !defined(FSL_SHADER_LIB)
inline uint packSnorm2x16(float2 v)
{
    uint2 SNorm = uint2((uint)round(clamp(v.x, -1.f, 1.f) * 32767.0f), (uint)round(clamp(v.y, -1.f, 1.f) * 32767.0f));
    return (0x0000FFFF & SNorm.x) | ((SNorm.y << 16) & 0xFFFF0000);
}

inline float2 unpackSnorm2x16(uint p)
{
    float2 ret = float2(clamp((0x0000FFFF & p) / 32767.0f, -1.f, 1.f), clamp(((0xFFFF0000 & p) >> 16) / 32767.0f, -1.f, 1.f));

    return ret;
}

inline uint packUnorm2x16(float2 v)
{
    uint2 UNorm = uint2((uint)round(saturate(v.x) * 65535.0f), (uint)round(saturate(v.y) * 65535.0f));
    return (0x0000FFFF & UNorm.x) | ((UNorm.y << 16) & 0xFFFF0000);
}

inline float2 unpackUnorm2x16(uint p)
{
    float2 ret;
    ret.x = saturate((0x0000FFFF & p) / 65535.0f);
    ret.y = saturate(((0xFFFF0000 & p) >> 16) / 65535.0f);
    return ret;
}

inline uint packUnorm4x8(CONST_REF(float4) v)
{
    uint4 UNorm = uint4((uint)round(saturate(v.x) * 255.0f), (uint)round(saturate(v.y) * 255.0f), (uint)round(saturate(v.z) * 255.0f),
                        (uint)round(saturate(v.w) * 255.0f));
    return (0x000000FF & UNorm.x) | ((UNorm.y << 8) & 0x0000FF00) | ((UNorm.z << 16) & 0x00FF0000) | ((UNorm.w << 24) & 0xFF000000);
}

inline float4 unpackUnorm4x8(uint p)
{
    return float4(float(p & 0x000000FF) / 255.0f, float((p & 0x0000FF00) >> 8) / 255.0f, float((p & 0x00FF0000) >> 16) / 255.0f,
                  float((p & 0xFF000000) >> 24) / 255.0f);
}
#endif

// Functions only supported in c++
#if !defined(FSL_SHADER_LIB)

#define F16_EXPONENT_BITS  0x1F
#define F16_EXPONENT_SHIFT 10
#define F16_EXPONENT_BIAS  15
#define F16_MANTISSA_BITS  0x3ff
#define F16_MANTISSA_SHIFT (23 - F16_EXPONENT_SHIFT)
#define F16_MAX_EXPONENT   (F16_EXPONENT_BITS << F16_EXPONENT_SHIFT)

inline uint16_t floatToHalf(float val)
{
    union Cast
    {
        uint32_t u;
        float    f;
    };

    Cast cst;
    cst.f = val;

    uint32_t f32 = cst.u;
    uint16_t f16 = 0;
    /* Decode IEEE 754 little-endian 32-bit floating-point value */
    int      sign = (f32 >> 16) & 0x8000;
    /* Map exponent to the range [-127,128] */
    int      exponent = ((f32 >> 23) & 0xff) - 127;
    int      mantissa = f32 & 0x007fffff;
    if (exponent == 128)
    { /* Infinity or NaN */
        f16 = (uint16_t)(sign | F16_MAX_EXPONENT);
        if (mantissa)
            f16 |= (mantissa & F16_MANTISSA_BITS);
    }
    else if (exponent > 15)
    { /* Overflow - flush to Infinity */
        f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
    }
    else if (exponent > -15)
    { /* Representable value */
        exponent += F16_EXPONENT_BIAS;
        mantissa >>= F16_MANTISSA_SHIFT;
        f16 = (unsigned short)(sign | exponent << F16_EXPONENT_SHIFT | mantissa);
    }
    else
    {
        f16 = (unsigned short)sign;
    }
    return f16;
}

inline uint32_t packFloat2ToHalf2(float2 f) { return (floatToHalf(f.x) & 0x0000FFFF) | ((floatToHalf(f.y) << 16) & 0xFFFF0000); }

#endif

#ifdef FSL_SHADER_LIB
#undef inline
#else
#undef float4x4
#undef float3x3
#undef to_f3x3
#endif

#undef CONST_REF
