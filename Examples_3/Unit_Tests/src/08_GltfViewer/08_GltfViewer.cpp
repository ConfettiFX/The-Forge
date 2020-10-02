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

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/sort.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"

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

bool				bToggleFXAA					= true;
bool				bVignetting					= true;
bool				bToggleVSync				= false;
bool				bScreenShotMode				= false;

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
	mat4 mProjectView;
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
	mat4	projViewMat;
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
		SyncToken token = {};
		for (uint32_t i = 0; i < pData->pHandle->images_count; ++i)
			gltfLoadTextureAtIndex(pData, i, false, &token, TEXTURE_CONTAINER_BASIS, &mTextures[i]);

		if (pData->mNodeCount)
			createNodeTransformsBuffer();
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

		mat4* nodeTransforms = (mat4*)alloca(sizeof(mat4) * pData->mNodeCount);

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
		uint64_t materialDataStride = pBindlessTexturesSamplersSet ? sizeof(GLTFMaterialData) : round_up_64(sizeof(GLTFMaterialData), 256);

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
		
		size_t i = 0;
		for (uint32_t m = 0; m < pData->mMaterialCount; ++m)
		{
			const GLTFMaterial& material = pData->pMaterials[m];
			uint64_t materialBufferOffset = i * materialDataStride;
			
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
			
			DescriptorData params[11] = {};
			params[0].pName = "cbMaterialData";
			params[0].ppBuffers = &pMaterialBuffer;
			params[0].pOffsets = &materialBufferOffset;
			params[0].pSizes = &materialDataStride;
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
				updateDescriptorSet(pRenderer, (uint32_t)i, pMaterialSet, paramIdx, params);
			}
			
			i += 1;
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

struct FXAAINFO
{
	float2 ScreenSize;
	uint Use;
	uint padding00;
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
RenderTarget*		pPostProcessRT		= NULL;
RenderTarget*		pDepthBuffer		= NULL;
RenderTarget*		pShadowRT			= NULL;

Fence*				pRenderCompleteFences[gImageCount]	= { NULL };

Semaphore*			pImageAcquiredSemaphore				= NULL;
Semaphore*			pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*				pShaderZPass						= NULL;
Shader*				pShaderZPass_NonOptimized			= NULL;
Shader*				pMeshOptDemoShader					= NULL;
Shader*				pFloorShader						= NULL;
Shader*				pVignetteShader						= NULL;
Shader*				pFXAAShader							= NULL;
Shader*				pWaterMarkShader					= NULL;

Pipeline*			pPipelineShadowPass					= NULL;
Pipeline*			pPipelineShadowPass_NonOPtimized	= NULL;
Pipeline*			pMeshOptDemoPipeline				= NULL;
Pipeline*			pFloorPipeline						= NULL;
Pipeline*			pVignettePipeline					= NULL;
Pipeline*			pFXAAPipeline						= NULL;
Pipeline*			pWaterMarkPipeline					= NULL;

RootSignature*		pRootSignatureShadow				= NULL;
RootSignature*		pRootSignatureShaded				= NULL;
RootSignature*		pRootSignaturePostEffects			= NULL;

DescriptorSet*      pDescriptorSetVignette;
DescriptorSet*      pDescriptorSetFXAA;
DescriptorSet*      pDescriptorSetWatermark;
DescriptorSet*      pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_COUNT];
DescriptorSet*      pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_COUNT];

VirtualJoystickUI   gVirtualJoystick                    = {};

Buffer*				pUniformBuffer[gImageCount]			= { NULL };
Buffer*				pShadowUniformBuffer[gImageCount]	= { NULL };
Buffer*				pFloorUniformBuffer[gImageCount]	= { NULL };

Buffer*				TriangularVB						= NULL;
Buffer*				pFloorVB							= NULL;
Buffer*				pFloorIB							= NULL;
Buffer*				WaterMarkVB							= NULL;

Sampler*			pDefaultSampler						= NULL;
Sampler*			pBilinearClampSampler				= NULL;

UniformBlock		gUniformData;
UniformBlock_Floor	gFloorUniformBlock;
UniformBlock_Shadow gShadowUniformData;

//--------------------------------------------------------------------------------------------
// THE FORGE OBJECTS
//--------------------------------------------------------------------------------------------

