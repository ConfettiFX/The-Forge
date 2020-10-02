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

///////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
///////////////////////////////////////////////////////////////////////////////

// TODO : Add Confetti copyright statement here, as well as note that intel code is modified

// Unit test headers
#include "AsteroidSim.h"
#include "TextureGen.h"
#include "NoiseOctaves.h"
#include "Random.h"

//TinySTL
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"

//Renderer
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Core/ThreadSystem.h"
#include "../../../../Common_3/OS/Math/MathTypes.h"

#if !defined(TARGET_IOS)
//PostProcess
#include "../../../../Middleware_3/PaniniProjection/Panini.h"
#endif

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

const uint32_t gImageCount = 3;

#define MAX_LOD_OFFSETS 10

Timer      gAccumTimer;
HiresTimer mFrameTimer;

ThreadSystem* pThreadSystem;

struct UniformViewProj
{
	mat4 mProjectView;
};

struct UniformCompute
{
	mat4     mViewProj;
	vec4     mCamPos;
	float    mDeltaTime;
	uint     mStartIndex;
	uint     mEndIndex;
	uint     mNumLODs;
#if defined(METAL)
	uint     mIndexOffsets[MAX_LOD_OFFSETS + 1];
#else
	uint     mIndexOffsets[4 * MAX_LOD_OFFSETS + 1];    // Andrés: Do VK/DX samples work with this declaration? Doesn't match rootConstant.
#endif
};

struct UniformBasic
{
	mat4     mModelViewProj;
	mat4     mNormalMat;
	float4   mSurfaceColor;
	float4   mDeepColor;
	int32_t  mTextureID;
	uint32_t _pad0[3];
};

struct IndirectArguments
{
	//16 - byte aligned
#if defined(DIRECT3D12)
	// Currently setting a root constant only works with Dx
	uint32_t                   mDrawID;
	IndirectDrawIndexArguments mDrawArgs;
	uint32_t                   mPad[2];
#else
	IndirectDrawIndexArguments mDrawArgs;
#if !defined(ORBIS)
	// Padding not supported on Orbis
	uint32_t                   mPad[3];
#endif
#endif
};

struct Subset
{
	CmdPool*           pCmdPools[gImageCount];
	Cmd*               pCmds[gImageCount];
	Buffer*            pAsteroidInstanceBuffer;
	Buffer*            pSubsetIndirect;
	IndirectArguments* mIndirectArgs;
};

struct ThreadData
{
	uint32_t      mIndex;
	mat4          mViewProj;
	uint32_t      mFrameIndex;
	RenderTarget* pRenderTarget;
	RenderTarget* pDepthBuffer;
	float         mDeltaTime;
};

struct Vertex
{
	vec4 mPosition;
	vec4 mNormal;
};

enum
{
	RenderingMode_Instanced = 0,
	RenderingMode_ExecuteIndirect = 1,
	RenderingMode_GPUUpdate = 2,
	RenderingMode_Count,
};

// Simulation parameters
// 2.5k asteroids for switch debug because NX Cpu in debug mode can't loop over more objects at a practical speed for this demo.
#if defined(NX64) && defined(NN_SDK_BUILD_DEBUG)
const uint32_t gNumAsteroids = 2500U;
#elif defined ANDROID
const uint32_t gNumAsteroids = 10000U;
#else
const uint32_t gNumAsteroids = 50000U;    // 50000 is optimal.
#endif

#if defined(PROSPERO)
const uint32_t gNumSubsets = 4;           // To avoid overflowing command buffer memory of 1MB
#else
const uint32_t gNumSubsets = 1;           // 4 is optimal. Also equivalent to the number of threads used.
#endif
const uint32_t gNumAsteroidsPerSubset = (gNumAsteroids + gNumSubsets - 1) / gNumSubsets;
const uint32_t gTextureCount = 10;

ProfileToken   gGpuProfileToken;
AsteroidSimulation      gAsteroidSim;
eastl::vector<Subset>   gAsteroidSubsets;
ThreadData              gThreadData[gNumSubsets];
Texture*                pAsteroidTex = NULL;
bool                    gUseThreads = true;
bool                    gToggleVSync = false;
uint32_t                gRenderingMode = RenderingMode_GPUUpdate;
uint32_t                gPreviousRenderingMode = gRenderingMode;

Renderer* pRenderer = NULL;

Queue*      pGraphicsQueue = NULL;
CmdPool*    pCmdPools[gImageCount];
Cmd*        pCmds[gImageCount];
CmdPool*    pComputeCmdPools[gImageCount];
Cmd*        pComputeCmds[gImageCount];
CmdPool*    pUICmdPools[gImageCount];
Cmd*        pUICmds[gImageCount];

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

// Basic shader variables, used by instanced rendering.
Shader*           pBasicShader = NULL;
Pipeline*         pBasicPipeline = NULL;
RootSignature*    pBasicRoot = NULL;
Sampler*          pBasicSampler = NULL;

// Execute Indirect variables
Shader*           pIndirectShader = NULL;
Pipeline*         pIndirectPipeline = NULL;
RootSignature*    pIndirectRoot = NULL;
Buffer*           pIndirectBuffer = NULL;
Buffer*           pIndirectUniformBuffer[gImageCount] = { NULL };
CommandSignature* pIndirectCommandSignature = NULL;
CommandSignature* pIndirectSubsetCommandSignature = NULL;

// Compute shader variables
Shader*           pComputeShader = NULL;
Pipeline*         pComputePipeline = NULL;
RootSignature*    pComputeRoot = NULL;
Buffer*           pComputeUniformBuffer[gImageCount] = {};

// Skybox Variables
Shader*           pSkyBoxDrawShader = NULL;
Pipeline*         pSkyBoxDrawPipeline = NULL;
RootSignature*    pSkyBoxRoot = NULL;
Sampler*          pSkyBoxSampler = NULL;
Buffer*           pSkyboxUniformBuffer[gImageCount] = { NULL };
Buffer*           pSkyBoxVertexBuffer = NULL;
Texture*          pSkyBoxTextures[6];

// Descriptor binder
DescriptorSet*    pDescriptorSetSkybox[2] = { NULL };
DescriptorSet*    pDescriptorSetCompute[2] = { NULL };
DescriptorSet*    pDescriptorSetIndirectDraw[2] = { NULL };
DescriptorSet*    pDescriptorSetDirectDraw[2] = { NULL };

// Necessary buffers
Buffer* pAsteroidVertexBuffer = NULL;
Buffer* pAsteroidIndexBuffer = NULL;
Buffer* pStaticAsteroidBuffer = NULL;
Buffer* pDynamicAsteroidBuffer = NULL;

// UI
UIApp              gAppUI;
GuiComponent*      pGui;
ICameraController* pCameraController = NULL;
VirtualJoystickUI gVirtualJoystick;


uint32_t gFrameIndex = 0;

const char* pSkyBoxImageFileNames[] = { "Skybox_right1",  "Skybox_left2",  "Skybox_top3",
										"Skybox_bottom4", "Skybox_front5", "Skybox_back6" };

