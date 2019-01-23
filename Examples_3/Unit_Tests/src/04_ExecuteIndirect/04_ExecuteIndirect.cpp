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
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"

//Renderer
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

#if !defined(TARGET_IOS)
//PostProcess
#include "../../../../Middleware_3/PaniniProjection/Panini.h"
#endif

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

#define MAX_LOD_OFFSETS 10

FileSystem gFileSystem;
ThreadPool gThreadSystem;
LogManager gLogManager;
Timer      gAccumTimer;
HiresTimer mFrameTimer;

const char* pszBases[FSR_Count] = {
	"../../../src/04_ExecuteIndirect/",                 // FSR_BinShaders
	"../../../src/04_ExecuteIndirect/",                 // FSR_SrcShaders
	"../../../UnitTestResources/",                      // FSR_Textures
	"../../../UnitTestResources/",                      // FSR_Meshes
	"../../../UnitTestResources/",                      // FSR_Builtin_Fonts
	"../../../src/04_ExecuteIndirect/",                 // FSR_GpuConfig
	"",                                                 // FSR_Animation
	"",                                                 // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",                // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",                  // FSR_MIDDLEWARE_UI
	"../../../../../Middleware_3/PaniniProjection/",    // FSR_MIDDLEWARE_PANINI
};

struct UniformViewProj
{
	mat4 mProjectView;
};

struct UniformCompute
{
	mat4     mViewProj;
	vec4     mCamPos;
	float    mDeltaTime;
	uint32_t mStartIndex;
	uint32_t mEndIndex;
	int32_t  mNumLODs;
#if !defined(METAL)
	int32_t mIndexOffsets[4 * MAX_LOD_OFFSETS + 1];    // Andrés: Do VK/DX samples work with this declaration? Doesn't match rootConstant.
#else
	int32_t                    mIndexOffsets[MAX_LOD_OFFSETS + 1];
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
	uint32_t                   mDrawID;    // Currently setting a root constant only works with Dx
	IndirectDrawIndexArguments mDrawArgs;
	uint32_t                   pad1, pad2;
#elif defined(VULKAN)
	IndirectDrawIndexArguments mDrawArgs;
	uint32_t                   pad1, pad2, pad3;    // This one is just padding
#elif defined(METAL)    // Padding messes up the expected indirect data layout on Metal.
	IndirectDrawIndexArguments mDrawArgs;
#endif
};

