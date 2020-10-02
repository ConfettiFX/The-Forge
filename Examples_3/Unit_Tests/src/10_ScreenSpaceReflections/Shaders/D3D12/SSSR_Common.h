/**********************************************************************
Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef FFX_SSSR_COMMON
#define FFX_SSSR_COMMON

#define FFX_SSSR_PI                                  3.14159265358979f
#define FFX_SSSR_GOLDEN_RATIO                        1.61803398875f

#define FFX_SSSR_FLOAT_MAX                           3.402823466e+38

#define FFX_SSSR_FALSE                               0
#define FFX_SSSR_TRUE                                1

#define FFX_SSSR_USE_ROUGHNESS_OVERRIDE              FFX_SSSR_FALSE
#define FFX_SSSR_ROUGHNESS_OVERRIDE                  0.1

#define FFX_SSSR_TEMPORAL_VARIANCE_THRESHOLD         0.0005

#if FFX_SSSR_USE_ROUGHNESS_OVERRIDE
float FfxSssrUnpackRoughness(FFX_SSSR_ROUGHNESS_TEXTURE_FORMAT packed) { return FFX_SSSR_ROUGHNESS_OVERRIDE; }
#else
FFX_SSSR_ROUGHNESS_UNPACK_FUNCTION
#endif

FFX_SSSR_NORMALS_UNPACK_FUNCTION
FFX_SSSR_MOTION_VECTOR_UNPACK_FUNCTION
FFX_SSSR_DEPTH_UNPACK_FUNCTION
FFX_SSSR_SCENE_RADIANCE_UNPACK_FUNCTION

// Common constants
cbuffer Constants : register(b0)
{
    float4x4 g_inv_view_proj;
    float4x4 g_proj;
    float4x4 g_inv_proj;
    float4x4 g_view;
    float4x4 g_inv_view;
    float4x4 g_prev_view_proj;

    uint g_frame_index;
    uint g_max_traversal_intersections;
    uint g_min_traversal_occupancy;
    uint g_most_detailed_mip;
    float g_temporal_stability_factor;
    float g_depth_buffer_thickness;
    uint g_samples_per_quad;
    uint g_temporal_variance_guided_tracing_enabled;
    float g_roughness_threshold;
    uint g_skip_denoiser;
};

// Mat must be able to transform origin from its current space into screen space.
float3 ProjectPosition(float3 origin, float4x4 mat)
{
    float4 projected = mul(float4(origin, 1), mat);
    projected.xyz /= projected.w;
    projected.xy = 0.5 * projected.xy + 0.5;
    projected.y = (1 - projected.y);
    return projected.xyz;
}

// Mat must be able to transform origin from screen space to a linear space.
float3 InvProjectPosition(float3 origin, float4x4 mat)
{
    origin.y = (1 - origin.y);
    origin.xy = 2 * origin.xy - 1;
    float4 projected = mul(float4(origin, 1), mat);
    projected.xyz /= projected.w;
    return projected.xyz;
}

// Origin and direction must be in the same space and mat must be able to transform from that space into screen space.
float3 ProjectDirection(float3 origin, float3 direction, float3 screen_space_origin, float4x4 mat)
{
    float3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

struct Ray
{
    float3 origin;
    float3 direction;
};

// Create a ray that originates at the depth buffer surface and points away from the camera.
Ray CreateViewSpaceRay(float3 screen_space_pos)
{
    float3 view_space_pos = InvProjectPosition(screen_space_pos, g_inv_proj);
    Ray view_space_ray;
    view_space_ray.origin = view_space_pos;
    view_space_ray.direction = view_space_pos;
    return view_space_ray;
}

float3 LoadNormal(int2 index, Texture2D<FFX_SSSR_NORMALS_TEXTURE_FORMAT> tex)
{
    return FfxSssrUnpackNormals(tex.Load(int3(index, 0)));
}

float LoadRoughness(int2 index, Texture2D<FFX_SSSR_ROUGHNESS_TEXTURE_FORMAT> tex)
{
    return FfxSssrUnpackRoughness(tex.Load(int3(index, 0)));
}

bool IsGlossy(float roughness)
{
    return roughness < g_roughness_threshold;
}

bool IsMirrorReflection(float roughness)
{
    return roughness < 0.0001;
}

float GetEdgeStoppingNormalWeight(float3 normal_p, float3 normal_q, float sigma)
{
    return pow(max(dot(normal_p, normal_q), 0.0), sigma);
}

float GetEdgeStoppingRoughnessWeight(float roughness_p, float roughness_q, float sigma_min, float sigma_max)
{
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

min16float GetEdgeStoppingRoughnessWeightFP16(min16float roughness_p, min16float roughness_q, min16float sigma_min, min16float sigma_max)
{
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

// Roughness weight to prevent ghosting on pure mirror reflections
float GetRoughnessAccumulationWeight(float roughness)
{
    float near_singular_roughness = 0.00001;
    return smoothstep(0.0, near_singular_roughness, roughness);
}

float Gaussian(float x, float m, float sigma)
{
    float a = length(x - m) / sigma;
    a *= a;
    return exp(-0.5 * a);
}

float Luminance(float3 clr)
{
    return max(dot(clr, float3(0.299, 0.587, 0.114)), 0.00001);
}

uint Pack(uint2 coord)
{
    return (coord.x & 0xFFFF) | (coord.y & 0xFFFF) << 16;
}

uint2 Unpack(uint packed)
{
    return uint2(packed & 0xFFFF, packed >> 16);
}

bool IsBaseRay(uint2 did, uint samples_per_quad)
{
    switch (samples_per_quad)
    {
    case 1:
        return ((did.x & 1) | (did.y & 1)) == 0; // Deactivates 3 out of 4 rays
    case 2:
        return (did.x & 1) == (did.y & 1); // Deactivates 2 out of 4 rays. Keeps diagonal.
    default: // case 4:
        return true;
    }
}

// Has to match the calculation in IsBaseRay. Assumes lane is in Z order.
// i.e. 4 consecutive lanes 0 1 2 3
// are remapped to 
// 0 1
// 2 3
uint GetBaseLane(uint lane, uint samples_per_quad)
{
    switch (samples_per_quad)
    {
    case 1:
        return lane & (~0b11); // Map to upper left
    case 2:
        return lane ^ 0b1; // Toggle horizontal
    default: // case 4:
        return lane; // Identity
    }
}

uint PackFloat16(min16float2 v)
{
    uint2 p = f32tof16(float2(v));
    return p.x | (p.y << 16);
}

min16float2 UnpackFloat16(uint a)
{
    float2 tmp = f16tof32(
        uint2(a & 0xFFFF, a >> 16));
    return min16float2(tmp);
}


// From ffx_a.h



uint BitfieldExtract(uint src, uint off, uint bits) { uint mask = (1 << bits) - 1; return (src >> off) & mask; } // ABfe
uint BitfieldInsert(uint src, uint ins, uint bits) { uint mask = (1 << bits) - 1; return (ins & mask) | (src & (~mask)); } // ABfiM

//  LANE TO 8x8 MAPPING
//  ===================
//  00 01 08 09 10 11 18 19 
//  02 03 0a 0b 12 13 1a 1b
//  04 05 0c 0d 14 15 1c 1d
//  06 07 0e 0f 16 17 1e 1f 
//  20 21 28 29 30 31 38 39 
//  22 23 2a 2b 32 33 3a 3b
//  24 25 2c 2d 34 35 3c 3d
//  26 27 2e 2f 36 37 3e 3f 
uint2 RemapLane8x8(uint lane) { return uint2(BitfieldInsert(BitfieldExtract(lane, 2u, 3u), lane, 1u), BitfieldInsert(BitfieldExtract(lane, 3u, 3u), BitfieldExtract(lane, 1u, 2u), 2u)); } // ARmpRed8x8

#endif // FFX_SSSR_COMMON