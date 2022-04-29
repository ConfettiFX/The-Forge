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

// Unit Test for Input.

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IFont.h"
#include "../../../../Common_3/OS/Interfaces/IUI.h"

//Renderer
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/OS/Interfaces/IMemory.h"

struct VertexDesc
{
	float4 mPosition;
	float2 mTexcoord;
};

#define COUNT_OF(a) (sizeof(a) / sizeof(a[0]))

const uint32_t gImageCount = 3;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount] = { NULL };
Cmd*     pCmds[gImageCount] = { NULL };

SwapChain*    pSwapChain = NULL;
Fence*        pRenderCompleteFences[gImageCount] = {};
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = {};

Shader* pBasicShader = NULL;
Pipeline* pBasicPipeline = NULL;

Buffer* pJoystickBuffer = NULL;
Buffer* pElementBuffer = NULL;
Buffer* pQuadIndexBuffer = NULL;
Buffer* scaleBuffer[gImageCount] = { NULL };

RootSignature* pRootSignature = NULL;
Sampler*       pSampler = NULL;
Texture*       pJoystick = NULL;
Texture*       pAxis = NULL;
Texture*       pButton = NULL;
Texture*       pLight = NULL;
DescriptorSet* pDescriptorJoystick = NULL;
DescriptorSet* pDescriptorAxis = NULL;
DescriptorSet* pDescriptorButton = NULL;
DescriptorSet* pDescriptorLight = NULL;

uint32_t gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

/// UI
UIComponent* pGuiWindow = NULL;

uint32_t gFontID = 0;

FontDrawDesc gFrameTimeDraw = FontDrawDesc{NULL, 0, 0xff00ffff, 18};

VertexDesc gJoystickVerts[4];
VertexDesc gElementVerts[4];

constexpr uint32_t gUniqueJoystickKeyCount = 19;
uint32_t gJoystickBindings[gUniqueJoystickKeyCount + 1];
float2 gJoystickElementPosition[gUniqueJoystickKeyCount + 1];
uint32_t gJoystickBindingsIterator = 0;
uint32_t gJoystickAxis = 0;
float2 gKeyColor = { 1.0f, 1.0f };
float gHalfPi = 3.14159f * 0.5f;
float gElementProjectionScale = 0.0f;
float gPlayColorTime = 0.0f;
float gPlayDuration = 0.3f;
bool gImageUseButton = true;
bool gGamePadPresent = false;

#define SET_PASS gKeyColor = { 0.0f, 1.0f }; gPlayColorTime = gPlayDuration
#define SET_FAIL gKeyColor = { 1.0f, 0.0f }; gPlayColorTime = gPlayDuration
#define SET_SKIP gKeyColor = { 0.7f, 0.7f }; gPlayColorTime = gPlayDuration

void Restart(void)
{
	gJoystickBindingsIterator = 0;
	gJoystickAxis = 0;
	gImageUseButton = true;
}

void KeyUpdater(float dt)
{
	gPlayColorTime -= dt;
	if (gPlayColorTime <= 0.0f)
	{
		gPlayColorTime = 0.0f;
		gKeyColor = { 1.0f, 1.0f };
	}

	if (gJoystickBindingsIterator == gUniqueJoystickKeyCount)
	{
		Restart();
	}
}

bool StickChecker(InputActionContext* ctx)
{
	if (!ctx->mBool && (ctx->mFloat2.x == 0.0f && ctx->mFloat2.y == 0.0f))
	{
		return true;
	}

	if (!*ctx->pCaptured)
	{
		return true;
	}

	uint32_t binding = *(uint32_t*)ctx->pUserData;
	if (binding == gJoystickBindings[gJoystickBindingsIterator])
	{
		if (fabs(ctx->mFloat2.x) > fabs(ctx->mFloat2.y))
		{
			if (ctx->mFloat2.x < -0.0f && gJoystickAxis == 1)
			{
				++gJoystickAxis;
				SET_PASS;
				return true;
			}
			else if (ctx->mFloat2.x > 0.0f && gJoystickAxis == 3)
			{
				++gJoystickBindingsIterator;
				gJoystickAxis = 0;
				SET_PASS;
				return true;
			}
		}
		else
		{
			if (ctx->mFloat2.y > 0.0f && gJoystickAxis == 0)
			{
				++gJoystickAxis;
				SET_PASS;
				return true;
			}
			else if (ctx->mFloat2.y < -0.0f && gJoystickAxis == 2)
			{
				++gJoystickAxis;
				SET_PASS;
				return true;
			}
		}
	}

	if (gPlayColorTime == 0.0f)
	{
		SET_FAIL;
	}

	return true;
}