struct Subset
{
	CmdPool*           pCmdPool;
	Cmd**              ppCmds;
	Buffer*            pAsteroidInstanceBuffer;
	Buffer*            pSubsetIndirect;
	UniformBasic*      pInstanceData;
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
const uint32_t gNumAsteroids = 50000U;    // 50000 is optimal.
const uint32_t gNumSubsets = 1;           // 4 is optimal. Also equivalent to the number of threads used.
const uint32_t gNumAsteroidsPerSubset = (gNumAsteroids + gNumSubsets - 1) / gNumSubsets;
const uint32_t gTextureCount = 10;

const uint32_t gImageCount = 3;

AsteroidSimulation      gAsteroidSim;
tinystl::vector<Subset> gAsteroidSubsets;
ThreadData              gThreadData[gNumSubsets];
Texture*                pAsteroidTex = NULL;
bool                    gUseThreads = true;
bool                    gToggleVSync = false;
uint32_t                gRenderingMode = RenderingMode_GPUUpdate;
int                     gPreviousRenderingMode = gRenderingMode;

Renderer* pRenderer = NULL;

Queue*      pGraphicsQueue = NULL;
CmdPool*    pCmdPool = NULL;
Cmd**       ppCmds = NULL;
CmdPool*    pComputeCmdPool = NULL;
Cmd**       ppComputeCmds = NULL;
CmdPool*    pUICmdPool = NULL;
Cmd**       ppUICmds = NULL;
DepthState* pDepth = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

// Basic shader variables, used by instanced rendering.
Shader*          pBasicShader = NULL;
Pipeline*        pBasicPipeline = NULL;
RasterizerState* pBasicRast = NULL;
RootSignature*   pBasicRoot = NULL;
Sampler*         pBasicSampler = NULL;

// Execute Indirect variables
Shader*           pIndirectShader = NULL;
Pipeline*         pIndirectPipeline = NULL;
RootSignature*    pIndirectRoot = NULL;
Buffer*           pIndirectBuffer[gImageCount] = {};
Buffer*           pIndirectUniformBuffer[gImageCount] = { NULL };
CommandSignature* pIndirectCommandSignature = NULL;
CommandSignature* pIndirectSubsetCommandSignature = NULL;

// Compute shader variables
Shader*        pComputeShader = NULL;
Pipeline*      pComputePipeline = NULL;
RootSignature* pComputeRoot = NULL;
Buffer*        pComputeUniformBuffer[gImageCount] = {};

// Skybox Variables
Shader*          pSkyBoxDrawShader = NULL;
Pipeline*        pSkyBoxDrawPipeline = NULL;
RasterizerState* pSkyboxRast = NULL;
RootSignature*   pSkyBoxRoot = NULL;
Sampler*         pSkyBoxSampler = NULL;
Buffer*          pSkyboxUniformBuffer[gImageCount] = { NULL };
Buffer*          pSkyBoxVertexBuffer = NULL;
Texture*         pSkyBoxTextures[6];

// Necessary buffers
Buffer* pAsteroidVertexBuffer = NULL;
Buffer* pAsteroidIndexBuffer = NULL;
Buffer* pStaticAsteroidBuffer = NULL;
Buffer* pDynamicAsteroidBuffer = NULL;

// UI
UIApp              gAppUI;
GuiComponent*      pGui;
ICameraController* pCameraController = NULL;
#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

GpuProfiler* pGpuProfiler = NULL;

uint32_t gFrameIndex = 0;

const char* pSkyBoxImageFileNames[] = { "Skybox_right1.png",  "Skybox_left2.png",  "Skybox_top3.png",
										"Skybox_bottom4.png", "Skybox_front5.png", "Skybox_back6.png" };

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
DynamicUIControls gPaniniControls;
RenderTarget*     pIntermediateRenderTarget = NULL;
bool              gbPaniniEnabled = false;
TextDrawDesc      gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class ExecuteIndirect: public IApp
{
	public:
	bool Init()
	{
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

		addCmdPool(pRenderer, pGraphicsQueue, false, &pUICmdPool);
		addCmd_n(pUICmdPool, false, gImageCount, &ppUICmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pComputeCmdPool);
		addCmd_n(pComputeCmdPool, false, gImageCount, &ppComputeCmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pSkyBoxImageFileNames[i];
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			addResource(&textureDesc);
		}

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		CreateTextures(gTextureCount);

		CreateSubsets();

		gThreadSystem.CreateThreads(gNumSubsets);

		ShaderLoadDesc instanceShader = {};
		instanceShader.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		instanceShader.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc indirectShader = {};
		indirectShader.mStages[0] = { "ExecuteIndirect.vert", NULL, 0, FSR_SrcShaders };
		indirectShader.mStages[1] = { "ExecuteIndirect.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc gpuUpdateShader = {};
		gpuUpdateShader.mStages[0] = { "ComputeUpdate.comp", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &instanceShader, &pBasicShader);
		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &indirectShader, &pIndirectShader);
		addShader(pRenderer, &gpuUpdateShader, &pComputeShader);

		RootSignatureDesc basicRootDesc = { &pBasicShader, 1 };
		RootSignatureDesc skyRootDesc = { &pSkyBoxDrawShader, 1 };
		RootSignatureDesc computeRootDesc = { &pComputeShader, 1 };
		RootSignatureDesc indirectRootDesc = { &pIndirectShader, 1 };
		addRootSignature(pRenderer, &basicRootDesc, &pBasicRoot);
		addRootSignature(pRenderer, &skyRootDesc, &pSkyBoxRoot);
		addRootSignature(pRenderer, &computeRootDesc, &pComputeRoot);
		addRootSignature(pRenderer, &indirectRootDesc, &pIndirectRoot);

		/* Setup Pipelines */

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		RasterizerStateDesc rasterizerStateCullDesc = {};
		rasterizerStateCullDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterizerStateCullDesc, &pBasicRast);
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pSkyboxRast);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pBasicSampler);
		addSampler(pRenderer, &samplerDesc, &pSkyBoxSampler);

		ComputePipelineDesc computePipelineDesc = {};
		computePipelineDesc.pShaderProgram = pComputeShader;
		computePipelineDesc.pRootSignature = pComputeRoot;
		addComputePipeline(pRenderer, &computePipelineDesc, &pComputePipeline);

		/* Initialize Asteroid Simulation */

		tinystl::vector<Vertex>   vertices;
		tinystl::vector<uint16_t> indices;
		uint32_t                  numVerticesPerMesh;
		gAsteroidSim.numLODs = 3;
		gAsteroidSim.indexOffsets = (int*)conf_calloc(gAsteroidSim.numLODs + 2, sizeof(int));

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
		bufDesc.mDesc.mVertexStride = sizeof(float) * 4;
		bufDesc.pData = skyBoxPoints;
		bufDesc.ppBuffer = &pSkyBoxVertexBuffer;
		addResource(&bufDesc);

		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufDesc.mDesc.mSize = sizeof(UniformViewProj);
		bufDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			bufDesc.ppBuffer = &pIndirectUniformBuffer[i];
			addResource(&bufDesc);
			bufDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&bufDesc);
		}

		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		bufDesc.mDesc.mFirstElement = 0;
		bufDesc.mDesc.mElementCount = gNumAsteroidsPerSubset;
		bufDesc.mDesc.mStructStride = sizeof(UniformBasic);
		bufDesc.mDesc.mSize = bufDesc.mDesc.mElementCount * bufDesc.mDesc.mStructStride;
		for (int i = 0; i < gNumSubsets; i++)
		{
			gAsteroidSubsets[i].pInstanceData = (UniformBasic*)conf_calloc(gNumAsteroidsPerSubset, sizeof(UniformBasic));
			bufDesc.pData = gAsteroidSubsets[i].pInstanceData;
			bufDesc.ppBuffer = &gAsteroidSubsets[i].pAsteroidInstanceBuffer;
			addResource(&bufDesc);
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
			addResource(&bufDesc);
		}

		// Vertex Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(Vertex) * (uint32_t)vertices.size();
		bufDesc.mDesc.mVertexStride = sizeof(Vertex);
		bufDesc.pData = vertices.data();
		bufDesc.ppBuffer = &pAsteroidVertexBuffer;
		addResource(&bufDesc);

		// Index Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
		bufDesc.mDesc.mSize = sizeof(uint16_t) * (uint32_t)indices.size();
		bufDesc.pData = indices.data();
		bufDesc.ppBuffer = &pAsteroidIndexBuffer;
		addResource(&bufDesc);

		// Static Asteroid RW Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(AsteroidStatic) * gNumAsteroids;
		bufDesc.mDesc.mFirstElement = 0;
		bufDesc.mDesc.mElementCount = gNumAsteroids;
		bufDesc.mDesc.mStructStride = sizeof(AsteroidStatic);
		bufDesc.mDesc.pCounterBuffer = NULL;
		bufDesc.pData = gAsteroidSim.asteroidsStatic.data();
		bufDesc.ppBuffer = &pStaticAsteroidBuffer;
		addResource(&bufDesc);

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
		addResource(&bufDesc);

		/* Prepare execute indirect command signatures and buffers */

