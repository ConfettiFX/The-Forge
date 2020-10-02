#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "SSSR_Common.h"

void LoadFromGroupSharedMemory(int2 idx, thread float3& radiance, thread float& roughness, threadgroup uint (&g_shared_0)[12][12], threadgroup uint (&g_shared_1)[12][12])
{
    uint2 tmp;
    tmp.x = g_shared_0[idx.x][idx.y];
    tmp.y = g_shared_1[idx.x][idx.y];

    float4 min16tmp = float4(float2(UnpackFloat16(tmp.x)), float2(UnpackFloat16(tmp.y)));
    radiance = min16tmp.xyz;
    roughness = min16tmp.w;
}

void StoreInGroupSharedMemory(int2 idx, float3 radiance, float roughness, threadgroup uint (&g_shared_0)[12][12], threadgroup uint (&g_shared_1)[12][12])
{
    float4 tmp = float4(radiance, roughness);
    g_shared_0[idx.x][idx.y] = PackFloat16(half2(tmp.xy));
    g_shared_1[idx.x][idx.y] = PackFloat16(half2(tmp.zw));
}

float3 LoadRadiance(int2 idx, thread texture2d<float> g_temporally_denoised_reflections)
{
    return g_temporally_denoised_reflections.read(uint2(idx)).xyz;
}

float LoadRoughnessValue(int2 idx, thread texture2d<float> g_roughness)
{
    return FfxSssrUnpackRoughness(g_roughness.read(uint2(idx), 0));
}

float4 ResolveScreenspaceReflections(int2 gtid, float center_roughness, threadgroup uint (&g_shared_0)[12][12], threadgroup uint (&g_shared_1)[12][12])
{
    float3 sum = float3(0.0);
    float total_weight = 0.0;

    for (int dy = -2; dy <= 2; dy++)
    {
        for (int dx = -2; dx <= 2; dx++)
        {
            int2 texel_coords = gtid + int2(dx, dy);

            float3 radiance;
            float roughness;
            LoadFromGroupSharedMemory(texel_coords, radiance, roughness, g_shared_0, g_shared_1);

            float weight = GetEdgeStoppingRoughnessWeightFP16(center_roughness, roughness, 0.001, 0.01);
            sum += radiance * weight;
            total_weight += weight;
        }
    }
    sum /= fast::max(total_weight, 0.0001);
    return float4(sum, 1.0);
}

void LoadWithOffset(int2 did, int2 offset, thread float3& radiance, thread float& roughness, thread texture2d<float> g_temporally_denoised_reflections, thread texture2d<float> g_roughness)
{
    did += offset;
    radiance = LoadRadiance(did, g_temporally_denoised_reflections);
    roughness = LoadRoughnessValue(did, g_roughness);
}

void StoreWithOffset(int2 gtid, int2 offset, float3 radiance, float roughness, threadgroup uint (&g_shared_0)[12][12], threadgroup uint (&g_shared_1)[12][12])
{
    gtid += offset;
    StoreInGroupSharedMemory(gtid, radiance, roughness, g_shared_0, g_shared_1);
}

void InitializeGroupSharedMemory(int2 did, int2 gtid, threadgroup uint (&g_shared_0)[12][12], threadgroup uint (&g_shared_1)[12][12], thread texture2d<float> g_temporally_denoised_reflections, thread texture2d<float> g_roughness)
{
    int2 offset_0 = int2(0);
    if (gtid.x < 4)
    {
        offset_0 = int2(8, 0);
    }
    else if (gtid.y >= 4)
    {
        offset_0 = int2(4);
    }
    else
    {
        offset_0 = -gtid;
    }

    int2 offset_1 = int2(0);
    if (gtid.y < 4)
    {
        offset_1 = int2(0, 8);
    }
    else
    {
        offset_1 = -gtid;
    }

    float3 radiance_0;
    float roughness_0;

    float3 radiance_1;
    float roughness_1;

    float3 radiance_2;
    float roughness_2;

    did -= int2(2);
    LoadWithOffset(did, int2(0), radiance_0, roughness_0, g_temporally_denoised_reflections, g_roughness);
    LoadWithOffset(did, offset_0, radiance_1, roughness_1, g_temporally_denoised_reflections, g_roughness);
    LoadWithOffset(did, offset_1, radiance_2, roughness_2, g_temporally_denoised_reflections, g_roughness);

    StoreWithOffset(gtid, int2(0), radiance_0, roughness_0, g_shared_0, g_shared_1);
    if ((gtid.x < 4) || (gtid.y >= 4))
    {
        StoreWithOffset(gtid, offset_0, radiance_1, roughness_1, g_shared_0, g_shared_1);
    }
    if (gtid.y < 4)
    {
        StoreWithOffset(gtid, offset_1, radiance_2, roughness_2, g_shared_0, g_shared_1);
    }
}

void Resolve(int2 did, int2 gtid, constant CSConstants& Constants, threadgroup uint (&g_shared_0)[12][12], threadgroup uint (&g_shared_1)[12][12], thread texture2d<float> g_temporally_denoised_reflections, thread texture2d<float> g_roughness, thread texture2d<float, access::write> g_denoised_reflections)
{
    InitializeGroupSharedMemory(did, gtid, g_shared_0, g_shared_1, g_temporally_denoised_reflections, g_roughness);
    threadgroup_barrier(mem_flags::mem_threadgroup);

    gtid += int2(2);

    float3 center_radiance;
    float center_roughness;
    LoadFromGroupSharedMemory(gtid, center_radiance, center_roughness, g_shared_0, g_shared_1);

    if ((!IsGlossy(center_roughness, Constants)) || IsMirrorReflection(center_roughness))
    {
        return;
    }

    g_denoised_reflections.write(ResolveScreenspaceReflections(gtid, center_roughness, g_shared_0, g_shared_1), uint2(did));
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
    threadgroup uint g_shared_0[12][12];
    threadgroup uint g_shared_1[12][12];

    uint packed_base_coords = csData.g_tile_list[gl_WorkGroupID.x];
    uint2 base_coords = Unpack(packed_base_coords);
    uint2 coords = base_coords + gl_LocalInvocationID.xy;
    Resolve(int2(coords), int2(gl_LocalInvocationID.xy), csData.Constants, g_shared_0, g_shared_1, csData.g_temporally_denoised_reflections, csData.g_roughness, csData.g_denoised_reflections);
}