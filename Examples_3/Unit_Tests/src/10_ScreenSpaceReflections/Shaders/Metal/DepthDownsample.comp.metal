#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wunused-variable"

#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_atomic>

using namespace metal;

float GetMipsCount(float2 texture_size)
{
    float max_dim = fast::max(texture_size.x, texture_size.y);
    return 1.0 + floor(log2(max_dim));
}

uint GetThreadgroupCount(uint2 image_size)
{
    return ((image_size.x + 63u) / 64u) * ((image_size.y + 63u) / 64u);
}

uint2 ARmpRed8x8(uint a)
{
    return uint2(insert_bits(extract_bits(a, 2, 3), a, 0, 1), insert_bits(extract_bits(a, 3, 3), extract_bits(a, 1, 2), 0, 2));
}

float4 SpdLoadSourceImage(uint2 index, thread texture2d<float> g_depth_buffer)
{
    return float4(g_depth_buffer.read(index, 0u).x);
}

float4 SpdReduce4(float4 v0, float4 v1, float4 v2, float4 v3)
{
    return fast::min(fast::min(v0, v1), fast::min(v2, v3));
}

float4 SpdReduceLoadSourceImage4(uint2 i0, uint2 i1, uint2 i2, uint2 i3, thread texture2d<float> g_depth_buffer)
{
    float4 v0 = SpdLoadSourceImage(i0, g_depth_buffer);
    float4 v1 = SpdLoadSourceImage(i1, g_depth_buffer);
    float4 v2 = SpdLoadSourceImage(i2, g_depth_buffer);
    float4 v3 = SpdLoadSourceImage(i3, g_depth_buffer);
    return SpdReduce4(v0, v1, v2, v3);
}

float4 SpdReduceLoadSourceImage4(uint2 base, thread texture2d<float> g_depth_buffer)
{
    return SpdReduceLoadSourceImage4(base + uint2(0u), base + uint2(0u, 1u), base + uint2(1u, 0u), base + uint2(1u), g_depth_buffer);
}

void SpdStore(uint2 pix, float4 outValue, uint index, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer)
{
    g_downsampled_depth_buffer[index + 1u].write(float4(outValue.x), pix);
}

float4 SpdReduceQuad(float4 v, uint gl_SubgroupInvocationID)
{
    uint quad = gl_SubgroupInvocationID & ~3u;
    float4 v0 = v;
    float4 v1 = simd_shuffle(v, quad | 1u);
    float4 v2 = simd_shuffle(v, quad | 2u);
    float4 v3 = simd_shuffle(v, quad | 3u);
    return SpdReduce4(v0, v1, v2, v3);
}

void SpdStoreIntermediate(uint x, uint y, float4 value, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    g_group_shared_depth_values[x][y] = value.x;
}

