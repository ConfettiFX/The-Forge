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
*
* The purpose of this demo is to show how to use the font system Fontstash with The Forge.
* All the features the font library supports are showcased here, such as font spacing, blurring,
* different text sizes and different fonts.
*********************************************************************************************************/


// tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

// Memory
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h" // NOTE: should be the last include in a .cpp!

// Define App directories
const char* pszBases[] =
{
	"../../../src/05_FontRendering/",	// FSR_BinShaders
	"../../../src/05_FontRendering/",	// FSR_SrcShaders
	"",									// FSR_BinShaders_Common
	"",									// FSR_SrcShaders_Common
	"../../../UnitTestResources/",		// FSR_Textures
	"../../../UnitTestResources/",		// FSR_Meshes
	"../../../UnitTestResources/",		// FSR_Builtin_Fonts
	"../../../src/05_FontRendering/",	// FSR_GpuConfig
	"",									// FSR_OtherFiles
};


/************************************************************************/
/* SCENE VARIABLES
*************************************************************************/
struct Fonts
{   // src: https://fontlibrary.org
	int titilliumBold;
	int comicRelief;
	int crimsonSerif;
	int monoSpace;
	int monoSpaceBold;
};

struct TextObject
{
	tinystl::string mText;
	TextDrawDesc	mDrawDesc;
	float2			mPosition;
};

struct SceneData
{
	size_t sceneTextArrayIndex = 0;
	tinystl::vector<tinystl::vector<TextObject>> sceneTextArray;
};

const uint32_t  gImageCount = 3;

Renderer*       pRenderer = NULL;
Queue*          pGraphicsQueue = NULL;
CmdPool*        pCmdPool = NULL;
Cmd**           ppCmds = NULL;
GpuProfiler*    pGpuProfiler = NULL;
HiresTimer      gTimer;

SwapChain*      pSwapChain = NULL;
Fence*          pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*      pImageAcquiredSemaphore = NULL;
Semaphore*      pRenderCompleteSemaphores[gImageCount] = { NULL };

uint32_t        gFrameIndex = 0;

LogManager      gLogManager;
UIApp           gAppUI;
SceneData       gSceneData;
Fonts           gFonts;
/************************************************************************/
/* APP FUNCTIONS
*************************************************************************/
class FontRendering : public IApp
{
public:
	bool Init()
	{
		// window and renderer setup
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

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);
		initDebugRendererInterface(pRenderer, NULL, FSRoot(-1));

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);
		finishResourceLoading();

		// UI setup
		if (!gAppUI.Init(pRenderer))
			return false;

		// setup scene text
		const FSRoot fontRoot = FSRoot::FSR_Builtin_Fonts;
		gFonts.titilliumBold = addDebugFont("TitilliumText/TitilliumText-Bold.otf", fontRoot);
		gFonts.comicRelief   = addDebugFont("ComicRelief/ComicRelief.ttf", fontRoot);
		gFonts.crimsonSerif  = addDebugFont("Crimson/Crimson-Roman.ttf", fontRoot);
		gFonts.monoSpace     = addDebugFont("InconsolataLGC/Inconsolata-LGC.otf", fontRoot);
		gFonts.monoSpaceBold = addDebugFont("InconsolataLGC/Inconsolata-LGC-Bold.otf", fontRoot);

		requestMouseCapture(false);

		tinystl::vector<TextObject> sceneTexts;
		TextDrawDesc drawDescriptor;
		const char* txt = "";

		
		// This demo was created with a target resolution of 1920x1080.
		// To keep sizing and spacing coherent between devices, we need to take the device's resolution into account
		// and scale font size and font spacing accordingly.
		float scalingFactor = float(mSettings.mWidth) / 1920.0f;

		// TITLE
		//--------------------------------------------------------------------------
		drawDescriptor.mFontColor = 0xff000000; // black : (ABGR)
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
		float2 labelPos = float2(mSettings.mWidth * 0.18f, mSettings.mHeight * 0.30f);
		float2 alphabetPos = float2(mSettings.mWidth * 0.31f, mSettings.mHeight * 0.30f);
		float2 offset = float2(0, drawDescriptor.mFontSize * 1.8f);
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
			sceneTexts.push_back({ string1[i], drawDescriptor, float2(mSettings.mWidth * 0.20f, drawDescriptor.mFontSize * float(i) + mSettings.mHeight * 0.55f) });
		}
		//--------------------------------------------------------------------------


		gSceneData.sceneTextArray.push_back(sceneTexts);
		sceneTexts.clear();

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		removeDebugRendererInterface();

		gAppUI.Exit();

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
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		// PROCESS INPUT
		//-------------------------------------------------------------------------------------
		const int offset = getKeyDown(KEY_LEFT_BUMPER) ? -1 : +1;   // shift+space = previous text
		if (getKeyUp(KEY_LEFT_TRIGGER))
		{
			gSceneData.sceneTextArrayIndex = (gSceneData.sceneTextArrayIndex + offset) % gSceneData.sceneTextArray.size();
		}
	}

	void Draw()
	{
		gTimer.GetUSec(true);

		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 1.0f, 1.0f, 1.0f, 1.0f };

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		// draw text
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render Text");

		if (!gSceneData.sceneTextArray.empty())
		{
			const tinystl::vector<TextObject>& texts = gSceneData.sceneTextArray[gSceneData.sceneTextArrayIndex];
			for (int i = 0; i < texts.size(); ++i)
			{
				const TextDrawDesc* desc = &texts[i].mDrawDesc;
				drawDebugText(cmd, texts[i].mPosition.x, texts[i].mPosition.y, texts[i].mText, desc);
			}
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		// draw profiler timings text
		TextDrawDesc uiTextDesc;	// default
		uiTextDesc.mFontColor = 0xff444444;
		uiTextDesc.mFontSize = 18;
		drawDebugText(cmd, 8.0f, 15.0f, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &uiTextDesc);
#ifndef METAL
		drawDebugText(cmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &uiTextDesc);
#endif

		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName()
	{
		return "05_FontRendering";
	}

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
};

DEFINE_APPLICATION_MAIN(FontRendering)