ICameraController*	pCameraController					= NULL;
ICameraController*	pLightView							= NULL;

GuiComponent*		pGuiWindow;
GuiComponent*		pGuiGraphics;

IWidget*			pSelectLodWidget					= NULL;

UIApp				gAppUI;

#if defined(__ANDROID__) || defined(__linux__) || defined(ORBIS) || defined(PROSPERO)
uint32_t			modelToLoadIndex					= 0;
uint32_t			guiModelToLoadIndex					= 0;
#endif

const char*		gMissingTextureString				= "MissingTexture";

const char*					    gModelFile;
uint32_t						gPreviousLoadedModel = mModelSelected;

const uint			gBackroundColor = { 0xb2b2b2ff };
static uint			gLightColor[gTotalLightCount] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffff66 };
static float		gLightColorIntensity[gTotalLightCount] = { 2.0f, 0.2f, 0.2f, 0.25f };
static float2		gLightDirection = { -122.0f, 222.0f };

GLTFAsset gCurrentAsset;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class GLTFViewer : public IApp
{
public:
	static bool InitShaderResources()
	{
		// shader
		
		ShaderLoadDesc FloorShader = {};
		
		FloorShader.mStages[0] = { "floor.vert", NULL, 0 };

		addShader(pRenderer, &FloorShader, &pShaderZPass_NonOptimized);

#if defined(__ANDROID__)
		FloorShader.mStages[1] = { "floorMOBILE.frag", NULL, 0 };
#else
		FloorShader.mStages[1] = { "floor.frag", NULL, 0 };
#endif
		
		addShader(pRenderer, &FloorShader, &pFloorShader);
		
		ShaderLoadDesc MeshOptDemoShader = {};
		
		MeshOptDemoShader.mStages[0] = { "basic.vert", NULL, 0 };

		addShader(pRenderer, &MeshOptDemoShader, &pShaderZPass);

#if defined(__ANDROID__)
		MeshOptDemoShader.mStages[1] = { "basicMOBILE.frag", NULL, 0 };
#else
		MeshOptDemoShader.mStages[1] = { "basic.frag", NULL, 0 };
#endif
		
		addShader(pRenderer, &MeshOptDemoShader, &pMeshOptDemoShader);
		
		ShaderLoadDesc VignetteShader = {};
		
		VignetteShader.mStages[0] = { "Triangular.vert", NULL, 0 };
		VignetteShader.mStages[1] = { "vignette.frag", NULL, 0 };
		
		addShader(pRenderer, &VignetteShader, &pVignetteShader);
		
		ShaderLoadDesc FXAAShader = {};
		
		FXAAShader.mStages[0] = { "Triangular.vert", NULL, 0 };
		FXAAShader.mStages[1] = { "FXAA.frag", NULL, 0 };
		
		addShader(pRenderer, &FXAAShader, &pFXAAShader);
		
		ShaderLoadDesc WaterMarkShader = {};
		
		WaterMarkShader.mStages[0] = { "watermark.vert", NULL, 0 };
		WaterMarkShader.mStages[1] = { "watermark.frag", NULL, 0 };
		
		addShader(pRenderer, &WaterMarkShader, &pWaterMarkShader);
		
		const char* pStaticSamplerNames[] = { "clampMiplessLinearSampler" };
		Sampler* pStaticSamplers[] = { pBilinearClampSampler };
		Shader*           shaders[] = { pShaderZPass, pShaderZPass_NonOptimized };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		rootDesc.ppStaticSamplers = pStaticSamplers;
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureShadow);
		
		Shader*  demoShaders[] = { pMeshOptDemoShader, pFloorShader };
		
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = demoShaders;
		
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureShaded);

		Shader* postShaders[] = { pVignetteShader, pFXAAShader, pWaterMarkShader };
		rootDesc.mShaderCount = 3;
		rootDesc.ppShaders = postShaders;

		addRootSignature(pRenderer, &rootDesc, &pRootSignaturePostEffects);
		
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
			cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
			// sets the rectangles to match with first attachment, I know that it's not very portable.
			RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0];
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
				cmdBindPushConstants(cmd, pRootSignatureShadow, "cbRootConstants", &pushConstants);
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
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,          "Meshes");
		
		// window and renderer setup
		RendererDesc settings = { 0 };
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
		
		if (!gAppUI.Init(pRenderer))
			return false;
		
		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");
		
		initProfiler();

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}
	
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
		/************************************************************************/
		// GUI
		/************************************************************************/
		GuiDesc guiDesc = {};		
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
		
