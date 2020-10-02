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
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"

//input
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

const uint32_t gImageCount = 3;
ProfileToken   gGpuProfileToken;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

CmdPool* pUICmdPools[gImageCount];
Cmd*     pUICmds[gImageCount];

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Sampler*         pSampler = NULL;

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

VirtualJoystickUI gVirtualJoystick;

Shader*           pComputeShader = NULL;
Pipeline*         pComputePipeline = NULL;
RootSignature*    pComputeRootSignature = NULL;

DescriptorSet*    pDescriptorSetCompute[2] = { NULL };
DescriptorSet*    pDescriptorSetGrass = NULL;
#ifdef METAL
DescriptorSet*    pDescriptorSetGrassVertexHull[2] = { NULL };
#endif

uint32_t gFrameIndex = 0;

GrassUniformBlock gGrassUniformData;

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
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
#if !defined(METAL)
			ShaderLoadDesc grassShader = {};
			grassShader.mStages[0] = { "grass.vert", NULL, 0 };
			grassShader.mStages[1] = { "grass.frag", NULL, 0 };
			grassShader.mStages[2] = { "grass.tesc", NULL, 0 };
			grassShader.mStages[3] = { "grass.tese", NULL, 0 };
#else
			ShaderLoadDesc grassVertexHullShader = {};
			grassVertexHullShader.mStages[0] = { "grass_verthull.comp", NULL, 0 };

			ShaderLoadDesc grassShader = {};
			grassShader.mStages[0] = { "grass.domain.vert", NULL, 0 };
			grassShader.mStages[1] = { "grass.frag", NULL, 0 };
#endif

			addShader(pRenderer, &grassShader, &pGrassShader);

			RootSignatureDesc grassRootDesc = { &pGrassShader, 1 };
			addRootSignature(pRenderer, &grassRootDesc, &pGrassRootSignature);

			DescriptorSetDesc setDesc = { pGrassRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGrass);

#ifdef METAL
			addShader(pRenderer, &grassVertexHullShader, &pGrassVertexHullShader);

			RootSignatureDesc vertexHullRootDesc = { &pGrassVertexHullShader, 1 };
			addRootSignature(pRenderer, &vertexHullRootDesc, &pGrassVertexHullRootSignature);

			setDesc = { pGrassVertexHullRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGrassVertexHull[0]);
			setDesc = { pGrassVertexHullRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGrassVertexHull[1]);

			PipelineDesc pipelineDesc = {};
			pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& grassVertexHullPipelineDesc = pipelineDesc.mComputeDesc;
			grassVertexHullPipelineDesc.pRootSignature = pGrassVertexHullRootSignature;
			grassVertexHullPipelineDesc.pShaderProgram = pGrassVertexHullShader;
			addPipeline(pRenderer, &pipelineDesc, &pGrassVertexHullPipeline);