float skyBoxPoints[] = {
	10.0f,  -10.0f, -10.0f, 6.0f,    // -z
	-10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
	-10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

	-10.0f, -10.0f, 10.0f,  2.0f,    //-x
	-10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
	-10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

	10.0f,  -10.0f, -10.0f, 1.0f,    //+x
	10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
	10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

	-10.0f, -10.0f, 10.0f,  5.0f,    // +z
	-10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
	10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

	-10.0f, 10.0f,  -10.0f, 3.0f,    //+y
	10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
	10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

	10.0f,  -10.0f, 10.0f,  4.0f,    //-y
	10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
	-10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
};

// Panini Projection state and parameter variables
#if !defined(TARGET_IOS)
Panini           gPanini;
PaniniParameters gPaniniParams;
#endif
DynamicUIWidgets gPaniniControls;
RenderTarget*     pIntermediateRenderTarget = NULL;
bool              gbPaniniEnabled = false;
TextDrawDesc      gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class ExecuteIndirect: public IApp
{
	public:
	ExecuteIndirect()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
		gToggleVSync = mSettings.mDefaultVSyncEnabled;
	}
	
	bool Init()
	{
        // FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,	"Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES,	"CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,		"GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,			"Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,			"Fonts");
		
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

			addCmdPool(pRenderer, &cmdPoolDesc, &pUICmdPools[i]);
			cmdDesc.pPool = pUICmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pUICmds[i]);

			addCmdPool(pRenderer, &cmdPoolDesc, &pComputeCmdPools[i]);
			cmdDesc.pPool = pComputeCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pComputeCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
            textureDesc.pFileName = pSkyBoxImageFileNames[i];
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			addResource(&textureDesc, NULL);
		}

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		CreateTextures(gTextureCount);

		CreateSubsets();

		initThreadSystem(&pThreadSystem);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
							FILTER_LINEAR,
							MIPMAP_MODE_NEAREST,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pBasicSampler);
		addSampler(pRenderer, &samplerDesc, &pSkyBoxSampler);

		ShaderLoadDesc instanceShader = {};
		instanceShader.mStages[0] = { "basic.vert", NULL, 0};
		instanceShader.mStages[1] = { "basic.frag", NULL, 0};

		ShaderLoadDesc indirectShader = {};
		indirectShader.mStages[0] = { "ExecuteIndirect.vert", NULL, 0};
		indirectShader.mStages[1] = { "ExecuteIndirect.frag", NULL, 0};

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0};
		skyShader.mStages[1] = { "skybox.frag", NULL, 0};

		ShaderLoadDesc gpuUpdateShader = {};
		gpuUpdateShader.mStages[0] = { "ComputeUpdate.comp", NULL, 0};

		addShader(pRenderer, &instanceShader, &pBasicShader);
		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &indirectShader, &pIndirectShader);
		addShader(pRenderer, &gpuUpdateShader, &pComputeShader);

		const char* pStaticSamplerNames[] = { "uSampler0", "uSampler1" };
		Sampler* pStaticSamplers[] = { pBasicSampler, pSkyBoxSampler };
		RootSignatureDesc basicRootDesc = { &pBasicShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		RootSignatureDesc skyRootDesc = { &pSkyBoxDrawShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		RootSignatureDesc computeRootDesc = { &pComputeShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		RootSignatureDesc indirectRootDesc = { &pIndirectShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		addRootSignature(pRenderer, &basicRootDesc, &pBasicRoot);
		addRootSignature(pRenderer, &skyRootDesc, &pSkyBoxRoot);
		addRootSignature(pRenderer, &computeRootDesc, &pComputeRoot);
		addRootSignature(pRenderer, &indirectRootDesc, &pIndirectRoot);

		DescriptorSetDesc setDesc = { pSkyBoxRoot, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pSkyBoxRoot, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
		// GPU Culling
		setDesc = { pComputeRoot, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCompute[0]);
		setDesc = { pComputeRoot, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCompute[1]);
		// Indirect Draw
		setDesc = { pIndirectRoot, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetIndirectDraw[0]);
		setDesc = { pIndirectRoot, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetIndirectDraw[1]);
		// Direct Draw
		setDesc = { pBasicRoot, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDirectDraw[0]);
		setDesc = { pBasicRoot, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, gNumSubsets };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDirectDraw[1]);

		/* Setup Pipelines */
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineDesc = desc.mComputeDesc;
		computePipelineDesc.pShaderProgram = pComputeShader;
		computePipelineDesc.pRootSignature = pComputeRoot;
		addPipeline(pRenderer, &desc, &pComputePipeline);

		/* Initialize Asteroid Simulation */

		eastl::vector<Vertex>   vertices;
		eastl::vector<uint16_t> indices;
		uint32_t                  numVerticesPerMesh;
		gAsteroidSim.numLODs = 3;
		gAsteroidSim.indexOffsets = (int*)tf_calloc(gAsteroidSim.numLODs + 2, sizeof(int));

		CreateAsteroids(vertices, indices, gAsteroidSim.numLODs, 1000, 123, numVerticesPerMesh, gAsteroidSim.indexOffsets);
		gAsteroidSim.Init(123, gNumAsteroids, 1000, numVerticesPerMesh, gTextureCount);

		/* Prepare buffers */

		BufferLoadDesc bufDesc;

		uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufDesc.mDesc.mSize = skyBoxDataSize;
		bufDesc.pData = skyBoxPoints;
		bufDesc.ppBuffer = &pSkyBoxVertexBuffer;
		addResource(&bufDesc, NULL);

		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufDesc.mDesc.mSize = sizeof(UniformViewProj);
		bufDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			bufDesc.ppBuffer = &pIndirectUniformBuffer[i];
			addResource(&bufDesc, NULL);
			bufDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&bufDesc, NULL);
		}

		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		bufDesc.mDesc.mFirstElement = 0;
		bufDesc.mDesc.mElementCount = gNumAsteroidsPerSubset;
		bufDesc.mDesc.mStructStride = sizeof(UniformBasic);
		bufDesc.mDesc.mSize = bufDesc.mDesc.mElementCount * bufDesc.mDesc.mStructStride;
		bufDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			bufDesc.ppBuffer = &gAsteroidSubsets[i].pAsteroidInstanceBuffer;
			addResource(&bufDesc, NULL);
		}

		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufDesc.mDesc.mSize = sizeof(UniformCompute);
		bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			bufDesc.ppBuffer = &pComputeUniformBuffer[i];
			addResource(&bufDesc, NULL);
		}

		// Vertex Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(Vertex) * (uint32_t)vertices.size();
		bufDesc.pData = vertices.data();
		bufDesc.ppBuffer = &pAsteroidVertexBuffer;
		addResource(&bufDesc, NULL);

		// Index Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(uint16_t) * (uint32_t)indices.size();
		bufDesc.pData = indices.data();
		bufDesc.ppBuffer = &pAsteroidIndexBuffer;
		addResource(&bufDesc, NULL);

		// Static Asteroid RW Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(AsteroidStatic) * gNumAsteroids;
		bufDesc.mDesc.mFirstElement = 0;
		bufDesc.mDesc.mElementCount = gNumAsteroids;
		bufDesc.mDesc.mStructStride = sizeof(AsteroidStatic);
		bufDesc.mDesc.pCounterBuffer = NULL;
		bufDesc.pData = gAsteroidSim.asteroidsStatic.data();
		bufDesc.ppBuffer = &pStaticAsteroidBuffer;
		addResource(&bufDesc, NULL);

		// Dynamic Asteroid RW Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(AsteroidDynamic) * gNumAsteroids;
		bufDesc.mDesc.mFirstElement = 0;
		bufDesc.mDesc.mElementCount = gNumAsteroids;
		bufDesc.mDesc.mStructStride = sizeof(AsteroidDynamic);
		bufDesc.mDesc.pCounterBuffer = NULL;
		bufDesc.pData = gAsteroidSim.asteroidsDynamic.data();
		bufDesc.ppBuffer = &pDynamicAsteroidBuffer;
		addResource(&bufDesc, NULL);

		/* Prepare execute indirect command signatures and buffers */

