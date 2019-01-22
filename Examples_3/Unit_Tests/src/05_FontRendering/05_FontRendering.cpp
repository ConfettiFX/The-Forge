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

/********************************************************************************************************/
/* THE FORGE - FONT RENDERING DEMO
*
* The purpose of this demo is to show how to use the font system Fontstash with The Forge.
* All the features the font library supports are showcased here, such as font spacing, blurring,
* different text sizes and different fonts. It also contains sample code for simple layout operations.
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
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"    // NOTE: should be the last include in a .cpp!

// Define App directories
const char* pszBases[FSR_Count] = {
	"../../../src/05_FontRendering/",       // FSR_BinShaders
	"../../../src/05_FontRendering/",       // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/05_FontRendering/",       // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

/************************************************************************/
/* SCENE VARIABLES
*************************************************************************/
struct Fonts
{    // src: https://fontlibrary.org
	int titilliumBold;
	int comicRelief;
	int crimsonSerif;
	int monoSpace;
	int monoSpaceBold;
};

struct ScreenText
{
	tinystl::string mText;
	TextDrawDesc    mDrawDesc;

	// screen space position:
	// [0, 0] = top left
	// [1, 1] = bottom right
	float2 mScreenPosition;
};

struct SceneData
{
	size_t                                       sceneTextArrayIndex = 0;
	tinystl::vector<tinystl::vector<ScreenText>> sceneTextArray;

	uint32_t theme = 1;              // enable dark theme (its better <3) | spacebar to change theme
	bool     bFitToScreen = true;    // scales all the text down if any scene text is off screen
};

// todo: rename this enum
enum PropertiesWithSkinColor
{
	PROP_TEXT,
	PROP_HEADER
};
static const uint32_t gLightSkinTextColor = 0xff333333;
static const uint32_t gLightSkinHeaderColor = 0xff000000;

static const uint32_t gDarkSkinTextColor = 0xffb0b0b0;
static const uint32_t gDarkSkinHeaderColor = 0xffffffff;

uint32_t GetSkinColorOfProperty(PropertiesWithSkinColor EProp, bool bDarkSkin)
{
	switch (EProp)
	{
		case PROP_TEXT: return bDarkSkin ? gDarkSkinTextColor : gLightSkinTextColor; break;
		case PROP_HEADER: return bDarkSkin ? gDarkSkinHeaderColor : gLightSkinHeaderColor; break;
		default: return gLightSkinHeaderColor; break;
	}
}
uint32_t GetSkinColorOfProperty(PropertiesWithSkinColor EProp, uint32_t theme) { return GetSkinColorOfProperty(EProp, (bool)theme); }

const uint32_t gImageCount = 3;

Renderer*    pRenderer = NULL;
Queue*       pGraphicsQueue = NULL;
CmdPool*     pCmdPool = NULL;
Cmd**        ppCmds = NULL;
GpuProfiler* pGpuProfiler = NULL;
HiresTimer   gTimer;

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;

LogManager gLogManager;
SceneData  gSceneData;
Fonts      gFonts;

/************************************************************************/
/* APP UI VARIABLES
*************************************************************************/
UIApp         gAppUI;
GuiComponent* pUIWindow = NULL;
bool          gbShowSceneControlsUIWindow = true;    // toggle this w/ F1

enum ColorTheme : uint32_t
{
	COLOR_THEME_LIGHT = 0,
	COLOR_THEME_DARK,

	NUM_COLOR_THEMES
};
static const char*      pThemeLabels[] = { "Light", "Dark",

                                      NULL };
static const ColorTheme ColorThemes[] = { COLOR_THEME_LIGHT, COLOR_THEME_DARK,

										  NUM_COLOR_THEMES };

// state variable to keep track of drop down value change (we should ideally hook up event callbacks instead of this)
static uint32_t gPreviousTheme = 0;
static bool     gPreviousFitToScreen = false;

/************************************************************************/
/* TEXT & LAYOUT FUNCTIONS
 *************************************************************************/
