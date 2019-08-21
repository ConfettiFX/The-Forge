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

#define _USE_MATH_DEFINES

// Unit Test for testing Compute Shaders and Tessellation
// using Responsive Real-Time Grass Rendering for General 3D Scenes

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"

//input
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

ICameraController* pCameraController = NULL;

// Grass Parameters
static const unsigned int NUM_BLADES = 1 << 16;
#define MIN_HEIGHT 1.3f
#define MAX_HEIGHT 3.5f
#define MIN_WIDTH 0.1f
#define MAX_WIDTH 0.14f
#define MIN_BEND 7.0f
#define MAX_BEND 13.0f
#define PLANE_SIZE 100.0f

enum
{
	WIND_MODE_STRAIGHT = 0,
	WIND_MODE_RADIAL = 1,
};

static uint32_t gFillMode = FILL_MODE_SOLID;
static uint32_t gWindMode = WIND_MODE_STRAIGHT;
static uint32_t gMaxTessellationLevel = 5;

static float gWindSpeed = 25.0f;
static float gWindWidth = 6.0f;
static float gWindStrength = 15.0f;

bool gToggleVSync = false;

struct GrassUniformBlock
{
	mat4 mWorld;
	mat4 mView;
	mat4 mInvView;
	mat4 mProj;
	mat4 mViewProj;

	float mDeltaTime;
	float mTotalTime;

	int mWindMode;
	int mMaxTessellationLevel;

	float mWindSpeed;
	float mWindWidth;
	float mWindStrength;
};

struct Blade
{
	// Position and direction
	vec4 mV0;
	// Bezier point and height
	vec4 mV1;
	// Physical model guide and width
	vec4 mV2;
	// Up vector and stiffness coefficient
	vec4 mUp;
};

struct BladeDrawIndirect
{
	uint32_t mVertexCount;
	uint32_t mInstanceCount;
	uint32_t mFirstVertex;
	uint32_t mFirstInstance;
};

#ifdef METAL
struct PatchTess
{
	half edges[4];
	half inside[2];
};

struct HullOut
{
	float4 position;
	float4 tese_v1;
	float4 tese_v2;
	float4 tese_up;
	float4 tese_widthDir;
};
#endif

eastl::vector<Blade> gBlades;

Timer      gAccumTimer;
HiresTimer gTimer;

UIApp         gAppUI = {};
GuiComponent* pGui;

