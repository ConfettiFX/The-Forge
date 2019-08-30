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


#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "../../../../Common_3/Tools/AssetPipeline/src/AssetLoader.h"

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be last include in cpp file

const uint gLightCount = 3;
const uint gTotalLightCount = gLightCount + 1;

#define SHADOWMAP_MSAA_SAMPLES 1

#if defined(TARGET_IOS) || defined(__ANDROID__)
#define SHADOWMAP_RES 1024u
#else
#define SHADOWMAP_RES 2048u
#endif

#if !defined(TARGET_IOS) && !defined(__ANDROID__)
#define USE_BASIS 1
#endif

struct UniformBlock
{
	mat4 mProjectView;
	vec4 mCameraPosition;
	vec4 mLightColor[gTotalLightCount];
	vec4 mLightDirection[gLightCount];
	int4 mQuantizationParams;
};

struct ShadowUniformBlock
{
	mat4 ViewProj;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4  mWorldMat;
	mat4  InvTranspose;
	int   unlit;
	int   hasAlbedoMap;
	int   hasNormalMap;
	int   hasMetallicRoughnessMap;
	int   hasAOMap;
	int   hasEmissiveMap;
	vec4  centerOffset;
	vec4  posOffset;
	vec2  uvOffset;
	vec2  uvScale;
	float posScale;
	float padding0;
};

struct FloorUniformBlock
{
	mat4	worldMat;
	mat4	projViewMat;
	vec4	screenSize;
};

//Structure of Array for vertex/index data
struct MeshBatch
{
	Buffer* pPositionStream;
	Buffer* pNormalStream;
	Buffer* pUVStream;
	Buffer* pIndicesStream;
	Buffer* pBaseColorStream;
	Buffer* pMetallicRoughnessStream;
	Buffer* pAlphaStream;
	Buffer* pConstantBuffer;
	int     NoofVertices;
	int     NoofIndices;
	int     NoofInstances;
	int     MaterialIndex;
	int     SamplerIndex[5];
	int     NoofUVSets;
};

struct PropData
{
	mat4                      WorldMatrix;
	eastl::vector<MeshBatch*> MeshBatches;
};

struct FXAAINFO
{
	vec2 ScreenSize;
	uint Use;
	uint padding00;
};

const uint32_t  gImageCount = 3;
bool			bToggleMicroProfiler = false;
bool			bPrevToggleMicroProfiler = false;
bool			bToggleFXAA = true;
bool			bVignetting = true;
bool            bToggleVSync = false;
bool			bScreenShotMode = false;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain* pSwapChain = NULL;
RenderTarget* pForwardRT = NULL;
RenderTarget* pPostProcessRT = NULL;
RenderTarget* pDepthBuffer = NULL;
RenderTarget* pRenderTargetShadowMap = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pShaderZPass = NULL;
Pipeline* pPipelineShadowPass = NULL;

Shader*	  pShaderZPass_NonOPtimized = NULL;
Pipeline* pPipelineShadowPass_NonOPtimized = NULL;

Shader*   pMeshOptDemoShader = NULL;
Pipeline* pMeshOptDemoPipeline = NULL;

Shader*   pFloorShader = NULL;
Pipeline* pFloorPipeline = NULL;

Shader*   pVignetteShader = NULL;
Pipeline* pVignettePipeline = NULL;

Shader*   pFXAAShader = NULL;
Pipeline* pFXAAPipeline = NULL;

Shader*   pWaterMarkShader = NULL;
Pipeline* pWaterMarkPipeline = NULL;

RootSignature*    pRootSignature = NULL;
DescriptorBinder* pDescriptorBinder = NULL;
#if defined(TARGET_IOS) || defined(__ANDROID__)
VirtualJoystickUI gVirtualJoystick;
#endif
RasterizerState* pRasterizerStateCullBack = NULL;
DepthState* pDepthStateForRendering = NULL;

BlendState* pBlendStateAlphaBlend = NULL;

Buffer*	 pUniformBuffer[gImageCount] = { NULL };
Buffer*	 pShadowUniformBuffer[gImageCount] = { NULL };
Buffer*	 pFloorShadowUniformBuffer = NULL;
Buffer*	 TriangularVB = NULL;
Sampler* pBilinearClampSampler = NULL;
		 
Buffer*	 pFloorUniformBuffer[gImageCount] = { NULL };
Buffer*	 pFloorVB = NULL;
Buffer*	 pFloorIB = NULL;
		 
Buffer*	 WaterMarkVB = NULL;

uint32_t gFrameIndex = 0;

UniformBlock gUniformData;
ShadowUniformBlock gShadowUniformData;
FloorUniformBlock gFloorUniformBlock;

ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;

GuiComponent* pGuiWindow;
GuiComponent* pGuiGraphics;
GpuProfiler* pGpuProfiler = NULL;

/// UI
UIApp gAppUI;

FileSystem gFileSystem;

const wchar_t* gMissingTextureString = L"MissingTexture";
Texture* pTextureBlack = nullptr;

// Model UI
eastl::vector<uint32_t>			gDropDownWidgetData;
eastl::vector<eastl::string>	gModelFiles;
uint32_t						modelToLoadIndex = 0;
uint32_t						guiModelToLoadIndex = 0;
bool							gLoadOptimizedModel = false;
bool                            gAddLod = false;
char							gModel_File[128] = { "Lantern.gltf" };
char							gGuiModelToLoad[128] = { "Lantern.gltf" };
uint							gBackroundColor = { 0xb2b2b2ff };
uint							gLightColor[gTotalLightCount] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffff66 };
float							gLightColorIntensity[gTotalLightCount] = { 2.0f, 0.2f, 0.2f, 0.25f };
float2							gLightDirection = { -122.0f, 222.0f };

// Model Quantization Settings
int gPosBits = 16;
int gTexBits = 16;
int gNrmBits = 8;

//Model
struct LOD
{
	Model model;
	PropData modelProp;
	eastl::vector<Texture*> pMaterialTextures;
	eastl::vector<Sampler*> pSamplers;
	eastl::vector<uint32_t> textureIndexforMaterial;
	eastl::unordered_map<uint32_t, uint32_t> materialIDMap;
	eastl::unordered_map<uint64_t, uint32_t> samplerIDMap;
};

int gCurrentLod = 0;
eastl::vector<LOD> gLODs(1);

