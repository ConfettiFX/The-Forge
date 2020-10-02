#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "SSSR_Common.h"

float3 ClipAABB(float3 aabb_min, float3 aabb_max, float3 prev_sample)
{
    float3 aabb_center = (aabb_max + aabb_min) * 0.5;
    float3 extent_clip = (aabb_max - aabb_min) * 0.5 + 0.001;

    float3 color_vector = prev_sample - aabb_center;

    float3 color_vector_clip = color_vector / extent_clip;

    color_vector_clip = abs(color_vector_clip);
    float max_abs_unit = fast::max(fast::max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z);

    if (max_abs_unit > 1.0)
    {
        return aabb_center + color_vector / max_abs_unit;
    }
    else
    {
        return prev_sample;
    }
}

float3 EstimateStdDeviation(int2 did, thread const texture2d<float>& tex)
{
    float3 color_sum = 0.0;
    float3 color_sum_squared = 0.0;

    int radius = 1;
    float weight = (float(radius) * 2.0 + 1.0) * (float(radius) * 2.0 + 1.0);

    for (int dx = -radius; dx <= radius; dx++)
    {
        for (int dy = -radius; dy <= radius; dy++)
        {
            int2 texel_coords = did + int2(dx, dy);
            float3 value = tex.read(uint2(texel_coords)).xyz;
            color_sum += value;
            color_sum_squared += (value * value);
        }
    }

    float3 color_std = (color_sum_squared - color_sum * color_sum / weight) / (weight - 1.0);
    return sqrt(fast::max(color_std, float3(0.0)));
}

float3 SampleRadiance(int2 texel_coords, thread const texture2d<float>& tex)
{
    return tex.read(uint2(texel_coords)).xyz;
}

float2 GetSurfaceReprojection(int2 did, float2 uv, float2 motion_vector)
{
    float2 history_uv = uv - motion_vector;
    return history_uv;
}

float2 GetHitPositionReprojection(int2 did, thread const float2& uv, thread const float& reflected_ray_length, constant CSConstants& Constants, thread texture2d<float> g_depth_buffer)
{
    float z = FfxSssrUnpackDepth(g_depth_buffer.read(uint2(did), 0).x);
    float3 view_space_ray = CreateViewSpaceRay(float3(uv, z), Constants).direction;

    float surface_depth = length(view_space_ray);
    float ray_length = surface_depth + reflected_ray_length;

    view_space_ray /= surface_depth;
    view_space_ray *= ray_length;
    float3 world_hit_position = (float4(view_space_ray, 1.0) * Constants.g_inv_view).xyz;
    float3 prev_hit_position = ProjectPosition(world_hit_position, transpose(Constants.g_prev_view_proj));
    float2 history_uv = prev_hit_position.xy;
    return history_uv;
}

float SampleHistory(float2 uv, uint2 image_size, float3 normal, float roughness, float3 radiance_min, float3 radiance_max, thread float3& radiance, constant CSConstants& Constants, thread texture2d<float> g_temporally_denoised_reflections_history, thread texture2d<float> g_normal_history, thread texture2d<float> g_roughness_history)
{
    int2 texel_coords = int2(float2(image_size) * uv);
    radiance = SampleRadiance(texel_coords, g_temporally_denoised_reflections_history);
    radiance = ClipAABB(radiance_min, radiance_max, radiance);

    float3 history_normal = LoadNormal(texel_coords, g_normal_history);
    float history_roughness = LoadRoughness(texel_coords, g_roughness_history);

    const float normal_sigma = 8.0;
    const float roughness_sigma_min = 0.01;
    const float roughness_sigma_max = 0.1;
    const float main_accumulation_factor = 0.90 + 0.1 * Constants.g_temporal_stability_factor;

    float accumulation_speed = main_accumulation_factor
        * GetEdgeStoppingNormalWeight(normal, history_normal, normal_sigma)
        * GetEdgeStoppingRoughnessWeight(roughness, history_roughness, roughness_sigma_min, roughness_sigma_max)
        * GetRoughnessAccumulationWeight(roughness);

    return fast::clamp(accumulation_speed, 0.0, 1.0);
}

float ComputeTemporalVariance(float3 history_radiance, float3 radiance)
{
    float history_luminance = Luminance(history_radiance);
    float luminance = Luminance(radiance);
    return abs(history_luminance - luminance) / fast::max(fast::max(history_luminance, luminance), 0.00001);
}

