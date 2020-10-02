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

/********************************************************************************************************
*
* The Forge - AUDIO UNIT TEST
*
* The purpose of this demo is to show how to use the audio library used by The Forge,
* as well as ensuring that library is properly integrated (using Forge FS, mem allocator, etc...).
*
*********************************************************************************************************/

//Interfaces
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"

// Rendering
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

// Middleware packages
#include "../../../../Middleware_3/UI/AppUI.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Audio
#include "soloud.h"
#include "soloud_speech.h"
#include "soloud_fftfilter.h"
#include "soloud_wav.h"

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t gImageCount = 3;
Renderer*      pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain*    pSwapChain = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;

UIApp gAppUI;
GuiComponent* pStandaloneControlsGUIWindow = NULL;


//--------------------------------------------------------------------------------------------
// AUDIO DATA
const char* gWavBgTestFile = "test.wav";
const char* gOggWarNoiseFile = "war_loop.ogg";

SoLoud::handle gSpeechHandle = 0;
SoLoud::handle gBgWavHandle = 0;
SoLoud::handle gWarWavHandle = 0;

struct GlobalAudio
{
	SoLoud::Soloud mSoLoud;  // SoLoud engine core
	SoLoud::FFTFilter mFftFilter;

	SoLoud::Speech mSpeech;  // A sound source (speech, in this case)
	SoLoud::Wav    mBgWavObj;
	SoLoud::Wav    mWarWavObj;
};

GlobalAudio* pGlobal = NULL;

char gSpeechText[512] = "Hello, this is an audio unit test.";
bool gSpeechWetFilter = false;
bool gBackGroundAudioOn = true;
float gGlobalVolume = 1.f;
float gWarWavPan = 0.f;
float gSpeechPan = 0.f;
bool gUseSpeechWetFilter = false;


static void SetGlobalVolume()
{
	pGlobal->mSoLoud.setGlobalVolume(gGlobalVolume);
}

static void ToggleBgAudio()
{
	gBackGroundAudioOn = !gBackGroundAudioOn;
	if (gBackGroundAudioOn)
	{
		gBgWavHandle = pGlobal->mSoLoud.playBackground(pGlobal->mBgWavObj);
		pGlobal->mSoLoud.setLooping(gBgWavHandle, true);
	}
	else
	{
		pGlobal->mSoLoud.stop(gBgWavHandle);
		gBgWavHandle = 0;
	}
}

static void PlayWarAudio()
{
	if (gWarWavHandle != 0)
	{
		pGlobal->mSoLoud.stop(gWarWavHandle);
		gWarWavHandle = 0;
	}

	gWarWavHandle = pGlobal->mSoLoud.play(pGlobal->mWarWavObj, -1.f, gWarWavPan);
}

static void SetWarAudioPan()
{
	if (gWarWavHandle != 0)
	{
		pGlobal->mSoLoud.setPan(gWarWavHandle, gWarWavPan);
	}
}

static void SetSpeechFilter()
{
	pGlobal->mSpeech.setFilter(1, gUseSpeechWetFilter ? &pGlobal->mFftFilter : NULL);
}

static void PlaySpeechAudio()
{
	if (gSpeechHandle != 0)
	{
		pGlobal->mSoLoud.stop(gSpeechHandle);
		gSpeechHandle = 0;
	}

	pGlobal->mSpeech.setText(gSpeechText);
	gSpeechHandle = pGlobal->mSoLoud.play(pGlobal->mSpeech, -1.f, gSpeechPan);
	SetSpeechFilter();
}

static void SetSpeechAudioPan()
{
	if (gSpeechHandle != 0)
	{
		pGlobal->mSoLoud.setPan(gSpeechHandle, gSpeechPan);
	}
}

