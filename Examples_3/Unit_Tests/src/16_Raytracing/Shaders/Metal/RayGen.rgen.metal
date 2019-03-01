/*
 See LICENSE folder for this sample’s licensing information.
 
 Abstract:
 Metal shaders used for ray tracing
 */

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "ShaderTypes.h"


// Generates rays starting from the camera origin and traveling towards the image plane aligned
// with the camera's coordinate system.
// [numthreads(8, 8, 1)]
kernel void rayGen(uint2 tid                     [[thread_position_in_grid]],
                      // Buffers bound on the CPU. Note that 'constant' should be used for small
                      // read-only data which will be reused across threads. 'device' should be
                      // used for writable data or data which will only be used by a single thread.
                      device Ray *rays              [[buffer(0)]],
                      constant Uniforms & uniforms  [[buffer(1)]],
                      constant RayGenConfigBlock & configBlock [[buffer(10)]],
                   
                      texture2d<float, access::write> dstTex  [[texture(0)]] )
{
    // Since we aligned the thread count to the threadgroup size, the thread index may be out of bounds
    // of the render target size.
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        // Compute linear ray index from 2D position
        unsigned int rayIdx = tid.y * uniforms.width + tid.x;
        
        // Ray we will produce
        device Ray & ray = rays[rayIdx];
        
        uint2 launchIndex = tid.xy;
        uint2 launchDim = uint2(uniforms.width, uniforms.height);
        
        float2 crd = float2(launchIndex);
        float2 dims = float2(launchDim);
        
        float2 d = ((crd / dims) * 2.f - 1.f);
        float aspectRatio = dims.x / dims.y;
        
        ray.origin = configBlock.mCameraPosition;//float3(0, 0, -2);
        ray.direction = normalize(float3(d.x * aspectRatio, -d.y, 1.0f));

        // The camera emits primary rays
        ray.mask = 0xFF;
        ray.maxDistance = 10000;//INFINITY;
        
        //if is primary ray then HitGroupID provided by instance is used to choose which hit shader will be invoked
        //Set this to 0 when you configure ray in Hit/Miss shader. Set 1 in raygen shader.
        ray.isPrimaryRay = 1;
        //cleanup
        dstTex.write(float4(0.0, 0.0, 0.0, 0.0f), tid);
    }
}
