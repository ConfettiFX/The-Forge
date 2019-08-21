/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of TheForge
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

// This shader loads draw / triangle Id per pixel and reconstruct interpolated vertex data.

#include <metal_stdlib>
using namespace metal;

#include "Shader_Defs.h"
#include "shading.h"
#include "ASMShader_Defs.h"

#define SHADOW_TYPE_ESM				0
#define SHADOW_TYPE_ASM				1
#define SHADOW_TYPE_MESH_BAKED_SDF	2

struct SceneVertexPos
{
    packed_float3 position;
};

struct SceneVertexTexcoord {
    packed_float2 texCoord;
};

struct SceneVertexNormal {
    packed_float3 normal;
};

struct SceneVertexTangent {
    packed_float3 tangent;
};

struct VSOutput {
	float4 position [[position]];
    float2 screenPos;
};

struct IndirectDrawArguments
{
    uint vertexCount;
    uint instanceCount;
    uint startVertex;
    uint startInstance;
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
    float3 attribute_x = attributes * db_dx;
    float3 attribute_y = attributes * db_dy;
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
    float2 attribute_x = float2(dot(db_dx,attr0), dot(db_dx,attr1));
    float2 attribute_y = float2(dot(db_dy,attr0), dot(db_dy,attr1));
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

struct BindlessDiffuseData
{
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> textures;
};

struct BindlessNormalData
{
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> textures;
};

struct BindlessSpecularData
{
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> textures;
};



struct IndirectDrawArgumentsData {
	constant uint* data[NUM_CULLING_VIEWPORTS];
};

struct Uniforms_renderSettingUniformBlock
{
	float4 WindowDimension;
	int ShadowType;
};


struct Uniforms_objectUniformBlock
{
	float4x4 mWorldViewProjMat;
	float4x4 mWorldMat;
};

struct Uniforms_cameraUniformBlock
{
	float4x4 View;
	float4x4 Project;
	float4x4 ViewProject ;
	float4x4 InvView;
	float4x4  InvProj;
	float4x4  InvViewProject;
	float4  mCameraPos;
	float mNear;
	float mFarNearDiff;
	float mFarNear;
	float paddingForAlignment0;
	packed_float2 mTwoOverRes;
};

struct Uniforms_lightUniformBlock
{
	float4x4 mLightViewProj;
	float4 lightPosition;
	float4 lightColor;
	float4 mLightUpVec;
	float4 mTanLightAngleAndThresholdValue;
	packed_float3 mLightDir;
};

struct Uniforms_ESMInputConstants
{
	float mEsmControl;
};

struct Uniforms_ASMUniformBlock
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

float calcESMShadowFactor(float3 worldPos, constant Uniforms_lightUniformBlock & lightUniformBlock, constant Uniforms_ESMInputConstants& ESMInputConstants, depth2d<float> shadowMap, sampler sh)
{
	float4 posLS = lightUniformBlock.mLightViewProj * float4(worldPos.xyz, 1.0);
	posLS /= posLS.w;
	posLS.y *= -1;
	posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);
	
	
	float2 HalfGaps = float2(0.00048828125, 0.00048828125);
	
	posLS.xy += HalfGaps;
	
	float shadowFactor = 0.0;
	
	
	
	const float esmControl = ESMInputConstants.mEsmControl;
	
	
	if ( !all(posLS.xy > 0) || !all(posLS.xy < 1))
	{
		return shadowFactor;
	}
	
	float4 shadowDepthSample = float4(0, 0, 0, 0);
	shadowDepthSample.x = shadowMap.sample(sh, posLS.xy, level(0.0f));
	shadowDepthSample.y = shadowMap.sample(sh, posLS.xy, level(0.0f), int2(1, 0));
	shadowDepthSample.z = shadowMap.sample(sh, posLS.xy, level(0.0f), int2(0, 1));
	shadowDepthSample.w = shadowMap.sample(sh, posLS.xy, level(0.0f), int2(1, 1));
	float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
	
	shadowFactor = saturate(2.0 - exp((posLS.z - avgShadowDepthSample) * esmControl));
	
	return shadowFactor;
}