#if !defined(TARGET_IOS)
		pGuiWindow->AddWidget(CheckboxWidget("Toggle VSync", &bToggleVSync));
#endif
		
		pGuiWindow->AddWidget(SeparatorWidget());

		DropdownWidget loadModelWidget("Load Model", &mModelSelected, pModelNames, gModelValues, mModelCount);

		pGuiWindow->AddWidget(loadModelWidget);
		
		pGuiWindow->AddWidget(SeparatorWidget());
		
		pSelectLodWidget = pGuiWindow->AddWidget(SliderIntWidget("LOD", &gCurrentLod, 0, gMaxLod));
		
		////////////////////////////////////////////////////////////////////////////////////////////
		
		guiDesc = {};		
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.35f);
		pGuiGraphics = gAppUI.AddGuiComponent("Graphics Options", &guiDesc);
		
		pGuiGraphics->AddWidget(CheckboxWidget("Enable FXAA", &bToggleFXAA));
		pGuiGraphics->AddWidget(CheckboxWidget("Enable Vignetting", &bVignetting));
		
		pGuiGraphics->AddWidget(SeparatorWidget());
		
		CollapsingHeaderWidget LightWidgets("Light Options", false, false);
		LightWidgets.AddSubWidget(SliderFloatWidget("Light Azimuth", &gLightDirection.x, float(-180.0f), float(180.0f), float(0.001f)));
		LightWidgets.AddSubWidget(SliderFloatWidget("Light Elevation", &gLightDirection.y, float(210.0f), float(330.0f), float(0.001f)));
		
		LightWidgets.AddSubWidget(SeparatorWidget());
		
		CollapsingHeaderWidget LightColor1Picker("Main Light Color");
		LightColor1Picker.AddSubWidget(ColorPickerWidget("Main Light Color", &gLightColor[0]));
		LightWidgets.AddSubWidget(LightColor1Picker);
		
		CollapsingHeaderWidget LightColor1Intensity("Main Light Intensity");
		LightColor1Intensity.AddSubWidget(SliderFloatWidget("Main Light Intensity", &gLightColorIntensity[0], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor1Intensity);
		
		LightWidgets.AddSubWidget(SeparatorWidget());
		
		CollapsingHeaderWidget LightColor2Picker("Light2 Color");
		LightColor2Picker.AddSubWidget(ColorPickerWidget("Light2 Color", &gLightColor[1]));
		LightWidgets.AddSubWidget(LightColor2Picker);
		
		CollapsingHeaderWidget LightColor2Intensity("Light2 Intensity");
		LightColor2Intensity.AddSubWidget(SliderFloatWidget("Light2 Intensity", &gLightColorIntensity[1], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor2Intensity);
		
		LightWidgets.AddSubWidget(SeparatorWidget());
		
		CollapsingHeaderWidget LightColor3Picker("Light3 Color");
		LightColor3Picker.AddSubWidget(ColorPickerWidget("Light3 Color", &gLightColor[2]));
		LightWidgets.AddSubWidget(LightColor3Picker);
		
		CollapsingHeaderWidget LightColor3Intensity("Light3 Intensity");
		LightColor3Intensity.AddSubWidget(SliderFloatWidget("Light3 Intensity", &gLightColorIntensity[2], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor3Intensity);
		
		LightWidgets.AddSubWidget(SeparatorWidget());
		
		CollapsingHeaderWidget AmbientLightColorPicker("Ambient Light Color");
		AmbientLightColorPicker.AddSubWidget(ColorPickerWidget("Ambient Light Color", &gLightColor[3]));
		LightWidgets.AddSubWidget(AmbientLightColorPicker);
		
		CollapsingHeaderWidget LightColor4Intensity("Ambient Light Intensity");
		LightColor4Intensity.AddSubWidget(SliderFloatWidget("Light Intensity", &gLightColorIntensity[3], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor4Intensity);
		
		LightWidgets.AddSubWidget(SeparatorWidget());
		
		pGuiGraphics->AddWidget(LightWidgets);
		
		CameraMotionParameters cmp{ 1.0f, 120.0f, 40.0f };
		vec3                   camPos{ 3.0f, 2.5f, -4.0f };
		vec3                   lookAt{ 0.0f, 0.4f, 0.0f };
		
		pLightView = createGuiCameraController(camPos, lookAt);
		pCameraController = createFpsCameraController(normalize(camPos) * 3.0f, lookAt);
		pCameraController->setMotionParameters(cmp);
		
		if (!initInputSystem(pWindow))
			return false;
		
		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		actionDesc =
		{
			InputBindings::BUTTON_ANY, [](InputActionContext* ctx)
			{
				bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.25f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 0.25f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		waitForAllResourceLoads();
		
		return true;
	}
	
	static bool LoadModel(GLTFAsset& asset, const char* modelFileName)
	{
		//eastl::vector<PathHandle> validFileLists;
				
		eastl::string fileNameOnly = modelFileName;

		gCurrentAsset.removeResources();
		
		GeometryLoadDesc loadDesc = {};
		loadDesc.ppGeometry = &gCurrentAsset.pGeom;
		loadDesc.pFileName = modelFileName;
		loadDesc.pVertexLayout = &gVertexLayoutModel;
		addResource(&loadDesc, NULL);

		uint32_t res = gltfLoadContainer(modelFileName, GLTF_FLAG_CALCULATE_BOUNDS, &gCurrentAsset.pData);
		if (res)
		{
			return false;
		}
		
		asset.Init(pRenderer, pDefaultSampler);
		
		pGuiWindow->RemoveWidget(pSelectLodWidget);
		//gMaxLod = max((int)validFileLists.size() - 1, 0);
		//pSelectLodWidget = pGuiWindow->AddWidget(SliderIntWidget("LOD", &gCurrentLod, 0, gMaxLod));
		waitForAllResourceLoads();
		return true;
	}
	
	static bool AddDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignaturePostEffects, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVignette);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFXAA);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetWatermark);

		setDesc = { pRootSignatureShadow, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE]);
		setDesc = { pRootSignatureShadow, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
		
		setDesc = { pRootSignatureShaded, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE]);
		setDesc = { pRootSignatureShaded, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
		
		return true;
	}
	
	static void RemoveDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetVignette);
		removeDescriptorSet(pRenderer, pDescriptorSetFXAA);
		removeDescriptorSet(pRenderer, pDescriptorSetWatermark);
		removeDescriptorSet(pRenderer, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE]);
		removeDescriptorSet(pRenderer, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_PER_FRAME]);
	}
	
	static void PrepareDescriptorSets()
	{
		// Shadow
		{
			DescriptorData params[2] = {};
			if (gCurrentAsset.pNodeTransformsBuffer)
			{
				params[0].pName = "modelToWorldMatrices";
				params[0].ppBuffers = &gCurrentAsset.pNodeTransformsBuffer;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetsShadow[DESCRIPTOR_UPDATE_FREQ_NONE], 1, params);
			}
			
			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pForwardRT->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVignette, 1, params);
			
			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pPostProcessRT->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetFXAA, 1, params);
			
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
			DescriptorData params[3] = {};
			params[0].pName = "ShadowTexture";
			params[0].ppTextures = &pShadowRT->pTexture;
			if (gCurrentAsset.pNodeTransformsBuffer)
			{
				params[1].pName = "modelToWorldMatrices";
				params[1].ppBuffers = &gCurrentAsset.pNodeTransformsBuffer;
			}
			updateDescriptorSet(pRenderer, 0, pDescriptorSetsShaded[DESCRIPTOR_UPDATE_FREQ_NONE], gCurrentAsset.pNodeTransformsBuffer ? 2 : 1, params);
			
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
		removeShader(pRenderer, pVignetteShader);
		removeShader(pRenderer, pFloorShader);
		removeShader(pRenderer, pMeshOptDemoShader);
		removeShader(pRenderer, pFXAAShader);
		removeShader(pRenderer, pWaterMarkShader);
		
		removeRootSignature(pRenderer, pRootSignatureShadow);
		removeRootSignature(pRenderer, pRootSignatureShaded);
		removeRootSignature(pRenderer, pRootSignaturePostEffects);
	}
	
	static void RemoveModelDependentResources()
	{
		gCurrentAsset.removeResources();
		RemoveShaderResources();
		gCurrentAsset = GLTFAsset();
	}
	
	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);
		
		exitInputSystem();
		
		exitProfiler();
		
		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);
		
		gVirtualJoystick.Exit();
		
		gAppUI.Exit();
		
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pShadowUniformBuffer[i]);
			removeResource(pUniformBuffer[i]);
			removeResource(pFloorUniformBuffer[i]);
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
		
		removeResource(TriangularVB);
		
		removeResource(pFloorVB);
		removeResource(pFloorIB);
		
		removeResource(pTextureBlack);

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		removeRenderer(pRenderer);
		
		gModelFile = NULL;
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
			pipelineSettings.pColorFormats = &pPostProcessRT->mFormat;
			pipelineSettings.mSampleCount = pPostProcessRT->mSampleCount;
			pipelineSettings.mSampleQuality = pPostProcessRT->mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignaturePostEffects;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pVignetteShader;
			addPipeline(pRenderer, &desc, &pVignettePipeline);
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
			pipelineSettings.pRootSignature = pRootSignaturePostEffects;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pShaderProgram = pFXAAShader;
			addPipeline(pRenderer, &desc, &pFXAAPipeline);
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

			if (!gAppUI.Load(pSwapChain->ppRenderTargets))
				return false;

			if (!gVirtualJoystick.Load(pSwapChain->ppRenderTargets[0]))
				return false;

			loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

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
		}

		InitModelDependentResources();
		
		LoadPipelines();
		
		return true;
	}
	
	static void RemovePipelines()
	{
		removePipeline(pRenderer, pPipelineShadowPass_NonOPtimized);
		removePipeline(pRenderer, pPipelineShadowPass);
		removePipeline(pRenderer, pVignettePipeline);
		removePipeline(pRenderer, pFloorPipeline);
		removePipeline(pRenderer, pMeshOptDemoPipeline);
		removePipeline(pRenderer, pFXAAPipeline);
		removePipeline(pRenderer, pWaterMarkPipeline);
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

		removeResource(WaterMarkVB);

		unloadProfilerUI();
		gAppUI.Unload();
		
		gVirtualJoystick.Unload();
		
		removeSwapChain(pRenderer, pSwapChain);
		
		removeRenderTarget(pRenderer, pPostProcessRT);
		removeRenderTarget(pRenderer, pForwardRT);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pShadowRT);
	}
	
	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		
