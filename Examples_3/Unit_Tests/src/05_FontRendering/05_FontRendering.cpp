/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

/********************************************************************************************************/
/* THE FORGE - FONT RENDERING DEMO
/*
/* The purpose of this demo is to show how to use the font system Fontstash with The Forge.
/* All the features the font library supports are showcased here, such as font spacing, blurring,
/* different text sizes and different fonts.
/********************************************************************************************************/


//tiny stl
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../Common_3/OS/Interfaces/IUIManager.h"
#include "../../Common_3/OS/Interfaces/IApp.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../Common_3/OS/Math/MathTypes.h"

// UI
#include "../../Common_3/OS/UI/UIRenderer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"


#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#else
#error PLATFORM NOT SUPPORTED
#endif

// Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
	"../../..//src/05_FontRendering/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../..//src/05_FontRendering/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"../../..//src/00_Common/" RESOURCE_DIR "/Binary/",			// FSR_BinShaders_Common
	"../../..//src/00_Common/" RESOURCE_DIR "/",				// FSR_SrcShaders_Common
	"../../..//UnitTestResources/Textures/",					// FSR_Textures
	"../../..//UnitTestResources/Meshes/",						// FSR_Meshes
	"../../..//UnitTestResources/Fonts/",						// FSR_Builtin_Fonts
	"",															// FSR_OtherFiles
};

LogManager gLogManager;
/************************************************************************/
/* SCENE VARIABLES
/************************************************************************/
struct Fonts
{	// src: https://fontlibrary.org
	int titilliumBold;
	int comicRelief;
	int crimsonSerif;
	int monoSpace;
	int monoSpaceBold;
};

struct TextObject
{
	String		 mText;
	TextDrawDesc mDrawDesc;
	vec2		 mPosition;
};

struct SceneData
{
	size_t sceneTextArrayIndex = 0;
	tinystl::vector<tinystl::vector<TextObject>> sceneTextArray;
};

const uint32_t	gImageCount = 3;

Renderer*		pRenderer = nullptr;
RenderTarget*	pRenderTarget = nullptr;
Queue*			pGraphicsQueue = nullptr;
CmdPool*		pCmdPool = nullptr;
Cmd**			ppCmds = nullptr;
GpuProfiler*	pGpuProfiler = nullptr;
UIManager*		pUIManager = nullptr;
HiresTimer		gTimer;

SwapChain*		pSwapChain = nullptr;

Fence*			pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*		pImageAcquiredSemaphore = nullptr;
Semaphore*		pRenderCompleteSemaphores[gImageCount] = { nullptr };

uint32_t		gFrameIndex = 0;

SceneData		gSceneData;
Fonts			gFonts;
/************************************************************************/
/* APP FUNCTIONS
/************************************************************************/
class FontRendering : public IApp
{
public:
	bool Init()
	{
		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);

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

		initResourceLoaderInterface(pRenderer);
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);
		finishResourceLoading();

		if (!addSwapChain())
			return false;

		// UI setup
		UISettings uiSettings = {};
		uiSettings.pDefaultFontName = "TitilliumText/TitilliumText-Bold.ttf";
		addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

		requestMouseCapture(false);

		// setup scene text
		FSRoot fontRoot = FSRoot::FSR_Builtin_Fonts;
#if defined(TARGET_IOS)
		fontRoot = FSRoot::FSR_Absolute;