struct ASMArguments
{
	ASMArguments(constant Uniforms_ASMUniformBlock& argsAsmUniformBlock,
				 depth2d<float> argsDepthAtlasTexture,
				 array_ref<texture2d<float, access::sample>> argsIndexTexture,
				 texture2d<float> argsDEMTexture,
				 texture2d<float> argsPrerenderLodClampTexture,
				 sampler argsShadowCmpSampler,
				 sampler argsClampBorderNearSampler,
				 sampler argsClampMiplessLinearSampler,
				 sampler argsClampMiplessNearSampler)
	:asmUniformBlock(argsAsmUniformBlock),
	DepthAtlasTexture(argsDepthAtlasTexture),
	IndexTexture(argsIndexTexture),
	DEMTexture(argsDEMTexture),
	PrerenderLodClampTexture(argsPrerenderLodClampTexture),
	ShadowCmpSampler(argsShadowCmpSampler),
	clampBorderNearSampler(argsClampBorderNearSampler),
	clampMiplessLinearSampler(argsClampMiplessLinearSampler),
	clampMiplessNearSampler(argsClampMiplessNearSampler)
	{};
	constant Uniforms_ASMUniformBlock & asmUniformBlock;
	depth2d<float> DepthAtlasTexture;
	sampler ShadowCmpSampler;
	sampler clampBorderNearSampler ;
	sampler clampMiplessLinearSampler;
	sampler clampMiplessNearSampler;
	array_ref<texture2d<float, access::sample>> IndexTexture;
	texture2d<float> DEMTexture;
	texture2d<float> PrerenderLodClampTexture;
	
};

struct ASMFrustumDesc
{
	float3 mIndexCoord;
	int mStartingMip;
};


float GetASMFadeInConstant(float w)
{
	return 2.0 * fract(abs(w));
}

float PCF(float3 shadowMapCoord, float2 kernelSize, thread ASMArguments& asmArguments )
{
	const float depthBias = -0.0005;
	//const float depthBias = -0.001f;
	//const float depthBias = 0.f;
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
		float depthCompareVal = asmArguments.DepthAtlasTexture.sample_compare( asmArguments.ShadowCmpSampler, sampleCoord, shadowMapCoord.z - depthBias );
		
		shadowFactor += tapWeight[i] * depthCompareVal;
	}
	return shadowFactor;
}


float4 SampleFrustumIndirectionTexture(
									   thread ASMFrustumDesc& frustumDesc, float mip, thread ASMArguments& asmArguments)
{
	float lerpVal = fract(mip);
	int floorMip = (int)floor(mip);
		
	float4 indirectionCoordData =  asmArguments.IndexTexture[frustumDesc.mStartingMip + floorMip].sample(asmArguments.clampBorderNearSampler, float2(frustumDesc.mIndexCoord.xy));
	
	
	if(lerpVal > 0.0)
	{
		float4 upperIndirectionCoordData = asmArguments.IndexTexture[frustumDesc.mStartingMip + floorMip + 1].sample(asmArguments.clampBorderNearSampler,	  float2(frustumDesc.mIndexCoord.xy));
		
		indirectionCoordData = mix(indirectionCoordData, upperIndirectionCoordData, lerpVal);
	}
	
	return indirectionCoordData;
	
}

//indirectionCoordData = t, aka, result of sampling indirection texture using indexCoord
float3 GetASMTileCoord(float3 indexCoord, float4 indirectionCoordData)
{
	float3 newAsmTileCoord = float3(indexCoord.x, indexCoord.y, indexCoord.z);
	newAsmTileCoord.xy = float2(floor( abs( indirectionCoordData.w ) ) ) *
		ASMOneOverDepthAtlasSize * newAsmTileCoord.xy + indirectionCoordData.xy;
	
	//Index coordinate z value is being subtracted here because the application is using reversed depth buffer
	newAsmTileCoord.z = newAsmTileCoord.z - indirectionCoordData.z;
	return newAsmTileCoord;
}

float3 GetIndirectionTextureCoord(float4x4 indexTexMat, float3 worldPos)
{
	return (indexTexMat * float4(worldPos, 1.0)).xyz;
}