#endif
		}

		ShaderLoadDesc computeShader = {};
		computeShader.mStages[0] = { "compute.comp", NULL, 0 };
		addShader(pRenderer, &computeShader, &pComputeShader);

		RootSignatureDesc computeRootDesc = { &pComputeShader, 1 };
		addRootSignature(pRenderer, &computeRootDesc, &pComputeRootSignature);

		DescriptorSetDesc setDesc = { pComputeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCompute[0]);
		setDesc = { pComputeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCompute[1]);

		PipelineDesc pipelineDesc = {};
		pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineDesc = pipelineDesc.mComputeDesc;
		computePipelineDesc.pRootSignature = pComputeRootSignature;
		computePipelineDesc.pShaderProgram = pComputeShader;
		addPipeline(pRenderer, &pipelineDesc, &pComputePipeline);

		SamplerDesc samplerDesc = { FILTER_NEAREST,
									FILTER_NEAREST,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		BufferLoadDesc ubGrassDesc = {};
		ubGrassDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubGrassDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubGrassDesc.mDesc.mSize = sizeof(GrassUniformBlock);
		ubGrassDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubGrassDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubGrassDesc.ppBuffer = &pGrassUniformBuffer[i];
			addResource(&ubGrassDesc, NULL);
		}

		BufferLoadDesc sbBladeDesc = {};
		sbBladeDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		sbBladeDesc.mDesc.mFirstElement = 0;
		sbBladeDesc.mDesc.mElementCount = NUM_BLADES;
		sbBladeDesc.mDesc.mStructStride = sizeof(Blade);
		sbBladeDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sbBladeDesc.mDesc.mSize = NUM_BLADES * sizeof(Blade);

		sbBladeDesc.pData = gBlades.data();
		sbBladeDesc.ppBuffer = &pBladeStorageBuffer;
		addResource(&sbBladeDesc, NULL);

		BufferLoadDesc sbCulledBladeDesc = {};
		sbCulledBladeDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sbCulledBladeDesc.mDesc.mFirstElement = 0;
		sbCulledBladeDesc.mDesc.mElementCount = NUM_BLADES;
		sbCulledBladeDesc.mDesc.mStructStride = sizeof(Blade);
		sbCulledBladeDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sbCulledBladeDesc.mDesc.mSize = NUM_BLADES * sizeof(Blade);
		sbCulledBladeDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;

		sbCulledBladeDesc.pData = gBlades.data();
		sbCulledBladeDesc.ppBuffer = &pCulledBladeStorageBuffer;
		addResource(&sbCulledBladeDesc, NULL);

		BufferLoadDesc sbBladeNumDesc = {};
		sbBladeNumDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
		sbBladeNumDesc.mDesc.mFirstElement = 0;
		sbBladeNumDesc.mDesc.mElementCount = 1;
		sbBladeNumDesc.mDesc.mStructStride = sizeof(BladeDrawIndirect);
		sbBladeNumDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sbBladeNumDesc.mDesc.mSize = sizeof(BladeDrawIndirect);
		sbBladeNumDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;

		sbBladeNumDesc.pData = &indirectDraw;
		sbBladeNumDesc.ppBuffer = &pBladeNumBuffer;
		addResource(&sbBladeNumDesc, NULL);

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
		addResource(&tessFactorBufferDesc, NULL);

		BufferLoadDesc hullOutputBufferDesc = {};
		hullOutputBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		hullOutputBufferDesc.mDesc.mFirstElement = 0;
		hullOutputBufferDesc.mDesc.mElementCount = NUM_BLADES;
		hullOutputBufferDesc.mDesc.mStructStride = sizeof(HullOut);
		hullOutputBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		hullOutputBufferDesc.mDesc.mSize = NUM_BLADES * sizeof(HullOut);
		hullOutputBufferDesc.pData = NULL;
		hullOutputBufferDesc.ppBuffer = &pHullOutputBuffer;
		addResource(&hullOutputBufferDesc, NULL);
#endif

		IndirectArgumentDescriptor indirectArgDescs[1] = {};
		indirectArgDescs[0] = {};
		indirectArgDescs[0].mType = INDIRECT_DRAW;    // Indirect Index Draw Arguments
		CommandSignatureDesc cmdSigDesc = {};
		cmdSigDesc.mIndirectArgCount = sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]);
		cmdSigDesc.pArgDescs = indirectArgDescs;
		cmdSigDesc.pRootSignature = pGrassRootSignature;
		addIndirectCommandSignature(pRenderer, &cmdSigDesc, &pIndirectCommandSignature);

		gGrassUniformData.mTotalTime = 0.0f;
		gGrassUniformData.mMaxTessellationLevel = gMaxTessellationLevel;
		gGrassUniformData.mWindMode = gWindMode;

		GuiDesc guiDesc = {};				
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f , mSettings.mHeight * 0.2f);

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");
		pGui = gAppUI.AddGuiComponent("Tessellation Properties", &guiDesc);

		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
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

#if !defined(TARGET_IOS)
			pGui->AddWidget(CheckboxWidget("Toggle Vsync", &gToggleVSync));