#endif

		UIRenderer* pUIRenderer = pUIManager->pUIRenderer;	// shorthand
		gFonts.titilliumBold = pUIRenderer->addFont("TitilliumText/TitilliumText-Bold.ttf", "TitilliumText-Bold", fontRoot);
		gFonts.comicRelief = pUIRenderer->addFont("ComicRelief/ComicRelief.ttf", "Comic Relief", fontRoot);
		gFonts.crimsonSerif = pUIRenderer->addFont("Crimson/Crimson-Roman.ttf", "Crimson Serif", fontRoot);
		gFonts.monoSpace = pUIRenderer->addFont("InconsolataLGC/Inconsolata-LGC.ttf", "Inconsolata", fontRoot);
		gFonts.monoSpaceBold = pUIRenderer->addFont("InconsolataLGC/Inconsolata-LGC-Bold.ttf", "InconsolataBold", fontRoot);

		tinystl::vector<TextObject> sceneTexts;
		TextDrawDesc drawDescriptor;
		const char* txt = "";

		// This demo was created with a target resolution of 1920x1080.
		// To keep sizing and spacing coherent between devices, we need to take the device's resolution into account
		// and scale font size and font spacing accordingly.
		float scalingFactor = float(mSettings.mWidth) / 1920.0f;

		// TITLE
		//--------------------------------------------------------------------------
		drawDescriptor.mFontColor = 0xff000000;	// black : (ABGR)
		drawDescriptor.mFontID = gFonts.monoSpaceBold;
		drawDescriptor.mFontSize = 50.0f * scalingFactor;
		txt = "Fontstash Font Rendering";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.3f, mSettings.mHeight * 0.05f } });
		//--------------------------------------------------------------------------

		// FONT SPACING
		//--------------------------------------------------------------------------
		drawDescriptor.mFontID = gFonts.monoSpace;
		drawDescriptor.mFontSpacing = 3.0f * scalingFactor;
		drawDescriptor.mFontSize = 20.0f * scalingFactor;

		drawDescriptor.mFontSpacing = 0.0f;
		txt = "Font Spacing = 0.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.2f, mSettings.mHeight * 0.15f } });

		drawDescriptor.mFontSpacing = 1.0f * scalingFactor;
		txt = "Font Spacing = 1.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.2f, mSettings.mHeight * 0.17f } });

		drawDescriptor.mFontSpacing = 2.0f * scalingFactor;
		txt = "Font Spacing = 2.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.2f, mSettings.mHeight * 0.19f } });

		drawDescriptor.mFontSpacing = 4.0f * scalingFactor;
		txt = "Font Spacing = 4.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.2f, mSettings.mHeight * 0.21f } });
		//--------------------------------------------------------------------------

		// FONT BLUR
		//--------------------------------------------------------------------------
		drawDescriptor.mFontSpacing = 0.0f;
		drawDescriptor.mFontBlur = 0.0f;
		txt = "Blur = 0.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.4f, mSettings.mHeight * 0.15f } });

		drawDescriptor.mFontBlur = 1.0f;
		txt = "Blur = 1.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.4f, mSettings.mHeight * 0.17f } });

		drawDescriptor.mFontBlur = 2.0f;
		txt = "Blur = 2.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.4f, mSettings.mHeight * 0.19f } });

		drawDescriptor.mFontBlur = 4.0f;
		txt = "Blur = 4.0f";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.4f, mSettings.mHeight * 0.21f } });
		//--------------------------------------------------------------------------

		// FONT COLOR
		//--------------------------------------------------------------------------
		drawDescriptor.mFontBlur = 0.0f;
		drawDescriptor.mFontSize = 20 * scalingFactor;

		drawDescriptor.mFontColor = 0xff0000dd;
		txt = "Font Color: Red   | 0xff0000dd";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.6f, mSettings.mHeight * 0.15f } });

		drawDescriptor.mFontColor = 0xff00dd00;
		txt = "Font Color: Green | 0xff00dd00";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.6f, mSettings.mHeight * 0.17f } });

		drawDescriptor.mFontColor = 0xffdd0000;
		txt = "Font Color: Blue  | 0xffdd0000";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.6f, mSettings.mHeight * 0.19f } });

		drawDescriptor.mFontColor = 0xff333333;
		txt = "Font Color: Gray  | 0xff333333";
		sceneTexts.push_back({ txt, drawDescriptor,{ mSettings.mWidth * 0.6f, mSettings.mHeight * 0.21f } });
		//--------------------------------------------------------------------------


		// DIFFERENT FONTS
		//--------------------------------------------------------------------------
		const char* alphabetText = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789";
		const char* fontNames[] = { "TitilliumText-Bold", "Crimson-Serif", "Comic Relief", "Inconsolata-Mono" };
		const int   fontIDs[] = { gFonts.titilliumBold, gFonts.crimsonSerif, gFonts.comicRelief, gFonts.monoSpace };

		drawDescriptor.mFontSize = 30.0f * scalingFactor;
		vec2 labelPos = vec2(mSettings.mWidth * 0.18f, mSettings.mHeight * 0.30f);
		vec2 alphabetPos = vec2(mSettings.mWidth * 0.31f, mSettings.mHeight * 0.30f);
		vec2 offset = vec2(0, drawDescriptor.mFontSize * 1.8f);
		for (int i = 0; i < 4; ++i)
		{
			// font label
			drawDescriptor.mFontID = fontIDs[i];
			txt = fontNames[i];
			sceneTexts.push_back({ txt, drawDescriptor, labelPos });

			// alphabet
			txt = alphabetText;
			sceneTexts.push_back({ txt, drawDescriptor, alphabetPos });

			// offset for the next font
			labelPos += offset;
			alphabetPos += offset;
		}
		//--------------------------------------------------------------------------


		// WALL OF TEXT (UTF-8)
		//--------------------------------------------------------------------------
		drawDescriptor.mFontColor = 0xff000000;
		static const char *const string1[11] =
		{
			u8"Your name is Gus Graves, and you\u2019re a firefighter in the small town of Timber Valley, where the largest employer is the",
			u8"mysterious research division of the MGL Corporation, a powerful and notoriously secretive player in the military-industrial",
			u8"complex. It\u2019s sunset on Halloween, and just as you\u2019re getting ready for a stream of trick-or-treaters at home, your",
			u8"chief calls you into the station. There\u2019s a massive blaze at the MGL building on the edge of town. You jump off the fire",
			u8"engine as it rolls up to the inferno and gasp not only at the incredible size of the fire but at the strange beams of light",
			u8"brilliantly flashing through holes in the building\u2019s crumbling walls. As you approach the structure for a closer look,",
			u8"the wall and floor of the building collapse to expose a vast underground chamber where all kinds of debris are being pulled",
			u8"into a blinding light at the center of a giant metallic ring. The ground begins to fall beneath your feet, and you try to",
			u8"scurry up the steepening slope to escape, but it\u2019s too late. You\u2019re pulled into the device alongside some mangled",
			u8"equipment and the bodies of lab technicians who didn\u2019t survive the accident. You see your fire engine gravitating toward",
			u8"you as you accelerate into a tunnel of light."
		};

		drawDescriptor.mFontSize = 30.5f * scalingFactor;
		drawDescriptor.mFontID = gFonts.crimsonSerif;
		for (int i = 0; i < 11; i++)
		{
			sceneTexts.push_back({ string1[i], drawDescriptor, vec2(mSettings.mWidth * 0.20f, drawDescriptor.mFontSize * float(i) + mSettings.mHeight * 0.55f) });
		}
		//--------------------------------------------------------------------------


		gSceneData.sceneTextArray.push_back(sceneTexts);
		sceneTexts.clear();

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex]);

		removeUIManagerInterface(pRenderer, pUIManager);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);
		removeSwapChain(pRenderer, pSwapChain);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex]);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		const float w = (float)pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mWidth;
		const float h = (float)pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mHeight;
		const float aspectRatio = w / h;
		const UIRenderer* pUIRenderer = pUIManager->pUIRenderer;
		const tinystl::vector<TextObject>& texts = gSceneData.sceneTextArray[gSceneData.sceneTextArrayIndex];

		// PROCESS INPUT
		//-------------------------------------------------------------------------------------
