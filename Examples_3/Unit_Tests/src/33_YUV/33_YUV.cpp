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

//Unit Test for YUV/YCbCr formats
// This format is only supportted on Vulkan Renderer for now.


//Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

//Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"


///Demo structures
//General
ProfileToken			gGpuProfileToken						= PROFILE_INVALID_TOKEN;
UIComponent*			pGuiWindow;

const uint32_t			gImageCount								= 3;
uint32_t				gFrameIndex								= 0;
bool					bToggleYCbCr							= true;
bool					bYCbCrSupported							= false;

Queue*					pGraphicsQueue							= NULL;
CmdPool*				pCmdPools[gImageCount]					= {NULL};
Cmd*					pCmds[gImageCount]						= {NULL};
Renderer*				pRenderer								= NULL;

SwapChain*				pSwapChain								= NULL;
Fence*					pRenderCompleteFences[gImageCount]		= {NULL};
Semaphore*				pImageAcquiredSemaphore					= NULL;
Semaphore*				pRenderCompleteSemaphores[gImageCount]	= {NULL};


//UI
FontDrawDesc			gFrameTimeDraw;
uint32_t				gFontID										= 0; 

//Main parts
Pipeline*				pSimplePipeline								= NULL; // To demonstrate the YUV image wihtout any conversion
RootSignature*			pSimpleRootSignature						= NULL;
Sampler*				pSimpleSampler								= NULL;
Texture*				pSimpleTexture								= NULL;
DescriptorSet*			pSimpleDescriptorSets_NoneFreq				= {NULL};
Shader*					pSimpleShader								= NULL;

Pipeline*				pYCbCrPipeline								= NULL;
RootSignature*			pYCbCrRootSignature							= NULL;
Sampler*				pYCbCrSampler								= NULL;
Texture*				pYCbCrTexture								= NULL;
DescriptorSet*			pYCbCrDescriptorSets_NoneFreq				= {NULL};
Shader*					pYCbCrShader								= NULL;

const char*				pSamplerName								= {"uYCbCrSampler"};

