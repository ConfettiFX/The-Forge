/*
 See LICENSE folder for this sample’s licensing information.
 
 Abstract:
 Metal shaders used for ray tracing
 */

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "ShaderTypes.h"
#include "ClosestHitShader.h"

// Interpolates vertex attribute of an arbitrary type across the surface of a triangle
// given the barycentric coordinates and triangle index in an intersection struct
template<typename T>
inline T interpolateVertexAttribute(const device T *attributes, const device uint* indices, Intersection intersection) {
	// Barycentric coordinates sum to one
	float3 uvw;
	uvw.xy = intersection.coordinates;
	uvw.z = 1.0f - uvw.x - uvw.y;
	
	uint triangleIndex = intersection.primitiveIndex;
	
	uint i0 = indices[3 * triangleIndex + 0];
	uint i1 = indices[3 * triangleIndex + 1];
	uint i2 = indices[3 * triangleIndex + 2];
	
	// Lookup value for each vertex
	T T0 = attributes[i0];
	T T1 = attributes[i1];
	T T2 = attributes[i2];
	
	// Compute sum of vertex attributes weighted by barycentric coordinates
	return uvw.x * T0 + uvw.y * T1 + uvw.z * T2;
}

inline float3 interpolateVertexAttribute(const device packed_float3 *attributes, const device uint* indices, Intersection intersection) {
	// Barycentric coordinates sum to one
	float3 uvw;
	uvw.xy = intersection.coordinates;
	uvw.z = 1.0f - uvw.x - uvw.y;
	
	uint triangleIndex = intersection.primitiveIndex;
	
	uint i0 = indices[3 * triangleIndex + 0];
	uint i1 = indices[3 * triangleIndex + 1];
	uint i2 = indices[3 * triangleIndex + 2];
	
	// Lookup value for each vertex
	float3 T0 = float3(attributes[i0]);
	float3 T1 = float3(attributes[i1]);
	float3 T2 = float3(attributes[i2]);
	
	// Compute sum of vertex attributes weighted by barycentric coordinates
	return uvw.x * T0 + uvw.y * T1 + uvw.z * T2;
}