// calculates the length of the previous text, takes into account the spacing between texts,
// returns the position (normalized) for the next, new text
//
// use this for avoiding overlapping of text in the horizontal direction.
//
float GetNextTextPosition(
	const float normalizedXCoordOfPreviousText, const char* pTextPrevious, const TextDrawDesc& drawDescPrevious,
	const IApp::Settings& mSettings)
{
	// TextA = @pTextPrevious
	// TextB = CurrentText to be drawn
	//
	// <==========> ----- offsetFromPreviousText [0, 1] : this is the value we use to set the position for the CurrentText
	// TextA <==> TextB
	// <===>   ^  ^
	//   ^     |  +------ normalizedXCoordOfCurrentText [0, 1]
	//   |     |
	//   |     +--------- spacingBetweenTexts [0, 1]
	//   |
	//   +--------------- previousTextSizeInPx [0, screenSize]
	//
	const float  spacingBetweenTexts = 0.01f;    // normalized to screen size(width)
	const float2 previousTextSizeInPx = gAppUI.MeasureText(pTextPrevious, drawDescPrevious);
	const float  offsetFromPreviousText = previousTextSizeInPx.getX() / mSettings.mWidth + spacingBetweenTexts;
	return normalizedXCoordOfPreviousText + offsetFromPreviousText;
};

// gets the width of the longest string in pixels.
// use this function for measuring different fonts
//
float GetLongestStringLengthInPx(const char* const* ppTexts, int numTexts, const TextDrawDesc* pDrawDescs)
{
	float longestTextSz = 0.0f;
	for (int i = 0; i < numTexts; ++i)
	{
		longestTextSz = max(longestTextSz, gAppUI.MeasureText(ppTexts[i], pDrawDescs[i]).getX());
	}
	return longestTextSz;
}

// gets the width of the longest string in pixels.
// use this function for measuring text lines that uses the same TextDrawDesc
//
float GetLongestStringLengthInPx(const char* const* ppTexts, int numTexts, const TextDrawDesc& drawDesc)
{
	float longestTextSz = 0.0f;
	for (int i = 0; i < numTexts; ++i)
	{
		longestTextSz = max(longestTextSz, gAppUI.MeasureText(ppTexts[i], drawDesc).getX());
	}
	return longestTextSz;
}

// returns the screen coordinates (x,y) for drawing the given text in the middle of the screen.
//
inline float GetScreenCenteredPosition(const float normalizedTextLength) { return (1.0f - normalizedTextLength) * 0.5f; }
float2       GetCenteredTextPosition(const char* pText, const TextDrawDesc& drawDesc, const IApp::Settings& mSettings)
{
	const float2 normalizedTextSize = gAppUI.MeasureText(pText, drawDesc) / float2(mSettings.mWidth, mSettings.mHeight);
	const float2 normalizedScreenCoords(
		GetScreenCenteredPosition(normalizedTextSize.getX()), GetScreenCenteredPosition(normalizedTextSize.getY()));
	return normalizedScreenCoords;
}

//static float gBiasX = 0.0f;
//static float gBiasY = 0.0f;
static float2 gBias(0.0f, 0.0f);

/************************************************************************/
/* APP IMPLEMENTATION
*************************************************************************/
class FontRendering: public IApp
{
	public:
	FontRendering()
	{
#ifndef METAL
		//set window size
		mSettings.mWidth = 1920;
		mSettings.mHeight = 1080;
#endif
	}

