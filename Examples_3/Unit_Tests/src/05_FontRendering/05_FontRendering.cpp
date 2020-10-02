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

/********************************************************************************************************/
/* THE FORGE - FONT RENDERING DEMO
*
* The purpose of this demo is to show how to use the font system Fontstash with The Forge.
* All the features the font library supports are showcased here, such as font spacing, blurring,
* different text sizes and different fonts. It also contains sample code for simple layout operations.
*********************************************************************************************************/

// tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

// Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Input
// Memory
#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // NOTE: should be the last include in a .cpp!

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
	eastl::string mText;
	TextDrawDesc    mDrawDesc;

	// screen space position:
	// [0, 0] = top left
	// [1, 1] = bottom right
	float2 mScreenPosition;
};

struct SceneData
{
	size_t                                       sceneTextArrayIndex = 0;
	eastl::vector<eastl::vector<ScreenText>> sceneTextArray;

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
ProfileToken   gGpuProfileToken;

Renderer*    pRenderer = NULL;
Queue*       pGraphicsQueue = NULL;
CmdPool*     pCmdPools[gImageCount];
Cmd*         pCmds[gImageCount];

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;

SceneData  gSceneData;
Fonts      gFonts;

/************************************************************************/
/* APP UI VARIABLES
*************************************************************************/
#if defined(TARGET_IOS) || defined(ANDROID)
const int TextureAtlasDimension = 512;
#elif defined(XBOX)
const int TextureAtlasDimension = 1024;
#else    // PC / LINUX / MAC
const int TextureAtlasDimension = 2048;
#endif
UIApp         gAppUI(TextureAtlasDimension);
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
//static float2 gBias(0.0f, 0.0f);
static const ClearValue gLightBackgroundColor = { { 1.0f, 1.0f, 1.0f, 1.0f } };
static const ClearValue gDarkBackgroundColor = { { 0.05f, 0.05f, 0.05f, 1.0f } };
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
        // FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,	"Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES,	"CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,		"GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,			"Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,			"Fonts");
        
		// window and renderer setup
		RendererDesc rendererDesc = { 0 };
		initRenderer(GetName(), &rendererDesc, &pRenderer);
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
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		waitForAllResourceLoads();

		// initialize UI middleware
		if (!gAppUI.Init(pRenderer))
			return false;    // report?

		// load the fonts
		gFonts.titilliumBold = gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");
		gFonts.comicRelief = gAppUI.LoadFont("ComicRelief/ComicRelief.ttf");
		gFonts.crimsonSerif = gAppUI.LoadFont("Crimson/Crimson-Roman.ttf");
		gFonts.monoSpace = gAppUI.LoadFont("InconsolataLGC/Inconsolata-LGC.otf");
		gFonts.monoSpaceBold = gAppUI.LoadFont("InconsolataLGC/Inconsolata-LGC-Bold.otf");

		// setup the UI window		
		vec2        UIWndSize = vec2{ 250, 300 };
		vec2        UIWndPosition = vec2{ mSettings.mWidth * 0.01f, mSettings.mHeight * 0.5f };
		GuiDesc     guiDesc(UIWndPosition, UIWndSize, TextDrawDesc());
		pUIWindow = gAppUI.AddGuiComponent("Controls", &guiDesc);

		CheckboxWidget fitScreenCheckbox("Fit to Screen", &gSceneData.bFitToScreen);

		const size_t   NUM_THEMES = sizeof(pThemeLabels) / sizeof(const char*) - 1;    // -1 for the NULL element
		DropdownWidget ThemeDropdown("Theme", &gSceneData.theme, pThemeLabels, (uint32_t*)ColorThemes, NUM_THEMES);

		pUIWindow->AddWidget(ThemeDropdown);
		pUIWindow->AddWidget(fitScreenCheckbox);

		gPreviousTheme = gSceneData.theme;

		if (!initInputSystem(pWindow))
			return false;

		initProfiler();

        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

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
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		exitProfiler();

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
	}

	bool Load()
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
		swapChainDesc.mColorClearValue = gSceneData.theme ? gDarkBackgroundColor : gLightBackgroundColor;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		if (!pSwapChain)
			return false;

		InitializeSceneText();

		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);
		unloadProfilerUI();
		gAppUI.Unload();
		removeSwapChain(pRenderer, pSwapChain);
		gSceneData.sceneTextArray.set_capacity(0);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

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

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = gSceneData.theme ? gDarkBackgroundColor : gLightBackgroundColor;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		// draw text (uses AppUI)
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Render Text");

		if (!gSceneData.sceneTextArray.empty())
		{
			const eastl::vector<ScreenText>& texts = gSceneData.sceneTextArray[gSceneData.sceneTextArrayIndex];
			for (int i = 0; i < texts.size(); ++i)
			{
				const float2 pxPosition = texts[i].mScreenPosition * float2(mSettings.mWidth, mSettings.mHeight);
				gAppUI.DrawText(cmd, pxPosition, texts[i].mText.c_str(), &texts[i].mDrawDesc);
			}
		}

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// draw profiler timings text (uses debugText)
		TextDrawDesc uiTextDesc;    // default
		uiTextDesc.mFontColor = gSceneData.theme ? 0xff21D8DE : 0xff444444;
		uiTextDesc.mFontSize = 18;
        
#if !defined(__ANDROID__)
		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.f, 15.f), &uiTextDesc);
        cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 30.f), gGpuProfileToken, &uiTextDesc);
#else
		cmdDrawCpuProfile(cmd, float2(8.f, 15.f), &uiTextDesc);
#endif

		if (gbShowSceneControlsUIWindow)
			gAppUI.Gui(pUIWindow);

		cmdDrawProfilerUI();
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
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
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "05_FontRendering"; }

	void InitializeSceneText()
	{
		gSceneData.sceneTextArray.clear();
		eastl::vector<ScreenText> sceneTexts;
		TextDrawDesc                drawDescriptor;
		const char*                 txt = "";

		const float SCREEN_WIDTH = (float)mSettings.mWidth;
		const float SCREEN_HEIGHT = (float)mSettings.mHeight;

		//float2 dpiScaling = getDpiScale();

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
		FontPropertyValue fontPropertyValues[numElements];
		fontPropertyValues[0].i = 0;
		fontPropertyValues[1].i = 1;
		fontPropertyValues[2].i = 2;
		fontPropertyValues[3].i = 4;

		fontPropertyValues[4].i = 0;
		fontPropertyValues[5].i = 1;
		fontPropertyValues[6].i = 2;
		fontPropertyValues[7].i = 4;

		// note:
			// cannot initialize the union's int variable like this
			// need to explicitly assign the int variable outisde the
			// initializer list. initilize to 0.0f for now.
			//
			// 0.0f,    // ((int)0xff0000dd),
			// 0.0f,    // ((int)0xff00dd00),
			// 0.0f,    // ((int)0xffdd5050),
			// 0.0f,    // ((int)0xff888888)
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
		eastl::vector<ScreenText>& AllSceneText = gSceneData.sceneTextArray.back();

		float offScreenExtentLeft = 0.0f;
		float offScreenExtentRight = 0.0f;
		float offScreenExtentTop = 0.0f;
		float offScreenExtentBottom = 0.0f;
		for (const ScreenText& screenText : AllSceneText)
		{
			const float2 textMeasure =
				gAppUI.MeasureText(screenText.mText.c_str(), screenText.mDrawDesc) / float2(SCREEN_WIDTH, SCREEN_HEIGHT);
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