#if defined(DIRECT3D12)
		IndirectArgumentDescriptor indirectArgDescs[2] = {};
		indirectArgDescs[0].mType = INDIRECT_CONSTANT;    // Root Constant
		indirectArgDescs[0].pName = "rootConstant";
		indirectArgDescs[1].mType = INDIRECT_DRAW_INDEX;    // Indirect Index Draw Arguments
#else
		// Metal and Vulkan doesn't allow constants as part of command signature
		IndirectArgumentDescriptor indirectArgDescs[1] = {};
		indirectArgDescs[0].mType = INDIRECT_DRAW_INDEX;    // Indirect Index Draw Arguments
#endif

		CommandSignatureDesc cmdSignatureDesc = { pIndirectRoot, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), indirectArgDescs };
		addIndirectCommandSignature(pRenderer, &cmdSignatureDesc, &pIndirectCommandSignature);
		addIndirectCommandSignature(pRenderer, &cmdSignatureDesc, &pIndirectSubsetCommandSignature);

		// initialize argument data
		IndirectArguments* indirectInit =
			(IndirectArguments*)tf_calloc(gNumAsteroids, sizeof(IndirectArguments));    // For use with compute shader
		IndirectArguments* indirectSubsetInit =
			(IndirectArguments*)tf_calloc(gNumAsteroidsPerSubset, sizeof(IndirectArguments));    // For use with multithreading subsets
		for (uint32_t i = 0; i < gNumAsteroids; i++)
		{
#if defined(DIRECT3D12)
			indirectInit[i].mDrawID = i;
#endif
			indirectInit[i].mDrawArgs.mInstanceCount = 1;
			indirectInit[i].mDrawArgs.mStartInstance = 0;
			indirectInit[i].mDrawArgs.mStartIndex = 0;
			indirectInit[i].mDrawArgs.mIndexCount = 60;
			indirectInit[i].mDrawArgs.mVertexOffset = 0;
		}
		for (uint32_t i = 0; i < gNumAsteroidsPerSubset; i++)
		{
#if defined(DIRECT3D12)
			indirectSubsetInit[i].mDrawID = i;
#endif
			indirectSubsetInit[i].mDrawArgs.mInstanceCount = 1;
			indirectSubsetInit[i].mDrawArgs.mStartInstance = 0;
			indirectSubsetInit[i].mDrawArgs.mStartIndex = 0;
			indirectSubsetInit[i].mDrawArgs.mIndexCount = 60;
			indirectSubsetInit[i].mDrawArgs.mVertexOffset = 0;
		}

		BufferLoadDesc indirectBufDesc = {};
		indirectBufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
		indirectBufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectBufDesc.mDesc.pCounterBuffer = NULL;
		indirectBufDesc.mDesc.mFirstElement = 0;
		indirectBufDesc.mDesc.mElementCount = gNumAsteroids;
		indirectBufDesc.mDesc.mStructStride = sizeof(IndirectArguments);
		indirectBufDesc.mDesc.mSize = indirectBufDesc.mDesc.mStructStride * indirectBufDesc.mDesc.mElementCount;
		indirectBufDesc.pData = indirectInit;
		indirectBufDesc.ppBuffer = &pIndirectBuffer;
		addResource(&indirectBufDesc, NULL);

		indirectBufDesc.mDesc.mElementCount = gNumAsteroidsPerSubset;
		indirectBufDesc.mDesc.mSize = sizeof(IndirectArguments) * gNumAsteroidsPerSubset;

		indirectBufDesc.pData = indirectSubsetInit;
		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			indirectBufDesc.ppBuffer = &(gAsteroidSubsets[i].pSubsetIndirect);
			addResource(&indirectBufDesc, NULL);
		}

		/* UI and Camera Setup */
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

		GuiDesc guiDesc = {};
		guiDesc.mStartPosition += vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.5f);
		pGui = gAppUI.AddGuiComponent(GetName(), &guiDesc);

        initProfiler();
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		static const char*    enumNames[] = { "Instanced Rendering", "Execute Indirect", "Execute Indirect with GPU Compute", NULL };
		static const uint32_t enumValues[] = { RenderingMode_Instanced, RenderingMode_ExecuteIndirect, RenderingMode_GPUUpdate, 0 };
		/************************************************************************/
		/************************************************************************/
#if !defined(TARGET_IOS)
		pGui->AddWidget(CheckboxWidget("Toggle VSync", &gToggleVSync));
#endif

		pGui->AddWidget(DropdownWidget("Rendering Mode: ", &gRenderingMode, enumNames, enumValues, 3));
		pGui->AddWidget(CheckboxWidget("Multithreaded CPU Update", &gUseThreads));
		pGui->AddWidget(CheckboxWidget("Enable Panini Projection", &gbPaniniEnabled));
		/************************************************************************/
		// Panini props
		/************************************************************************/
#if !defined(TARGET_IOS)
		gPaniniControls.AddWidget(SliderFloatWidget("Camera Horizontal FoV", &gPaniniParams.FoVH, 30.0f, 179.0f, 1.0f));
		gPaniniControls.AddWidget(SliderFloatWidget("Panini D Parameter", &gPaniniParams.D, 0.0f, 1.0f, 0.001f));
		gPaniniControls.AddWidget(SliderFloatWidget("Panini S Parameter", &gPaniniParams.S, 0.0f, 1.0f, 0.001f));
		gPaniniControls.AddWidget(SliderFloatWidget("Screen Scale", &gPaniniParams.scale, 1.0f, 10.0f, 0.01f));
		if (gbPaniniEnabled)
			gPaniniControls.ShowWidgets(pGui);
		else
			gPaniniControls.HideWidgets(pGui);
#endif
		/************************************************************************/
		/************************************************************************/
		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ -121.4f, 69.9f, -562.8f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

#if !defined(TARGET_IOS)
		gPanini.Init(pRenderer);
		gPanini.SetMaxDraws(1);
