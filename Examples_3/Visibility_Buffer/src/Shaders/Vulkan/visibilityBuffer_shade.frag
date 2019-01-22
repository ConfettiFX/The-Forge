#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif

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
// USERMACRO: VK_EXT_DESCRIPTOR_INDEXING_ENABLED [0,1]
// USERMACRO: VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED [0,1]

#extension GL_GOOGLE_include_directive : enable

#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
	#extension GL_EXT_nonuniform_qualifier : enable
#endif

#define REPEAT_TEN(base) CASE(base) CASE(base+1) CASE(base+2) CASE(base+3) CASE(base+4) CASE(base+5) CASE(base+6) CASE(base+7) CASE(base+8) CASE(base+9)
#define REPEAT_HUNDRED(base)	REPEAT_TEN(base) REPEAT_TEN(base+10) REPEAT_TEN(base+20) REPEAT_TEN(base+30) REPEAT_TEN(base+40) REPEAT_TEN(base+50) \
								REPEAT_TEN(base+60) REPEAT_TEN(base+70) REPEAT_TEN(base+80) REPEAT_TEN(base+90)

#define CASE_LIST CASE(0)	REPEAT_HUNDRED(1) REPEAT_HUNDRED(101) \
							REPEAT_TEN(201) REPEAT_TEN(211) REPEAT_TEN(221) REPEAT_TEN(231) REPEAT_TEN(241) \
							CASE(251) CASE(252) CASE(253) CASE(254) CASE(255)

#include "packing.h"
#include "shading.h"
#include "non_uniform_resource_index.h"

struct DerivativesOutput
{
	vec3 db_dx;
	vec3 db_dy;
};

// Computes the partial derivatives of a triangle from the projected screen space vertices
DerivativesOutput computePartialDerivatives(vec2 v[3])
{
	DerivativesOutput derivative;
	float d = 1.0 / determinant(mat2(v[2] - v[1], v[0] - v[1]));
	derivative.db_dx = vec3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
	derivative.db_dy = vec3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
	return derivative;
}

