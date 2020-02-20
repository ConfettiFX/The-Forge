#version 450 core

/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "Packing.h"
#include "Shader_Defs.h"
#include "non_uniform_resource_index.h"
#include "shading.h"
#include "ASMShader_Defs.h"

#define SHADOW_TYPE_ESM				0
#define SHADOW_TYPE_ASM				1
#define SHADOW_TYPE_MESH_BAKED_SDF	2

struct DerivativesOutput
{
	vec3 db_dx;
	vec3 db_dy;
};

vec4 MulMat(mat4 lhs, vec4 rhs)
{
    vec4 dst;
	dst[0] = lhs[0][0]*rhs[0] + lhs[0][1]*rhs[1] + lhs[0][2]*rhs[2] + lhs[0][3]*rhs[3];
	dst[1] = lhs[1][0]*rhs[0] + lhs[1][1]*rhs[1] + lhs[1][2]*rhs[2] + lhs[1][3]*rhs[3];
	dst[2] = lhs[2][0]*rhs[0] + lhs[2][1]*rhs[1] + lhs[2][2]*rhs[2] + lhs[2][3]*rhs[3];
	dst[3] = lhs[3][0]*rhs[0] + lhs[3][1]*rhs[1] + lhs[3][2]*rhs[2] + lhs[3][3]*rhs[3];
    return dst;
}

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

layout(std430, UPDATE_FREQ_NONE, binding = 0) readonly buffer vertexPos
{
	VertexPos vertexPosData[];
};

layout(std430, UPDATE_FREQ_NONE, binding = 1) readonly buffer vertexTexCoord
{
	uint vertexTexCoordData[];
};

layout(std430, UPDATE_FREQ_NONE, binding = 2) readonly buffer vertexNormal
{
	uint vertexNormalData[];
};

layout(std430, UPDATE_FREQ_NONE, binding = 3) readonly buffer vertexTangent
{
	uint vertexTangentData[];
};

layout(std430, UPDATE_FREQ_PER_FRAME, binding = 4) readonly buffer filteredIndexBuffer
{
	uint filteredIndexBufferData[];
};

layout(std430, UPDATE_FREQ_PER_FRAME, binding = 5) readonly buffer indirectMaterialBuffer
{
	uint indirectMaterialBufferData[];
};

layout(std430, UPDATE_FREQ_NONE, binding = 6) readonly buffer meshConstantsBuffer
{
	MeshConstants meshConstantsBufferData[];
};

layout(UPDATE_FREQ_NONE, binding = 7) uniform sampler textureSampler;
layout(UPDATE_FREQ_NONE, binding = 8) uniform sampler clampMiplessLinearSampler;
layout(UPDATE_FREQ_NONE, binding = 9) uniform sampler clampMiplessNearSampler;
layout(UPDATE_FREQ_NONE, binding = 10) uniform sampler clampBorderNearSampler;
layout(UPDATE_FREQ_NONE, binding = 11) uniform sampler ShadowCmpSampler;

// Per frame descriptors
layout(std430, UPDATE_FREQ_PER_FRAME, binding = 12) readonly buffer indirectDrawArgsBlock
{
	uint indirectDrawArgsData[];
} indirectDrawArgs[2];


layout (UPDATE_FREQ_PER_FRAME, binding = 13) uniform objectUniformBlock
{
	mat4 WorldViewProjMat;
    mat4 WorldMat;
};

layout(column_major, UPDATE_FREQ_PER_FRAME, binding = 14) uniform cameraUniformBlock
{
    mat4 View;
    mat4 Project;
    mat4 ViewProject;
	mat4 InvView;
    mat4 InvProj;
    mat4 InvViewProject;
    vec4 mCameraPos;
    float mNear;
    float mFarNearDiff;
    float mFarNear;
    float paddingForAlignment0;
    vec2 mTwoOverRes;
    float _pad1;
    float _pad2;
    vec2 mWindowSize;
    float _pad3;
    float _pad4;
    vec4 mDeviceZToWorldZ;
};

layout(UPDATE_FREQ_PER_FRAME, binding = 15) uniform lightUniformBlock
{
	mat4 lightViewProj;
    vec4 lightPosition;
    vec4 lightColor;
	vec4 mLightUpVec;
	vec4 mTanLightAngleAndThresholdValue;
	vec3 mLightDir;
};