const char* pszBases[FSR_Count] = {
	"../../../src/08_GltfViewer/",    // FSR_BinShaders
	"../../../src/08_GltfViewer/",    // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/08_GltfViewer/",    // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_Audio
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class MeshOptimization : public IApp
{
public:
	MeshOptimization()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	static bool InitShaderResources()
	{
		// shader

		ShaderLoadDesc zPassShaderDesc = {};

		zPassShaderDesc.mStages[0] = { "zPass.vert", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &zPassShaderDesc, &pShaderZPass);

		zPassShaderDesc = {};

		zPassShaderDesc.mStages[0] = { "zPassFloor.vert", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &zPassShaderDesc, &pShaderZPass_NonOPtimized);

		ShaderLoadDesc FloorShader = {};

		FloorShader.mStages[0] = { "floor.vert", NULL, 0, FSR_SrcShaders };
#if defined(__ANDROID__)
		FloorShader.mStages[1] = { "floorMOBILE.frag", NULL, 0, FSR_SrcShaders };
#else
		FloorShader.mStages[1] = { "floor.frag", NULL, 0, FSR_SrcShaders };
#endif

		addShader(pRenderer, &FloorShader, &pFloorShader);

		ShaderLoadDesc MeshOptDemoShader = {};

		MeshOptDemoShader.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
#if defined(__ANDROID__)
		MeshOptDemoShader.mStages[1] = { "basicMOBILE.frag", NULL, 0, FSR_SrcShaders };
#else
		MeshOptDemoShader.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };
#endif

		addShader(pRenderer, &MeshOptDemoShader, &pMeshOptDemoShader);

		ShaderLoadDesc VignetteShader = {};

		VignetteShader.mStages[0] = { "Triangular.vert", NULL, 0, FSR_SrcShaders };
		VignetteShader.mStages[1] = { "vignette.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &VignetteShader, &pVignetteShader);

		ShaderLoadDesc FXAAShader = {};

		FXAAShader.mStages[0] = { "Triangular.vert", NULL, 0, FSR_SrcShaders };
		FXAAShader.mStages[1] = { "FXAA.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &FXAAShader, &pFXAAShader);

		ShaderLoadDesc WaterMarkShader = {};

		WaterMarkShader.mStages[0] = { "watermark.vert", NULL, 0, FSR_SrcShaders };
		WaterMarkShader.mStages[1] = { "watermark.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &WaterMarkShader, &pWaterMarkShader);

		Shader*           shaders[] = { pShaderZPass, pShaderZPass_NonOPtimized, pVignetteShader, pFloorShader, pMeshOptDemoShader, pFXAAShader, pWaterMarkShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 0;
		rootDesc.ppStaticSamplerNames = NULL;
		rootDesc.ppStaticSamplers = NULL;
		rootDesc.mShaderCount = 7;
		rootDesc.ppShaders = shaders;

		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		eastl::vector<DescriptorBinderDesc> descriptorBinderDesc;
		size_t gModelMeshCount = gLODs[0].model.mMeshArray.size();
		for (size_t i = 1; i < gLODs.size(); ++i)
		{
			size_t lodMeshSize = gLODs[i].model.mMeshArray.size();
			if (lodMeshSize > gModelMeshCount)
				gModelMeshCount = lodMeshSize;
		}

		for (size_t i = 0; i < gModelMeshCount * 2 + 5; ++i)
		{
			DescriptorBinderDesc tempBinder = { pRootSignature };
			descriptorBinderDesc.push_back(tempBinder);
		}
		addDescriptorBinder(pRenderer, 0, (uint32_t)descriptorBinderDesc.size(), descriptorBinderDesc.data(), &pDescriptorBinder);

		return true;
	}

	static bool AddLOD()
	{
		gLODs.push_back();

		RemoveShaderResources();
        RemovePipelines();

		if (!LoadModel(gLODs.back()))
			return false;

		if (!InitShaderResources())
			return false;
        
        LoadPipelines();

		return true;
	}

	static bool InitModelDependentResources()
	{
		if (!LoadModel(gLODs[0]))
			return false;

		if (!InitShaderResources())
			return false;

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
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSizeTarget->mDesc.mWidth, (float)pSizeTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSizeTarget->mDesc.mWidth, pSizeTarget->mDesc.mHeight);
		}
	}

	static void drawShadowMap(Cmd* cmd)
	{
		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pShadowUniformBuffer[gFrameIndex], &gShadowUniformData };
		updateResource(&shaderCbv);

		TextureBarrier barriers[] =
		{
			{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};

		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadowMap->mDesc.mClearValue;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Shadow Map", true);
		// Start render pass and apply load actions
		setRenderTarget(cmd, 0, NULL, pRenderTargetShadowMap, &loadActions);

		cmdBindPipeline(cmd, pPipelineShadowPass_NonOPtimized);

		DescriptorData params[2] = {};
		params[0].pName = "ShadowUniformBuffer";
		params[0].ppBuffers = &pShadowUniformBuffer[gFrameIndex];

		params[1].pName = "cbPerProp";
		params[1].ppBuffers = &pFloorShadowUniformBuffer;

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 2, params);

		cmdBindVertexBuffer(cmd, 1, &pFloorVB, NULL);
		cmdBindIndexBuffer(cmd, pFloorIB, 0);

		cmdDrawIndexed(cmd, 6, 0, 0);

		cmdBindPipeline(cmd, pPipelineShadowPass);

		for (MeshBatch* mesh : gLODs[gCurrentLod].modelProp.MeshBatches)
		{
			params[1].pName = "cbPerProp";
			params[1].ppBuffers = &mesh->pConstantBuffer;

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 2, params);

			Buffer* pVertexBuffers[] = { mesh->pPositionStream, mesh->pNormalStream, mesh->pUVStream,
				mesh->pBaseColorStream, mesh->pMetallicRoughnessStream, mesh->pAlphaStream };
			cmdBindVertexBuffer(cmd, 6, pVertexBuffers, NULL);

			cmdBindIndexBuffer(cmd, mesh->pIndicesStream, 0);

			cmdDrawIndexed(cmd, mesh->NoofIndices, 0, 0);
		}

		setRenderTarget(cmd, 0, NULL, NULL, NULL);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	bool Init()
	{
#if defined(TARGET_IOS)
		add_uti_to_map("gltf", "dyn.ah62d4rv4ge80s5dyq2");
#endif

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		initProfiler(pRenderer);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

#if defined(__ANDROID__) || defined(TARGET_IOS)
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", FSR_Textures))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}
#endif

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
		floorVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		floorVbDesc.mDesc.mSize = floorVbDesc.mDesc.mVertexStride * 4;
		floorVbDesc.pData = floorPoints;
		floorVbDesc.ppBuffer = &pFloorVB;
		addResource(&floorVbDesc);

		uint32_t floorIndices[] = {
			0, 1, 3,
			3, 1, 2
		};

		BufferLoadDesc indexBufferDesc = {};
		indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		indexBufferDesc.mDesc.mSize = sizeof(uint32_t) * 6;
		indexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		indexBufferDesc.pData = floorIndices;
		indexBufferDesc.ppBuffer = &pFloorIB;
		addResource(&indexBufferDesc);

		float screenTriangularPoints[] = {
				-1.0f,  3.0f, 0.5f, 0.0f, -1.0f,
				-1.0f, -1.0f, 0.5f, 0.0f, 1.0f,
				3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		screenQuadVbDesc.mDesc.mSize = screenQuadVbDesc.mDesc.mVertexStride * 3;
		screenQuadVbDesc.pData = screenTriangularPoints;
		screenQuadVbDesc.ppBuffer = &TriangularVB;
		addResource(&screenQuadVbDesc);

		TextureDesc defaultTextureDesc = {};
		defaultTextureDesc.mArraySize = 1;
		defaultTextureDesc.mDepth = 1;
		defaultTextureDesc.mFormat = ImageFormat::RGBA8;
		defaultTextureDesc.mWidth = 4;
		defaultTextureDesc.mHeight = 4;
		defaultTextureDesc.mMipLevels = 1;
		defaultTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		defaultTextureDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		defaultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		defaultTextureDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		defaultTextureDesc.pDebugName = gMissingTextureString;
		TextureLoadDesc defaultLoadDesc = {};
		defaultLoadDesc.pDesc = &defaultTextureDesc;
		RawImageData idata;
		unsigned char blackData[64];
		memset(blackData, 0, sizeof(unsigned char) * 64);

		idata.mArraySize = 1;
		idata.mDepth = defaultTextureDesc.mDepth;
		idata.mWidth = defaultTextureDesc.mWidth;
		idata.mHeight = defaultTextureDesc.mHeight;
		idata.mFormat = defaultTextureDesc.mFormat;
		idata.mMipLevels = defaultTextureDesc.mMipLevels;
		idata.pRawData = blackData;

		defaultLoadDesc.pRawImageData = &idata;

		defaultLoadDesc.ppTexture = &pTextureBlack;
		addResource(&defaultLoadDesc);

#if defined(__ANDROID__) || defined(__LINUX__)
		// Get list of Models
		eastl::vector<eastl::string> filesInDirectory;
		eastl::string meshDirectory = FileSystem::FixPath("", FSRoot::FSR_Meshes);
		FileSystem::GetFilesWithExtension(meshDirectory, ".gltf", filesInDirectory);
		size_t modelFileCount = filesInDirectory.size();
		eastl::vector<const char*> modelFileNames(modelFileCount);
		gModelFiles.resize(modelFileCount);
		gDropDownWidgetData.resize(modelFileCount);
		for (size_t i = 0; i < modelFileCount; ++i)
		{
			const eastl::string & file = filesInDirectory[i];
			eastl::string fileName = FileSystem::GetFileName(file).append(".gltf");
			gModelFiles[i] = fileName;
			modelFileNames[i] = gModelFiles[i].c_str();
			gDropDownWidgetData[i] = (uint32_t)i;
		}

		strcpy(gModel_File, gModelFiles[0].c_str());
		strcpy(gGuiModelToLoad, gModelFiles[0].c_str());

#else
		eastl::string fullModelPath = FileSystem::FixPath(gModel_File, FSRoot::FSR_Meshes);
		strcpy(gModel_File, fullModelPath.c_str());
        strcpy(gGuiModelToLoad, gModel_File);
#endif

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pUniformBuffer[i];
			addResource(&ubDesc);
		}
        
        BufferLoadDesc floorShadowDesc = {};
        floorShadowDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        floorShadowDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        floorShadowDesc.mDesc.mSize = sizeof(UniformObjData);
        floorShadowDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        floorShadowDesc.pData = NULL;
        floorShadowDesc.ppBuffer = &pFloorShadowUniformBuffer;
        addResource(&floorShadowDesc);

		BufferLoadDesc subDesc = {};
		subDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		subDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		subDesc.mDesc.mSize = sizeof(ShadowUniformBlock);
		subDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		subDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			subDesc.ppBuffer = &pShadowUniformBuffer[i];
			addResource(&subDesc);
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
			addResource(&ubDesc);
		}

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullBack);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepthStateForRendering);

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
		addBlendState(pRenderer, &blendStateAlphaDesc, &pBlendStateAlphaBlend);

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		/************************************************************************/
		// GUI
		/************************************************************************/
		GuiDesc guiDesc = {};
		guiDesc.mStartSize = vec2(300.0f, 250.0f);
		guiDesc.mStartPosition = vec2(100.0f, guiDesc.mStartSize.getY());
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);

		pGuiWindow->AddWidget(CheckboxWidget("Enable Micro Profiler", &bToggleMicroProfiler));

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGuiWindow->AddWidget(CheckboxWidget("Toggle VSync", &bToggleVSync));
#endif