// Helper functions to interpolate vertex attributes at point 'd' using the partial derivatives
vec3 interpolateAttribute(mat3 attributes, vec3 db_dx, vec3 db_dy, vec2 d)
{
	vec3 attribute_x = attributes * db_dx;
	vec3 attribute_y = attributes * db_dy;
	vec3 attribute_s = attributes[0];
	
	return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

float interpolateAttribute(vec3 attributes, vec3 db_dx, vec3 db_dy, vec2 d)
{
	float attribute_x = dot(attributes, db_dx);
	float attribute_y = dot(attributes, db_dy);
	float attribute_s = attributes[0];
	
	return (attribute_s + d.x * attribute_x + d.y * attribute_y);
}

struct GradientInterpolationResults
{
	vec2 interp;
	vec2 dx;
	vec2 dy;
};

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
GradientInterpolationResults interpolateAttributeWithGradient(mat3x2 attributes, vec3 db_dx, vec3 db_dy, vec2 d, vec2 twoOverRes)
{
	vec3 attr0 = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
	vec3 attr1 = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
	vec2 attribute_x = vec2(dot(db_dx,attr0), dot(db_dx,attr1));
	vec2 attribute_y = vec2(dot(db_dy,attr0), dot(db_dy,attr1));
	vec2 attribute_s = attributes[0];
	
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

struct VertexPos
{
	float x, y, z;
};

#ifdef LINUX
struct VertexNormal
{
	float x, y, z;
};

struct VertexTangent
{
	float x, y, z;
};
#endif


layout(std430, set = 0, binding = 0) readonly buffer vertexPos
{
	VertexPos vertexPosData[];
};

layout(std430, set = 0, binding = 1) readonly buffer vertexTexCoord
{
#ifdef WINDOWS
	uint vertexTexCoordData[];
#elif defined(LINUX)
	vec2 vertexTexCoordData[];
#endif
};

layout(std430, set = 0, binding = 2) readonly buffer vertexNormal
{
#ifdef WINDOWS
	uint vertexNormalData[];
#elif defined(LINUX)
	VertexNormal vertexNormalData[];
#endif
};

layout(std430, set = 0, binding = 3) readonly buffer vertexTangent
{
#ifdef WINDOWS
	uint vertexTangentData[];
#elif defined(LINUX)
	VertexTangent vertexTangentData[];
#endif
};

layout(std430, set = 0, binding = 4) readonly buffer filteredIndexBuffer
{
	uint filteredIndexBufferData[];
};

layout(std430, set = 0, binding = 5) readonly buffer indirectMaterialBuffer
{
	uint indirectMaterialBufferData[];
};

layout(std430, set = 0, binding = 6) readonly buffer meshConstantsBuffer
{
	MeshConstants meshConstantsBufferData[];
};

layout(set = 0, binding = 7) uniform sampler textureSampler;
layout(set = 0, binding = 8) uniform sampler depthSampler;

// Per frame descriptors
layout(std430, set = 0, binding = 9) readonly buffer indirectDrawArgsBlock
{
	uint indirectDrawArgsData[];
} indirectDrawArgs[2];

layout(set = 0, binding = 10) uniform uniforms
{
	PerFrameConstants uniformsData;
};

layout(set = 0, binding = 11) restrict readonly buffer lights
{
	LightData lightsBuffer[];
};

layout(set = 0, binding = 12) restrict readonly buffer lightClustersCount
{
	uint lightClustersCountBuffer[];
};

layout(set = 0, binding = 13) restrict readonly buffer lightClusters
{
	uint lightClustersBuffer[];
};

#if(SAMPLE_COUNT > 1)
layout(set = 0, binding=14) uniform texture2DMS vbTex;
#else
layout(set = 0, binding=14) uniform texture2D vbTex;
#endif

#if USE_AMBIENT_OCCLUSION
layout(set = 0, binding = 15) uniform texture2D aoTex;
#endif
layout(set = 0, binding = 16) uniform texture2D shadowMap;

layout(set = 0, binding = 17) uniform texture2D diffuseMaps[MAX_TEXTURE_UNITS];
layout(set = 0, binding = 18 + MAX_TEXTURE_UNITS) uniform texture2D normalMaps[MAX_TEXTURE_UNITS];
layout(set = 0, binding = 18 + MAX_TEXTURE_UNITS * 2) uniform texture2D specularMaps[MAX_TEXTURE_UNITS];


layout(push_constant) uniform RootConstantDrawScene_Block
{
    vec4 lightColor;
	uint lightingMode;
	uint outputMode;
	vec4 CameraPlane; //x : near, y : far
}RootConstantDrawScene;


layout(location = 0) in vec2 iScreenPos;

layout(location = 0) out vec4 oColor;

// Pixel shader
void main()
{
	// Load Visibility Buffer raw packed float4 data from render target
#if(SAMPLE_COUNT > 1)
    vec4 visRaw = texelFetch(sampler2DMS(vbTex, depthSampler), ivec2(gl_FragCoord.xy), gl_SampleID);
#else
    vec4 visRaw = texelFetch(sampler2D(vbTex, depthSampler), ivec2(gl_FragCoord.xy), 0);
#endif
    // Unpack float4 render target data into uint to extract data
    uint alphaBit_drawID_triID = packUnorm4x8(visRaw);
    
	vec3 shadedColor = vec3(1.0f, 1.0f, 1.0f);

    // Early exit if this pixel doesn't contain triangle data
	// Early exit if this pixel doesn't contain triangle data
	if (alphaBit_drawID_triID != ~0)
	{
		// Extract packed data
		uint drawID = (alphaBit_drawID_triID >> 23) & 0x000000FF;
		uint triangleID = (alphaBit_drawID_triID & 0x007FFFFF);
		uint alpha1_opaque0 = (alphaBit_drawID_triID >> 31);

		// This is the start vertex of the current draw batch
		uint startIndex = (alpha1_opaque0 == 0) ? indirectDrawArgs[0].indirectDrawArgsData[drawID * 8 + 2] : indirectDrawArgs[1].indirectDrawArgsData[drawID * 8 + 2];

		uint triIdx0 = (triangleID * 3 + 0) + startIndex;
		uint triIdx1 = (triangleID * 3 + 1) + startIndex;
		uint triIdx2 = (triangleID * 3 + 2) + startIndex;

		uint index0 = filteredIndexBufferData[triIdx0];
		uint index1 = filteredIndexBufferData[triIdx1];
		uint index2 = filteredIndexBufferData[triIdx2];

		// Load vertex data of the 3 vertices
		vec3 v0pos = vec3(vertexPosData[index0].x, vertexPosData[index0].y, vertexPosData[index0].z);
		vec3 v1pos = vec3(vertexPosData[index1].x, vertexPosData[index1].y, vertexPosData[index1].z);
		vec3 v2pos = vec3(vertexPosData[index2].x, vertexPosData[index2].y, vertexPosData[index2].z);

		// Transform positions to clip space
		vec4 pos0 = (uniformsData.transform[VIEW_CAMERA].mvp * vec4(v0pos, 1));
		vec4 pos1 = (uniformsData.transform[VIEW_CAMERA].mvp * vec4(v1pos, 1));
		vec4 pos2 = (uniformsData.transform[VIEW_CAMERA].mvp * vec4(v2pos, 1));

		// Calculate the inverse of w, since it's going to be used several times
		vec3 one_over_w = 1.0 / vec3(pos0.w, pos1.w, pos2.w);

		// Project vertex positions to calcualte 2D post-perspective positions
		pos0 *= one_over_w[0];
		pos1 *= one_over_w[1];
		pos2 *= one_over_w[2];

		vec2 pos_scr[3] = { pos0.xy, pos1.xy, pos2.xy };

		// Compute partial derivatives. This is necessary to interpolate triangle attributes per pixel.
		DerivativesOutput derivativesOut = computePartialDerivatives(pos_scr);

		// Calculate delta vector (d) that points from the projected vertex 0 to the current screen point
		vec2 d = iScreenPos + -pos_scr[0];

		// Interpolate the 1/w (one_over_w) for all three vertices of the triangle
		// using the barycentric coordinates and the delta vector
		float w = 1.0 / interpolateAttribute(one_over_w, derivativesOut.db_dx, derivativesOut.db_dy, d);

		// Reconstruct the Z value at this screen point performing only the necessary matrix * vector multiplication
		// operations that involve computing Z
		float z = w * uniformsData.transform[VIEW_CAMERA].projection[2][2] + uniformsData.transform[VIEW_CAMERA].projection[3][2];

		// Calculate the world position coordinates:
		// First the projected coordinates at this point are calculated using In.screenPos and the computed Z value at this point.
		// Then, multiplying the perspective projected coordinates by the inverse view-projection matrix (invVP) produces world coordinates
		vec3 position = (uniformsData.transform[VIEW_CAMERA].invVP * vec4(iScreenPos * w, z, w)).xyz;

		// TEXTURE COORD INTERPOLATION
		// Apply perspective correction to texture coordinates
		mat3x2 texCoords =
		{
#ifdef WINDOWS
			unpack2Floats(vertexTexCoordData[index0]) * one_over_w[0],
			unpack2Floats(vertexTexCoordData[index1]) * one_over_w[1],
			unpack2Floats(vertexTexCoordData[index2]) * one_over_w[2]
#elif defined(LINUX)
			vertexTexCoordData[index0] * one_over_w[0],
			vertexTexCoordData[index1] * one_over_w[1],
			vertexTexCoordData[index2] * one_over_w[2]
#endif
		};

		// Interpolate texture coordinates and calculate the gradients for texture sampling with mipmapping support
		GradientInterpolationResults results = interpolateAttributeWithGradient(texCoords, derivativesOut.db_dx, derivativesOut.db_dy, d, uniformsData.twoOverRes);
	
        float linearZ = depthLinearization(z/w, RootConstantDrawScene.CameraPlane.x, RootConstantDrawScene.CameraPlane.y);
	    float mip = pow(pow(linearZ, 0.9f) * 5.0f, 1.5f);
	
	    vec2 texCoordDX = results.dx * w * mip;
	    vec2 texCoordDY = results.dy * w * mip;
	    vec2 texCoord = results.interp * w;

		// NORMAL INTERPOLATION
		// Apply perspective division to normals
#ifdef LINUX
		// Load normals
		vec3 v0normal = vec3(vertexNormalData[index0].x, vertexNormalData[index0].y, vertexNormalData[index0].z);
		vec3 v1normal = vec3(vertexNormalData[index1].x, vertexNormalData[index1].y, vertexNormalData[index1].z);
		vec3 v2normal = vec3(vertexNormalData[index2].x, vertexNormalData[index2].y, vertexNormalData[index2].z);
#endif
		mat3x3 normals =
		{
#ifdef WINDOWS
			decodeDir(unpackUnorm2x16(vertexNormalData[index0])) * one_over_w[0],
			decodeDir(unpackUnorm2x16(vertexNormalData[index1])) * one_over_w[1],
			decodeDir(unpackUnorm2x16(vertexNormalData[index2])) * one_over_w[2]
#elif defined(LINUX)
			v0normal * one_over_w[0],
			v1normal * one_over_w[1],
			v2normal * one_over_w[2]
#endif
		};

		vec3 normal = normalize(interpolateAttribute(normals, derivativesOut.db_dx, derivativesOut.db_dy, d));

		// TANGENT INTERPOLATION
		// Apply perspective division to tangents
#ifdef LINUX
		// Load tangents
		vec3 v0tan = vec3(vertexTangentData[index0].x, vertexTangentData[index0].y, vertexTangentData[index0].z);
		vec3 v1tan = vec3(vertexTangentData[index1].x, vertexTangentData[index1].y, vertexTangentData[index1].z);
		vec3 v2tan = vec3(vertexTangentData[index2].x, vertexTangentData[index2].y, vertexTangentData[index2].z);
#endif
		mat3x3 tangents =
		{
#ifdef WINDOWS
			decodeDir(unpackUnorm2x16(vertexTangentData[index0])) * one_over_w[0],
			decodeDir(unpackUnorm2x16(vertexTangentData[index1])) * one_over_w[1],
			decodeDir(unpackUnorm2x16(vertexTangentData[index2])) * one_over_w[2]
#elif defined(LINUX)
			v0tan * one_over_w[0],
			v1tan * one_over_w[1],
			v2tan * one_over_w[2]
#endif
		};

		vec3 tangent = normalize(interpolateAttribute(tangents, derivativesOut.db_dx, derivativesOut.db_dy, d));

		uint materialBaseSlot = BaseMaterialBuffer(alpha1_opaque0 == 1, 1);
		uint materialID = indirectMaterialBufferData[materialBaseSlot + drawID];

		vec4 normalMapRG;
		vec4 diffuseColor;
		vec4 specularData;
		bool isTwoSided;
#if VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED
		normalMapRG = textureGrad(sampler2D(normalMaps[materialID], textureSampler), texCoord, texCoordDX, texCoordDY);
		diffuseColor = textureGrad(sampler2D(diffuseMaps[materialID], textureSampler), texCoord, texCoordDX, texCoordDY);
		specularData = textureGrad(sampler2D(specularMaps[materialID], textureSampler), texCoord, texCoordDX, texCoordDY);
		isTwoSided = (alpha1_opaque0 == 1) && (meshConstantsBufferData[materialID].twoSided == 1);
#elif VK_EXT_DESCRIPTOR_INDEXING_ENABLED
		normalMapRG = textureGrad(sampler2D(normalMaps[nonuniformEXT(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		diffuseColor = textureGrad(sampler2D(diffuseMaps[nonuniformEXT(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		specularData = textureGrad(sampler2D(specularMaps[nonuniformEXT(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		isTwoSided = (alpha1_opaque0 == 1) && (meshConstantsBufferData[materialID].twoSided == 1);
#else
		switch (materialID)
		{
			// define an enum
#define CASE(id) case id: \
		normalMapRG = textureGrad(sampler2D(normalMaps[id], textureSampler), texCoord, texCoordDX, texCoordDY); \
		diffuseColor = textureGrad(sampler2D(diffuseMaps[id], textureSampler), texCoord, texCoordDX, texCoordDY); \
		specularData = textureGrad(sampler2D(specularMaps[id], textureSampler), texCoord, texCoordDX, texCoordDY); \
		isTwoSided = (alpha1_opaque0 == 1) && (meshConstantsBufferData[materialID].twoSided == 1); \
break;
			CASE_LIST
		}
#undef CASE
#endif

		vec3 reconstructedNormalMap;
		reconstructedNormalMap.xy = normalMapRG.ga * 2 - 1;
		reconstructedNormalMap.z = sqrt(1 - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));

		// Calculate vertex binormal from normal and tangent
		vec3 binormal = normalize(cross(tangent, normal));

		// Calculate pixel normal using the normal map and the tangent space vectors
		normal = reconstructedNormalMap.x * tangent + reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal;

		
		float Roughness = clamp(specularData.a, 0.05f, 0.99f);
		float Metallic = specularData.b;

		// Sample Diffuse color
		vec4 posLS = uniformsData.transform[VIEW_SHADOW].vp * vec4(position, 1);
#if USE_AMBIENT_OCCLUSION
		float ao = texelFetch(sampler2D(aoTex, depthSampler), ivec2(gl_FragCoord.xy), 0).r;
#else
		float ao = 1.0f;
#endif

    bool isBackFace = false;    

	vec3 ViewVec = normalize(uniformsData.camPos.xyz - position.xyz);
	
	//if it is backface
	//this should be < 0 but our mesh's edge normals are smoothed, badly
	
	if(isTwoSided && dot(normal, ViewVec) < 0.0)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}

	vec3 HalfVec = normalize(ViewVec - uniformsData.lightDir.xyz);
	vec3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = clamp(dot(normal, ViewVec), 0.0, 1.0);

	float NoL = dot(normal, -uniformsData.lightDir.xyz);	

	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : clamp(NoL, 0.0, 1.0));

	vec3 shadedColor;
	
	vec3 DiffuseColor = diffuseColor.xyz;
	
	float shadowFactor = 1.0f;

	float fLightingMode = clamp(float(RootConstantDrawScene.lightingMode), 0.0, 1.0);

	shadedColor = calculateIllumination(
		    normal,
		    ViewVec,
			HalfVec,
			ReflectVec,
			NoL,
			NoV,
			uniformsData.camPos.xyz,
			uniformsData.esmControl,
			uniformsData.lightDir.xyz,
			posLS,
			position,
			shadowMap,
			DiffuseColor,
			DiffuseColor,
			Roughness,
			Metallic,			
			depthSampler,
			isBackFace,
			fLightingMode,
			shadowFactor);

	shadedColor = shadedColor * RootConstantDrawScene.lightColor.rgb * RootConstantDrawScene.lightColor.a * NoL * ao;

		// Find the light cluster for the current pixel
		uvec2 clusterCoords = uvec2(floor((iScreenPos * 0.5f + 0.5f) * uvec2(LIGHT_CLUSTER_WIDTH, LIGHT_CLUSTER_HEIGHT)));

		uint numLightsInCluster = lightClustersCountBuffer[LIGHT_CLUSTER_COUNT_POS(clusterCoords.x, clusterCoords.y)];

		// Accumulate light contributions
		for (uint i = 0; i < numLightsInCluster; i++)
		{
			uint lightId = lightClustersBuffer[LIGHT_CLUSTER_DATA_POS(i, clusterCoords.x, clusterCoords.y)];
			
            shadedColor += pointLightShade(
            normal,
            ViewVec,
            HalfVec,
            ReflectVec,
            NoL,
            NoV,
            lightsBuffer[lightId].position.xyz,
            lightsBuffer[lightId].color.rgb,
            uniformsData.camPos.xyz,
            uniformsData.lightDir.xyz,
            posLS,
            position.xyz,
            DiffuseColor,
            DiffuseColor,
            Roughness,
            Metallic,		
            isBackFace,
            fLightingMode);
		}

		float ambientIntencity = 0.2f;
        vec3 ambient = diffuseColor.xyz * ambientIntencity;

        vec3 FinalColor = shadedColor + ambient;

        // Output final pixel color
        oColor = vec4(FinalColor, 1);
	}
    else
    oColor = vec4(shadedColor, 0);
	
}