layout(UPDATE_FREQ_PER_FRAME, binding = 0) uniform ESMInputConstants
{
    float mEsmControl;
};

layout(column_major, UPDATE_FREQ_PER_FRAME, binding = 16) uniform renderSettingUniformBlock
{
    vec4 WindowDimension;
    int ShadowType;
};


layout(row_major, UPDATE_FREQ_PER_FRAME, binding = 17) uniform ASMUniformBlock
{
	mat4 mIndexTexMat;
	mat4 mPrerenderIndexTexMat;
	vec4 mSearchVector;
	vec4 mPrerenderSearchVector;
	vec4 mWarpVector;
	vec4 mPrerenderWarpVector;
	vec4 mMiscBool;
	float mPenumbraSize;
};

layout(UPDATE_FREQ_NONE, binding = 18) uniform texture2D vbPassTexture;

layout(UPDATE_FREQ_NONE, binding = 19) uniform texture2D SDFShadowTexture;

layout(UPDATE_FREQ_NONE, binding = 20) uniform texture2D IndexTexture[10];
layout(UPDATE_FREQ_NONE, binding = 21) uniform texture2D DepthAtlasTexture;
layout(UPDATE_FREQ_NONE, binding = 22) uniform texture2D DEMTexture;
layout(UPDATE_FREQ_NONE, binding = 23) uniform texture2D PrerenderLodClampTexture;
layout(UPDATE_FREQ_NONE, binding = 24) uniform texture2D ESMShadowTexture;

layout(UPDATE_FREQ_NONE, binding = 25) uniform texture2D diffuseMaps[MAX_TEXTURE_UNITS];
layout(UPDATE_FREQ_NONE, binding = 26) uniform texture2D normalMaps[MAX_TEXTURE_UNITS];
layout(UPDATE_FREQ_NONE, binding = 27) uniform texture2D specularMaps[MAX_TEXTURE_UNITS];


/*layout(push_constant) uniform RootConstantDrawScene_Block
{
    vec4 lightColor;
	uint lightingMode;
	uint outputMode;
	vec4 CameraPlane; //x : near, y : far
}RootConstantDrawScene;*/


layout(location = 0) in vec2 iScreenPos;

layout(location = 0) out vec4 oColor;



float calcESMShadowFactor(vec3 worldPos)
{
	vec4 posLS = lightViewProj * vec4(worldPos.xyz, 1.0);

	float shadowFactor = 0.0;
	const float esmControl = mEsmControl;

	(posLS /= vec4((posLS).w));
	((posLS).y *= float((-1)));
	((posLS).xy = (((posLS).xy * vec2(0.5)) + vec2(0.5, 0.5)));
	vec2 HalfGaps = vec2(0.00048828124, 0.00048828124);
	vec2 Gaps = vec2(0.0009765625, 0.0009765625);
	((posLS).xy += HalfGaps);


	if ((all(greaterThan((posLS).xy, vec2(0))) && all(lessThan((posLS).xy, vec2(1)))))
	{
		vec4 shadowDepthSample = vec4(0, 0, 0, 0);
		vec2 testPos =  vec2(posLS.xy) + vec2(1.f / 2048.f, 0.f);
		
		shadowDepthSample.x = textureLod(sampler2D(ESMShadowTexture, clampMiplessLinearSampler), posLS.xy, 0).r;
		shadowDepthSample.y =  textureLod(sampler2D(ESMShadowTexture, clampMiplessLinearSampler), vec2(posLS.xy) + vec2(1.f / 2048.f, 0.f), 0 ).r;
		shadowDepthSample.z =  textureLod(sampler2D(ESMShadowTexture, clampMiplessLinearSampler), posLS.xy + vec2(0.f, 1.f / 2048.f), 0).r;
		shadowDepthSample.w =  textureLod(sampler2D(ESMShadowTexture, clampMiplessLinearSampler), posLS.xy + vec2(1.f / 2048.f, 1.f / 2048.f), 0).r;
		float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
		shadowFactor = 2.0 - exp((posLS.z - avgShadowDepthSample) * mEsmControl);
		shadowFactor = clamp(shadowFactor, 0.0, 1.0);
	}

	return shadowFactor;
}


struct ASMFrustumDesc
{
	vec3 mIndexCoord;
	int mStartingMip;
};


float GetASMFadeInConstant(float w)
{
	return 2.0 * fract(abs(w));
}

