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

// Unit test headers
#include "AsteroidSim.h"
#include "TextureGen.h"
#include "NoiseOctaves.h"
#include "Random.h"

//TinySTL
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_map.h"

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

//Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#if !defined(TARGET_IOS)
//PostProcess
#include "../../../../Middleware_3/PaniniProjection/Panini.h"
#endif

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

const uint32_t gImageCount = 3;

#define MAX_LOD_OFFSETS 10


ThreadSystem* pThreadSystem;

struct UniformViewProj
{
	CameraMatrix mProjectView;
};

struct UniformCompute
{
    CameraMatrix     mViewProj;
	vec4     mCamPos;
	float    mDeltaTime;
	uint     mStartIndex;
	uint     mEndIndex;
	uint     mNumLODs;
#if defined(METAL) || defined(ORBIS) || defined(PROSPERO)
	uint     mIndexOffsets[MAX_LOD_OFFSETS + 1];
#else
	uint     mIndexOffsets[4 * MAX_LOD_OFFSETS + 1];    // Andrés: Do VK/DX samples work with this declaration? Doesn't match rootConstant.
#endif
};

struct UniformBasic
{
    CameraMatrix     mModelViewProj;
	mat4     mNormalMat;
	float4   mSurfaceColor;
	float4   mDeepColor;
	int32_t  mTextureID;
	uint32_t _pad0[3];
};

struct Subset
{
	CmdPool*           pCmdPools[gImageCount];
	Cmd*               pCmds[gImageCount];
	Buffer*            pAsteroidInstanceBuffer[gImageCount];
	Buffer*            pSubsetIndirect[gImageCount];
};

struct ThreadData
{
	CameraMatrix  mViewProj;
	RenderTarget* pRenderTarget;
	RenderTarget* pDepthBuffer;
	uint32_t      mIndex;
	uint32_t      mFrameIndex;
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
uint32_t          gDrawIdRootConstantIndex = 0;
Sampler*          pBasicSampler = NULL;

// Execute Indirect variables
Shader*           pIndirectShader = NULL;
Pipeline*         pIndirectPipeline = NULL;
RootSignature*    pIndirectRoot = NULL;
Buffer*           pIndirectBuffer = NULL;
Buffer*           pIndirectUniformBuffer[gImageCount] = { NULL };
CommandSignature* pIndirectCommandSignature = NULL;

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
DescriptorSet*    pDescriptorSetIndirectDrawForCompute = NULL;
DescriptorSet*    pDescriptorSetDirectDraw[2] = { NULL };

// Necessary buffers
Buffer* pAsteroidVertexBuffer = NULL;
Buffer* pAsteroidIndexBuffer = NULL;
Buffer* pStaticAsteroidBuffer = NULL;
Buffer* pDynamicAsteroidBuffer[gImageCount] = {};

// UI
UIComponent*      pGui;
ICameraController* pCameraController = NULL;

uint32_t gFrameIndex = 0;

uint32_t gFontID = 0; 

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

FontDrawDesc gFrameTimeDraw;

const char* gTestScripts[] = { "Test_Instanced.lua", "Test_ExecuteIndirect.lua", "Test_GPUupdate.lua" };
uint32_t gCurrentScriptIndex = 0;

// Asteroid simulation
eastl::vector<Vertex>   gVertices;
eastl::vector<uint16_t> gIndices;
uint32_t                  numVerticesPerMesh;

void RunScript(void* pUserData)
{
	LuaScriptDesc runDesc = {};
	runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
	luaQueueScriptToRun(&runDesc);
}

class ExecuteIndirect: public IApp
{
	public:
	ExecuteIndirect()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init()
	{
        // FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,	"Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES,	"CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,		"GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,			"Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,			"Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,			"Scripts");
		
		initThreadSystem(&pThreadSystem);

		/* Initialize Asteroid Simulation */
		gAsteroidSim.numLODs = 3;
		gAsteroidSim.indexOffsets = (int*)tf_calloc(gAsteroidSim.numLODs + 2, sizeof(int));

		CreateAsteroids(gVertices, gIndices, gAsteroidSim.numLODs, 1000, 123, numVerticesPerMesh, gAsteroidSim.indexOffsets);
		gAsteroidSim.Init(123, gNumAsteroids, 1000, numVerticesPerMesh, gTextureCount);

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
			// Textures representing color should be stored in SRGB or HDR format
			textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
			addResource(&textureDesc, NULL);
		}

