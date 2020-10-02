inline uint4 spvSubgroupBallot(bool value)
{
    simd_vote vote = simd_ballot(value);
    // simd_ballot() returns a 64-bit integer-like object, but
    // SPIR-V callers expect a uint4. We must convert.
    // FIXME: This won't include higher bits if Apple ever supports
    // 128 lanes in an SIMD-group.
    return uint4((uint)((simd_vote::vote_t)vote & 0xFFFFFFFF), (uint)(((simd_vote::vote_t)vote >> 32) & 0xFFFFFFFF), 0, 0);
}

inline uint spvSubgroupBallotBitCount(uint4 ballot)
{
    return popcount(ballot.x) + popcount(ballot.y) + popcount(ballot.z) + popcount(ballot.w);
}

inline uint spvSubgroupBallotInclusiveBitCount(uint4 ballot, uint gl_SubgroupInvocationID)
{
    uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupInvocationID + 1, 32u)), extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupInvocationID + 1 - 32, 0)), uint2(0));
    return spvSubgroupBallotBitCount(ballot & mask);
}

inline uint spvSubgroupBallotExclusiveBitCount(uint4 ballot, uint gl_SubgroupInvocationID)
{
    uint4 mask = uint4(extract_bits(0xFFFFFFFF, 0, min(gl_SubgroupInvocationID, 32u)), extract_bits(0xFFFFFFFF, 0, (uint)max((int)gl_SubgroupInvocationID - 32, 0)), uint2(0));
    return spvSubgroupBallotBitCount(ballot & mask);
}

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

struct CSConstants
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

float3 ProjectPosition(thread const float3& origin, thread const float4x4& mat)
{
    float4 projected = mat * float4(origin, 1.0);
    projected.xyz = projected.xyz / float3(projected.w);
    projected.xy = projected.xy * 0.5 + float2(0.5);
    projected.y = 1.0 - projected.y;
    return projected.xyz;
}

float3 InvProjectPosition(float3 origin, thread const float4x4& mat)
{
    origin.y = 1.0 - origin.y;
    origin.xy = origin.xy * 2.0 - float2(1.0);
    float4 projected = mat * float4(origin, 1.0);
    projected.xyz = projected.xyz / float3(projected.w);
    return projected.xyz;
}

float3 ProjectDirection(thread const float3& origin, thread const float3& direction, thread const float3& screen_space_origin, thread const float4x4& mat)
{
    float3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

struct Ray
{
    float3 origin;
    float3 direction;
};

Ray CreateViewSpaceRay(thread const float3& screen_space_pos, constant CSConstants& Constants)
{
    float3 view_space_pos = InvProjectPosition(screen_space_pos, transpose(Constants.g_inv_proj));
    Ray view_space_ray;
    view_space_ray.origin = view_space_pos;
    view_space_ray.direction = view_space_pos;
    return view_space_ray;
}

float3 LoadNormal(thread const int2& index, thread const texture2d<float>& tex)
{
    return FfxSssrUnpackNormals(tex.read(uint2(index), 0));
}

float LoadRoughness(thread const int2& index, thread const texture2d<float>& tex)
{
    return FfxSssrUnpackRoughness(tex.read(uint2(index), 0));
}

bool IsGlossy(thread const float& roughness, constant CSConstants& Constants)
{
    return roughness < Constants.g_roughness_threshold;
}

bool IsMirrorReflection(thread const float& roughness)
{
    return roughness < 0.0001;
}

float GetEdgeStoppingNormalWeight(thread const float3& normal_p, thread const float3& normal_q, thread const float& sigma)
{
    return pow(fast::max(dot(normal_p, normal_q), 0.0), sigma);
}

float GetEdgeStoppingRoughnessWeight(thread const float& roughness_p, thread const float& roughness_q, thread const float& sigma_min, thread const float& sigma_max)
{
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

half GetEdgeStoppingRoughnessWeightFP16(thread const half& roughness_p, thread const half& roughness_q, thread const half& sigma_min, thread const half& sigma_max)
{
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

float GetRoughnessAccumulationWeight(thread const float& roughness)
{
    float near_singular_roughness = 0.00001;
    return smoothstep(0.0, near_singular_roughness, roughness);
}

float Gaussian(thread const float& x, thread const float& m, thread const float& sigma)
{
    float a = abs(x - m) / sigma;
    a *= a;
    return exp((-0.5) * a);
}

float Luminance(thread const float3& clr)
{
    return fast::max(dot(clr, float3(0.299, 0.587, 0.114)), 0.00001);
}

uint Pack(thread const uint2& coord)
{
    return (coord.x & 0xFFFF) | (coord.y & 0xFFFF) << 16;
}

uint2 Unpack(thread const uint& _packed)
{
    return uint2(_packed & 0xFFFF, _packed >> 16);
}

bool IsBaseRay(thread const uint2& did, uint samples_per_quad)
{
    switch (samples_per_quad)
    {
        case 1u:
        {
            return ((did.x & 1u) | (did.y & 1u)) == 0u;
        }
        case 2u:
        {
            return (did.x & 1u) == (did.y & 1u);
        }
        default:
        {
            return true;
        }
    }
}

uint GetBaseLane(thread const uint& lane, thread const uint& samples_per_quad)
{
    switch (samples_per_quad)
    {
        case 1u:
        {
            return lane & ~3u;
        }
        case 2u:
        {
            return lane ^ 1u;
        }
        default:
        {
            return lane;
        }
    }
}

uint PackFloat16(half2 v)
{
    return as_type<uint>(v);
}

half2 UnpackFloat16(uint a)
{
    return as_type<half2>(a);
}

uint2 RemapLane8x8(thread const uint& lane)
{
    return uint2(insert_bits(extract_bits(lane, 2, 3), lane, 0, 1), insert_bits(extract_bits(lane, 3, 3), extract_bits(lane, 1, 2), 0, 2));
}