float PCF(vec3 shadowMapCoord, vec2 kernelSize )
{
	const float depthBias = -0.0005f;
    vec2 tapOffset[9] =
    {
        vec2( 0.00, 0.00),
        vec2( 1.20, 0.00),
        vec2(-1.20, 0.00),
        vec2( 0.00, 1.20),
        vec2( 0.00,-1.20),
        vec2( 0.84, 0.84),
        vec2(-0.84, 0.84),
        vec2(-0.84,-0.84),
        vec2( 0.84,-0.84),
    };

    float tapWeight[9] =
    {
        0.120892,
        0.110858,
        0.110858,
        0.110858,
        0.110858,
        0.111050,
        0.111050,
        0.111050,
        0.111050,
    };

    float shadowFactor = 0;
    for( int i = 0; i < 9; ++i )
    {
        vec2 sampleCoord = vec2(shadowMapCoord.xy) + kernelSize * tapOffset[i];

		shadowFactor += tapWeight[i] * textureLod( 
			sampler2DShadow( DepthAtlasTexture, ShadowCmpSampler), 
				vec3(sampleCoord.x, sampleCoord.y, shadowMapCoord.z - depthBias), 0.0 );
    }
	return shadowFactor;
}


vec4 SampleFrustumIndirectionTexture(ASMFrustumDesc frustumDesc, float mip)
{
	
	float lerpVal = fract(mip);
	uint floorMip = uint(floor(mip));
	
#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
	vec4 indirectionCoordData =  textureLod(  sampler2D(
		IndexTexture[ nonuniformEXT( frustumDesc.mStartingMip + floorMip ) ], clampBorderNearSampler), 
			vec2(frustumDesc.mIndexCoord.xy), 0);
#else
	vec4 indirectionCoordData =  textureLod(  sampler2D(
		IndexTexture[ (frustumDesc.mStartingMip + floorMip ) ], clampBorderNearSampler), 
			vec2(frustumDesc.mIndexCoord.xy), 0);
#endif

	if(lerpVal > 0.0)
	{
#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
		vec4 upperIndirectionCoordData = textureLod(  sampler2D(
			IndexTexture[nonuniformEXT(frustumDesc.mStartingMip + floorMip + 1)], clampBorderNearSampler), 
			vec2(frustumDesc.mIndexCoord.xy), 0);
#else
	vec4 upperIndirectionCoordData = textureLod(  sampler2D(
		IndexTexture[(frustumDesc.mStartingMip + floorMip + 1)], clampBorderNearSampler), 
		vec2(frustumDesc.mIndexCoord.xy), 0);
#endif

		indirectionCoordData = mix(indirectionCoordData, upperIndirectionCoordData, lerpVal);
	}
	
	return indirectionCoordData;
	
}


vec3 GetASMTileCoord(vec3 indexCoord, vec4 indirectionCoordData)
{


	indexCoord.xy = floor( abs( indirectionCoordData.w ) ) * 
		ASMOneOverDepthAtlasSize * indexCoord.xy + indirectionCoordData.xy;

    indexCoord.z = indexCoord.z - indirectionCoordData.z;
    return indexCoord;
}


vec3 GetIndirectionTextureCoord(mat4 indexTexMat, vec3 worldPos)
{
	return MulMat(indexTexMat, vec4(worldPos, 1.0)).xyz;
}


bool GetClampedDepthDifference(ASMFrustumDesc frustumDesc, out float depthDiff)
{

	const int DEM_LOD = gs_ASMMaxRefinement;
	
	vec4 indirectionCoordData = SampleFrustumIndirectionTexture(frustumDesc, DEM_LOD);

	if(indirectionCoordData.w > 0)
	{
		vec3 asmTileCoord = GetASMTileCoord(frustumDesc.mIndexCoord, indirectionCoordData);

		float demDepth = textureLod(  sampler2D(DEMTexture, 
			clampMiplessLinearSampler), asmTileCoord.xy, 0).x;
		depthDiff = clamp( (demDepth  - asmTileCoord.z), 0.f, 1.f);
		return true;
	}
	return false;
}