#endif
		}
		else
		{
			pGui->AddWidget(LabelWidget("Tessellation is not supported on this GPU"));
		}

		CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		if (!initInputSystem(pWindow))
			return false;

		initProfiler();
		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

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

		// Prepare descriptor sets
		DescriptorData computeParams[4] = {};
		computeParams[0].pName = "Blades";
		computeParams[0].ppBuffers = &pBladeStorageBuffer;
		computeParams[1].pName = "CulledBlades";
		computeParams[1].ppBuffers = &pCulledBladeStorageBuffer;
		computeParams[2].pName = "NumBlades";
		computeParams[2].ppBuffers = &pBladeNumBuffer;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetCompute[0], 3, computeParams);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			computeParams[0].pName = "GrassUniformBlock";
			computeParams[0].ppBuffers = &pGrassUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetCompute[1], 1, computeParams);
			if (pRenderer->pActiveGpuSettings->mTessellationSupported)
			{
				updateDescriptorSet(pRenderer, i, pDescriptorSetGrass, 1, computeParams);
#ifdef METAL
				updateDescriptorSet(pRenderer, i, pDescriptorSetGrassVertexHull[1], 1, computeParams);
#endif
			}
		}

#ifdef METAL
		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
			DescriptorData vertexHullParams[4] = {};
			vertexHullParams[0].pName = "vertexInput";
			vertexHullParams[0].ppBuffers = &pCulledBladeStorageBuffer;
			vertexHullParams[1].pName = "drawInfo";
			vertexHullParams[1].ppBuffers = &pBladeNumBuffer;
			vertexHullParams[2].pName = "tessellationFactorBuffer";
			vertexHullParams[2].ppBuffers = &pTessFactorsBuffer;
			vertexHullParams[3].pName = "hullOutputBuffer";
			vertexHullParams[3].ppBuffers = &pHullOutputBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetGrassVertexHull[0], 4, vertexHullParams);
	}
#endif

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		exitProfiler();

		destroyCameraController(pCameraController);

		gAppUI.Exit();

		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
			removeDescriptorSet(pRenderer, pDescriptorSetGrass);

#ifdef METAL
			removeDescriptorSet(pRenderer, pDescriptorSetGrassVertexHull[0]);
			removeDescriptorSet(pRenderer, pDescriptorSetGrassVertexHull[1]);

			removeShader(pRenderer, pGrassVertexHullShader);
			removePipeline(pRenderer, pGrassVertexHullPipeline);
			removeRootSignature(pRenderer, pGrassVertexHullRootSignature);
#endif

			removeShader(pRenderer, pGrassShader);
			removeRootSignature(pRenderer, pGrassRootSignature);
		}

		removeDescriptorSet(pRenderer, pDescriptorSetCompute[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetCompute[1]);

		removeResource(pBladeStorageBuffer);
		removeResource(pCulledBladeStorageBuffer);

		removeResource(pBladeNumBuffer);

#ifdef METAL
		removeResource(pTessFactorsBuffer);
		removeResource(pHullOutputBuffer);
#endif

		gVirtualJoystick.Exit();

		removeIndirectCommandSignature(pRenderer, pIndirectCommandSignature);

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

			removeCmd(pRenderer, pUICmds[i]);
			removeCmdPool(pRenderer, pUICmdPools[i]);
		}

		removeSampler(pRenderer, pSampler);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pGrassUniformBuffer[i]);

		removeShader(pRenderer, pComputeShader);

		removePipeline(pRenderer, pComputePipeline);

		removeRootSignature(pRenderer, pComputeRootSignature);

        exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		removeRenderer(pRenderer);

		gBlades.set_capacity(0);
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

		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
			VertexLayout vertexLayout = {};
#ifndef METAL
			vertexLayout.mAttribCount = 4;
#else
			vertexLayout.mAttribCount = 5;
