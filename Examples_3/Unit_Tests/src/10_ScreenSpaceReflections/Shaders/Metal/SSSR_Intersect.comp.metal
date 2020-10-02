#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "SSSR_Common.h"

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension, device uint* g_ranking_tile_buffer, device uint* g_sobol_buffer, device uint* g_scrambling_tile_buffer)
{
    pixel_i &= 127u;
    pixel_j &= 127u;
    sample_index &= 255u;
    sample_dimension &= 255u;

    uint ranked_sample_index = sample_index ^ g_ranking_tile_buffer[sample_dimension + (pixel_i + pixel_j * 128u) * 8u];

    uint value = g_sobol_buffer[sample_dimension + (ranked_sample_index * 256u)];

    value ^= g_scrambling_tile_buffer[(sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u];

    return (float(value) + 0.5) / 256.0;
}

float2 SampleRandomVector2(uint2 pixel, constant CSConstants& Constants, device uint* g_ranking_tile_buffer, device uint* g_sobol_buffer, device uint* g_scrambling_tile_buffer)
{
    float2 u = float2(
        mod(SampleRandomNumber(pixel.x, pixel.y, 0u, 0u, g_ranking_tile_buffer, g_sobol_buffer, g_scrambling_tile_buffer) + (float(Constants.g_frame_index & 255u) * FFX_SSSR_GOLDEN_RATIO), 1.0),
        mod(SampleRandomNumber(pixel.x, pixel.y, 0u, 1u, g_ranking_tile_buffer, g_sobol_buffer, g_scrambling_tile_buffer) + (float(Constants.g_frame_index & 255u) * FFX_SSSR_GOLDEN_RATIO), 1.0));
    return u;
}

float3 sampleGGXVNDF(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    float3 Vh = normalize(float3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));

    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;

    float3 T1 = lensq > 0.0 ? float3(-Vh.y, Vh.x, 0.0) * rsqrt(lensq) : float3(1.0, 0.0, 0.0);
    float3 T2 = cross(Vh, T1);

    float r = sqrt(U1);
    float phi = 2.0 * FFX_SSSR_PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    float3 Nh = T1 * t1 + T2 * t2 + Vh * sqrt(fast::max(0.0, 1.0 - t1 * t1 - t2 * t2));

    float3 Ne = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, fast::max(0.0, Nh.z)));
    return Ne;
}

float3 Sample_GGX_VNDF_Ellipsoid(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    return sampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2);
}

float3 Sample_GGX_VNDF_Hemisphere(float3 Ve, float alpha, float U1, float U2)
{
    return Sample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
}

float3x3 CreateTBN(float3 N)
{
    float3 U;
    if (abs(N.z) > 0.0)
    {
        float k = sqrt(N.y * N.y + N.z * N.z);
        U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
    }
    else
    {
        float k_1 = sqrt(N.x * N.x + N.y * N.y);
        U.x = N.y / k_1; U.y = -N.x / k_1; U.z = 0.0;
    }

    float3x3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return transpose(TBN);
}

float3 SampleReflectionVector(float3 view_direction, float3 normal, float roughness, int2 did, constant CSConstants& Constants, device uint* g_ranking_tile_buffer, device uint* g_sobol_buffer, device uint* g_scrambling_tile_buffer)
{
    float3x3 tbn_transform = CreateTBN(normal);
    float3 view_direction_tbn = tbn_transform * (-view_direction);

    float2 u = SampleRandomVector2(uint2(did), Constants, g_ranking_tile_buffer, g_sobol_buffer, g_scrambling_tile_buffer);

    float3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);

    float3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    float3x3 inv_tbn_transform = transpose(tbn_transform);
    return inv_tbn_transform * reflected_direction_tbn;
}

float2 GetMipResolution(thread const float2& screen_dimensions, thread const int& mip_level)
{
    return screen_dimensions * pow(0.5, float(mip_level));
}

float LoadDepth(float2 idx, int mip, thread texture2d<float> g_depth_buffer_hierarchy)
{
    return FfxSssrUnpackDepth(g_depth_buffer_hierarchy.read(uint2(idx), mip).x);
}

void InitialAdvanceRay(float3 origin, float3 direction, float3 inv_direction, float2 current_mip_resolution, float2 current_mip_resolution_inv, float2 floor_offset, float2 uv_offset, thread float3& position, thread float& current_t)
{
    float2 current_mip_position = current_mip_resolution * origin.xy;

    float2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;

    float2 t = (xy_plane - origin.xy) * inv_direction.xy;
    current_t = fast::min(t.x, t.y);
    position = origin + direction * current_t;
}

