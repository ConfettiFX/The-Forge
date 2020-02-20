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


#include "Packing.h"
#include "Shader_Defs.h"
#include "shading.h"
#include "ASMShader_Defs.h"

#define SHADOW_TYPE_ESM				0
#define SHADOW_TYPE_ASM				1
#define SHADOW_TYPE_MESH_BAKED_SDF	2

cbuffer lightUniformBlock : register(b1, UPDATE_FREQ_PER_FRAME)
{
    float4x4 mLightViewProj;
    float4 lightPosition;
    float4 lightColor;
	float4 mLightUpVec;
	float4 mTanLightAngleAndThresholdValue;
	float3 mLightDir;
};

cbuffer cameraUniformBlock : register(b3, UPDATE_FREQ_PER_FRAME)
{
    float4x4 View;
    float4x4 Project;
    float4x4 ViewProject ;
    row_major float4x4 InvView ;
	float4x4  InvProj;
	float4x4  InvViewProject;
	float4  mCameraPos;
	float mNear;
	float mFarNearDiff;
	float mFarNear;
	float paddingForAlignment0;
	float2 mTwoOverRes;
};

cbuffer ASMUniformBlock : register(b2, UPDATE_FREQ_PER_FRAME)
{
	float4x4 mIndexTexMat;
	float4x4 mPrerenderIndexTexMat;
	float4 mSearchVector;
	float4 mPrerenderSearchVector;
	float4 mWarpVector;
	float4 mPrerenderWarpVector;
	float4 mMiscBool;
	float mPenumbraSize;
};

cbuffer objectUniformBlock : register(b0, UPDATE_FREQ_PER_FRAME)
{
    float4x4 WorldViewProjMat;
    float4x4 WorldMat;
};

cbuffer renderSettingUniformBlock : register(b4, UPDATE_FREQ_PER_FRAME)
{
    float4 WindowDimension;
    int ShadowType;
};

cbuffer ESMInputConstants : register(b5, UPDATE_FREQ_PER_FRAME)
{
    float mEsmControl;
};


/*Texture2D<float4> IndexTexture[10] : register(t0, UPDATE_FREQ_PER_DRAW);
Texture2D<float> DepthAtlasTexture : register(t10, UPDATE_FREQ_PER_DRAW);
Texture2D<float> DEMTexture : register(t11, UPDATE_FREQ_PER_DRAW);
Texture2D<float> PrerenderLodClampTexture : register(t12, UPDATE_FREQ_PER_DRAW);
Texture2D<float> ESMShadowTexture : register(t13, UPDATE_FREQ_PER_DRAW);
Texture2D<float2> SDFShadowTexture : register(t14, UPDATE_FREQ_PER_DRAW);*/


Texture2D<float4> IndexTexture[10] : register(t770);
Texture2D<float> DepthAtlasTexture : register(t780);
Texture2D<float> DEMTexture : register(t781);
Texture2D<float> PrerenderLodClampTexture : register(t782);
Texture2D<float> ESMShadowTexture : register(t783);
Texture2D<float2> SDFShadowTexture : register(t784);

Texture2D<float4> vbPassTexture : register(t0);

ByteAddressBuffer vertexPos: register(t16);
ByteAddressBuffer vertexTexCoord: register(t17);
ByteAddressBuffer vertexNormal: register(t18);
ByteAddressBuffer vertexTangent: register(t19);
ByteAddressBuffer filteredIndexBuffer: register(t20, UPDATE_FREQ_PER_FRAME);
ByteAddressBuffer indirectMaterialBuffer: register(t21, UPDATE_FREQ_PER_FRAME);
StructuredBuffer<MeshConstants> meshConstantsBuffer: register(t22);

// Per frame descriptors
StructuredBuffer<uint> indirectDrawArgs[2]: register(t23, UPDATE_FREQ_PER_FRAME);

Texture2D diffuseMaps[MAX_TEXTURE_UNITS] : register(t0, space4);
Texture2D normalMaps[MAX_TEXTURE_UNITS] : register(t257, space5);
Texture2D specularMaps[MAX_TEXTURE_UNITS] : register(t514, space6);