#if defined(__ANDROID__) || defined(__LINUX__)
		pGuiWindow->AddWidget(DropdownWidget("Models", &guiModelToLoadIndex, modelFileNames.data(), gDropDownWidgetData.data(), (uint32_t)gModelFiles.size()));
#else		
		pGuiWindow->AddWidget(SeparatorWidget());

		ButtonWidget loadModelButtonWidget("Load Model                                      ");
		loadModelButtonWidget.pOnEdited = MeshOptimization::LoadNewModel;
		pGuiWindow->AddWidget(loadModelButtonWidget);

		pGuiWindow->AddWidget(SeparatorWidget());

		//ButtonWidget loadLODButtonWidget("Load Model LOD");
		//loadLODButtonWidget.pOnEdited = MeshOptimization::LoadLOD;
		//pGuiWindow->AddWidget(loadLODButtonWidget);
#endif

		//pGuiWindow->AddWidget(SliderIntWidget("Select LOD", &gCurrentLod, 0, 5));
		//pGuiWindow->AddWidget(SeparatorWidget());

		////////////////////////////////////////////////////////////////////////////////////////////

		guiDesc = {};
		guiDesc.mStartSize = vec2(400.0f, 250.0f);
		guiDesc.mStartPosition = vec2(mSettings.mWidth - guiDesc.mStartSize.getX(), guiDesc.mStartSize.getY());
		pGuiGraphics = gAppUI.AddGuiComponent("Graphics Options", &guiDesc);

		pGuiGraphics->AddWidget(CheckboxWidget("Enable FXAA", &bToggleFXAA));
		pGuiGraphics->AddWidget(CheckboxWidget("Enable Vignetting", &bVignetting));

		pGuiGraphics->AddWidget(SeparatorWidget());

		//CollapsingHeaderWidget BGColorPicker("Backround Color");
		//BGColorPicker.AddSubWidget(ColorPickerWidget("Backround Color", &gBackroundColor));				
		//pGuiGraphics->AddWidget(BGColorPicker);

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

#if defined(TARGET_IOS) || defined(__ANDROID__)
        if (!gVirtualJoystick.Init(pRenderer, "circlepad", FSR_Textures))
        {
            LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
            return false;
        }