bool AdvanceRay(float3 origin, float3 direction, float3 inv_direction, float2 current_mip_position, float2 current_mip_resolution_inv, float2 floor_offset, float2 uv_offset, float surface_z, thread float3& position, thread float& current_t)
{
    float2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;
    float3 boundary_planes = float3(xy_plane, surface_z);

    float3 t = (boundary_planes - origin) * inv_direction;

    t.z = (direction.z > 0.0) ? t.z : FFX_SSSR_FLOAT_MAX;

    float t_min = fast::min(fast::min(t.x, t.y), t.z);

    bool above_surface = surface_z > position.z;

    bool skipped_tile = (isunordered(t_min, t.z) || t_min != t.z) && above_surface;

    current_t = above_surface ? t_min : current_t;

    position = origin + direction * current_t;

    return skipped_tile;
}

float3 HierarchicalRaymarch(float3 origin, float3 direction, bool is_mirror, float2 screen_size, thread bool& valid_hit, constant CSConstants& Constant, thread texture2d<float> g_depth_buffer_hierarchy)
{
    int most_detailed_mip = is_mirror ? 0 : Constant.g_most_detailed_mip;

    float3 inv_direction = select(float3(FFX_SSSR_FLOAT_MAX), float3(1.0) / direction, direction != float3(0.0));

    int current_mip = most_detailed_mip;

    float2 current_mip_resolution = GetMipResolution(screen_size, current_mip);
    float2 current_mip_resolution_inv = float2(1.0) / current_mip_resolution;

    float2 uv_offset = float2(0.005 * exp2(float(most_detailed_mip))) / screen_size;
    uv_offset = select(uv_offset, -uv_offset, direction.xy < float2(0.0));

    float2 floor_offset = select(float2(1.0), float2(0.0), direction.xy < float2(0.0));

    float3 position;
    float current_t;
    InitialAdvanceRay(origin, direction, inv_direction, current_mip_resolution, current_mip_resolution_inv, floor_offset, uv_offset, position, current_t);

    uint min_traversal_occupancy = Constant.g_min_traversal_occupancy;
    uint max_traversal_intersections = Constant.g_max_traversal_intersections;

    bool exit_due_to_low_occupancy = false;
    int i = 0;
    while (uint(i) < max_traversal_intersections && current_mip >= most_detailed_mip && !exit_due_to_low_occupancy)
    {
        float2 current_mip_position = current_mip_resolution * position.xy;
        float surface_z = LoadDepth(current_mip_position, current_mip, g_depth_buffer_hierarchy);
        bool skipped_tile = AdvanceRay(origin, direction, inv_direction, current_mip_position, current_mip_resolution_inv, floor_offset, uv_offset, surface_z, position, current_t);
        current_mip += skipped_tile ? 1 : -1;
        current_mip_resolution *= skipped_tile ? 0.5 : 2.0;
        current_mip_resolution_inv *= skipped_tile ? 2.0 : 0.5;
        i++;

        exit_due_to_low_occupancy = !is_mirror && spvSubgroupBallotBitCount(spvSubgroupBallot(true)) <= min_traversal_occupancy;
    }
    valid_hit = uint(i) < max_traversal_intersections;
    return position;
}

float ValidateHit(float3 hit, Ray reflected_ray, float3 world_space_ray_direction, float2 screen_size, constant CSConstants& Constants, thread texture2d<float> g_depth_buffer_hierarchy, thread texture2d<float> g_normal)
{
    if (any(hit.xy < float2(0.0)) || any(hit.xy > float2(1.0)))
    {
        return 0.0;
    }

    int2 texel_coords = int2(screen_size * hit.xy);
    float surface_z = LoadDepth(float2(texel_coords / int2(2)), 1, g_depth_buffer_hierarchy);
    if (surface_z == 1.0)
    {
        return 0.0;
    }

    float3 hit_normal = LoadNormal(texel_coords, g_normal);
    if (dot(hit_normal, world_space_ray_direction) > 0.0)
    {
        return 0.0;
    }

    float3 view_space_surface = CreateViewSpaceRay(float3(hit.xy, surface_z), Constants).origin;
    float3 view_space_hit = CreateViewSpaceRay(hit, Constants).origin;
    float _distance = length(view_space_surface - view_space_hit);

    float2 fov = float2(screen_size.y / screen_size.x, 1.0) * 0.05;
    float2 border = smoothstep(float2(0.0), fov, hit.xy) * (float2(1.0) - smoothstep(float2(1.0) - fov, float2(1.0), hit.xy));
    float vignette = border.x * border.y;

    float confidence = 1.0 - smoothstep(0.0, Constants.g_depth_buffer_thickness, _distance);
    confidence *= confidence;
    return vignette * confidence;
}