const char* pszBases[FSR_Count] = {
	"../../../src/07_Tessellation/",        // FSR_BinShaders
	"../../../src/07_Tessellation/",        // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/07_Tessellation/",        // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_Audio
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

const uint32_t gImageCount = 3;
bool           bToggleMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

CmdPool* pUICmdPool = NULL;
Cmd**    ppUICmds = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Sampler*         pSampler = NULL;
DepthState*      pDepth = NULL;
RasterizerState* pRast = NULL;
RasterizerState* pWireframeRast = NULL;

RenderTarget* PGrassRenderTarget = NULL;

Buffer* pGrassUniformBuffer[gImageCount] = { NULL };
Buffer* pBladeStorageBuffer = NULL;
Buffer* pCulledBladeStorageBuffer = NULL;

Buffer* pBladeNumBuffer = NULL;

#ifdef METAL
Buffer* pTessFactorsBuffer = NULL;
Buffer* pHullOutputBuffer = NULL;
#endif

CommandSignature* pIndirectCommandSignature = NULL;

Shader*   pGrassShader = NULL;
Pipeline* pGrassPipeline = NULL;
#ifdef METAL
Shader*   pGrassVertexHullShader = NULL;
Pipeline* pGrassVertexHullPipeline = NULL;
#endif

Pipeline* pGrassPipelineForWireframe = NULL;

RootSignature* pGrassRootSignature = NULL;

#ifdef METAL
RootSignature* pGrassVertexHullRootSignature = NULL;
#endif

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

Shader*           pComputeShader = NULL;
Pipeline*         pComputePipeline = NULL;
RootSignature*    pComputeRootSignature = NULL;

DescriptorBinder* pDescriptorBinder = NULL;

uint32_t gFrameIndex = 0;

GrassUniformBlock gGrassUniformData;

GpuProfiler* pGpuProfiler = NULL;

unsigned gStartTime = 0;

struct ObjectProperty
{
	float mRotX = 0, mRotY = 0;
} gObjSettings;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class Tessellation: public IApp
{
	public:
	Tessellation()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init()
	{
		HiresTimer startTime;

		initNoise();

		initBlades();

		BladeDrawIndirect indirectDraw;
		indirectDraw.mVertexCount = NUM_BLADES;
		indirectDraw.mInstanceCount = 1;
		indirectDraw.mFirstVertex = 0;
		indirectDraw.mFirstInstance = 0;

		// renderer for swapchains
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

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		initProfiler(pRenderer);
		profileRegisterInput();
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", FSR_Absolute))
			return false;
#endif

#if defined(DIRECT3D12) || defined(VULKAN)
		ShaderLoadDesc grassShader = {};
		grassShader.mStages[0] = { "grass.vert", NULL, 0, FSR_SrcShaders };
		grassShader.mStages[1] = { "grass.frag", NULL, 0, FSR_SrcShaders };
		grassShader.mStages[2] = { "grass.tesc", NULL, 0, FSR_SrcShaders };
		grassShader.mStages[3] = { "grass.tese", NULL, 0, FSR_SrcShaders };
#else
		ShaderLoadDesc grassVertexHullShader = {};
		grassVertexHullShader.mStages[0] = { "grass_verthull.comp", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc grassShader = {};
		grassShader.mStages[0] = { "grass.domain.vert", NULL, 0, FSR_SrcShaders };
		grassShader.mStages[1] = { "grass.frag", NULL, 0, FSR_SrcShaders };
#endif
		ShaderLoadDesc computeShader = {};
		computeShader.mStages[0] = { "compute.comp", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &grassShader, &pGrassShader);
		addShader(pRenderer, &computeShader, &pComputeShader);

		RootSignatureDesc grassRootDesc = { &pGrassShader, 1 };
		RootSignatureDesc computeRootDesc = { &pComputeShader, 1 };
		addRootSignature(pRenderer, &grassRootDesc, &pGrassRootSignature);
		addRootSignature(pRenderer, &computeRootDesc, &pComputeRootSignature);

		eastl::vector<DescriptorBinderDesc> descriptorBinderDesc;
		descriptorBinderDesc.push_back({ pGrassRootSignature });
		descriptorBinderDesc.push_back({ pComputeRootSignature });

#ifdef METAL
		addShader(pRenderer, &grassVertexHullShader, &pGrassVertexHullShader);

		RootSignatureDesc vertexHullRootDesc = { &pGrassVertexHullShader, 1 };
		addRootSignature(pRenderer, &vertexHullRootDesc, &pGrassVertexHullRootSignature);
		descriptorBinderDesc.push_back({ pGrassVertexHullRootSignature });
#endif
		addDescriptorBinder(pRenderer, 0, (uint32_t)descriptorBinderDesc.size(), descriptorBinderDesc.data(), &pDescriptorBinder);

		PipelineDesc pipelineDesc = { };
		pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
		computePipelineDesc.pRootSignature = pComputeRootSignature;
		computePipelineDesc.pShaderProgram = pComputeShader;
		addPipeline(pRenderer, &pipelineDesc, &pComputePipeline);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRast);

		SamplerDesc samplerDesc = { FILTER_NEAREST,
									FILTER_NEAREST,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		RasterizerStateDesc rasterizerStateWireframeDesc = {};
		rasterizerStateWireframeDesc.mCullMode = CULL_MODE_NONE;
		rasterizerStateWireframeDesc.mFillMode = FILL_MODE_WIREFRAME;
		addRasterizerState(pRenderer, &rasterizerStateWireframeDesc, &pWireframeRast);

#ifdef METAL
        pipelineDesc.mComputeDesc = {};
		ComputePipelineDesc& grassVertexHullPipelineDesc = pipelineDesc.mComputeDesc;
		grassVertexHullPipelineDesc.pRootSignature = pGrassVertexHullRootSignature;
		grassVertexHullPipelineDesc.pShaderProgram = pGrassVertexHullShader;
		addPipeline(pRenderer, &pipelineDesc, &pGrassVertexHullPipeline);
#endif

		BufferLoadDesc ubGrassDesc = {};
		ubGrassDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubGrassDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubGrassDesc.mDesc.mSize = sizeof(GrassUniformBlock);
		ubGrassDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubGrassDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubGrassDesc.ppBuffer = &pGrassUniformBuffer[i];
			addResource(&ubGrassDesc);
		}

		BufferLoadDesc sbBladeDesc = {};
		sbBladeDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		sbBladeDesc.mDesc.mFirstElement = 0;
		sbBladeDesc.mDesc.mElementCount = NUM_BLADES;
		sbBladeDesc.mDesc.mVertexStride = sizeof(Blade);
		sbBladeDesc.mDesc.mStructStride = sizeof(Blade);
		sbBladeDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sbBladeDesc.mDesc.mSize = NUM_BLADES * sizeof(Blade);

		sbBladeDesc.pData = gBlades.data();
		sbBladeDesc.ppBuffer = &pBladeStorageBuffer;
		addResource(&sbBladeDesc);

		BufferLoadDesc sbCulledBladeDesc = {};
		sbCulledBladeDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sbCulledBladeDesc.mDesc.mFirstElement = 0;
		sbCulledBladeDesc.mDesc.mElementCount = NUM_BLADES;
		sbCulledBladeDesc.mDesc.mVertexStride = sizeof(Blade);
		sbCulledBladeDesc.mDesc.mStructStride = sizeof(Blade);
		sbCulledBladeDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sbCulledBladeDesc.mDesc.mSize = NUM_BLADES * sizeof(Blade);

		sbCulledBladeDesc.pData = gBlades.data();
		sbCulledBladeDesc.ppBuffer = &pCulledBladeStorageBuffer;
		addResource(&sbCulledBladeDesc);

		BufferLoadDesc sbBladeNumDesc = {};
		sbBladeNumDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
		sbBladeNumDesc.mDesc.mFirstElement = 0;
		sbBladeNumDesc.mDesc.mElementCount = 1;
		sbBladeNumDesc.mDesc.mStructStride = sizeof(BladeDrawIndirect);
#ifndef TARGET_IOS
		sbBladeNumDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
#else
		sbBladeNumDesc.mDesc.mMemoryUsage =
			RESOURCE_MEMORY_USAGE_CPU_TO_GPU;    // On iOS, we need to map this buffer to CPU memory to support tessellated execute-indirect.
#endif
		sbBladeNumDesc.mDesc.mSize = sizeof(BladeDrawIndirect);

		sbBladeNumDesc.pData = &indirectDraw;
		sbBladeNumDesc.ppBuffer = &pBladeNumBuffer;
		addResource(&sbBladeNumDesc);

#ifdef METAL
		BufferLoadDesc tessFactorBufferDesc = {};
		tessFactorBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		tessFactorBufferDesc.mDesc.mFirstElement = 0;
		tessFactorBufferDesc.mDesc.mElementCount = NUM_BLADES;
		tessFactorBufferDesc.mDesc.mStructStride = sizeof(PatchTess);
		tessFactorBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		tessFactorBufferDesc.mDesc.mSize = NUM_BLADES * sizeof(PatchTess);
		tessFactorBufferDesc.pData = NULL;
		tessFactorBufferDesc.ppBuffer = &pTessFactorsBuffer;
		addResource(&tessFactorBufferDesc);

		BufferLoadDesc hullOutputBufferDesc = {};
		hullOutputBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		hullOutputBufferDesc.mDesc.mFirstElement = 0;
		hullOutputBufferDesc.mDesc.mElementCount = NUM_BLADES;
		hullOutputBufferDesc.mDesc.mStructStride = sizeof(HullOut);
		hullOutputBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		hullOutputBufferDesc.mDesc.mSize = NUM_BLADES * sizeof(HullOut);
		hullOutputBufferDesc.pData = NULL;
		hullOutputBufferDesc.ppBuffer = &pHullOutputBuffer;
		addResource(&hullOutputBufferDesc);
#endif

		eastl::vector<IndirectArgumentDescriptor> indirectArgDescs(1);
		indirectArgDescs[0] = {};
		indirectArgDescs[0].mType = INDIRECT_DRAW;    // Indirect Index Draw Arguments
		CommandSignatureDesc cmdDesc = {};
		cmdDesc.mIndirectArgCount = (uint32_t)indirectArgDescs.size();
		cmdDesc.pArgDescs = indirectArgDescs.data();
		cmdDesc.pCmdPool = pCmdPool;
		cmdDesc.pRootSignature = pGrassRootSignature;
		addIndirectCommandSignature(pRenderer, &cmdDesc, &pIndirectCommandSignature);

		gGrassUniformData.mTotalTime = 0.0f;
		gGrassUniformData.mMaxTessellationLevel = gMaxTessellationLevel;
		gGrassUniformData.mWindMode = gWindMode;

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(300.0f, 250.0f) / dpiScale;
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
		pGui = gAppUI.AddGuiComponent("Tessellation Properties", &guiDesc);

    pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler));

		static const char* enumNames[] = {
			"SOLID",
			"WIREFRAME",
		};
		static const uint32_t enumValues[] = {
			FILL_MODE_SOLID,
			FILL_MODE_WIREFRAME,
		};

		static const char* enumWindNames[] = {
			"STRAIGHT",
			"RADIAL",
		};
		static const uint32_t enumWindValues[] = {
			WIND_MODE_STRAIGHT,
			WIND_MODE_RADIAL,
		};

		pGui->AddWidget(DropdownWidget("Fill Mode : ", &gFillMode, enumNames, enumValues, 2));
		pGui->AddWidget(DropdownWidget("Wind Mode", &gWindMode, enumWindNames, enumWindValues, 2));

		pGui->AddWidget(SliderFloatWidget("Wind Speed : ", &gWindSpeed, 1.0f, 100.0f));
		pGui->AddWidget(SliderFloatWidget("Wave Width : ", &gWindWidth, 1.0f, 20.0f));
		pGui->AddWidget(SliderFloatWidget("Wind Strength : ", &gWindStrength, 1.0f, 100.0f));

		pGui->AddWidget(SliderUintWidget("Max Tessellation Level : ", &gMaxTessellationLevel, 1, 10));

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGui->AddWidget(CheckboxWidget("Toggle Vsync", &gToggleVSync));
#endif

		CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		pCameraController->setMotionParameters(cmp);

		InputSystem::RegisterInputEvent(cameraInputEvent);

		LOGF(LogLevel::eINFO, "Load time %f ms", startTime.GetUSec(true) / 1000.0f);
		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitProfiler();

		destroyCameraController(pCameraController);

		gAppUI.Exit();

		removeResource(pBladeStorageBuffer);
		removeResource(pCulledBladeStorageBuffer);

		removeResource(pBladeNumBuffer);

#ifdef METAL
		removeResource(pTessFactorsBuffer);
		removeResource(pHullOutputBuffer);
#endif

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		removeIndirectCommandSignature(pRenderer, pIndirectCommandSignature);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}

		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
		removeCmdPool(pRenderer, pUICmdPool);

		removeSampler(pRenderer, pSampler);
		removeDepthState(pDepth);

		removeRasterizerState(pRast);
		removeRasterizerState(pWireframeRast);
		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pGrassUniformBuffer[i]);

		removeShader(pRenderer, pGrassShader);
