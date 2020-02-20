RaytracingAccelerationStructure		gRtScene	: register(t0);

#define TOTAL_IMGS 84
#define M_PI_F ((float)3.141592653589793)
#define M_1_PI_F ((float)0.318309886183790)

ByteAddressBuffer indices : register(t1);
ByteAddressBuffer positions : register(t2);
ByteAddressBuffer normals : register(t3);
ByteAddressBuffer uvs : register(t4);

ByteAddressBuffer materialIndices : register(t5);
ByteAddressBuffer materialTextureIndices : register(t6);
Texture2D<float4> materialTextures[TOTAL_IMGS] : register(t7);

SamplerState linearSampler : register(s0);

cbuffer gSettings : register(b0, space1)
{
    float4x4 mCameraToWorld;
	float2 mZ1PlaneSize;
	float mProjNear;
	float mProjFarMinusNear;
    float3 mLightDirection;
	float mRandomSeed;
	float2 mSubpixelJitter;
	uint mFrameIndex;
	uint mFramesSinceCameraMove;
};

struct RayPayload
{
	float3 radiance;
	uint recursionDepth;
};

struct ShadowRayPayload
{
	bool miss;
};

struct IntersectionAttribs
{
	float2 baryCrd;
};

// Uses the inversion method to map two uniformly random numbers to a three dimensional
// unit hemisphere where the probability of a given sample is proportional to the cosine
// of the angle between the sample direction and the "up" direction (0, 1, 0)
float3 sampleCosineWeightedHemisphere(float2 u) {
	float phi = 2.0f * M_PI_F * u.x;
	
	float sin_phi, cos_phi;
	sincos(phi, sin_phi, cos_phi);
	
	float cos_theta = sqrt(u.y);
	float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
	
	return float3(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi);
}

// Aligns a direction on the unit hemisphere such that the hemisphere's "up" direction
// (0, 1, 0) maps to the given surface normal direction
float3 alignHemisphereWithNormal(float3 sample, float3 normal) {
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
	return frac( sin(dot(n.xy, float2(12.9898, 78.233f)))* 43758.5453 );
}
float2 PDnrand2( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898, 78.233f)))* float2(43758.5453, 28001.8384) );
}
float3 PDnrand3( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898, 78.233f)))* float3(43758.5453, 28001.8384, 50849.4141 ) );
}
float4 PDnrand4( float2 n ) {
	return frac( sin(dot(n.xy, float2(12.9898, 78.233f)))* float4(43758.5453, 28001.8384, 50849.4141, 12996.89) );
}

// Convert uniform distribution into triangle-shaped distribution.
// https://www.shadertoy.com/view/4t2SDh
// Input is in range [0, 1]
// Output is in range [-1, 1], which is useful for dithering.
float2 uniformNoiseToTriangular(float2 n) {
	float2 orig = n*2.0-1.0;
	n = orig*rsqrt(abs(orig));
	n = max(-1.0,n);
	n = n-float2(sign(orig));
	return n;
}

[shader("closesthit")]
void chs(inout RayPayload payload : SV_RayPayload, IntersectionAttribs attribs : SV_IntersectionAttributes)
{
	const uint maxRecursionDepth = 1; // inclusive

	if (payload.recursionDepth > maxRecursionDepth)
	{
		return;
	}

	uint2 pixelPos = DispatchRaysIndex().xy;
	float2 pixelUV = (float2(pixelPos) + 0.5) / float2(DispatchRaysDimensions().xy);
	
	// Barycentric coordinates sum to one
	float3 uvw;
	uvw.yz = attribs.baryCrd;
	uvw.x = 1.0f - uvw.y - uvw.z;

	uint triangleIndex = PrimitiveIndex();
	
	uint4 i012 = indices.Load4((3 * triangleIndex) << 2);
	uint i0 = i012[0];
	uint i1 = i012[1];
	uint i2 = i012[2];

	float4 position012[3] = {
		asfloat(positions.Load4((i0 * 3) << 2)),
		asfloat(positions.Load4((i1 * 3) << 2)),
		asfloat(positions.Load4((i2 * 3) << 2))
	};
	float4 normal012[3] = {
		asfloat(normals.Load4((i0 * 3) << 2)),
		asfloat(normals.Load4((i1 * 3) << 2)),
		asfloat(normals.Load4((i2 * 3) << 2))
	};
	float2 uv012[3] = {
		asfloat(uvs.Load2((i0 * 1) << 3)),
		asfloat(uvs.Load2((i1 * 1) << 3)),
		asfloat(uvs.Load2((i2 * 1) << 3))
	};
	float3 position = uvw.x * position012[0].xyz + uvw.y * position012[1].xyz + uvw.z * position012[2].xyz;
	float3 normal = normalize(uvw.x * normal012[0].xyz + uvw.y * normal012[1].xyz + uvw.z * normal012[2].xyz);
	float2 uv = uvw.x * uv012[0] + uvw.y * uv012[1] + uvw.z * uv012[2];

	uint materialIndex = materialIndices.Load(triangleIndex << 2);
	Texture2D<float4> albedoTexture = materialTextures[materialTextureIndices.Load((5 * materialIndex) << 2)];

	float3 surfaceAlbedo = albedoTexture.SampleLevel(linearSampler, uv, 0).rgb;

	float3 rayOrigin = position + normal * 0.001;

	float3 lightDir = mLightDirection;

	RayDesc shadowRay;
	shadowRay.Origin = rayOrigin;
	shadowRay.Direction = lightDir;
	shadowRay.TMin = 0.0;
	shadowRay.TMax = 10000;
	
	ShadowRayPayload shadowPayload;
	shadowPayload.miss = false;
	TraceRay(gRtScene, 0, 0xFF, 1, 0, 1, shadowRay, shadowPayload);

	if (shadowPayload.miss)
	{
		float3 lightSample = 5.0 * saturate(dot(lightDir, normal)) * surfaceAlbedo * M_1_PI_F;
		payload.radiance += lightSample;
	}

	float2 sample = uniformNoiseToTriangular(PDnrand2(pixelUV + mRandomSeed + payload.recursionDepth)) * 0.5 + 0.5;
	float3 sampleDirLocal = sampleCosineWeightedHemisphere(sample);
	float3 sampleDir = alignHemisphereWithNormal(sampleDirLocal, normal);
	
	if (payload.recursionDepth < maxRecursionDepth)
	{
		RayDesc indirectRay;
		indirectRay.Origin = rayOrigin;
		indirectRay.Direction = sampleDir;
		indirectRay.TMin = 0.0;
		indirectRay.TMax = 10000;

		RayPayload indirectRayPayload;
		indirectRayPayload.recursionDepth = payload.recursionDepth + 1;
		indirectRayPayload.radiance = 0.0;
		TraceRay(gRtScene, 0, 0xFF, 0, 0, 0, indirectRay, indirectRayPayload);

		// The indirect sample has a PDF of cos(theta) / pi, so we only need to multiply 
		// by the albedo for Lambertian diffuse.
		payload.radiance += indirectRayPayload.radiance * surfaceAlbedo;
	}
}