#if defined(DIRECT3D12)
		tinystl::vector<IndirectArgumentDescriptor> indirectArgDescs(2);
		indirectArgDescs[0] = {};
		indirectArgDescs[0].mType = INDIRECT_CONSTANT;    // Root Constant
		indirectArgDescs[0].mRootParameterIndex =
			pIndirectRoot->pDxRootConstantRootIndices
				[pIndirectRoot->pDescriptors[pIndirectRoot->pDescriptorNameToIndexMap[tinystl::hash("rootConstant")]].mIndexInParent];
		indirectArgDescs[0].mCount = 1;
		indirectArgDescs[1] = {};
		indirectArgDescs[1].mType = INDIRECT_DRAW_INDEX;    // Indirect Index Draw Arguments
#else
        // Metal and Vulkan doesn't allow constants as part of command signature
        tinystl::vector<IndirectArgumentDescriptor> indirectArgDescs(1);
        indirectArgDescs[0] = {};
        indirectArgDescs[0].mType = INDIRECT_DRAW_INDEX;    // Indirect Index Draw Arguments
#endif

		CommandSignatureDesc cmdSignatureDesc = { pCmdPool, pIndirectRoot, (uint32_t)indirectArgDescs.size(), indirectArgDescs.data() };
		addIndirectCommandSignature(pRenderer, &cmdSignatureDesc, &pIndirectCommandSignature);
		addIndirectCommandSignature(pRenderer, &cmdSignatureDesc, &pIndirectSubsetCommandSignature);

		// initialize argument data
		IndirectArguments* indirectInit =
			(IndirectArguments*)conf_calloc(gNumAsteroids, sizeof(IndirectArguments));    // For use with compute shader
		IndirectArguments* indirectSubsetInit =
			(IndirectArguments*)conf_calloc(gNumAsteroidsPerSubset, sizeof(IndirectArguments));    // For use with multithreading subsets
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
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			indirectBufDesc.ppBuffer = &pIndirectBuffer[i];
			addResource(&indirectBufDesc);
		}
		conf_free(indirectInit);

		indirectBufDesc.mDesc.mElementCount = gNumAsteroidsPerSubset;
		indirectBufDesc.mDesc.mSize = sizeof(IndirectArguments) * gNumAsteroidsPerSubset;

		indirectBufDesc.pData = indirectSubsetInit;
		for (int i = 0; i < gNumSubsets; i++)
		{
			indirectBufDesc.ppBuffer = &(gAsteroidSubsets[i].pSubsetIndirect);
			addResource(&indirectBufDesc);
		}
		conf_free(indirectSubsetInit);

		finishResourceLoading();

		/* UI and Camera Setup */
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
		GuiDesc guiDesc = {};
		pGui = gAppUI.AddGuiComponent(GetName(), &guiDesc);

		static const char*    enumNames[] = { "Instanced Rendering", "Execute Indirect", "Execute Indirect with GPU Compute", NULL };
		static const uint32_t enumValues[] = { RenderingMode_Instanced, RenderingMode_ExecuteIndirect, RenderingMode_GPUUpdate, 0 };
		/************************************************************************/
		/************************************************************************/
#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGui->AddWidget(CheckboxWidget("Toggle VSync", &gToggleVSync));
#endif

		pGui->AddWidget(DropdownWidget("Rendering Mode: ", &gRenderingMode, enumNames, enumValues, 3));
		pGui->AddWidget(CheckboxWidget("Multithreaded CPU Update", &gUseThreads));
		pGui->AddWidget(CheckboxWidget("Enable Panini Projection", &gbPaniniEnabled));
		/************************************************************************/
		// Panini props
		/************************************************************************/
#if !defined(TARGET_IOS)
		gPaniniControls.mDynamicProperties.emplace_back(
			SliderFloatWidget("Camera Horizontal FoV", &gPaniniParams.FoVH, 30.0f, 179.0f, 1.0f).Clone());
		gPaniniControls.mDynamicProperties.emplace_back(
			SliderFloatWidget("Panini D Parameter", &gPaniniParams.D, 0.0f, 1.0f, 0.001f).Clone());
		gPaniniControls.mDynamicProperties.emplace_back(
			SliderFloatWidget("Panini S Parameter", &gPaniniParams.S, 0.0f, 1.0f, 0.001f).Clone());
		gPaniniControls.mDynamicProperties.emplace_back(
			SliderFloatWidget("Screen Scale", &gPaniniParams.scale, 1.0f, 10.0f, 0.01f).Clone());
		if (gbPaniniEnabled)
			gPaniniControls.ShowDynamicProperties(pGui);
		else
			gPaniniControls.HideDynamicProperties(pGui);
