#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "ShaderTypes.h"

#ifndef TARGET_IOS
struct CSData {
    texture2d<float, access::write> gOutput;
};
#endif

// [numthreads(8, 8, 1)]
kernel void missShadow(uint2 tid                               [[thread_position_in_grid]],
                       //default resources
                       device Ray *rays                        [[buffer(0)]],
                       constant Uniforms & uniforms            [[buffer(1)]],
                       device Intersection *intersections      [[buffer(2)]],
                       device float3 *vertexAttribs            [[buffer(4)]],
                       device Payload *payload                 [[buffer(5)]],
                       device uint *triangleMasks              [[buffer(6)]], //Rustam: change to instanceMasks
                       device uint *hitGroupID                 [[buffer(7)]],
                       device ShaderSettings &shaderSettings   [[buffer(8)]],
#ifndef TARGET_IOS
                      constant CSData& csData                    [[buffer(UPDATE_FREQ_NONE)]]
#else
                      texture2d<float, access::write> gOutput    [[texture(0)]]
#endif
)
{
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        unsigned int rayIdx = tid.y * uniforms.width + tid.x;
        device Ray & ray = rays[rayIdx];
        device Intersection & intersection = intersections[rayIdx];
        
        if (ray.maxDistance >= 0.0f && intersection.distance < 0.0f) {
            // The ray missed the scene, so terminate the ray's path
            ray.maxDistance = -1.0f;

            device Payload & pload = payload[rayIdx];
#ifndef TARGET_IOS
            csData.gOutput.write(float4(pload.color, 0.0f), tid);
#else
            gOutput.write(float4(pload.color, 0.0f), tid);
#endif
        }
    }
}