SamplerComparisonState ShadowCmpSampler : register(s4);
SamplerState clampBorderNearSampler : register(s3);
SamplerState clampMiplessLinearSampler : register(s2);
SamplerState clampMiplessNearSampler : register(s1);

SamplerState textureSampler : register(s0);

struct ASMFrustumDesc
{
	float3 mIndexCoord;
	int mStartingMip;
};


struct PsIn
{
	float4 Position : SV_Position;
	float2 ScreenPos : TEXCOORD0;
};

struct PsOut
{
    float4 FinalColor : SV_Target0;
};



float calcESMShadowFactor(float3 worldPos)
{
	float4 posLS = mul(mLightViewProj, float4(worldPos.xyz, 1.0));
	posLS /= posLS.w;
	posLS.y *= -1;
	posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);


	float2 HalfGaps = float2(0.00048828125, 0.00048828125);
	float2 Gaps = float2(0.0009765625, 0.0009765625);

	posLS.xy += HalfGaps;

	float shadowFactor = 0.0;

	const float esmControl = mEsmControl;


	if ( !all(posLS.xy > 0) || !all(posLS.xy < 1))
	{
		return shadowFactor;
	}

	float4 shadowDepthSample = float4(0, 0, 0, 0);
	shadowDepthSample.x = ESMShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0).r;
	shadowDepthSample.y = ESMShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(1, 0)).r;
	shadowDepthSample.z = ESMShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(0, 1)).r;
	shadowDepthSample.w = ESMShadowTexture.SampleLevel(clampMiplessLinearSampler, posLS.xy, 0, int2(1, 1)).r;
	float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
	shadowFactor = saturate(2.0 - exp((posLS.z - avgShadowDepthSample) * mEsmControl));
	return shadowFactor;
}


float GetASMFadeInConstant(float w)
{
	return 2.0 * frac(abs(w));
}

float PCF(float3 shadowMapCoord, float2 kernelSize )
{
	const float depthBias = -0.0005;
	//const float depthBias = -0.001f;
	//const float depthBias = -0.000f;
    float2 tapOffset[9] =
    {
        float2( 0.00, 0.00),
        float2( 1.20, 0.00),
        float2(-1.20, 0.00),
        float2( 0.00, 1.20),
        float2( 0.00,-1.20),
        float2( 0.84, 0.84),
        float2(-0.84, 0.84),
        float2(-0.84,-0.84),
        float2( 0.84,-0.84),
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
        float2 sampleCoord = shadowMapCoord.xy + kernelSize * tapOffset[i];

		shadowFactor += tapWeight[i] * DepthAtlasTexture.SampleCmpLevelZero( ShadowCmpSampler, sampleCoord, shadowMapCoord.z - depthBias );
    }
	return shadowFactor;
}

float4 SampleFrustumIndirectionTexture(ASMFrustumDesc frustumDesc, float mip)
{
	
	float lerpVal = frac(mip);
	int floorMip = (int)floor(mip);
	
	float4 indirectionCoordData =  IndexTexture[ NonUniformResourceIndex( frustumDesc.mStartingMip + floorMip ) ].SampleLevel(clampBorderNearSampler, 
		float2(frustumDesc.mIndexCoord.xy), 0);

	if(lerpVal > 0.0)
	{
		float4 upperIndirectionCoordData = IndexTexture[NonUniformResourceIndex(frustumDesc.mStartingMip + floorMip + 1)].SampleLevel(clampBorderNearSampler, 
			float2(frustumDesc.mIndexCoord.xy), 0);

		indirectionCoordData = lerp(indirectionCoordData, upperIndirectionCoordData, lerpVal);
	}
	
	return indirectionCoordData;
	
}



//indirectionCoordData = t, aka, result of sampling indirection texture using indexCoord
float3 GetASMTileCoord(float3 indexCoord, float4 indirectionCoordData)
{
	
	indexCoord.xy = floor( abs( indirectionCoordData.w ) ) * 
		ASMOneOverDepthAtlasSize * indexCoord.xy + indirectionCoordData.xy;

	//Index coordinate z value is being subtracted here because the application is using reversed depth buffer
    indexCoord.z = indexCoord.z - indirectionCoordData.z;
    return indexCoord;
}

