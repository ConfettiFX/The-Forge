/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/sort.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/OS/Interfaces/IUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IScripting.h"
#include "../../../../Common_3/OS/Interfaces/IFont.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/ThirdParty/OpenSource/cgltf/GLTFLoader.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be last include in cpp file

//--------------------------------------------------------------------------------------------
// GLOBAL DEFINTIONS
//--------------------------------------------------------------------------------------------

uint32_t			gFrameIndex					= 0;

const uint32_t		gImageCount					= 3;
const uint32_t		gLightCount					= 3;
const uint32_t		gTotalLightCount			= gLightCount + 1;

static const char* pModelNames[]				=
{
	"Lantern.gltf",
	"FlightHelmet.gltf",
	"capsule.gltf",
	"cube.gltf",
	"lion.gltf",
	"matBall.gltf"
};
static const uint32_t gModelValues[]			= { 0, 1, 2, 3, 4, 5 };
static uint32_t mModelSelected					= 0;
static const uint32_t mModelCount				= 6;

// Model Quantization Settings
int					gCurrentLod					= 0;
int					gMaxLod						= 5;

bool				bEnableVignette				= true;
bool				bScreenShotMode				= false;

static const uint32_t TEMPORAL_AA_JITTER_SAMPLES = 8;

#if defined(QUEST_VR)
// Off by default in VR.
// Usually it's a bad idea to use TAA in VR.  It produces really bad results in VR and the effect is too costly.
bool				bEnableTemporalAA = false;
#else
bool				bEnableTemporalAA = true;
#endif


bool				gCurrentTemporalAARenderTarget = 0;
uint32_t			gCurrentTemporalAAJitterSample = 0;

vec2				gTemporalAAJitterSamples[TEMPORAL_AA_JITTER_SAMPLES] = {
	vec2(-0.5F,    0.33333337F),
	vec2( 0.5F,   -0.7777778F),
	vec2(-0.75F,  -0.111111104F),
	vec2( 0.25F,   0.5555556F),
	vec2(-0.25F,  -0.5555556F),
	vec2( 0.75F,   0.111111164F),
	vec2(-0.875F,  0.7777778F),
	vec2( 0.125F, -0.9259259F),
};

CameraMatrix	gTemporalAAPreviousViewProjection = CameraMatrix::identity();
CameraMatrix	gTemporalAAReprojection = CameraMatrix::identity();

ProfileToken   gGpuProfileToken;
Texture*			pTextureBlack = NULL;
//--------------------------------------------------------------------------------------------
// PRE PROCESSORS
//--------------------------------------------------------------------------------------------

#define SHADOWMAP_MSAA_SAMPLES 1

#if defined(TARGET_IOS) || defined(__ANDROID__)
#define SHADOWMAP_RES 1024u
#else
#define SHADOWMAP_RES 2048u
#endif
//--------------------------------------------------------------------------------------------
// STRUCT DEFINTIONS
//--------------------------------------------------------------------------------------------

struct UniformBlock
{
	CameraMatrix mProjectView;
	mat4 mShadowLightViewProj;
	vec4 mCameraPosition;
	vec4 mLightColor[gTotalLightCount];
	vec4 mLightDirection[gLightCount];
};

struct UniformBlock_Shadow
{
	mat4 ViewProj;
};

struct UniformBlock_Floor
{
	mat4	worldMat;
	CameraMatrix	projViewMat;
	vec4	screenSize;
};

struct MeshPushConstants
{
	uint32_t nodeIndex;
};

struct GLTFTextureProperties
{
	int16_t mTextureIndex;
	int16_t mSamplerIndex;
	int32_t mUVStreamIndex;
	float mRotation;
	float mValueScale;
	float2 mOffset;
	float2 mScale;
};

struct GLTFMaterialData
{
	uint32_t mAlphaMode;
	float mAlphaCutoff;
	float2 mEmissiveGBScale;
	
    float4 mBaseColorFactor;
    float4 mMetallicRoughnessFactors; // RG, or specular RGB + A glossiness
	
	GLTFTextureProperties mBaseColorProperties;
	GLTFTextureProperties mMetallicRoughnessProperties;
	
	GLTFTextureProperties mNormalTextureProperties;
	GLTFTextureProperties mOcclusionTextureProperties;
	GLTFTextureProperties mEmissiveTextureProperties;
};

static VertexLayout gVertexLayoutModel;

struct GLTFAsset
{
	Renderer* pRenderer = NULL;
	Geometry* pGeom = NULL;
	GLTFContainer* pData = NULL;
	eastl::vector<Texture*> mTextures;
	eastl::vector<Sampler*> mSamplers;
	DescriptorSet* pMaterialSet = NULL;
	Buffer* pNodeTransformsBuffer = NULL;
	Buffer* pMaterialBuffer = NULL;
	Sampler* pDefaultSampler = NULL;
	
	void Init(Renderer* renderer, Sampler* defaultSampler)
	{
		pRenderer = renderer;
		pDefaultSampler = defaultSampler;
		
		mSamplers.resize(pData->mSamplerCount);
		for (uint32_t i = 0; i < pData->mSamplerCount; ++i)
			addSampler(pRenderer, pData->pSamplers + i, &mSamplers[i]);
		
		mTextures.resize(pData->pHandle->images_count);

		// Classify textures by usage.
		// Base color textures need sRGB format, normal maps and non-color textures need different compression settings.
		eastl::vector<TextureCreationFlags> textureCreationFlags;
		textureCreationFlags.resize(pData->pHandle->images_count);

		for (uint32_t materialIndex = 0; materialIndex < pData->pHandle->materials_count; ++materialIndex)
		{
			const GLTFMaterial& material = pData->pMaterials[materialIndex];

			switch (material.mMaterialType)
			{
				case GLTF_MATERIAL_TYPE_METALLIC_ROUGHNESS:
				{
					// Textures representing color should be stored in SRGB or HDR format
					updateTextureCreationFlags(textureCreationFlags, material.mMetallicRoughness.mBaseColorTexture, TEXTURE_CREATION_FLAG_SRGB);
					updateTextureCreationFlags(textureCreationFlags, material.mMetallicRoughness.mMetallicRoughnessTexture, TEXTURE_CREATION_FLAG_NONE);
				}
				break;

				case GLTF_MATERIAL_TYPE_SPECULAR_GLOSSINESS:
				{
					// Textures representing color should be stored in SRGB or HDR format
					updateTextureCreationFlags(textureCreationFlags, material.mSpecularGlossiness.mDiffuseTexture, TEXTURE_CREATION_FLAG_SRGB);
					updateTextureCreationFlags(textureCreationFlags, material.mSpecularGlossiness.mSpecularGlossinessTexture, TEXTURE_CREATION_FLAG_SRGB);
				} break;
			}

			updateTextureCreationFlags(textureCreationFlags, material.mNormalTexture, TEXTURE_CREATION_FLAG_NORMAL_MAP);
			updateTextureCreationFlags(textureCreationFlags, material.mOcclusionTexture, TEXTURE_CREATION_FLAG_NONE);
			updateTextureCreationFlags(textureCreationFlags, material.mEmissiveTexture, TEXTURE_CREATION_FLAG_SRGB);
		}

		SyncToken token = {};
		for (uint32_t imageIndex = 0; imageIndex < pData->pHandle->images_count; ++imageIndex)
		{
			TextureCreationFlags flags = textureCreationFlags[imageIndex];
			gltfLoadTextureAtIndex(pData, imageIndex, flags, &token, TEXTURE_CONTAINER_BASIS, &mTextures[imageIndex]);
		}

		if (pData->mNodeCount)
			createNodeTransformsBuffer();
	}

	static void updateTextureCreationFlags(eastl::vector<TextureCreationFlags>& creationFlags, const GLTFTextureView& texture, TextureCreationFlags flag)
	{
		if (texture.mTextureIndex != -1)
		{
			TextureCreationFlags oldFlag = creationFlags[texture.mTextureIndex];
			if (oldFlag != TEXTURE_CREATION_FLAG_NONE && oldFlag != flag)
			{
				LOGF(LogLevel::eWARNING, "Malformed GLTF file, texture %s is used in multiple incompatible ways", texture.pName);
			}

			creationFlags[texture.mTextureIndex] = flag;
		}
	}

	void updateTransform(size_t nodeIndex, mat4* nodeTransforms, bool* nodeTransformsInited)
	{
		if (nodeTransformsInited[nodeIndex]) { return; }

		if (pData->pNodes[nodeIndex].mScale.getX() != pData->pNodes[nodeIndex].mScale.getY() ||
			pData->pNodes[nodeIndex].mScale.getX() != pData->pNodes[nodeIndex].mScale.getZ())
		{
			LOGF(LogLevel::eWARNING, "Node %llu has a non-uniform scale and will have an incorrect normal when rendered.", (uint64_t)nodeIndex);
		}

		mat4 matrix = pData->pNodes[nodeIndex].mMatrix;
		if (pData->pNodes[nodeIndex].mParentIndex != UINT_MAX)
		{
			updateTransform((size_t)pData->pNodes[nodeIndex].mParentIndex, nodeTransforms, nodeTransformsInited);
			matrix = nodeTransforms[pData->pNodes[nodeIndex].mParentIndex] * matrix;
		}
		nodeTransforms[nodeIndex] = matrix;
		nodeTransformsInited[nodeIndex] = true;
	}