		CreateTextures(gTextureCount);

		CreateSubsets();

		SamplerDesc samplerDesc = { FILTER_LINEAR,
							FILTER_LINEAR,
							MIPMAP_MODE_NEAREST,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pBasicSampler);
		addSampler(pRenderer, &samplerDesc, &pSkyBoxSampler);

		/* Prepare buffers */

		// initialize argument data
		uint32_t drawArgOffset = pRenderer->pActiveGpuSettings->mIndirectRootConstant ? 1 : 0;
		uint32_t indirectArgElementCount = drawArgOffset + 5;
		uint32_t* indirectInit =
			(uint32_t*)tf_calloc(gNumAsteroids * indirectArgElementCount, sizeof(uint32_t));    // For use with compute shader
		for (uint32_t i = 0; i < gNumAsteroids; i++)
		{
			IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectInit[i * indirectArgElementCount + drawArgOffset];
			arg->mInstanceCount = 1;
			arg->mStartInstance = 0;
			arg->mStartIndex = 0;
			arg->mIndexCount = 60;
			arg->mVertexOffset = 0;
			
			if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
			{
				indirectInit[i * indirectArgElementCount] = i;
			}
		}

		BufferLoadDesc indirectBufDesc = {};
		indirectBufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
		indirectBufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectBufDesc.mDesc.pCounterBuffer = NULL;
		indirectBufDesc.mDesc.mFirstElement = 0;
		indirectBufDesc.mDesc.mElementCount = gNumAsteroids * indirectArgElementCount;
		indirectBufDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectBufDesc.mDesc.mStartState = RESOURCE_STATE_INDIRECT_ARGUMENT;
		indirectBufDesc.mDesc.mSize = indirectBufDesc.mDesc.mStructStride * indirectBufDesc.mDesc.mElementCount;
		indirectBufDesc.pData = indirectInit;
		indirectBufDesc.ppBuffer = &pIndirectBuffer;
		addResource(&indirectBufDesc, NULL);

		indirectBufDesc.mDesc.mElementCount = gNumAsteroidsPerSubset * indirectArgElementCount;
		indirectBufDesc.mDesc.mSize = indirectBufDesc.mDesc.mStructStride * indirectBufDesc.mDesc.mElementCount;

		indirectBufDesc.pData = indirectInit;
		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			for (uint32_t j = 0; j < gImageCount; j++)
			{
				indirectBufDesc.ppBuffer = &(gAsteroidSubsets[i].pSubsetIndirect[j]);
				addResource(&indirectBufDesc, NULL);
			}
		}

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
		bufDesc.mDesc.mStartState = RESOURCE_STATE_COPY_DEST;
		for (uint32_t i = 0; i < gNumSubsets; i++)
		{
			for (uint32_t j = 0; j < gImageCount; j++)
			{
				bufDesc.ppBuffer = &gAsteroidSubsets[i].pAsteroidInstanceBuffer[j];
				addResource(&bufDesc, NULL);
			}
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
		bufDesc.mDesc.mSize = sizeof(Vertex) * (uint32_t)gVertices.size();
		bufDesc.pData = gVertices.data();
		bufDesc.ppBuffer = &pAsteroidVertexBuffer;
		addResource(&bufDesc, NULL);

		// Index Buffer
		bufDesc = {};
		bufDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		bufDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bufDesc.mDesc.mSize = sizeof(uint16_t) * (uint32_t)gIndices.size();
		bufDesc.pData = gIndices.data();
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
		for (uint32_t i = 0; i < gImageCount; i++)
		{
			bufDesc.ppBuffer = &pDynamicAsteroidBuffer[i];
			addResource(&bufDesc, NULL);
		}

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

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition += vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.5f);
		uiCreateComponent(GetName(), &guiDesc, &pGui);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		static const char*    enumNames[] = { "Instanced Rendering", "Execute Indirect", "Execute Indirect with GPU Compute"};
		/************************************************************************/
		/************************************************************************/

		DropdownWidget renderModeDropdown;
		renderModeDropdown.pData = &gRenderingMode;
		renderModeDropdown.pNames = enumNames;
		renderModeDropdown.mCount = sizeof(enumNames) / sizeof(enumNames[0]);

