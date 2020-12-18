/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"


///Demo structures
//General
VirtualJoystickUI		gVirtualJoystick;
ProfileToken			gGpuProfileToken						= PROFILE_INVALID_TOKEN;
GuiComponent*			pGuiWindow;

const uint32_t			gImageCount								= 3;
uint32_t				gFrameIndex								= 0;
bool					bToggleVSync							= false;
bool					bToggleYCbCr							= true;

Queue*					pGraphicsQueue							= NULL;
CmdPool*				pCmdPools[gImageCount]					= {NULL};
Cmd*					pCmds[gImageCount]						= {NULL};
Renderer*				pRenderer								= NULL;

SwapChain*				pSwapChain								= NULL;
Fence*					pRenderCompleteFences[gImageCount]		= {NULL};
Semaphore*				pImageAcquiredSemaphore					= NULL;
Semaphore*				pRenderCompleteSemaphores[gImageCount]	= {NULL};


//UI
UIApp					gAppUI;
TextDrawDesc			gFrameTimeDraw								= TextDrawDesc(0, 0xff00ffff, 18);

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
	 YUV()
	 {
		bToggleVSync = mSettings.mDefaultVSyncEnabled;
	 }

	bool Init()
	{
		// File paths
		{
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_SHADER_SOURCES,  "Shaders");
			fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,	RD_SHADER_BINARIES, "CompiledShaders");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_GPU_CONFIG,		"GPUCfg");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_TEXTURES,		"Textures");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,  RD_MESHES,			"Meshes");
			fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT,	RD_FONTS,			"Fonts");
		}

		if (!initInputSystem(pWindow))
			return false;

		// App Actions
		{
			InputActionDesc actionDesc = {InputBindings::BUTTON_DUMP, [](InputActionContext *ctx) {  dumpProfileData(((Renderer*)ctx->pUserData), ((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
			addInputAction(&actionDesc);
			actionDesc = {InputBindings::BUTTON_FULLSCREEN, [](InputActionContext *ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
			addInputAction(&actionDesc);
			actionDesc = {InputBindings::BUTTON_EXIT, [](InputActionContext *ctx) { requestShutdown(); return true; }};
			addInputAction(&actionDesc);
			actionDesc = {
				InputBindings::BUTTON_ANY, [](InputActionContext *ctx) {
					bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
					setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
					return true;
				}, this
			};
			addInputAction(&actionDesc);
		}

		return true;
	}

	void Exit()
	{
		exitInputSystem();
	}

	bool Load()
	{
		if (mSettings.mResetGraphics || !pRenderer) 
		{
			// Renderer initialization
			{
				// Window and renderer setup
				RendererDesc settings = { 0 };
				initRenderer(GetName(), &settings, &pRenderer);

				// Check for init success
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

				if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
				{
					LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
					return false;
				}
			}

			// Setting up the resources
			{
#if defined(VULKAN)
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
					textureDesc.pVkSamplerYcbcrConversionInfo = &pYCbCrSampler->mVkSamplerYcbcrConversionInfo;
					loadDesc = {};
					loadDesc.pFileName = "test_224x116_IYUV";
					loadDesc.mContainer = TEXTURE_CONTAINER_DDS;
					loadDesc.pDesc = &textureDesc;
					loadDesc.ppTexture = &pYCbCrTexture;
					addResource(&loadDesc, NULL);
				}
#endif
			}

			waitForAllResourceLoads();

			// Init
			{
#if defined(VULKAN)
			// Simple
				{
					ShaderLoadDesc shader = {};
					shader.mStages[0] = { "basic.vert", NULL, 0 };
					shader.mStages[1] = { "basic.frag", NULL, 0 };
					addShader(pRenderer, &shader, &pSimpleShader);
					Shader* shaders[] = { pSimpleShader };

					RootSignatureDesc rootDesc = {};
					rootDesc.mStaticSamplerCount = 1;
					rootDesc.ppStaticSamplerNames = &pSamplerName;
					rootDesc.ppStaticSamplers = &pSimpleSampler;
					rootDesc.mShaderCount = 1;
					rootDesc.ppShaders = shaders;
					addRootSignature(pRenderer, &rootDesc, &pSimpleRootSignature);

					DescriptorSetDesc desc = { pSimpleRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
					addDescriptorSet(pRenderer, &desc, &pSimpleDescriptorSets_NoneFreq);
				}

				// YCbCr
				{
					ShaderLoadDesc shader = {};
					shader.mStages[0] = { "basic.vert", NULL, 0 };
					shader.mStages[1] = { "basic.frag", NULL, 0 };
					addShader(pRenderer, &shader, &pYCbCrShader);
					Shader* shaders[] = { pYCbCrShader };

					RootSignatureDesc rootDesc = {};
					rootDesc.mStaticSamplerCount = 1;
					rootDesc.ppStaticSamplerNames = &pSamplerName;
					rootDesc.ppStaticSamplers = &pYCbCrSampler;
					rootDesc.mShaderCount = 1;
					rootDesc.ppShaders = shaders;
					addRootSignature(pRenderer, &rootDesc, &pYCbCrRootSignature);

					DescriptorSetDesc desc = { pYCbCrRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
					addDescriptorSet(pRenderer, &desc, &pYCbCrDescriptorSets_NoneFreq);
				}
#endif
			}

			if (!gAppUI.Init(pRenderer))
				return false;

			gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

			// Initialize microprofiler and it's UI.
			initProfiler();

			// Gpu profiler can only be added after initProfile.
			gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

			// GUI - coppied from the first unit test
			{
				GuiDesc guiDesc = {};
				guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
				pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
#if !defined(TARGET_IOS)
				pGuiWindow->AddWidget(CheckboxWidget("Toggle VSync\t\t\t\t\t", &bToggleVSync));
#endif
#if defined(VULKAN)
				pGuiWindow->AddWidget(CheckboxWidget("Toggle YCbCr conversion\t\t\t\t\t", &bToggleYCbCr));
#else
				pGuiWindow->AddWidget(LabelWidget("The selected API is not support by YCbCr conversion."));
#endif
			}
		}

		if (!addSwapChain())
			return false;

		// Load
		{
#if defined(VULKAN)
			// Create the pipeline
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

			// Prepare descriptor sets
			{	
				// No freq
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
#endif
		}

		if (!gAppUI.Load(pSwapChain->ppRenderTargets, 1))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppRenderTargets[0]))
			return false;

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);
		gAppUI.Unload();
		gVirtualJoystick.Unload();

		// Unload
		{
#if defined(VULKAN)
			removePipeline(pRenderer, pSimplePipeline);
			removePipeline(pRenderer, pYCbCrPipeline);
#endif
			removeSwapChain(pRenderer, pSwapChain);
		}

		if (mSettings.mQuit || mSettings.mResetGraphics) 
		{
			gVirtualJoystick.Exit();
			gAppUI.Exit();
			exitProfiler();

			{
#if defined(VULKAN)
				removeDescriptorSet(pRenderer, pSimpleDescriptorSets_NoneFreq);
				removeDescriptorSet(pRenderer, pYCbCrDescriptorSets_NoneFreq);

				removeShader(pRenderer, pSimpleShader);
				removeShader(pRenderer, pYCbCrShader);
				removeRootSignature(pRenderer, pSimpleRootSignature);
				removeRootSignature(pRenderer, pYCbCrRootSignature);

				// Remove the combined sampler
				{
					removeResource(pSimpleTexture);
					removeSampler(pRenderer, pSimpleSampler);
					removeResource(pYCbCrTexture);
					removeSampler(pRenderer, pYCbCrSampler);
				}
#endif
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
			removeRenderer(pRenderer);
		}
	}

	void Update(float deltaTime)
	{
#if !defined(TARGET_IOS)
		if (pSwapChain->mEnableVsync != bToggleVSync)
		{
			 waitQueueIdle(pGraphicsQueue);
			 gFrameIndex = 0;
			 ::toggleVSync(pRenderer, &pSwapChain);
		}
#endif
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		
		// Update GUI
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
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
			{
#if defined(VULKAN)
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
#endif			
			}
			
			// UI and finilization
			{
#if !defined(VULKAN)
				// Resources barriers
				{
					RenderTargetBarrier barriers[] =
					{
						{ pSwapChain->ppRenderTargets[swapchainImageIndex], RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
					};

					cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
				}
#endif
				LoadActionsDesc loadActions = {};
				loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
				cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
				
				gVirtualJoystick.Draw(cmd, {1.0f, 1.0f, 1.0f, 1.0f});
				const float txtIndent = 8.f;
				float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
				cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 30.f), gGpuProfileToken, &gFrameTimeDraw);
				cmdDrawProfilerUI();
				gAppUI.Gui(pGuiWindow);
				gAppUI.Draw(cmd);
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
			PresentStatus presentStatus = queuePresent(pGraphicsQueue, &presentDesc);
			flipProfiler();

			if (presentStatus == PRESENT_STATUS_DEVICE_RESET)
			{
				Thread::Sleep(5000);// Wait for a few seconds to allow the driver to come back online before doing a reset.
				mSettings.mResetGraphics = true;
			}
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
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		
		return NULL != pSwapChain;
	}

	const char* GetName() { return "33_YUV"; }
};

DEFINE_APPLICATION_MAIN(YUV)
