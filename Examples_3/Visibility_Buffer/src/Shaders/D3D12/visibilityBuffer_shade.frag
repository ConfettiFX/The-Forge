/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

// USERMACRO: SAMPLE_COUNT [1,2,4]
// USERMACRO: USE_AMBIENT_OCCLUSION [0,1]

#include "packing.h"
#include "shading.h"

// This shader loads draw / triangle Id per pixel and reconstruct interpolated vertex data.
struct VSOutput
{
	float4 position : SV_Position;
	float2 screenPos : TEXCOORD0;
};

struct DerivativesOutput
{
	float3 db_dx;
	float3 db_dy;
};

// Computes the partial derivatives of a triangle from the projected screen space vertices
DerivativesOutput computePartialDerivatives(float2 v[3])
{
	DerivativesOutput output;
	float d = 1.0 / determinant(float2x2(v[2] - v[1], v[0] - v[1]));
	output.db_dx = float3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
	output.db_dy = float3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
	return output;
}

// Helper functions to interpolate vertex attributes at point 'd' using the partial derivatives
float3 interpolateAttribute(float3x3 attributes, float3 db_dx, float3 db_dy, float2 d)
{
	float3 attribute_x = mul(db_dx, attributes);
	float3 attribute_y = mul(db_dy, attributes);
	float3 attribute_s = attributes[0];

	return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

float interpolateAttribute(float3 attributes, float3 db_dx, float3 db_dy, float2 d)
{
	float attribute_x = dot(attributes, db_dx);
	float attribute_y = dot(attributes, db_dy);
	float attribute_s = attributes[0];

	return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

struct GradientInterpolationResults
{
	float2 interp;
	float2 dx;
	float2 dy;
};

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
GradientInterpolationResults interpolateAttributeWithGradient(float3x2 attributes, float3 db_dx, float3 db_dy, float2 d, float2 twoOverRes)
{
	float3 attr0 = float3(attributes[0].x, attributes[1].x, attributes[2].x);
	float3 attr1 = float3(attributes[0].y, attributes[1].y, attributes[2].y);
	float2 attribute_x = float2(dot(db_dx, attr0), dot(db_dx, attr1));
	float2 attribute_y = float2(dot(db_dy, attr0), dot(db_dy, attr1));
	float2 attribute_s = attributes[0];

	GradientInterpolationResults result;
	result.dx = attribute_x * twoOverRes.x;
	result.dy = attribute_y * twoOverRes.y;
	result.interp = (attribute_s + d.x * attribute_x + d.y * attribute_y);
	return result;
}

float depthLinearization(float depth, float near, float far)
{
	return (2.0 * near) / (far + near - depth * (far - near));
}

// Static descriptors
#if(SAMPLE_COUNT > 1)
Texture2DMS<float4, SAMPLE_COUNT> vbTex: register(t0);
#else
Texture2D vbTex: register(t0);
#endif


#if USE_AMBIENT_OCCLUSION
Texture2D<float> aoTex : register(t100);
#endif

Texture2D shadowMap : register(t101);


Texture2D diffuseMaps[] : register(t0, space1);
Texture2D normalMaps[] : register(t0, space2);
Texture2D specularMaps[] : register(t0, space3);

StructuredBuffer<float3> vertexPos: register(t10);
StructuredBuffer<uint> vertexTexCoord: register(t11);
StructuredBuffer<uint> vertexNormal: register(t12);
StructuredBuffer<uint> vertexTangent: register(t13);
StructuredBuffer<uint> filteredIndexBuffer: register(t14);
StructuredBuffer<uint> indirectMaterialBuffer: register(t15);
StructuredBuffer<MeshConstants> meshConstantsBuffer: register(t16);

// Per frame descriptors
StructuredBuffer<uint> indirectDrawArgs[2]: register(t17);

StructuredBuffer<LightData> lights: register(t19);
ByteAddressBuffer lightClustersCount: register(t20);
ByteAddressBuffer lightClusters: register(t21);

SamplerState textureSampler: register(s0);
SamplerState depthSampler: register(s1);


ConstantBuffer<PerFrameConstants> uniforms : register(b0);

cbuffer RootConstantDrawScene : register(b1)
{
	float4 lightColor;
	uint lightingMode;
	uint outputMode;
	float4 CameraPlane; //x : near, y : far
	
};

// Pixel shader
float4 main(VSOutput input, uint i : SV_SampleIndex) : SV_Target0
{
	// Load Visibility Buffer raw packed float4 data from render target
#if(SAMPLE_COUNT > 1)
	float4 visRaw = vbTex.Load(uint2(input.position.xy), i);
#else
	float4 visRaw = vbTex.Load(uint3(input.position.xy, 0));
#endif
	// Unpack float4 render target data into uint to extract data
	uint alphaBit_drawID_triID = packUnorm4x8(visRaw);

	// Early exit if this pixel doesn't contain triangle data
	if (alphaBit_drawID_triID == ~0)
	{
		discard;
	}

	// Extract packed data
	uint drawID = (alphaBit_drawID_triID >> 23) & 0x000000FF;
	uint triangleID = (alphaBit_drawID_triID & 0x007FFFFF);
	uint alpha1_opaque0 = (alphaBit_drawID_triID >> 31);

	// This is the start vertex of the current draw batch
	uint startIndex = indirectDrawArgs[NonUniformResourceIndex(alpha1_opaque0)][drawID * 8 + 3];

	uint triIdx0 = (triangleID * 3 + 0) + startIndex;
	uint triIdx1 = (triangleID * 3 + 1) + startIndex;
	uint triIdx2 = (triangleID * 3 + 2) + startIndex;

	uint index0 = filteredIndexBuffer[triIdx0];
	uint index1 = filteredIndexBuffer[triIdx1];
	uint index2 = filteredIndexBuffer[triIdx2];

	// Load vertex data of the 3 vertices
	float3 v0pos = vertexPos[index0];
	float3 v1pos = vertexPos[index1];
	float3 v2pos = vertexPos[index2];

	// Transform positions to clip space
	float4 pos0 = mul(uniforms.transform[VIEW_CAMERA].mvp, float4(v0pos, 1));
	float4 pos1 = mul(uniforms.transform[VIEW_CAMERA].mvp, float4(v1pos, 1));
	float4 pos2 = mul(uniforms.transform[VIEW_CAMERA].mvp, float4(v2pos, 1));

	// Calculate the inverse of w, since it's going to be used several times
	float3 one_over_w = 1.0 / float3(pos0.w, pos1.w, pos2.w);

	// Project vertex positions to calculate 2D post-perspective positions
	pos0 *= one_over_w[0];
	pos1 *= one_over_w[1];
	pos2 *= one_over_w[2];

	float2 pos_scr[3] = { pos0.xy, pos1.xy, pos2.xy };

	// Compute partial derivatives. This is necessary to interpolate triangle attributes per pixel.
	DerivativesOutput derivativesOut = computePartialDerivatives(pos_scr);

	// Calculate delta vector (d) that points from the projected vertex 0 to the current screen point
	float2 d = input.screenPos + -pos_scr[0];

	// Interpolate the 1/w (one_over_w) for all three vertices of the triangle
	// using the barycentric coordinates and the delta vector
	float w = 1.0 / interpolateAttribute(one_over_w, derivativesOut.db_dx, derivativesOut.db_dy, d);

	// Reconstruct the Z value at this screen point performing only the necessary matrix * vector multiplication
	// operations that involve computing Z
	float z = w * uniforms.transform[VIEW_CAMERA].projection[2][2] + uniforms.transform[VIEW_CAMERA].projection[2][3];

	// Calculate the world position coordinates:
	// First the projected coordinates at this point are calculated using In.screenPos and the computed Z value at this point.
	// Then, multiplying the perspective projected coordinates by the inverse view-projection matrix (invVP) produces world coordinates
	float3 position = mul(uniforms.transform[VIEW_CAMERA].invVP, float4(input.screenPos * w, z, w)).xyz;

	// TEXTURE COORD INTERPOLATION
	// Apply perspective correction to texture coordinates
	float3x2 texCoords =
	{
			unpack2Floats(vertexTexCoord.Load(index0)) * one_over_w[0],
			unpack2Floats(vertexTexCoord.Load(index1)) * one_over_w[1],
			unpack2Floats(vertexTexCoord.Load(index2)) * one_over_w[2]
	};

	// Interpolate texture coordinates and calculate the gradients for texture sampling with mipmapping support
	GradientInterpolationResults results = interpolateAttributeWithGradient(texCoords, derivativesOut.db_dx, derivativesOut.db_dy, d, uniforms.twoOverRes);
			
	float linearZ = depthLinearization(z/w, CameraPlane.x, CameraPlane.y);
	float mip = pow(pow(linearZ, 0.9f) * 5.0f, 1.5f);
	
	float2 texCoordDX = results.dx * w * mip;
	float2 texCoordDY = results.dy * w * mip;
	float2 texCoord = results.interp * w;
	

	/////////////LOAD///////////////////////////////
	// TANGENT INTERPOLATION
	// Apply perspective division to tangents
	float3x3 tangents =
	{
			decodeDir(unpackUnorm2x16(vertexTangent.Load(index0))) * one_over_w[0],
			decodeDir(unpackUnorm2x16(vertexTangent.Load(index1))) * one_over_w[1],
			decodeDir(unpackUnorm2x16(vertexTangent.Load(index2))) * one_over_w[2]
	};

	float3 tangent = normalize(interpolateAttribute(tangents, derivativesOut.db_dx, derivativesOut.db_dy, d));

	// BaseMaterialBuffer returns constant offset values
	// The following value defines the maximum amount of indirect draw calls that will be 
	// drawn at once. This value depends on the number of submeshes or individual objects 
	// in the scene. Changing a scene will require to change this value accordingly.
	// #define MAX_DRAWS_INDIRECT 300 
	//
	// These values are offsets used to point to the material data depending on the 
	// type of geometry and on the culling view
	// #define MATERIAL_BASE_ALPHA0 0
	// #define MATERIAL_BASE_NOALPHA0 MAX_DRAWS_INDIRECT
	// #define MATERIAL_BASE_ALPHA1 (MAX_DRAWS_INDIRECT*2)
	// #define MATERIAL_BASE_NOALPHA1 (MAX_DRAWS_INDIRECT*3)
	uint materialBaseSlot = BaseMaterialBuffer(alpha1_opaque0 == 1, VIEW_CAMERA);

	// potential results for materialBaseSlot + drawID are
	// 0 - 299 - shadow alpha
	// 300 - 599 - shadow no alpha
	// 600 - 899 - camera alpha
	uint materialID = indirectMaterialBuffer[materialBaseSlot + drawID];

	// CALCULATE PIXEL COLOR USING INTERPOLATED ATTRIBUTES
	// Reconstruct normal map Z from X and Y
	// "NonUniformResourceIndex" is a "pseudo" function see
	// http://asawicki.info/news_1608_direct3d_12_-_watch_out_for_non-uniform_resource_index.html
	
	float4 normalMapRG = normalMaps[NonUniformResourceIndex(materialID)].SampleGrad(textureSampler, texCoord, texCoordDX, texCoordDY);	

	float3 reconstructedNormalMap;
	reconstructedNormalMap.xy = normalMapRG.ga * 2 - 1;
	reconstructedNormalMap.z = sqrt(1 - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));

	// NORMAL INTERPOLATION
	// Apply perspective division to normals
	float3x3 normals =
	{
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index0))) * one_over_w[0],
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index1))) * one_over_w[1],
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index2))) * one_over_w[2]
	};
	float3 normal = normalize(interpolateAttribute(normals, derivativesOut.db_dx, derivativesOut.db_dy, d));

	// Calculate vertex binormal from normal and tangent
	float3 binormal = normalize(cross(tangent, normal));

	// Calculate pixel normal using the normal map and the tangent space vectors
	normal = reconstructedNormalMap.x * tangent + reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal;

	// Sample Diffuse color
	float4 posLS = mul(uniforms.transform[VIEW_SHADOW].vp, float4(position, 1));
	float4 diffuseColor = diffuseMaps[NonUniformResourceIndex(materialID)].SampleGrad(textureSampler, texCoord, texCoordDX, texCoordDY);
	float4 speculaColor = specularMaps[NonUniformResourceIndex(materialID)].SampleGrad(textureSampler, texCoord, texCoordDX, texCoordDY);
	
	float Roughness = clamp(speculaColor.a, 0.05f, 0.99f);
	float Metallic = speculaColor.b;