	bool Init()
	{
		// window and renderer setup
		RendererDesc rendererDesc = { 0 };
		initRenderer(GetName(), &rendererDesc, &pRenderer);
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

		// this is for debug rendering: cpu/gpu profilers, debug msgs etc. (drawDebugText())
		// the App fonts are initialized/added through the UIApp object.   (gAppUI.DrawText())
		addDebugFont("TitilliumText/TitilliumText-Bold.otf", FSRoot::FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);
		finishResourceLoading();

		// initialize UI middleware
		if (!gAppUI.Init(pRenderer))
			return false;    // report?

		// load the fonts
		const FSRoot fontRoot = FSRoot::FSR_Builtin_Fonts;
		gFonts.titilliumBold = gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", fontRoot);
		gFonts.comicRelief = gAppUI.LoadFont("ComicRelief/ComicRelief.ttf", fontRoot);
		gFonts.crimsonSerif = gAppUI.LoadFont("Crimson/Crimson-Roman.ttf", fontRoot);
		gFonts.monoSpace = gAppUI.LoadFont("InconsolataLGC/Inconsolata-LGC.otf", fontRoot);
		gFonts.monoSpaceBold = gAppUI.LoadFont("InconsolataLGC/Inconsolata-LGC-Bold.otf", fontRoot);

		// setup the UI window
		const float dpiScl = getDpiScale().x;
		vec2        UIWndSize = vec2{ 250, 300 } / dpiScl;
		vec2        UIWndPosition = vec2{ mSettings.mWidth * 0.02f, mSettings.mHeight * 0.8f } / dpiScl;
		GuiDesc     guiDesc(UIWndPosition, UIWndSize, TextDrawDesc());
		pUIWindow = gAppUI.AddGuiComponent("Controls", &guiDesc);

		CheckboxWidget fitScreenCheckbox("Fit to Screen", &gSceneData.bFitToScreen);

		const size_t   NUM_THEMES = sizeof(pThemeLabels) / sizeof(const char*) - 1;    // -1 for the NULL element
		DropdownWidget ThemeDropdown("Theme", &gSceneData.theme, pThemeLabels, (uint32_t*)ColorThemes, NUM_THEMES);

		pUIWindow->AddWidget(ThemeDropdown);
		pUIWindow->AddWidget(fitScreenCheckbox);

		gPreviousTheme = gSceneData.theme;

		requestMouseCapture(false);
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
		if (!pSwapChain)
			return false;

		InitializeSceneText();
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

		// Toggle Theme
		const int offset = (getKeyDown(KEY_LEFT_SHIFT) || getKeyDown(KEY_RIGHT_SHIFT)) ? -1 : +1;
		if (getKeyUp(KEY_LEFT_TRIGGER))    // KEY_LEFT_TRIGGER = spacebar
		{
			gSceneData.theme = !gSceneData.theme;    // dark/light theme
													 // no need to call InitializeSceneText() here,
													 // change of value event will be handled further down this function.
		}

		// --------------------------------------------------------------------
		// old code fot switching the set of text to be displayed -------------
		// gSceneData.sceneTextArrayIndex = (gSceneData.sceneTextArrayIndex + offset) % gSceneData.sceneTextArray.size();
		// --------------------------------------------------------------------

		// Toggle Displaying UI Window
		if (getKeyUp(KEY_LEFT_STICK_BUTTON))    // F1 key
		{
			gbShowSceneControlsUIWindow = !gbShowSceneControlsUIWindow;
		}

		gAppUI.Update(deltaTime);

		// detect dropdown value change
		if (gPreviousTheme != gSceneData.theme)
		{
			gPreviousTheme = gSceneData.theme;
			InitializeSceneText();
		}

		// detect fit to screen toggle change
		if (gPreviousFitToScreen != gSceneData.bFitToScreen)
		{
			gPreviousFitToScreen = gSceneData.bFitToScreen;
			InitializeSceneText();
		}
	}