	void createNodeTransformsBuffer()
	{
		bool* nodeTransformsInited = (bool*)alloca(sizeof(bool) * pData->mNodeCount);
		memset(nodeTransformsInited, 0, sizeof(bool) * pData->mNodeCount);

		mat4* nodeTransforms = (mat4*)alloca(sizeof(mat4) * pData->mNodeCount); //-V630

		for (uint32_t i = 0; i < pData->mNodeCount; ++i)
		{
			updateTransform(i, nodeTransforms, nodeTransformsInited);
		}
		
		// Scale and centre the model.
		
		Point3 modelBounds[2] = { Point3(FLT_MAX), Point3(-FLT_MAX) };
		size_t nodeIndex = 0;
		for (uint32_t n = 0; n < pData->mNodeCount; ++n)
		{
			GLTFNode& node = pData->pNodes[n];

			if (node.mMeshIndex != UINT_MAX)
			{
				for (uint32_t i = 0; i < node.mMeshCount; ++i)
				{
					Point3 minBound = pData->pMeshes[node.mMeshIndex + i].mMin;
					Point3 maxBound = pData->pMeshes[node.mMeshIndex + i].mMax;
					Point3 localPoints[] = {
						Point3(minBound.getX(), minBound.getY(), minBound.getZ()),
						Point3(minBound.getX(), minBound.getY(), maxBound.getZ()),
						Point3(minBound.getX(), maxBound.getY(), minBound.getZ()),
						Point3(minBound.getX(), maxBound.getY(), maxBound.getZ()),
						Point3(maxBound.getX(), minBound.getY(), minBound.getZ()),
						Point3(maxBound.getX(), minBound.getY(), maxBound.getZ()),
						Point3(maxBound.getX(), maxBound.getY(), minBound.getZ()),
						Point3(maxBound.getX(), maxBound.getY(), maxBound.getZ()),
					};
					for (size_t j = 0; j < 8; j += 1)
					{
						vec4 worldPoint = nodeTransforms[nodeIndex] * localPoints[j];
						modelBounds[0] = minPerElem(modelBounds[0], Point3(worldPoint.getXYZ()));
						modelBounds[1] = maxPerElem(modelBounds[1], Point3(worldPoint.getXYZ()));
					}
				}
			}
			nodeIndex += 1;
		}
		
		const float targetSize = 1.0;
		
		vec3 modelSize = modelBounds[1] - modelBounds[0];
		float largestDim = max(modelSize.getX(), max(modelSize.getY(), modelSize.getZ()));
		Point3 modelCentreBase = Point3(
			0.5f * (modelBounds[0].getX() + modelBounds[1].getX()),
			modelBounds[0].getY(),
			0.5f * (modelBounds[0].getZ() + modelBounds[1].getZ()));
		Vector3 scaleVector = Vector3(targetSize / largestDim);
		scaleVector.setZ(-scaleVector.getZ());
		mat4 translateScale = mat4::scale(scaleVector) * mat4::translation(-Vector3(modelCentreBase));

		for (uint32_t i = 0; i < pData->mNodeCount; ++i)
		{
			nodeTransforms[i] = translateScale * nodeTransforms[i];
		}
		
		BufferLoadDesc nodeTransformsBufferLoadDesc = {};
		nodeTransformsBufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		nodeTransformsBufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		nodeTransformsBufferLoadDesc.mDesc.mStructStride = sizeof(mat4);
		nodeTransformsBufferLoadDesc.mDesc.mElementCount = pData->mNodeCount;
		nodeTransformsBufferLoadDesc.mDesc.mSize = pData->mNodeCount * sizeof(mat4);
		nodeTransformsBufferLoadDesc.pData = nodeTransforms;
		nodeTransformsBufferLoadDesc.mDesc.pName = "GLTF Node Transforms Buffer";
		nodeTransformsBufferLoadDesc.ppBuffer = &pNodeTransformsBuffer;
		addResource(&nodeTransformsBufferLoadDesc, NULL);
	}
	
	void updateTextureProperties(const GLTFTextureView& textureView, GLTFTextureProperties& textureProperties)
	{
		textureProperties.mTextureIndex = (int16_t)textureView.mTextureIndex;
		textureProperties.mSamplerIndex = (int16_t)textureView.mSamplerIndex;
		textureProperties.mUVStreamIndex = textureView.mUVStreamIndex;
		textureProperties.mValueScale = textureView.mScale;
		textureProperties.mScale = textureView.mTransform.mScale;
		textureProperties.mOffset = textureView.mTransform.mOffset;
		textureProperties.mRotation = textureView.mTransform.mRotation;
	}
	
	void updateParam(DescriptorData* params, const GLTFTextureView& textureView, const char* textureName, const char* samplerName, uint32_t& nextParamIdx)
	{
		if (textureView.mTextureIndex >= 0)
		{
			ASSERT(textureView.mTextureIndex < (ssize_t)mTextures.size());
			uint32_t index = nextParamIdx++;
			params[index].pName = textureName;
			params[index].ppTextures = &mTextures[textureView.mTextureIndex];
		}
		else
		{
			uint32_t index = nextParamIdx++;
			params[index].pName = textureName;
			params[index].ppTextures = &pTextureBlack;
		}

		uint32_t samplerIndex = nextParamIdx++;
		params[samplerIndex].pName = samplerName;
		if (textureView.mSamplerIndex >= 0)
		{
			ASSERT(textureView.mSamplerIndex < (ssize_t)mSamplers.size());
			params[samplerIndex].ppSamplers = &mSamplers[textureView.mSamplerIndex];
		}
		else
		{
			params[samplerIndex].ppSamplers = &pDefaultSampler;
		}
	}
	
	void createMaterialResources(RootSignature* pRootSignature, DescriptorSet* pBindlessTexturesSamplersSet = NULL) // if pBindlessTexturesSamplersSet is non-NULL, means that textures are bindless and the material data is accessed through a storage rather than uniform buffer.
	{
		uint32_t materialDataStride = pBindlessTexturesSamplersSet ? sizeof(GLTFMaterialData) : round_up(sizeof(GLTFMaterialData), 256);

		BufferLoadDesc materialBufferLoadDesc = {};
		materialBufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		materialBufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		materialBufferLoadDesc.mDesc.mStructStride = materialDataStride;
		materialBufferLoadDesc.mDesc.mSize = pData->mMaterialCount * materialDataStride;
		materialBufferLoadDesc.mDesc.pName = "GLTF Materials Buffer";
		materialBufferLoadDesc.ppBuffer = &pMaterialBuffer;
		addResource(&materialBufferLoadDesc, NULL);

		waitForAllResourceLoads();
		
		if (pBindlessTexturesSamplersSet)
		{
			// Use a single descriptor set and indexing into it using push constants.
			DescriptorSetDesc desc = {};
			desc.mMaxSets = 1;
			desc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_NONE;
			desc.pRootSignature = pRootSignature;
			addDescriptorSet(pRenderer, &desc, &pBindlessTexturesSamplersSet);
			
			DescriptorData params[3] = {};
			params[0].pName = "cbMaterialData";
			params[0].ppBuffers = &pMaterialBuffer;
			
			params[1].pName = "gltfTextures";
			params[1].ppTextures = mTextures.data();
			params[1].mCount = (uint32_t)mTextures.size();
			
			params[2].pName = "gltfSamplers";
			params[2].ppSamplers = mSamplers.data();
			params[2].mCount = (uint32_t)mSamplers.size();
			
			updateDescriptorSet(pRenderer, 0, pBindlessTexturesSamplersSet, 3, params);
		}
		else
		{
			// Use one set per material.
			DescriptorSetDesc desc = {};
			desc.mMaxSets = (uint32_t)pData->mMaterialCount;
			desc.mUpdateFrequency = DESCRIPTOR_UPDATE_FREQ_PER_DRAW;
			desc.pRootSignature = pRootSignature;
			addDescriptorSet(pRenderer, &desc, &pMaterialSet);
		}
		
		SyncToken token = {};
		
		for (uint32_t m = 0; m < pData->mMaterialCount; ++m)
		{
			const GLTFMaterial& material = pData->pMaterials[m];
			uint32_t materialBufferOffset = m * materialDataStride;
			
			BufferUpdateDesc updateDesc = {};
			updateDesc.pBuffer = pMaterialBuffer;
			updateDesc.mDstOffset = materialBufferOffset;
			beginUpdateResource(&updateDesc);
			{
				GLTFMaterialData* materialData = (GLTFMaterialData*)updateDesc.pMappedData;
				materialData->mAlphaMode = (uint32_t)material.mAlphaMode;
				materialData->mAlphaCutoff = material.mAlphaCutoff;
				materialData->mEmissiveGBScale = float2(material.mEmissiveFactor.y, material.mEmissiveFactor.z);
				
				switch (material.mMaterialType)
				{
					case GLTF_MATERIAL_TYPE_METALLIC_ROUGHNESS:
					{
						const GLTFMetallicRoughnessMaterial& metallicRoughness = material.mMetallicRoughness;
						materialData->mBaseColorFactor = metallicRoughness.mBaseColorFactor;
						materialData->mMetallicRoughnessFactors = float4(0.0f, metallicRoughness.mRoughnessFactor, metallicRoughness.mMetallicFactor, 0.f);
						updateTextureProperties(metallicRoughness.mBaseColorTexture, materialData->mBaseColorProperties);
						updateTextureProperties(metallicRoughness.mMetallicRoughnessTexture, materialData->mMetallicRoughnessProperties);
						break;
					}
					case GLTF_MATERIAL_TYPE_SPECULAR_GLOSSINESS:
					{
						const GLTFSpecularGlossinessMaterial& specularGlossiness = material.mSpecularGlossiness;
						materialData->mBaseColorFactor = specularGlossiness.mDiffuseFactor;
						materialData->mMetallicRoughnessFactors = float4(specularGlossiness.mSpecularFactor, specularGlossiness.mGlossinessFactor);
						updateTextureProperties(specularGlossiness.mDiffuseTexture, materialData->mBaseColorProperties);
						updateTextureProperties(specularGlossiness.mSpecularGlossinessTexture, materialData->mMetallicRoughnessProperties);
						break;
					}
				}
				
				updateTextureProperties(material.mNormalTexture, materialData->mNormalTextureProperties);
				updateTextureProperties(material.mOcclusionTexture, materialData->mOcclusionTextureProperties);
				updateTextureProperties(material.mEmissiveTexture, materialData->mEmissiveTextureProperties);
				
				if (materialData->mEmissiveTextureProperties.mTextureIndex >= 0)
				{
					materialData->mEmissiveGBScale *= materialData->mEmissiveTextureProperties.mValueScale;
					materialData->mEmissiveTextureProperties.mValueScale *= material.mEmissiveFactor.x;
				}
				else
				{
					materialData->mEmissiveTextureProperties.mValueScale = material.mEmissiveFactor.x;
				}
			}
			endUpdateResource(&updateDesc, &token);
			
			DescriptorDataRange range = { materialBufferOffset, materialDataStride };
			DescriptorData params[11] = {};
			params[0].pName = "cbMaterialData";
			params[0].ppBuffers = &pMaterialBuffer;
			params[0].pRanges = &range;
			uint32_t paramIdx = 1;
			
			switch (material.mMaterialType)
			{
				case GLTF_MATERIAL_TYPE_METALLIC_ROUGHNESS:
				{
					const GLTFMetallicRoughnessMaterial& metallicRoughness = material.mMetallicRoughness;
					updateParam(params, metallicRoughness.mBaseColorTexture, "baseColorMap", "baseColorSampler", paramIdx);
					updateParam(params, metallicRoughness.mMetallicRoughnessTexture, "metallicRoughnessMap", "metallicRoughnessSampler", paramIdx);
					break;
				}
				case GLTF_MATERIAL_TYPE_SPECULAR_GLOSSINESS:
				{
					const GLTFSpecularGlossinessMaterial& specularGlossiness = material.mSpecularGlossiness;
					updateParam(params, specularGlossiness.mDiffuseTexture, "baseColorMap", "baseColorSampler", paramIdx);
					updateParam(params, specularGlossiness.mSpecularGlossinessTexture, "metallicRoughnessMap", "metallicRoughnessSampler", paramIdx);
					break;
				}
			}

			updateParam(params, material.mNormalTexture, "normalMap", "normalMapSampler", paramIdx);
			updateParam(params, material.mOcclusionTexture, "occlusionMap", "occlusionMapSampler", paramIdx);
			updateParam(params, material.mEmissiveTexture, "emissiveMap", "emissiveMapSampler", paramIdx);
			
			if (!pBindlessTexturesSamplersSet)
			{
				updateDescriptorSet(pRenderer, m, pMaterialSet, paramIdx, params);
			}
		}
		
		waitForToken(&token);
	}
	
