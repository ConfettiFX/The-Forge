#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "SSSR_Common.h"

half4 LoadRadianceFromGroupSharedMemory(int2 idx, threadgroup uint (&g_shared_0)[17][17], threadgroup uint (&g_shared_1)[17][17])
{
    uint2 tmp;
    tmp.x = g_shared_0[idx.x][idx.y];
    tmp.y = g_shared_1[idx.x][idx.y];
    return half4(UnpackFloat16(tmp.x), UnpackFloat16(tmp.y));
}

half3 LoadNormalFromGroupSharedMemory(int2 idx, threadgroup uint (&g_shared_2)[17][17], threadgroup uint (&g_shared_3)[17][17])
{
    uint2 tmp;
    tmp.x = g_shared_2[idx.x][idx.y];
    tmp.y = g_shared_3[idx.x][idx.y];
    return half4(UnpackFloat16(tmp.x), UnpackFloat16(tmp.y)).xyz;
}

float LoadDepthFromGroupSharedMemory(int2 idx, threadgroup float (&g_shared_depth)[17][17])
{
    return g_shared_depth[idx.x][idx.y];
}

void StoreInGroupSharedMemory(int2 idx, float4 radiance, float3 normal, float depth, threadgroup uint (&g_shared_0)[17][17], threadgroup uint (&g_shared_1)[17][17], threadgroup uint (&g_shared_2)[17][17], threadgroup uint (&g_shared_3)[17][17], threadgroup float (&g_shared_depth)[17][17])
{
    g_shared_0[idx.x][idx.y] = PackFloat16(half2(radiance.xy));
    g_shared_1[idx.x][idx.y] = PackFloat16(half2(radiance.zw));
    g_shared_2[idx.x][idx.y] = PackFloat16(half2(normal.xy));
    g_shared_3[idx.x][idx.y] = PackFloat16(half2(normal.z, 0.0));
    g_shared_depth[idx.x][idx.y] = depth;
}

float LoadRayLengthFP16(int2 idx, thread texture2d<float, access::read_write> g_ray_lengths)
{
    return g_ray_lengths.read(uint2(idx)).x;
}

float3 LoadRadianceFP16(int2 idx, thread texture2d<float> g_intersection_result)
{
    return g_intersection_result.read(uint2(idx)).xyz;
}

float3 LoadNormalFP16(int2 idx, thread texture2d<float> g_normal)
{
    return FfxSssrUnpackNormals(g_normal.read(uint2(idx), 0));
}

float LoadDepth(int2 idx, thread texture2d<float> g_depth_buffer)
{
    return FfxSssrUnpackDepth(g_depth_buffer.read(uint2(idx), 0).x);
}

bool LoadHasRay(int2 idx, thread texture2d<float> g_has_ray)
{
    return g_has_ray.read(uint2(idx)).x != 0.0;
}

void LoadWithOffset(int2 did, int2 offset, thread float& ray_length, thread float3& radiance, thread float3& normal, thread float& depth, thread bool& has_ray, thread texture2d<float, access::read_write> g_ray_lengths, thread texture2d<float> g_intersection_result, thread texture2d<float> g_normal, thread texture2d<float> g_depth_buffer, thread texture2d<float> g_has_ray)
{
    did += offset;
    ray_length = LoadRayLengthFP16(did, g_ray_lengths);
    radiance = LoadRadianceFP16(did, g_intersection_result);
    normal = LoadNormalFP16(did, g_normal);
    depth = LoadDepth(did, g_depth_buffer);
    has_ray = LoadHasRay(did, g_has_ray);
}

void StoreWithOffset(int2 gtid, int2 offset, float ray_length, float3 radiance, float3 normal, float depth, threadgroup uint (&g_shared_0)[17][17], threadgroup uint (&g_shared_1)[17][17], threadgroup uint (&g_shared_2)[17][17], threadgroup uint (&g_shared_3)[17][17], threadgroup float (&g_shared_depth)[17][17])
{
    gtid += offset;
    StoreInGroupSharedMemory(gtid, float4(radiance, ray_length), normal, depth, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth);
}