// Uses the inversion method to map two uniformly random numbers to a three dimensional
// unit hemisphere where the probability of a given sample is proportional to the cosine
// of the angle between the sample direction and the "up" direction (0, 1, 0)
inline float3 sampleCosineWeightedHemisphere(float2 u) {
	float phi = 2.0f * M_PI_F * u.x;
	
	float cos_phi;
	float sin_phi;
	sincos(phi, sin_phi, cos_phi);
	
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
float PDnrand( float2 n ) {
	return fract( sin(dot(n.xy, float2(12.9898, 78.233f)))* 43758.5453 );
}
float2 PDnrand2( float2 n ) {
	return fract( sin(dot(n.xy, float2(12.9898, 78.233f)))* float2(43758.5453, 28001.8384) );
}
float3 PDnrand3( float2 n ) {
	return fract( sin(dot(n.xy, float2(12.9898, 78.233f)))* float3(43758.5453, 28001.8384, 50849.4141 ) );
}
float4 PDnrand4( float2 n ) {
	return fract( sin(dot(n.xy, float2(12.9898, 78.233f)))* float4(43758.5453, 28001.8384, 50849.4141, 12996.89) );
}

// Convert uniform distribution into triangle-shaped distribution.
// https://www.shadertoy.com/view/4t2SDh
// Input is in range [0, 1]
// Output is in range [-1, 1], which is useful for dithering.
inline float2 uniformNoiseToTriangular(float2 n) {
	float2 orig = n*2.0-1.0;
	n = orig*rsqrt(abs(orig));
	n = max(-1.0,n);
	n = n-float2(sign(orig));
	return n;
}

ushort MetalClosestHitShader::subshaderCount() {
	return 2;
}

void MetalClosestHitShader::shader0(uint pathIndex,
									constant Uniforms & uniforms,
									const device Intersection& intersection,
									device Payload &payload,
									constant CSDataPerFrame& csDataPerFrame,
									constant CSData& csData
									)
{
	const uint maxRecursionDepth = 1; // inclusive
	
	if (payload.recursionDepth > maxRecursionDepth)
	{
		SkipSubshaders();
		return;
	}
	
	uint2 pixelPos = uint2(pathIndex % uniforms.width, pathIndex / uniforms.width);
	float2 pixelUV = (float2(pixelPos) + 0.5) / float2(uniforms.width, uniforms.height);
	
	//uint mask = triangleMasks[intersection.primitiveIndex];
	
	float3 position = interpolateVertexAttribute(csData.positions, csData.indices, intersection);
	float3 normal = normalize(interpolateVertexAttribute(csData.normals, csData.indices, intersection));
	float2 uv = interpolateVertexAttribute(csData.uvs, csData.indices, intersection);
	
	uint materialIndex = csData.materialIndices[intersection.primitiveIndex];
	texture2d<float, access::sample> albedoTexture = csData.materialTextures[csData.materialTextureIndices[5 * materialIndex]];
	
	constexpr sampler s(filter::linear, address::repeat);
	
	float4 textureVal = albedoTexture.sample(s, uv);
	
	float3 lightDir = csDataPerFrame.gSettings.mLightDirection;
	
	float3 rayOrigin = position + normal * 0.001;
	
	float3 surfaceAlbedo = (DENOISER_ENABLED && payload.recursionDepth == 0) ? 1.0 : textureVal.rgb;
#if DENOISER_ENABLED
	if (payload.recursionDepth == 0) {
		payload.surfaceAlbedo = textureVal.rgb;
	}
#endif
	
	float3 lightSample = 5.0 * saturate(dot(lightDir, normal)) * surfaceAlbedo * M_1_PI_F;
	payload.lightSample = lightSample * payload.throughput;
	
	// The sample has a PDF of cos(theta) / pi, so we only need to multiply 
	// by the albedo for Lambertian diffuse.
	payload.throughput *= surfaceAlbedo;
	
	float2 sample = uniformNoiseToTriangular(PDnrand2(pixelUV + payload.randomSeed + payload.recursionDepth)) * 0.5 + 0.5;
	float3 sampleDirLocal = sampleCosineWeightedHemisphere(sample);
	float3 sampleDir = alignHemisphereWithNormal(sampleDirLocal, normal);
	
	payload.indirectRayOrigin = rayOrigin;
	payload.indirectRayDirection = sampleDir;
	
	if (payload.recursionDepth + 1 > maxRecursionDepth)
	{
		SkipSubshaders(); // Don't dispatch an indirect ray.
	}
	
	Ray outRay;
	outRay.origin = rayOrigin;
	outRay.direction = lightDir;
	outRay.maxDistance = 10000;
	outRay.mask = 0xFF;
	
	TraceRay(outRay, /* missShaderIndex = */ 1, /* rayContributionToHitGroupIndex = */ 1);
	
	payload.recursionDepth += 1;
}

void MetalClosestHitShader::shader1(uint pathIndex,
									constant Uniforms & uniforms,
									const device Intersection& intersection,
									device Payload &payload,
									constant CSDataPerFrame& csDataPerFrame,
									constant CSData& csData
									)
{
	Ray outRay;
	outRay.origin = payload.indirectRayOrigin;
	outRay.direction = payload.indirectRayDirection;
	outRay.maxDistance = 10000;
	outRay.mask = 0xFF;
	
	TraceRay(outRay, /* missShaderIndex = */ 0);
}

// [numthreads(64, 1, 1)]
DEFINE_METAL_CLOSEST_HIT_SHADER(chs, 0)
DEFINE_METAL_CLOSEST_HIT_SHADER(chs_0, 1)