bool GetClampedDepthDifference(thread ASMFrustumDesc& frustumDesc, thread ASMArguments& asmArguments,
							   thread float& depthDiff)
{
	const float DEM_LOD = (float)gs_ASMMaxRefinement;
	
	float4 indirectionCoordData = SampleFrustumIndirectionTexture(frustumDesc, DEM_LOD, asmArguments);
	
	if(indirectionCoordData.w > 0)
	{
		float3 indexCoord = frustumDesc.mIndexCoord;
		float3 asmTileCoord = GetASMTileCoord(indexCoord, indirectionCoordData);
		float demDepth = asmArguments.DEMTexture.sample(asmArguments.clampMiplessLinearSampler, asmTileCoord.xy, 0).r;
		depthDiff = saturate( demDepth  - asmTileCoord.z );
		return true;
	}
	return false;
}

float SampleASMShadowMap(float mip, float kernelSize, thread ASMFrustumDesc& frustumDesc,
						  thread ASMArguments& asmArguments,
						 thread float& fadeInFactor )
{
	float4 IndexT = SampleFrustumIndirectionTexture(frustumDesc, mip, asmArguments);
	float shadowFactor = 0.0;
	
	float2 newKernelSize = float2( kernelSize / gs_ASMDepthAtlasTextureWidth,
								  kernelSize / gs_ASMDepthAtlasTextureHeight );
	if(IndexT.w != 0)
	{
		fadeInFactor = GetASMFadeInConstant(IndexT.w);
		
		float3 indexCoord = frustumDesc.mIndexCoord;
		float3 depthTileCoord = GetASMTileCoord(indexCoord, IndexT);
		shadowFactor = PCF(depthTileCoord, newKernelSize, asmArguments);
		
	}
	
	return shadowFactor;
}

float GetASMShadowFactor(float mip, float kernelSize, thread ASMFrustumDesc& frustumDesc, thread ASMFrustumDesc& preRenderFrustumDesc, thread ASMArguments& asmArguments)
{
	float shadowFactor = 1.0;
	float fadeInFactor = 1.0;
	
	if(asmArguments.asmUniformBlock.mMiscBool.x == 1.0)
	{
		float lodClamp = asmArguments.PrerenderLodClampTexture.sample(														  asmArguments.clampBorderNearSampler, preRenderFrustumDesc.mIndexCoord.xy, 0).r * gs_ASMMaxRefinement;
		
		shadowFactor = SampleASMShadowMap(max(mip, lodClamp),
										  kernelSize, preRenderFrustumDesc, asmArguments, fadeInFactor);
	}
	
	
	if(fadeInFactor > 0)
	{
		float otherShadowFactor = 0.0;
		float otherFadeInFactor = 1.0;
		
		otherShadowFactor = SampleASMShadowMap(mip, kernelSize,
											   frustumDesc, asmArguments, otherFadeInFactor);
		
		
		shadowFactor = mix(shadowFactor, otherShadowFactor, fadeInFactor);
	}
	return shadowFactor;
}

float GetBlockerDistance(thread ASMFrustumDesc& frustumDesc, float3 worldPos,
						 float4x4 indexTexMat, float3 blockerSearchDir, float mip ,
						 thread ASMArguments& asmArguments)
{
	
	int absMip = (int)mip;
	float3 indexCoord = GetIndirectionTextureCoord(indexTexMat, worldPos);
	
	float4 indirectionData = asmArguments.IndexTexture[frustumDesc.mStartingMip + absMip].
		sample(asmArguments.clampBorderNearSampler,	float2(indexCoord.xy));
	
	float3 tileCoord = GetASMTileCoord(indexCoord, indirectionData);
	
	float2 tileCoordBoundsMin = floor( tileCoord.xy * ASMDepthAtlasSizeOverDepthTileSize - ASMHalfOverDepthTileSize ) * ASMDEMTileSizeOverDEMAtlasSize + ASMDEMTileCoord;
	float2 tileCoordBoundsMax = tileCoordBoundsMin + ASMDEMTileSize;
	
	float num = 0;
	float sum = 0;
	
	if(indirectionData.w != 0)
	{
		float3 sampleCoord = tileCoord;
		sampleCoord.z += 0.5 * blockerSearchDir.z * 0.1;
		
		for(int i = 0; i < 9; ++i)
		{
			float demValue = asmArguments.DEMTexture.sample(asmArguments.clampMiplessLinearSampler,
											clamp(sampleCoord.xy, tileCoordBoundsMin, tileCoordBoundsMax)).r;
			
			if(demValue >= sampleCoord.z)
			{
				sum += demValue;
				num += 1.0;
			}
			sampleCoord += blockerSearchDir * 0.1;
		}
		
	}
	float blockerDepth = num > 0 ? sum * (1.f / num) : 1.0;
	return saturate(blockerDepth - tileCoord.z) * gs_ASMTileFarPlane;
}