	void removeResources()
	{
		if (!pData)
			return;

		for (Sampler* sampler : mSamplers)
			removeSampler(pRenderer, sampler);
		
		mSamplers.set_capacity(0);
		
		for (Texture* texture : mTextures)
			removeResource(texture);
		
		gltfUnloadContainer(pData);
		removeResource(pGeom);

		mTextures.set_capacity(0);
	
		if (pMaterialSet)
			removeDescriptorSet(pRenderer, pMaterialSet);
		
		if (pMaterialBuffer)
			removeResource(pMaterialBuffer);

		if (pNodeTransformsBuffer)
			removeResource(pNodeTransformsBuffer);
		
		pData = NULL;
		pMaterialBuffer = NULL;
		pMaterialSet = NULL;
		pRenderer = NULL;
	}
};

struct TAAUniformBuffer
{
	CameraMatrix ReprojectionMatrix;
};

struct SkyboxUniformBuffer
{
	CameraMatrix InverseViewProjection;
};

struct PostProcessRootConstant
{
	int2 SceneTextureSize;
	float VignetteRadius;
};

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------

Renderer*			pRenderer			= NULL;

Queue*				pGraphicsQueue		= NULL;

CmdPool*			pCmdPools[gImageCount];
Cmd*				pCmds[gImageCount];

SwapChain*			pSwapChain			= NULL;

RenderTarget*		pForwardRT			= NULL;
RenderTarget*		pDepthBuffer		= NULL;
RenderTarget*		pShadowRT			= NULL;
RenderTarget*		pTemporalAAHistoryRT[2] = { NULL, NULL };

Fence*				pRenderCompleteFences[gImageCount]	= { NULL };

Semaphore*			pImageAcquiredSemaphore				= NULL;
Semaphore*			pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*				pShaderZPass						= NULL;
Shader*				pShaderZPass_NonOptimized			= NULL;
Shader*				pMeshOptDemoShader					= NULL;
Shader*				pFloorShader						= NULL;
Shader*				pWaterMarkShader					= NULL;
Shader*				pSkyboxShader						= NULL;
Shader*				pFinalPostProcessShader				= NULL;
Shader*				pTemporalAAShader					= NULL;

Pipeline*			pPipelineShadowPass					= NULL;
Pipeline*			pPipelineShadowPass_NonOPtimized	= NULL;
Pipeline*			pMeshOptDemoPipeline				= NULL;
Pipeline*			pFloorPipeline						= NULL;
Pipeline*			pWaterMarkPipeline					= NULL;
Pipeline*			pSkyboxPipeline						= NULL;
Pipeline*			pFinalPostProcessPipeline			= NULL;
Pipeline*			pTemporalAAPipeline					= NULL;

RootSignature*		pRootSignatureShadow				= NULL;
RootSignature*		pRootSignatureShaded				= NULL;
RootSignature*		pRootSignaturePostEffects			= NULL;
RootSignature*		pRootSignatureSkybox				= NULL;
RootSignature*		pRootSignatureFinalPostProcess		= NULL;
RootSignature*		pRootSignatureTemporalAA			= NULL;

uint32_t            gShadowRootConstantIndex = 0;
uint32_t            gShadedRootConstantIndex = 0;
uint32_t            gPostProcessRootConstantIndex = 0;

DescriptorSet*      pDescriptorSetWatermark;
DescriptorSet*      pDescriptorSetSkybox;
DescriptorSet*      pDescriptorSetsFinalPostProcess[3];
DescriptorSet*      pDescriptorSetsTemporalAA[2];
DescriptorSet*      pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_COUNT];
DescriptorSet*      pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_COUNT];

Buffer*				pUniformBuffer[gImageCount]			= { NULL };
Buffer*				pShadowUniformBuffer[gImageCount]	= { NULL };
Buffer*				pFloorUniformBuffer[gImageCount]	= { NULL };

Buffer*				pSkyboxUniformBuffer[gImageCount]	= { NULL };
Buffer*				pTAAUniformBuffer[gImageCount]		= { NULL };

Buffer*				TriangularVB						= NULL;
Buffer*				pFloorVB							= NULL;
Buffer*				pFloorIB							= NULL;
Buffer*				WaterMarkVB							= NULL;

Sampler*			pDefaultSampler						= NULL;
Sampler*			pBilinearClampSampler				= NULL;
Sampler*			pPointClampSampler					= NULL;

UniformBlock		gUniformData;
UniformBlock_Floor	gFloorUniformBlock;
UniformBlock_Shadow gShadowUniformData;

//--------------------------------------------------------------------------------------------
// THE FORGE OBJECTS
//--------------------------------------------------------------------------------------------

ICameraController*	pCameraController					= NULL;
ICameraController*	pLightView							= NULL;

UIComponent*		pGuiWindow;
UIComponent*		pGuiGraphics;
UIWidget*			pSelectLodWidget					= NULL;

#if defined(__ANDROID__) || defined(__linux__) || defined(ORBIS) || defined(PROSPERO)
uint32_t			modelToLoadIndex					= 0;
uint32_t			guiModelToLoadIndex					= 0;
#endif

const char*		gMissingTextureString				= "MissingTexture";

const char*					    gModelFile;
uint32_t						gPreviousLoadedModel = mModelSelected;

static float4		gLightColor[gTotalLightCount] = { float4(1.f), float4(1.f), float4(1.f), float4(1.f, 1.f, 1.f, 0.25f) };
static float		gLightColorIntensity[gTotalLightCount] = { 0.1f, 0.2f, 0.2f, 0.25f };
static float2		gLightDirection = { -122.0f, 222.0f };

GLTFAsset gCurrentAsset;

Texture* gEnvironmentMapIem = NULL;
Texture* gEnvironmentMapPmrem = NULL;
Texture* gEnvironmentBRDF = NULL;

FontDrawDesc gFrameTimeDraw; 
uint32_t gFontID = 0; 

const char* gTestScripts[] = { "Test_Lantern.lua", "Test_FlightHelmet.lua", "Test_capsule.lua", "Test_cube.lua", "Test_lion.lua", "Test_matBall.lua" };
uint32_t gScriptIndexes[] = { 0, 1, 2, 3, 4, 5 };
uint32_t gCurrentScriptIndex = 0;
void RunScript()
{
	LuaScriptDesc runDesc = {};
	runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
	luaQueueScriptToRun(&runDesc);
}

static HiresTimer gTimer;