#endif
		/************************************************************************/
		/************************************************************************/
		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ -121.4f, 69.9f, -562.8f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		requestMouseCapture(true);

		pCameraController->setMotionParameters(cmp);

		InputSystem::RegisterInputEvent(cameraInputEvent);

#if !defined(TARGET_IOS)
		gPanini.Init(pRenderer);
#endif

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount], true);

#if !defined(TARGET_IOS)
		gPaniniControls.Destroy();
		gPanini.Exit();
#endif

		destroyCameraController(pCameraController);

		removeDebugRendererInterface();

		gAppUI.Exit();

		removeGpuProfiler(pRenderer, pGpuProfiler);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pSkyboxUniformBuffer[i]);

		removeResource(pSkyBoxVertexBuffer);
		removeResource(pAsteroidVertexBuffer);
		removeResource(pAsteroidIndexBuffer);
		removeResource(pStaticAsteroidBuffer);
		removeResource(pDynamicAsteroidBuffer);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pIndirectBuffer[i]);
			removeResource(pComputeUniformBuffer[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pIndirectUniformBuffer[i]);
		removeResource(pAsteroidTex);

		for (uint32_t i = 0; i < 6; ++i)
			removeResource(pSkyBoxTextures[i]);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			removeResource(gAsteroidSubsets[i].pAsteroidInstanceBuffer);
			conf_free(gAsteroidSubsets[i].pInstanceData);
			conf_free(gAsteroidSubsets[i].mIndirectArgs);
			removeResource(gAsteroidSubsets[i].pSubsetIndirect);
		}

		conf_free(gAsteroidSim.indexOffsets);

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

		removeDepthState(pDepth);
		removeSampler(pRenderer, pSkyBoxSampler);
		removeSampler(pRenderer, pBasicSampler);
		removeRasterizerState(pSkyboxRast);
		removeRasterizerState(pBasicRast);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
		removeCmd_n(pComputeCmdPool, gImageCount, ppComputeCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeCmdPool(pRenderer, pUICmdPool);
		removeCmdPool(pRenderer, pComputeCmdPool);

		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			removeCmd_n(gAsteroidSubsets[i].pCmdPool, gImageCount, gAsteroidSubsets[i].ppCmds);
			removeCmdPool(pRenderer, gAsteroidSubsets[i].pCmdPool);
		}

		removeQueue(pGraphicsQueue);

		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], ImageFormat::Enum::NONE))
			return false;
#endif

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = sizeof(vec4);

		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pRasterizerState = pBasicRast;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pBasicRoot;
		pipelineSettings.pShaderProgram = pBasicShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &pipelineSettings, &pBasicPipeline);

		pipelineSettings.pRootSignature = pIndirectRoot;
		pipelineSettings.pShaderProgram = pIndirectShader;
		addPipeline(pRenderer, &pipelineSettings, &pIndirectPipeline);

		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings.pBlendState = NULL;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = pSkyboxRast;
		pipelineSettings.pRootSignature = pSkyBoxRoot;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		addPipeline(pRenderer, &pipelineSettings, &pSkyBoxDrawPipeline);

#if defined(VULKAN)
		transitionRenderTargets();
#endif

		RenderTargetDesc postProcRTDesc = {};
		postProcRTDesc.mArraySize = 1;
		postProcRTDesc.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		postProcRTDesc.mDepth = 1;
		postProcRTDesc.mFormat = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		postProcRTDesc.mHeight = mSettings.mHeight;
		postProcRTDesc.mWidth = mSettings.mWidth;
		postProcRTDesc.mSampleCount = SAMPLE_COUNT_1;
		postProcRTDesc.mSampleQuality = 0;
		addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

#if !defined(TARGET_IOS)
		RenderTarget* rts[1];
		rts[0] = pIntermediateRenderTarget;
		bool bSuccess = gPanini.Load(rts);
		gPanini.SetSourceTexture(pIntermediateRenderTarget->pTexture);
#else
        bool bSuccess = true;
#endif

		return bSuccess;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount], true);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pBasicPipeline);
		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pIndirectPipeline);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pIntermediateRenderTarget);

#if !defined(TARGET_IOS)
		gPanini.Unload();