	void Draw()
	{
		gTimer.GetUSec(true);

		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const float darkBackgroundColor = 0.05f;
		if (gSceneData.theme)
			loadActions.mClearColorValues[0] = { darkBackgroundColor, darkBackgroundColor, darkBackgroundColor, 1.0f };

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		// draw text (uses AppUI)
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render Text");

		if (!gSceneData.sceneTextArray.empty())
		{
			const tinystl::vector<ScreenText>& texts = gSceneData.sceneTextArray[gSceneData.sceneTextArrayIndex];
			for (int i = 0; i < texts.size(); ++i)
			{
				const float2 pxPosition = texts[i].mScreenPosition * float2(mSettings.mWidth, mSettings.mHeight);
				gAppUI.DrawText(cmd, pxPosition, texts[i].mText, texts[i].mDrawDesc);
			}
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		// draw profiler timings text (uses debugText)
		TextDrawDesc uiTextDesc;    // default
		uiTextDesc.mFontColor = gSceneData.theme ? 0xff21D8DE : 0xff444444;
		uiTextDesc.mFontSize = 18;
		drawDebugText(cmd, 8.0f, 15.0f, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &uiTextDesc);
#ifndef METAL
		drawDebugText(cmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &uiTextDesc);
#endif
		if (gbShowSceneControlsUIWindow)
			gAppUI.Gui(pUIWindow);

		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "05_FontRendering"; }

	void InitializeSceneText()
	{
		gSceneData.sceneTextArray.clear();
		tinystl::vector<ScreenText> sceneTexts;
		TextDrawDesc                drawDescriptor;
		const char*                 txt = "";

		const float SCREEN_WIDTH = (float)mSettings.mWidth;
		const float SCREEN_HEIGHT = (float)mSettings.mHeight;

		float2 dpiScaling = getDpiScale();

		// [0]: screen space distance from top of the window to the title
		// [1]: screen space distance between areas of title and ROW 1
		// [2]: screen space distance between areas of title and ROW 2
		// ...
		const float screenSizeDistanceFromPreviousRow[] = { 0.050f, 0.070f, 0.105f, 0.085f, 0.085f };

		// ROW 0 ===================================================================
		// TITLE: FONTSTASH FONT RENDERING
		//
		drawDescriptor.mFontColor = GetSkinColorOfProperty(PROP_HEADER, gSceneData.theme);    // color : (ABGR)
		drawDescriptor.mFontID = gFonts.monoSpaceBold;
		drawDescriptor.mFontSize = 50.0f;
		txt = "Fontstash Font Rendering";
		const float2 centeredCoords = GetCenteredTextPosition(txt, drawDescriptor, mSettings);
		sceneTexts.push_back({ txt, drawDescriptor, { centeredCoords.getX(), screenSizeDistanceFromPreviousRow[0] } });
		// ROW 0 ===================================================================

		// some pre-calculations here to center the text for any resolution:
		//
		// CALCULATING WIDTH:
		// for ROW 1 we show 3 columns of text. We want to center the 2nd column
		// add offset 1st and 3rd column.
		//
		// a few things to remember here:
		//
		// - since font spacing has different length for each line of text, we use
		//   the last one with spacing=4 to calculate the position for the entire column.
		//
		// - we use the MeasureText() to get the size of the text to be drawn with a given
		//   font in pixels, and use that to calculated a normalized position := [0.0f, 1.0f]
		//   where (0.0f, 0.0f) is top left corner, and (1.0f, 1.0f) is bottom right corner of
		//   the screen.
		//
		// here's how we calculate the normalized positions of each text block (spacing, blur, color):
		//
		// blur    = screen center
		// spacing = blur - (longestSpacingFontTextNormalizedLength + normalizedLengthBetweenEachColumn)
		// color   = blur + blurTextNormalizedLength + normalizedLengthBetweenEachColumn
		//
		// CALCULATING HEIGHT:
		// add the total row height of the previous row to the previous rows first element's position
		// as we go
		//

		// CALCULATING HEIGHT:
		const float2 TITLE_SIZE_PX = gAppUI.MeasureText(txt, drawDescriptor);
		const float  blurPositionY =
			screenSizeDistanceFromPreviousRow[0] + TITLE_SIZE_PX.getY() / SCREEN_HEIGHT + screenSizeDistanceFromPreviousRow[1];

		// CALCULATING WIDTH:
		const float normalizedLengthBetweenEachColumn = 0.050f;

		drawDescriptor.mFontSpacing = 4.0f;    // set these upfront for input for MeasureText() function
		drawDescriptor.mFontSize = 20.0f;
		drawDescriptor.mFontID = gFonts.monoSpace;
		txt = "Font Spacing = 4.0f";
		const float longestSpacingFontTextNormalizedLength = gAppUI.MeasureText(txt, drawDescriptor).getX() / SCREEN_WIDTH;

		drawDescriptor.mFontSpacing = 0.0f;    // set these upfront for input for MeasureText() function
		drawDescriptor.mFontBlur = 0.0f;
		txt = "Blur = 0.0f";
		const float blurTextNormalizedLength = gAppUI.MeasureText(txt, drawDescriptor).getX() / SCREEN_WIDTH;

		const float blurPosition = GetCenteredTextPosition(txt, drawDescriptor, mSettings).getX();
		const float spacingPosition = blurPosition - (longestSpacingFontTextNormalizedLength + normalizedLengthBetweenEachColumn);
		const float colorPosition = blurPosition + blurTextNormalizedLength + normalizedLengthBetweenEachColumn;

		// ROW 1 ===================================================================
		// FONT PROPERTIES: SPACING, BLUR AND COLOR
		//
		union FontPropertyValue
		{
			float f;
			int   i;
		};
		drawDescriptor.mFontID = gFonts.monoSpace;
		drawDescriptor.mFontSize = 20.0f;

		const int         numSubColumns = 3;    // we display 3 font properties: spacing, blur and color
		const int         numSubRows = 4;       // we display 4 values for each of the font properties
		const int         numElements = numSubRows * numSubColumns;
		FontPropertyValue fontPropertyValues[numElements] = {
			0.0f, 1.0f, 2.0f, 4.0f,

			0.0f, 1.0f, 2.0f, 4.0f,

			// note:
			// cannot initialize the union's int variable like this
			// need to explicitly assign the int variable outisde the
			// initializer list. initilize to 0.0f for now.
			//
			0.0f,    // ((int)0xff0000dd),
			0.0f,    // ((int)0xff00dd00),
			0.0f,    // ((int)0xffdd5050),
			0.0f,    // ((int)0xff888888)
		};
		fontPropertyValues[8].i = 0xff0000dd;
		fontPropertyValues[9].i = 0xff00dd00;
		fontPropertyValues[10].i = 0xffdd5050;
		fontPropertyValues[11].i = 0xff888888;

		const char*  pSubRowTexts[numElements] = { "Font Spacing = 0.0f",
                                                  "Font Spacing = 1.0f",
                                                  "Font Spacing = 2.0f",
                                                  "Font Spacing = 4.0f",

                                                  "Blur = 0.0f",
                                                  "Blur = 1.0f",
                                                  "Blur = 2.0f",
                                                  "Blur = 4.0f",

                                                  "Font Color: Red   | 0xff0000dd",
                                                  "Font Color: Green | 0xff00dd00",
                                                  "Font Color: Blue  | 0xffdd5050",
                                                  "Font Color: Gray  | 0xff888888" };
		const float* pFontPropertyXPositions[numSubColumns] = { &spacingPosition, &blurPosition, &colorPosition };

		float rowHeightPerElem = 0.0f;
		for (int subColumn = 0; subColumn < numSubColumns; ++subColumn)
		{
			float subRowPositionY = blurPositionY;

			// reset properties when we iterate on a sub column
			drawDescriptor.mFontSpacing = 0.0f;
			drawDescriptor.mFontBlur = 0.0f;
			drawDescriptor.mFontColor = GetSkinColorOfProperty(PROP_TEXT, gSceneData.theme);

			for (int subRow = 0; subRow < numSubRows; ++subRow)
			{
				const int text_index = subRow + numSubRows * subColumn;

				// set font properties and text data, and save it in scene data
				switch (subColumn)
				{
					case 0: drawDescriptor.mFontSpacing = fontPropertyValues[text_index].f; break;
					case 1: drawDescriptor.mFontBlur = fontPropertyValues[text_index].f; break;
					case 2: drawDescriptor.mFontColor = (unsigned)fontPropertyValues[text_index].i; break;
				}
				txt = pSubRowTexts[text_index];
				sceneTexts.push_back({ txt, drawDescriptor, { *(pFontPropertyXPositions[subColumn]), subRowPositionY } });

				if (text_index == 0)    // measure the height of sub-row once
				{
					rowHeightPerElem = gAppUI.MeasureText(txt, drawDescriptor).getY() / SCREEN_HEIGHT;
				}

				// iterate Y position, move on to the next row
				subRowPositionY += rowHeightPerElem;    // + offset?
			}
		}
		// ROW 1 ===================================================================

		// ROW 2 ===================================================================
		// ALPHABET WITH DIFFERENT FONTS
		//
		const int   numFonts = 4;
		const float alphabetFontSize = 30.0f;
		const char* alphabetText = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789";
		const char* fontNames[numFonts] = { "TitilliumText-Bold", "Crimson-Serif", "Comic Relief", "Inconsolata-Mono" };
		const int   fontIDs[numFonts] = { gFonts.titilliumBold, gFonts.crimsonSerif, gFonts.comicRelief, gFonts.monoSpace };
		float2      textLengthsForEachFont[numFonts] = {};
		float       textHeightsForEachFont[numFonts] = {};

		// set font properties for each different font
		TextDrawDesc drawDescs[numFonts] = {};
		for (int i = 0; i < numFonts; ++i)
		{
			drawDescs[i].mFontSize = alphabetFontSize;
			drawDescs[i].mFontID = fontIDs[i];
			drawDescs[i].mFontColor = GetSkinColorOfProperty(PROP_HEADER, gSceneData.theme);
			drawDescs[i].mFontSpacing = 0.0f;
		}

		// CALCULATING WIDTH:
		// calculate longest text line, given the fonts
		for (int i = 0; i < numFonts; ++i)
		{
			const float2 alphabetMeasure = gAppUI.MeasureText(alphabetText, drawDescs[i]);
			textLengthsForEachFont[i] =
				float2(gAppUI.MeasureText(fontNames[i], drawDescs[i]).getX() / SCREEN_WIDTH, alphabetMeasure.getX() / SCREEN_WIDTH);
			textHeightsForEachFont[i] = alphabetMeasure.getY();
		}

		// calculate the position to center the alphabet on screen using the longest text line
		float       alphabetLineLengths[numFonts] = {};
		const float normalizedLengthBetweenAlphabetAndLabel = 0.025f;
		for (int i = 0; i < numFonts; ++i)
			alphabetLineLengths[i] =
				textLengthsForEachFont[i].getX() + normalizedLengthBetweenAlphabetAndLabel + textLengthsForEachFont[i].getY();

		float maxalphabetLineLength = 0.0f;    // normalized
		float totalOfAlphabetHeights = 0.0f;
		for (int i = 0; i < numFonts; ++i)
		{
			maxalphabetLineLength = max(maxalphabetLineLength, alphabetLineLengths[i]);
			totalOfAlphabetHeights += textHeightsForEachFont[i];
		}

		const float centeredAlphabetTextNormalizedPositionX = GetScreenCenteredPosition(maxalphabetLineLength);

		// CALCULATING HEIGHT:
		// use previous row's first elements position + height of the entire previous row + margin offset
		const float centeredAlphabetTextNormalizedPositionY =
			blurPositionY + (gAppUI.MeasureText(fontNames[0], drawDescs[0]).getY() / SCREEN_HEIGHT) * numSubRows +
			screenSizeDistanceFromPreviousRow[2];

		// set row positions and label-alphabet offsets
		float2      labelPos = float2(centeredAlphabetTextNormalizedPositionX, centeredAlphabetTextNormalizedPositionY);
		const float longestFontNameInPixels = GetLongestStringLengthInPx(fontNames, numFonts, drawDescs);
		const float fontLabelToAlphabetTextDistance = 0.01f;    // [0, 1]
		const float labelPositionNormalizedOffset = longestFontNameInPixels / mSettings.mWidth + fontLabelToAlphabetTextDistance;

		float2 alphabetPos = labelPos + float2(labelPositionNormalizedOffset, 0.0f);

		float alphabetHeights[numFonts] = {};
		float largestAlphabetHeight = 0.0f;
		for (int i = 0; i < numFonts; ++i)
		{
			alphabetHeights[i] = gAppUI.MeasureText(alphabetText, drawDescs[i]).getY() / SCREEN_HEIGHT;
			largestAlphabetHeight = max(largestAlphabetHeight, alphabetHeights[i]);
		}

		for (int i = 0; i < numFonts; ++i)
		{
			// font label
			drawDescriptor.mFontID = fontIDs[i];
			txt = fontNames[i];
			sceneTexts.push_back({ txt, drawDescs[i], labelPos });

			// alphabet
			txt = alphabetText;
			sceneTexts.push_back({ txt, drawDescs[i], alphabetPos });

			// offset for the next font
			labelPos = labelPos + float2(0.0f, largestAlphabetHeight + 0.01f);
			alphabetPos = alphabetPos + float2(0.0f, largestAlphabetHeight + 0.01f);
		}
		// ROW 2 ===================================================================

		// ROW 3 ===================================================================
		// WALL OF TEXT (UTF-8)
		//
		drawDescriptor.mFontColor = GetSkinColorOfProperty(PROP_TEXT, gSceneData.theme);
		const int                numParagraphLines = 11;
		static const char* const string1[numParagraphLines] = {
			u8"Your name is Gus Graves, and you\u2019re a firefighter in the small town of Timber Valley, where the largest employer is "
			u8"the",
			u8"mysterious research division of the MGL Corporation, a powerful and notoriously secretive player in the military-industrial",
			u8"complex. It\u2019s sunset on Halloween, and just as you\u2019re getting ready for a stream of trick-or-treaters at home, "
			u8"your",
			u8"chief calls you into the station. There\u2019s a massive blaze at the MGL building on the edge of town. You jump off the "
			u8"fire",
			u8"engine as it rolls up to the inferno and gasp not only at the incredible size of the fire but at the strange beams of light",
			u8"brilliantly flashing through holes in the building\u2019s crumbling walls. As you approach the structure for a closer look,",
			u8"the wall and floor of the building collapse to expose a vast underground chamber where all kinds of debris are being pulled",
			u8"into a blinding light at the center of a giant metallic ring. The ground begins to fall beneath your feet, and you try to",
			u8"scurry up the steepening slope to escape, but it\u2019s too late. You\u2019re pulled into the device alongside some mangled",
			u8"equipment and the bodies of lab technicians who didn\u2019t survive the accident. You see your fire engine gravitating "
			u8"toward",
			u8"you as you accelerate into a tunnel of light."
		};

		const float paragraphPositionY =
			centeredAlphabetTextNormalizedPositionY + totalOfAlphabetHeights / SCREEN_HEIGHT + screenSizeDistanceFromPreviousRow[3];

		drawDescriptor.mFontSize = 30.5f;
		drawDescriptor.mFontID = gFonts.crimsonSerif;
		const float longestLineLengthInPixels = GetLongestStringLengthInPx(string1, numParagraphLines, drawDescriptor);
		const float longestNormalizedLength = longestLineLengthInPixels / mSettings.mWidth;
		const float centeredParagraphPosition = GetScreenCenteredPosition(longestNormalizedLength);
		float       normalizedYPosition = paragraphPositionY;
		for (int i = 0; i < numParagraphLines; i++)
		{
			sceneTexts.push_back({ string1[i], drawDescriptor, float2(centeredParagraphPosition, normalizedYPosition) });
			normalizedYPosition += gAppUI.MeasureText(string1[i], drawDescriptor).getY() / SCREEN_HEIGHT;
		}
		float paragraphLineHeight = gAppUI.MeasureText(string1[0], drawDescriptor).getY() / SCREEN_HEIGHT;
		// ROW 3 ===================================================================

		// ROW 4 ===================================================================
		// C PROGRAM - HELLO WORLD
		//
		drawDescriptor.mFontColor = GetSkinColorOfProperty(PROP_TEXT, gSceneData.theme);
		static const char* const string2[] = { "#include<stdio.h>", "int main(){", "    printf(\"Hello World!\\n\");", "    return 0;",
											   "}" };
		const int                numCodeLines = sizeof(string2) / sizeof(const char*);

		const float codePositionY = paragraphPositionY + paragraphLineHeight * numParagraphLines + screenSizeDistanceFromPreviousRow[4];

		drawDescriptor.mFontSize = 30.5f;
		drawDescriptor.mFontID = gFonts.monoSpace;
		normalizedYPosition = codePositionY;
		const float centeredCodePosition = GetScreenCenteredPosition(gAppUI.MeasureText(string2[2], drawDescriptor).getX() / SCREEN_WIDTH);

		for (int i = 0; i < numCodeLines; i++)
		{
			sceneTexts.push_back({ string2[i], drawDescriptor, float2(centeredCodePosition, normalizedYPosition) });
			normalizedYPosition += gAppUI.MeasureText(string2[i], drawDescriptor).getY() / SCREEN_HEIGHT;
		}
		// ROW 4 ===================================================================

		// save the UI data to scene data
		gSceneData.sceneTextArray.push_back(sceneTexts);
		sceneTexts.clear();

		// calculate the required scaling factor to fit the text to screen
		if (gSceneData.bFitToScreen)
		{
			FitToScreen((int)SCREEN_WIDTH, (int)SCREEN_HEIGHT);
		}
	}

	void FitToScreen(int SCREEN_WIDTH, int SCREEN_HEIGHT)
	{
		// detect if we need scaling, i.e., if there's offscreen text.
		bool bOffScreenLeft = false;
		bool bOffScreenRight = false;
		bool bOffScreenTop = false;
		bool bOffScreenBottom = false;

		// we currently have only one set of text, hence we'll not use the loop
		// and directly work on the 1st element.
		// this code section needs to be extended for fitting text to screen
		// for different sets of text.
		//
		//for(int textSet = 0; textSet < gSceneData.sceneTextArray.size(); ++ textSet)
		tinystl::vector<ScreenText>& AllSceneText = gSceneData.sceneTextArray.back();

		float offScreenExtentLeft = 0.0f;
		float offScreenExtentRight = 0.0f;
		float offScreenExtentTop = 0.0f;
		float offScreenExtentBottom = 0.0f;
		for (const ScreenText& screenText : AllSceneText)
		{
			const float2 textMeasure = gAppUI.MeasureText(screenText.mText, screenText.mDrawDesc) / float2(SCREEN_WIDTH, SCREEN_HEIGHT);
			const float  textExtentLeft = screenText.mScreenPosition.getX();
			const float  textExtentRight = textExtentLeft + textMeasure.getX();
			const float  textExtentTop = screenText.mScreenPosition.getY();
			const float  textExtentBottom = textExtentTop + textMeasure.getY();

			bOffScreenLeft |= textExtentLeft < 0.0f;
			bOffScreenRight |= textExtentRight > 1.0f;
			bOffScreenTop |= textExtentTop < 0.0f;
			bOffScreenBottom |= textExtentBottom > 1.0f;

			offScreenExtentLeft = min(offScreenExtentLeft, textExtentLeft);
			offScreenExtentRight = max(offScreenExtentRight, textExtentRight - 1.0f);
			offScreenExtentTop = min(offScreenExtentTop, textExtentTop);
			offScreenExtentBottom = max(offScreenExtentBottom, textExtentBottom - 1.0f);
		}

		const bool bOffscreenText = bOffScreenLeft || bOffScreenRight || bOffScreenTop || bOffScreenBottom;
		if (bOffscreenText)
		{
			// calculate the scaling factor
			//
			// based on how much text goes offscreen (offScreenExtentRight/Left), calculate the
			// new scale for width. Without the scaling, if we have offscreen text, we know how much.
			// -> If the screen width is 1.0f, and we have offScreenExtentRight and offScreenExtentLeft,
			//    it means we're currently covering a width of (1.0f + offScreenExtentRight + offScreenExtentLeft)
			//    Let's call the length of the current content L0 := (1.0f + offScreenExtentRight + offScreenExtentLeft)
			// -> We want the content to cover all of the screen, i.e. the new length of content should be L1 = L0 * scalingFactor
			//    We know L1, its the length of the entire screen, hence L1 := 1.0f;
			//
			// We do the same calculation for Y offset, and pick the minimum of the two scaling factors.
			//
			const float contentMarginFromLeftAndRight = 0.02f;
			const float scalingFactorX = bOffScreenLeft || bOffScreenRight
											 ? 1.0f / (1.0f + offScreenExtentRight + (-offScreenExtentLeft) + contentMarginFromLeftAndRight)
											 : 1.0f;

			const float contentMarginFromTopAndBottom = 0.02f;
			const float scalingFactorY = bOffScreenTop || bOffScreenBottom
											 ? 1.0f / (1.0f + offScreenExtentBottom + (-offScreenExtentTop) + contentMarginFromTopAndBottom)
											 : 1.0f;

			float scalingFactor = min(scalingFactorX, scalingFactorY);

			// only center the screen, do not offset from top after fitting
			float2 bias = float2((1.0f - scalingFactor) * 0.5f, 0.0f);

			// adjust screen text to fit to screen
			for (ScreenText& screenText : AllSceneText)
			{
				screenText.mScreenPosition = screenText.mScreenPosition * scalingFactor + bias;
				screenText.mDrawDesc.mFontSize = screenText.mDrawDesc.mFontSize * scalingFactor;
				screenText.mDrawDesc.mFontSpacing = screenText.mDrawDesc.mFontSpacing * scalingFactor;
			}
		}
	}
};

DEFINE_APPLICATION_MAIN(FontRendering)