class GLTFViewer : public IApp
{
public:
	static bool InitShaderResources()
	{
		// shader
		
		ShaderLoadDesc FloorShader = {};
		
		FloorShader.mStages[0] = {"floor.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_NONE};

		addShader(pRenderer, &FloorShader, &pShaderZPass_NonOptimized);

		FloorShader.mStages[0] = {"floor.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW};
		FloorShader.mStages[1] = { "floor.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		
		addShader(pRenderer, &FloorShader, &pFloorShader);
		
		ShaderLoadDesc MeshOptDemoShader = {};
		
		MeshOptDemoShader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_NONE};

		addShader(pRenderer, &MeshOptDemoShader, &pShaderZPass);

		MeshOptDemoShader.mStages[0] = {"basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW};
		MeshOptDemoShader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		
		addShader(pRenderer, &MeshOptDemoShader, &pMeshOptDemoShader);

		ShaderLoadDesc FinalPostProcessShader = {};

		FinalPostProcessShader.mStages[0] = { "Triangular.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		FinalPostProcessShader.mStages[1] = { "FinalPostProcess.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &FinalPostProcessShader, &pFinalPostProcessShader);

		ShaderLoadDesc TemporalAAShader = {};

		TemporalAAShader.mStages[0] = { "Triangular.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		TemporalAAShader.mStages[1] = { "TemporalAA.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &TemporalAAShader, &pTemporalAAShader);
		
		ShaderLoadDesc WaterMarkShader = {};
		
		WaterMarkShader.mStages[0] = { "watermark.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		WaterMarkShader.mStages[1] = { "watermark.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		
		addShader(pRenderer, &WaterMarkShader, &pWaterMarkShader);

		ShaderLoadDesc SkyboxShader = {};

		SkyboxShader.mStages[0] = { "Skybox.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		SkyboxShader.mStages[1] = { "Skybox.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &SkyboxShader, &pSkyboxShader);
		
		const char* pStaticSamplerNames[] = { "clampMiplessLinearSampler", "clampMiplessPointSampler", "cubemapSampler"};
		Sampler* pStaticSamplers[] = { pBilinearClampSampler, pPointClampSampler, pDefaultSampler };

		Shader*           shaders[] = { pShaderZPass, pShaderZPass_NonOptimized };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 3;
		rootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		rootDesc.ppStaticSamplers = pStaticSamplers;
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureShadow);
		gShadowRootConstantIndex = getDescriptorIndexFromName(pRootSignatureShadow, "cbRootConstants");
		
		Shader*  demoShaders[] = { pMeshOptDemoShader, pFloorShader };
		
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = demoShaders;
		
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureShaded);
		gShadedRootConstantIndex = getDescriptorIndexFromName(pRootSignatureShaded, "cbRootConstants");

		Shader* postShaders[] = { pWaterMarkShader };
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = postShaders;

		addRootSignature(pRenderer, &rootDesc, &pRootSignaturePostEffects);

		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pFinalPostProcessShader;

		addRootSignature(pRenderer, &rootDesc, &pRootSignatureFinalPostProcess);
		gPostProcessRootConstantIndex = getDescriptorIndexFromName(pRootSignatureFinalPostProcess, "PostProcessRootConstant");

		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pTemporalAAShader;

		addRootSignature(pRenderer, &rootDesc, &pRootSignatureTemporalAA);

		const char* skyboxSamplerNames[] = { "skyboxTextureSampler" };
		rootDesc.ppStaticSamplerNames = skyboxSamplerNames;
		rootDesc.ppStaticSamplers = &pDefaultSampler;
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pSkyboxShader;

		addRootSignature(pRenderer, &rootDesc, &pRootSignatureSkybox);
		
		if (!AddDescriptorSets())
			return false;
		
		return true;
	}
	
	static bool InitModelDependentResources()
	{
		// Create vertex layout
		gVertexLayoutModel.mAttribCount = 3;

		gVertexLayoutModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutModel.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutModel.mAttribs[0].mBinding = 0;
		gVertexLayoutModel.mAttribs[0].mLocation = 0;
		gVertexLayoutModel.mAttribs[0].mOffset = 0;

		//normals
		gVertexLayoutModel.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutModel.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutModel.mAttribs[1].mLocation = 1;
		gVertexLayoutModel.mAttribs[1].mBinding = 0;
		gVertexLayoutModel.mAttribs[1].mOffset = 3 * sizeof(float);

		//texture
		gVertexLayoutModel.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutModel.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutModel.mAttribs[2].mLocation = 2;
		gVertexLayoutModel.mAttribs[2].mBinding = 0;
		gVertexLayoutModel.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats

		if (!LoadModel(gCurrentAsset, gModelFile))
			return false;
		
		if (!InitShaderResources())
			return false;

		waitForAllResourceLoads();
		
		PrepareDescriptorSets();
		
		return true;
	}
	
	static void setRenderTarget(Cmd* cmd, uint32_t count, RenderTarget** pDestinationRenderTargets, RenderTarget* pDepthStencilTarget, LoadActionsDesc* loadActions)
	{
		if (count == 0 && pDestinationRenderTargets == NULL && pDepthStencilTarget == NULL)
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		else
		{
			ASSERT(pDepthStencilTarget || (pDestinationRenderTargets && pDestinationRenderTargets[0]));

			cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
			// sets the rectangles to match with first attachment, I know that it's not very portable.
			RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0]; //-V522
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSizeTarget->mWidth, (float)pSizeTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSizeTarget->mWidth, pSizeTarget->mHeight);
		}
	}
	
	static void drawShadowMap(Cmd* cmd)
	{
		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pShadowUniformBuffer[gFrameIndex] };
        beginUpdateResource(&shaderCbv);
		*(UniformBlock_Shadow*)shaderCbv.pMappedData = gShadowUniformData;
        endUpdateResource(&shaderCbv, NULL);
        
		RenderTargetBarrier barriers[] =
		{
			{ pShadowRT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
		
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pShadowRT->mClearValue;
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Shadow Map");
		// Start render pass and apply load actions
		setRenderTarget(cmd, 0, NULL, pShadowRT, &loadActions);
		
		cmdBindPipeline(cmd, pPipelineShadowPass_NonOPtimized);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
#ifdef ORBIS
		cmdBindDescriptorSet(cmd, 0, gCurrentAsset.pMaterialSet);
#endif
		
		const uint32_t stride = sizeof(float) * 5;
		cmdBindVertexBuffer(cmd, 1, &pFloorVB, &stride, NULL);
		cmdBindIndexBuffer(cmd, pFloorIB, INDEX_TYPE_UINT16, 0);
		
		cmdDrawIndexed(cmd, 6, 0, 0);
		
		cmdBindPipeline(cmd, pPipelineShadowPass);
		cmdBindVertexBuffer(cmd, 1, &gCurrentAsset.pGeom->pVertexBuffers[0], &gCurrentAsset.pGeom->mVertexStrides[0], NULL);
		cmdBindIndexBuffer(cmd, gCurrentAsset.pGeom->pIndexBuffer, gCurrentAsset.pGeom->mIndexType, 0);

		MeshPushConstants pushConstants = {};

		for (uint32_t n = 0; n < gCurrentAsset.pData->mNodeCount; ++n)
		{
			GLTFNode& node = gCurrentAsset.pData->pNodes[n];
			if (node.mMeshIndex != UINT_MAX)
			{
				cmdBindPushConstants(cmd, pRootSignatureShadow, gShadowRootConstantIndex, &pushConstants);
				for (uint32_t i = 0; i < node.mMeshCount; ++i)
				{
					GLTFMesh& mesh = gCurrentAsset.pData->pMeshes[node.mMeshIndex + i];
					cmdDrawIndexed(cmd, mesh.mIndexCount, mesh.mStartIndex, 0);
				}
			}

			pushConstants.nodeIndex += 1;
		}
	
		setRenderTarget(cmd, 0, NULL, NULL, NULL);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
	}
	
	bool Init()
	{
		initHiresTimer(&gTimer);

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,          "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,		   "Scripts");

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
		LuaScriptDesc scriptDescs[numScripts] = {};
		for (uint32_t i = 0; i < numScripts; ++i)
			scriptDescs[i].pScriptFileName = gTestScripts[i];
		luaDefineScripts(scriptDescs, numScripts);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		SamplerDesc defaultSamplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT
		};
		addSampler(pRenderer, &defaultSamplerDesc, &pDefaultSampler);

		SamplerDesc samplerClampDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
		};
		addSampler(pRenderer, &samplerClampDesc, &pBilinearClampSampler);

		SamplerDesc samplerPointDesc = {
			FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
		};
		addSampler(pRenderer, &samplerPointDesc, &pPointClampSampler);

		float floorPoints[] = {
			-1.0f, 0.0f, 1.0f, -1.0f, -1.0f,
			-1.0f, 0.0f, -1.0f, -1.0f, 1.0f,
			1.0f, 0.0f, -1.0f, 1.0f, 1.0f,
			1.0f, 0.0f, 1.0f, 1.0f, -1.0f,
		};

		BufferLoadDesc floorVbDesc = {};
		floorVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		floorVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		floorVbDesc.mDesc.mSize = sizeof(float) * 5 * 4;
		floorVbDesc.pData = floorPoints;
		floorVbDesc.ppBuffer = &pFloorVB;
		addResource(&floorVbDesc, NULL);

		uint16_t floorIndices[] =
		{
			0, 1, 3,
			3, 1, 2
		};

		BufferLoadDesc indexBufferDesc = {};
		indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		indexBufferDesc.mDesc.mSize = sizeof(uint16_t) * 6;
		indexBufferDesc.pData = floorIndices;
		indexBufferDesc.ppBuffer = &pFloorIB;
		addResource(&indexBufferDesc, NULL);

		float screenTriangularPoints[] =
		{
			-1.0f,  3.0f, 0.5f, 0.0f, -1.0f,
			-1.0f, -1.0f, 0.5f, 0.0f, 1.0f,
			3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mSize = sizeof(float) * 5 * 3;
		screenQuadVbDesc.pData = screenTriangularPoints;
		screenQuadVbDesc.ppBuffer = &TriangularVB;
		addResource(&screenQuadVbDesc, NULL);

		TextureDesc defaultTextureDesc = {};
		defaultTextureDesc.mArraySize = 1;
		defaultTextureDesc.mDepth = 1;
		defaultTextureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		defaultTextureDesc.mWidth = 4;
		defaultTextureDesc.mHeight = 4;
		defaultTextureDesc.mMipLevels = 1;
		defaultTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		defaultTextureDesc.mStartState = RESOURCE_STATE_COMMON;
		defaultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		defaultTextureDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		defaultTextureDesc.pName = gMissingTextureString;
		TextureLoadDesc defaultLoadDesc = {};
		defaultLoadDesc.pDesc = &defaultTextureDesc;
		defaultLoadDesc.ppTexture = &pTextureBlack;
		addResource(&defaultLoadDesc, NULL);

		TextureUpdateDesc updateDesc = { pTextureBlack };
		beginUpdateResource(&updateDesc);
		memset(updateDesc.pMappedData, 0, 4 * 4 * sizeof(uint32_t));
		endUpdateResource(&updateDesc, NULL);

		gModelFile = pModelNames[mModelSelected];

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		BufferLoadDesc subDesc = {};
		subDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		subDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		subDesc.mDesc.mSize = sizeof(UniformBlock_Shadow);
		subDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		subDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			subDesc.ppBuffer = &pShadowUniformBuffer[i];
			addResource(&subDesc, NULL);
		}

		ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pFloorUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(TAAUniformBuffer);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pTAAUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(SkyboxUniformBuffer);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		/************************************************************************/
		// GUI
		/************************************************************************/
		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