void InitializeGroupSharedMemory(int2 did, int2 gtid, constant CSConstants& Constants, threadgroup uint (&g_shared_0)[17][17], threadgroup uint (&g_shared_1)[17][17], threadgroup uint (&g_shared_2)[17][17], threadgroup uint (&g_shared_3)[17][17], threadgroup float (&g_shared_depth)[17][17], thread texture2d<float, access::read_write> g_ray_lengths, thread texture2d<float> g_intersection_result, thread texture2d<float> g_normal, thread texture2d<float> g_depth_buffer, thread texture2d<float> g_has_ray, uint lane_index)
{
    uint samples_per_quad = Constants.g_samples_per_quad;

    int2 offset_0 = int2(0);
    int2 offset_1 = int2(8, 0);
    int2 offset_2 = int2(0, 8);
    int2 offset_3 = int2(8);

    float ray_length_0;
    float3 radiance_0;
    float3 normal_0;
    float depth_0;
    bool has_ray_0;

    float ray_length_1;
    float3 radiance_1;
    float3 normal_1;
    float depth_1;
    bool has_ray_1;

    float ray_length_2;
    float3 radiance_2;
    float3 normal_2;
    float depth_2;
    bool has_ray_2;

    float ray_length_3;
    float3 radiance_3;
    float3 normal_3;
    float depth_3;
    bool has_ray_3;

    did -= int2(4);
    LoadWithOffset(did, offset_0, ray_length_0, radiance_0, normal_0, depth_0, has_ray_0, g_ray_lengths, g_intersection_result, g_normal, g_depth_buffer, g_has_ray);
    LoadWithOffset(did, offset_1, ray_length_1, radiance_1, normal_1, depth_1, has_ray_1, g_ray_lengths, g_intersection_result, g_normal, g_depth_buffer, g_has_ray);
    LoadWithOffset(did, offset_2, ray_length_2, radiance_2, normal_2, depth_2, has_ray_2, g_ray_lengths, g_intersection_result, g_normal, g_depth_buffer, g_has_ray);
    LoadWithOffset(did, offset_3, ray_length_3, radiance_3, normal_3, depth_3, has_ray_3, g_ray_lengths, g_intersection_result, g_normal, g_depth_buffer, g_has_ray);

    uint base_lane_index = GetBaseLane(lane_index, samples_per_quad);
    bool is_base_ray = base_lane_index == lane_index;

    uint lane_index_0 = (has_ray_0 || is_base_ray) ? lane_index : base_lane_index;
    uint lane_index_1 = (has_ray_1 || is_base_ray) ? lane_index : base_lane_index;
    uint lane_index_2 = (has_ray_2 || is_base_ray) ? lane_index : base_lane_index;
    uint lane_index_3 = (has_ray_3 || is_base_ray) ? lane_index : base_lane_index;

    radiance_0 = simd_shuffle(radiance_0, uint(lane_index_0));
    radiance_1 = simd_shuffle(radiance_1, uint(lane_index_1));
    radiance_2 = simd_shuffle(radiance_2, uint(lane_index_2));
    radiance_3 = simd_shuffle(radiance_3, uint(lane_index_3));

    ray_length_0 = simd_shuffle(ray_length_0, uint(lane_index_0));
    ray_length_1 = simd_shuffle(ray_length_1, uint(lane_index_1));
    ray_length_2 = simd_shuffle(ray_length_2, uint(lane_index_2));
    ray_length_3 = simd_shuffle(ray_length_3, uint(lane_index_3));

    StoreWithOffset(gtid, offset_0, ray_length_0, radiance_0, normal_0, depth_0, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth);
    StoreWithOffset(gtid, offset_1, ray_length_1, radiance_1, normal_1, depth_1, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth);
    StoreWithOffset(gtid, offset_2, ray_length_2, radiance_2, normal_2, depth_2, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth);
    StoreWithOffset(gtid, offset_3, ray_length_3, radiance_3, normal_3, depth_3, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth);
}

float3 ResolveScreenspaceReflections(int2 gtid, float3 center_radiance, float3 center_normal, float center_depth, threadgroup uint (&g_shared_0)[17][17], threadgroup uint (&g_shared_1)[17][17], threadgroup uint (&g_shared_2)[17][17], threadgroup uint (&g_shared_3)[17][17], threadgroup float (&g_shared_depth)[17][17])
{
    float3 accumulated_radiance = center_radiance;
    float accumulated_weight = 1.0;

    const int2 reuse_offsets[] = {
        int2(0, 1),
        int2(-2, 1),
        int2(2, -3),
        int2(-3, 0),
        int2(1, 2),
        int2(-1, -2),
        int2(3, 0),
        int2(-3, 3),
        int2(0, -3),
        int2(-1, -1),
        int2(2, 1),
        int2(-2, -2),
        int2(1, 0),
        int2(0, 2),
        int2(3, -1)
    };

    for (int i = 0; i < 15; i++)
    {
        int2 new_idx = gtid + reuse_offsets[i];
        half3 normal = LoadNormalFromGroupSharedMemory(new_idx, g_shared_2, g_shared_3);
        float depth = LoadDepthFromGroupSharedMemory(new_idx, g_shared_depth);
        half4 radiance = LoadRadianceFromGroupSharedMemory(new_idx, g_shared_0, g_shared_1);
        float weight = 1.0
            * GetEdgeStoppingNormalWeight(center_normal, float3(normal), 64.0)
            * Gaussian(center_depth, depth, 0.02);

        accumulated_weight += weight;
        accumulated_radiance += (float3(radiance.xyz) * weight);
    }

    accumulated_radiance /= float3(fast::max(accumulated_weight, 0.00001));
    return accumulated_radiance;
}

