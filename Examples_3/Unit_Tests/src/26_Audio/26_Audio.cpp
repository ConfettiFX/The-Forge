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

// Rendering
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Middleware packages
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Audio
#include "soloud.h"
#include "soloud_speech.h"
#include "soloud_fftfilter.h"
#include "soloud_wav.h"

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"



const char* pszBases[FSR_Count] = {
	"../../../src/13_UserInterface/",       // FSR_BinShaders
	"../../../src/13_UserInterface/",       // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/13_UserInterface/",       // FSR_GpuConfig
	"",                                     // FSR_Animation
	"../../../UnitTestResources/",          // FSR_Audio
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t gImageCount = 3;
Renderer*      pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

DepthState* pDepth = NULL;

uint32_t gFrameIndex = 0;

UIApp gAppUI;
GuiComponent* pStandaloneControlsGUIWindow = NULL;


//--------------------------------------------------------------------------------------------
// AUDIO DATA
const char* gWavBgTestFile = "test.wav";
const char* gOggWarNoiseFile = "war_loop.ogg";

SoLoud::Soloud gSoLoud;  // SoLoud engine core
SoLoud::FFTFilter gFftFilter;

SoLoud::Speech gSpeech;  // A sound source (speech, in this case)
SoLoud::Wav    gBgWavObj;
SoLoud::Wav    gWarWavObj;
SoLoud::handle gSpeechHandle = 0;
SoLoud::handle gBgWavHandle = 0;
SoLoud::handle gWarWavHandle = 0;

char gSpeechText[512] = "Hello, this is an audio unit test.";
bool gSpeechWetFilter = false;
bool gBackGroundAudioOn = true;
float gGlobalVolume = 1.f;
float gWarWavPan = 0.f;
float gSpeechPan = 0.f;
bool gUseSpeechWetFilter = false;


static void SetGlobalVolume()
{
	gSoLoud.setGlobalVolume(gGlobalVolume);
}

static void ToggleBgAudio()
{
	gBackGroundAudioOn = !gBackGroundAudioOn;
	if (gBackGroundAudioOn)
	{
		gBgWavHandle = gSoLoud.playBackground(gBgWavObj);
		gSoLoud.setLooping(gBgWavHandle, true);
	}
	else
	{
		gSoLoud.stop(gBgWavHandle);
		gBgWavHandle = 0;
	}
}

static void PlayWarAudio()
{
	if (gWarWavHandle != 0)
	{
		gSoLoud.stop(gWarWavHandle);
		gWarWavHandle = 0;
	}

	gWarWavHandle = gSoLoud.play(gWarWavObj, -1.f, gWarWavPan);
}

static void SetWarAudioPan()
{
	if (gWarWavHandle != 0)
	{
		gSoLoud.setPan(gWarWavHandle, gWarWavPan);
	}
}

static void SetSpeechFilter()
{
	gSpeech.setFilter(1, gUseSpeechWetFilter ? &gFftFilter : NULL);
}

static void PlaySpeechAudio()
{
	if (gSpeechHandle != 0)
	{
		gSoLoud.stop(gSpeechHandle);
		gSpeechHandle = 0;
	}

	gSpeech.setText(gSpeechText);
	gSpeechHandle = gSoLoud.play(gSpeech, -1.f, gSpeechPan);
	SetSpeechFilter();
}

static void SetSpeechAudioPan()
{
	if (gSpeechHandle != 0)
	{
		gSoLoud.setPan(gSpeechHandle, gSpeechPan);
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
		InputSystem::SetHideMouseCursorWhileCaptured(false);
		// WINDOW AND RENDERER SETUP
		//
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		if (!pRenderer)    //check for init success
			return false;

		// CREATE COMMAND LIST AND GRAPHICS/COMPUTE QUEUES
		//
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

		// Resource loader 
		initResourceLoaderInterface(pRenderer);

		// INITIALIZE PIPILINE STATES
		//
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		// INITIALIZE THE USER INTERFACE
		//
		if (!gAppUI.Init(pRenderer))
		{
			// todo: display err msg (multiplatform?)
			return false;
		}

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		// Add the GUI Panels/Windows
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 16 };

		float   dpiScale = getDpiScale().x;
		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.05f };
		vec2    UIPanelSize = vec2(650.f, 1000.f) / dpiScale;
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
		gSoLoud.init();
				
		SoLoud::result res = gBgWavObj.load(gWavBgTestFile);
		ASSERT(res == SoLoud::SO_NO_ERROR);
		res = gWarWavObj.load(gOggWarNoiseFile);
		ASSERT(res == SoLoud::SO_NO_ERROR);		

		gSoLoud.setGlobalVolume(gGlobalVolume);

		gBgWavHandle = gSoLoud.playBackground(gBgWavObj);
		gSoLoud.setLooping(gBgWavHandle, true);

		return true;
	}

	void Exit()
	{
		// wait for rendering to finish before freeing resources
		waitQueueIdle(pGraphicsQueue);
				
		gSoLoud.stopAll();
		gSoLoud.deinit();

		gAppUI.Exit();
				
		removeDepthState(pDepth);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		// INITIALIZE SWAP-CHAIN AND DEPTH BUFFER
		//
		if (!addSwapChain())
			return false;
		if (!addDepthBuffer())
			return false;

		// LOAD USER INTERFACE
		//
		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{		

		/************************************************************************/
		// Scene Update
		/************************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

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

		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
		//
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		// Acquire the main render target from the swapchain
		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];
		Cmd*          cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);    // start recording commands

		TextureBarrier barriers[] =    // wait for resource transition
		{
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		// bind and clear the render target
		LoadActionsDesc loadActions = {};    // render target clean command
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = clearVal;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

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
		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
		endCmd(cmd);
		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	const char* GetName() { return "26_Audio"; }

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
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 1.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
};

DEFINE_APPLICATION_MAIN(AudioUnitTest)