		luaRegisterWidget(uiCreateComponentWidget(pGui, "Rendering Mode: ", &renderModeDropdown, WIDGET_TYPE_DROPDOWN));

		CheckboxWidget multithreadCheckbox;
		multithreadCheckbox.pData = &gUseThreads;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Multithreaded CPU Update", &multithreadCheckbox, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget paniniCheckbox;
		paniniCheckbox.pData = &gbPaniniEnabled;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Enable Panini Projection", &paniniCheckbox, WIDGET_TYPE_CHECKBOX));
		/************************************************************************/
		// Panini props
		/************************************************************************/
#if !defined(TARGET_IOS)
		SliderFloatWidget camHorFovSlider;
		camHorFovSlider.pData = &gPaniniParams.FoVH;
		camHorFovSlider.mMin = 30.0f;
		camHorFovSlider.mMax = 179.0f;
		camHorFovSlider.mStep = 1.0f;
		uiCreateDynamicWidgets(&gPaniniControls, "Camera Horizontal FoV", &camHorFovSlider, WIDGET_TYPE_SLIDER_FLOAT);

		SliderFloatWidget paniniSliderD;
		paniniSliderD.pData = &gPaniniParams.D;
		paniniSliderD.mMin = 0.0f;
		paniniSliderD.mMax = 1.0f;
		paniniSliderD.mStep = 0.001f;
		uiCreateDynamicWidgets(&gPaniniControls, "Panini D Parameter", &paniniSliderD, WIDGET_TYPE_SLIDER_FLOAT);

		SliderFloatWidget paniniSliderS;
		paniniSliderS.pData = &gPaniniParams.S;
		paniniSliderS.mMin = 0.0f;
		paniniSliderS.mMax = 1.0f;
		paniniSliderS.mStep = 0.001f;
		uiCreateDynamicWidgets(&gPaniniControls, "Panini S Parameter", &paniniSliderS, WIDGET_TYPE_SLIDER_FLOAT);

		SliderFloatWidget screenScaleSlider;
		screenScaleSlider.pData = &gPaniniParams.scale;
		screenScaleSlider.mMin = 1.0f;
		screenScaleSlider.mMax = 10.0f;
		screenScaleSlider.mStep = 0.01f;
		uiCreateDynamicWidgets(&gPaniniControls, "Screen Scale", &screenScaleSlider, WIDGET_TYPE_SLIDER_FLOAT);