float SampleASMShadowMap(float mip, float kernelSize, ASMFrustumDesc frustumDesc, inout float fadeInFactor )
{
	vec4 IndexT = SampleFrustumIndirectionTexture(frustumDesc, mip);
	float shadowFactor = 0.0;

	//TODO
	vec2 newKernelSize = vec2( kernelSize / gs_ASMDepthAtlasTextureWidth, 
		kernelSize / gs_ASMDepthAtlasTextureHeight );
	
	if(IndexT.w != 0)
	{
		fadeInFactor = GetASMFadeInConstant(IndexT.w);
		vec3 depthTileCoord = GetASMTileCoord(frustumDesc.mIndexCoord, IndexT);
		shadowFactor = PCF(depthTileCoord, newKernelSize);
	}

	return shadowFactor;
}

float GetASMShadowFactor(float mip, float kernelSize, ASMFrustumDesc frustumDesc, ASMFrustumDesc preRenderFrustumDesc)
{
	float shadowFactor = 1.0;
	float fadeInFactor = 1.0;

	if(mMiscBool.x == 1.0)
	{
		float lodClamp = textureLod( sampler2D(PrerenderLodClampTexture,
			clampBorderNearSampler), preRenderFrustumDesc.mIndexCoord.xy, 0).x;
		lodClamp = lodClamp * float(gs_ASMMaxRefinement);
				
		shadowFactor = SampleASMShadowMap(max(mip, lodClamp), kernelSize, preRenderFrustumDesc, fadeInFactor);
	}


	if(fadeInFactor > 0)
	{
		float otherShadowFactor = 0.0;
		float otherFadeInFactor = 1.0;

		otherShadowFactor = SampleASMShadowMap(mip, kernelSize, 
			frustumDesc, otherFadeInFactor);

		shadowFactor = mix(shadowFactor, otherShadowFactor, fadeInFactor);
	}
	return shadowFactor;
}


float GetBlockerDistance(ASMFrustumDesc frustumDesc, vec3 worldPos, 
	mat4 indexTexMat, vec3 blockerSearchDir, float mip)
{
	vec3 indexCoord = GetIndirectionTextureCoord(indexTexMat, worldPos);
#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
	vec4 indirectionData = textureLod(sampler2D(IndexTexture[  nonuniformEXT(frustumDesc.mStartingMip + uint(mip)) ],
		clampBorderNearSampler),vec2(indexCoord.xy), 0).rgba;
#else
	vec4 indirectionData = textureLod(sampler2D(IndexTexture[ (frustumDesc.mStartingMip + uint(mip)) ],
		clampBorderNearSampler),vec2(indexCoord.xy), 0).rgba;
#endif

	vec3 tileCoord = GetASMTileCoord(indexCoord, indirectionData );

	vec2 tileCoordBoundsMin = floor( tileCoord.xy * ASMDepthAtlasSizeOverDepthTileSize - ASMHalfOverDepthTileSize ) * ASMDEMTileSizeOverDEMAtlasSize + ASMDEMTileCoord;
	vec2 tileCoordBoundsMax = tileCoordBoundsMin + ASMDEMTileSize;

	float num = 0;
	float sum = 0;

	if(indirectionData.w != 0)
	{
		vec3 sampleCoord = tileCoord;
		sampleCoord.z += 0.5 * blockerSearchDir.z * 0.1;

		for(int i = 0; i < 9; ++i)
		{
			float demValue = textureLod( sampler2D(DEMTexture, clampMiplessLinearSampler),
				clamp(sampleCoord.xy, tileCoordBoundsMin, tileCoordBoundsMax), 0).r;

			if(demValue >= sampleCoord.z)
			{
				sum += demValue;
				num += 1.0;
			}
			sampleCoord += blockerSearchDir * 0.1;
		}
	
	}
	float blockerDepth = num > 0 ? sum * (1.f / num) : 1.0;
	return clamp((blockerDepth - tileCoord.z), 0, 1.0) * gs_ASMTileFarPlane;
}