float3 GetIndirectionTextureCoord(float4x4 indexTexMat, float3 worldPos)
{
	return mul(indexTexMat, float4(worldPos, 1.0)).xyz;
}

bool GetClampedDepthDifference(ASMFrustumDesc frustumDesc, out float depthDiff)
{
	const int DEM_LOD = gs_ASMMaxRefinement;
	
	float4 indirectionCoordData = SampleFrustumIndirectionTexture(frustumDesc, DEM_LOD);

	if(indirectionCoordData.w > 0)
	{
		float3 asmTileCoord = GetASMTileCoord(frustumDesc.mIndexCoord, indirectionCoordData);
		float demDepth = DEMTexture.SampleLevel(clampMiplessLinearSampler, asmTileCoord.xy, 0);
		depthDiff = saturate( demDepth  - asmTileCoord.z );
		return true;
	}
	return false;
}



float SampleASMShadowMap(float mip, float kernelSize, ASMFrustumDesc frustumDesc, inout float fadeInFactor )
{
	float4 IndexT = SampleFrustumIndirectionTexture(frustumDesc, mip);
	float shadowFactor = 0.0;

	float2 newKernelSize = float2( kernelSize / gs_ASMDepthAtlasTextureWidth, 
		kernelSize / gs_ASMDepthAtlasTextureHeight );
	if(IndexT.w != 0)
	{
		fadeInFactor = GetASMFadeInConstant(IndexT.w);
		float3 depthTileCoord = GetASMTileCoord(frustumDesc.mIndexCoord, IndexT);
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
		float lodClamp = PrerenderLodClampTexture.SampleLevel(
			clampBorderNearSampler, preRenderFrustumDesc.mIndexCoord.xy, 0) * gs_ASMMaxRefinement;
		
		shadowFactor = SampleASMShadowMap(max(mip, lodClamp), kernelSize, preRenderFrustumDesc, fadeInFactor);
	}


	if(fadeInFactor > 0)
	{
		float otherShadowFactor = 0.0;
		float otherFadeInFactor = 1.0;

		otherShadowFactor = SampleASMShadowMap(mip, kernelSize, 
			frustumDesc, otherFadeInFactor);


		shadowFactor = lerp(shadowFactor, otherShadowFactor, fadeInFactor);
	}
	return shadowFactor;
}


float GetBlockerDistance(ASMFrustumDesc frustumDesc, float3 worldPos, 
	float4x4 indexTexMat, float3 blockerSearchDir, float mip)
{
	int absMip = (int)(mip);
	float3 indexCoord = GetIndirectionTextureCoord(indexTexMat, worldPos);
	float4 indirectionData = IndexTexture[NonUniformResourceIndex(frustumDesc.mStartingMip + absMip)].
		SampleLevel(clampBorderNearSampler,	float2(indexCoord.xy), 0);

	float3 tileCoord = GetASMTileCoord(indexCoord, indirectionData);

	float2 tileCoordBoundsMin = floor( tileCoord.xy * ASMDepthAtlasSizeOverDepthTileSize - ASMHalfOverDepthTileSize ) * ASMDEMTileSizeOverDEMAtlasSize + ASMDEMTileCoord;
	float2 tileCoordBoundsMax = tileCoordBoundsMin + ASMDEMTileSize;

	float num = 0;
	float sum = 0;

	if(indirectionData.w != 0)
	{
		float3 sampleCoord = tileCoord;
		sampleCoord.z += 0.5 * blockerSearchDir.z * 0.1;

		[loop]for(int i = 0; i < 9; ++i)
		{
			float demValue = DEMTexture.SampleLevel(clampMiplessLinearSampler, 
				clamp(sampleCoord.xy, tileCoordBoundsMin, tileCoordBoundsMax), 0);

			[flatten]if(demValue >= sampleCoord.z)
			{
				sum += demValue;
				num += 1.0;
			}
			sampleCoord += blockerSearchDir * 0.1;
		}
	
	}
	float blockerDepth = num > 0 ? sum * rcp(num) : 1.0;
	return saturate(blockerDepth - tileCoord.z) * gs_ASMTileFarPlane;
}