void SpdDownsampleMips_0_1(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip, uint gl_SubgroupInvocationID, thread texture2d<float> g_depth_buffer, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    float4 v[4];

    uint2 tex = workGroupID * uint2(64u) + uint2(x * 2u, y * 2u);
    uint2 pix = workGroupID * uint2(32u) + uint2(x, y);
    v[0] = SpdReduceLoadSourceImage4(tex, g_depth_buffer);
    SpdStore(pix, v[0], 0, g_downsampled_depth_buffer);

    tex = workGroupID * uint2(64u) + uint2(x * 2u + 32u, y * 2u);
    pix = workGroupID * uint2(32u) + uint2(x + 16u, y);
    v[1] = SpdReduceLoadSourceImage4(tex, g_depth_buffer);
    SpdStore(pix, v[1], 0, g_downsampled_depth_buffer);

    tex = workGroupID * uint2(64u) + uint2(x * 2u, y * 2u + 32u);
    pix = workGroupID * uint2(32u) + uint2(x, y + 16u);
    v[2] = SpdReduceLoadSourceImage4(tex, g_depth_buffer);
    SpdStore(pix, v[2], 0, g_downsampled_depth_buffer);

    tex = workGroupID * uint2(64u) + uint2(x * 2u + 32u, y * 2u + 32u);
    pix = workGroupID * uint2(32u) + uint2(x + 16u, y + 16u);
    v[3] = SpdReduceLoadSourceImage4(tex, g_depth_buffer);
    SpdStore(pix, v[3], 0, g_downsampled_depth_buffer);

    if (mip <= 1u)
    {
        return;
    }

    v[0] = SpdReduceQuad(v[0], gl_SubgroupInvocationID);
    v[1] = SpdReduceQuad(v[1], gl_SubgroupInvocationID);
    v[2] = SpdReduceQuad(v[2], gl_SubgroupInvocationID);
    v[3] = SpdReduceQuad(v[3], gl_SubgroupInvocationID);

    if ((localInvocationIndex % 4u) == 0u)
    {
        SpdStore(workGroupID * uint2(16u) + uint2(x / 2u, y / 2u), v[0], 1, g_downsampled_depth_buffer);
        SpdStoreIntermediate(x / 2u, y / 2u, v[0], g_group_shared_depth_values);

        SpdStore(workGroupID * uint2(16u) + uint2(x / 2u + 8u, y / 2u), v[1], 1, g_downsampled_depth_buffer);
        SpdStoreIntermediate(x / 2u + 8u, y / 2u, v[1], g_group_shared_depth_values);

        SpdStore(workGroupID * uint2(16u) + uint2(x / 2u, y / 2u + 8u), v[2], 1, g_downsampled_depth_buffer);
        SpdStoreIntermediate(x / 2u, y / 2u + 8u, v[2], g_group_shared_depth_values);

        SpdStore(workGroupID * uint2(16u) + uint2(x / 2u + 8u, y / 2u + 8u), v[3], 1, g_downsampled_depth_buffer);
        SpdStoreIntermediate(x / 2u + 8u, y / 2u + 8u, v[3], g_group_shared_depth_values);
    }
}

float4 SpdLoadIntermediate(uint x, uint y, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    return float4(g_group_shared_depth_values[x][y]);
}

void SpdDownsampleMip_2(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip, uint gl_SubgroupInvocationID, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    float4 v = SpdLoadIntermediate(x, y, g_group_shared_depth_values);
    v = SpdReduceQuad(v, gl_SubgroupInvocationID);

    if ((localInvocationIndex % 4u) == 0u)
    {
        SpdStore(workGroupID * uint2(8u) + uint2(x / 2u, y / 2u), v, mip, g_downsampled_depth_buffer);
        SpdStoreIntermediate(x + ((y / 2u) % 2u), y, v, g_group_shared_depth_values);
    }
}

void SpdDownsampleMip_3(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip, uint gl_SubgroupInvocationID, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    if (localInvocationIndex < 64u)
    {
        float4 v = SpdLoadIntermediate(x * 2u + y % 2u, y * 2u, g_group_shared_depth_values);
        v = SpdReduceQuad(v, gl_SubgroupInvocationID);

        if ((localInvocationIndex % 4u) == 0u)
        {
            SpdStore(workGroupID * uint2(4u) + uint2(x / 2u, y / 2u), v, mip, g_downsampled_depth_buffer);
            SpdStoreIntermediate(x * 2u + y / 2u, y * 2u, v, g_group_shared_depth_values);
        }
    }
}

void SpdDownsampleMip_4(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip, uint gl_SubgroupInvocationID, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    if (localInvocationIndex < 16u)
    {
        float4 v = SpdLoadIntermediate(x * 4u + y, y * 4u, g_group_shared_depth_values);
        v = SpdReduceQuad(v, gl_SubgroupInvocationID);

        if ((localInvocationIndex % 4u) == 0u)
        {
            SpdStore(workGroupID * uint2(2u) + uint2(x / 2u, y / 2u), v, mip, g_downsampled_depth_buffer);
            SpdStoreIntermediate((x / 2u) + y, 0, v, g_group_shared_depth_values);
        }
    }
}

void SpdDownsampleMip_5(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint mip, uint gl_SubgroupInvocationID, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    if (localInvocationIndex < 4u)
    {
        float4 v = SpdLoadIntermediate(localInvocationIndex, 0u, g_group_shared_depth_values);
        v = SpdReduceQuad(v, gl_SubgroupInvocationID);

        if ((localInvocationIndex % 4u) == 0u)
        {
            SpdStore(workGroupID, v, mip, g_downsampled_depth_buffer);
        }
    }
}