#if !defined(__ANDROID__) && !defined(TARGET_IOS)
		if (pSwapChain->mEnableVsync != bToggleVSync)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif
		
		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 3.0f;
		mat4 projMat = mat4::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.001f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat;
		gUniformData.mCameraPosition = vec4(pCameraController->getViewPosition(), 1.0f);
		
		for (uint i = 0; i < gTotalLightCount; ++i)
		{
			gUniformData.mLightColor[i] = vec4(float((gLightColor[i] >> 24) & 0xff),
											   float((gLightColor[i] >> 16) & 0xff),
											   float((gLightColor[i] >> 8) & 0xff),
											   float((gLightColor[i] >> 0) & 0xff)) / 255.0f;
			
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
		gFloorUniformBlock.screenSize = vec4((float)mSettings.mWidth, (float)mSettings.mHeight, 1.0f / mSettings.mWidth, bVignetting ? 1.0f : 0.0f);
		
		
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
		
		/************************************************************************/
		/************************************************************************/
		
		gAppUI.Update(deltaTime);
	}
	
	void PostDrawUpdate()
	{
		if (gPreviousLoadedModel != mModelSelected)
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
		
		vec4 bgColor = vec4(float((gBackroundColor >> 24) & 0xff),
							float((gBackroundColor >> 16) & 0xff),
							float((gBackroundColor >> 8) & 0xff),
							float((gBackroundColor >> 0) & 0xff)) / 255.0f;
		
		
		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		drawShadowMap(cmd);
		
		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0].r = bgColor.getX();
			loadActions.mClearColorValues[0].g = bgColor.getY();
			loadActions.mClearColorValues[0].b = bgColor.getZ();
			loadActions.mClearColorValues[0].a = bgColor.getW();
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
					cmdBindPushConstants(cmd, pRootSignatureShaded, "cbRootConstants", &pushConstants);
					for (uint32_t i = 0; i < node.mMeshCount; ++i)
					{
						GLTFMesh& mesh = gCurrentAsset.pData->pMeshes[node.mMeshIndex + i];

						cmdBindDescriptorSet(cmd, (uint32_t)gCurrentAsset.pData->pMaterialIndices[node.mMeshIndex + i], gCurrentAsset.pMaterialSet);
						cmdDrawIndexed(cmd, mesh.mIndexCount, mesh.mStartIndex, 0);
					}
				}

				pushConstants.nodeIndex += 1;
			}
			
			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		pRenderTarget = pPostProcessRT;
		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			{ pForwardRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
		};
		
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);
		
		if (bVignetting)
		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Vignetting");

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
			
			cmdBindPipeline(cmd, pVignettePipeline);
			
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetVignette);
			
			cmdDraw(cmd, 3, 0);
			
			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}
        
		pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		{
			RenderTargetBarrier barriers[] =
			{
				{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
				{ pPostProcessRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);
			
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0].r = 0.0f;
			loadActions.mClearColorValues[0].g = 0.0f;
			loadActions.mClearColorValues[0].b = 0.0f;
			loadActions.mClearColorValues[0].a = 0.0f;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "FXAA");

			cmdBindPipeline(cmd, pFXAAPipeline);
			
			FXAAINFO FXAAinfo;
			FXAAinfo.ScreenSize = float2((float)mSettings.mWidth, (float)mSettings.mHeight);
			FXAAinfo.Use = bToggleFXAA ? 1 : 0;
			
			cmdBindDescriptorSet(cmd, 0, bVignetting ? pDescriptorSetFXAA : pDescriptorSetVignette);
			cmdBindPushConstants(cmd, pRootSignaturePostEffects, "FXAARootConstant", &FXAAinfo);
			
			cmdDraw(cmd, 3, 0);
			
			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);
			
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}
		
		
		if (bScreenShotMode)
		{
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
			
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Water Mark");
			
			cmdBindPipeline(cmd, pWaterMarkPipeline);
			
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetVignette);
			
			const uint32_t stride = sizeof(float) * 5;
			cmdBindVertexBuffer(cmd, 1, &WaterMarkVB, &stride, NULL);
			cmdDraw(cmd, 6, 0);
			
			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}
		
		if (!bScreenShotMode)
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
			static HiresTimer gTimer;
			gTimer.GetUSec(true);
			
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			//gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