float SampleShadowFactor(float3 worldPos, thread ASMArguments& asmArguments)
{
	float shadowFactor = 0.0;
	float demLOD = gs_ASMMaxRefinement;
	
	ASMFrustumDesc frustumDesc;
	frustumDesc.mIndexCoord = float3(0.0, 0.0, 0.0);
	frustumDesc.mStartingMip = 0;
	
	
	ASMFrustumDesc preRenderFrustumDesc;
	preRenderFrustumDesc.mIndexCoord = float3(0.0, 0.0, 0.0);
	preRenderFrustumDesc.mStartingMip = 5;
	
	float blockerDistance = 0.0;
	float preRenderBlockerDistance = 0.0;
	
	if(asmArguments.asmUniformBlock.mMiscBool.y == 1.0)
	{
		blockerDistance = GetBlockerDistance(frustumDesc, worldPos,
											 asmArguments.asmUniformBlock.mIndexTexMat, asmArguments.asmUniformBlock.mSearchVector.xyz, demLOD, asmArguments);
		
		preRenderBlockerDistance = GetBlockerDistance(preRenderFrustumDesc, worldPos,
													  asmArguments.asmUniformBlock.mPrerenderIndexTexMat, asmArguments.asmUniformBlock.mPrerenderSearchVector.xyz, demLOD, asmArguments);
	}
	
	frustumDesc.mIndexCoord = GetIndirectionTextureCoord(
														 asmArguments.asmUniformBlock.mIndexTexMat, worldPos + blockerDistance * asmArguments.asmUniformBlock.mWarpVector.xyz);
	preRenderFrustumDesc.mIndexCoord = GetIndirectionTextureCoord(
																  asmArguments.asmUniformBlock.mPrerenderIndexTexMat, worldPos + preRenderBlockerDistance * asmArguments.asmUniformBlock.mPrerenderWarpVector.xyz);
	
	float depthDiff = 0.0;
	if(GetClampedDepthDifference(frustumDesc, asmArguments, depthDiff))
	{
		float penumbraSizeFactor = saturate(asmArguments.asmUniformBlock.mPenumbraSize * depthDiff - 0.05);
		float kernelSize = saturate(depthDiff * 10.0 + 0.5);
		
		float lod = penumbraSizeFactor * gs_ASMMaxRefinement;
		float mip = floor(lod);
				
		
		shadowFactor = GetASMShadowFactor(mip, kernelSize,
										  frustumDesc, preRenderFrustumDesc, asmArguments);
		
		if(penumbraSizeFactor > 0.0 && penumbraSizeFactor < 1.0)
		{
			float upperShadowFactor = GetASMShadowFactor(mip + 1, kernelSize,
														 frustumDesc, preRenderFrustumDesc, asmArguments);
			
			shadowFactor = mix(shadowFactor, upperShadowFactor, lod - mip);
		}
		
	}
	return 1.0 - shadowFactor;
}