void Resolve(int2 did, int2 gtid, constant CSConstants& Constants, threadgroup uint (&g_shared_0)[17][17], threadgroup uint (&g_shared_1)[17][17], threadgroup uint (&g_shared_2)[17][17], threadgroup uint (&g_shared_3)[17][17], threadgroup float (&g_shared_depth)[17][17], thread texture2d<float, access::read_write> g_ray_lengths, thread texture2d<float> g_intersection_result, thread texture2d<float> g_normal, thread texture2d<float> g_depth_buffer, thread texture2d<float> g_has_ray, thread uint& gl_SubgroupInvocationID, thread texture2d<float> g_roughness, thread texture2d<float, access::write> g_spatially_denoised_reflections)
{
    float center_roughness = LoadRoughness(did, g_roughness);
    InitializeGroupSharedMemory(did, gtid, Constants, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth, g_ray_lengths, g_intersection_result, g_normal, g_depth_buffer, g_has_ray, gl_SubgroupInvocationID);
    threadgroup_barrier(mem_flags::mem_threadgroup);

    if (!IsGlossy(center_roughness, Constants) || IsMirrorReflection(center_roughness))
    {
        return;
    }

    gtid += int2(4);

    float4 center_radiance = float4(LoadRadianceFromGroupSharedMemory(gtid, g_shared_0, g_shared_1));
    float3 center_normal = float3(LoadNormalFromGroupSharedMemory(gtid, g_shared_2, g_shared_3));
    float center_depth = float(LoadDepthFromGroupSharedMemory(gtid, g_shared_depth));
    g_spatially_denoised_reflections.write(float4(ResolveScreenspaceReflections(gtid, center_radiance.xyz, center_normal, center_depth, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth), 1.0), uint2(did));
    g_ray_lengths.write(float4(center_radiance.w), uint2(did));
}

struct CSData
{
    constant CSConstants& Constants [[id(0)]];
    texture2d<float, access::read_write> g_ray_lengths [[id(1)]];
    texture2d<float> g_intersection_result [[id(2)]];
    texture2d<float> g_normal [[id(3)]];
    texture2d<float> g_depth_buffer [[id(4)]];
    texture2d<float> g_has_ray [[id(5)]];
    texture2d<float> g_roughness [[id(6)]];
    texture2d<float, access::write> g_spatially_denoised_reflections [[id(7)]];
    device uint* g_tile_list [[id(8)]];
};

//[numthreads(64, 1, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
uint gl_SubgroupInvocationID [[thread_index_in_simdgroup]],
uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]],
uint3 gl_WorkGroupID [[threadgroup_position_in_grid]])
{
    threadgroup uint g_shared_0[17][17];
    threadgroup uint g_shared_1[17][17];
    threadgroup uint g_shared_2[17][17];
    threadgroup uint g_shared_3[17][17];
    threadgroup float g_shared_depth[17][17];

    uint packed_base_coords = csData.g_tile_list[gl_WorkGroupID.x];
    uint2 base_coords = Unpack(packed_base_coords);
    uint2 group_thread_id_2d = RemapLane8x8(gl_LocalInvocationID.x);
    uint2 coords = base_coords + group_thread_id_2d;
    Resolve(int2(coords), int2(group_thread_id_2d), csData.Constants, g_shared_0, g_shared_1, g_shared_2, g_shared_3, g_shared_depth, csData.g_ray_lengths, csData.g_intersection_result, csData.g_normal, csData.g_depth_buffer, csData.g_has_ray, gl_SubgroupInvocationID, csData.g_roughness, csData.g_spatially_denoised_reflections);
}