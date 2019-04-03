#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "ShaderTypes.h"

// Interpolates vertex attribute of an arbitrary type across the surface of a triangle
// given the barycentric coordinates and triangle index in an intersection struct
template<typename T>
inline T interpolateVertexAttribute(device T *attributes, Intersection intersection) {
    // Barycentric coordinates sum to one
    float3 uvw;
    uvw.xy = intersection.coordinates;
    uvw.z = 1.0f - uvw.x - uvw.y;
    
    unsigned int triangleIndex = intersection.primitiveIndex;
    
    // Lookup value for each vertex
    T T0 = attributes[triangleIndex * 3 + 0];
    T T1 = attributes[triangleIndex * 3 + 1];
    T T2 = attributes[triangleIndex * 3 + 2];
    
    // Compute sum of vertex attributes weighted by barycentric coordinates
    return uvw.x * T0 + uvw.y * T1 + uvw.z * T2;
}

// Uses the inversion method to map two uniformly random numbers to a three dimensional
// unit hemisphere where the probability of a given sample is proportional to the cosine
// of the angle between the sample direction and the "up" direction (0, 1, 0)
inline float3 sampleCosineWeightedHemisphere(float2 u) {
    float phi = 2.0f * M_PI_F * u.x;
    
    float cos_phi;
    float sin_phi = sincos(phi, cos_phi);
    
    float cos_theta = sqrt(u.y);
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
    
    return float3(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi);
}

// Aligns a direction on the unit hemisphere such that the hemisphere's "up" direction
// (0, 1, 0) maps to the given surface normal direction
inline float3 alignHemisphereWithNormal(float3 sample, float3 normal) {
    // Set the "up" vector to the normal
    float3 up = normal;
    
    // Find an arbitrary direction perpendicular to the normal. This will become the
    // "right" vector.
    float3 right = normalize(cross(normal, float3(0.0072f, 1.0f, 0.0034f)));
    
    // Find a third vector perpendicular to the previous two. This will be the
    // "forward" vector.
    float3 forward = cross(right, up);
    
    // Map the direction on the unit hemisphere to the coordinate system aligned
    // with the normal.
    return sample.x * right + sample.y * up + sample.z * forward;
}

// Consumes ray/triangle intersection results to compute the shaded image
// [numthreads(8, 8, 1)]
kernel void chsPlane(uint2 tid                                       [[thread_position_in_grid]],
                //default resources
                device Ray *rays                        [[buffer(0)]],
                constant Uniforms & uniforms            [[buffer(1)]],
                device Intersection *intersections      [[buffer(2)]],
                device float3 *vertexAttribs            [[buffer(4)]],
                device Payload *payload                 [[buffer(5)]],
                device uint *triangleMasks              [[buffer(6)]], //Rustam: change to instanceMasks
                device uint *hitGroupID                 [[buffer(7)]],
                device ShaderSettings &shaderSettings   [[buffer(8)]],
                     
                constant RayGenConfigBlock & configBlock [[buffer(10)]],
                
                texture2d<float, access::write> dstTex  [[texture(0)]])
{
    if (tid.x < uniforms.width && tid.y < uniforms.height) {
        unsigned int rayIdx = tid.y * uniforms.width + tid.x;
        device Ray & ray = rays[rayIdx];
        device Intersection & intersection = intersections[rayIdx];
        
        device Payload & pload = payload[rayIdx];
        // Intersection distance will be negative if ray missed or was disabled in a previous
        // iteration.
        if (ray.maxDistance >= 0.0f && intersection.distance >= 0.0f) {
            //uint mask = triangleMasks[intersection.primitiveIndex];
            
            if (ray.isPrimaryRay != 0)
            {
                //Do nothing if hitGroupID of instance and of shader settings do not match
                //This intersection will be processed by another compute shader
                if (hitGroupID[intersection.instanceIndex] != shaderSettings.hitGroupID)
                    return;
            }
            
            // Barycentric coordinates sum to one
            float3 uvw;
            uvw.xy = intersection.coordinates;
            uvw.z = 1.0f - uvw.x - uvw.y;
            
            const float3 A = float3(1, 1, 1);
            const float3 B = float3(1, 1, 1);
            const float3 C = float3(1, 1, 1);
            
            float3 color = A * uvw.x + B * uvw.y + C * uvw.z;
            
            // Clear the destination image to black
            //dstTex.write(float4(color, 1.0f), tid);
            pload.color = color;
            
            float3 normal = float3(0.0, 1.0, 0.0);
            ray.origin = ray.origin + ray.direction * intersection.distance + normal * 0.001;
            ray.direction = normalize(configBlock.mLightDirection);

            ray.mask = 0xFF;
            ray.maxDistance = 10000;//INFINITY;
            
            //if is primary ray then HitGroupID provided by instance is used to choose which hit shader will be invoked
            ray.isPrimaryRay = 0;
        } else
        {
            ray.maxDistance = -1.0f;
        }
        

    }
}