float SampleShadowFactor(float3 worldPos)
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
		float penumbraSizeFactor = saturate(mPenumbraSize * depthDiff - 0.05);
		float kernelSize = saturate(depthDiff * 10.0 + 0.5);

		float lod = penumbraSizeFactor * gs_ASMMaxRefinement;
		float mip = floor(lod);
		
			
		
		shadowFactor = GetASMShadowFactor(mip, kernelSize, 
			frustumDesc, preRenderFrustumDesc);

		if(penumbraSizeFactor > 0.0 && penumbraSizeFactor < 1.0)
		{
			float upperShadowFactor = GetASMShadowFactor(mip + 1, kernelSize, 
				frustumDesc, preRenderFrustumDesc);

			shadowFactor = lerp(shadowFactor, upperShadowFactor, lod - mip);
		}

	}
	return 1.0 - shadowFactor;
}

struct DerivativesOutput
{
	float3 db_dx;
	float3 db_dy;
};

struct GradientInterpolationResults
{
	float2 interp;
	float2 dx;
	float2 dy;
};

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
GradientInterpolationResults interpolateAttributeWithGradient(float3x2 attributes, float3 db_dx, float3 db_dy, float2 deltaV, float2 twoOverRes)
{
	float3 attr0 = float3(attributes[0].x, attributes[1].x, attributes[2].x);
	float3 attr1 = float3(attributes[0].y, attributes[1].y, attributes[2].y);
	float2 attribute_x = float2(dot(db_dx, attr0), dot(db_dx, attr1));
	float2 attribute_y = float2(dot(db_dy, attr0), dot(db_dy, attr1));
	float2 attribute_s = attributes[0];

	GradientInterpolationResults result;
	result.dx = attribute_x * twoOverRes.x;
	result.dy = attribute_y * twoOverRes.y;
	result.interp = (attribute_s + deltaV.x * attribute_x + deltaV.y * attribute_y);
	return result;
}



// Computes the partial derivatives of a triangle from the projected screen space vertices
DerivativesOutput computePartialDerivatives(float2 v[3])
{
	DerivativesOutput output;
	float d = 1.0 / determinant(float2x2(v[2] - v[1], v[0] - v[1]));
	output.db_dx = float3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
	output.db_dy = float3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
	return output;
}

float3 interpolateAttribute(float3x3 attributes, float3 db_dx, float3 db_dy, float2 deltaV)
{
	float3 attributeX = mul(db_dx, attributes);
	float3 attributeY = mul(db_dy, attributes);
	float3 attributeS = attributes[0];

	return (attributeS + deltaV.x * attributeX + deltaV.y * attributeY);
}

float interpolateAttribute(float3 attributes, float3 db_dx, float3 db_dy, float2 deltaV)
{
	float attributeX = dot(db_dx, attributes);
	float attributeY = dot(db_dy, attributes);
	float attributeS = attributes[0];

	return (attributeS + deltaV.x * attributeX + deltaV.y * attributeY);
}

float depthLinearization(float depth, float near, float far)
{
	return (2.0 * near) / (far + near - depth * (far - near));
}