#endif
	}

	float frameTime;

	void Update(float deltaTime)
	{
#if !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
		{
			waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		frameTime = deltaTime;

		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(170.0f);
		}

		if (getKeyUp(KEY_LEFT_TRIGGER))
		{
			gRenderingMode = (++gRenderingMode) % RenderingMode_Count;
		}

		pCameraController->update(deltaTime);
		gAppUI.Update(deltaTime);

		static bool paniniEnabled = gbPaniniEnabled;
		if (paniniEnabled != gbPaniniEnabled)
		{
			if (gbPaniniEnabled)
			{
				gPaniniControls.ShowDynamicProperties(pGui);
			}
			else
			{
				gPaniniControls.HideDynamicProperties(pGui);
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
			waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, false);
			gPreviousRenderingMode = gRenderingMode;
		}

		// Prepare images for frame buffers
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pSwapchainRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		RenderTarget* pSceneRenderTarget = gbPaniniEnabled ? pIntermediateRenderTarget : pSwapchainRenderTarget;
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

		uint32_t frameIdx = gFrameIndex;

		// Update projection view matrices

		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
#if !defined(TARGET_IOS)
		mat4 projMat = mat4::perspective(gPaniniParams.FoVH * (float)PI / 180.0f, aspectInverse, 0.1f, 10000.0f);
#else
        mat4 projMat = mat4::perspective(90.0f * (float)PI / 180.0f, aspectInverse, 0.1f, 10000.0f);
#endif
		mat4 viewProjMat = projMat * viewMat;

		// Load screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0 };

		tinystl::vector<Cmd*> allCmds;

		/************************************************************************/
		// Draw Skybox
		/************************************************************************/
		Cmd* cmd = ppCmds[frameIdx];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		UniformViewProj viewProjUniformData;
		viewProjUniformData.mProjectView = viewProjMat;
		BufferUpdateDesc indirectUniformUpdate = { pIndirectUniformBuffer[gFrameIndex], &viewProjUniformData };
		updateResource(&indirectUniformUpdate);

		viewMat.setTranslation(vec3(0));
		viewProjUniformData.mProjectView = projMat * viewMat;
		BufferUpdateDesc skyboxUniformUpdate = { pSkyboxUniformBuffer[gFrameIndex], &viewProjUniformData };
		updateResource(&skyboxUniformUpdate);

		TextureBarrier barrier = { pSceneRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		cmdBindRenderTargets(cmd, 1, &pSceneRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneRenderTarget->mDesc.mWidth, (float)pSceneRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSceneRenderTarget->mDesc.mWidth, pSceneRenderTarget->mDesc.mHeight);

		DescriptorData skyboxParams[8] = {};
		skyboxParams[0].pName = "uniformBlock";
		skyboxParams[0].ppBuffers = &pSkyboxUniformBuffer[gFrameIndex];
		skyboxParams[1].pName = "RightText";
		skyboxParams[1].ppTextures = &pSkyBoxTextures[0];
		skyboxParams[2].pName = "LeftText";
		skyboxParams[2].ppTextures = &pSkyBoxTextures[1];
		skyboxParams[3].pName = "TopText";
		skyboxParams[3].ppTextures = &pSkyBoxTextures[2];
		skyboxParams[4].pName = "BotText";
		skyboxParams[4].ppTextures = &pSkyBoxTextures[3];
		skyboxParams[5].pName = "FrontText";
		skyboxParams[5].ppTextures = &pSkyBoxTextures[4];
		skyboxParams[6].pName = "BackText";
		skyboxParams[6].ppTextures = &pSkyBoxTextures[5];
		skyboxParams[7].pName = "uSampler0";
		skyboxParams[7].ppSamplers = &pSkyBoxSampler;
		cmdBindDescriptors(cmd, pSkyBoxRoot, 8, skyboxParams);
		cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
		cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, NULL);
		cmdDraw(cmd, 36, 0);

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
				WorkItem pWorkGroups[gNumSubsets];

				for (int i = 0; i < gNumSubsets; i++)
				{
					gThreadData[i].mDeltaTime = frameTime;
					gThreadData[i].mIndex = i;
					gThreadData[i].mViewProj = viewProjMat;
					gThreadData[i].pRenderTarget = pSceneRenderTarget;
					gThreadData[i].pDepthBuffer = pDepthBuffer;
					gThreadData[i].mFrameIndex = frameIdx;
					pWorkGroups[i].pData = &gThreadData[i];
					pWorkGroups[i].pFunc = RenderSubset;
					gThreadSystem.AddWorkItem(&pWorkGroups[i]);
				}

				// wait for all threads to finish
				gThreadSystem.Complete(0);

				for (int i = 0; i < gNumSubsets; i++)
					allCmds.push_back(gAsteroidSubsets[i].ppCmds[frameIdx]);    // Asteroid Cmds
			}
			else
			{
				//Update Asteroids on CPU
				for (uint32_t i = 0; i < gNumSubsets; i++)
				{
					RenderSubset(i, viewProjMat, frameIdx, pSceneRenderTarget, pDepthBuffer, frameTime);
					allCmds.push_back(gAsteroidSubsets[i].ppCmds[frameIdx]);    // Asteroid Cmds
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
#if !defined(METAL)    // Andrés: Check mIndexOffset declaration.
			for (uint32_t i = 0; i <= gAsteroidSim.numLODs; i++)
				computeUniformData.mIndexOffsets[i * 4] = gAsteroidSim.indexOffsets[i];
#else
            for (uint32_t i = 0; i <= gAsteroidSim.numLODs + 1; i++)
                computeUniformData.mIndexOffsets[i] = gAsteroidSim.indexOffsets[i];
#endif

			cmd = ppComputeCmds[frameIdx];
			beginCmd(cmd);

			BufferUpdateDesc computeUniformUpdate = { pComputeUniformBuffer[gFrameIndex], &computeUniformData };
			updateResource(&computeUniformUpdate);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "GPU Culling");

			// Update dynamic asteroid positions using compute shader
			DescriptorData computeParams[4] = {};
			computeParams[0].pName = "uniformBlock";
			computeParams[0].ppBuffers = &pComputeUniformBuffer[frameIdx];
			computeParams[1].pName = "asteroidsStatic";
			computeParams[1].ppBuffers = &pStaticAsteroidBuffer;
			computeParams[2].pName = "asteroidsDynamic";
			computeParams[2].ppBuffers = &pDynamicAsteroidBuffer;
			computeParams[3].pName = "drawCmds";
			computeParams[3].ppBuffers = &pIndirectBuffer[frameIdx];
			cmdBindDescriptors(cmd, pComputeRoot, 4, computeParams);

			BufferBarrier uavBarrier = { pIndirectBuffer[gFrameIndex], RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 1, &uavBarrier, 0, NULL, false);

			cmdBindPipeline(cmd, pComputePipeline);
			cmdDispatch(cmd, uint32_t(ceil(gNumAsteroids / 128.0f)), 1, 1);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Asteroid rendering");

			BufferBarrier srvBarrier = { pIndirectBuffer[gFrameIndex], RESOURCE_STATE_INDIRECT_ARGUMENT };
			cmdResourceBarrier(cmd, 1, &srvBarrier, 0, NULL, false);

			// Execute indirect
			cmdBindRenderTargets(cmd, 1, &pSceneRenderTarget, pDepthBuffer, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneRenderTarget->mDesc.mWidth, (float)pSceneRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSceneRenderTarget->mDesc.mWidth, pSceneRenderTarget->mDesc.mHeight);

			DescriptorData indirectParams[5] = {};
			indirectParams[0].pName = "uniformBlock";
			indirectParams[0].ppBuffers = &pIndirectUniformBuffer[gFrameIndex];
			indirectParams[1].pName = "asteroidsStatic";
			indirectParams[1].ppBuffers = &pStaticAsteroidBuffer;
			indirectParams[2].pName = "asteroidsDynamic";
			indirectParams[2].ppBuffers = &pDynamicAsteroidBuffer;
			indirectParams[3].pName = "uTex0";
			indirectParams[3].ppTextures = &pAsteroidTex;
			indirectParams[4].pName = "uSampler0";
			indirectParams[4].ppSamplers = &pBasicSampler;
			cmdBindDescriptors(cmd, pIndirectRoot, 5, indirectParams);

			cmdBindPipeline(cmd, pIndirectPipeline);
			cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, 0);
			cmdExecuteIndirect(cmd, pIndirectCommandSignature, gNumAsteroids, pIndirectBuffer[gFrameIndex], 0, NULL, 0);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

			endCmd(cmd);
			allCmds.push_back(cmd);
		}

		/************************************************************************/
		// Draw PostProcess & UI
		/************************************************************************/
		cmd = ppUICmds[frameIdx];
		beginCmd(cmd);

		LoadActionsDesc* pLoadAction = NULL;

		// we want to clear the render target for Panini post process if its enabled.
		// create the load action here, and assign the pLoadAction pointer later on if necessary.
		LoadActionsDesc swapChainClearAction = {};
		swapChainClearAction.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		swapChainClearAction.mClearColorValues[0] = pSwapchainRenderTarget->mDesc.mClearValue;

		if (gbPaniniEnabled)
		{
			TextureBarrier barriers[] = {
				{ pIntermediateRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pSwapchainRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, 2, &barriers[0], true);
			cmdFlushBarriers(cmd);

			pLoadAction = &swapChainClearAction;
		}

		cmdBindRenderTargets(cmd, 1, &pSwapchainRenderTarget, NULL, pLoadAction, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pSwapchainRenderTarget->mDesc.mWidth, (float)pSwapchainRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSwapchainRenderTarget->mDesc.mWidth, pSwapchainRenderTarget->mDesc.mHeight);

#if !defined(TARGET_IOS)
		if (gbPaniniEnabled)
		{
			gPanini.SetSourceTexture(pIntermediateRenderTarget->pTexture);
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Panini Projection Pass");
			gPanini.Draw(cmd);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
			//cmdEndRender(cmd, 1, &pSwapchainRenderTarget, NULL);
		}
#endif
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);

		static HiresTimer timer;
		timer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", timer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