class YUV : public IApp
{
public:
	bool Init()
	{
		// File paths
		{
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_SHADER_SOURCES,  "Shaders");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_SHADER_BINARIES, "CompiledShaders");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_GPU_CONFIG,		"GPUCfg");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_TEXTURES,		"Textures");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,  RD_MESHES,			"Meshes");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_FONTS,			"Fonts");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,  RD_SCRIPTS,			"Scripts");
		}

		// Renderer initialization
		{
			RendererDesc settings;
			memset(&settings, 0, sizeof(settings));
			initRenderer(GetName(), &settings, &pRenderer);

			// Check for init success
			if (!pRenderer)
				return false;

			bYCbCrSupported = pRenderer->pCapBits->canShaderReadFrom[TinyImageFormat_G8_B8_R8_3PLANE_420_UNORM];

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
		}

		// Setting up the resources
		if (bYCbCrSupported)
		{

			// Create the samplers
			{
				SamplerDesc samplerDesc =
				{
					FILTER_LINEAR,
					FILTER_LINEAR,
					MIPMAP_MODE_NEAREST,
					ADDRESS_MODE_REPEAT,
					ADDRESS_MODE_REPEAT,
					ADDRESS_MODE_REPEAT,
				};
				addSampler(pRenderer, &samplerDesc, &pSimpleSampler);


				// YCbCr conversion
				samplerDesc =
				{
					FILTER_LINEAR,
					FILTER_LINEAR,
					MIPMAP_MODE_NEAREST,
					ADDRESS_MODE_CLAMP_TO_EDGE,
					ADDRESS_MODE_CLAMP_TO_EDGE,
					ADDRESS_MODE_CLAMP_TO_EDGE,
				};
				samplerDesc.mSamplerConversionDesc =
				{
					TinyImageFormat_G8_B8_R8_3PLANE_420_UNORM,
					SAMPLER_MODEL_CONVERSION_YCBCR_709,
					SAMPLER_RANGE_NARROW,
					SAMPLE_LOCATION_MIDPOINT,
					SAMPLE_LOCATION_MIDPOINT,
					FILTER_LINEAR,
					false,
				};
				addSampler(pRenderer, &samplerDesc, &pYCbCrSampler);
			}

			// Create the textures
			{
				TextureLoadDesc loadDesc = {};
				loadDesc.pFileName = "test_224x116_IYUV";
				loadDesc.mContainer = TEXTURE_CONTAINER_DDS;
				loadDesc.ppTexture = &pSimpleTexture;
				addResource(&loadDesc, NULL);

				TextureDesc textureDesc = {};
				textureDesc.pVkSamplerYcbcrConversionInfo = &pYCbCrSampler->mVulkan.mVkSamplerYcbcrConversionInfo;
				loadDesc = {};
				loadDesc.pFileName = "test_224x116_IYUV";
				loadDesc.mContainer = TEXTURE_CONTAINER_DDS;
				loadDesc.pDesc = &textureDesc;
				loadDesc.ppTexture = &pYCbCrTexture;
				addResource(&loadDesc, NULL);
			}
		}

		waitForAllResourceLoads();

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

		// Gpu profiler can only be added after initProfile.
		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		// GUI - coppied from the first unit test
		{
			UIComponentDesc guiDesc = {};
			guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
			uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

			if (bYCbCrSupported)
			{
				CheckboxWidget yCbCrToggle;
				yCbCrToggle.pData = &bToggleYCbCr;
				luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Toggle YCbCr conversion\t\t\t\t\t", &yCbCrToggle, WIDGET_TYPE_CHECKBOX));
			}
			else
			{
				LabelWidget notSupportedLabel;
				luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "The selected API is not support by YCbCr conversion.", &notSupportedLabel, WIDGET_TYPE_LABEL));
			}

		}

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
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		exitUserInterface();

		exitFontSystem();

		exitProfiler();

		if (bYCbCrSupported)
		{
			removeResource(pSimpleTexture);
			removeSampler(pRenderer, pSimpleSampler);
			removeResource(pYCbCrTexture);
			removeSampler(pRenderer, pYCbCrSampler);
		}


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
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime)
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);
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
		
		RenderTarget*	pRenderTarget					= pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*		pRenderCompleteSemaphore	= pRenderCompleteSemaphores[gFrameIndex];
		Fence*				pRenderCompleteFence			= pRenderCompleteFences[gFrameIndex];
		
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (FENCE_STATUS_INCOMPLETE == fenceStatus)
			 waitForFences(pRenderer, 1, &pRenderCompleteFence);
		
		// Reset cmd pool for this frame
		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);
		
		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);
		{
			cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

			// Draw
			if (bYCbCrSupported)
			{
				// Resources barriers
				{
					RenderTargetBarrier barriers[] =
					{
						{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
					};
			
					cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
				}
			
				// Clear
				{
					LoadActionsDesc loadActions = {};
					loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
					cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Main Pass");
					cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
					cmdSetViewport(cmd, 0.0f, 0.0f, float(pRenderTarget->mWidth), float(pRenderTarget->mHeight), 0.0f, 1.0f);
					cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
				}
			
				// Draw
				{
					cmdBindPipeline(cmd, bToggleYCbCr ? pYCbCrPipeline : pSimplePipeline);
					cmdBindDescriptorSet(cmd, 0, bToggleYCbCr ? pYCbCrDescriptorSets_NoneFreq : pSimpleDescriptorSets_NoneFreq);
					cmdDraw(cmd, 3, 0);
				}
			
				cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);
				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}
			
			// UI and finilization
			{
				if (!bYCbCrSupported)
				{
					// Resources barriers
					{
						RenderTargetBarrier barriers[] =
						{
							{ pSwapChain->ppRenderTargets[swapchainImageIndex], RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
						};

						cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
					}
				}
				LoadActionsDesc loadActions = {};
				loadActions.mLoadActionsColor[0] = bYCbCrSupported ? LOAD_ACTION_LOAD : LOAD_ACTION_CLEAR;
				cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

				const float txtIndent = 8.f;

				gFrameTimeDraw.mFontColor = 0xff00ffff;
				gFrameTimeDraw.mFontSize = 18.0f;
				gFrameTimeDraw.mFontID = gFontID;
				float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
				cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 75.0f), gGpuProfileToken, &gFrameTimeDraw);

				cmdDrawUserInterface(cmd);

				cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			
			RenderTargetBarrier barriers[] =
			{
				{ pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
			}
		}
		endCmd(cmd);

		// Sumbit and present the queue
		{
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
		}

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

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
		
		return NULL != pSwapChain;
	}

	void addDescriptorSets()
	{
		if (bYCbCrSupported)
		{
			DescriptorSetDesc desc = { pSimpleRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &desc, &pSimpleDescriptorSets_NoneFreq);

			DescriptorSetDesc descYCbCr = { pYCbCrRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &descYCbCr, &pYCbCrDescriptorSets_NoneFreq);
		}
	}

	void removeDescriptorSets()
	{
		if (bYCbCrSupported)
		{
			removeDescriptorSet(pRenderer, pSimpleDescriptorSets_NoneFreq);
			removeDescriptorSet(pRenderer, pYCbCrDescriptorSets_NoneFreq);
		}
	}

	void addRootSignatures()
	{
		if (bYCbCrSupported)
		{
			Shader* shaders[] = { pSimpleShader };
			RootSignatureDesc rootDesc = {};
			rootDesc.mStaticSamplerCount = 1;
			rootDesc.ppStaticSamplerNames = &pSamplerName;
			rootDesc.ppStaticSamplers = &pSimpleSampler;
			rootDesc.mShaderCount = 1;
			rootDesc.ppShaders = shaders;
			addRootSignature(pRenderer, &rootDesc, &pSimpleRootSignature);


			Shader* shadersYCbCr[] = { pYCbCrShader };

			rootDesc.ppStaticSamplers = &pYCbCrSampler;
			rootDesc.ppShaders = shadersYCbCr;
			addRootSignature(pRenderer, &rootDesc, &pYCbCrRootSignature);
		}
	}

	void removeRootSignatures()
	{
		if (bYCbCrSupported)
		{
			removeRootSignature(pRenderer, pSimpleRootSignature);
			removeRootSignature(pRenderer, pYCbCrRootSignature);
		}
	}

	void addShaders()
	{
		if (bYCbCrSupported)
		{
			ShaderLoadDesc shader = {};
			shader.mStages[0] = { "basic.vert", NULL, 0 };
			shader.mStages[1] = { "basic.frag", NULL, 0 };
			addShader(pRenderer, &shader, &pSimpleShader);

			shader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			shader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &shader, &pYCbCrShader);
		}
	}

	void removeShaders()
	{
		if (bYCbCrSupported)
		{
			removeShader(pRenderer, pSimpleShader);
			removeShader(pRenderer, pYCbCrShader);
		}
	}

	void addPipelines()
	{
		if (bYCbCrSupported)
		{
			RasterizerStateDesc rasterizerStateDesc = {};
			rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

			PipelineDesc pipelineDesc = {};
			pipelineDesc.pName = "Main Pipeline";
			pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;

			GraphicsPipelineDesc & graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
			graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			graphicsPipelineDesc.mRenderTargetCount = 1;
			graphicsPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
			graphicsPipelineDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
			graphicsPipelineDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
			graphicsPipelineDesc.pVertexLayout = NULL;
			graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;

			graphicsPipelineDesc.pShaderProgram = pSimpleShader;
			graphicsPipelineDesc.pRootSignature = pSimpleRootSignature;
			addPipeline(pRenderer, &pipelineDesc, &pSimplePipeline);

			graphicsPipelineDesc.pShaderProgram = pYCbCrShader;
			graphicsPipelineDesc.pRootSignature = pYCbCrRootSignature;
			addPipeline(pRenderer, &pipelineDesc, &pYCbCrPipeline);
		}
	}

	void removePipelines()
	{
		if (bYCbCrSupported)
		{
			removePipeline(pRenderer, pSimplePipeline);
			removePipeline(pRenderer, pYCbCrPipeline);
		}
	}
	
	void prepareDescriptorSets()
	{
		if (bYCbCrSupported)
		{
			constexpr uint32_t PARAMS_COUNT = 1;
			DescriptorData params[PARAMS_COUNT] = {};
			params[0].pName = "uYCbCrSampler";
			params[0].ppTextures = &pSimpleTexture;
			params[0].mCount = 1;
			updateDescriptorSet(pRenderer, 0, pSimpleDescriptorSets_NoneFreq, PARAMS_COUNT, params);

			params[0].ppTextures = &pYCbCrTexture;
			updateDescriptorSet(pRenderer, 0, pYCbCrDescriptorSets_NoneFreq, PARAMS_COUNT, params);
		}
	}

	const char* GetName() { return "33_YUV"; }
};

DEFINE_APPLICATION_MAIN(YUV)