// Pixel shader
PsOut main(PsIn input) : SV_Target0
{
	float4 visRaw = vbPassTexture.Load(uint3(input.Position.xy, 0));

	uint alphaBitDrawIDTriID = packUnorm4x8(visRaw);

	// Early exit if this pixel doesn't contain triangle data
	if (alphaBitDrawIDTriID == ~0)
	{
		discard;
	}

	//extract packed data
	uint drawID = (alphaBitDrawIDTriID >> 23) & 0x000000FF;
	uint triangleID = (alphaBitDrawIDTriID & 0x007FFFFF);
	uint alpha1_opaque0 = (alphaBitDrawIDTriID >> 31);

	//this is the start vertex of the current draw batch
	uint startIndex = indirectDrawArgs[NonUniformResourceIndex(alpha1_opaque0)][drawID * 8 + 3];

	uint triIdx0 = (triangleID * 3 + 0) + startIndex;
	uint triIdx1 = (triangleID * 3 + 1) + startIndex;
	uint triIdx2 = (triangleID * 3 + 2) + startIndex;

	uint index0 = filteredIndexBuffer.Load(triIdx0 << 2);
	uint index1 = filteredIndexBuffer.Load(triIdx1 << 2);
	uint index2 = filteredIndexBuffer.Load(triIdx2 << 2);

	float3 v0pos = asfloat(vertexPos.Load4(index0 * 12)).xyz;
	float3 v1pos = asfloat(vertexPos.Load4(index1 * 12)).xyz;
	float3 v2pos = asfloat(vertexPos.Load4(index2 * 12)).xyz;

	float4 pos0 = mul(WorldViewProjMat, float4(v0pos, 1));
	float4 pos1 = mul(WorldViewProjMat, float4(v1pos, 1));
	float4 pos2 = mul(WorldViewProjMat, float4(v2pos, 1));

	float3 oneOverW = 1.0 / float3(pos0.w, pos1.w, pos2.w);

	pos0 *= oneOverW.x;
	pos1 *= oneOverW.y;
	pos2 *= oneOverW.z;

	float2 posSrc[3] = {pos0.xy, pos1.xy, pos2.xy};

	//Compute partial derivatives. This is necessary to interpolate triangle attributes per pixel.
	DerivativesOutput derivativesOut = computePartialDerivatives(posSrc);

	//Calculate delta vector that points from the projected vertex 0 to the current screen point
	float2 deltaV = input.ScreenPos + -posSrc[0];

	//interpoalte the 1/w (oneOverW) for all 3 vertices of the triangle
	//using the barycentric coordinates and the delta vector
	float interpolatedW = 1.0 / interpolateAttribute(oneOverW, derivativesOut.db_dx, derivativesOut.db_dy, deltaV);

	//reconstruct the z value at this screen point
	float zVal = interpolatedW * Project[2][2] + Project[2][3];

	// Calculate the world position coordinates:
	// First the projected coordinates at this point are calculated using Screen Position and the Z value
	// Then by multiplying the perspective projected coordinates by the inverse view projection matrix, it produces world coord

	float3 WorldPos = mul(InvViewProject, float4(input.ScreenPos * interpolatedW, zVal, interpolatedW)).xyz;


	//Texture coord interpolation
	//Apply perspective correction to texture coordinates
	float3x2 texCoords = 
	{
		unpack2Floats(vertexTexCoord.Load(index0 << 2)) * oneOverW[0],
		unpack2Floats(vertexTexCoord.Load(index1 << 2)) * oneOverW[1],
		unpack2Floats(vertexTexCoord.Load(index2 << 2)) * oneOverW[2]
	};
	// Interpolate texture coordinates and calculate the gradients for 
	// texture sampling with mipmapping support
	GradientInterpolationResults results = interpolateAttributeWithGradient(
		texCoords, derivativesOut.db_dx, derivativesOut.db_dy, deltaV, mTwoOverRes);
	

	float farValue = mFarNearDiff + mNear;
	float linearZ = depthLinearization(zVal / interpolatedW, mNear, farValue);

	float mip = pow(pow(linearZ, 0.9f) * 5.0f, 1.5f);

	float2 texCoordDX = results.dx * interpolatedW * mip;
	float2 texCoordDY = results.dy * interpolatedW * mip;
	float2 texCoord = results.interp * interpolatedW;

	/////////////LOAD///////////////////////////////
	// TANGENT INTERPOLATION
	// Apply perspective division to tangents

	float3x3 tangents = 
	{
		decodeDir(unpackUnorm2x16(vertexTangent.Load(index0 << 2))) * oneOverW[0],
		decodeDir(unpackUnorm2x16(vertexTangent.Load(index1 << 2))) * oneOverW[1],
		decodeDir(unpackUnorm2x16(vertexTangent.Load(index2 << 2))) * oneOverW[2]
	};

	float3 tangent = normalize(interpolateAttribute(tangents, 
		derivativesOut.db_dx, derivativesOut.db_dy, deltaV));

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
	uint materialID = indirectMaterialBuffer.Load((materialBaseSlot + drawID) << 2);

	//Calculate pixel color using interpolated attributes
	//reconstruct normal map Z from X and Y

	float4 normalMapRG = normalMaps[NonUniformResourceIndex(materialID)].
		SampleGrad(textureSampler, texCoord, texCoordDX, texCoordDY);

	float3 reconstructedNormalMap;
	reconstructedNormalMap.xy = normalMapRG.ga * 2.f - 1.f;
	reconstructedNormalMap.z = sqrt(1.f - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));

	//Normal interpolation
	//Apply perspective division
	float3x3 normals = 
	{
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index0 << 2))) * oneOverW[0],
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index1 << 2))) * oneOverW[1],
		decodeDir(unpackUnorm2x16(vertexNormal.Load(index2 << 2))) * oneOverW[2]
	};

	float3 normal = normalize(interpolateAttribute(normals, 
		derivativesOut.db_dx, derivativesOut.db_dy, deltaV));
	
	float3 binormal = normalize(cross(tangent, normal));

	normal = reconstructedNormalMap.x * tangent + 
		reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal;
	
	


	float shadowFactor = 1.0f;


	if(ShadowType == SHADOW_TYPE_ASM)
	{
		shadowFactor = SampleShadowFactor(WorldPos);
	}
	else if(ShadowType == SHADOW_TYPE_ESM)
	{
		shadowFactor = calcESMShadowFactor(WorldPos);
	}
	else if(ShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
	{
		shadowFactor = SDFShadowTexture.Load(uint3(input.Position.xy, 0)).r;
	}

	float4 diffuseColor = diffuseMaps[NonUniformResourceIndex(materialID)].SampleGrad(textureSampler, texCoord, texCoordDX, texCoordDY);
	float4 specularColor = specularMaps[NonUniformResourceIndex(materialID)].SampleGrad(textureSampler, texCoord, texCoordDX, texCoordDY);

	float Roughness = clamp(specularColor.a, 0.05f, 0.99f);
	float Metallic = specularColor.b;

	float3 camPos = mCameraPos.xyz;

	float3 ViewVec = normalize(mCameraPos.xyz - WorldPos.xyz);

	bool isTwoSided = (alpha1_opaque0 == 1) && meshConstantsBuffer[NonUniformResourceIndex(materialID)].twoSided;
	bool isBackFace = false;

	if(isTwoSided && dot(normal, ViewVec) < 0.0)
	{
		//flip normal
		normal = -normal;
		isBackFace = true;
	}

	float3 lightDir = -mLightDir;

	float3 HalfVec = normalize(ViewVec - lightDir);
	float3 ReflectVec = reflect(-ViewVec, normal);
	float NoV = saturate(dot(normal, ViewVec));


	float NoL = dot(normal, -lightDir);	


	// Deal with two faced materials
	NoL = (isTwoSided ? abs(NoL) : saturate(NoL));

	float3 shadedColor;

	float3 F0 = specularColor.xyz;
	float3 DiffuseColor = diffuseColor.xyz;
	
	float fLightingMode = 1.f;

	shadedColor = calculateIllumination(
		    normal,
		    ViewVec,
			HalfVec,
			ReflectVec,
			NoL,
			NoV,
			camPos.xyz,
			lightDir.xyz,
			WorldPos,
			DiffuseColor,
			DiffuseColor,
			Roughness,
			Metallic,
			isBackFace,
			fLightingMode,
			shadowFactor);

	shadedColor = shadedColor * lightColor.rgb * lightColor.a * NoL;

	float ambientIntencity = 0.2f;
    float3 ambient = diffuseColor.xyz * ambientIntencity;

	shadedColor += ambient;

	//shadedColor = float3(shadowFactor, shadowFactor, shadowFactor);
    
    PsOut output;
    output.FinalColor = float4(shadedColor.xyz, 1.0f);
    return output;
}