#ifdef METAL
		removeShader(pRenderer, pGrassVertexHullShader);
#endif
		removeShader(pRenderer, pComputeShader);

#ifdef METAL
		removePipeline(pRenderer, pGrassVertexHullPipeline);
#endif
		removePipeline(pRenderer, pComputePipeline);

		removeRootSignature(pRenderer, pGrassRootSignature);
				
		removeDescriptorBinder(pRenderer, pDescriptorBinder);

#ifdef METAL
		removeRootSignature(pRenderer, pGrassVertexHullRootSignature);
#endif
		removeRootSignature(pRenderer, pComputeRootSignature);

		removeGpuProfiler(pRenderer, pGpuProfiler);

        removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
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

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif

		VertexLayout vertexLayout = {};
#ifndef METAL
		vertexLayout.mAttribCount = 4;
#else
		vertexLayout.mAttribCount = 5;
#endif

		//v0 -- position (Metal)
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		//v1
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD1;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

		//v2
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD2;
		vertexLayout.mAttribs[2].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[2].mBinding = 0;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 8 * sizeof(float);

		//up
		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TEXCOORD3;
		vertexLayout.mAttribs[3].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[3].mBinding = 0;
		vertexLayout.mAttribs[3].mLocation = 3;
		vertexLayout.mAttribs[3].mOffset = 12 * sizeof(float);

