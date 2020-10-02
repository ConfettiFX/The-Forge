#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

//[numthreads(8, 8, 1)]
kernel void stageMain(
texture2d<float> Source [[texture(0)]],
texture2d<float, access::write> Destination [[texture(1)]],
uint3 did [[thread_position_in_grid]]
)
{
    uint2 screen_size = uint2(Source.get_width(), Source.get_height());
    if ((did.x < screen_size.x) && (did.y < screen_size.y))
    {
        float storeTemp = Source.read(uint2(did.xy), 0).x;
        Destination.write(float4(storeTemp), uint2(did.xy));
    }
}