#endif

			//v0 -- position (Metal)
			vertexLayout.mAttribs[0].mSemantic = SEMANTIC_TEXCOORD0;
			vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
			vertexLayout.mAttribs[0].mBinding = 0;
			vertexLayout.mAttribs[0].mLocation = 0;
			vertexLayout.mAttribs[0].mOffset = 0;

			//v1
			vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD1;
			vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
			vertexLayout.mAttribs[1].mBinding = 0;
			vertexLayout.mAttribs[1].mLocation = 1;
			vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

			//v2
			vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD2;
			vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
			vertexLayout.mAttribs[2].mBinding = 0;
			vertexLayout.mAttribs[2].mLocation = 2;
			vertexLayout.mAttribs[2].mOffset = 8 * sizeof(float);

			//up
			vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TEXCOORD3;
			vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
			vertexLayout.mAttribs[3].mBinding = 0;
			vertexLayout.mAttribs[3].mLocation = 3;
			vertexLayout.mAttribs[3].mOffset = 12 * sizeof(float);

#ifdef METAL
			// widthDir
			vertexLayout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD3;
			vertexLayout.mAttribs[4].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
			vertexLayout.mAttribs[4].mBinding = 0;
			vertexLayout.mAttribs[4].mLocation = 4;
			vertexLayout.mAttribs[4].mOffset = 16 * sizeof(float);
#endif

			DepthStateDesc depthStateDesc = {};
			depthStateDesc.mDepthTest = true;
			depthStateDesc.mDepthWrite = true;
			depthStateDesc.mDepthFunc = CMP_GEQUAL;

			RasterizerStateDesc rasterizerStateDesc = {};
			rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

			RasterizerStateDesc rasterizerStateWireframeDesc = {};
			rasterizerStateWireframeDesc.mCullMode = CULL_MODE_NONE;
			rasterizerStateWireframeDesc.mFillMode = FILL_MODE_WIREFRAME;

			PipelineDesc desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_PATCH_LIST;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			pipelineSettings.pDepthState = &depthStateDesc;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRootSignature = pGrassRootSignature;
			pipelineSettings.pShaderProgram = pGrassShader;
			addPipeline(pRenderer, &desc, &pGrassPipeline);
			pipelineSettings.pRasterizerState = &rasterizerStateWireframeDesc;
			addPipeline(pRenderer, &desc, &pGrassPipelineForWireframe);
		}

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfilerUI();

		gVirtualJoystick.Unload();

		gAppUI.Unload();

		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
			removePipeline(pRenderer, pGrassPipeline);
			removePipeline(pRenderer, pGrassPipelineForWireframe);
		}

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

		//check for Vsync toggle