#endif		

		pCameraController->setMotionParameters(cmp);

		if (!initInputSystem(pWindow))
			return false;

		// Microprofiler Actions
		// #TODO: Remove this once the profiler UI is ported to use our UI system
		InputActionDesc actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { onProfilerButton(false, &ctx->mFloat2, true); return !bToggleMicroProfiler; } };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_SOUTH, [](InputActionContext* ctx) { onProfilerButton(ctx->mBool, ctx->pPosition, false); return true; } };
		addInputAction(&actionDesc);

		// App Actions
		actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		actionDesc =
		{
			InputBindings::BUTTON_ANY, [](InputActionContext* ctx)
			{
				bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition, !bToggleMicroProfiler);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!bToggleMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
			{
#if defined(TARGET_IOS) || defined(__ANDROID__)
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
#endif				
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.5f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		return true;
	}

	static void getQuantizationSettings(eastl::string filename, int & posBits, int & texBits, int & normBits)
	{
		size_t searchPos = filename.find("_OPTIMIZED");
		size_t pBitsPos = filename.find("_p", searchPos);

		pBitsPos += 2;
		size_t uvBitsPos = filename.find_first_of("_uv", pBitsPos);
		int pBits = atoi(filename.substr(pBitsPos, uvBitsPos - pBitsPos).c_str());

		uvBitsPos += 3;
		size_t nBitsPos = filename.find_first_of("_n", uvBitsPos);
		int uvBits = atoi(filename.substr(uvBitsPos, nBitsPos - uvBitsPos).c_str());

		nBitsPos += 2;
		int nBits = atoi(filename.substr(nBitsPos, 1).c_str());

		posBits = pBits;
		texBits = uvBits;
		normBits = nBits;
	}

	static float3 getOffsetToCenterModel(eastl::vector<Mesh> & meshes, mat4 worldMat)
	{
		float maxZ = 0.0f;
		float minZ = 0.0f;
		float maxY = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float minX = 0.0f;

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			for (size_t j = 0; j < meshes[i].streams.size(); ++j)
			{
				Stream & stream = meshes[i].streams[j];

				if (stream.type == cgltf_attribute_type_position)
				{
					for (size_t k = 0; k < stream.data.size(); ++k)
					{
						vec4 pos;
						pos.setX(stream.data[k].f[0]);
						pos.setY(stream.data[k].f[1]);
						pos.setZ(stream.data[k].f[2]);

						pos = worldMat * pos;

						if (pos.getZ() > maxZ)
							maxZ = pos.getZ();
						if (pos.getZ() < minZ)
							minZ = pos.getZ();
						if (pos.getY() > maxY)
							maxY = pos.getY();
						if (pos.getY() < minY)
							minY = pos.getY();
						if (pos.getX() > maxX)
							maxX = pos.getX();
						if (pos.getX() < minX)
							minX = pos.getX();
					}
				}
			}
		}

		vec3 minPos(minX, minY, minZ);
		vec3 maxPos(maxX, maxY, maxZ);
		vec3 diagVec = maxPos - minPos;

		vec3 aabbCenter = minPos + diagVec * 0.5f;

		vec3 offset = -aabbCenter;

		return float3(offset.getX(), -minY, offset.getZ());
	}

	static bool LoadModel(LOD & lod)
	{
		eastl::string modelFileName(gModel_File);
		if (-1 != modelFileName.find("_OPTIMIZED"))
		{
			gLoadOptimizedModel = true;
			getQuantizationSettings(modelFileName, gPosBits, gTexBits, gNrmBits);
		}
		else
		{
			gLoadOptimizedModel = false;
			gPosBits = 16;
			gTexBits = 16;
			gNrmBits = 8;
		}

		::Settings settings = {};
		settings.pos_bits = gPosBits;
		settings.tex_bits = gTexBits;
		settings.nrm_bits = gNrmBits;
		settings.anim_freq = 30;

		unsigned int loadFlags = alGEN_MATERIAL_ID | alGEN_NORMALS | alMAKE_LEFT_HANDED;
		if (!gLoadOptimizedModel)
			loadFlags |= alOPTIMIZE;
		else
			loadFlags |= alIS_QUANTIZED;

		FSRoot modelRootPath = FSRoot::FSR_Absolute;
#if defined(__ANDROID__) || defined(__LINUX__)
		modelRootPath = FSRoot::FSR_Meshes;
#endif
		Model & model = lod.model;
		PropData & modelProp = lod.modelProp;
		eastl::vector<Sampler*> & pSamplers = lod.pSamplers;
		eastl::unordered_map<uint64_t, uint32_t> & samplerIDMap = lod.samplerIDMap;
		eastl::vector<uint32_t> & textureIndexforMaterial = lod.textureIndexforMaterial;
		eastl::unordered_map<uint32_t, uint32_t> & materialIDMap = lod.materialIDMap;
		eastl::vector<Texture*> & pMaterialTextures = lod.pMaterialTextures;

		bool result = AssetLoader::LoadModel(gModel_File, modelRootPath, &model, loadFlags);
		if (result == false)
		{
			return result;
		}

		// Add samplers
		SamplerDesc samplerDescDefault = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
											ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		// Set sampler desc based on glTF specification
		pSamplers.resize(model.data->samplers_count);

		if (model.data->samplers_count > 0)
		{
			for (size_t i = 0; i < model.data->samplers_count; ++i)
			{
				SamplerDesc samplerDesc = samplerDescDefault;

				cgltf_sampler sampler = model.data->samplers[i];

				switch (sampler.mag_filter)
				{
				case 9728:
					samplerDesc.mMagFilter = FILTER_NEAREST;
					break;
				case 9729:
					samplerDesc.mMagFilter = FILTER_LINEAR;
					break;
				default:
					samplerDesc.mMagFilter = (FilterType)sampler.mag_filter;
				}

				switch (sampler.min_filter)
				{
				case 9728:
					samplerDesc.mMinFilter = FILTER_NEAREST;
					break;
				case 9729:
					samplerDesc.mMinFilter = FILTER_LINEAR;
					break;
				case 9984:
					samplerDesc.mMinFilter = FILTER_NEAREST;
					samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
					break;
				case 9985:
					samplerDesc.mMinFilter = FILTER_LINEAR;
					samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
					break;
				case 9986:
					samplerDesc.mMinFilter = FILTER_NEAREST;
					samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
					break;
				case 9987:
					samplerDesc.mMinFilter = FILTER_LINEAR;
					samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
					break;
				default:
					samplerDesc.mMinFilter = (FilterType)sampler.mag_filter;
				}

				switch (sampler.wrap_s)
				{
				case 33071:
					samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
					break;
				case 33648:
					samplerDesc.mAddressU = ADDRESS_MODE_MIRROR;
					break;
				case 10497:
					samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
					break;
				default:
					samplerDesc.mAddressU = (AddressMode)sampler.wrap_s;
				}

				switch (sampler.wrap_t)
				{
				case 33071:
					samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
					break;
				case 33648:
					samplerDesc.mAddressV = ADDRESS_MODE_MIRROR;
					break;
				case 10497:
					samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
					break;
				default:
					samplerDesc.mAddressV = (AddressMode)sampler.wrap_t;
				}

				addSampler(pRenderer, &samplerDesc, &pSamplers[i]);

				uint64_t samplerID = (uint64_t)&model.data->samplers[i];
				pSamplers[i]->mSamplerId = samplerID;
				samplerIDMap[samplerID] = (uint32_t)i;
			}
		}

		// add default sampler
		pSamplers.push_back(NULL);
		addSampler(pRenderer, &samplerDescDefault, &pSamplers.back());
		pSamplers.back()->mSamplerId = 0;
		samplerIDMap[0] = uint32_t(pSamplers.size() - 1);

		size_t meshCount = model.mMeshArray.size();
		if (meshCount == 0)
			return false;

		modelProp.MeshBatches.reserve(meshCount);
		modelProp.WorldMatrix = mat4::identity();

		eastl::vector<Mesh> transparentMeshArray;
		eastl::vector<Mesh> opaqueMeshArray;
		transparentMeshArray.reserve(meshCount);
		opaqueMeshArray.reserve(meshCount);

		for (int i = 0; i < meshCount; i++)
		{
			const cgltf_material * subMeshMaterial = model.mMeshArray[i].material;
			if (subMeshMaterial && subMeshMaterial->alpha_mode != 0)
			{
				transparentMeshArray.push_back(model.mMeshArray[i]);
			}
			else
			{
				opaqueMeshArray.push_back(model.mMeshArray[i]);
			}
		}

		transparentMeshArray.resize(transparentMeshArray.size());
		opaqueMeshArray.resize(opaqueMeshArray.size());

		eastl::vector<Mesh> modelMeshArray = opaqueMeshArray;
		modelMeshArray.insert(modelMeshArray.end(), transparentMeshArray.begin(), transparentMeshArray.end());

		transparentMeshArray.clear();
		opaqueMeshArray.clear();

		QuantizationParams qp = prepareQuantization(modelMeshArray, settings);

		float3 centerPosOffset = getOffsetToCenterModel(modelMeshArray, modelProp.WorldMatrix);

		for (int i = 0; i < meshCount; i++)
		{
			Mesh & subMesh = modelMeshArray[i];
			quantizeMesh(subMesh, settings, qp);
		}

		for (cgltf_size i = 0; i < model.data->materials_count; ++i)
		{
			quantizeMaterial(&model.data->materials[i]);
		}

		// for floor
		{
			UniformObjData meshConstantBufferData = {};
			meshConstantBufferData.mWorldMat = mat4::scale(vec3(3.0f));
			meshConstantBufferData.InvTranspose = transpose(inverse(meshConstantBufferData.mWorldMat));
            BufferUpdateDesc floorShadowBuffer = {};
            floorShadowBuffer.mSize = sizeof(UniformObjData);
            floorShadowBuffer.pBuffer = pFloorShadowUniformBuffer;
            floorShadowBuffer.pData = &meshConstantBufferData;
            updateResource(&floorShadowBuffer);
		}

		for (int i = 0; i < meshCount; i++)
		{
			Mesh & subMesh = modelMeshArray[i];

			UniformObjData meshConstantBufferData = {};
			meshConstantBufferData.mWorldMat = modelProp.WorldMatrix;
			meshConstantBufferData.InvTranspose = transpose(inverse(modelProp.WorldMatrix));
			meshConstantBufferData.unlit = 0;
			meshConstantBufferData.hasAlbedoMap = 0;
			meshConstantBufferData.hasNormalMap = 0;
			meshConstantBufferData.hasMetallicRoughnessMap = 0;
			meshConstantBufferData.hasAOMap = 0;
			meshConstantBufferData.hasEmissiveMap = 0;
			meshConstantBufferData.centerOffset[0] = centerPosOffset[0];
			meshConstantBufferData.centerOffset[1] = centerPosOffset[1];
			meshConstantBufferData.centerOffset[2] = centerPosOffset[2];
			meshConstantBufferData.centerOffset[3] = 0.0f;
			meshConstantBufferData.posOffset[0] = qp.pos_offset[0] * 0.5f;
			meshConstantBufferData.posOffset[1] = qp.pos_offset[1];
			meshConstantBufferData.posOffset[2] = qp.pos_offset[2] * 0.5f;
			meshConstantBufferData.posOffset[3] = 0.0f;
			meshConstantBufferData.posScale = qp.pos_scale;
			meshConstantBufferData.uvOffset[0] = qp.uv_offset[0];
			meshConstantBufferData.uvOffset[1] = qp.uv_offset[1];
			meshConstantBufferData.uvScale[0] = qp.uv_scale[0];
			meshConstantBufferData.uvScale[1] = qp.uv_scale[1];

			MeshBatch* pMeshBatch = (MeshBatch*)conf_placement_new<MeshBatch>(conf_calloc(1, sizeof(MeshBatch)));

			modelProp.MeshBatches.push_back(pMeshBatch);

			pMeshBatch->NoofIndices = (int)subMesh.indices.size();
			pMeshBatch->NoofUVSets = 0;

			cgltf_material * subMeshMaterial = subMesh.material;

			if (gLoadOptimizedModel)
			{
				meshConstantBufferData.uvOffset[0] =
					subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.offset[0];
				meshConstantBufferData.uvOffset[1] =
					subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.offset[1];
				meshConstantBufferData.uvScale[0] =
					subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.scale[0];
				meshConstantBufferData.uvScale[1] =
					subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.scale[1];
				meshConstantBufferData.uvScale[0] *= float((1 << gTexBits) - 1);
				meshConstantBufferData.uvScale[1] *= float((1 << gTexBits) - 1);
			}

			eastl::vector<uint16_t> vertexBaseColors;
			eastl::vector<uint16_t> vertexMetallicRoughness;
			eastl::vector<uint16_t> vertexAlphaSettings;

			eastl::vector<uint16_t> meshUVs;

			uint16_t materialBaseColor[4] = { 255, 255, 255, 255 };
			uint16_t materialMetallicRoughness[2] = { 255, 255 };
			uint16_t materialAlphaSettings[2] = { 0, 255 };

			// Vertex buffers for mesh
			{
				BufferLoadDesc desc = {};
				desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
				desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;

				if (subMeshMaterial)
				{
					meshConstantBufferData.unlit = subMeshMaterial->unlit;

					materialBaseColor[0] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[0]);
					materialBaseColor[1] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[1]);
					materialBaseColor[2] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[2]);
					materialBaseColor[3] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[3]);

					materialMetallicRoughness[0] = uint16_t(subMeshMaterial->pbr_metallic_roughness.metallic_factor);
					materialMetallicRoughness[1] = uint16_t(subMeshMaterial->pbr_metallic_roughness.roughness_factor);

					materialAlphaSettings[0] = subMeshMaterial->alpha_mode;
					materialAlphaSettings[1] = uint16_t(subMeshMaterial->alpha_cutoff);
				}

				for (size_t h = 0; h < subMesh.streams.size(); ++h)
				{
					Stream const & stream = subMesh.streams[h];

					eastl::vector<uint16_t> float4Data;
					eastl::vector<uint16_t> float2Data;
					float4Data.resize(stream.data.size() * 4);
					float2Data.resize(stream.data.size() * 2);

					vertexMetallicRoughness.resize(stream.data.size() * 2);
					vertexAlphaSettings.resize(stream.data.size() * 2);

					for (size_t k = 0; k < stream.data.size(); ++k)
					{
						uint16_t f0 = (uint16_t)stream.data[k].f[0];
						uint16_t f1 = (uint16_t)stream.data[k].f[1];
						uint16_t f2 = (uint16_t)stream.data[k].f[2];
						uint16_t f3 = (uint16_t)stream.data[k].f[3];

						float4Data[k * 4 + 0] = f0;
						float4Data[k * 4 + 1] = f1;
						float4Data[k * 4 + 2] = f2;
						float4Data[k * 4 + 3] = f3;

						float2Data[k * 2 + 0] = f0;
						float2Data[k * 2 + 1] = f1;

						vertexMetallicRoughness[k * 2 + 0] = materialMetallicRoughness[0];
						vertexMetallicRoughness[k * 2 + 1] = materialMetallicRoughness[1];
						vertexAlphaSettings[k * 2 + 0] = materialAlphaSettings[0];
						vertexAlphaSettings[k * 2 + 1] = materialAlphaSettings[1];
					}

					if (stream.type == cgltf_attribute_type::cgltf_attribute_type_position)
					{
						pMeshBatch->NoofVertices = (int)stream.data.size();

						desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						desc.pData = float4Data.data();
						desc.ppBuffer = &pMeshBatch->pPositionStream;
						addResource(&desc);

						// Used to store per vertex color if a color stream does not exist
						vertexBaseColors.resize(stream.data.size() * 4);

						desc.mDesc.mVertexStride = sizeof(uint16_t) * 2;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						desc.pData = vertexMetallicRoughness.data();
						desc.ppBuffer = &pMeshBatch->pMetallicRoughnessStream;
						addResource(&desc);

						desc.mDesc.mVertexStride = sizeof(uint16_t) * 2;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						desc.pData = vertexAlphaSettings.data();
						desc.ppBuffer = &pMeshBatch->pAlphaStream;
						addResource(&desc);
					}
					else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_normal)
					{
						pMeshBatch->NoofVertices = (int)stream.data.size();

						desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						desc.pData = float4Data.data();
						desc.ppBuffer = &pMeshBatch->pNormalStream;
						addResource(&desc);
					}
					else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_texcoord)
					{
						pMeshBatch->NoofVertices = (int)stream.data.size();

						meshUVs.clear();
						meshUVs.insert(meshUVs.end(), float2Data.begin(), float2Data.end());
					}
					else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_color)
					{
						pMeshBatch->NoofVertices = (int)stream.data.size();

						desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						desc.pData = float4Data.data();
						desc.ppBuffer = &pMeshBatch->pBaseColorStream;
						addResource(&desc);
					}
				}
			}

			// Index buffer for mesh
			{
				// Index buffer for the scene
				BufferLoadDesc desc = {};
				desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
				desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				desc.mDesc.mIndexType = INDEX_TYPE_UINT32;
				desc.mDesc.mSize = sizeof(uint) * (uint)subMesh.indices.size();
				desc.pData = subMesh.indices.data();
				desc.ppBuffer = &pMeshBatch->pIndicesStream;
				addResource(&desc);
			}

			// Textures for mesh
			{
				if (!pMeshBatch->pUVStream)
				{
					BufferLoadDesc desc = {};
					desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
					desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					desc.mDesc.mVertexStride = sizeof(uint16_t) * 2;
					desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
					if (meshUVs.empty())
					{
						meshUVs.resize(pMeshBatch->NoofVertices * 2);
					}
					desc.pData = meshUVs.data();
					desc.ppBuffer = &pMeshBatch->pUVStream;
					addResource(&desc);
				}

				if (!pMeshBatch->pBaseColorStream)
				{
					for (size_t k = 0; k < vertexBaseColors.size(); k += 4)
					{
						vertexBaseColors[k] = materialBaseColor[0];
						vertexBaseColors[k + 1] = materialBaseColor[1];
						vertexBaseColors[k + 2] = materialBaseColor[2];
						vertexBaseColors[k + 3] = materialBaseColor[3];
					}

					BufferLoadDesc desc = {};
					desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
					desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
					desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
					desc.pData = vertexBaseColors.data();
					desc.ppBuffer = &pMeshBatch->pBaseColorStream;
					addResource(&desc);
				}

				//bool MR = mat->has_pbr_metallic_roughness;
				//if (MR == true)
				{
					uint32_t materialIndex = (uint32_t)textureIndexforMaterial.size();

					if (materialIDMap.find(subMesh.materialID) == materialIDMap.end())
					{
						materialIDMap[subMesh.materialID] = materialIndex;

						TextureLoadDesc textureLoadDesc = {};
						textureLoadDesc.mRoot = FSR_Textures;

						pMaterialTextures.push_back(pTextureBlack);
						textureIndexforMaterial.push_back(materialIndex);
						pMeshBatch->SamplerIndex[0] = samplerIDMap[0];

						if (subMeshMaterial && subMeshMaterial->pbr_metallic_roughness.base_color_texture.texture)
						{
							cgltf_texture* texture = subMeshMaterial->pbr_metallic_roughness.base_color_texture.texture;

							pMeshBatch->SamplerIndex[0] = samplerIDMap[(uint64_t)texture->sampler];

							eastl::string baseColorTextureName(texture->image->uri);
							eastl_size_t extensionPos = baseColorTextureName.find_last_of('.');
							baseColorTextureName.resize(extensionPos);
#if USE_BASIS					
							baseColorTextureName.append(".basis");
#endif							
							pMaterialTextures.back() = NULL;
							textureLoadDesc.pFilename = baseColorTextureName.c_str();
							textureLoadDesc.ppTexture = &pMaterialTextures.back();
							addResource(&textureLoadDesc, true);
						}

						pMaterialTextures.push_back(pTextureBlack);
						textureIndexforMaterial.push_back(materialIndex + 1);
						pMeshBatch->SamplerIndex[1] = samplerIDMap[0];

						if (subMeshMaterial && subMeshMaterial->normal_texture.texture)
						{
							cgltf_texture* texture = subMeshMaterial->normal_texture.texture;

							pMeshBatch->SamplerIndex[1] = samplerIDMap[(uint64_t)texture->sampler];

							eastl::string normalTextureName(texture->image->uri);
							eastl_size_t extensionPos = normalTextureName.find_last_of('.');
							normalTextureName.resize(extensionPos);
#if USE_BASIS				
							normalTextureName.append(".basis");
#endif
							pMaterialTextures.back() = NULL;
							textureLoadDesc.pFilename = normalTextureName.c_str();
							textureLoadDesc.ppTexture = &pMaterialTextures.back();
							addResource(&textureLoadDesc, true);
						}

						pMaterialTextures.push_back(pTextureBlack);
						textureIndexforMaterial.push_back(materialIndex + 2);
						pMeshBatch->SamplerIndex[2] = samplerIDMap[0];

						if (subMeshMaterial && subMeshMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture)
						{
							cgltf_texture* texture = subMeshMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture;

							pMeshBatch->SamplerIndex[2] = samplerIDMap[(uint64_t)texture->sampler];

							eastl::string mrTextureName(texture->image->uri);
							eastl_size_t extensionPos = mrTextureName.find_last_of('.');
							mrTextureName.resize(extensionPos);
#if USE_BASIS						
							mrTextureName.append(".basis");
#endif
							pMaterialTextures.back() = NULL;
							textureLoadDesc.pFilename = mrTextureName.c_str();
							textureLoadDesc.ppTexture = &pMaterialTextures.back();
							addResource(&textureLoadDesc, true);
						}

						pMaterialTextures.push_back(pTextureBlack);
						textureIndexforMaterial.push_back(materialIndex + 3);
						pMeshBatch->SamplerIndex[3] = samplerIDMap[0];

						if (subMeshMaterial && subMeshMaterial->occlusion_texture.texture)
						{
							cgltf_texture* texture = subMeshMaterial->occlusion_texture.texture;

							pMeshBatch->SamplerIndex[3] = samplerIDMap[(uint64_t)texture->sampler];

							eastl::string aoTextureName(texture->image->uri);
							eastl_size_t extensionPos = aoTextureName.find_last_of('.');
							aoTextureName.resize(extensionPos);
#if USE_BASIS						
							aoTextureName.append(".basis");
#endif
							pMaterialTextures.back() = NULL;
							textureLoadDesc.pFilename = aoTextureName.c_str();
							textureLoadDesc.ppTexture = &pMaterialTextures.back();
							addResource(&textureLoadDesc, true);
						}

						pMaterialTextures.push_back(pTextureBlack);
						textureIndexforMaterial.push_back(materialIndex + 4);
						pMeshBatch->SamplerIndex[4] = samplerIDMap[0];

						if (subMeshMaterial && subMeshMaterial->emissive_texture.texture)
						{
							cgltf_texture* texture = subMeshMaterial->emissive_texture.texture;

							pMeshBatch->SamplerIndex[4] = samplerIDMap[(uint64_t)texture->sampler];

							eastl::string emmisiveTextureName(texture->image->uri);
							eastl_size_t extensionPos = emmisiveTextureName.find_last_of('.');
							emmisiveTextureName.resize(extensionPos);
#if USE_BASIS						
							emmisiveTextureName.append(".basis");
#endif
							pMaterialTextures.back() = NULL;
							textureLoadDesc.pFilename = emmisiveTextureName.c_str();
							textureLoadDesc.ppTexture = &pMaterialTextures.back();
							addResource(&textureLoadDesc, true);
						}
					}

					pMeshBatch->MaterialIndex = materialIDMap[subMesh.materialID];
				}
			}

			//set constant buffer for mesh
			{
				int matIndex = pMeshBatch->MaterialIndex;

				if (pMaterialTextures[matIndex] != pTextureBlack)
					meshConstantBufferData.hasAlbedoMap = 1;
				if (pMaterialTextures[matIndex + 1] != pTextureBlack)
					meshConstantBufferData.hasNormalMap = 1;
				if (pMaterialTextures[matIndex + 2] != pTextureBlack)
					meshConstantBufferData.hasMetallicRoughnessMap = 1;
				if (pMaterialTextures[matIndex + 3] != pTextureBlack)
					meshConstantBufferData.hasAOMap = 1;
				if (pMaterialTextures[matIndex + 4] != pTextureBlack)
					meshConstantBufferData.hasEmissiveMap = 1;

				BufferLoadDesc desc = {};
				desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				desc.mDesc.mSize = sizeof(UniformObjData);
				desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				desc.pData = &meshConstantBufferData;
				desc.ppBuffer = &pMeshBatch->pConstantBuffer;
				addResource(&desc);
			}
		}

		finishResourceLoading();
		return true;
	}

	static void RemoveShaderResources()
	{
		removeShader(pRenderer, pShaderZPass);
		removeShader(pRenderer, pShaderZPass_NonOPtimized);
		removeShader(pRenderer, pVignetteShader);
		removeShader(pRenderer, pFloorShader);
		removeShader(pRenderer, pMeshOptDemoShader);
		removeShader(pRenderer, pFXAAShader);
		removeShader(pRenderer, pWaterMarkShader);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);
		removeRootSignature(pRenderer, pRootSignature);
	}

	static void RemoveModelDependentResources()
	{
		RemoveShaderResources();

		for (size_t i = 0; i < gLODs.size(); ++i)
		{
			Model & model = gLODs[i].model;
			PropData & modelProp = gLODs[i].modelProp;
			eastl::vector<Sampler*> & pSamplers = gLODs[i].pSamplers;
			eastl::unordered_map<uint64_t, uint32_t> & samplerIDMap = gLODs[i].samplerIDMap;
			eastl::vector<uint32_t> & textureIndexforMaterial = gLODs[i].textureIndexforMaterial;
			eastl::unordered_map<uint32_t, uint32_t> & materialIDMap = gLODs[i].materialIDMap;
			eastl::vector<Texture*> & pMaterialTextures = gLODs[i].pMaterialTextures;

			for (MeshBatch* meshBatch : modelProp.MeshBatches)
			{
				removeResource(meshBatch->pConstantBuffer);
				removeResource(meshBatch->pIndicesStream);
				removeResource(meshBatch->pNormalStream);
				removeResource(meshBatch->pPositionStream);
				removeResource(meshBatch->pUVStream);
				removeResource(meshBatch->pBaseColorStream);
				removeResource(meshBatch->pMetallicRoughnessStream);
				removeResource(meshBatch->pAlphaStream);
				meshBatch->~MeshBatch();
				conf_free(meshBatch);
			}

			modelProp.MeshBatches.clear();

			for (uint i = 0; i < pMaterialTextures.size(); ++i)
			{
				if (pMaterialTextures[i] && pMaterialTextures[i] != pTextureBlack)
				{
					removeResource(pMaterialTextures[i]);
				}
			}

			pMaterialTextures.clear();
			materialIDMap.clear();
			textureIndexforMaterial.clear();

			for (uint i = 0; i < pSamplers.size(); ++i)
			{
				removeSampler(pRenderer, pSamplers[i]);
			}

			pSamplers.clear();
			samplerIDMap.clear();

			model.mMeshArray.clear();
			cgltf_free(model.data);
		}
		
		gLODs.resize(1);
		gCurrentLod = 0;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		exitProfiler();

		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pShadowUniformBuffer[i]);
			removeResource(pUniformBuffer[i]);
			removeResource(pFloorUniformBuffer[i]);
		}

		removeResource(pFloorShadowUniformBuffer);

		removeDepthState(pDepthStateForRendering);
		removeBlendState(pBlendStateAlphaBlend);
		removeRasterizerState(pRasterizerStateCullBack);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeSampler(pRenderer, pBilinearClampSampler);

		removeResource(TriangularVB);

		removeResource(pFloorVB);
		removeResource(pFloorIB);

		removeResource(pTextureBlack);
		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}
    
    static void LoadPipelines()
    {
        VertexLayout vertexLayout = {};
        vertexLayout.mAttribCount = 6;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA16UI;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA16I;
        vertexLayout.mAttribs[1].mBinding = 1;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 0;// 3 * sizeof(float);
        vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[2].mFormat = ImageFormat::RG16UI;
        vertexLayout.mAttribs[2].mBinding = 2;
        vertexLayout.mAttribs[2].mLocation = 2;
        vertexLayout.mAttribs[2].mOffset = 0;// 6 * sizeof(float);
        
        vertexLayout.mAttribs[3].mSemantic = SEMANTIC_COLOR;
        vertexLayout.mAttribs[3].mFormat = ImageFormat::RGBA16UI;
        vertexLayout.mAttribs[3].mBinding = 3;
        vertexLayout.mAttribs[3].mLocation = 3;
        vertexLayout.mAttribs[3].mOffset = 0;// 3 * sizeof(float);
        vertexLayout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD1;
        vertexLayout.mAttribs[4].mFormat = ImageFormat::RG16UI;
        vertexLayout.mAttribs[4].mBinding = 4;
        vertexLayout.mAttribs[4].mLocation = 4;
        vertexLayout.mAttribs[4].mOffset = 0;// 6 * sizeof(float);
        vertexLayout.mAttribs[5].mSemantic = SEMANTIC_TEXCOORD2;
        vertexLayout.mAttribs[5].mFormat = ImageFormat::RG16UI;
        vertexLayout.mAttribs[5].mBinding = 5;
        vertexLayout.mAttribs[5].mLocation = 5;
        vertexLayout.mAttribs[5].mOffset = 0;// 6 * sizeof(float);
        
        PipelineDesc desc = {};
        
        /************************************************************************/
        // Setup the resources needed for shadow map
        /************************************************************************/
        {
            desc = {};
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
            shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            shadowMapPipelineSettings.mRenderTargetCount = 0;
            shadowMapPipelineSettings.pDepthState = pDepthStateForRendering;
            shadowMapPipelineSettings.mDepthStencilFormat = pRenderTargetShadowMap->mDesc.mFormat;
            shadowMapPipelineSettings.mSampleCount = pRenderTargetShadowMap->mDesc.mSampleCount;
            shadowMapPipelineSettings.mSampleQuality = pRenderTargetShadowMap->mDesc.mSampleQuality;
            shadowMapPipelineSettings.pRootSignature = pRootSignature;
            shadowMapPipelineSettings.pRasterizerState = pRasterizerStateCullBack;
            shadowMapPipelineSettings.pShaderProgram = pShaderZPass;
            shadowMapPipelineSettings.pVertexLayout = &vertexLayout;
            addPipeline(pRenderer, &desc, &pPipelineShadowPass);
        }
        
        {
            desc = {};
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = 1;
            pipelineSettings.pDepthState = pDepthStateForRendering;
            pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
            pipelineSettings.pBlendState = pBlendStateAlphaBlend;
            pipelineSettings.pColorFormats = &pForwardRT->mDesc.mFormat;
            pipelineSettings.pSrgbValues = &pForwardRT->mDesc.mSrgb;
            pipelineSettings.mSampleCount = pForwardRT->mDesc.mSampleCount;
            pipelineSettings.mSampleQuality = pForwardRT->mDesc.mSampleQuality;
            pipelineSettings.pRootSignature = pRootSignature;
            pipelineSettings.pVertexLayout = &vertexLayout;
            pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
            pipelineSettings.pShaderProgram = pMeshOptDemoShader;
            addPipeline(pRenderer, &desc, &pMeshOptDemoPipeline);
        }
        
        VertexLayout screenTriangle_VertexLayout = {};
        
        screenTriangle_VertexLayout.mAttribCount = 2;
        screenTriangle_VertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        screenTriangle_VertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
        screenTriangle_VertexLayout.mAttribs[0].mBinding = 0;
        screenTriangle_VertexLayout.mAttribs[0].mLocation = 0;
        screenTriangle_VertexLayout.mAttribs[0].mOffset = 0;
        screenTriangle_VertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        screenTriangle_VertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
        screenTriangle_VertexLayout.mAttribs[1].mBinding = 0;
        screenTriangle_VertexLayout.mAttribs[1].mLocation = 1;
        screenTriangle_VertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
        
        {
            desc = {};
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
            shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            shadowMapPipelineSettings.mRenderTargetCount = 0;
            shadowMapPipelineSettings.pDepthState = pDepthStateForRendering;
            shadowMapPipelineSettings.mDepthStencilFormat = pRenderTargetShadowMap->mDesc.mFormat;
            shadowMapPipelineSettings.mSampleCount = pRenderTargetShadowMap->mDesc.mSampleCount;
            shadowMapPipelineSettings.mSampleQuality = pRenderTargetShadowMap->mDesc.mSampleQuality;
            shadowMapPipelineSettings.pRootSignature = pRootSignature;
            shadowMapPipelineSettings.pRasterizerState = pRasterizerStateCullBack;
            shadowMapPipelineSettings.pShaderProgram = pShaderZPass_NonOPtimized;
            shadowMapPipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
            addPipeline(pRenderer, &desc, &pPipelineShadowPass_NonOPtimized);
        }
        
        {
            desc = {};
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = 1;
            pipelineSettings.pDepthState = pDepthStateForRendering;
            pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
            pipelineSettings.pBlendState = pBlendStateAlphaBlend;
            pipelineSettings.pColorFormats = &pForwardRT->mDesc.mFormat;
            pipelineSettings.pSrgbValues = &pForwardRT->mDesc.mSrgb;
            pipelineSettings.mSampleCount = pForwardRT->mDesc.mSampleCount;
            pipelineSettings.mSampleQuality = pForwardRT->mDesc.mSampleQuality;
            pipelineSettings.pRootSignature = pRootSignature;
            pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
            pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
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
            pipelineSettings.pBlendState = pBlendStateAlphaBlend;
            pipelineSettings.pColorFormats = &pPostProcessRT->mDesc.mFormat;
            pipelineSettings.pSrgbValues = &pPostProcessRT->mDesc.mSrgb;
            pipelineSettings.mSampleCount = pPostProcessRT->mDesc.mSampleCount;
            pipelineSettings.mSampleQuality = pPostProcessRT->mDesc.mSampleQuality;
            pipelineSettings.pRootSignature = pRootSignature;
            pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
            pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
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
            pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
            pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
            pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
            pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
            pipelineSettings.pRootSignature = pRootSignature;
            pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
            pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
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
            pipelineSettings.pBlendState = pBlendStateAlphaBlend;
            pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
            pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
            pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
            pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
            pipelineSettings.pRootSignature = pRootSignature;
            pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
            pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
            pipelineSettings.pShaderProgram = pWaterMarkShader;
            addPipeline(pRenderer, &desc, &pWaterMarkPipeline);
        }
    }

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addRenderTargets())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#if defined(TARGET_IOS) || defined(__ANDROID__)
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);

		InitModelDependentResources();

        LoadPipelines();

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
		screenQuadVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		screenQuadVbDesc.mDesc.mSize = screenQuadVbDesc.mDesc.mVertexStride * 6;
		screenQuadVbDesc.pData = screenWaterMarkPoints;
		screenQuadVbDesc.ppBuffer = &WaterMarkVB;
		addResource(&screenQuadVbDesc);

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

		RemoveModelDependentResources();

		unloadProfiler();
		gAppUI.Unload();

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Unload();
#endif

		removeResource(WaterMarkVB);

        RemovePipelines();
        
		removeSwapChain(pRenderer, pSwapChain);

		removeRenderTarget(pRenderer, pPostProcessRT);
		removeRenderTarget(pRenderer, pForwardRT);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pRenderTargetShadowMap);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

