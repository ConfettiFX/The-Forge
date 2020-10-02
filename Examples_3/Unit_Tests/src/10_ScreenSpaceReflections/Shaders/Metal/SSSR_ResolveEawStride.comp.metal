#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "SSSR_Common.h"

float3 LoadRadiance(int2 idx, thread texture2d<float> g_temporally_denoised_reflections)
{
    return g_temporally_denoised_reflections.read(uint2(idx)).xyz;
}

float LoadRoughnessValue(int2 idx, thread texture2d<float> g_roughness)
{
    return FfxSssrUnpackRoughness(g_roughness.read(uint2(idx), 0));
}

float GetRoughnessRadiusWeight(float roughness_p, float roughness_q, float dist)
{
    return 1.0 - smoothstep(10.0 * roughness_p, 500.0 * roughness_p, dist);
}

float4 ResolveScreenspaceReflections(int2 did, float center_roughness, thread texture2d<float> g_temporally_denoised_reflections, thread texture2d<float> g_roughness)
{
    float3 sum = float3(0.0);
    float total_weight = 0.0;

    for (int dy = -2; dy <= 2; dy++)
    {
        for (int dx = -2; dx <= 2; dx++)
        {
            int2 texel_coords = did + FFX_SSSR_EAW_STRIDE * int2(dx, dy);

            float3 radiance = LoadRadiance(texel_coords, g_temporally_denoised_reflections);
            float roughness = LoadRoughnessValue(texel_coords, g_roughness);

            float weight = GetEdgeStoppingRoughnessWeightFP16(center_roughness, roughness, 0.001, 0.01)
                * GetRoughnessRadiusWeight(center_roughness, roughness, length(float2(texel_coords - did)));
            sum += radiance * weight;
            total_weight += weight;
        }
    }
    sum /= fast::max(total_weight, 0.0001);
    return float4(sum, 1.0);
}

void Resolve(int2 did, constant CSConstants& Constants, thread texture2d<float> g_temporally_denoised_reflections, thread texture2d<float> g_roughness, thread texture2d<float, access::write> g_denoised_reflections)
{
    float3 center_radiance = LoadRadiance(did, g_temporally_denoised_reflections);
    float center_roughness = LoadRoughnessValue(did, g_roughness);
    if ((!IsGlossy(center_roughness, Constants)) || IsMirrorReflection(center_roughness))
    {
        return;
    }
    g_denoised_reflections.write(ResolveScreenspaceReflections(did, center_roughness, g_temporally_denoised_reflections, g_roughness), uint2(did));
}

struct CSData
{
    constant CSConstants& Constants [[id(0)]];
    texture2d<float> g_temporally_denoised_reflections [[id(1)]];
    texture2d<float> g_roughness [[id(2)]];
    texture2d<float, access::write> g_denoised_reflections [[id(3)]];
    device uint* g_tile_list [[id(4)]];
};

//[numthreads(8, 8, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]],
uint3 gl_WorkGroupID [[threadgroup_position_in_grid]]
)
{
    uint packed_base_coords = csData.g_tile_list[gl_WorkGroupID.x];
    uint2 base_coords = Unpack(packed_base_coords);
    uint2 coords = base_coords + gl_LocalInvocationID.xy;
    Resolve(int2(coords), csData.Constants, csData.g_temporally_denoised_reflections, csData.g_roughness, csData.g_denoised_reflections);
}