#endif

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
		typedef bool (*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		waitForAllResourceLoads();
		tf_free(indirectInit);
		tf_free(indirectSubsetInit);
		
		// Prepare descriptor sets
		DescriptorData skyboxParams[6] = {};
		skyboxParams[0].pName = "RightText";
		skyboxParams[0].ppTextures = &pSkyBoxTextures[0];
		skyboxParams[1].pName = "LeftText";
		skyboxParams[1].ppTextures = &pSkyBoxTextures[1];
		skyboxParams[2].pName = "TopText";
		skyboxParams[2].ppTextures = &pSkyBoxTextures[2];
		skyboxParams[3].pName = "BotText";
		skyboxParams[3].ppTextures = &pSkyBoxTextures[3];
		skyboxParams[4].pName = "FrontText";
		skyboxParams[4].ppTextures = &pSkyBoxTextures[4];
		skyboxParams[5].pName = "BackText";
		skyboxParams[5].ppTextures = &pSkyBoxTextures[5];
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 6, skyboxParams);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			skyboxParams[0].pName = "uniformBlock";
			skyboxParams[0].ppBuffers = &pSkyboxUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, skyboxParams);
		}

		// Update dynamic asteroid positions using compute shader
		DescriptorData computeParams[4] = {};
		computeParams[0].pName = "asteroidsStatic";
		computeParams[0].ppBuffers = &pStaticAsteroidBuffer;
		computeParams[1].pName = "asteroidsDynamic";
		computeParams[1].ppBuffers = &pDynamicAsteroidBuffer;
		computeParams[2].pName = "drawCmds";
		computeParams[2].ppBuffers = &pIndirectBuffer;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetCompute[0], 3, computeParams);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			computeParams[0].pName = "uniformBlock";
			computeParams[0].ppBuffers = &pComputeUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetCompute[1], 1, computeParams);
		}

		DescriptorData indirectParams[5] = {};
		indirectParams[0].pName = "asteroidsStatic";
		indirectParams[0].ppBuffers = &pStaticAsteroidBuffer;
		indirectParams[1].pName = "asteroidsDynamic";
		indirectParams[1].ppBuffers = &pDynamicAsteroidBuffer;
		indirectParams[2].pName = "uTex0";
		indirectParams[2].ppTextures = &pAsteroidTex;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetIndirectDraw[0], 3, indirectParams);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			indirectParams[0].pName = "uniformBlock";
			indirectParams[0].ppBuffers = &pIndirectUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetIndirectDraw[1], 1, indirectParams);
		}

		DescriptorData directParams[1] = {};
		directParams[0].pName = "uTex0";
		directParams[0].ppTextures = &pAsteroidTex;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetDirectDraw[0], 1, directParams);
		for (uint32_t i = 0; i < gNumSubsets; ++i)
		{
			directParams[0].pName = "instanceBuffer";
			directParams[0].ppBuffers = &gAsteroidSubsets[i].pAsteroidInstanceBuffer;
			updateDescriptorSet(pRenderer, i, pDescriptorSetDirectDraw[1], 1, directParams);
		}
		
		return true;
	}

	void Exit()
	{
		shutdownThreadSystem(pThreadSystem);
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

#if !defined(TARGET_IOS)
		gPaniniControls.Destroy();
		gPanini.Exit();
#endif

		destroyCameraController(pCameraController);

		gVirtualJoystick.Exit();

		gAppUI.Exit();

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetCompute[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetCompute[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetIndirectDraw[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetIndirectDraw[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetDirectDraw[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetDirectDraw[1]);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pSkyboxUniformBuffer[i]);

		removeResource(pSkyBoxVertexBuffer);
		removeResource(pAsteroidVertexBuffer);
		removeResource(pAsteroidIndexBuffer);
		removeResource(pStaticAsteroidBuffer);
		removeResource(pDynamicAsteroidBuffer);
		removeResource(pIndirectBuffer);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pComputeUniformBuffer[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pIndirectUniformBuffer[i]);
		removeResource(pAsteroidTex);

		for (uint32_t i = 0; i < 6; ++i)
			removeResource(pSkyBoxTextures[i]);


		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			removeResource(gAsteroidSubsets[i].pAsteroidInstanceBuffer);
			tf_free(gAsteroidSubsets[i].mIndirectArgs);
			removeResource(gAsteroidSubsets[i].pSubsetIndirect);
		}

		tf_free(gAsteroidSim.indexOffsets);

		removeIndirectCommandSignature(pRenderer, pIndirectCommandSignature);
		removeIndirectCommandSignature(pRenderer, pIndirectSubsetCommandSignature);

		removeShader(pRenderer, pBasicShader);
		removeShader(pRenderer, pSkyBoxDrawShader);
		removeShader(pRenderer, pIndirectShader);
		removeShader(pRenderer, pComputeShader);

		removePipeline(pRenderer, pComputePipeline);

		removeRootSignature(pRenderer, pBasicRoot);
		removeRootSignature(pRenderer, pSkyBoxRoot);
		removeRootSignature(pRenderer, pIndirectRoot);
		removeRootSignature(pRenderer, pComputeRoot);

		removeSampler(pRenderer, pSkyBoxSampler);
		removeSampler(pRenderer, pBasicSampler);


		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmd(pRenderer, pUICmds[i]);
			removeCmd(pRenderer, pComputeCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
			removeCmdPool(pRenderer, pUICmdPools[i]);
			removeCmdPool(pRenderer, pComputeCmdPools[i]);
		}

		for (uint32_t i = 0; i < gNumSubsets; ++i)
		{
			for (uint32_t j = 0; j < gImageCount; ++j)
			{
				removeCmd(pRenderer, gAsteroidSubsets[i].pCmds[j]);
				removeCmdPool(pRenderer, gAsteroidSubsets[i].pCmdPools[j]);
			}
		}

		removeQueue(pRenderer, pGraphicsQueue);

		exitResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);

		gAsteroidSubsets.set_capacity(0);
		gAsteroidSim.Exit();
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppRenderTargets[0]))
			return false;

		loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = sizeof(vec4);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		RasterizerStateDesc rasterizerStateCullDesc = {};
		rasterizerStateCullDesc.mCullMode = CULL_MODE_BACK;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pRasterizerState = &rasterizerStateCullDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = pBasicRoot;
		pipelineSettings.pShaderProgram = pBasicShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &desc, &pBasicPipeline);

		pipelineSettings.pRootSignature = pIndirectRoot;
		pipelineSettings.pShaderProgram = pIndirectShader;
		addPipeline(pRenderer, &desc, &pIndirectPipeline);

		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings.pBlendState = NULL;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pRootSignature = pSkyBoxRoot;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		addPipeline(pRenderer, &desc, &pSkyBoxDrawPipeline);

		RenderTargetDesc postProcRTDesc = {};
		postProcRTDesc.mArraySize = 1;
		postProcRTDesc.mClearValue = {{0.0f, 0.0f, 0.0f, 0.0f}};
		postProcRTDesc.mDepth = 1;
		postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		postProcRTDesc.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postProcRTDesc.mHeight = mSettings.mHeight;
		postProcRTDesc.mWidth = mSettings.mWidth;
		postProcRTDesc.mSampleCount = SAMPLE_COUNT_1;
		postProcRTDesc.mSampleQuality = 0;
		addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

