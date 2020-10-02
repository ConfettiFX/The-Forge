#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Uniforms_RootConstant
{
    uint2 MipSize;
};

//[numthreads(16, 16, 1)]
kernel void stageMain(
constant Uniforms_RootConstant& RootConstant [[buffer(UPDATE_FREQ_USER)]],
texture2d<float, access::read_write> Source [[texture(0)]],
texture2d<float, access::write> Destination [[texture(1)]],
uint3 id [[thread_position_in_grid]]
)
{
    if ((id.x < RootConstant.MipSize.x) && (id.y < RootConstant.MipSize.y))
    {
        float color = 1.0;
        for (uint x = 0; x < 2; x++)
        {
            for (uint y = 0; y < 2; y++)
            {
                color = fast::min(color, Source.read(id.xy * 2u + uint2(x, y)).x);
            }
        }
        Destination.write(float4(color), uint2(id.xy));
    }
}


