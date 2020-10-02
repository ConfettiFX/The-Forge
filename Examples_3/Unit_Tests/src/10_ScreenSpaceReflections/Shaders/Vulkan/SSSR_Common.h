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

layout(set = 0, binding = 0, std140) uniform Constants
{
    layout(row_major) mat4 g_inv_view_proj;
    layout(row_major) mat4 g_proj;
    layout(row_major) mat4 g_inv_proj;
    layout(row_major) mat4 g_view;
    layout(row_major) mat4 g_inv_view;
    layout(row_major) mat4 g_prev_view_proj;
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

vec3 ProjectPosition(vec3 origin, mat4 mat)
{
    vec4 projected = mat * vec4(origin, 1.0);
    projected.xyz = projected.xyz / projected.w;
    projected.xy = projected.xy * 0.5 + 0.5;
    projected.y = 1.0 - projected.y;
    return projected.xyz;
}

vec3 InvProjectPosition(vec3 origin, mat4 mat)
{
    origin.y = 1.0 - origin.y;
    origin.xy = (origin.xy * 2.0) - vec2(1.0);
    vec4 projected = mat * vec4(origin, 1.0);
    projected.xyz = projected.xyz / projected.w;
    return projected.xyz;
}

vec3 ProjectDirection(vec3 origin, vec3 direction, vec3 screen_space_origin, mat4 mat)
{
    vec3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

struct Ray
{
    vec3 origin;
    vec3 direction;
};

Ray CreateViewSpaceRay(vec3 screen_space_pos)
{
    vec3 view_space_pos = InvProjectPosition(screen_space_pos, g_inv_proj);
    Ray view_space_ray;
    view_space_ray.origin = view_space_pos;
    view_space_ray.direction = view_space_pos;
    return view_space_ray;
}

vec3 LoadNormal(ivec2 index, texture2D tex)
{
    return FfxSssrUnpackNormals(texelFetch(tex, index, 0));
}

float LoadRoughness(ivec2 index, texture2D tex)
{
    return FfxSssrUnpackRoughness(texelFetch(tex, index, 0));
}

bool IsGlossy(float roughness)
{
    return roughness < g_roughness_threshold;
}

bool IsMirrorReflection(float roughness)
{
    return roughness < 0.0001;
}

float GetEdgeStoppingNormalWeight(vec3 normal_p, vec3 normal_q, float sigma)
{
    return pow(max(dot(normal_p, normal_q), 0.0), sigma);
}

float GetEdgeStoppingRoughnessWeight(float roughness_p, float roughness_q, float sigma_min, float sigma_max)
{
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

mediump float GetEdgeStoppingRoughnessWeightFP16(mediump float roughness_p, mediump float roughness_q, mediump float sigma_min, mediump float sigma_max)
{
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

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

float Luminance(vec3 clr)
{
    return max(dot(clr, vec3(0.299, 0.587, 0.114)), 0.00001);
}

uint Pack(uvec2 coord)
{
    return (coord.x & 0xFFFF) | ((coord.y & 0xFFFF) << uint(16));
}

uvec2 Unpack(uint _packed)
{
    return uvec2(_packed & 0xFFFF, _packed >> uint(16));
}

bool IsBaseRay(uvec2 did, uint samples_per_quad)
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

uint GetBaseLane(uint lane, uint samples_per_quad)
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

uint PackFloat16(vec2 v)
{
    return packHalf2x16(v);
}

mediump vec2 UnpackFloat16(uint a)
{
    return unpackHalf2x16(a);
}

uvec2 RemapLane8x8(uint lane) 
{
    return uvec2(bitfieldInsert(bitfieldExtract(lane, 2, 3), lane, 0, 1), bitfieldInsert(bitfieldExtract(lane, 3, 3), bitfieldExtract(lane, 1, 2), 0, 2));
}