#if !defined(TARGET_IOS)
		RenderTarget* rts[1];
		rts[0] = pIntermediateRenderTarget;
		bool bSuccess = gPanini.Load(rts);
		gPanini.SetSourceTexture(pIntermediateRenderTarget->pTexture, 0);
#else
        bool bSuccess = true;
#endif

		return bSuccess;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfilerUI();

		gVirtualJoystick.Unload();

		gAppUI.Unload();

		removePipeline(pRenderer, pBasicPipeline);
		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pIndirectPipeline);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pIntermediateRenderTarget);
		removeSwapChain(pRenderer, pSwapChain);

#if !defined(TARGET_IOS)
		gPanini.Unload();
#endif
	}

	float frameTime;

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

#if !defined(TARGET_IOS)
		if (pSwapChain->mEnableVsync != gToggleVSync)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		frameTime = deltaTime;

		pCameraController->update(deltaTime);

		gAppUI.Update(deltaTime);

		static bool paniniEnabled = gbPaniniEnabled;
		if (paniniEnabled != gbPaniniEnabled)
		{
			if (gbPaniniEnabled)
			{
				gPaniniControls.ShowWidgets(pGui);
			}
			else
			{
				gPaniniControls.HideWidgets(pGui);
			}

			paniniEnabled = gbPaniniEnabled;
		}

#if !defined(TARGET_IOS)
		if (gbPaniniEnabled)
		{
			gPanini.SetParams(gPaniniParams);
			gPanini.Update(deltaTime);
		}
#endif
	}

	void Draw()
	{
		// Sync all frames in flight in case there is a change in the render modes
		if (gPreviousRenderingMode != gRenderingMode)
		{
			waitForFences(pRenderer, gImageCount, pRenderCompleteFences);
			gPreviousRenderingMode = gRenderingMode;
		}

		// Prepare images for frame buffers
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pSwapchainRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		RenderTarget* pSceneRenderTarget = gbPaniniEnabled ? pIntermediateRenderTarget : pSwapchainRenderTarget;
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);
		resetCmdPool(pRenderer, pComputeCmdPools[gFrameIndex]);
		resetCmdPool(pRenderer, pUICmdPools[gFrameIndex]);
		for (uint32_t i = 0; i < gNumSubsets; ++i)
		{
			resetCmdPool(pRenderer, gAsteroidSubsets[i].pCmdPools[gFrameIndex]);
		}

		// Update projection view matrices

		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
#if !defined(TARGET_IOS)
		mat4 projMat = mat4::perspective(gPaniniParams.FoVH * (float)PI / 180.0f, aspectInverse, 10000.0f, 0.1f);
#else
        mat4 projMat = mat4::perspective(90.0f * (float)PI / 180.0f, aspectInverse, 10000.0f, 0.1f);