bool KeyChecker(InputActionContext* ctx)
{
	if (ctx->pUserData == nullptr || (!ctx->mBool && (ctx->mFloat2.x == 0.0f && ctx->mFloat2.y == 0.0f)))
	{
		return true;
	}

	if (ctx->mDeviceType == InputDeviceType::INPUT_DEVICE_KEYBOARD && ctx->mPhase == 1 && gJoystickBindingsIterator <= gUniqueJoystickKeyCount)
	{
		switch (ctx->mBinding)
		{
		case 15:
		{
			if (gJoystickBindingsIterator >= 17)
			{
				++gJoystickAxis;
				if (gJoystickAxis == 4)
				{
					gJoystickAxis = 0;
					++gJoystickBindingsIterator;
				}
			}
			else
			{
				++gJoystickBindingsIterator;
			}

			if (gJoystickBindingsIterator == 17)
			{
				gImageUseButton = false;
			}

			SET_SKIP;

			return true;
		}

		case 21:
		{
			SET_SKIP;
			Restart();

			return true;
		}

		default:
			return true;
		}
	}

	uint32_t binding = *(uint32_t*)ctx->pUserData;
	if ((ctx->mDeviceType == InputDeviceType::INPUT_DEVICE_GAMEPAD && ctx->mPhase == 1 && binding == gJoystickBindings[gJoystickBindingsIterator])
#if TARGET_IOS
        || gJoystickBindings[gJoystickBindingsIterator] == InputBindings::BUTTON_HOME
#endif
        )
	{
		++gJoystickBindingsIterator;
		SET_PASS;

		if (gJoystickBindingsIterator == 17)
		{
			gImageUseButton = false;
		}
		return true;
	}

	if (gPlayColorTime == 0.0f)
	{
		SET_FAIL;
	}

	return true;
};

class Input : public IApp
{
public:
	Input()
	{
		mSettings.mWidth = 512;
		mSettings.mHeight = 317;
		mSettings.mCentered = true;
		mSettings.mBorderlessWindow = false;
		mSettings.mDragToResize = false;
		mSettings.mForceLowDPI = true;
	}

	bool Init()
	{
		// FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SCREENSHOTS,     "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,         "Scripts");

		// window and renderer setup
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

			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		TextureLoadDesc textureDesc = {};
		textureDesc.pFileName = "controllermap";
		textureDesc.ppTexture = &pJoystick;
		addResource(&textureDesc, NULL);

		textureDesc.pFileName = "axis";
		textureDesc.ppTexture = &pAxis;
		addResource(&textureDesc, NULL);

		textureDesc.pFileName = "button";
		textureDesc.ppTexture = &pButton;
		addResource(&textureDesc, NULL);

		textureDesc.pFileName = "light";
		textureDesc.ppTexture = &pLight;
		addResource(&textureDesc, NULL);

		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0 };
		basicShader.mStages[1] = { "basic.frag", NULL, 0 };
		addShader(pRenderer, &basicShader, &pBasicShader);

		SamplerDesc samplerDesc = {};
		samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
		samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
		samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
		samplerDesc.mMinFilter = FILTER_LINEAR;
		samplerDesc.mMagFilter = FILTER_LINEAR;
		samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		addSampler(pRenderer, &samplerDesc, &pSampler);