#ifndef METAL
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
#endif

		char buff[256] = "";
		char modeStr[128] = "Instanced";
		if (gRenderingMode == RenderingMode_ExecuteIndirect)
			strcpy(modeStr, "ExecuteIndirect");
		if (gRenderingMode == RenderingMode_GPUUpdate)
			strcpy(modeStr, "GPU update");

		sprintf(buff, "SPACE - Rendering mode - %s", modeStr);
		drawDebugText(cmd, 8, 65, buff, NULL);

#ifndef TARGET_IOS
		drawDebugText(cmd, 8, 80, "F1 - Toggle UI", NULL);
		gAppUI.Gui(pGui);
#endif

#ifndef METAL
		drawDebugGpuProfile(cmd, 8, 110, pGpuProfiler, NULL);
#endif

		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		barrier = { pSwapchainRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		endCmd(cmd);
		allCmds.push_back(cmd);

		/************************************************************************/
		// Submit commands to graphics queue
		/************************************************************************/
		queueSubmit(
			pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
			&pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "04_ExecuteIndirect"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { 1.0f, 0 };
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	void transitionRenderTargets()
	{
		// Transition render targets to desired state
		const uint32_t numBarriers = gImageCount + 1;
		TextureBarrier rtBarriers[numBarriers] = {};
		for (uint32_t i = 0; i < gImageCount; ++i)
			rtBarriers[i] = { pSwapChain->ppSwapchainRenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
		rtBarriers[numBarriers - 1] = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
		beginCmd(ppCmds[0]);
		cmdResourceBarrier(ppCmds[0], 0, 0, numBarriers, rtBarriers, false);
		endCmd(ppCmds[0]);
		queueSubmit(pGraphicsQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0], false);
	}
	/************************************************************************/
	// Camera
	/************************************************************************/
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
	/************************************************************************/
	// Asteroid Mesh Creation
	/************************************************************************/
	void ComputeAverageNormals(tinystl::vector<Vertex>& vertices, tinystl::vector<uint16_t>& indices)
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
			float length = 1.f / sqrt(dot(vert.mNormal.getXYZ(), vert.mNormal.getXYZ()));

			vert.mNormal *= length;
		}
	}

	void CreateIcosahedron(tinystl::vector<Vertex>& outVertices, tinystl::vector<uint16_t>& outIndices)
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

	void Subdivide(tinystl::vector<Vertex>& outVertices, tinystl::vector<uint16_t>& outIndices)
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
			tinystl::unordered_map<Edge, uint16_t>* midPointMap;
			tinystl::vector<Vertex>*                outVertices;

			GetMidpointIndex(tinystl::unordered_map<Edge, uint16_t>* v1, tinystl::vector<Vertex>* v2): midPointMap(v1), outVertices(v2){};

			uint16_t operator()(Edge e)
			{
				tinystl::unordered_hash_iterator<tinystl::unordered_hash_node<Edge, uint16_t>> it;
				it = midPointMap->find(e);

				if (it == midPointMap->end())
				{
					Vertex a = (*outVertices)[e.i0];
					Vertex b = (*outVertices)[e.i1];

					Vertex m;
					m.mPosition = vec4((a.mPosition.getXYZ() + b.mPosition.getXYZ()) * 0.5f, 1.0f);

					it = midPointMap->insert(tinystl::make_pair(e, uint16_t(outVertices->size()))).first;
					outVertices->push_back(m);
				}

				return it->second;
			}
		};

		tinystl::unordered_map<Edge, uint16_t> midPointMap;
		tinystl::vector<uint16_t>              newIndices;
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

		tinystl::vector<uint16_t> temp(outIndices);
		outIndices = newIndices;
		newIndices = temp;
	}

	void Spherify(tinystl::vector<Vertex>& out_vertices)
	{
		for (Vertex& vert : out_vertices)
		{
			float l = 1.f / sqrt(dot(vert.mPosition.getXYZ(), vert.mPosition.getXYZ()));
			vert.mPosition = vec4(vert.mPosition.getXYZ() * l, 1.0);
		}
	}

	void CreateGeosphere(
		tinystl::vector<Vertex>& outVertices, tinystl::vector<uint16_t>& outIndices, unsigned subdivisions, int* indexOffsets)
	{
		CreateIcosahedron(outVertices, outIndices);
		indexOffsets[0] = 0;

		tinystl::vector<Vertex>   vertices(outVertices);
		tinystl::vector<uint16_t> indices(outIndices);

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
		tinystl::vector<Vertex>& vertices, tinystl::vector<uint16_t>& indices, unsigned subdivisions, unsigned numMeshes, unsigned rngSeed,
		unsigned& outVerticesPerMesh, int* indexOffsets)
	{
		srand(rngSeed);

		MyRandom rng(rngSeed);

		tinystl::vector<Vertex> origVerts;

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

			tinystl::vector<Vertex> newVertices(origVerts);
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
		Image image;
		genTextures(texture_count, &image);

		TextureLoadDesc textureDesc = {};
		textureDesc.pImage = &image;
		textureDesc.ppTexture = &pAsteroidTex;
		addResource(&textureDesc);

		image.Destroy();
	}

	void CreateSubsets()
	{
		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			int idxOffset = gNumAsteroidsPerSubset * i;

			Subset subset;

			subset.mIndirectArgs = (IndirectArguments*)conf_calloc(gNumAsteroidsPerSubset, sizeof(IndirectArguments));

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

			addCmdPool(pRenderer, pGraphicsQueue, false, &subset.pCmdPool);
			addCmd_n(subset.pCmdPool, false, gImageCount, &subset.ppCmds);

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
		uint32_t startIdx = index * gNumAsteroidsPerSubset;
		uint32_t endIdx = min(startIdx + gNumAsteroidsPerSubset, gNumAsteroids);

		Subset& subset = gAsteroidSubsets[index];
		Cmd*    cmd = subset.ppCmds[frameIdx];

		beginCmd(cmd);

		gAsteroidSim.update(deltaTime, startIdx, endIdx, pCameraController->getViewPosition());

		vec4 frustumPlanes[6];
		mat4::extractFrustumClipPlanes(
			viewProj, frustumPlanes[0], frustumPlanes[1], frustumPlanes[2], frustumPlanes[3], frustumPlanes[4], frustumPlanes[5], true);

		if (gRenderingMode == RenderingMode_Instanced)
		{
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
				UniformBasic& uniformData = gAsteroidSubsets[index].pInstanceData[localIndex];
				uniformData.mModelViewProj = mvp;
				uniformData.mNormalMat = mat4(normalMat, vec3(0, 0, 0));
				uniformData.mSurfaceColor = staticAsteroid.surfaceColor;
				uniformData.mDeepColor = staticAsteroid.deepColor;
				uniformData.mTextureID = staticAsteroid.textureID;
			}

			BufferUpdateDesc uniformUpdate = { gAsteroidSubsets[index].pAsteroidInstanceBuffer, gAsteroidSubsets[index].pInstanceData };
			updateResource(&uniformUpdate);

			// Render all asteroids
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			DescriptorData params[3] = {};
			params[0].pName = "instanceBuffer";
			params[0].ppBuffers = &gAsteroidSubsets[index].pAsteroidInstanceBuffer;
			params[1].pName = "uTex0";
			params[1].ppTextures = &pAsteroidTex;
			params[2].pName = "uSampler0";
			params[2].ppSamplers = &pBasicSampler;
			cmdBindDescriptors(cmd, pBasicRoot, 3, params);
			cmdBindPipeline(cmd, pBasicPipeline);
			cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, 0);

			for (uint32_t i = startIdx; i < endIdx; i++)
			{
				const AsteroidDynamic& dynamicAsteroid = gAsteroidSim.asteroidsDynamic[i];

				const mat4& transform = dynamicAsteroid.transform;
				if (ShouldCullAsteroid(transform.getTranslation(), frustumPlanes))
					continue;

				DescriptorData rootConst = {};
				rootConst.pName = "rootConstant";
				rootConst.pRootConstant = &i;
				cmdBindDescriptors(cmd, pBasicRoot, 1, &rootConst);
				cmdDrawIndexed(cmd, dynamicAsteroid.indexCount, dynamicAsteroid.indexStart, 0);
			}
		}
		else if (gRenderingMode == RenderingMode_ExecuteIndirect)
		{
			// Setup indirect draw arguments
			uint32_t numToDraw = 0;

			IndirectArguments*        argData = (IndirectArguments*)conf_calloc(gNumAsteroidsPerSubset, sizeof(IndirectArguments));
			tinystl::vector<uint32_t> drawIDs;
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

			BufferUpdateDesc dynamicBufferUpdate;
			dynamicBufferUpdate.pBuffer = pDynamicAsteroidBuffer;
			dynamicBufferUpdate.pData = gAsteroidSim.asteroidsDynamic.data();
			dynamicBufferUpdate.mSize = sizeof(AsteroidDynamic) * (endIdx - startIdx);
			dynamicBufferUpdate.mSrcOffset = sizeof(AsteroidDynamic) * startIdx;
			dynamicBufferUpdate.mDstOffset = sizeof(AsteroidDynamic) * startIdx;
			BufferUpdateDesc indirectBufferUpdate = { subset.pSubsetIndirect, argData };

			updateResource(&dynamicBufferUpdate);

			// Update indirect arguments
			BufferBarrier barrier = { subset.pSubsetIndirect, RESOURCE_STATE_COPY_DEST };
			cmdResourceBarrier(cmd, 1, &barrier, 0, NULL, false);
			updateResource(&indirectBufferUpdate);
			barrier = { subset.pSubsetIndirect, RESOURCE_STATE_INDIRECT_ARGUMENT };
			cmdResourceBarrier(cmd, 1, &barrier, 0, NULL, false);

			//// Execute Indirect Draw
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			DescriptorData indirectParams[5] = {};
			indirectParams[0].pName = "uniformBlock";
			indirectParams[0].ppBuffers = &pIndirectUniformBuffer[gFrameIndex];
			indirectParams[1].pName = "asteroidsStatic";
			indirectParams[1].ppBuffers = &pStaticAsteroidBuffer;
			indirectParams[2].pName = "asteroidsDynamic";
			indirectParams[2].ppBuffers = &pDynamicAsteroidBuffer;
			indirectParams[3].pName = "uTex0";
			indirectParams[3].ppTextures = &pAsteroidTex;
			indirectParams[4].pName = "uSampler0";
			indirectParams[4].ppSamplers = &pBasicSampler;
			cmdBindDescriptors(cmd, pIndirectRoot, 5, indirectParams);
			cmdBindPipeline(cmd, pIndirectPipeline);
			cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, 0);
			cmdExecuteIndirect(cmd, pIndirectSubsetCommandSignature, numToDraw, subset.pSubsetIndirect, 0, NULL, 0);
			conf_free(argData);
		}

		endCmd(cmd);
	}

	static void RenderSubset(void* pData)
	{
		// For multithreading call
		ThreadData* data = (ThreadData*)pData;
		RenderSubset(data->mIndex, data->mViewProj, data->mFrameIndex, data->pRenderTarget, data->pDepthBuffer, data->mDeltaTime);
	}

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(ExecuteIndirect)