#endif
		mat4 viewProjMat = projMat * viewMat;

		// Load screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = {{0.0f, 0.0f, 0.0f, 0.0f}};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pDepthBuffer->mClearValue;

		eastl::vector<Cmd*> allCmds;

		/************************************************************************/
		// Draw Skybox
		/************************************************************************/
		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		UniformViewProj viewProjUniformData;
		viewProjUniformData.mProjectView = viewProjMat;
		BufferUpdateDesc indirectUniformUpdate = { pIndirectUniformBuffer[gFrameIndex] };
		beginUpdateResource(&indirectUniformUpdate);
		*(UniformViewProj*)indirectUniformUpdate.pMappedData = viewProjUniformData;
		endUpdateResource(&indirectUniformUpdate, NULL);

		viewMat.setTranslation(vec3(0));
		viewProjUniformData.mProjectView = projMat * viewMat;
		BufferUpdateDesc skyboxUniformUpdate = { pSkyboxUniformBuffer[gFrameIndex] };
		beginUpdateResource(&skyboxUniformUpdate);
		*(UniformViewProj*)skyboxUniformUpdate.pMappedData = viewProjUniformData;
		endUpdateResource(&skyboxUniformUpdate, NULL);

		ResourceState sceneRtState = gbPaniniEnabled ? RESOURCE_STATE_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_PRESENT;
		RenderTargetBarrier barrier = { pSceneRenderTarget, sceneRtState, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

		cmdBindRenderTargets(cmd, 1, &pSceneRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneRenderTarget->mWidth, (float)pSceneRenderTarget->mHeight, 1.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSceneRenderTarget->mWidth, pSceneRenderTarget->mHeight);

        cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkybox[1]);

		const uint32_t skyboxStride = sizeof(float) * 4;
        cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, &skyboxStride, NULL);
		cmdDraw(cmd, 36, 0);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneRenderTarget->mWidth, (float)pSceneRenderTarget->mHeight, 0.0f, 1.0f);
		endCmd(cmd);

		allCmds.push_back(cmd);

		/************************************************************************/
		// Draw all asteroids using corresponding settings
		/************************************************************************/
		if (gRenderingMode != RenderingMode_GPUUpdate)
		{
			if (gUseThreads)
			{
				// With Multithreading
				for (uint32_t i = 0; i < gNumSubsets; i++)
				{
					gThreadData[i].mDeltaTime = frameTime;
					gThreadData[i].mIndex = i;
					gThreadData[i].mViewProj = viewProjMat;
					gThreadData[i].pRenderTarget = pSceneRenderTarget;
					gThreadData[i].pDepthBuffer = pDepthBuffer;
					gThreadData[i].mFrameIndex = gFrameIndex;
				}
				addThreadSystemRangeTask(pThreadSystem, &ExecuteIndirect::RenderSubset, gThreadData, gNumSubsets);

				// wait for all threads to finish
				waitThreadSystemIdle(pThreadSystem);

				for (uint32_t i = 0; i < gNumSubsets; i++)
					allCmds.push_back(gAsteroidSubsets[i].pCmds[gFrameIndex]);    // Asteroid Cmds
			}
			else
			{
				//Update Asteroids on CPU
				for (uint32_t i = 0; i < gNumSubsets; i++)
				{
					RenderSubset(i, viewProjMat, gFrameIndex, pSceneRenderTarget, pDepthBuffer, frameTime);
					allCmds.push_back(gAsteroidSubsets[i].pCmds[gFrameIndex]);    // Asteroid Cmds
				}
			}
		}
		else
		{
			// Update uniform data
			UniformCompute computeUniformData;
			computeUniformData.mDeltaTime = frameTime;
			computeUniformData.mStartIndex = 0;
			computeUniformData.mEndIndex = gNumAsteroids;
			computeUniformData.mCamPos = vec4(pCameraController->getViewPosition(), 1.0f);
			computeUniformData.mViewProj = viewProjMat;
			computeUniformData.mNumLODs = gAsteroidSim.numLODs;
#if defined(METAL)    // Andrés: Check mIndexOffset declaration.
			for (uint32_t i = 0; i <= gAsteroidSim.numLODs + 1; i++)
				computeUniformData.mIndexOffsets[i] = gAsteroidSim.indexOffsets[i];
#else
			for (uint32_t i = 0; i <= gAsteroidSim.numLODs; i++)
				computeUniformData.mIndexOffsets[i * 4] = gAsteroidSim.indexOffsets[i];
#endif

			cmd = pComputeCmds[gFrameIndex];
			beginCmd(cmd);

			BufferUpdateDesc computeUniformUpdate = { pComputeUniformBuffer[gFrameIndex] };
			beginUpdateResource(&computeUniformUpdate);
			*(UniformCompute*)computeUniformUpdate.pMappedData = computeUniformData;
			endUpdateResource(&computeUniformUpdate, NULL);

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "GPU Culling");

			// Update dynamic asteroid positions using compute shader
			BufferBarrier uavBarrier = { pIndirectBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 1, &uavBarrier, 0, NULL, 0, NULL);

            cmdBindPipeline(cmd, pComputePipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetCompute[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetCompute[1]);

			cmdDispatch(cmd, uint32_t(ceil(gNumAsteroids / 128.0f)), 1, 1);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Asteroid rendering");

			BufferBarrier srvBarrier = { pIndirectBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT };
			cmdResourceBarrier(cmd, 1, &srvBarrier, 0, NULL, 0, NULL);

			LoadActionsDesc loadActionLoad = {};
			loadActionLoad.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			// Execute indirect
			cmdBindRenderTargets(cmd, 1, &pSceneRenderTarget, pDepthBuffer, &loadActionLoad, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneRenderTarget->mWidth, (float)pSceneRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSceneRenderTarget->mWidth, pSceneRenderTarget->mHeight);

			const uint32_t asteroidStride = sizeof(Vertex);
            cmdBindPipeline(cmd, pIndirectPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetIndirectDraw[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetIndirectDraw[1]);
			cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, &asteroidStride, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, INDEX_TYPE_UINT16, 0);
			cmdExecuteIndirect(cmd, pIndirectCommandSignature, gNumAsteroids, pIndirectBuffer, 0, NULL, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

			endCmd(cmd);
			allCmds.push_back(cmd);
		}

		/************************************************************************/
		// Draw PostProcess & UI
		/************************************************************************/
		cmd = pUICmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw PostProcess & UI");
		LoadActionsDesc* pLoadAction = NULL;

		// we want to clear the render target for Panini post process if its enabled.
		// create the load action here, and assign the pLoadAction pointer later on if necessary.
		LoadActionsDesc swapChainClearAction = {};
		swapChainClearAction.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		swapChainClearAction.mClearColorValues[0] = pSwapchainRenderTarget->mClearValue;

		LoadActionsDesc swapChainLoadAction = {};
		swapChainLoadAction.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		if (gbPaniniEnabled)
		{
			RenderTargetBarrier barriers[] = {
				{ pSwapchainRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
				{ pIntermediateRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);
			pLoadAction = &swapChainClearAction;
		}
		else
		{
			pLoadAction = &swapChainLoadAction;
		}

		cmdBindRenderTargets(cmd, 1, &pSwapchainRenderTarget, NULL, pLoadAction, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pSwapchainRenderTarget->mWidth, (float)pSwapchainRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSwapchainRenderTarget->mWidth, pSwapchainRenderTarget->mHeight);

#if !defined(TARGET_IOS)
		if (gbPaniniEnabled)
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Panini Projection Pass");
			gPanini.Draw(cmd);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			//cmdEndRender(cmd, 1, &pSwapchainRenderTarget, NULL);
		}
#endif
		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		const float txtOffset = 8.f;
		float txtOrigY = txtOffset;
        float2 txtSize = cmdDrawCpuProfile(cmd, float2(txtOffset, txtOrigY), &gFrameTimeDraw);
		txtOrigY += txtSize.y + txtOffset;

#ifndef TARGET_IOS
		gAppUI.Gui(pGui);
#endif

        cmdDrawGpuProfile(cmd, float2(txtOffset, txtOrigY), gGpuProfileToken, &gFrameTimeDraw);

		cmdDrawProfilerUI();
		gAppUI.Draw(cmd);
		cmdEndDebugMarker(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		barrier = { pSwapchainRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
		endCmd(cmd);
		allCmds.push_back(cmd);

		/************************************************************************/
		// Submit commands to graphics queue
		/************************************************************************/
		waitForAllResourceLoads();

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = (uint32_t)allCmds.size();
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = allCmds.data();
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

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "04_ExecuteIndirect"; }

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

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
	/************************************************************************/
	// Asteroid Mesh Creation
	/************************************************************************/
	void ComputeAverageNormals(eastl::vector<Vertex>& vertices, eastl::vector<uint16_t>& indices)
	{
		for (Vertex& vert : vertices)
		{
			vert.mNormal = vec4(0, 0, 0, 0);
		}

		uint32_t numTriangles = (uint32_t)indices.size() / 3;
		for (uint32_t i = 0; i < numTriangles; ++i)
		{
			Vertex& v1 = vertices[indices[i * 3 + 0]];
			Vertex& v2 = vertices[indices[i * 3 + 1]];
			Vertex& v3 = vertices[indices[i * 3 + 2]];

			vec3 u = v2.mPosition.getXYZ() - v1.mPosition.getXYZ();
			vec3 v = v3.mPosition.getXYZ() - v1.mPosition.getXYZ();

			vec4 n = vec4(cross(u, v), 0.0);

			v1.mNormal += n;
			v2.mNormal += n;
			v3.mNormal += n;
		}

		for (Vertex& vert : vertices)
		{
			float length = 1.0f / sqrt(dot(vert.mNormal.getXYZ(), vert.mNormal.getXYZ()));

			vert.mNormal *= length;
		}
	}

	void CreateIcosahedron(eastl::vector<Vertex>& outVertices, eastl::vector<uint16_t>& outIndices)
	{
		static const float a = sqrt(2.0f / (5.0f - sqrt(5.0f)));
		static const float b = sqrt(2.0f / (5.0f + sqrt(5.0f)));

		static const uint32_t numVertices = 12;
		static const Vertex   vertices[numVertices] =    // x, y, z
			{
				{ { -b, a, 0, 1 } }, { { b, a, 0, 1 } }, { { -b, -a, 0, 1 } }, { { b, -a, 0, 1 } },
				{ { 0, -b, a, 1 } }, { { 0, b, a, 1 } }, { { 0, -b, -a, 1 } }, { { 0, b, -a, 1 } },
				{ { a, 0, -b, 1 } }, { { a, 0, b, 1 } }, { { -a, 0, -b, 1 } }, { { -a, 0, b, 1 } },
			};

		static const uint32_t numTriangles = 20;
		static const uint16_t indices[numTriangles * 3] = {
			0, 5, 11, 0, 1, 5, 0, 7, 1, 0, 10, 7, 0, 11, 10, 1, 9, 5, 5, 4,  11, 11, 2,  10, 10, 6, 7, 7, 8, 1,
			3, 4, 9,  3, 2, 4, 3, 6, 2, 3, 8,  6, 3, 9,  8,  4, 5, 9, 2, 11, 4,  6,  10, 2,  8,  7, 6, 9, 1, 8,
		};

		outVertices.clear();
		outVertices.insert(outVertices.end(), vertices, vertices + numVertices);
		outIndices.clear();
		outIndices.insert(outIndices.end(), indices, indices + numTriangles * 3);
	}

	void Subdivide(eastl::vector<Vertex>& outVertices, eastl::vector<uint16_t>& outIndices)
	{
		struct Edge
		{
			Edge(uint16_t _i0, uint16_t _i1): i0(_i0), i1(_i1)
			{
				if (i0 > i1)
				{
					uint16_t temp = i0;
					i0 = i1;
					i1 = temp;
				}
			}

			uint16_t i0, i1;

			bool operator==(const Edge& c) const { return i0 == c.i0 && i1 == c.i1; }

			operator uint32_t() const { return (uint32_t(i0) << 16) | i1; }
		};

		struct GetMidpointIndex
		{
			eastl::unordered_map<Edge, uint16_t>* midPointMap;
			eastl::vector<Vertex>*                outVertices;

			GetMidpointIndex(eastl::unordered_map<Edge, uint16_t>* v1, eastl::vector<Vertex>* v2): midPointMap(v1), outVertices(v2){};

			uint16_t operator()(Edge e)
			{
				eastl::hash_map<Edge, uint16_t>::iterator it = midPointMap->find(e);

				if (it == midPointMap->end())
				{
					Vertex a = (*outVertices)[e.i0];
					Vertex b = (*outVertices)[e.i1];

					Vertex m;
					m.mPosition = vec4((a.mPosition.getXYZ() + b.mPosition.getXYZ()) * 0.5f, 1.0f);

					it = midPointMap->insert(eastl::make_pair(e, uint16_t(outVertices->size()))).first;
					outVertices->push_back(m);
				}

				return it->second;
			}
		};

		eastl::unordered_map<Edge, uint16_t> midPointMap;
		eastl::vector<uint16_t>              newIndices;
		newIndices.reserve((uint32_t)outIndices.size() * 4);
		outVertices.reserve((uint32_t)outVertices.size() * 2);

		GetMidpointIndex getMidpointIndex(&midPointMap, &outVertices);

		uint32_t numTriangles = (uint32_t)outIndices.size() / 3;
		for (uint32_t i = 0; i < numTriangles; ++i)
		{
			uint16_t t0 = outIndices[i * 3 + 0];
			uint16_t t1 = outIndices[i * 3 + 1];
			uint16_t t2 = outIndices[i * 3 + 2];

			uint16_t m0 = getMidpointIndex(Edge(t0, t1));
			uint16_t m1 = getMidpointIndex(Edge(t1, t2));
			uint16_t m2 = getMidpointIndex(Edge(t2, t0));

			uint16_t indices[] = { t0, m0, m2, m0, t1, m1, m0, m1, m2, m2, m1, t2 };

			newIndices.insert(newIndices.end(), indices, indices + 12);
		}

		eastl::vector<uint16_t> temp(outIndices);
		outIndices = newIndices;
		newIndices = temp;
	}

	void Spherify(eastl::vector<Vertex>& out_vertices)
	{
		for (Vertex& vert : out_vertices)
		{
			float l = 1.f / sqrt(dot(vert.mPosition.getXYZ(), vert.mPosition.getXYZ()));
			vert.mPosition = vec4(vert.mPosition.getXYZ() * l, 1.0);
		}
	}

	void CreateGeosphere(
		eastl::vector<Vertex>& outVertices, eastl::vector<uint16_t>& outIndices, unsigned subdivisions, int* indexOffsets)
	{
		CreateIcosahedron(outVertices, outIndices);
		indexOffsets[0] = 0;

		eastl::vector<Vertex>   vertices(outVertices);
		eastl::vector<uint16_t> indices(outIndices);

		unsigned offset = 0;

		for (unsigned i = 0; i < subdivisions; ++i)
		{
			indexOffsets[i + 1] = unsigned((uint32_t)outIndices.size());
			Subdivide(vertices, indices);

			offset = unsigned((uint32_t)outVertices.size());
			outVertices.insert(outVertices.end(), vertices.begin(), vertices.end());

			for (uint16_t idx : indices)
			{
				outIndices.push_back(idx + offset);
			}
		}

		indexOffsets[subdivisions + 1] = unsigned((uint32_t)outIndices.size());

		Spherify(outVertices);
	}

	void CreateAsteroids(
		eastl::vector<Vertex>& vertices, eastl::vector<uint16_t>& indices, unsigned subdivisions, unsigned numMeshes, unsigned rngSeed,
		unsigned& outVerticesPerMesh, int* indexOffsets)
	{
		srand(rngSeed);

		MyRandom rng(rngSeed);

		eastl::vector<Vertex> origVerts;

		CreateGeosphere(origVerts, indices, subdivisions, indexOffsets);

		outVerticesPerMesh = unsigned((uint32_t)origVerts.size());

		float noiseScale = 1.5f;
		float radiusScale = 0.9f;
		float radiusBias = 0.3f;

		// perturb vertices here to create more interesting shapes
		for (unsigned i = 0; i < numMeshes; ++i)
		{
			float randomNoise = rng.GetUniformDistribution(0.f, 10000.f);
			float randomPersistence = rng.GetNormalDistribution(0.95f, 0.04f);

			eastl::vector<Vertex> newVertices(origVerts);
			NoiseOctaves<4>         textureNoise(randomPersistence);
			float                   noise = randomNoise;

			for (Vertex& v : newVertices)
			{
				vec3  posScaled = v.mPosition.getXYZ() * noiseScale;
				float radius = textureNoise(posScaled.getX(), posScaled.getY(), posScaled.getZ(), noise);
				radius = radius * radiusScale + radiusBias;

				v.mPosition = vec4(v.mPosition.getXYZ() * radius, 1.0f);
			}

			ComputeAverageNormals(newVertices, indices);

			vertices.insert(vertices.end(), newVertices.begin(), newVertices.end());
		}
	}

	void CreateTextures(uint32_t texture_count)
	{
		genTextures(texture_count, &pAsteroidTex);
	}

	void CreateSubsets()
	{
		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			Subset subset;

			subset.mIndirectArgs = (IndirectArguments*)tf_calloc(gNumAsteroidsPerSubset, sizeof(IndirectArguments));

			for (uint32_t j = 0; j < gNumAsteroidsPerSubset; j++)
			{
#if defined(DIRECT3D12)
				subset.mIndirectArgs[j].mDrawID = j;
#endif
				subset.mIndirectArgs[j].mDrawArgs.mIndexCount = 60;
				subset.mIndirectArgs[j].mDrawArgs.mStartIndex = 0;
				subset.mIndirectArgs[j].mDrawArgs.mInstanceCount = 0;
				subset.mIndirectArgs[j].mDrawArgs.mStartInstance = 0;
				subset.mIndirectArgs[j].mDrawArgs.mVertexOffset = 0;
			}

			for (uint32_t j = 0; j < gImageCount; ++j)
			{
				CmdPoolDesc cmdPoolDesc = {};
				cmdPoolDesc.pQueue = pGraphicsQueue;
				addCmdPool(pRenderer, &cmdPoolDesc, &subset.pCmdPools[j]);
				CmdDesc cmdDesc = {};
				cmdDesc.pPool = subset.pCmdPools[j];
				addCmd(pRenderer, &cmdDesc, &subset.pCmds[j]);
			}

			gAsteroidSubsets.push_back(subset);
		}
	}
	/************************************************************************/
	// Multi Threading Subset Rendering
	/************************************************************************/
	static bool ShouldCullAsteroid(const vec3& asteroidPos, const vec4 planes[6])
	{
		//based on the values used above this will give a bounding sphere
		static const float radius = 4.5f;    // 2.25f;// 1.7f;

		for (int i = 0; i < 6; ++i)
		{
			float distance = dot(asteroidPos, planes[i].getXYZ()) + planes[i].getW();

			if (distance < -radius)
				return true;
		}

		return false;
	}

	static void RenderSubset(
		unsigned index, const mat4& viewProj, uint32_t frameIdx, RenderTarget* pRenderTarget, RenderTarget* pDepthBuffer, float deltaTime)
	{
        LoadActionsDesc loadActionLoad = {};
        loadActionLoad.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		uint32_t startIdx = index * gNumAsteroidsPerSubset;
		uint32_t endIdx = min(startIdx + gNumAsteroidsPerSubset, gNumAsteroids);

		Subset& subset = gAsteroidSubsets[index];
		Cmd*    cmd = subset.pCmds[frameIdx];

		beginCmd(cmd);

		gAsteroidSim.Update(deltaTime, startIdx, endIdx, pCameraController->getViewPosition());

		vec4 frustumPlanes[6];
		mat4::extractFrustumClipPlanes(
			viewProj, frustumPlanes[0], frustumPlanes[1], frustumPlanes[2], frustumPlanes[3], frustumPlanes[4], frustumPlanes[5], true);

		if (gRenderingMode == RenderingMode_Instanced)
		{
			BufferUpdateDesc uniformUpdate = { gAsteroidSubsets[index].pAsteroidInstanceBuffer };
			beginUpdateResource(&uniformUpdate);
			UniformBasic* instanceData = (UniformBasic*)uniformUpdate.pMappedData;

			// Update asteroids data
			for (uint32_t i = startIdx, localIndex = 0; i < endIdx; i++, ++localIndex)
			{
				const AsteroidStatic&  staticAsteroid = gAsteroidSim.asteroidsStatic[i];
				const AsteroidDynamic& dynamicAsteroid = gAsteroidSim.asteroidsDynamic[i];

				const mat4& transform = dynamicAsteroid.transform;
				if (ShouldCullAsteroid(transform.getTranslation(), frustumPlanes))
					continue;

				mat4 mvp = viewProj * transform;
				mat3 normalMat = inverse(transpose(mat3(transform[0].getXYZ(), transform[1].getXYZ(), transform[2].getXYZ())));

				// Update uniforms
				UniformBasic& uniformData = instanceData[localIndex];
				uniformData.mModelViewProj = mvp;
				uniformData.mNormalMat = mat4(normalMat, vec3(0, 0, 0));
				uniformData.mSurfaceColor = staticAsteroid.surfaceColor;
				uniformData.mDeepColor = staticAsteroid.deepColor;
				uniformData.mTextureID = staticAsteroid.textureID;
			}

			endUpdateResource(&uniformUpdate, NULL);

			// Render all asteroids
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActionLoad, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

            cmdBindPipeline(cmd, pBasicPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetDirectDraw[0]);
			cmdBindDescriptorSet(cmd, index, pDescriptorSetDirectDraw[1]);
            
			const uint32_t asteroidStride = sizeof(Vertex);
			cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, &asteroidStride, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, INDEX_TYPE_UINT16, 0);

			for (uint32_t i = startIdx; i < endIdx; i++)
			{
				const AsteroidDynamic& dynamicAsteroid = gAsteroidSim.asteroidsDynamic[i];

				const mat4& transform = dynamicAsteroid.transform;
				if (ShouldCullAsteroid(transform.getTranslation(), frustumPlanes))
					continue;

				cmdBindPushConstants(cmd, pBasicRoot, "rootConstant", &i);
				cmdDrawIndexed(cmd, dynamicAsteroid.indexCount, dynamicAsteroid.indexStart, 0);
			}
		}
		else if (gRenderingMode == RenderingMode_ExecuteIndirect)
		{
			// Setup indirect draw arguments
			uint32_t numToDraw = 0;

			BufferUpdateDesc indirectBufferUpdate = { subset.pSubsetIndirect };
			beginUpdateResource(&indirectBufferUpdate);
			IndirectArguments*        argData = (IndirectArguments*)indirectBufferUpdate.pMappedData;
			eastl::vector<uint32_t> drawIDs;
			for (uint32_t i = startIdx; i < endIdx; ++i)
			{
				AsteroidStatic  staticAsteroid = gAsteroidSim.asteroidsStatic[i];
				AsteroidDynamic dynamicAsteroid = gAsteroidSim.asteroidsDynamic[i];

				if (ShouldCullAsteroid(dynamicAsteroid.transform.getTranslation(), frustumPlanes))
					continue;

#if defined(DIRECT3D12)
				argData[numToDraw].mDrawID = i;
#endif
				argData[numToDraw].mDrawArgs.mInstanceCount = 1;
				argData[numToDraw].mDrawArgs.mStartInstance = i;
				argData[numToDraw].mDrawArgs.mStartIndex = dynamicAsteroid.indexStart;
				argData[numToDraw].mDrawArgs.mIndexCount = dynamicAsteroid.indexCount;
				argData[numToDraw].mDrawArgs.mVertexOffset = staticAsteroid.vertexStart;
				numToDraw++;

				drawIDs.push_back(i);    // For vulkan and metal
			}

			// Update indirect arguments
			endUpdateResource(&indirectBufferUpdate, NULL);

			BufferUpdateDesc dynamicBufferUpdate = { pDynamicAsteroidBuffer, sizeof(AsteroidDynamic) * startIdx };
			beginUpdateResource(&dynamicBufferUpdate);
			memcpy(dynamicBufferUpdate.pMappedData, gAsteroidSim.asteroidsDynamic.data() + startIdx, sizeof(AsteroidDynamic) * (endIdx - startIdx));
			endUpdateResource(&dynamicBufferUpdate, NULL);

			//// Execute Indirect Draw
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActionLoad, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

            cmdBindPipeline(cmd, pIndirectPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetIndirectDraw[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetIndirectDraw[1]);

			const uint32_t asteroidStride = sizeof(Vertex);
            cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, &asteroidStride, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, INDEX_TYPE_UINT16, 0);
			cmdExecuteIndirect(cmd, pIndirectSubsetCommandSignature, numToDraw, subset.pSubsetIndirect, 0, NULL, 0);
		}

		endCmd(cmd);
	}

	static void RenderSubset(void* pData, uintptr_t i)
	{
		// For multithreading call
		ThreadData* data = ((ThreadData*)pData)+i;
		RenderSubset(data->mIndex, data->mViewProj, data->mFrameIndex, data->pRenderTarget, data->pDepthBuffer, data->mDeltaTime);
	}
};

DEFINE_APPLICATION_MAIN(ExecuteIndirect)