float SampleShadowFactor(vec3 worldPos)
{
	float shadowFactor = 0.0;
	float fadeInFactor = 0.0;
	float demLOD = gs_ASMMaxRefinement;
	ASMFrustumDesc frustumDesc;
	frustumDesc.mStartingMip = 0;
	

	ASMFrustumDesc preRenderFrustumDesc;
	preRenderFrustumDesc.mStartingMip = 5;

	float blockerDistance = 0.0;
	float preRenderBlockerDistance = 0.0;

	if(mMiscBool.y == 1.0)
	{
		blockerDistance = GetBlockerDistance(frustumDesc, worldPos, 
			mIndexTexMat, mSearchVector.xyz, demLOD);

		preRenderBlockerDistance = GetBlockerDistance(preRenderFrustumDesc, worldPos, 
			mPrerenderIndexTexMat, mPrerenderSearchVector.xyz, demLOD);
	}


	frustumDesc.mIndexCoord = GetIndirectionTextureCoord(
		mIndexTexMat, worldPos + blockerDistance * mWarpVector.xyz);
	preRenderFrustumDesc.mIndexCoord = GetIndirectionTextureCoord(
		mPrerenderIndexTexMat, worldPos + preRenderBlockerDistance * mPrerenderWarpVector.xyz);

	float depthDiff = 0.0;
	if(GetClampedDepthDifference(frustumDesc, depthDiff))
	{
		float penumbraSizeFactor = clamp((mPenumbraSize * depthDiff - 0.05), 0.0, 1.0);
		float kernelSize = clamp((depthDiff * 10.0 + 0.5), 0.0, 1.0);

		float lod = penumbraSizeFactor * gs_ASMMaxRefinement;
		float mip = floor(lod);
		
			
		
		shadowFactor = GetASMShadowFactor(mip, kernelSize, 
			frustumDesc, preRenderFrustumDesc);

		if(penumbraSizeFactor > 0.0 && penumbraSizeFactor < 1.0)
		{
			float upperShadowFactor = GetASMShadowFactor(mip + 1, kernelSize, 
				frustumDesc, preRenderFrustumDesc);

			shadowFactor = mix(shadowFactor, upperShadowFactor, lod - mip);
		}

	}
	return 1.0 - shadowFactor;
}