void SpdDownsampleNextFour(uint x, uint y, uint2 workGroupID, uint localInvocationIndex, uint baseMip, uint mips, thread uint& gl_SubgroupInvocationID, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    if (mips <= baseMip)
    {
        return;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    SpdDownsampleMip_2(x, y, workGroupID, localInvocationIndex, baseMip, gl_SubgroupInvocationID, g_downsampled_depth_buffer, g_group_shared_depth_values);

    if (mips <= (baseMip + 1u))
    {
        return;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    SpdDownsampleMip_3(x, y, workGroupID, localInvocationIndex, baseMip + 1, gl_SubgroupInvocationID, g_downsampled_depth_buffer, g_group_shared_depth_values);

    if (mips <= (baseMip + 2u))
    {
        return;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    SpdDownsampleMip_4(x, y, workGroupID, localInvocationIndex, baseMip + 2, gl_SubgroupInvocationID, g_downsampled_depth_buffer, g_group_shared_depth_values);

    if (mips <= (baseMip + 3u))
    {
        return;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    SpdDownsampleMip_5(x, y, workGroupID, localInvocationIndex, baseMip + 3, gl_SubgroupInvocationID, g_downsampled_depth_buffer, g_group_shared_depth_values);
}

void SpdIncreaseAtomicCounter(device atomic_uint* g_global_atomic, threadgroup uint& g_group_shared_counter)
{
    g_group_shared_counter = atomic_fetch_add_explicit(g_global_atomic, 1u, memory_order_relaxed);
}

void SpdResetAtomicCounter(device atomic_uint* g_global_atomic)
{
	atomic_store_explicit(g_global_atomic, 0, memory_order_relaxed);
}

uint SpdGetAtomicCounter(threadgroup uint& g_group_shared_counter)
{
    return g_group_shared_counter;
}

bool SpdExitWorkgroup(uint numWorkGroups, uint localInvocationIndex, device atomic_uint* g_global_atomic, threadgroup uint& g_group_shared_counter)
{
    if (localInvocationIndex == 0u)
    {
        SpdIncreaseAtomicCounter(g_global_atomic, g_group_shared_counter);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    return SpdGetAtomicCounter(g_group_shared_counter) != (numWorkGroups - 1u);
}

float4 SpdLoad(uint2 index, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer)
{
    return float4(g_downsampled_depth_buffer[6].read(index).x);
}

float4 SpdReduceLoad4(uint2 i0, uint2 i1, uint2 i2, uint2 i3, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer)
{
    float4 v0 = SpdLoad(i0, g_downsampled_depth_buffer);
    float4 v1 = SpdLoad(i1, g_downsampled_depth_buffer);
    float4 v2 = SpdLoad(i2, g_downsampled_depth_buffer);
    float4 v3 = SpdLoad(i3, g_downsampled_depth_buffer);
    return SpdReduce4(v0, v1, v2, v3);
}

float4 SpdReduceLoad4(uint2 base, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer)
{
    return SpdReduceLoad4(base + uint2(0u), base + uint2(0u, 1u), base + uint2(1u, 0u), base + uint2(1u), g_downsampled_depth_buffer);
}

void SpdDownsampleMips_6_7(uint x, uint y, uint mips, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, threadgroup float (&g_group_shared_depth_values)[16][16])
{
    uint2 tex = uint2(x * 4u + 0u, y * 4u + 0u);
    uint2 pix = uint2(x * 2u + 0u, y * 2u + 0u);
    float4 v0 = SpdReduceLoad4(tex, g_downsampled_depth_buffer);
    SpdStore(pix, v0, 6, g_downsampled_depth_buffer);

    tex = uint2(x * 4u + 2u, y * 4u + 0u);
    pix = uint2(x * 2u + 1u, y * 2u + 0u);
    float4 v1 = SpdReduceLoad4(tex, g_downsampled_depth_buffer);
    SpdStore(pix, v1, 6, g_downsampled_depth_buffer);

    tex = uint2(x * 4u + 0u, y * 4u + 2u);
    pix = uint2(x * 2u + 0u, y * 2u + 1u);
    float4 v2 = SpdReduceLoad4(tex, g_downsampled_depth_buffer);
    SpdStore(pix, v2, 6, g_downsampled_depth_buffer);

    tex = uint2(x * 4u + 2u, y * 4u + 2u);
    pix = uint2(x * 2u + 1u, y * 2u + 1u);
    float4 v3 = SpdReduceLoad4(tex, g_downsampled_depth_buffer);
    SpdStore(pix, v3, 6, g_downsampled_depth_buffer);

    if (mips <= 7u)
    {
        return;
    }

    float4 v = SpdReduce4(v0, v1, v2, v3);
    SpdStore(uint2(x, y), v, 7, g_downsampled_depth_buffer);
    SpdStoreIntermediate(x, y, v, g_group_shared_depth_values);
}

void SpdDownsample(uint2 workGroupID, uint localInvocationIndex, uint mips, thread const uint& numWorkGroups, thread uint& gl_SubgroupInvocationID, thread texture2d<float> g_depth_buffer, thread const array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer, device atomic_uint* g_global_atomic, threadgroup float (&g_group_shared_depth_values)[16][16], threadgroup uint& g_group_shared_counter)
{
    uint2 sub_xy = ARmpRed8x8(localInvocationIndex % 64u);
    uint x = sub_xy.x + 8u * ((localInvocationIndex >> (6u & 31u)) % 2u);
    uint y = sub_xy.y + 8u * (localInvocationIndex >> (7u & 31u));
    SpdDownsampleMips_0_1(x, y, workGroupID, localInvocationIndex, mips, gl_SubgroupInvocationID, g_depth_buffer, g_downsampled_depth_buffer, g_group_shared_depth_values);

    SpdDownsampleNextFour(x, y, workGroupID, localInvocationIndex, 2, mips, gl_SubgroupInvocationID, g_downsampled_depth_buffer, g_group_shared_depth_values);

    if (mips <= 6u)
    {
        return;
    }

    if (SpdExitWorkgroup(numWorkGroups, localInvocationIndex, g_global_atomic, g_group_shared_counter))
    {
        return;
    }

    SpdResetAtomicCounter(g_global_atomic);

    SpdDownsampleMips_6_7(x, y, mips, g_downsampled_depth_buffer, g_group_shared_depth_values);

    SpdDownsampleNextFour(x, y, uint2(0u), localInvocationIndex, 8, mips, gl_SubgroupInvocationID, g_downsampled_depth_buffer, g_group_shared_depth_values);
}

struct CSData {
	texture2d<float> g_depth_buffer [[id(0)]];
	array<texture2d<float, access::read_write>, 13> g_downsampled_depth_buffer [[id(1)]];
	device atomic_uint* g_global_atomic [[id(14)]];
};


//[numthreads(32,8,1)]
kernel void stageMain(
device CSData& csData      [[buffer(UPDATE_FREQ_NONE)]],
uint3 gl_GlobalInvocationID  [[thread_position_in_grid]],
uint3 gl_WorkGroupID         [[threadgroup_position_in_grid]],
uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]], 
uint gl_SubgroupInvocationID [[thread_index_in_simdgroup]])
{
    threadgroup float g_group_shared_depth_values[16][16];
    threadgroup uint g_group_shared_counter;

	float2 depth_image_size = float2(csData.g_depth_buffer.get_width(), csData.g_depth_buffer.get_height());
    uint2 u_depth_image_size = uint2(depth_image_size);

    for (uint i = 0; i < 2; i++)
    {
        for (uint j = 0; j < 8; j++)
        {
            uint2 idx = uint2(2u * gl_GlobalInvocationID.x + i, 8u * gl_GlobalInvocationID.y + j);
            if ((idx.x < u_depth_image_size.x) && (idx.y < u_depth_image_size.y))
            {
				csData.g_downsampled_depth_buffer[0].write(float4(csData.g_depth_buffer.read(idx, 0u).x), idx);
            }
        }
    }

	float2 image_size = float2(csData.g_downsampled_depth_buffer[0].get_width(), csData.g_downsampled_depth_buffer[0].get_height());
    float mips_count = GetMipsCount(image_size);
    uint threadgroup_count = GetThreadgroupCount(uint2(image_size));

    SpdDownsample(gl_WorkGroupID.xy, gl_LocalInvocationIndex, uint(mips_count), threadgroup_count, gl_SubgroupInvocationID, csData.g_depth_buffer, csData.g_downsampled_depth_buffer, csData.g_global_atomic, g_group_shared_depth_values, g_group_shared_counter);
}
