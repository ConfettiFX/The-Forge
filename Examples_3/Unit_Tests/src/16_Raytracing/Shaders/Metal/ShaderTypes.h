#ifndef ShaderTypes_h
#define ShaderTypes_h

#include <simd/simd.h>

struct Uniforms
{
    unsigned int width;
    unsigned int height;
    unsigned int blocksWide;
};

// Represents a three dimensional ray which will be intersected with the scene. The ray type
// is customized using properties of the MPSRayIntersector.
struct Ray {
    // Starting point
    packed_float3 origin;
    
    // Mask which will be bitwise AND-ed with per-triangle masks to filter out certain
    // intersections. This is used to make the light source visible to the camera but not
    // to shadow or secondary rays.
    uint mask;
    
    // Direction the ray is traveling
    packed_float3 direction;
    
    // Maximum intersection distance to accept. This is used to prevent shadow rays from
    // overshooting the light source when checking for visibility.
    float maxDistance;
    
    uint isPrimaryRay;
};

// Represents an intersection between a ray and the scene, returned by the MPSRayIntersector.
// The intersection type is customized using properties of the MPSRayIntersector.
// MPSIntersectionDataTypeDistancePrimitiveIndexInstanceIndexCoordinates
struct Intersection {
    // The distance from the ray origin to the intersection point. Negative if the ray did not
    // intersect the scene.
    float distance;
    
    // The index of the intersected primitive (triangle), if any. Undefined if the ray did not
    // intersect the scene.
    unsigned primitiveIndex;
    
    unsigned instanceIndex;
    
    // The barycentric coordinates of the intersection point, if any. Undefined if the ray did
    // not intersect the scene.
    float2 coordinates;
};

struct ShaderSettings
{
    uint hitGroupID;
};

struct Payload
{
    packed_float3 color;
};

struct RayGenConfigBlock
{
    float3 mCameraPosition;
    float3 mLightDirection;
};

#endif /* ShaderTypes_h */