//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class AudioUnitTest : public IApp
{
public:
	bool Init()
	{
        // FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,	"Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES,	"CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,		"GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,			"Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,			"Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_AUDIO,			"Audio");
        
		// WINDOW AND RENDERER SETUP
		//
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		if (!pRenderer)    //check for init success
			return false;

		// CREATE COMMAND LIST AND GRAPHICS/COMPUTE QUEUES
		//
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

		// Resource loader 
		initResourceLoaderInterface(pRenderer);

		// INITIALIZE THE USER INTERFACE
		//
		if (!gAppUI.Init(pRenderer))
		{
			// todo: display err msg (multiplatform?)
			return false;
		}

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

		// Add the GUI Panels/Windows
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 16 };
				
		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.15f };
		vec2    UIPanelSize = vec2(650.f, 1000.f);
		GuiDesc guiDesc(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		pStandaloneControlsGUIWindow = gAppUI.AddGuiComponent("AUDIO", &guiDesc);

		SliderFloatWidget globalAudioSlider("Volume", &gGlobalVolume, 0.f, 1.f, 0.05f);
		globalAudioSlider.pOnEdited = SetGlobalVolume;

		ButtonWidget toggleBgAudioButton("Toggle Background Audio");
		toggleBgAudioButton.pOnDeactivatedAfterEdit = ToggleBgAudio;

		SliderFloatWidget warAudioPanSlider("Pan", &gWarWavPan, -1.f, 1.f, 0.05f);
		warAudioPanSlider.pOnEdited = SetWarAudioPan;

		ButtonWidget playWarAudioButton("Trigger War Noises!");
		playWarAudioButton.pOnDeactivatedAfterEdit = PlayWarAudio;


		SliderFloatWidget speechAudioPanSlider("Pan", &gSpeechPan, -1.f, 1.f, 0.05f);
		speechAudioPanSlider.pOnEdited = SetSpeechAudioPan;

		CheckboxWidget wetFilterCheckbox("Apply Wet Filter", &gUseSpeechWetFilter);
		wetFilterCheckbox.pOnDeactivatedAfterEdit = SetSpeechFilter;

		TextboxWidget speechTextbox("Speech Text", gSpeechText, 512);

		ButtonWidget playSpeechAudioButton("Play Speech Text!");
		playSpeechAudioButton.pOnDeactivatedAfterEdit = PlaySpeechAudio;
				
		{				
			pStandaloneControlsGUIWindow->AddWidget(LabelWidget("GLOBALS"));
			pStandaloneControlsGUIWindow->AddWidget(globalAudioSlider);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(LabelWidget("BACKGROUND AUDIO"));
			pStandaloneControlsGUIWindow->AddWidget(toggleBgAudioButton);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(LabelWidget("AUDIO TRIGGER"));
			pStandaloneControlsGUIWindow->AddWidget(warAudioPanSlider);
			pStandaloneControlsGUIWindow->AddWidget(playWarAudioButton);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(LabelWidget("SPEECH AUDIO"));
			pStandaloneControlsGUIWindow->AddWidget(speechAudioPanSlider);
			pStandaloneControlsGUIWindow->AddWidget(wetFilterCheckbox);
			pStandaloneControlsGUIWindow->AddWidget(speechTextbox);
			pStandaloneControlsGUIWindow->AddWidget(playSpeechAudioButton);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());		
		}

		// Init the audio data
		pGlobal = tf_new(GlobalAudio);
		pGlobal->mSoLoud.init();
				
		SoLoud::result res = pGlobal->mBgWavObj.load(gWavBgTestFile);
		ASSERT(res == SoLoud::SO_NO_ERROR);
		res = pGlobal->mWarWavObj.load(gOggWarNoiseFile);
		ASSERT(res == SoLoud::SO_NO_ERROR);		

		pGlobal->mSoLoud.setGlobalVolume(gGlobalVolume);

		gBgWavHandle = pGlobal->mSoLoud.playBackground(pGlobal->mBgWavObj);
		pGlobal->mSoLoud.setLooping(gBgWavHandle, true);

		if (!initInputSystem(pWindow))
			return false;

		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_ANY, [](InputActionContext* ctx) { return gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition); } };
		addInputAction(&actionDesc);

		return true;
	}

	void Exit()
	{
		// wait for rendering to finish before freeing resources
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		pGlobal->mSoLoud.stopAll();
		pGlobal->mSoLoud.deinit();

		gAppUI.Exit();

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

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		removeRenderer(pRenderer);

		tf_delete(pGlobal);
	}

	bool Load()
	{
		// INITIALIZE SWAP-CHAIN AND DEPTH BUFFER
		//
		if (!addSwapChain())
			return false;

		// LOAD USER INTERFACE
		//
		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		/************************************************************************/
		// GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		static HiresTimer gTimer;
		ClearValue  clearVal;
		clearVal.r = 0.0f;
		clearVal.g = 0.0f;
		clearVal.b = 0.0f;
		clearVal.a = 1.0f;

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		// FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
		//
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		// Acquire the main render target from the swapchain
		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];
		Cmd*          cmd = pCmds[gFrameIndex];
		beginCmd(cmd);    // start recording commands

		RenderTargetBarrier barriers[] =    // wait for resource transition
		{
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		// bind and clear the render target
		LoadActionsDesc loadActions = {};    // render target clean command
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = clearVal;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		// DRAW THE USER INTERFACE
		//
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		gTimer.GetUSec(true);

		gAppUI.Gui(pStandaloneControlsGUIWindow);    // adds the gui element to AppUI::ComponentsToUpdate list
		TextDrawDesc textDrawDesc(0, 0xff00dddd, 18);
		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &textDrawDesc);
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		// PRESENT THE GRPAHICS QUEUE
		//
		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
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

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "31_Audio"; }

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
};

DEFINE_APPLICATION_MAIN(AudioUnitTest)