#if USE_AMBIENT_OCCLUSION
	float ao = aoTex.Load(uint3(input.position.xy, 0));
#else
	float ao = 1.0f;
#endif
	

	bool isTwoSided = (alpha1_opaque0 == 1) && meshConstantsBuffer[NonUniformResourceIndex(materialID)].twoSided;
	bool isBackFace = false;

	float3 ViewVec = normalize(uniforms.camPos.xyz - position.xyz);
	
	//if it is backface
	//this should be < 0 but our mesh's edge normals are smoothed, badly
	
	if(isTwoSided && dot(normal, ViewVec) < 0.0)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}

	float3 HalfVec = normalize(ViewVec - uniforms.lightDir.xyz);
	float3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = saturate(dot(normal, ViewVec));

	float NoL = dot(normal, -uniforms.lightDir.xyz);	

	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : saturate(NoL));

	float3 shadedColor;

	float3 F0 = speculaColor.xyz;
	float3 DiffuseColor = diffuseColor.xyz;
	
	float shadowFactor = 1.0f;

	float fLightingMode = saturate(float(lightingMode));

	shadedColor = calculateIllumination(
		    normal,
		    ViewVec,
			HalfVec,
			ReflectVec,
			NoL,
			NoV,
			uniforms.camPos.xyz,
			uniforms.esmControl,
			uniforms.lightDir.xyz,
			posLS,
			position,
			shadowMap,
			DiffuseColor,
			F0,
			Roughness,
			Metallic,			
			depthSampler,
			isBackFace,
			fLightingMode,
			shadowFactor);
			
	
	shadedColor = shadedColor * lightColor.rgb * lightColor.a * NoL * ao;
	
	// point lights
	// Find the light cluster for the current pixel
	uint2 clusterCoords = uint2(floor((input.screenPos * 0.5 + 0.5) * float2(LIGHT_CLUSTER_WIDTH, LIGHT_CLUSTER_HEIGHT)));

	uint numLightsInCluster = lightClustersCount.Load(LIGHT_CLUSTER_COUNT_POS(clusterCoords.x, clusterCoords.y) * 4);

	// Accumulate light contributions
	for (uint i = 0; i < numLightsInCluster; i++)
	{
		uint lightId = lightClusters.Load(LIGHT_CLUSTER_DATA_POS(i, clusterCoords.x, clusterCoords.y) * 4);

		shadedColor += pointLightShade(
		normal,
		ViewVec,
		HalfVec,
		ReflectVec,
		NoL,
		NoV,
		lights[lightId].position,
		lights[lightId].color,
		uniforms.camPos.xyz,
		uniforms.lightDir.xyz,
		posLS,
		position,
		DiffuseColor,
		F0,
		Roughness,
		Metallic,		
		isBackFace,
		fLightingMode);
	}


	float ambientIntencity = 0.2f;
	float3 ambient = diffuseColor.xyz * ambientIntencity;

	float3 FinalColor = shadedColor + ambient;

	return float4(FinalColor, 1.0);
}