void Intersect(int2 did, constant CSConstants& Constants, device uint* g_ranking_tile_buffer, device uint* g_sobol_buffer, device uint* g_scrambling_tile_buffer, thread texture2d<float> g_depth_buffer_hierarchy, thread texture2d<float> g_normal, thread texture2d<float, access::write> g_intersection_result, thread texture2d<float> g_roughness, thread texture2d<float> g_lit_scene, thread texturecube<float> g_environment_map, thread sampler g_environment_map_sampler, thread texture2d<float, access::write> g_ray_lengths, thread texture2d<float, access::write> g_denoised_reflections)
{
    uint2 screen_size = uint2(g_intersection_result.get_width(), g_intersection_result.get_height());

    uint skip_denoiser = Constants.g_skip_denoiser;

    float2 uv = (float2(did) + float2(0.5)) / float2(screen_size);
    float3 world_space_normal = LoadNormal(did, g_normal);
    float roughness = LoadRoughness(did, g_roughness);
    bool is_mirror = IsMirrorReflection(roughness);

    int most_detailed_mip = is_mirror ? 0 : Constants.g_most_detailed_mip;
    float2 mip_resolution = GetMipResolution(float2(screen_size), most_detailed_mip);
    float z = LoadDepth(uv * mip_resolution, most_detailed_mip, g_depth_buffer_hierarchy);

    Ray screen_space_ray;
    screen_space_ray.origin = float3(uv, z);

    Ray view_space_ray = CreateViewSpaceRay(screen_space_ray.origin, Constants);

    float3 view_space_surface_normal = (float4(normalize(world_space_normal), 0.0) * Constants.g_view).xyz;
    float3 view_space_reflected_direction = SampleReflectionVector(view_space_ray.direction, view_space_surface_normal, roughness, did, Constants, g_ranking_tile_buffer, g_sobol_buffer, g_scrambling_tile_buffer);
    screen_space_ray.direction = ProjectDirection(view_space_ray.origin, view_space_reflected_direction, screen_space_ray.origin, transpose(Constants.g_proj));

    bool valid_hit;
    float3 hit = HierarchicalRaymarch(screen_space_ray.origin, screen_space_ray.direction, is_mirror, float2(screen_size), valid_hit, Constants, g_depth_buffer_hierarchy);
    float3 world_space_reflected_direction = (float4(view_space_reflected_direction, 0.0) * Constants.g_inv_view).xyz;
    float confidence = valid_hit ? ValidateHit(hit, screen_space_ray, world_space_reflected_direction, float2(screen_size), Constants, g_depth_buffer_hierarchy, g_normal) : 0.0;

    float3 world_space_origin = InvProjectPosition(screen_space_ray.origin, transpose(Constants.g_inv_view_proj));
    float3 world_space_hit = InvProjectPosition(hit, transpose(Constants.g_inv_view_proj));
    float3 world_space_ray = world_space_hit - world_space_origin;

    float3 reflection_radiance = float3(0.0);
    if (confidence > 0.0)
    {
        reflection_radiance = FfxSssrUnpackSceneRadiance(g_lit_scene.read(uint2(float2(screen_size) * hit.xy), 0));
    }

    float3 environment_lookup = g_environment_map.sample(g_environment_map_sampler, world_space_reflected_direction, level(0.0)).xyz;
    reflection_radiance *= confidence;

    g_intersection_result.write(float4(reflection_radiance, 1.0), uint2(did));
    g_ray_lengths.write(float4(length(world_space_ray)), uint2(did));

    int2 idx = is_mirror || (skip_denoiser != 0u) ? did : int2(-1);
    g_denoised_reflections.write(float4(reflection_radiance, 1.0), uint2(idx));
}

struct CSData
{
    constant CSConstants& Constants [[id(0)]];
    device uint* g_ranking_tile_buffer [[id(1)]];
    device uint* g_sobol_buffer [[id(2)]];
    device uint* g_scrambling_tile_buffer [[id(3)]];
    texture2d<float> g_depth_buffer_hierarchy [[id(4)]];
    texture2d<float> g_normal [[id(5)]];
    texture2d<float, access::write> g_intersection_result [[id(6)]];
    texture2d<float> g_roughness [[id(7)]];
    texture2d<float> g_lit_scene [[id(8)]];
    texturecube<float> g_environment_map [[id(9)]];
    texture2d<float, access::write> g_ray_lengths [[id(10)]];
    texture2d<float, access::write> g_denoised_reflections [[id(11)]];
    device uint* g_ray_list [[id(12)]];
    sampler g_environment_map_sampler [[id(13)]];
};

//[numthreads(8, 8, 1)]
kernel void stageMain(
device CSData& csData [[buffer(UPDATE_FREQ_NONE)]],
uint group_index [[thread_index_in_threadgroup]],
uint3 gl_WorkGroupID [[threadgroup_position_in_grid]]
)
{
    uint ray_index = (gl_WorkGroupID.x * 64u) + group_index;
    uint packed_coords = csData.g_ray_list[ray_index];
    uint2 coords = Unpack(packed_coords);
    Intersect(int2(coords), csData.Constants, csData.g_ranking_tile_buffer, csData.g_sobol_buffer, csData.g_scrambling_tile_buffer, csData.g_depth_buffer_hierarchy, csData.g_normal, csData.g_intersection_result, csData.g_roughness, csData.g_lit_scene, csData.g_environment_map, csData.g_environment_map_sampler, csData.g_ray_lengths, csData.g_denoised_reflections);
}