		SeparatorWidget separator;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

		DropdownWidget loadModelWidget;
		loadModelWidget.pData = &mModelSelected;
		for (uint32_t i = 0; i < mModelCount; ++i)
		{
			loadModelWidget.mNames.push_back((char*)pModelNames[i]);
			loadModelWidget.mValues.push_back(gModelValues[i]);
		}
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Load Model", &loadModelWidget, WIDGET_TYPE_DROPDOWN));

		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

		SliderIntWidget lodSlider;
		lodSlider.pData = &gCurrentLod;
		lodSlider.mMin = 0;
		lodSlider.mMax = gMaxLod;
		pSelectLodWidget = uiCreateComponentWidget(pGuiWindow, "LOD", &lodSlider, WIDGET_TYPE_SLIDER_INT);
		luaRegisterWidget(pSelectLodWidget);

		////////////////////////////////////////////////////////////////////////////////////////////

		guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.35f);
		uiCreateComponent("Graphics Options", &guiDesc, &pGuiGraphics);

		CheckboxWidget temporalAACheckbox;
		temporalAACheckbox.pData = &bEnableTemporalAA;
		luaRegisterWidget(uiCreateComponentWidget(pGuiGraphics, "Enable Temporal AA", &temporalAACheckbox, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget vignetteCheckbox;
		vignetteCheckbox.pData = &bEnableVignette;
		luaRegisterWidget(uiCreateComponentWidget(pGuiGraphics, "Enable Vignetting", &vignetteCheckbox, WIDGET_TYPE_CHECKBOX));

		luaRegisterWidget(uiCreateComponentWidget(pGuiGraphics, "", &separator, WIDGET_TYPE_SEPARATOR));

		CollapsingHeaderWidget LightWidgets;
		LightWidgets.mDefaultOpen = false;
		uiSetCollapsingHeaderWidgetCollapsed(&LightWidgets, false);

		SliderFloatWidget azimuthSlider;
		azimuthSlider.pData = &gLightDirection.x;
		azimuthSlider.mMin = float(-180.0f);
		azimuthSlider.mMax = float(180.0f);
		azimuthSlider.mStep = float(0.001f);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light Azimuth", &azimuthSlider, WIDGET_TYPE_SLIDER_FLOAT);

		SliderFloatWidget elevationSlider;
		elevationSlider.pData = &gLightDirection.y;
		elevationSlider.mMin = float(210.0f);
		elevationSlider.mMax = float(330.0f);
		elevationSlider.mStep = float(0.001f);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light Elevation", &elevationSlider, WIDGET_TYPE_SLIDER_FLOAT);

		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

		CollapsingHeaderWidget LightColor1Picker;
		ColorPickerWidget light1Picker;
		light1Picker.pData = &gLightColor[0];
		uiCreateCollapsingHeaderSubWidget(&LightColor1Picker, "Main Light Color", &light1Picker, WIDGET_TYPE_COLOR_PICKER);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Main Light Color", &LightColor1Picker, WIDGET_TYPE_COLLAPSING_HEADER);

		CollapsingHeaderWidget LightColor1Intensity;
		SliderFloatWidget lightIntensitySlider;
		lightIntensitySlider.pData = &gLightColorIntensity[0];
		lightIntensitySlider.mMin = 0.0f;
		lightIntensitySlider.mMax = 5.0f;
		lightIntensitySlider.mStep = 0.001f;
		uiCreateCollapsingHeaderSubWidget(&LightColor1Intensity, "Main Light Intensity", &lightIntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Main Light Intensity", &LightColor1Intensity, WIDGET_TYPE_COLLAPSING_HEADER);

		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

		CollapsingHeaderWidget LightColor2Picker;
		ColorPickerWidget light2Picker;
		light2Picker.pData = &gLightColor[1];
		uiCreateCollapsingHeaderSubWidget(&LightColor2Picker, "Light2 Color", &light2Picker, WIDGET_TYPE_COLOR_PICKER);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light2 Color", &LightColor2Picker, WIDGET_TYPE_COLLAPSING_HEADER);

		CollapsingHeaderWidget LightColor2Intensity;
		SliderFloatWidget light2IntensitySlider;
		light2IntensitySlider.pData = &gLightColorIntensity[1];
		light2IntensitySlider.mMin = 0.0f;
		light2IntensitySlider.mMax = 5.0f;
		light2IntensitySlider.mStep = 0.001f;
		uiCreateCollapsingHeaderSubWidget(&LightColor2Intensity, "Light2 Intensity", &light2IntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light2 Intensity", &LightColor2Intensity, WIDGET_TYPE_COLLAPSING_HEADER);

		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

		CollapsingHeaderWidget LightColor3Picker;
		ColorPickerWidget light3Picker;
		light3Picker.pData = &gLightColor[2];
		uiCreateCollapsingHeaderSubWidget(&LightColor3Picker, "Light3 Color", &light3Picker, WIDGET_TYPE_COLOR_PICKER);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light3 Color", &LightColor3Picker, WIDGET_TYPE_COLLAPSING_HEADER);

		CollapsingHeaderWidget LightColor3Intensity;
		SliderFloatWidget light3IntensitySlider;
		light3IntensitySlider.pData = &gLightColorIntensity[2];
		light3IntensitySlider.mMin = 0.0f;
		light3IntensitySlider.mMax = 5.0f;
		light3IntensitySlider.mStep = 0.001f;
		uiCreateCollapsingHeaderSubWidget(&LightColor3Intensity, "Light3 Intensity", &light3IntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Light3 Intensity", &LightColor3Intensity, WIDGET_TYPE_COLLAPSING_HEADER);

		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

		CollapsingHeaderWidget AmbientLightColorPicker;
		ColorPickerWidget ambientPicker;
		ambientPicker.pData = &gLightColor[3];
		uiCreateCollapsingHeaderSubWidget(&AmbientLightColorPicker, "Ambient Light Color", &ambientPicker, WIDGET_TYPE_COLOR_PICKER);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Ambient Light Color", &AmbientLightColorPicker, WIDGET_TYPE_COLLAPSING_HEADER);

		CollapsingHeaderWidget AmbientColorIntensity;
		SliderFloatWidget ambientIntensitySlider;
		ambientIntensitySlider.pData = &gLightColorIntensity[3];
		ambientIntensitySlider.mMin = 0.0f;
		ambientIntensitySlider.mMax = 5.0f;
		ambientIntensitySlider.mStep = 0.001f;
		uiCreateCollapsingHeaderSubWidget(&AmbientColorIntensity, "Ambient Light Intensity", &ambientIntensitySlider, WIDGET_TYPE_SLIDER_FLOAT);
		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "Ambient Light Intensity", &AmbientColorIntensity, WIDGET_TYPE_COLLAPSING_HEADER);

		uiCreateCollapsingHeaderSubWidget(&LightWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

		luaRegisterWidget(uiCreateComponentWidget(pGuiGraphics, "Light Options", &LightWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

		DropdownWidget ddTestScripts;
		ddTestScripts.pData = &gCurrentScriptIndex;
		for (uint32_t i = 0; i < sizeof(gTestScripts) / sizeof(gTestScripts[0]); ++i)
		{
			ddTestScripts.mNames.push_back((char*)gTestScripts[i]);
			ddTestScripts.mValues.push_back(gScriptIndexes[i]);
		}
		luaRegisterWidget(uiCreateComponentWidget(pGuiGraphics, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

		ButtonWidget bRunScript;
		UIWidget* pRunScript = uiCreateComponentWidget(pGuiGraphics, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pRunScript, RunScript);
		luaRegisterWidget(pRunScript);

		waitForAllResourceLoads();

		CameraMotionParameters cmp{ 1.0f, 120.0f, 40.0f };
		vec3                   camPos{ 3.0f, 2.5f, -4.0f };
		vec3                   lookAt{ 0.0f, 0.4f, 0.0f };
		
		pLightView = initGuiCameraController(camPos, lookAt);
		pCameraController = initFpsCameraController(normalize(camPos) * 3.0f, lookAt);
		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;
		
		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			bool capture = uiOnInput(ctx->mBinding, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			if(ctx->mBinding != InputBindings::FLOAT_LEFTSTICK)
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
			return true;
		};
		actionDesc = { InputBindings::BUTTON_ANY, onUIInput, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, onUIInput, this, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*ctx->pCaptured)
			{
				float2 val = uiIsFocused() ? float2(0.0f) : ctx->mFloat2;
				index ? pCameraController->onRotate(val) : pCameraController->onMove(val);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.25f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 0.25f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		gFrameIndex = 0;

		return true;
	}
	
	static bool LoadModel(GLTFAsset& asset, const char* modelFileName)
	{
		//eastl::vector<PathHandle> validFileLists;

		gCurrentAsset.removeResources();
		
		GeometryLoadDesc loadDesc = {};
		loadDesc.ppGeometry = &gCurrentAsset.pGeom;
		loadDesc.pFileName = modelFileName;
		loadDesc.pVertexLayout = &gVertexLayoutModel;
		loadDesc.mOptimizationFlags = MESH_OPTIMIZATION_FLAG_ALL;
		addResource(&loadDesc, NULL);

		uint32_t res = gltfLoadContainer(modelFileName, NULL, GLTF_FLAG_CALCULATE_BOUNDS, &gCurrentAsset.pData);
		if (res)
		{
			return false;
		}
		
		asset.Init(pRenderer, pDefaultSampler);
		
		uiDestroyComponentWidget(pGuiWindow, pSelectLodWidget);
		//gMaxLod = max((int)validFileLists.size() - 1, 0);
		//pSelectLodWidget = pGuiWindow->AddWidget(SliderIntWidget("LOD", &gCurrentLod, 0, gMaxLod));
		waitForAllResourceLoads();
		return true;
	}
	
	static bool AddDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignaturePostEffects, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetWatermark);

		setDesc = { pRootSignatureShadow, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE]);
		setDesc = { pRootSignatureShadow, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
		
		setDesc = { pRootSignatureShaded, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE]);
		setDesc = { pRootSignatureShaded, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);

		setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox);

		setDesc = { pRootSignatureFinalPostProcess, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsFinalPostProcess[0]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsFinalPostProcess[1]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsFinalPostProcess[2]);

		setDesc = { pRootSignatureTemporalAA, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsTemporalAA[0]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsTemporalAA[1]);

		return true;
	}
	
	static void RemoveDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetWatermark);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox);
		removeDescriptorSet(pRenderer, pDescriptorSetsFinalPostProcess[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetsFinalPostProcess[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetsFinalPostProcess[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetsTemporalAA[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetsTemporalAA[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
	}
	
	static void PrepareDescriptorSets()
	{
		// Shadow
		{
			DescriptorData params[4] = {};
			if (gCurrentAsset.pNodeTransformsBuffer)
			{
				params[0].pName = "modelToWorldMatrices";
				params[0].ppBuffers = &gCurrentAsset.pNodeTransformsBuffer;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE], 1, params);
			}

			RenderTarget* historyInput = pTemporalAAHistoryRT[gCurrentTemporalAARenderTarget];
			RenderTarget* historyOutput = pTemporalAAHistoryRT[!gCurrentTemporalAARenderTarget];

			{
				params[0].pName = "sceneTexture";
				params[0].ppTextures = &pForwardRT->pTexture;

				params[2].pName = "depthTexture";
				params[2].ppTextures = &pDepthBuffer->pTexture;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					params[1].pName = "historyTexture";
					params[1].ppTextures = &historyInput->pTexture;

					params[3].pName = "TAAUniformBuffer";
					params[3].ppBuffers = &pTAAUniformBuffer[i];
					updateDescriptorSet(pRenderer, i, pDescriptorSetsTemporalAA[0], 4, params);

					params[1].pName = "historyTexture";
					params[1].ppTextures = &historyOutput->pTexture;

					params[3].pName = "TAAUniformBuffer";
					params[3].ppBuffers = &pTAAUniformBuffer[i];
					updateDescriptorSet(pRenderer, i, pDescriptorSetsTemporalAA[1], 4, params);
				}
			}

			{
				params[0].pName = "sceneTexture";
				params[0].ppTextures = &pForwardRT->pTexture;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetsFinalPostProcess[0], 1, params);

				params[0].pName = "sceneTexture";
				params[0].ppTextures = &historyOutput->pTexture;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetsFinalPostProcess[1], 1, params);

				params[0].pName = "sceneTexture";
				params[0].ppTextures = &historyInput->pTexture;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetsFinalPostProcess[2], 1, params);
			}

			{
				params[0].pName = "skyboxTexture";
				params[0].ppTextures = &gEnvironmentMapPmrem;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					params[1].pName = "SkyboxUniformBuffer";
					params[1].ppBuffers = &pSkyboxUniformBuffer[i];
					updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox, 2, params);
				}
			}

			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pTextureBlack;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetWatermark, 1, params);
			
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData params[2] = {};
				params[0].pName = "cbPerPass";
				params[0].ppBuffers = &pShadowUniformBuffer[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 1, params);
			}
		}
		// Shading
		{
			DescriptorData params[5] = {};
			params[0].pName = "ShadowTexture";
			params[0].ppTextures = &pShadowRT->pTexture;

			params[1].pName = "iemCubemap";
			params[1].ppTextures = &gEnvironmentMapIem;

			params[2].pName = "pmremCubemap";
			params[2].ppTextures = &gEnvironmentMapPmrem;

			params[3].pName = "environmentBRDF";
			params[3].ppTextures = &gEnvironmentBRDF;

			if (gCurrentAsset.pNodeTransformsBuffer)
			{
				params[4].pName = "modelToWorldMatrices";
				params[4].ppBuffers = &gCurrentAsset.pNodeTransformsBuffer;
			}
			updateDescriptorSet(pRenderer, 0, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE], gCurrentAsset.pNodeTransformsBuffer ? 5 : 4, params);
			
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cbPerPass";
				params[0].ppBuffers = &pUniformBuffer[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME], 1, params);
			}
		}
		
		gCurrentAsset.createMaterialResources(pRootSignatureShaded, /* bindlessTexturesSamplersSet = */ NULL);
	}
	
	static void RemoveShaderResources()
	{
		RemoveDescriptorSets();
		
		removeShader(pRenderer, pShaderZPass);
		removeShader(pRenderer, pShaderZPass_NonOptimized);
		removeShader(pRenderer, pFloorShader);
		removeShader(pRenderer, pMeshOptDemoShader);
		removeShader(pRenderer, pWaterMarkShader);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pFinalPostProcessShader);
		removeShader(pRenderer, pTemporalAAShader);
		
		removeRootSignature(pRenderer, pRootSignatureShadow);
		removeRootSignature(pRenderer, pRootSignatureShaded);
		removeRootSignature(pRenderer, pRootSignaturePostEffects);
		removeRootSignature(pRenderer, pRootSignatureSkybox);
		removeRootSignature(pRenderer, pRootSignatureFinalPostProcess);
		removeRootSignature(pRenderer, pRootSignatureTemporalAA);
	}
	
	static void RemoveModelDependentResources()
	{
		gCurrentAsset.removeResources();
		RemoveShaderResources();
		gCurrentAsset = GLTFAsset();
	}
	
	void Exit()
	{
		exitInputSystem();

		exitCameraController(pCameraController);

		exitCameraController(pLightView);

		gModelFile = NULL;

		exitProfiler();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pShadowUniformBuffer[i]);
			removeResource(pUniformBuffer[i]);
			removeResource(pFloorUniformBuffer[i]);
			removeResource(pSkyboxUniformBuffer[i]);
			removeResource(pTAAUniformBuffer[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		removeSampler(pRenderer, pDefaultSampler);
		removeSampler(pRenderer, pBilinearClampSampler);
		removeSampler(pRenderer, pPointClampSampler);

		removeResource(TriangularVB);

		removeResource(pFloorVB);
		removeResource(pFloorIB);

		removeResource(pTextureBlack);

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}
	
	static void LoadPipelines()
	{
		PipelineDesc desc = {};
		
		/************************************************************************/
		// Setup the resources needed for shadow map
		/************************************************************************/
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;

		DepthStateDesc skyboxStateDesc = {};
		skyboxStateDesc.mDepthTest = true;
		skyboxStateDesc.mDepthWrite = false;
		skyboxStateDesc.mDepthFunc = CMP_EQUAL;

		BlendStateDesc blendStateAlphaDesc = {};
		blendStateAlphaDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateAlphaDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateAlphaDesc.mBlendModes[0] = BM_ADD;
		blendStateAlphaDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateAlphaDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateAlphaDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateAlphaDesc.mMasks[0] = ALL;
		blendStateAlphaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateAlphaDesc.mIndependentBlend = false;

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
			shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			shadowMapPipelineSettings.mRenderTargetCount = 0;
			shadowMapPipelineSettings.pDepthState = &depthStateDesc;
			shadowMapPipelineSettings.mDepthStencilFormat = pShadowRT->mFormat;
			shadowMapPipelineSettings.mSampleCount = pShadowRT->mSampleCount;
			shadowMapPipelineSettings.mSampleQuality = pShadowRT->mSampleQuality;
			shadowMapPipelineSettings.pRootSignature = pRootSignatureShadow;
			shadowMapPipelineSettings.pRasterizerState = &rasterizerStateDesc;
			shadowMapPipelineSettings.pShaderProgram = pShaderZPass;
			shadowMapPipelineSettings.pVertexLayout = &gVertexLayoutModel;
			addPipeline(pRenderer, &desc, &pPipelineShadowPass);
		}
		
		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = &depthStateDesc;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
			pipelineSettings.pBlendState = &blendStateAlphaDesc;
			pipelineSettings.pColorFormats = &pForwardRT->mFormat;
			pipelineSettings.mSampleCount = pForwardRT->mSampleCount;
			pipelineSettings.mSampleQuality = pForwardRT->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignatureShaded;
			pipelineSettings.pVertexLayout = &gVertexLayoutModel;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pMeshOptDemoShader;
			addPipeline(pRenderer, &desc, &pMeshOptDemoPipeline);
		}
		
		VertexLayout screenTriangle_VertexLayout = {};
		
		screenTriangle_VertexLayout.mAttribCount = 2;
		screenTriangle_VertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		screenTriangle_VertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		screenTriangle_VertexLayout.mAttribs[0].mBinding = 0;
		screenTriangle_VertexLayout.mAttribs[0].mLocation = 0;
		screenTriangle_VertexLayout.mAttribs[0].mOffset = 0;
		screenTriangle_VertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		screenTriangle_VertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		screenTriangle_VertexLayout.mAttribs[1].mBinding = 0;
		screenTriangle_VertexLayout.mAttribs[1].mLocation = 1;
		screenTriangle_VertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
		
		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
			shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			shadowMapPipelineSettings.mRenderTargetCount = 0;
			shadowMapPipelineSettings.pDepthState = &depthStateDesc;
			shadowMapPipelineSettings.mDepthStencilFormat = pShadowRT->mFormat;
			shadowMapPipelineSettings.mSampleCount = pShadowRT->mSampleCount;
			shadowMapPipelineSettings.mSampleQuality = pShadowRT->mSampleQuality;
			shadowMapPipelineSettings.pRootSignature = pRootSignatureShadow;
			shadowMapPipelineSettings.pRasterizerState = &rasterizerStateDesc;
			shadowMapPipelineSettings.pShaderProgram = pShaderZPass_NonOptimized;
			shadowMapPipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			addPipeline(pRenderer, &desc, &pPipelineShadowPass_NonOPtimized);
		}
		
		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = &depthStateDesc;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
			pipelineSettings.pBlendState = &blendStateAlphaDesc;
			pipelineSettings.pColorFormats = &pForwardRT->mFormat;
			pipelineSettings.mSampleCount = pForwardRT->mSampleCount;
			pipelineSettings.mSampleQuality = pForwardRT->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignatureShaded;
			pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pFloorShader;
			addPipeline(pRenderer, &desc, &pFloorPipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pBlendState = &blendStateAlphaDesc;
			pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignaturePostEffects;
			pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pWaterMarkShader;
			addPipeline(pRenderer, &desc, &pWaterMarkPipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = &skyboxStateDesc;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
			pipelineSettings.pBlendState = NULL;
			pipelineSettings.pColorFormats = &pForwardRT->mFormat;
			pipelineSettings.mSampleCount = pForwardRT->mSampleCount;
			pipelineSettings.mSampleQuality = pForwardRT->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignatureSkybox;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pSkyboxShader;
			addPipeline(pRenderer, &desc, &pSkyboxPipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pBlendState = NULL;
			pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignatureFinalPostProcess;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pFinalPostProcessShader;
			addPipeline(pRenderer, &desc, &pFinalPostProcessPipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pBlendState = NULL;
			pipelineSettings.pColorFormats = &pForwardRT->mFormat;
			pipelineSettings.mSampleCount = pForwardRT->mSampleCount;
			pipelineSettings.mSampleQuality = pForwardRT->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignatureTemporalAA;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pTemporalAAShader;
			addPipeline(pRenderer, &desc, &pTemporalAAPipeline);
		}
	}

	bool mModelReload = false;
	
	bool Load()
	{
		if (!mModelReload)
		{
			if (!addSwapChain())
				return false;

			if (!addRenderTargets())
				return false;

			if (!addDepthBuffer())
				return false;

			RenderTarget* ppPipelineRenderTargets[] = {
			pSwapChain->ppRenderTargets[0],
			pDepthBuffer
			};

			if (!addFontSystemPipelines(ppPipelineRenderTargets, 2, NULL))
				return false;

			if (!addUserInterfacePipelines(ppPipelineRenderTargets[0]))
				return false;

			float wmHeight = min(mSettings.mWidth, mSettings.mHeight) * 0.09f;
			float wmWidth = wmHeight * 2.8077f;

			float widthGap = wmWidth * 2.0f / (float)mSettings.mWidth;
			float heightGap = wmHeight * 2.0f / (float)mSettings.mHeight;

			float pixelGap = 80.0f;
			float widthRight = 1.0f - pixelGap / (float)mSettings.mWidth;
			float heightDown = -1.0f + pixelGap / (float)mSettings.mHeight;

			float screenWaterMarkPoints[] = {
				widthRight - widthGap,	heightDown + heightGap, 0.5f, 0.0f, 0.0f,
				widthRight - widthGap,	heightDown,				0.5f, 0.0f, 1.0f,
				widthRight,				heightDown + heightGap, 0.5f, 1.0f, 0.0f,

				widthRight,				heightDown + heightGap, 0.5f, 1.0f, 0.0f,
				widthRight - widthGap,	heightDown,				0.5f, 0.0f, 1.0f,
				widthRight,				heightDown,				0.5f, 1.0f, 1.0f
			};

			BufferLoadDesc screenQuadVbDesc = {};
			screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			screenQuadVbDesc.mDesc.mSize = sizeof(float) * 5 * 6;
			screenQuadVbDesc.pData = screenWaterMarkPoints;
			screenQuadVbDesc.ppBuffer = &WaterMarkVB;
			addResource(&screenQuadVbDesc, NULL);

			TextureLoadDesc loadDesc = {};
			loadDesc.mContainer = TEXTURE_CONTAINER_KTX;

			SyncToken token = {};

			loadDesc.ppTexture = &gEnvironmentMapIem;
			loadDesc.pFileName = "autumn_hockey_4k_iem";
			addResource(&loadDesc, &token);

			loadDesc.ppTexture = &gEnvironmentMapPmrem;
			loadDesc.pFileName = "autumn_hockey_4k_pmrem";
			addResource(&loadDesc, &token);

			loadDesc.ppTexture = &gEnvironmentBRDF;
			loadDesc.pFileName = "brdf";
			addResource(&loadDesc, &token);
		}

		InitModelDependentResources();
		
		LoadPipelines();
		
		return true;
	}
	
	static void RemovePipelines()
	{
		removePipeline(pRenderer, pPipelineShadowPass_NonOPtimized);
		removePipeline(pRenderer, pPipelineShadowPass);
		removePipeline(pRenderer, pFloorPipeline);
		removePipeline(pRenderer, pMeshOptDemoPipeline);
		removePipeline(pRenderer, pWaterMarkPipeline);
		removePipeline(pRenderer, pSkyboxPipeline);
		removePipeline(pRenderer, pFinalPostProcessPipeline);
		removePipeline(pRenderer, pTemporalAAPipeline);
	}
	
	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);
		waitForFences(pRenderer, gImageCount, pRenderCompleteFences);
		
		RemoveModelDependentResources();
		RemovePipelines();

		if (mModelReload)
		{
			return;
		}

		removeResource(gEnvironmentMapIem);
		removeResource(gEnvironmentMapPmrem);
		removeResource(gEnvironmentBRDF);

		removeResource(WaterMarkVB);

		removeUserInterfacePipelines();

		removeFontSystemPipelines(); 
		
		removeSwapChain(pRenderer, pSwapChain);

		removeRenderTarget(pRenderer, pForwardRT);
		removeRenderTarget(pRenderer, pTemporalAAHistoryRT[0]);
		removeRenderTarget(pRenderer, pTemporalAAHistoryRT[1]);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pShadowRT);
	}
	
	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		
		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 3.0f;
		CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.001f, 1000.0f);

		if (bEnableTemporalAA)
		{
			vec2 jitterSample = gTemporalAAJitterSamples[gCurrentTemporalAAJitterSample];

			CameraMatrix projMatNoJitter = projMat;

			projMat.applyProjectionSampleOffset(jitterSample.getX() / float(mSettings.mWidth), jitterSample.getY() / float(mSettings.mHeight));

			CameraMatrix projViewNoJitter = projMatNoJitter * viewMat;
			CameraMatrix invProjViewNoJitter = CameraMatrix::inverse(projViewNoJitter);
			gTemporalAAReprojection = gTemporalAAPreviousViewProjection * invProjViewNoJitter;
			gTemporalAAPreviousViewProjection = projViewNoJitter;
		}

		gUniformData.mProjectView = projMat * viewMat;
		gUniformData.mCameraPosition = vec4(pCameraController->getViewPosition(), 1.0f);
		
		for (uint i = 0; i < gTotalLightCount; ++i)
		{
			gUniformData.mLightColor[i] = gLightColor[i].toVec4();
			
			gUniformData.mLightColor[i].setW(gLightColorIntensity[i]);
		}
		
		float Azimuth = (PI / 180.0f) * gLightDirection.x;
		float Elevation = (PI / 180.0f) * (gLightDirection.y - 180.0f);
		
		vec3 sunDirection = normalize(vec3(cosf(Azimuth)*cosf(Elevation), sinf(Elevation), sinf(Azimuth)*cosf(Elevation)));
		
		gUniformData.mLightDirection[0] = vec4(sunDirection, 0.0f);
		// generate 2nd, 3rd light from the main light
		gUniformData.mLightDirection[1] = vec4(-sunDirection.getX(), sunDirection.getY(), -sunDirection.getZ(), 0.0f);
		gUniformData.mLightDirection[2] = vec4(-sunDirection.getX(), -sunDirection.getY(), -sunDirection.getZ(), 0.0f);
		
		gFloorUniformBlock.projViewMat = gUniformData.mProjectView;
		gFloorUniformBlock.worldMat = mat4::scale(vec3(3.0f));
		gFloorUniformBlock.screenSize = vec4((float)mSettings.mWidth, (float)mSettings.mHeight, 1.0f / mSettings.mWidth, bEnableVignette ? 1.0f : 0.0f);
		
		
		/************************************************************************/
		// Light Matrix Update - for shadow map
		/************************************************************************/
		
		vec3 lightPos = sunDirection * 4.0f;
		pLightView->moveTo(lightPos);
		pLightView->lookAt(vec3(0.0f));
		
		mat4 lightView = pLightView->getViewMatrix();
		//perspective as spotlight, for future use. TODO: Use a frustum fitting algorithm to maximise effective resolution!
		const float shadowRange = 2.7f;
		//const float shadowHalfRange = shadowRange * 0.5f;
		mat4 lightProjMat = mat4::orthographicReverseZ(-shadowRange, shadowRange, -shadowRange, shadowRange, shadowRange * 0.5f, shadowRange * 4.0f);
		
		gShadowUniformData.ViewProj = lightProjMat * lightView;
		gUniformData.mShadowLightViewProj = gShadowUniformData.ViewProj;
	}
	
	void PostDrawUpdate()
	{
		// HACK: This is a hack to avoid model reloading breaking TAA state.
		// The problem is that this sample reloads model by reloading almost everything, and that includes all
		//   root signatures, pipelines, shaders, etc. When this happens on the wrong frame this messes up TAA pipeline states.
		//
		// This code makes sure that reloading doesn't happen on odd TAA frames and the state of reloaded TAA pipelines matches
		//   what the runtime expects.
		// 
		// We should heavily refactor this sample at some point, especially the model loading code.
		bool allowReload = gCurrentTemporalAARenderTarget == 0;
		if (allowReload && gPreviousLoadedModel != mModelSelected)
		{
			gPreviousLoadedModel = mModelSelected;
			gModelFile = pModelNames[mModelSelected];
			
			mModelReload = true;
			Unload();
			Load();
			mModelReload = false;
		}
		gCurrentLod = min(gCurrentLod, gMaxLod);
		
		//
//
//		const LOD &currentLodMesh = gLODs[gCurrentLod];
//		const Model &model = currentLodMesh.model;
//
//		float distanceFromCamera = length(pCameraController->getViewPosition() - model.CenterPosition);
//
//		if (distanceFromCamera < 1.0f)
//			gCurrentLod = 0;
//		else
//			gCurrentLod = min((int)log2(pow((float)distanceFromCamera, 0.6f) + 1.0f), gMaxLod);
	}	
	
	static void LoadLOD()
	{
		waitQueueIdle(pGraphicsQueue);
		
		eastl::vector<const char*> extFilter;
		extFilter.push_back("gltf");
		extFilter.push_back("glb");
		
		//PathHandle meshDir = fsGetResourceDirEnumPath(RD_MESHES);
		
	//	fsShowOpenFileDialog("Select model to load", meshDir, SelectModelFunc, &gGuiModelToLoad, "Model File", &extFilter[0], extFilter.size());
	}
	
	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);
		
		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pUniformBuffer[gFrameIndex] };
		beginUpdateResource(&shaderCbv);
		*(UniformBlock*)shaderCbv.pMappedData = gUniformData;
		endUpdateResource(&shaderCbv, NULL);

		RenderTarget* pRenderTarget = NULL;
		
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		drawShadowMap(cmd);
		
		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0].r = 0.0F;
			loadActions.mClearColorValues[0].g = 0.0F;
			loadActions.mClearColorValues[0].b = 0.0F;
			loadActions.mClearColorValues[0].a = 0.0F;
			loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
			loadActions.mClearDepth.depth = 0.0f;
			loadActions.mClearDepth.stencil = 0;

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Floor");

			pRenderTarget = pForwardRT;

			RenderTargetBarrier barriers[] =
			{
				{ pRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pShadowRT, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE }
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);
            
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
			
			cmdBindPipeline(cmd, pFloorPipeline);
			
			// Update uniform buffers
			BufferUpdateDesc Cb = { pFloorUniformBuffer[gFrameIndex] };
			beginUpdateResource(&Cb);
			*(UniformBlock_Floor*)Cb.pMappedData = gFloorUniformBlock;
			endUpdateResource(&Cb, NULL);
			
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
			cmdBindDescriptorSet(cmd, 0, gCurrentAsset.pMaterialSet);

			const uint32_t stride = sizeof(float) * 5;
			cmdBindVertexBuffer(cmd, 1, &pFloorVB, &stride, NULL);
			cmdBindIndexBuffer(cmd, pFloorIB, INDEX_TYPE_UINT16, 0);
			
			cmdDrawIndexed(cmd, 6, 0, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}
		
		//// draw scene
		
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Scene");

			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
			
			cmdBindPipeline(cmd, pMeshOptDemoPipeline);
			
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
			cmdBindVertexBuffer(cmd, 1, &gCurrentAsset.pGeom->pVertexBuffers[0], &gCurrentAsset.pGeom->mVertexStrides[0], NULL);
			cmdBindIndexBuffer(cmd, gCurrentAsset.pGeom->pIndexBuffer, gCurrentAsset.pGeom->mIndexType, 0);

			MeshPushConstants pushConstants = {};

			for (uint32_t n = 0; n < gCurrentAsset.pData->mNodeCount; ++n)
			{
				GLTFNode& node = gCurrentAsset.pData->pNodes[n];
				if (node.mMeshIndex != UINT_MAX)
				{
					cmdBindPushConstants(cmd, pRootSignatureShaded, gShadedRootConstantIndex, &pushConstants);
					for (uint32_t i = 0; i < node.mMeshCount; ++i)
					{
						GLTFMesh& mesh = gCurrentAsset.pData->pMeshes[node.mMeshIndex + i];

						cmdBindDescriptorSet(cmd, (uint32_t)gCurrentAsset.pData->pMaterialIndices[node.mMeshIndex + i], gCurrentAsset.pMaterialSet);
						cmdDrawIndexed(cmd, mesh.mIndexCount, mesh.mStartIndex, 0);
					}
				}

				pushConstants.nodeIndex += 1;
			}

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Skybox");

			BufferUpdateDesc bufferUpdate = { pSkyboxUniformBuffer[gFrameIndex] };
			beginUpdateResource(&bufferUpdate);
			*(SkyboxUniformBuffer*)bufferUpdate.pMappedData = { CameraMatrix::inverse(gUniformData.mProjectView) };
			endUpdateResource(&bufferUpdate, NULL);

			cmdBindPipeline(cmd, pSkyboxPipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkybox);
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

		RenderTargetBarrier barriers[] = {
			{ pForwardRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
		};
		
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		DescriptorSet* postProcessDescriptorSet = pDescriptorSetsFinalPostProcess[0];
		if (bEnableTemporalAA)
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Temporal AA");

			BufferUpdateDesc bufferUpdate = { pTAAUniformBuffer[gFrameIndex] };
			beginUpdateResource(&bufferUpdate);
			*(TAAUniformBuffer*)bufferUpdate.pMappedData = { gTemporalAAReprojection };
			endUpdateResource(&bufferUpdate, NULL);

			RenderTarget* historyInput = pTemporalAAHistoryRT[gCurrentTemporalAARenderTarget];
			RenderTarget* historyOutput = pTemporalAAHistoryRT[!gCurrentTemporalAARenderTarget];

			pRenderTarget = historyOutput;

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

			RenderTargetBarrier depthBarrier = {
				pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &depthBarrier);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBindPipeline(cmd, pTemporalAAPipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetsTemporalAA[gCurrentTemporalAARenderTarget]);
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			RenderTargetBarrier temporalAABarriers[] = {
				{ historyInput, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ historyOutput, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE }
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, temporalAABarriers);

			postProcessDescriptorSet = pDescriptorSetsFinalPostProcess[gCurrentTemporalAARenderTarget + 1];

			gCurrentTemporalAAJitterSample = (gCurrentTemporalAAJitterSample + 1) % TEMPORAL_AA_JITTER_SAMPLES;
			gCurrentTemporalAARenderTarget = !gCurrentTemporalAARenderTarget;
		}

		cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

		pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Final Post Process");

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

			RenderTargetBarrier barriers[] =
			{
				{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

			PostProcessRootConstant rootConstant;
			rootConstant.SceneTextureSize = int2(pRenderTarget->mWidth, pRenderTarget->mHeight);
			rootConstant.VignetteRadius = bEnableVignette ? 0.2F : 0.0F;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBindPipeline(cmd, pFinalPostProcessPipeline);
			cmdBindDescriptorSet(cmd, 0, postProcessDescriptorSet);
			cmdBindPushConstants(cmd, pRootSignatureFinalPostProcess, gPostProcessRootConstantIndex, &rootConstant);
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		if (bScreenShotMode)
		{
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
			
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Water Mark");
			
			cmdBindPipeline(cmd, pWaterMarkPipeline);
			
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetWatermark);
			
			const uint32_t stride = sizeof(float) * 5;
			cmdBindVertexBuffer(cmd, 1, &WaterMarkVB, &stride, NULL);
			cmdDraw(cmd, 6, 0);
			
			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}
		
		if (!bScreenShotMode)
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
			getHiresTimerUSec(&gTimer, true);
			
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

			gFrameTimeDraw.mFontColor = 0xff00ffff;
			gFrameTimeDraw.mFontSize = 18.0f;
			gFrameTimeDraw.mFontID = 0;
			float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
			cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

			cmdDrawUserInterface(cmd);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

		RenderTargetBarrier finalBarriers = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &finalBarriers);
		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
		endCmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);
		
		flipProfiler();

		PostDrawUpdate();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}
	
	const char* GetName() { return "08_GltfViewer"; }
	
	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		
		// This unit test does manual tone mapping
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, false);
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		
		return pSwapChain != NULL;
	}
	
	bool addRenderTargets()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		RT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		RT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		RT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;

		RT.mClearValue.r = 0.0F;
		RT.mClearValue.g = 0.0F;
		RT.mClearValue.b = 0.0F;
		RT.mClearValue.a = 0.0F;
		
		RT.mWidth = mSettings.mWidth;
		RT.mHeight = mSettings.mHeight;
		
		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		
		RT.pName = "HDR Forward Target";
		addRenderTarget(pRenderer, &RT, &pForwardRT);

		RT.pName = "TAA History 0";
		RT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		RT.mClearValue = {};
		addRenderTarget(pRenderer, &RT, &pTemporalAAHistoryRT[0]);

		RT.pName = "TAA History 1";
		RT.mStartState = RESOURCE_STATE_RENDER_TARGET;
		RT.mClearValue = {};
		addRenderTarget(pRenderer, &RT, &pTemporalAAHistoryRT[1]);

		gCurrentTemporalAAJitterSample = 0;
		gCurrentTemporalAARenderTarget = 0;

		return pForwardRT != NULL && pTemporalAAHistoryRT[0] != NULL && pTemporalAAHistoryRT[1] != NULL;
	}
	
	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
		/************************************************************************/
		// Shadow Map Render Target
		/************************************************************************/
		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue.depth = 0.0f;
		shadowRTDesc.mClearValue.stencil = 0;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		shadowRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
		shadowRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		shadowRTDesc.mWidth = SHADOWMAP_RES;
		shadowRTDesc.mHeight = SHADOWMAP_RES;
		shadowRTDesc.mSampleCount = (SampleCount)SHADOWMAP_MSAA_SAMPLES;
		shadowRTDesc.mSampleQuality = 0;    // don't need higher quality sample patterns as the texture will be blurred heavily
		shadowRTDesc.pName = "Shadow Map RT";		
		addRenderTarget(pRenderer, &shadowRTDesc, &pShadowRT);
		
		return pDepthBuffer != NULL && pShadowRT != NULL;
	}
	
	void RecenterCameraView(float maxDistance, const vec3& lookAt = vec3(0))
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;
		
		float lenSqr = lengthSqr(d);
		if (lenSqr > (maxDistance * maxDistance))
		{
			d *= (maxDistance / sqrtf(lenSqr));
		}
		
		p = d + lookAt;
		pCameraController->moveTo(p);
		pCameraController->lookAt(lookAt);
	}
};

DEFINE_APPLICATION_MAIN(GLTFViewer)