#ifndef TARGET_IOS
		const int offset = getKeyDown(KEY_SHIFT) ? -1 : +1;	// shift+space = previous text
		if (getKeyUp(KEY_SPACE))
		{
			gSceneData.sceneTextArrayIndex = (gSceneData.sceneTextArrayIndex + offset) % gSceneData.sceneTextArray.size();
		}
#endif
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
		pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 1.0f, 1.0f, 1.0f, 1.0f };

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		cmdBeginRender(cmd, 1, &pRenderTarget, NULL, &loadActions);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		// draw text
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render Text");
		cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);

		if (!gSceneData.sceneTextArray.empty())
		{
			const tinystl::vector<TextObject>& texts = gSceneData.sceneTextArray[gSceneData.sceneTextArrayIndex];
			for (int i = 0; i < texts.size(); ++i)
			{
				const TextDrawDesc* desc = &texts[i].mDrawDesc;
				cmdUIDrawText(cmd, pUIManager, texts[i].mPosition, texts[i].mText, desc);
			}
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		// draw profiler timings text
		TextDrawDesc uiTextDesc;	// default
		uiTextDesc.mFontColor = 0xff444444;
		uiTextDesc.mFontSize = 18;
		cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 15.0f), "CPU ", gTimer.GetUSec(true) / 1000.0f, &uiTextDesc);
		cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 40.0f), "GPU ", (float)pGpuProfiler->mCumulativeTime * 1000.0f, &uiTextDesc);
		cmdUIEndRender(cmd, pUIManager);

		cmdEndRender(cmd, 1, &pRenderTarget, NULL);
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
		FenceStatus fenceStatus;
		getFenceStatus(pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence);
	}

	String GetName()
	{
		return "05_FontRendering";
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.pQueue = pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}
};

DEFINE_APPLICATION_MAIN(FontRendering)