		Shader*           shaders[] = { pBasicShader };
		const char*       pStaticSamplers[] = { "Sampler" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSampler;
		rootDesc.mShaderCount = COUNT_OF(shaders);
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &desc, &pDescriptorJoystick);
		addDescriptorSet(pRenderer, &desc, &pDescriptorAxis);
		addDescriptorSet(pRenderer, &desc, &pDescriptorButton);
		addDescriptorSet(pRenderer, &desc, &pDescriptorLight);

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

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        // Gpu profiler can only be added after initProfiler.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        if (!initInputSystem(&inputDesc))
            return false;

		waitForAllResourceLoads();

		uint32_t jStart = gUniqueJoystickKeyCount - 2;
		InputActionDesc actionDesc;
		for (uint32_t i = 0; i < jStart; ++i)
		{
			gJoystickBindings[i] = InputBindings::BUTTON_BINDINGS_BEGIN + i;
			actionDesc = { gJoystickBindings[i], KeyChecker, &gJoystickBindings[i] };
			addInputAction(&actionDesc);
		}

		actionDesc = { InputBindings::FLOAT_L2, KeyChecker, &gJoystickBindings[InputBindings::BUTTON_L2 - InputBindings::BUTTON_BINDINGS_BEGIN] };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_R2, KeyChecker, &gJoystickBindings[InputBindings::BUTTON_R2 - InputBindings::BUTTON_BINDINGS_BEGIN] };
		addInputAction(&actionDesc);

		gJoystickBindings[17] = InputBindings::FLOAT_LEFTSTICK;
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, StickChecker, &gJoystickBindings[17] };
		addInputAction(&actionDesc);
		gJoystickBindings[18] = InputBindings::FLOAT_RIGHTSTICK;
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, StickChecker, &gJoystickBindings[18] };
		addInputAction(&actionDesc);

		gJoystickBindings[19] = -1;
		actionDesc = { InputBindings::BUTTON_ANY, KeyChecker };
		addInputAction(&actionDesc);


		// Explicitly mapping like this for clarity. First InputBinding is the key, rest of arithmetics is offset.
		gJoystickElementPosition[InputBindings::BUTTON_DPAD_LEFT - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.4f, -0.5f };
		gJoystickElementPosition[InputBindings::BUTTON_DPAD_RIGHT - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.2f, -0.5f };
		gJoystickElementPosition[InputBindings::BUTTON_DPAD_UP - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.3f, -0.4f };
		gJoystickElementPosition[InputBindings::BUTTON_DPAD_DOWN - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.3f, -0.65f };

		gJoystickElementPosition[InputBindings::BUTTON_SOUTH - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.6f, -0.2f };
		gJoystickElementPosition[InputBindings::BUTTON_EAST - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.77f, 0.02f };
		gJoystickElementPosition[InputBindings::BUTTON_WEST - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.45f, -0.0f };
		gJoystickElementPosition[InputBindings::BUTTON_NORTH - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.62f, 0.22f };

		gJoystickElementPosition[InputBindings::BUTTON_L1 - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.56f, 0.65f };
		gJoystickElementPosition[InputBindings::BUTTON_R1 - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.56f, 0.65f };
		gJoystickElementPosition[InputBindings::BUTTON_L2 - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.56f, 0.85f };
		gJoystickElementPosition[InputBindings::BUTTON_R2 - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.56f, 0.85f };

		gJoystickElementPosition[InputBindings::BUTTON_L3 - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.62f, -0.1f };
		gJoystickElementPosition[InputBindings::BUTTON_R3 - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.3f, -0.6f };
		gJoystickElementPosition[InputBindings::BUTTON_HOME - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.0f, 0.0f };
		gJoystickElementPosition[InputBindings::BUTTON_START - InputBindings::BUTTON_BINDINGS_BEGIN] = { 0.2f, 0.0f };
		gJoystickElementPosition[InputBindings::BUTTON_SELECT - InputBindings::BUTTON_BINDINGS_BEGIN] = { -0.2f, -0.0f };

		gJoystickElementPosition[InputBindings::FLOAT_LEFTSTICK + gUniqueJoystickKeyCount - 2] = { -0.61f, -0.1f };
		gJoystickElementPosition[InputBindings::FLOAT_RIGHTSTICK + gUniqueJoystickKeyCount - 2] = { 0.3f, -0.61f };

		gJoystickElementPosition[gUniqueJoystickKeyCount] = { 0.0f, 0.0f };

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

        exitUserInterface();

        exitFontSystem();

		exitProfiler();


		removeDescriptorSet(pRenderer, pDescriptorLight);
		removeDescriptorSet(pRenderer, pDescriptorButton);
		removeDescriptorSet(pRenderer, pDescriptorAxis);
		removeDescriptorSet(pRenderer, pDescriptorJoystick);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(scaleBuffer[i]);
		}

		removeResource(pElementBuffer);
		removeResource(pJoystickBuffer);
		removeResource(pQuadIndexBuffer);
		removeResource(pLight);
		removeResource(pButton);
		removeResource(pAxis);
		removeResource(pJoystick);

		removeSampler(pRenderer, pSampler);
		removeShader(pRenderer, pBasicShader);
		removeRootSignature(pRenderer, pRootSignature);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
		pRenderer = NULL;
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

        RenderTarget* ppPipelineRenderTargets[] = {
            pSwapChain->ppRenderTargets[0]
        };

        if (!addFontSystemPipelines(ppPipelineRenderTargets, 1, NULL))
            return false;

        if (!addUserInterfacePipelines(ppPipelineRenderTargets[0]))
            return false;

		//layout and pipeline for sphere draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;

		DepthStateDesc depthStateDesc = {};

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pBasicShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pBasicPipeline);

		gJoystickVerts[0].mPosition = { 1.0f,  1.0f, 0.0f, 1.0f };
		gJoystickVerts[0].mTexcoord = { 1.0f, 0.0f };
		gJoystickVerts[1].mPosition = { -1.0f,  1.0f, 0.0f, 1.0f };
		gJoystickVerts[1].mTexcoord = { 0.0f, 0.0f };
		gJoystickVerts[2].mPosition = { -1.0f, -1.0f, 0.0f, 1.0f };
		gJoystickVerts[2].mTexcoord = { 0.0f, 1.0f };
		gJoystickVerts[3].mPosition = { 1.0f, -1.0f, 0.0f, 1.0f };
		gJoystickVerts[3].mTexcoord = { 1.0f, 1.0f };

		float elementWidth = 0.09f;
		float elementHeight = elementWidth;
		gElementProjectionScale = (float)mSettings.mWidth / mSettings.mHeight;

		gElementVerts[0].mPosition = { elementWidth,  elementHeight, 0.0f, 1.0f };
		gElementVerts[0].mTexcoord = { 1.0f, 0.0f };
		gElementVerts[1].mPosition = { -elementWidth,  elementHeight, 0.0f, 1.0f };
		gElementVerts[1].mTexcoord = { 0.0f, 0.0f };
		gElementVerts[2].mPosition = { -elementWidth, -elementHeight, 0.0f, 1.0f };
		gElementVerts[2].mTexcoord = { 0.0f, 1.0f };
		gElementVerts[3].mPosition = { elementWidth, -elementHeight, 0.0f, 1.0f };
		gElementVerts[3].mTexcoord = { 1.0f, 1.0f };

		uint32_t indices[] =
		{
				0, 1, 2, 0, 2, 3
		};

		BufferLoadDesc joystickVBDesc = {};
		joystickVBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		joystickVBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		joystickVBDesc.mDesc.mSize = sizeof(gJoystickVerts);
		joystickVBDesc.pData = gJoystickVerts;
		joystickVBDesc.ppBuffer = &pJoystickBuffer;
		addResource(&joystickVBDesc, NULL);

		waitForAllResourceLoads();

		BufferLoadDesc elementVBDesc = {};
		elementVBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		elementVBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		elementVBDesc.mDesc.mSize = sizeof(gElementVerts);
		elementVBDesc.pData = gElementVerts;
		elementVBDesc.ppBuffer = &pElementBuffer;
		addResource(&elementVBDesc, NULL);

		waitForAllResourceLoads();

		BufferLoadDesc ibDesc = {};
		ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc.mDesc.mSize = sizeof(indices);
		ibDesc.pData = indices;
		ibDesc.ppBuffer = &pQuadIndexBuffer;
		addResource(&ibDesc, NULL);

		waitForAllResourceLoads();

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(float2);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &scaleBuffer[i];
			addResource(&ubDesc, NULL);
		}

		waitForAllResourceLoads();

		DescriptorData param = {};
		param.pName = "Texture";
		param.ppTextures = &pJoystick;
		updateDescriptorSet(pRenderer, 0, pDescriptorJoystick, 1, &param);

		param.ppTextures = &pAxis;
		updateDescriptorSet(pRenderer, 0, pDescriptorAxis, 1, &param);

		param.ppTextures = &pButton;
		updateDescriptorSet(pRenderer, 0, pDescriptorButton, 1, &param);

		param.ppTextures = &pLight;
		updateDescriptorSet(pRenderer, 0, pDescriptorLight, 1, &param);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

        removeUserInterfacePipelines();

        removeFontSystemPipelines();

		removePipeline(pRenderer, pBasicPipeline);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		gGamePadPresent = gamePadConnected(0);
		KeyUpdater(deltaTime);
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

		// Reset cmd pool for this frame
		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		const uint32_t stride = 6 * sizeof(float);

		RenderTargetBarrier barriers[2];

		barriers[0] = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Basic Draw");

		cmdBindPipeline(cmd, pBasicPipeline);
		cmdBindIndexBuffer(cmd, pQuadIndexBuffer, INDEX_TYPE_UINT32, 0);

		struct KPC
		{
			float2 position = float2(0.0f, 0.0f);
			float2 color = float2(1.0f, 1.0f);
			float angle = 0.0f;
			float projectionScale = 1.0f;
		} kpc;

        uint32_t kpcRootConstant = getDescriptorIndexFromName(pRootSignature, "KPCRootConstant");

		cmdBindDescriptorSet(cmd, 0, pDescriptorJoystick);
		cmdBindPushConstants(cmd, pRootSignature, kpcRootConstant, &kpc);
		cmdBindVertexBuffer(cmd, 1, &pJoystickBuffer, &stride, NULL);
		cmdDrawIndexed(cmd, 6, 0, 0);

		kpc.position = { 0.0f, 0.5f };
		kpc.color = gGamePadPresent ? float2(0.0f, 1.0f) : float2(1.0f, 0.0f);

		cmdBindDescriptorSet(cmd, 0, pDescriptorLight);
		cmdBindPushConstants(cmd, pRootSignature, kpcRootConstant, &kpc);
		cmdBindVertexBuffer(cmd, 1, &pElementBuffer, &stride, NULL);
		cmdDrawIndexed(cmd, 6, 0, 0);

		kpc.position = gJoystickElementPosition[gJoystickBindingsIterator];
		kpc.color = gKeyColor;
		kpc.angle = gJoystickAxis * gHalfPi;
		kpc.projectionScale = gElementProjectionScale;
		DescriptorSet* element = gImageUseButton ? pDescriptorButton : pDescriptorAxis;

		cmdBindDescriptorSet(cmd, 0, element);
		cmdBindPushConstants(cmd, pRootSignature, kpcRootConstant, &kpc);
		cmdBindVertexBuffer(cmd, 1, &pElementBuffer, &stride, NULL);
		cmdDrawIndexed(cmd, 6, 0, 0);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		//cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
        //
		//const float txtIndent = 8.f;
		//float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
		//cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 30.f), gGpuProfileToken, &gFrameTimeDraw);
        //
        //cmdDrawUserInterface(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		//cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

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
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);

		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "34_Input"; }

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
};
DEFINE_APPLICATION_MAIN(Input)

