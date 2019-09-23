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
kernel void chsShadow(uint2 tid                     [[thread_position_in_grid]],
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
        
        device Payload & pload = payload[rayIdx];
        // Intersection distance will be negative if ray missed or was disabled in a previous
        // iteration.
        if (ray.maxDistance >= 0.0f && intersection.distance >= 0.0f) {
            
            if (ray.isPrimaryRay != 0)
            {
                //Do nothing if hitGroupID of instance and of shader settings do not match
                //This intersection will be processed by another compute shader
                if (hitGroupID[intersection.instanceIndex] != shaderSettings.hitGroupID)
                {
                    return;
                }
            }
            
            //uint mask = triangleMasks[intersection.primitiveIndex];
            
            // Barycentric coordinates sum to one
            //float3 uvw;
            //uvw.xy = intersection.coordinates;
            //uvw.z = 1.0f - uvw.x - uvw.y;
            
            
            // Clear the destination image to black
#ifndef TARGET_IOS
            csData.gOutput.write(float4(0.8 * pload.color, 1.0f), tid);
#else
            gOutput.write(float4(0.8 * pload.color, 1.0f), tid);
#endif
        }

    }
        
}