// Pixel shader
void main()
{
    vec4 visRaw = texelFetch(sampler2D(vbPassTexture, clampMiplessLinearSampler), ivec2(gl_FragCoord.xy), 0);

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
		vec4 pos0 = (WorldViewProjMat * vec4(v0pos, 1));
		vec4 pos1 = (WorldViewProjMat * vec4(v1pos, 1));
		vec4 pos2 = (WorldViewProjMat * vec4(v2pos, 1));

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
		float z = w * Project[2][2] + Project[3][2];

		// Calculate the world position coordinates:
		// First the projected coordinates at this point are calculated using In.screenPos and the computed Z value at this point.
		// Then, multiplying the perspective projected coordinates by the inverse view-projection matrix (invVP) produces world coordinates
		vec3 position = (InvViewProject * vec4(iScreenPos * w, z, w)).xyz;

		// TEXTURE COORD INTERPOLATION
		// Apply perspective correction to texture coordinates
		mat3x2 texCoords =
		{
			unpack2Floats(vertexTexCoordData[index0]) * one_over_w[0],
			unpack2Floats(vertexTexCoordData[index1]) * one_over_w[1],
			unpack2Floats(vertexTexCoordData[index2]) * one_over_w[2]
		};

		// Interpolate texture coordinates and calculate the gradients for texture sampling with mipmapping support
		GradientInterpolationResults results = interpolateAttributeWithGradient(texCoords, derivativesOut.db_dx, derivativesOut.db_dy, d, mTwoOverRes);
	
		float farValue = mFarNearDiff + mNear;
		float linearZ = depthLinearization(z / w, mNear, farValue);
	    float mip = pow(pow(linearZ, 0.9f) * 5.0f, 1.5f);
	
	    vec2 texCoordDX = results.dx * w * mip;
	    vec2 texCoordDY = results.dy * w * mip;
	    vec2 texCoord = results.interp * w;

		// NORMAL INTERPOLATION
		// Apply perspective division to normals
		mat3x3 normals =
		{
			decodeDir(unpackUnorm2x16(vertexNormalData[index0])) * one_over_w[0],
			decodeDir(unpackUnorm2x16(vertexNormalData[index1])) * one_over_w[1],
			decodeDir(unpackUnorm2x16(vertexNormalData[index2])) * one_over_w[2]
		};

		vec3 normal = normalize(interpolateAttribute(normals, derivativesOut.db_dx, derivativesOut.db_dy, d));

		// TANGENT INTERPOLATION
		// Apply perspective division to tangents
		mat3x3 tangents =
		{
			decodeDir(unpackUnorm2x16(vertexTangentData[index0])) * one_over_w[0],
			decodeDir(unpackUnorm2x16(vertexTangentData[index1])) * one_over_w[1],
			decodeDir(unpackUnorm2x16(vertexTangentData[index2])) * one_over_w[2]
		};

		vec3 tangent = normalize(interpolateAttribute(tangents, derivativesOut.db_dx, derivativesOut.db_dy, d));

		uint materialBaseSlot = BaseMaterialBuffer(alpha1_opaque0 == 1, 1);
		uint materialID = indirectMaterialBufferData[materialBaseSlot + drawID];
		bool isTwoSided = (alpha1_opaque0 == 1) && (meshConstantsBufferData[materialID].twoSided == 1);

		vec4 normalMapRG;
		vec4 diffuseColor;
		vec4 specularData;
#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
		normalMapRG = textureGrad(sampler2D(normalMaps[nonuniformEXT(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		diffuseColor = textureGrad(sampler2D(diffuseMaps[nonuniformEXT(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		specularData = textureGrad(sampler2D(specularMaps[nonuniformEXT(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
#elif VK_FEATURE_TEXTURE_ARRAY_DYNAMIC_INDEXING_ENABLED
		normalMapRG = textureGrad(sampler2D(normalMaps[(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		diffuseColor = textureGrad(sampler2D(diffuseMaps[(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
		specularData = textureGrad(sampler2D(specularMaps[(materialID)], textureSampler), texCoord, texCoordDX, texCoordDY);
#else
		switch (materialID)
		{
			// define an enum
#define CASE(id) case id: \
		normalMapRG = textureGrad(sampler2D(normalMaps[id], textureSampler), texCoord, texCoordDX, texCoordDY); \
		diffuseColor = textureGrad(sampler2D(diffuseMaps[id], textureSampler), texCoord, texCoordDX, texCoordDY); \
		specularData = textureGrad(sampler2D(specularMaps[id], textureSampler), texCoord, texCoordDX, texCoordDY); \
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

		
		float shadowFactor = 1.0;

		if(ShadowType == SHADOW_TYPE_ASM)
		{
			shadowFactor = SampleShadowFactor(position);
		}
		
		else if(ShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			shadowFactor = texelFetch(
				sampler2D(SDFShadowTexture, clampMiplessNearSampler), ivec2(gl_FragCoord.xy), 0).x;
		}
		else if(ShadowType == SHADOW_TYPE_ESM)
		{
			shadowFactor = calcESMShadowFactor(position);
		}

		float Roughness = clamp(specularData.a, 0.05f, 0.99f);
		float Metallic = specularData.b;

		bool isBackFace = false;    

		vec3 camPos = mCameraPos.xyz;

		vec3 ViewVec = normalize(camPos - position.xyz);
	
		//if it is backface
		//this should be < 0 but our mesh's edge normals are smoothed, badly
	
		if(isTwoSided && dot(normal, ViewVec) < 0.0)
		{	
			//flip normal
			normal = -normal;
			isBackFace = true;
		}
		vec3 lightDir = -mLightDir.xyz;

		vec3 HalfVec = normalize(ViewVec - lightDir.xyz);
		vec3 ReflectVec = reflect(-ViewVec, normal);
		float NoV = clamp(dot(normal, ViewVec), 0.0, 1.0);

		float NoL = dot(normal, -lightDir.xyz);	

		// Deal with two faced materials
		NoL = (isTwoSided ? abs(NoL) : clamp(NoL, 0.0, 1.0));

		vec3 shadedColor;
		vec3 DiffuseColor = diffuseColor.xyz;
		float fLightingMode = 1.0;

		shadedColor = calculateIllumination(
				normal,
				ViewVec,
				HalfVec,
				ReflectVec,
				NoL,
				NoV,
				camPos,
				lightDir.xyz,
				position,
				DiffuseColor,
				DiffuseColor,
				Roughness,
				Metallic,			
				isBackFace,
				fLightingMode,
				shadowFactor);

		shadedColor = shadedColor * lightColor.rgb * lightColor.a * NoL;

		float ambientIntencity = 0.2f;
        vec3 ambient = diffuseColor.xyz * ambientIntencity;

        vec3 FinalColor = shadedColor + ambient;

        // Output final pixel color
        oColor = vec4(FinalColor, 1);
	
	}
	else
	{    
		oColor = vec4(shadedColor, 0);
	}
}