#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct CSData
{
    device uint* g_tile_counter [[id(0)]];
    device uint* g_ray_counter [[id(1)]];
    device uint* g_intersect_args [[id(2)]];
    device uint* g_denoiser_args [[id(3)]];
};

//[numthreads(1, 1, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]]
)
{
    uint tile_counter = csData.g_tile_counter[0];
    uint ray_counter = csData.g_ray_counter[0];

    csData.g_tile_counter[0] = 0u;
    csData.g_ray_counter[0] = 0u;

    csData.g_intersect_args[0] = (ray_counter + 63u) / 64u;
    csData.g_intersect_args[1] = 1u;
    csData.g_intersect_args[2] = 1u;

    csData.g_denoiser_args[0] = tile_counter;
    csData.g_denoiser_args[1] = 1u;
    csData.g_denoiser_args[2] = 1u;
}