// Pixel shader
fragment float4 stageMain(VSOutput input                                              [[stage_in]],
                          uint32_t sampleID                                           [[sample_id]],

                          constant SceneVertexPos* vertexPos                          [[buffer(0)]],
                          constant SceneVertexTexcoord* vertexTexCoord                [[buffer(1)]],
                          constant SceneVertexNormal* vertexNormal                    [[buffer(2)]],
                          constant SceneVertexTangent* vertexTangent                  [[buffer(3)]],
                          constant uint* filteredIndexBuffer                          [[buffer(4)]],
                          constant uint* indirectMaterialBuffer                       [[buffer(5)]],
                          constant MeshConstants* meshConstantsBuffer                 [[buffer(6)]],

                          constant IndirectDrawArgumentsData& indirectDrawArgs        [[buffer(7)]],
                          //constant PerFrameConstants& uniforms                        [[buffer(8)]],
						  constant Uniforms_objectUniformBlock & objectUniformBlock   [[buffer(8)]],
						  constant Uniforms_cameraUniformBlock & cameraUniformBlock 	[[buffer(9)]],
						  constant Uniforms_lightUniformBlock & lightUniformBlock  	  [[buffer(10)]],
						  constant Uniforms_ASMUniformBlock & ASMUniformBlock			[[buffer(11)]],
						  constant Uniforms_ESMInputConstants & ESMInputConstants		[[buffer(12)]],
						  constant Uniforms_renderSettingUniformBlock & renderSettingUniformBlock [[buffer(13)]],

                          device BindlessDiffuseData& diffuseMaps                     [[buffer(14)]],
                          device BindlessNormalData& normalMaps                       [[buffer(15)]],
                          device BindlessSpecularData& specularMaps                   [[buffer(16)]],

                          sampler textureSampler                                      [[sampler(0)]],
                          sampler ShadowCmpSampler                                        [[sampler(1)]],
						  sampler clampBorderNearSampler 							  [[sampler(2)]],
						  sampler clampMiplessLinearSampler								[[sampler(3)]],
						  sampler clampMiplessNearSampler								[[sampler(4)]],
                          texture2d<float,access::read> vbPassTexture                         [[texture(0)]],
						  array<texture2d<float,access::sample>, 10> IndexTexture						[[texture(1)]],
						  depth2d<float> DepthAtlasTexture				[[texture(11)]],
						  texture2d<float> DEMTexture						[[texture(12)]],
						  texture2d<float> PrerenderLodClampTexture		[[texture(13)]],
						  depth2d<float> ESMShadowTexture 				[[texture(14)]],
						  texture2d<float> SDFShadowTexture				[[texture(15)]]
                          )
{
	float4 visRaw = vbPassTexture.read(uint2(input.position.xy), sampleID);
	
	// Unpack float4 render target data into uint to extract data
	uint alphaBit_drawID_triID = pack_float_to_unorm4x8(visRaw);

    float3 shadedColor = float3(1.0f, 1.0f, 1.0f);
	
	// Early exit if this pixel doesn't contain triangle data
	if (alphaBit_drawID_triID != ~0u)
	{
		// Extract packed data
		uint drawID = (alphaBit_drawID_triID >> 23) & 0x000000FF;
		uint triangleID = (alphaBit_drawID_triID & 0x007FFFFF);
		uint alpha1_opaque0 = (alphaBit_drawID_triID >> 31);

		// This is the start vertex of the current draw batch
		uint startIndex = indirectDrawArgs.data[alpha1_opaque0][drawID * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + 2];

		// Calculate vertex indices for this triangle
		uint triIdx0 = (triangleID * 3 + 0) + startIndex;
		uint triIdx1 = (triangleID * 3 + 1) + startIndex;
		uint triIdx2 = (triangleID * 3 + 2) + startIndex;

		uint index0 = filteredIndexBuffer[triIdx0];
		uint index1 = filteredIndexBuffer[triIdx1];
		uint index2 = filteredIndexBuffer[triIdx2];
		
		// Load vertex data of the 3 vertices
		float3 v0pos = vertexPos[index0].position;
		float3 v1pos = vertexPos[index1].position;
		float3 v2pos = vertexPos[index2].position;

		// Transform positions to clip space
		float4 pos0 = objectUniformBlock.mWorldViewProjMat * float4(v0pos, 1);
		float4 pos1 = objectUniformBlock.mWorldViewProjMat * float4(v1pos, 1);
		float4 pos2 = objectUniformBlock.mWorldViewProjMat * float4(v2pos, 1);

		// Calculate the inverse of w, since it's going to be used several times
		float3 one_over_w = 1.0 / float3(pos0.w, pos1.w, pos2.w);

		// Project vertex positions to calcualte 2D post-perspective positions
		pos0 *= one_over_w[0];
		pos1 *= one_over_w[1];
		pos2 *= one_over_w[2];

		float2 pos_scr[3] = {pos0.xy, pos1.xy, pos2.xy};

		// Compute partial derivatives. This is necessary to interpolate triangle attributes per pixel.
		DerivativesOutput derivativesOut = computePartialDerivatives(pos_scr);

		// Calculate delta vector (d) that points from the projected vertex 0 to the current screen point
		float2 d = input.screenPos + -pos_scr[0];

		// Interpolate the 1/w (one_over_w) for all three vertices of the triangle
		// using the barycentric coordinates and the delta vector
		float w = 1.0 / interpolateAttribute(one_over_w, derivativesOut.db_dx, derivativesOut.db_dy, d);

		// Reconstruct the Z value at this screen point performing only the necessary matrix * vector multiplication
		// operations that involve computing Z
		float z = w * cameraUniformBlock.Project[2][2] + cameraUniformBlock.Project[3][2];

		// Calculate the world position coordinates:
		// First the projected coordinates at this point are calculated using In.screenPos and the computed Z value at this point.
		// Then, multiplying the perspective projected coordinates by the inverse view-projection matrix (invVP) produces world coordinates
		float3 position = (cameraUniformBlock.InvViewProject * float4(input.screenPos * w, z, w)).xyz;

		// TEXTURE COORD INTERPOLATION
		// Apply perspective correction to texture coordinates
		float3x2 texCoords = {
			float2(vertexTexCoord[index0].texCoord) * one_over_w[0],
			float2(vertexTexCoord[index1].texCoord) * one_over_w[1],
			float2(vertexTexCoord[index2].texCoord) * one_over_w[2]
		};

		// Interpolate texture coordinates and calculate the gradients for texture sampling with mipmapping support
		GradientInterpolationResults results = interpolateAttributeWithGradient(texCoords, derivativesOut.db_dx, derivativesOut.db_dy, d, cameraUniformBlock.mTwoOverRes);
		
		float farValue = cameraUniformBlock.mFarNearDiff + cameraUniformBlock.mNear;
		
		
		float linearZ = depthLinearization(z/w, cameraUniformBlock.mNear, farValue);
		float mip = pow(pow(linearZ, 0.9f) * 5.0f, 1.5f);

		float2 texCoordDX = results.dx * w * mip;
		float2 texCoordDY = results.dy * w * mip;
		float2 texCoord = results.interp * w;
		
		// NORMAL INTERPOLATION
		// Apply perspective division to normals
		float3x3 normals = {
			float3(vertexNormal[index0].normal) * one_over_w[0],
			float3(vertexNormal[index1].normal) * one_over_w[1],
			float3(vertexNormal[index2].normal) * one_over_w[2]
		};

		float3 normal = normalize(interpolateAttribute(normals, derivativesOut.db_dx, derivativesOut.db_dy, d));

		// TANGENT INTERPOLATION
		// Apply perspective division to tangents
		float3x3 tangents = {
			float3(vertexTangent[index0].tangent) * one_over_w[0],
			float3(vertexTangent[index1].tangent) * one_over_w[1],
			float3(vertexTangent[index2].tangent) * one_over_w[2]
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
		
		// Get textures from arrays.
		texture2d<float> diffuseMap = diffuseMaps.textures[materialID];
		texture2d<float> normalMap = normalMaps.textures[materialID];
		texture2d<float> specularMap = specularMaps.textures[materialID];

		// CALCULATE PIXEL COLOR USING INTERPOLATED ATTRIBUTES
		// Reconstruct normal map Z from X and Y
		float4 normalMapRG = normalMap.sample(textureSampler,texCoord,gradient2d(texCoordDX,texCoordDY));

		float3 reconstructedNormalMap;
		reconstructedNormalMap.xy = normalMapRG.ga * 2 - 1;
		reconstructedNormalMap.z = sqrt(1 - dot(reconstructedNormalMap.xy, reconstructedNormalMap.xy));

		// Calculate vertex binormal from normal and tangent
		float3 binormal = normalize(cross(tangent, normal));
		
		// Calculate pixel normal using the normal map and the tangent space vectors
		normal = reconstructedNormalMap.x * tangent + reconstructedNormalMap.y * binormal + reconstructedNormalMap.z * normal;
		
		// Sample Diffuse color
		float4 diffuseColor = diffuseMap.sample(textureSampler,texCoord,gradient2d(texCoordDX,texCoordDY));
		float4 specularData = specularMap.sample(textureSampler,texCoord,gradient2d(texCoordDX,texCoordDY));

		float Roughness = clamp(specularData.a, 0.05f, 0.99f);
		float Metallic = specularData.b;



		const bool isTwoSided = (alpha1_opaque0 == 1) && (meshConstantsBuffer[materialID].twoSided == 1);
		bool isBackFace = false;

		float3 ViewVec = normalize(cameraUniformBlock.mCameraPos.xyz - position.xyz);

		//if it is backface
		//this should be < 0 but our mesh's edge normals are smoothed, badly
		if(isTwoSided && dot(normal, ViewVec) < 0.0)
		{
			//flip normal
			normal = -normal;
			isBackFace = true;
		}
		
		float3 lightDir = -lightUniformBlock.mLightDir.xyz;

		float3 HalfVec = normalize(ViewVec - lightDir.xyz);
		float3 ReflectVec = reflect(-ViewVec, normal);
		float NoV = saturate(dot(normal, ViewVec));
		
		float NoL = dot(normal, -lightDir.xyz);
		
		// Deal with two faced materials
		NoL = (isTwoSided ? abs(NoL) : saturate(NoL));

		float3 shadedColor;
		
		float3 DiffuseColor = diffuseColor.xyz;
		
		float shadowFactor = 1.0f;
		
		
		
		if(renderSettingUniformBlock.ShadowType == SHADOW_TYPE_ASM)
		{
			ASMArguments asmArguments(ASMUniformBlock, DepthAtlasTexture, IndexTexture, DEMTexture, PrerenderLodClampTexture, ShadowCmpSampler, clampBorderNearSampler, clampMiplessLinearSampler, clampMiplessNearSampler);
			/*asmArguments.ShadowCmpSampler = ShadowCmpSampler;
			asmArguments.clampBorderNearSampler = clampBorderNearSampler;
			asmArguments.clampMiplessLinearSampler = clampMiplessLinearSampler;
			asmArguments.clampMiplessNearSampler = clampMiplessNearSampler;*/
			shadowFactor = SampleShadowFactor(position, asmArguments);
			//shadowFactor = 0.5;
		
		}
		else if(renderSettingUniformBlock.ShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			shadowFactor = SDFShadowTexture.read(uint2(input.position.xy)).r;
		
		}
		else if(renderSettingUniformBlock.ShadowType == SHADOW_TYPE_ESM)
		{
			//shadowFactor = ESMShadowTexture.read(uint2(input.position.xy), sampleID).r;
			//shadowFactor *= ESMInputConstants.mEsmControl;
			
			shadowFactor = calcESMShadowFactor(position.xyz, lightUniformBlock, ESMInputConstants, ESMShadowTexture, clampMiplessLinearSampler);
		}
		
		
		float fLightingMode = 1.0f;
		
		shadedColor = calculateIllumination(
											normal,
											ViewVec,
											HalfVec,
											ReflectVec,
											NoL,
											NoV,
											cameraUniformBlock.mCameraPos.xyz,
											
											lightDir.xyz,
											position,
											DiffuseColor,
											DiffuseColor,
											Roughness,
											Metallic,
											isBackFace,
											fLightingMode,
											shadowFactor);
		
		shadedColor = shadedColor *  NoL;

		// point lights
		// Find the light cluster for the current pixel
		
		float ambientIntencity = 0.2f;
		float3 ambient = diffuseColor.xyz * ambientIntencity;
		
		float3 FinalColor = shadedColor + ambient;
		//FinalColor =  float3(shadowFactor, shadowFactor, shadowFactor);
		//FinalColor = float3(texCoord.x, texCoord.y, 0.0);
		//FinalColor = cameraUniformBlock.mCameraPos.xyz;
		//FinalColor = DiffuseColor.xyz;
		//FinalColor = visRaw.xyz;
		return float4(FinalColor, 1.0);
	}
	
	// Output final pixel color
	return float4(shadedColor, 0.0);
}