#if !defined(__ANDROID__)
				float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
				cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 30.f), gGpuProfileToken, &gFrameTimeDraw);
#else
				cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
#endif

				cmdDrawProfilerUI();

			gAppUI.Gui(pGuiWindow);
			gAppUI.Gui(pGuiGraphics);

			gAppUI.Draw(cmd);

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
		
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		
		return pSwapChain != NULL;
	}
	
	bool addRenderTargets()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		RT.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		RT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		
		vec4 bgColor = vec4(float((gBackroundColor >> 24) & 0xff),
							float((gBackroundColor >> 16) & 0xff),
							float((gBackroundColor >> 8) & 0xff),
							float((gBackroundColor >> 0) & 0xff)) / 255.0f;
		
		RT.mClearValue.r = bgColor.getX();
		RT.mClearValue.g = bgColor.getY();
		RT.mClearValue.b = bgColor.getZ();
		RT.mClearValue.a = bgColor.getW();
		
		RT.mWidth = mSettings.mWidth;
		RT.mHeight = mSettings.mHeight;
		
		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.pName = "Render Target";
		addRenderTarget(pRenderer, &RT, &pForwardRT);
		
		RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		RT.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		RT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		RT.mWidth = mSettings.mWidth;
		RT.mHeight = mSettings.mHeight;
		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.pName = "Post Process Render Target";
		addRenderTarget(pRenderer, &RT, &pPostProcessRT);
		
		return pForwardRT != NULL && pPostProcessRT != NULL;
	}
	
	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
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
	
	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
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
