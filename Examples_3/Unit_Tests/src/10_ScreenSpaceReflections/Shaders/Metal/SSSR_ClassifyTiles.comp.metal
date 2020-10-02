#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wunused-variable"

#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_atomic>

using namespace metal;

#include "SSSR_Common.h"

struct CSData
{
    constant CSConstants& Constants [[id(0)]];
    volatile device uint* g_tile_counter [[id(1)]];
    volatile device uint* g_ray_counter [[id(2)]];
    device uint* g_tile_list [[id(3)]];
    device uint* g_ray_list [[id(4)]];
    texture2d<float> g_roughness [[id(5)]];
    texture2d<float, access::read_write> g_temporal_variance [[id(6)]];
    texture2d<float, access::write> g_temporally_denoised_reflections [[id(7)]];
    texture2d<float, access::write> g_ray_lengths [[id(8)]];
    texture2d<float, access::write> g_denoised_reflections [[id(9)]];
};

//[numthreads(8, 8, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
uint3 gl_GlobalInvocationID [[thread_position_in_grid]],
uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]],
uint gl_SubgroupInvocationID [[thread_index_in_simdgroup]])
{
    threadgroup uint g_ray_count;
    threadgroup uint g_denoise_count;
    threadgroup uint g_ray_base_index;

    uint2 did = uint2(gl_GlobalInvocationID.xy);
    uint group_index = gl_LocalInvocationIndex;

    bool is_first_lane_of_wave = simd_is_first();
    bool is_first_lane_of_threadgroup = group_index == 0u;

    uint2 screen_size = uint2(csData.g_roughness.get_width(), csData.g_roughness.get_height());

    bool needs_ray = !(did.x >= screen_size.x || did.y >= screen_size.y);

    float roughness = FfxSssrUnpackRoughness(csData.g_roughness.read(did, 0));
    needs_ray = needs_ray && IsGlossy(roughness, csData.Constants);

    bool needs_denoiser = needs_ray && !IsMirrorReflection(roughness);

    bool is_base_ray = IsBaseRay(did, csData.Constants.g_samples_per_quad);
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray);

    if (csData.Constants.g_temporal_variance_guided_tracing_enabled != 0u && needs_denoiser && !needs_ray)
    {
        float temporal_variance = csData.g_temporal_variance.read(did).x;
        bool has_temporal_variance = temporal_variance != 0.0;

        needs_ray = needs_ray || has_temporal_variance;
    }

    if (is_first_lane_of_threadgroup)
    {
        g_ray_count = 0u;
        g_denoise_count = 0u;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    uint local_ray_index_in_wave = spvSubgroupBallotInclusiveBitCount(spvSubgroupBallot(needs_ray), gl_SubgroupInvocationID);
    uint wave_ray_count = spvSubgroupBallotBitCount(spvSubgroupBallot(needs_ray));
    bool wave_needs_denoiser = simd_any(needs_denoiser);
    uint wave_count = wave_needs_denoiser ? 1 : 0;

    uint local_ray_index_of_wave;
    if (is_first_lane_of_wave)
    {
        local_ray_index_of_wave = atomic_fetch_add_explicit((threadgroup atomic_uint*)&g_ray_count, wave_ray_count, memory_order_relaxed);
        atomic_fetch_add_explicit((threadgroup atomic_uint*)&g_denoise_count, wave_count, memory_order_relaxed);
    }
    local_ray_index_of_wave = simd_broadcast_first(local_ray_index_of_wave);

    threadgroup_barrier(mem_flags::mem_threadgroup);

    if (is_first_lane_of_threadgroup)
    {
        bool must_denoise = g_denoise_count > 0u;
        uint denoise_count = must_denoise ? 1 : 0;
        uint ray_count = g_ray_count;

        uint tile_index = atomic_fetch_add_explicit((volatile device atomic_uint*)&csData.g_tile_counter[0], denoise_count, memory_order_relaxed);
        uint ray_base_index = atomic_fetch_add_explicit((volatile device atomic_uint*)&csData.g_ray_counter[0], ray_count, memory_order_relaxed);

        int cleaned_index = must_denoise ? tile_index : -1;
        if (must_denoise)
        {
            csData.g_tile_list[cleaned_index] = Pack(did);
        }
        g_ray_base_index = ray_base_index;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);

    int2 target = needs_ray ? int2(-1, -1) : int2(did);
    int ray_index = needs_ray ? ((g_ray_base_index + local_ray_index_of_wave) + local_ray_index_in_wave) : -1;

    if (needs_ray)
    {
        csData.g_ray_list[ray_index] = Pack(did);
        csData.g_temporally_denoised_reflections.write(float4(0.0), uint2(target));
        csData.g_ray_lengths.write(float4(0.0), uint2(target));
    }
    csData.g_denoised_reflections.write(float4(0.0), did);
    csData.g_temporal_variance.write(float4(needs_ray ? (1.0 - csData.Constants.g_skip_denoiser) : 0.0), did);
}