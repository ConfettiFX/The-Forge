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

ENABLE_WAVEOPS()

#ifndef FFX_SSSR_ROUGHNESS_TEXTURE_FORMAT
    #define FFX_SSSR_ROUGHNESS_TEXTURE_FORMAT float4
#endif
#ifndef FFX_SSSR_ROUGHNESS_UNPACK_FUNCTION
    #define FFX_SSSR_ROUGHNESS_UNPACK_FUNCTION \
    float FfxSssrUnpackRoughness(FFX_SSSR_ROUGHNESS_TEXTURE_FORMAT _packed) { return _packed.r; }
#endif
#ifndef FFX_SSSR_NORMALS_TEXTURE_FORMAT
    #define FFX_SSSR_NORMALS_TEXTURE_FORMAT float4
#endif
#ifndef FFX_SSSR_NORMALS_UNPACK_FUNCTION
    #define FFX_SSSR_NORMALS_UNPACK_FUNCTION \
    float3 FfxSssrUnpackNormals(FFX_SSSR_NORMALS_TEXTURE_FORMAT _packed) { return normalize(_packed.xyz); }
#endif
#ifndef FFX_SSSR_DEPTH_TEXTURE_FORMAT
    #define FFX_SSSR_DEPTH_TEXTURE_FORMAT float
#endif
#ifndef FFX_SSSR_DEPTH_UNPACK_FUNCTION
    #define FFX_SSSR_DEPTH_UNPACK_FUNCTION \
    float FfxSssrUnpackDepth(FFX_SSSR_DEPTH_TEXTURE_FORMAT _packed) { return _packed; }
#endif
#ifndef FFX_SSSR_SCENE_TEXTURE_FORMAT
    #define FFX_SSSR_SCENE_TEXTURE_FORMAT float4
#endif
#ifndef FFX_SSSR_SCENE_RADIANCE_UNPACK_FUNCTION
    #define FFX_SSSR_SCENE_RADIANCE_UNPACK_FUNCTION \
    float3 FfxSssrUnpackSceneRadiance(FFX_SSSR_SCENE_TEXTURE_FORMAT _packed) { return _packed.xyz; }
#endif
#ifndef FFX_SSSR_MOTION_VECTOR_TEXTURE_FORMAT
    #define FFX_SSSR_MOTION_VECTOR_TEXTURE_FORMAT float2
#endif
#ifndef FFX_SSSR_MOTION_VECTOR_UNPACK_FUNCTION
    #define FFX_SSSR_MOTION_VECTOR_UNPACK_FUNCTION \
    float2 FfxSssrUnpackMotionVectors(FFX_SSSR_MOTION_VECTOR_TEXTURE_FORMAT _packed) { return _packed.xy * float2(0.5, -0.5); }
#endif
#ifndef FFX_SSSR_EAW_STRIDE
    #define FFX_SSSR_EAW_STRIDE 2
#endif

#define FFX_SSSR_PI                                  3.14159265358979f
#define FFX_SSSR_GOLDEN_RATIO                        1.61803398875f

#define FFX_SSSR_FLOAT_MAX                           3.402823466e+38

#define FFX_SSSR_FALSE                               0
#define FFX_SSSR_TRUE                                1

#define FFX_SSSR_USE_ROUGHNESS_OVERRIDE              FFX_SSSR_FALSE
#define FFX_SSSR_ROUGHNESS_OVERRIDE                  0.1

#define FFX_SSSR_TEMPORAL_VARIANCE_THRESHOLD         0.0005

#if FFX_SSSR_USE_ROUGHNESS_OVERRIDE
float FfxSssrUnpackRoughness(FFX_SSSR_ROUGHNESS_TEXTURE_FORMAT _packed) { return FFX_SSSR_ROUGHNESS_OVERRIDE; }
#else
FFX_SSSR_ROUGHNESS_UNPACK_FUNCTION
#endif

FFX_SSSR_NORMALS_UNPACK_FUNCTION
FFX_SSSR_MOTION_VECTOR_UNPACK_FUNCTION
FFX_SSSR_DEPTH_UNPACK_FUNCTION
FFX_SSSR_SCENE_RADIANCE_UNPACK_FUNCTION