#if !defined(TARGET_IOS)
		if (pSwapChain->mEnableVsync != gToggleVSync)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif
		/************************************************************************/
		// Update camera
		/************************************************************************/
		pCameraController->update(deltaTime);
		/************************************************************************/
		// Update uniform buffer
		/************************************************************************/
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 1000.0f, 0.1f);

		gGrassUniformData.mDeltaTime = deltaTime;
		gGrassUniformData.mProj = projMat;
		gGrassUniformData.mView = viewMat;
		gGrassUniformData.mViewProj = gGrassUniformData.mProj * gGrassUniformData.mView;
		gGrassUniformData.mInvView = inverse(viewMat);
		gGrassUniformData.mWorld = mat4::identity();

		static float t = 0.0f;
		t += deltaTime;
		gGrassUniformData.mTotalTime = t;

		gGrassUniformData.mMaxTessellationLevel = gMaxTessellationLevel;
		gGrassUniformData.mWindMode = gWindMode;

		gGrassUniformData.mWindSpeed = gWindSpeed;
		gGrassUniformData.mWindWidth = gWindWidth;
		gGrassUniformData.mWindStrength = gWindStrength;

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
		/************************************************************************/
		/************************************************************************/
	}

	void Draw()
	{
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);
		resetCmdPool(pRenderer, pUICmdPools[gFrameIndex]);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { {0.0f, 0.0f, 0.0f, 0.0f} };
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { {0.0f, 0} };    // Clear depth to the far plane and stencil to 0

		eastl::vector<Cmd*> allCmds;

		//update grass uniform buffer
		//this need to be done after acquireNextImage because we are using gFrameIndex which
		//gets changed when acquireNextImage is called.
		BufferUpdateDesc cbvUpdate = { pGrassUniformBuffer[gFrameIndex] };
		beginUpdateResource(&cbvUpdate);
		*(GrassUniformBlock*)cbvUpdate.pMappedData = gGrassUniformData;
		endUpdateResource(&cbvUpdate, NULL);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		const uint32_t* pThreadGroupSize = pComputeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Compute Pass");

		cmdBindPipeline(cmd, pComputePipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetCompute[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetCompute[1]);
		cmdDispatch(cmd, (int)ceil(NUM_BLADES / pThreadGroupSize[0]), pThreadGroupSize[1], pThreadGroupSize[2]);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

#ifdef METAL
		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
			// On Metal, we have to run the grass_vertexHull compute shader before running the post-tesselation shaders.
			cmdBindPipeline(cmd, pGrassVertexHullPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetGrassVertexHull[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGrassVertexHull[1]);
			cmdDispatch(cmd, (int)ceil(NUM_BLADES / pThreadGroupSize[0]), pThreadGroupSize[1], pThreadGroupSize[2]);
		}
#endif
		
		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		BufferBarrier srvBarriers[] = {
			{ pBladeNumBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT },
			{ pCulledBladeStorageBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
		};
		cmdResourceBarrier(cmd, 2, srvBarriers, 0, NULL, 1, barriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		if (pRenderer->pActiveGpuSettings->mTessellationSupported)
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Pass");

			// Draw computed results
			if (gFillMode == FILL_MODE_SOLID)
				cmdBindPipeline(cmd, pGrassPipeline);
			else if (gFillMode == FILL_MODE_WIREFRAME)
				cmdBindPipeline(cmd, pGrassPipelineForWireframe);

			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGrass);

#ifndef METAL
			const uint32_t stride = sizeof(Blade);
			cmdBindVertexBuffer(cmd, 1, &pCulledBladeStorageBuffer, &stride, NULL);
#else
			// When using tessellation on Metal, you should always bind the tessellationFactors buffer and the controlPointBuffer together as vertex buffer (following this order).
			Buffer* tessBuffers[] = { pTessFactorsBuffer, pHullOutputBuffer };
			const uint32_t strides[] = { sizeof(Blade), sizeof(HullOut) };
			cmdBindVertexBuffer(cmd, 2, tessBuffers, strides, NULL);
#endif
			cmdExecuteIndirect(cmd, pIndirectCommandSignature, 1, pBladeNumBuffer, 0, NULL, 0);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		BufferBarrier uavBarriers[] = {
			{ pBladeNumBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pCulledBladeStorageBuffer, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, RESOURCE_STATE_UNORDERED_ACCESS },
		};
		cmdResourceBarrier(cmd, 2, uavBarriers, 0, NULL, 0, NULL);

		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);

		endCmd(cmd);

		allCmds.push_back(cmd);

		// Draw UI
		cmd = pUICmds[gFrameIndex];
		beginCmd(cmd);

		LoadActionsDesc loadActionLoad = {};
		loadActionLoad.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActionLoad, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        float2 txtSize = cmdDrawCpuProfile(cmd, float2(8, 15), &gFrameTimeDraw);
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

        cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 30.f), gGpuProfileToken, &gFrameTimeDraw);

		cmdDrawProfilerUI();

		gAppUI.Gui(pGui);
		gAppUI.Draw(cmd);
		cmdEndDebugMarker(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		endCmd(cmd);
		allCmds.push_back(cmd);

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
		depthRT.mClearValue = {{0.0f, 0}};
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
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
};

DEFINE_APPLICATION_MAIN(Tessellation)