#if !defined(__ANDROID__) && !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != bToggleVSync)
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

		gUniformData.mQuantizationParams = int4(gPosBits, gTexBits, gNrmBits, 0);

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
		const float shadowHalfRange = shadowRange * 0.5f;
		mat4 lightProjMat = mat4::orthographicReverseZ(-shadowRange, shadowRange, -shadowRange, shadowRange, shadowRange * 0.5f, shadowRange * 4.0f);

		gShadowUniformData.ViewProj = lightProjMat * lightView;

		/************************************************************************/
		/************************************************************************/

		// ProfileSetDisplayMode()
		if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
		{
			Profile& S = *ProfileGet();
			int nValue = bToggleMicroProfiler ? 1 : 0;
			nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
			S.nDisplay = nValue;

			bPrevToggleMicroProfiler = bToggleMicroProfiler;
		}

		gAppUI.Update(deltaTime);

		if (gCurrentLod >= (int)gLODs.size())
			gCurrentLod = (int)gLODs.size() - 1;       
	}
	
	void PostDrawUpdate()
	{
		bool fileExists = false;
		File sceneFile;

#if defined(__ANDROID__) || defined(__LINUX__)
		if (guiModelToLoadIndex != modelToLoadIndex)
		{
			modelToLoadIndex = guiModelToLoadIndex;
			strcpy(gGuiModelToLoad, gModelFiles[modelToLoadIndex].c_str());

			if (strcmp(gGuiModelToLoad, gModel_File) != 0)
			{
				fileExists = sceneFile.Open(gGuiModelToLoad, FileMode::FM_ReadBinary, FSRoot::FSR_Meshes);
				if (fileExists)
				{
					sceneFile.Close();
					strcpy(gModel_File, gGuiModelToLoad);
					Unload();
					Load();
				}
				else
				{
					strcpy(gGuiModelToLoad, gModel_File);
				}
			}
			else
			{
				guiModelToLoadIndex = modelToLoadIndex;
			}
		}
#else
		if (strcmp(gGuiModelToLoad, gModel_File) != 0)
		{
			fileExists = sceneFile.Open(gGuiModelToLoad, FileMode::FM_ReadBinary, FSRoot::FSR_Absolute);
			if (fileExists)
			{
				sceneFile.Close();
				strcpy(gModel_File, gGuiModelToLoad);
				if (gAddLod)
				{
					AddLOD();
				}
				else
				{
					Unload();
					Load();
				}
			}
			else
			{
				strcpy(gGuiModelToLoad, gModel_File);
			}
		}
#endif
	}

	static void LoadNewModel()
	{
		eastl::vector<eastl::string> extFilter;
		extFilter.push_back("gltf");
		extFilter.push_back("glb");
		eastl::string meshDir = FileSystem::GetProgramDir();
		meshDir.append(FileSystem::FixPath("", FSRoot::FSR_Meshes));
#ifdef _WIN32
		eastl::replace(meshDir.begin(), meshDir.end(), '/', '\\');
#endif
		FileSystem::OpenFileDialog("Select model to load", meshDir,
			[](eastl::string url, void* pUser)
		{
			if (url.size())
			{
				strcpy(gGuiModelToLoad, url.c_str());
			}
		}, NULL, "Model File", extFilter);
        
        gAddLod = false;
	}

	static void LoadLOD()
	{
		waitQueueIdle(pGraphicsQueue);

		eastl::vector<eastl::string> extFilter;
		extFilter.push_back("gltf");
		extFilter.push_back("glb");
		eastl::string meshDir = FileSystem::GetProgramDir();
		meshDir.append(FileSystem::FixPath("", FSRoot::FSR_Meshes));

#ifdef _WIN32
		eastl::replace(meshDir.begin(), meshDir.end(), '/', '\\');
#endif
		FileSystem::OpenFileDialog("Select model to load", meshDir,
			[](eastl::string url, void* pUser)
		{
			if (url.size())
			{
				strcpy(gGuiModelToLoad, url.c_str());
			}
		},
			NULL, "Model File", extFilter);

        gAddLod = true;
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&shaderCbv);

		RenderTarget* pRenderTarget = NULL;

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		vec4 bgColor = vec4(float((gBackroundColor >> 24) & 0xff),
			float((gBackroundColor >> 16) & 0xff),
			float((gBackroundColor >> 8) & 0xff),
			float((gBackroundColor >> 0) & 0xff)) / 255.0f;


		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

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
            
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Floor", true);

			pRenderTarget = pForwardRT;

			TextureBarrier barriers[] =
			{
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
				{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
			};

			cmdResourceBarrier(cmd, 0, NULL, 3, barriers, false);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pFloorPipeline);

			// Update uniform buffers
			BufferUpdateDesc Cb = { pFloorUniformBuffer[gFrameIndex], &gFloorUniformBlock };
			updateResource(&Cb);

			DescriptorData params[4] = {};
			params[0].pName = "cbPerFrame";
			params[0].ppBuffers = &pFloorUniformBuffer[gFrameIndex];
			params[1].pName = "ShadowUniformBuffer";
			params[1].ppBuffers = &pShadowUniformBuffer[gFrameIndex];
			params[2].pName = "ShadowTexture";
			params[2].ppTextures = &pRenderTargetShadowMap->pTexture;
			params[3].pName = "clampMiplessLinearSampler";
			params[3].ppSamplers = &pBilinearClampSampler;

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 4, params);

			cmdBindVertexBuffer(cmd, 1, &pFloorVB, NULL);
			cmdBindIndexBuffer(cmd, pFloorIB, 0);

			cmdDrawIndexed(cmd, 6, 0, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		//// draw scene
		{
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
            loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
            
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Scene", true);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pMeshOptDemoPipeline);

			DescriptorData params[15] = {};
			params[0].pName = "cbPerPass";
			params[0].ppBuffers = &pUniformBuffer[gFrameIndex];
			params[1].pName = "ShadowUniformBuffer";
			params[1].ppBuffers = &pShadowUniformBuffer[gFrameIndex];
			params[2].pName = "ShadowTexture";
			params[2].ppTextures = &pRenderTargetShadowMap->pTexture;
			params[3].pName = "clampMiplessLinearSampler";
			params[3].ppSamplers = &pBilinearClampSampler;

			for (MeshBatch* mesh : gLODs[gCurrentLod].modelProp.MeshBatches)
			{
				params[4].pName = "cbPerProp";
				params[4].ppBuffers = &mesh->pConstantBuffer;

				int materialIndex = mesh->MaterialIndex;

				//bind textures
				const char* texNames[5] = {
					"albedoMap",
					"normalMap",
					"metallicRoughnessMap",
					"aoMap",
					"emissiveMap"
				};

				const char* samplerNames[5] = {
					"samplerAlbedo",
					"samplerNormal",
					"samplerMR",
					"samplerAO",
					"samplerEmissive"
				};

				for (int j = 0; j < 5; ++j)
				{
					params[5 + j].pName = texNames[j];
					uint textureId = gLODs[gCurrentLod].textureIndexforMaterial[materialIndex + j];
					params[5 + j].ppTextures = &gLODs[gCurrentLod].pMaterialTextures[textureId];

					params[10 + j].pName = samplerNames[j];
					params[10 + j].ppSamplers = &gLODs[gCurrentLod].pSamplers[mesh->SamplerIndex[j]];
				}

				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 15, params);

				Buffer* pVertexBuffers[] = { mesh->pPositionStream, mesh->pNormalStream, mesh->pUVStream,
					mesh->pBaseColorStream, mesh->pMetallicRoughnessStream, mesh->pAlphaStream };
				cmdBindVertexBuffer(cmd, 6, pVertexBuffers, NULL);

				cmdBindIndexBuffer(cmd, mesh->pIndicesStream, 0);

				cmdDrawIndexed(cmd, mesh->NoofIndices, 0, 0);
			}

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		{
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
            loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
            
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Vignetting", true);

			pRenderTarget = pPostProcessRT;

			TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pForwardRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
			};

			cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pVignettePipeline);

			DescriptorData params[3] = {};
			params[0].pName = "cbPerFrame";
			params[0].ppBuffers = &pFloorUniformBuffer[gFrameIndex];
			params[1].pName = "sceneTexture";
			params[1].ppTextures = &pForwardRT->pTexture;
			params[2].pName = "clampMiplessLinearSampler";
			params[2].ppSamplers = &pBilinearClampSampler;

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 3, params);

			cmdBindVertexBuffer(cmd, 1, &TriangularVB, NULL);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		{
			TextureBarrier barriers[] =
			{
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pPostProcessRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
			};

			cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0].r = 0.0f;
			loadActions.mClearColorValues[0].g = 0.0f;
			loadActions.mClearColorValues[0].b = 0.0f;
			loadActions.mClearColorValues[0].a = 0.0f;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "FXAA", true);

			cmdBindPipeline(cmd, pFXAAPipeline);

			FXAAINFO FXAAinfo;
			FXAAinfo.ScreenSize = vec2((float)mSettings.mWidth, (float)mSettings.mHeight);
			FXAAinfo.Use = bToggleFXAA ? 1 : 0;

			DescriptorData params[3] = {};
			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pPostProcessRT->pTexture;
			params[1].pName = "clampMiplessLinearSampler";
			params[1].ppSamplers = &pBilinearClampSampler;
			params[2].pName = "FXAARootConstant";
			params[2].pRootConstant = &FXAAinfo;

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 3, params);

			cmdBindVertexBuffer(cmd, 1, &TriangularVB, NULL);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		if (bScreenShotMode)
		{
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Water Mark", true);

			cmdBindPipeline(cmd, pWaterMarkPipeline);

			DescriptorData params[2] = {};
			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pTextureBlack;
			params[1].pName = "clampMiplessLinearSampler";
			params[1].ppSamplers = &pBilinearClampSampler;

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 2, params);

			cmdBindVertexBuffer(cmd, 1, &WaterMarkVB, NULL);
			cmdDraw(cmd, 6, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		if (!bScreenShotMode)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
			static HiresTimer gTimer;
			gTimer.GetUSec(true);

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
#if defined(TARGET_IOS) || defined(__ANDROID__)
			//gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

			gAppUI.DrawText(
				cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)
			gAppUI.DrawText(
				cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
				&gFrameTimeDraw);
			gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#endif

			cmdDrawProfiler(cmd);

			gAppUI.Gui(pGuiWindow);
			gAppUI.Draw(cmd);

			gAppUI.Gui(pGuiGraphics);
			gAppUI.Draw(cmd);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		TextureBarrier barriers[] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		static int frameCount = 0;
		frameCount += 1;
		flipProfiler();
		
		PostDrawUpdate();
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
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;

		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		swapChainDesc.mSrgb = false;

		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addRenderTargets()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mFormat = ImageFormat::RGBA8;

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
		RT.pDebugName = L"Render Target";
		addRenderTarget(pRenderer, &RT, &pForwardRT);

		RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mFormat = ImageFormat::RGBA8;
		RT.mWidth = mSettings.mWidth;
		RT.mHeight = mSettings.mHeight;
		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.pDebugName = L"Post Process Render Target";
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
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		/************************************************************************/
		// Shadow Map Render Target
		/************************************************************************/

		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue.depth = 0.0f;
		shadowRTDesc.mClearValue.stencil = 0;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mFormat = ImageFormat::D32F;
		shadowRTDesc.mWidth = SHADOWMAP_RES;
		shadowRTDesc.mHeight = SHADOWMAP_RES;
		shadowRTDesc.mSampleCount = (SampleCount)SHADOWMAP_MSAA_SAMPLES;
		shadowRTDesc.mSampleQuality = 0;    // don't need higher quality sample patterns as the texture will be blurred heavily
		shadowRTDesc.pDebugName = L"Shadow Map RT";

		addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadowMap);

		return pDepthBuffer != NULL && pRenderTargetShadowMap != NULL;
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

DEFINE_APPLICATION_MAIN(MeshOptimization)