		if (gbPaniniEnabled)
			uiShowDynamicWidgets(&gPaniniControls, pGui);
		else
			uiHideDynamicWidgets(&gPaniniControls, pGui);
#endif
		DropdownWidget ddTestScripts;
		ddTestScripts.pData = &gCurrentScriptIndex;
		ddTestScripts.pNames = gTestScripts;
		ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);

		luaRegisterWidget(uiCreateComponentWidget(pGui, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

		ButtonWidget bRunScript;
		UIWidget* pRunScript = uiCreateComponentWidget(pGui, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript); 
		luaRegisterWidget(pRunScript);

#if !defined(TARGET_IOS)
		gPanini.Init(pRenderer);
		gPanini.SetMaxDraws(1);
#endif

		waitForAllResourceLoads();
		tf_free(indirectInit);

		/************************************************************************/
		/************************************************************************/
		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ -121.4f, 69.9f, -562.8f };
		vec3                   lookAt{ 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; }};
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
			{
				uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			}
			return true;
		};

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0; 
	
		return true;
	}

	void Exit()
	{
		waitThreadSystemIdle(pThreadSystem);

		exitThreadSystem(pThreadSystem);
		exitInputSystem();

		exitCameraController(pCameraController);
	
		tf_free(gAsteroidSim.indexOffsets);
		gAsteroidSim.Exit();
		
		// Erase vertex/index buffer memory.
		gVertices.set_capacity(0);
		gIndices.set_capacity(0);

#if !defined(TARGET_IOS)
		uiDestroyDynamicWidgets(&gPaniniControls);
		gPanini.Exit();
#endif

		exitProfiler(); 

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pSkyboxUniformBuffer[i]);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pDynamicAsteroidBuffer[i]);

		removeResource(pSkyBoxVertexBuffer);
		removeResource(pAsteroidVertexBuffer);
		removeResource(pAsteroidIndexBuffer);
		removeResource(pStaticAsteroidBuffer);
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
			for (uint32_t j = 0; j < gImageCount; ++j)
				removeResource(gAsteroidSubsets[i].pAsteroidInstanceBuffer[j]);
			for (uint32_t j = 0; j < gImageCount; ++j)
				removeResource(gAsteroidSubsets[i].pSubsetIndirect[j]);
		}

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
		exitRenderer(pRenderer);
		pRenderer = NULL;
		gAsteroidSubsets.set_capacity(0);
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			if (!addSwapChain())
				return false;

			if (!addDepthBuffer())
				return false;

			RenderTargetDesc postProcRTDesc = {};
			postProcRTDesc.mArraySize = 1;
			postProcRTDesc.mClearValue = { {0.0f, 0.0f, 0.0f, 0.0f} };
			postProcRTDesc.mDepth = 1;
			postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
			postProcRTDesc.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
			postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			postProcRTDesc.mHeight = mSettings.mHeight;
			postProcRTDesc.mWidth = mSettings.mWidth;
			postProcRTDesc.mSampleCount = SAMPLE_COUNT_1;
			postProcRTDesc.mSampleQuality = 0;
			postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
			addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

		prepareDescriptorSets();

		UserInterfaceLoadDesc uiLoad = {};
		uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		uiLoad.mHeight = mSettings.mHeight;
		uiLoad.mWidth = mSettings.mWidth;
		uiLoad.mLoadType = pReloadDesc->mType;
		loadUserInterface(&uiLoad);

		FontSystemLoadDesc fontLoad = {};
		fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		fontLoad.mHeight = mSettings.mHeight;
		fontLoad.mWidth = mSettings.mWidth;
		fontLoad.mLoadType = pReloadDesc->mType;
		loadFontSystem(&fontLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTarget(pRenderer, pDepthBuffer);
			removeRenderTarget(pRenderer, pIntermediateRenderTarget);
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	float frameTime = 0.0f;

	void Update(float deltaTime)
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		frameTime = deltaTime;

		pCameraController->update(deltaTime);

		static bool paniniEnabled = gbPaniniEnabled;
		if (paniniEnabled != gbPaniniEnabled)
		{
			if (gbPaniniEnabled)
			{
				uiShowDynamicWidgets(&gPaniniControls, pGui);
			}
			else
			{
				uiHideDynamicWidgets(&gPaniniControls, pGui);
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
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

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

		Semaphore* pWaitSemaphores[2];
		uint32_t waitSemaphoresCount = 0;

		pWaitSemaphores[waitSemaphoresCount++] = pImageAcquiredSemaphore;

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
		CameraMatrix projMat = CameraMatrix::perspective(gPaniniParams.FoVH * (float)PI / 180.0f, aspectInverse, 10000.0f, 0.1f);
#else
        CameraMatrix projMat = CameraMatrix::perspective(90.0f * (float)PI / 180.0f, aspectInverse, 10000.0f, 0.1f);
#endif
		CameraMatrix viewProjMat = projMat * viewMat;

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
#if defined(METAL) || defined(ORBIS) || defined(PROSPERO)    // Andrés: Check mIndexOffset declaration.
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
			loadActionLoad.mLoadActionDepth = LOAD_ACTION_LOAD;
			// Execute indirect
			cmdBindRenderTargets(cmd, 1, &pSceneRenderTarget, pDepthBuffer, &loadActionLoad, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneRenderTarget->mWidth, (float)pSceneRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSceneRenderTarget->mWidth, pSceneRenderTarget->mHeight);

			const uint32_t asteroidStride = sizeof(Vertex);
            cmdBindPipeline(cmd, pIndirectPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetIndirectDraw[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetIndirectDrawForCompute);
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
		}
#endif
		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);

		const float txtOffset = 15.f;
		float txtOrigY = txtOffset;
		float2 screenCoords = float2(txtOffset, txtOrigY);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		float2 txtSize = cmdDrawCpuProfile(cmd, screenCoords, &gFrameTimeDraw);
		
		screenCoords.y += txtSize.y + 5 * txtOffset;
		cmdDrawGpuProfile(cmd, screenCoords, gGpuProfileToken, &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);
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

		// This call must be after "waitForAllResourceLoads" so we are sure that we get the semaphore for the last update.
		Semaphore* updateSemaphore = getLastSemaphoreCompleted(0);
		if (updateSemaphore)
		{
			// We will wait for this update operation to be completed before rendering.
			pWaitSemaphores[waitSemaphoresCount++] = updateSemaphore;
		}

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = (uint32_t)allCmds.size();
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = waitSemaphoresCount;
		submitDesc.ppCmds = allCmds.data();
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = pWaitSemaphores;
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
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
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
		// Indirect Draw when using a compute
		setDesc = { pIndirectRoot, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetIndirectDrawForCompute);
		// Direct Draw
		setDesc = { pBasicRoot, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDirectDraw[0]);
		setDesc = { pBasicRoot, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, gNumSubsets * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDirectDraw[1]);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetCompute[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetCompute[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetIndirectDraw[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetIndirectDraw[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetIndirectDrawForCompute);
		removeDescriptorSet(pRenderer, pDescriptorSetDirectDraw[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetDirectDraw[1]);
	}

	void addRootSignatures()
	{
		const char* pStaticSamplerNames[] = { "uSampler0", "uSampler1" };
		Sampler* pStaticSamplers[] = { pBasicSampler, pSkyBoxSampler };
		RootSignatureDesc basicRootDesc = { &pBasicShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		RootSignatureDesc skyRootDesc = { &pSkyBoxDrawShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		RootSignatureDesc computeRootDesc = { &pComputeShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		RootSignatureDesc indirectRootDesc = { &pIndirectShader, 1, 0, pStaticSamplerNames, pStaticSamplers, 2 };
		addRootSignature(pRenderer, &basicRootDesc, &pBasicRoot);
		gDrawIdRootConstantIndex = getDescriptorIndexFromName(pBasicRoot, "rootConstant");

		addRootSignature(pRenderer, &skyRootDesc, &pSkyBoxRoot);
		addRootSignature(pRenderer, &computeRootDesc, &pComputeRoot);
		addRootSignature(pRenderer, &indirectRootDesc, &pIndirectRoot);

		uint32_t indirectArgCount = 0;
		IndirectArgumentDescriptor indirectArgDescs[2] = {};
		if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
		{
			indirectArgDescs[0].mType = INDIRECT_CONSTANT;    // Root Constant
			indirectArgDescs[0].mIndex = getDescriptorIndexFromName(pIndirectRoot, "rootConstant");
			indirectArgDescs[0].mByteSize = sizeof(uint32_t);
			++indirectArgCount;
		}
		indirectArgDescs[indirectArgCount++].mType = INDIRECT_DRAW_INDEX;    // Indirect Index Draw Arguments

		CommandSignatureDesc cmdSignatureDesc = { pIndirectRoot, indirectArgDescs, indirectArgCount, true };
		addIndirectCommandSignature(pRenderer, &cmdSignatureDesc, &pIndirectCommandSignature);
	}

	void removeRootSignatures()
	{
		removeIndirectCommandSignature(pRenderer, pIndirectCommandSignature);

		removeRootSignature(pRenderer, pBasicRoot);
		removeRootSignature(pRenderer, pSkyBoxRoot);
		removeRootSignature(pRenderer, pIndirectRoot);
		removeRootSignature(pRenderer, pComputeRoot);
	}
	
	void addShaders()
	{
		ShaderLoadDesc instanceShader = {};
		instanceShader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		instanceShader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		ShaderLoadDesc indirectShader = {};
		indirectShader.mStages[0] = { "ExecuteIndirect.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		indirectShader.mStages[1] = { "ExecuteIndirect.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		ShaderLoadDesc gpuUpdateShader = {};
		gpuUpdateShader.mStages[0] = { "ComputeUpdate.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &instanceShader, &pBasicShader);
		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &indirectShader, &pIndirectShader);
		addShader(pRenderer, &gpuUpdateShader, &pComputeShader);		
	}

	void removeShaders()
	{
		removeShader(pRenderer, pBasicShader);
		removeShader(pRenderer, pSkyBoxDrawShader);
		removeShader(pRenderer, pIndirectShader);
		removeShader(pRenderer, pComputeShader);
	}

	void addPipelines()
	{
#if !defined(TARGET_IOS)
		RenderTarget* rts[1];
		rts[0] = pIntermediateRenderTarget;
		gPanini.Load(rts);
#endif

		PipelineDesc descCompute = {};
		descCompute.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineDesc = descCompute.mComputeDesc;
		computePipelineDesc.pShaderProgram = pComputeShader;
		computePipelineDesc.pRootSignature = pComputeRoot;
		addPipeline(pRenderer, &descCompute, &pComputePipeline);

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
	}

	void removePipelines()
	{
#if !defined(TARGET_IOS)
		gPanini.Unload();
#endif
		removePipeline(pRenderer, pComputePipeline);
		removePipeline(pRenderer, pBasicPipeline);
		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pIndirectPipeline);
	}

	void prepareDescriptorSets()
	{
#if !defined(TARGET_IOS)
		gPanini.SetSourceTexture(pIntermediateRenderTarget->pTexture, 0);
#endif
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
		// We just use one buffer when running the compute shader.
		computeParams[1].pName = "asteroidsDynamic";
		computeParams[1].ppBuffers = &pDynamicAsteroidBuffer[0];
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
		indirectParams[1].pName = "uTex0";
		indirectParams[1].ppTextures = &pAsteroidTex;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetIndirectDraw[0], 2, indirectParams);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			indirectParams[0].pName = "uniformBlock";
			indirectParams[0].ppBuffers = &pIndirectUniformBuffer[i];
			// Because "pDynamicAsteroidBuffer" is update on CPU we triple buffer it.
			indirectParams[1].pName = "asteroidsDynamic";
			indirectParams[1].ppBuffers = &pDynamicAsteroidBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetIndirectDraw[1], 2, indirectParams);

			// Because "pDynamicAsteroidBuffer" is updated in GPU, we just need one buffer.
			// But the shader expects a descriptorset with a per frame frequency. Because of this,
			// we need point "asteroidsDynamic" to the same buffer (pDynamicAsteroidBuffer[0]).
			indirectParams[1].ppBuffers = &pDynamicAsteroidBuffer[0];
			updateDescriptorSet(pRenderer, i, pDescriptorSetIndirectDrawForCompute, 2, indirectParams);
		}

		DescriptorData directParams[1] = {};
		directParams[0].pName = "uTex0";
		directParams[0].ppTextures = &pAsteroidTex;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetDirectDraw[0], 1, directParams);
		for (uint32_t i = 0; i < gNumSubsets; ++i)
		{
			for (uint32_t j = 0; j < gImageCount; j++)
			{
				directParams[0].pName = "instanceBuffer";
				directParams[0].ppBuffers = &gAsteroidSubsets[i].pAsteroidInstanceBuffer[j];
				updateDescriptorSet(pRenderer, i*gImageCount + j, pDescriptorSetDirectDraw[1], 1, directParams);
			}
		}
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
        depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
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

		outIndices = newIndices;
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
			Subset subset = {};

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
		unsigned index, const CameraMatrix& viewProj, uint32_t frameIdx, RenderTarget* pRenderTarget, RenderTarget* pDepthBuffer, float deltaTime)
	{
        LoadActionsDesc loadActionLoad = {};
        loadActionLoad.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActionLoad.mLoadActionDepth = LOAD_ACTION_LOAD;

		uint32_t startIdx = index * gNumAsteroidsPerSubset;
		uint32_t endIdx = min(startIdx + gNumAsteroidsPerSubset, gNumAsteroids);

		Subset& subset = gAsteroidSubsets[index];
		Cmd*    cmd = subset.pCmds[frameIdx];

		beginCmd(cmd);

		gAsteroidSim.Update(deltaTime, startIdx, endIdx, pCameraController->getViewPosition());

		vec4 frustumPlanes[6];
		CameraMatrix::extractFrustumClipPlanes(
			viewProj, frustumPlanes[0], frustumPlanes[1], frustumPlanes[2], frustumPlanes[3], frustumPlanes[4], frustumPlanes[5], true);

		if (gRenderingMode == RenderingMode_Instanced)
		{
			BufferUpdateDesc uniformUpdate = { gAsteroidSubsets[index].pAsteroidInstanceBuffer[frameIdx] };
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

				CameraMatrix mvp = viewProj * transform;
				mat3 normalMat = inverse(transpose(mat3(transform[0].getXYZ(), transform[1].getXYZ(), transform[2].getXYZ())));

				// Update uniforms
				UniformBasic& uniformData = instanceData[localIndex];
				uniformData.mModelViewProj = mvp;
				uniformData.mNormalMat = mat4(normalMat, vec3(0, 0, 0));
				uniformData.mSurfaceColor = staticAsteroid.surfaceColor;
				uniformData.mDeepColor = staticAsteroid.deepColor;
				uniformData.mTextureID = staticAsteroid.textureID;
			}

			SyncToken token = {};
			endUpdateResource(&uniformUpdate, &token);
			waitForToken(&token);

			BufferBarrier bufferBarrier = { gAsteroidSubsets[index].pAsteroidInstanceBuffer[frameIdx], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 1, &bufferBarrier, 0, NULL, 0, NULL);

			// Render all asteroids
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActionLoad, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

            cmdBindPipeline(cmd, pBasicPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetDirectDraw[0]);
			cmdBindDescriptorSet(cmd, index * gImageCount + frameIdx, pDescriptorSetDirectDraw[1]);
			const uint32_t asteroidStride = sizeof(Vertex);
			cmdBindVertexBuffer(cmd, 1, &pAsteroidVertexBuffer, &asteroidStride, NULL);
			cmdBindIndexBuffer(cmd, pAsteroidIndexBuffer, INDEX_TYPE_UINT16, 0);

			for (uint32_t i = startIdx; i < endIdx; i++)
			{
				const AsteroidDynamic& dynamicAsteroid = gAsteroidSim.asteroidsDynamic[i];

				const mat4& transform = dynamicAsteroid.transform;
				if (ShouldCullAsteroid(transform.getTranslation(), frustumPlanes))
					continue;

				uint32_t rcInd = i - startIdx;
				cmdBindPushConstants(cmd, pBasicRoot, gDrawIdRootConstantIndex, &rcInd);
				cmdDrawIndexed(cmd, dynamicAsteroid.indexCount, dynamicAsteroid.indexStart, 0);
			}

			cmdBindRenderTargets(cmd, 0, NULL, NULL, &loadActionLoad, NULL, NULL, -1, -1);
			bufferBarrier = { gAsteroidSubsets[index].pAsteroidInstanceBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST };
			cmdResourceBarrier(cmd, 1, &bufferBarrier, 0, NULL, 0, NULL);
		}
		else if (gRenderingMode == RenderingMode_ExecuteIndirect)
		{
			// Setup indirect draw arguments
			uint32_t numToDraw = 0;
			
			uint32_t drawArgOffset = pRenderer->pActiveGpuSettings->mIndirectRootConstant ? 1 : 0;
			uint32_t indirectArgElementCount = drawArgOffset + 5;

			BufferUpdateDesc indirectBufferUpdate = { subset.pSubsetIndirect[frameIdx] };
			beginUpdateResource(&indirectBufferUpdate);
			uint32_t* argData = (uint32_t*)indirectBufferUpdate.pMappedData;
			for (uint32_t i = startIdx; i < endIdx; ++i)
			{
				AsteroidStatic  staticAsteroid = gAsteroidSim.asteroidsStatic[i];
				AsteroidDynamic dynamicAsteroid = gAsteroidSim.asteroidsDynamic[i];

				if (ShouldCullAsteroid(dynamicAsteroid.transform.getTranslation(), frustumPlanes))
					continue;

				IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&argData[numToDraw * indirectArgElementCount + drawArgOffset];
				arg->mInstanceCount = 1;
				arg->mStartInstance = i;
				arg->mStartIndex = dynamicAsteroid.indexStart;
				arg->mIndexCount = dynamicAsteroid.indexCount;
				arg->mVertexOffset = staticAsteroid.vertexStart;
				if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
				{
					argData[numToDraw * indirectArgElementCount] = i;
				}
				numToDraw++;
			}

			// Update indirect arguments
			endUpdateResource(&indirectBufferUpdate, NULL);

			BufferUpdateDesc dynamicBufferUpdate = { pDynamicAsteroidBuffer[frameIdx], sizeof(AsteroidDynamic) * startIdx, sizeof(AsteroidDynamic) * (endIdx - startIdx) };
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
			cmdExecuteIndirect(cmd, pIndirectCommandSignature, numToDraw, subset.pSubsetIndirect[frameIdx], 0, NULL, 0);
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