#ifdef METAL
		// widthDir
		vertexLayout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD3;
		vertexLayout.mAttribs[4].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[4].mBinding = 0;
		vertexLayout.mAttribs[4].mLocation = 4;
		vertexLayout.mAttribs[4].mOffset = 16 * sizeof(float);
#endif

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_PATCH_LIST;
		pipelineSettings.pRasterizerState = pRast;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRootSignature = pGrassRootSignature;
		pipelineSettings.pShaderProgram = pGrassShader;
		addPipeline(pRenderer, &desc, &pGrassPipeline);
		pipelineSettings.pRasterizerState = pWireframeRast;
		addPipeline(pRenderer, &desc, &pGrassPipelineForWireframe);

#if defined(VULKAN)
		transitionRenderTargets();
#endif

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfiler();
#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pGrassPipeline);
		removePipeline(pRenderer, pGrassPipelineForWireframe);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		//check for Vsync toggle
#if !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		/************************************************************************/
		// Update camera
		/************************************************************************/
		if (InputSystem::GetBoolInput(KEY_BUTTON_X_TRIGGERED))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);
		/************************************************************************/
		// Update uniform buffer
		/************************************************************************/
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		gGrassUniformData.mDeltaTime = deltaTime;
		gGrassUniformData.mProj = projMat;
		gGrassUniformData.mView = viewMat;
		gGrassUniformData.mViewProj = gGrassUniformData.mProj * gGrassUniformData.mView;
		gGrassUniformData.mInvView = inverse(viewMat);
		gGrassUniformData.mWorld = mat4::identity();

		gGrassUniformData.mTotalTime = (float)(getSystemTime() - gStartTime) / 1000.0f;
		;

		gGrassUniformData.mMaxTessellationLevel = gMaxTessellationLevel;
		gGrassUniformData.mWindMode = gWindMode;

		gGrassUniformData.mWindSpeed = gWindSpeed;
		gGrassUniformData.mWindWidth = gWindWidth;
		gGrassUniformData.mWindStrength = gWindStrength;

    // ProfileSetDisplayMode()
      // TODO: need to change this better way 
    if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
    {
      Profile& S = *ProfileGet();
      int nValue = bToggleMicroProfiler ? 1 : 0;
      nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
      S.nDisplay = nValue;

      bPrevToggleMicroProfiler = bToggleMicroProfiler;
    }

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
		/************************************************************************/
		/************************************************************************/
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0.0f };    // Clear depth to the far plane and stencil to 0

		eastl::vector<Cmd*> allCmds;

		//update grass uniform buffer
		//this need to be done after acquireNextImage because we are using gFrameIndex which
		//gets changed when acquireNextImage is called.
		BufferUpdateDesc cbvUpdate = { pGrassUniformBuffer[gFrameIndex], &gGrassUniformData };
		updateResource(&cbvUpdate);

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		const uint32_t* pThreadGroupSize = pComputeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Compute Pass");

		DescriptorData computeParams[4] = {};
		cmdBindPipeline(cmd, pComputePipeline);

		computeParams[0].pName = "GrassUniformBlock";
		computeParams[0].ppBuffers = &pGrassUniformBuffer[gFrameIndex];

		computeParams[1].pName = "Blades";
		computeParams[1].ppBuffers = &pBladeStorageBuffer;

		computeParams[2].pName = "CulledBlades";
		computeParams[2].ppBuffers = &pCulledBladeStorageBuffer;

		computeParams[3].pName = "NumBlades";
		computeParams[3].ppBuffers = &pBladeNumBuffer;

		cmdBindDescriptors(cmd, pDescriptorBinder, pComputeRootSignature, 4, computeParams);
		cmdDispatch(cmd, (int)ceil(NUM_BLADES / pThreadGroupSize[0]), pThreadGroupSize[1], pThreadGroupSize[2]);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
		};
		BufferBarrier srvBarriers[] = {
			{ pBladeNumBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT },
			{ pCulledBladeStorageBuffer, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
		};
		cmdResourceBarrier(cmd, 2, srvBarriers, 1, barriers, false);

#ifdef METAL
		// On Metal, we have to run the grass_vertexHull compute shader before running the post-tesselation shaders.
		DescriptorData vertexHullParams[5] = {};
		cmdBindPipeline(cmd, pGrassVertexHullPipeline);
		vertexHullParams[0].pName = "GrassUniformBlock";
		vertexHullParams[0].ppBuffers = &pGrassUniformBuffer[gFrameIndex];
		vertexHullParams[1].pName = "vertexInput";
		vertexHullParams[1].ppBuffers = &pCulledBladeStorageBuffer;
		vertexHullParams[2].pName = "drawInfo";
		vertexHullParams[2].ppBuffers = &pBladeNumBuffer;
		vertexHullParams[3].pName = "tessellationFactorBuffer";
		vertexHullParams[3].ppBuffers = &pTessFactorsBuffer;
		vertexHullParams[4].pName = "hullOutputBuffer";
		vertexHullParams[4].ppBuffers = &pHullOutputBuffer;
		cmdBindDescriptors(cmd, pDescriptorBinder, pGrassVertexHullRootSignature, 5, vertexHullParams);
		cmdDispatch(cmd, (int)ceil(NUM_BLADES / pThreadGroupSize[0]), pThreadGroupSize[1], pThreadGroupSize[2]);
#endif

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Pass");

		// Draw computed results
		if (gFillMode == FILL_MODE_SOLID)
			cmdBindPipeline(cmd, pGrassPipeline);
		else if (gFillMode == FILL_MODE_WIREFRAME)
			cmdBindPipeline(cmd, pGrassPipelineForWireframe);

		DescriptorData grassParams[1] = {};
		grassParams[0].pName = "GrassUniformBlock";
		grassParams[0].ppBuffers = &pGrassUniformBuffer[gFrameIndex];
		cmdBindDescriptors(cmd, pDescriptorBinder, pGrassRootSignature, 1, grassParams);

#ifndef METAL
		cmdBindVertexBuffer(cmd, 1, &pCulledBladeStorageBuffer, NULL);
#else
		// When using tessellation on Metal, you should always bind the tessellationFactors buffer and the controlPointBuffer together as vertex buffer (following this order).
		Buffer* tessBuffers[] = { pTessFactorsBuffer, pHullOutputBuffer };
		cmdBindVertexBuffer(cmd, 2, tessBuffers, NULL);
#endif
		cmdExecuteIndirect(cmd, pIndirectCommandSignature, 1, pBladeNumBuffer, 0, NULL, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		BufferBarrier uavBarriers[] = {
			{ pBladeNumBuffer, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pCulledBladeStorageBuffer, RESOURCE_STATE_UNORDERED_ACCESS },
		};
		cmdResourceBarrier(cmd, 2, uavBarriers, 0, NULL, true);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

        cmdEndGpuFrameProfile(cmd, pGpuProfiler);

		endCmd(cmd);

		allCmds.push_back(cmd);

		// Draw UI
		cmd = ppUICmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		static HiresTimer timer;
		timer.GetUSec(true);

		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", timer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);
		gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);

		cmdDrawProfiler(cmd);

		gAppUI.Gui(pGui);
		gAppUI.Draw(cmd);
		cmdEndDebugMarker(cmd);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);

		endCmd(cmd);
		allCmds.push_back(cmd);

		queueSubmit(
			pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
			&pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
	}

	const char* GetName() { return "07_Tessellation"; }

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

	float generateRandomFloat() { return rand() / (float)RAND_MAX; }

	void initBlades()
	{
		gBlades.reserve(NUM_BLADES);

		for (unsigned int i = 0; i < NUM_BLADES; i++)
		{
			Blade currentBlade;    // = Blade();

			vec3 bladeUp(0.0f, 1.0f, 0.0f);

			// Generate positions and direction (v0)
			float x = (generateRandomFloat() - 0.5f) * PLANE_SIZE;
			float y = 0.0f;
			float z = (generateRandomFloat() - 0.5f) * PLANE_SIZE;
			float direction = generateRandomFloat() * 2.f * 3.14159265f;
			vec3  bladePosition(x, y, z);
			currentBlade.mV0 = vec4(bladePosition, direction);

			// Bezier point and height (v1)
			float height = MIN_HEIGHT + (generateRandomFloat() * (MAX_HEIGHT - MIN_HEIGHT));
			currentBlade.mV1 = vec4(bladePosition + bladeUp * height, height);

			// Physical model guide and width (v2)
			float width = MIN_WIDTH + (generateRandomFloat() * (MAX_WIDTH - MIN_WIDTH));
			currentBlade.mV2 = vec4(bladePosition + bladeUp * height, width);

			// Up vector and stiffness coefficient (up)
			float stiffness = MIN_BEND + (generateRandomFloat() * (MAX_BEND - MIN_BEND));
			currentBlade.mUp = vec4(bladeUp, stiffness);

			gBlades.push_back(currentBlade);
		}
	}
#if defined(VULKAN)
	void transitionRenderTargets()
	{
		TextureBarrier barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
		beginCmd(ppCmds[0]);
		cmdResourceBarrier(ppCmds[0], 0, NULL, 1, &barrier, false);
		endCmd(ppCmds[0]);
		queueSubmit(pGraphicsQueue, 1, ppCmds, pRenderCompleteFences[0], 0, NULL, 0, NULL);
		waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);
	}
#endif

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

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(Tessellation)