// Common constants
CBUFFER(Constants, UPDATE_FREQ_NONE, b0, binding = 0)
{
    DATA(float4x4, g_inv_view_proj[VR_MULTIVIEW_COUNT], None);
    DATA(float4x4, g_proj[VR_MULTIVIEW_COUNT], None);
    DATA(float4x4, g_inv_proj[VR_MULTIVIEW_COUNT], None);
    DATA(float4x4, g_view, None);
    DATA(float4x4, g_inv_view, None);
    DATA(float4x4, g_prev_view_proj[VR_MULTIVIEW_COUNT], None);

    DATA(uint, g_frame_index, None);
    DATA(uint, g_max_traversal_intersections, None);
    DATA(uint, g_min_traversal_occupancy, None);
    DATA(uint, g_most_detailed_mip, None);
    DATA(float, g_temporal_stability_factor, None);
    DATA(float, g_depth_buffer_thickness, None);
    DATA(uint, g_samples_per_quad, None);
    DATA(uint, g_temporal_variance_guided_tracing_enabled, None);
    DATA(float, g_roughness_threshold, None);
    DATA(uint, g_skip_denoiser, None);
};

DECLARE_RESOURCES()

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

STRUCT(Ray)
{
    DATA(float3, origin, None);
    DATA(float3, direction, None);
};

// Create a ray that originates at the depth buffer surface and points away from the camera.
Ray CreateViewSpaceRay(float3 screen_space_pos, uint ViewID)
{
    float3 view_space_pos = InvProjectPosition(screen_space_pos, Get(g_inv_proj)[ViewID]);
    Ray view_space_ray;
    view_space_ray.origin = view_space_pos;
    view_space_ray.direction = view_space_pos;
    return view_space_ray;
}

// TODO FSL: these textures are actually typed by configurable macros
float3 LoadNormal(int3 index, Tex2DArray(float4) tex)
{
    return FfxSssrUnpackNormals(LoadTex3D(tex, NO_SAMPLER, index, 0));
}

float LoadRoughness(int3 index, Tex2DArray(float4) tex)
{
    return FfxSssrUnpackRoughness(LoadTex3D(tex, NO_SAMPLER, index, 0));
}

bool IsGlossy(float roughness)
{
    return roughness < Get(g_roughness_threshold);
}

bool IsMirrorReflection(float roughness)
{
    return roughness < 0.0001;
}

float GetEdgeStoppingNormalWeight(float3 normal_p, float3 normal_q, float sigma)
{
    return pow(fast_max(dot(normal_p, normal_q), 0.0), sigma);
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
    float a = abs(x - m) / sigma;
    a *= a;
    return exp((-0.5) * a);
}

float Luminance(float3 clr)
{
    return fast_max(dot(clr, float3(0.299, 0.587, 0.114)), 0.00001);
}

uint Pack(uint3 coord)
{
    return (coord.x & 0xFFFf) | (coord.y & 0x7FFF) << 16 | (coord.z & 0x1) << 31;
}

uint3 Unpack(uint _packed)
{
    return uint3(_packed & 0xFFFF, _packed >> 16 & 0x7FFF, _packed >> 31);
}

bool IsBaseRay(uint2 did, uint samples_per_quad)
{
    switch (samples_per_quad)
    {
    case 1u:
        return ((did.x & 1u) | (did.y & 1u)) == 0u; // Deactivates 3 out of 4 rays
    case 2u:
        return (did.x & 1u) == (did.y & 1u); // Deactivates 2 out of 4 rays. Keeps diagonal.
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
    case 1u:
        return lane & ~3u; // Map to upper left
    case 2u:
        return lane ^ 1u; // Toggle horizontal
    default: // case 4:
        return lane; // Identity
    }
}

#ifdef VULKAN

uint PackFloat16(half2 v)
{ return packHalf2x16(v); }
half2 UnpackFloat16(uint a)
{ return unpackHalf2x16(a); }

#elif defined(METAL)

uint PackFloat16(half2 v)
{
    return as_type<uint>(v);
}
half2 UnpackFloat16(uint a)
{
    return as_type<half2>(a);
}

#else // D3D12

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

#endif


// From ffx_a.h

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

uint2 RemapLane8x8(uint lane)// ARmpRed8x8
{
    return uint2(insert_bits(extract_bits(lane, 2, 3), lane, 0, 1), insert_bits(extract_bits(lane, 3, 3), extract_bits(lane, 1, 2), 0, 2));
}

#endif // FFX_SSSR_COMMON