float4 ResolveScreenspaceReflections(int2 did, float2 uv, uint2 image_size, float roughness, constant CSConstants& Constants, thread texture2d<float> g_depth_buffer, thread texture2d<float> g_temporally_denoised_reflections_history, thread texture2d<float> g_normal_history, thread texture2d<float> g_roughness_history, thread texture2d<float> g_normal, thread texture2d<float> g_spatially_denoised_reflections, thread texture2d<float> g_ray_lengths, thread texture2d<float> g_motion_vectors)
{
    float3 normal = LoadNormal(did, g_normal);
    float3 radiance = g_spatially_denoised_reflections.read(uint2(did)).xyz;
    float3 radiance_history = g_temporally_denoised_reflections_history.read(uint2(did)).xyz;
    float ray_length = g_ray_lengths.read(uint2(did)).x;

    float2 motion_vector = FfxSssrUnpackMotionVectors(g_motion_vectors.read(uint2(did), 0).xy);
    float3 color_std = EstimateStdDeviation(did, g_spatially_denoised_reflections);
    color_std *= (dot(motion_vector, motion_vector) == 0.0) ? 8.0 : 2.2;

    float3 radiance_min = radiance - color_std;
    float3 radiance_max = radiance + color_std;

    float2 surface_reprojection_uv = GetSurfaceReprojection(did, uv, motion_vector);

    float2 hit_reprojection_uv = GetHitPositionReprojection(did, uv, ray_length, Constants, g_depth_buffer);

    float2 reprojection_uv = (roughness < 0.05) ? hit_reprojection_uv : surface_reprojection_uv;

    float3 reprojection = float3(0.0);
    float weight = 0.0;
    if (all(reprojection_uv > float2(0.0)) && all(reprojection_uv < float2(1.0)))
    {
        weight = SampleHistory(reprojection_uv, image_size, normal, roughness, radiance_min, radiance_max, reprojection, Constants, g_temporally_denoised_reflections_history, g_normal_history, g_roughness_history);
    }

    radiance = mix(radiance, reprojection, float3(weight));
    float temporal_variance = ComputeTemporalVariance(radiance_history, radiance) > FFX_SSSR_TEMPORAL_VARIANCE_THRESHOLD ? 1.0 : 0.0;
    return float4(radiance, temporal_variance);
}

void Resolve(int2 did, constant CSConstants& Constants, thread texture2d<float> g_depth_buffer, thread texture2d<float> g_temporally_denoised_reflections_history, thread texture2d<float> g_normal_history, thread texture2d<float> g_roughness_history, thread texture2d<float> g_normal, thread texture2d<float> g_spatially_denoised_reflections, thread texture2d<float> g_ray_lengths, thread texture2d<float> g_motion_vectors, thread texture2d<float> g_roughness, thread texture2d<float, access::write> g_temporally_denoised_reflections, thread texture2d<float, access::write> g_temporal_variance)
{
    float roughness = LoadRoughness(did, g_roughness);
    if ((!IsGlossy(roughness, Constants)) || IsMirrorReflection(roughness))
    {
        return;
    }

    uint2 image_size = uint2(g_temporally_denoised_reflections.get_width(), g_temporally_denoised_reflections.get_height());
    float2 uv = float2(float(did.x) + 0.5, float(did.y) + 0.5) / float2(image_size);

    float4 resolve = ResolveScreenspaceReflections(did, uv, image_size, roughness, Constants, g_depth_buffer, g_temporally_denoised_reflections_history, g_normal_history, g_roughness_history, g_normal, g_spatially_denoised_reflections, g_ray_lengths, g_motion_vectors);
    g_temporally_denoised_reflections.write(float4(resolve.xyz, 1.0), uint2(did));
    g_temporal_variance.write(float4(resolve.w), uint2(did));
}
struct CSData
{
    constant CSConstants& Constants [[id(0)]];
    texture2d<float> g_depth_buffer [[id(1)]];
    texture2d<float> g_temporally_denoised_reflections_history [[id(2)]];
    texture2d<float> g_normal_history [[id(3)]];
    texture2d<float> g_roughness_history [[id(4)]];
    texture2d<float> g_normal [[id(5)]];
    texture2d<float> g_spatially_denoised_reflections [[id(6)]];
    texture2d<float> g_ray_lengths [[id(7)]];
    texture2d<float> g_motion_vectors [[id(8)]];
    texture2d<float> g_roughness [[id(9)]];
    texture2d<float, access::write> g_temporally_denoised_reflections [[id(10)]];
    texture2d<float, access::write> g_temporal_variance [[id(11)]];
    device uint* g_tile_list [[id(12)]];
};

//[numthreads(8, 8, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]],
uint3 gl_WorkGroupID [[threadgroup_position_in_grid]])
{
    uint packed_base_coords = csData.g_tile_list[gl_WorkGroupID.x];
    uint2 base_coords = Unpack(packed_base_coords);
    uint2 coords = base_coords + gl_LocalInvocationID.xy;
    Resolve(int2(coords), csData.Constants, csData.g_depth_buffer, csData.g_temporally_denoised_reflections_history, csData.g_normal_history, csData.g_roughness_history, csData.g_normal, csData.g_spatially_denoised_reflections, csData.g_ray_lengths, csData.g_motion_vectors, csData.g_roughness, csData.g_temporally_denoised_reflections, csData.g_temporal_variance);
}