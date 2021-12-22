#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define TOTAL_IMGS 84
#define M_PI_F 3.141592653589793
#define M_1_PI_F 0.318309886183790

struct packed_vec3 
{
	float x;
	float y;
	float z;
};

vec3 to_vec3(packed_vec3 val)
{
	return vec3(val.x, val.y, val.z);
}

struct RayPayload
{
	vec3 radiance;
	uint recursionDepth;
};

layout(binding=0, set=0) uniform accelerationStructureEXT gRtScene;


layout(binding=2, set=0) buffer indices
{
    uint gIndices[];
};

layout(binding=3, set=0, std430) buffer positions
{
    packed_vec3 gPositions[];
};

layout(binding=4, set=0, std430) buffer normals
{
    packed_vec3 gNormals[];
};

layout(binding=5, set=0, std430) buffer uvs
{
    vec2 gUVs[];
};

layout(binding=6, set=0, std430) buffer materialIndices
{
    uint gMaterialIndices[];
};

layout(binding=7, set=0, std430) buffer materialTextureIndices
{
    uint gMaterialTextureIndices[];
};

layout(binding=8, set=0) uniform texture2D materialTextures[TOTAL_IMGS];

layout(binding=9, set=0) uniform sampler linearSampler;

layout(binding=0, set=1) uniform gSettings
{
    mat4 mCameraToWorld;
	vec2 mZ1PlaneSize;
	float mProjNear;
	float mProjFarMinusNear;
    vec2 mLightDirectionXY;
	float mLightDirectionZ;
	float mRandomSeed;
	vec2 mSubpixelJitter;
	uint mFrameIndex;
	uint mFramesSinceCameraMove;
};

hitAttributeEXT vec2 baryCoord;
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT RayPayload indirectRayPayload;
layout(location = 2) rayPayloadEXT bool shadowMiss;

// Uses the inversion method to map two uniformly random numbers to a three dimensional
// unit hemisphere where the probability of a given sample is proportional to the cosine
// of the angle between the sample direction and the "up" direction (0, 1, 0)
vec3 sampleCosineWeightedHemisphere(vec2 u) {
	float phi = 2.0f * M_PI_F * u.x;
	
	float sin_phi = sin(phi);
	float cos_phi = cos(phi);
	
	float cos_theta = sqrt(u.y);
	float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
	
	return vec3(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi);
}

// Aligns a direction on the unit hemisphere such that the hemisphere's "up" direction
// (0, 1, 0) maps to the given surface normal direction
vec3 alignHemisphereWithNormal(vec3 localVec, vec3 normal) {
	// Set the "up" vector to the normal
	vec3 up = normal;
	
	// Find an arbitrary direction perpendicular to the normal. This will become the
	// "right" vector.
	vec3 right = normalize(cross(normal, vec3(0.0072f, 1.0f, 0.0034f)));
	
	// Find a third vector perpendicular to the previous two. This will be the
	// "forward" vector.
	vec3 forward = cross(right, up);
	
	// Map the direction on the unit hemisphere to the coordinate system aligned
	// with the normal.
	return localVec.x * right + localVec.y * up + localVec.z * forward;
}

//====
// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/IncNoise.cginc
// The MIT License (MIT)
//
// Copyright (c) [2015] [Playdead]
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//note: normalized random, float=[0, 1]
float PDnrand( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898, 78.233f)))* 43758.5453 );
}
vec2 PDnrand2( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898, 78.233f)))* vec2(43758.5453, 28001.8384) );
}
vec3 PDnrand3( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898, 78.233f)))* vec3(43758.5453, 28001.8384, 50849.4141 ) );
}
vec4 PDnrand4( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898, 78.233f)))* vec4(43758.5453, 28001.8384, 50849.4141, 12996.89) );
}

// Convert uniform distribution into triangle-shaped distribution.
// https://www.shadertoy.com/view/4t2SDh
// Input is in range [0, 1]
// Output is in range [-1, 1], which is useful for dithering.
vec2 uniformNoiseToTriangular(vec2 n) {
	vec2 orig = n*2.0-1.0;
	n = orig / sqrt(abs(orig));
	n = max(vec2(-1.0),n);
	n = n-vec2(sign(orig));
	return n;
}

void main()
{
	const uint maxRecursionDepth = 1; // inclusive

	if (payload.recursionDepth > maxRecursionDepth)
	{
		return;
	}

	uvec2 pixelPos = gl_LaunchIDEXT.xy;
	vec2 pixelUV = (vec2(pixelPos) + 0.5) / vec2(gl_LaunchSizeEXT.xy);
	
	vec3 uvw;
	uvw.yz = baryCoord;
	uvw.x = 1.0f - uvw.y - uvw.z;

	uint triangleIndex = gl_PrimitiveID;

	uint i0 = gIndices[3 * triangleIndex + 0];
	uint i1 = gIndices[3 * triangleIndex + 1];
	uint i2 = gIndices[3 * triangleIndex + 2];

	vec3 position = uvw.x * to_vec3(gPositions[i0]) + uvw.y * to_vec3(gPositions[i1]) + uvw.z * to_vec3(gPositions[i2]);
	vec3 normal = normalize(uvw.x * to_vec3(gNormals[i0]) + uvw.y * to_vec3(gNormals[i1]) + uvw.z * to_vec3(gNormals[i2]));
	vec2 uv = uvw.x * gUVs[i0] + uvw.y * gUVs[i1] + uvw.z * gUVs[i2];

	uint materialIndex = gMaterialIndices[triangleIndex];
	uint albedoTextureIndex = gMaterialTextureIndices[5 * materialIndex];

	vec3 surfaceAlbedo = textureLod(sampler2D(materialTextures[albedoTextureIndex], linearSampler), uv, 0).rgb;

	vec3 rayOrigin = position + normal * 0.001;
	vec3 lightDir = vec3(mLightDirectionXY, mLightDirectionZ);

    uint rayFlags 	= gl_RayFlagsNoneEXT;
    uint cullMask 	= 0xff;
    float tMin = 0.0;
    float tMax = 10000.0;

	shadowMiss = false;
 	traceRayEXT(gRtScene, rayFlags, cullMask, 1, 0, 1, rayOrigin, tMin, lightDir, tMax, 2);

	if (shadowMiss)
	{
		vec3 lightSample = 5.0 * max(dot(lightDir, normal), 0.0) * surfaceAlbedo * M_1_PI_F;
		payload.radiance += lightSample;
	}

	vec2 sampleUV = uniformNoiseToTriangular(PDnrand2(pixelUV + mRandomSeed + payload.recursionDepth)) * 0.5 + 0.5;
	vec3 sampleDirLocal = sampleCosineWeightedHemisphere(sampleUV);
	vec3 sampleDir = alignHemisphereWithNormal(sampleDirLocal, normal);
	
	if (payload.recursionDepth < maxRecursionDepth)
	{
		indirectRayPayload.recursionDepth = payload.recursionDepth + 1;
		indirectRayPayload.radiance = vec3(0.0);
		traceRayEXT(gRtScene, rayFlags, cullMask, 0, 0, 0, rayOrigin, tMin, sampleDir, tMax, 1);

		// The indirect sample has a PDF of cos(theta) / pi, so we only need to multiply 
		// by the albedo for Lambertian diffuse.
		payload.radiance += indirectRayPayload.radiance * surfaceAlbedo;
	}
}
