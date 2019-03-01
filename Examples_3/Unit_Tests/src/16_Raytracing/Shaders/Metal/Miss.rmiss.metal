#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "ShaderTypes.h"


// Generates rays starting from the camera origin and traveling towards the image plane aligned
// with the camera's coordinate system.
// [numthreads(8, 8, 1)]
kernel void miss(uint2 tid                                       [[thread_position_in_grid]],
                 //default resources
                 device Ray *rays                        [[buffer(0)]],
                 constant Uniforms & uniforms            [[buffer(1)]],
                 device Intersection *intersections      [[buffer(2)]],
                 device float3 *vertexAttribs            [[buffer(4)]],
                 device uint *triangleMasks              [[buffer(6)]], //Rustam: change to instanceMasks
                 device uint *hitGroupID                 [[buffer(7)]],
                 device ShaderSettings &shaderSettings   [[buffer(8)]],
                 
                 texture2d<float, access::write> dstTex  [[texture(0)]])
{
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        unsigned int rayIdx = tid.y * uniforms.width + tid.x;
        device Ray & ray = rays[rayIdx];
        device Intersection & intersection = intersections[rayIdx];
        
        if (ray.maxDistance >= 0.0f && intersection.distance < 0.0f) {
            
            // The ray missed the scene, so terminate the ray's path
            ray.maxDistance = -1.0f;
            
            dstTex.write(float4(0.1, 0.0, 0.3, 1.0f), tid);
            
        }
        
        
    }
}

