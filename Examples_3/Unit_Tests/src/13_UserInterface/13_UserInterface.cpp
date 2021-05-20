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

/********************************************************************************************************
*
* The Forge - USER INTERFACE UNIT TEST
*
* The purpose of this demo is to show how to use the UI layer of The Forge,
* covering every feature the UI library has to offer.
*
*********************************************************************************************************/

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
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

Texture*	  pSpriteTexture = NULL;

//Buffer*			pProjViewUniformBuffer[gImageCount] = { NULL };
uint32_t gFrameIndex = 0;

//--------------------------------------------------------------------------------------------
// CAMERA CONTROLLER & SYSTEMS (File/Log/UI)
//--------------------------------------------------------------------------------------------
UIApp*        pAppUI = NULL;
GuiComponent* pStandaloneControlsGUIWindow = NULL;
GuiComponent* pGroupedGUIWindow = NULL;
static uint32_t gSelectedApiIndex = 0;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00dddd, 18);
//--------------------------------------------------------------------------------------------
// UI UNIT TEST DATA
//--------------------------------------------------------------------------------------------

typedef uint DropDownItemDataType;

struct UserInterfaceUnitTestingData
{
	static const unsigned STRING_SIZE = 512;

	// Data for Standalone UI Control Window
	struct Standalone
	{
		DropDownItemDataType mSelectedDropdownItemValue;
		int                  mSliderInt;
		unsigned             mSliderUint;
		float                mSliderFloat;
		float                mSliderFloatSteps;
		bool                 mCheckboxToggle;
		int32_t              mRadioButtonToggle;
		char                 mText[STRING_SIZE];
		size_t               mProgressBarValue;
		size_t               mProgressBarValueMax;
		uint                 mColorForSlider;
		const char**         mContextItems;
	} mStandalone;
};
UserInterfaceUnitTestingData gUIData;

// ContextMenu Items and Callbacks Example:
//
const char* sContextMenuItems[7] = { "Random Background Color",  "Random Profiler Color",    "Dummy Context Menu Item3",
										 "Dummy Context Menu Item4", "Dummy Context Menu Item5", "Dummy Context Menu Item6",
										 "Dummy Context Menu Item7" };
void            fnItem1Callback()    // sets slider color value: RGBA
{
	// LOGF(LogLevel::eINFO, "Contextual: Menu Item 1 Function called.");
	gUIData.mStandalone.mColorForSlider = packColorF32(
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // r
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // g
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // b
		1.0f                                       // a
	);
}
void fnItem2Callback()    // sets debug text's font color: ABGR
{
	//LOGF(LogLevel::eINFO, "Contextual: Menu Item 2 Function called.");
	gFrameTimeDraw.mFontColor = packColorF32(
		1.0f,                                      // a
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // b
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // g
		2.0f * rand() / (float)RAND_MAX - 1.0f     // r
	);
}

// DropDown Example:
//
// - dropDownItemNames for legible labels to display in UI
// - dropDownItemValues <- actual data the UI control(dropdown) selects
//
static const char*                dropDownItemNames[] = { "Red", "Green", "Blue", "Yellow", "Cyan", "Orange", NULL };
static const DropDownItemDataType dropDownItemValues[] = {    // can be any type - not only int.
	0xff0000dd,                                               // r
	0xff00dd00,                                               // g
	0xffdd0000,                                               // b
	0xff00dddd,                                               // y
	0xffdddd00,                                               // c
	0xff076fe2,                                               // o
	0
};

// assign the dropdown value to the text's font color
void ColorDropDownCallback() { gFrameTimeDraw.mFontColor = dropDownItemValues[gUIData.mStandalone.mSelectedDropdownItemValue]; }

struct ProgressBarAnimationData
{
	float mFillPeriod = 5.0f;     // filling the progress bar takes 5s
	float mCurrentTime = 0.0f;    // [0.0f, mFillPeriod]
	bool  mbAnimate = true;
	void  Update(float dt)
	{
		if (!mbAnimate)
			return;
		mCurrentTime += dt;    // animate progress bar
		gUIData.mStandalone.mProgressBarValue = (size_t)((mCurrentTime / mFillPeriod) * gUIData.mStandalone.mProgressBarValueMax);
		if (mCurrentTime >= mFillPeriod)
		{
			mbAnimate = false;
			mCurrentTime = 0.0f;

			gUIData.mStandalone.mProgressBarValue = gUIData.mStandalone.mProgressBarValueMax;
		}
	}
} gProgressBarAnim;

// button callback to animate loading bar
void ButtonCallback()
{
	gProgressBarAnim.mCurrentTime = 0.0f;
	gProgressBarAnim.mbAnimate = true;
}

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class UserInterfaceUnitTest : public IApp
{
public:
	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		// WINDOW AND RENDERER SETUP
		pRenderer = NULL;
		RendererDesc settings = { 0 };
		settings.mApi = (RendererApi)gSelectedApiIndex;
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

		// INITIALIZE RESOURCE/DEBUG SYSTEMS
		//
		initResourceLoaderInterface(pRenderer);

		TextureLoadDesc textureDesc = {};
		textureDesc.ppTexture = &pSpriteTexture;
		textureDesc.pFileName = "sprites";
		addResource(&textureDesc, NULL);

		waitForAllResourceLoads();

		// INITIALIZE THE USER INTERFACE

		UIAppDesc appUIDesc = {};
		initAppUI(pRenderer, &appUIDesc, &pAppUI);
		if (!pAppUI)
		{
			// todo: display err msg (multiplatform?)
			return false;
		}

		initAppUIFont(pAppUI, "TitilliumText/TitilliumText-Bold.otf");

		// Add the GUI Panels/Windows
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 16 };

		// Add the UI Controls to the GUI Panel
		//
		//-----------------------------------------------------------------------
		// Note:
		//  We bind the UI controls to data we've declared in the gUIData struct.
		//  The UI controls directly update the variable we bind to it with the
		//  UIProperty() constructor.
		//-----------------------------------------------------------------------

		//-----------------------------------------------------------------------
		// Standalone GUI Controls
		//-----------------------------------------------------------------------		
		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.05f };
		vec2    UIPanelSize = vec2(650.f, 1000.f);
		GuiDesc guiDesc;
		guiDesc.mStartPosition = UIPosition;
		guiDesc.mStartSize = UIPanelSize;
		guiDesc.mDefaultTextDrawDesc = UIPanelWindowTitleTextDesc;
		pStandaloneControlsGUIWindow = addAppUIGuiComponent(pAppUI, "Right-click me for context menu :)", &guiDesc);

#if defined(USE_MULTIPLE_RENDER_APIS)
		static const char* pApiNames[] =
		{
		#if defined(GLES)
			"GLES",
		#endif
		#if defined(DIRECT3D12)
			"D3D12",
		#endif
		#if defined(VULKAN)
			"Vulkan",
		#endif
		#if defined(DIRECT3D11)
			"D3D11",
		#endif
		};
		// Select Api 
		DropdownWidget selectApiWidget;
		selectApiWidget.pData = &gSelectedApiIndex;
		for (uint32_t i = 0; i < RENDERER_API_COUNT; ++i)
		{
			selectApiWidget.mNames.push_back((char*)pApiNames[i]);
			selectApiWidget.mValues.push_back(i);
		}
		IWidget* pSelectApiWidget = addGuiWidget(pStandaloneControlsGUIWindow, "Select API", &selectApiWidget, WIDGET_TYPE_DROPDOWN);
		pSelectApiWidget->pOnEdited = onAPISwitch;
		addWidgetLua(pSelectApiWidget);
		const char* apiTestScript = "Test_API_Switching.lua";
		addAppUITestScripts(pAppUI, &apiTestScript, 1);
#endif

		// Contextual (Context Menu)
		for (int i = 0; i < 7; i++)
		{
			char* menuItem = (char*)tf_calloc(strlen(sContextMenuItems[i]) + 1, sizeof(char));
			strcpy(menuItem, sContextMenuItems[i]);
			pStandaloneControlsGUIWindow->mContextualMenuLabels.emplace_back(menuItem);
		}
		pStandaloneControlsGUIWindow->mContextualMenuCallbacks.push_back(fnItem1Callback);
		pStandaloneControlsGUIWindow->mContextualMenuCallbacks.push_back(fnItem2Callback);

		{
			// Drop Down
			gFrameTimeDraw.mFontColor = dropDownItemValues[5];    // initial value
			gUIData.mStandalone.mSelectedDropdownItemValue = 5u;

			LabelWidget uiControls;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "[Label] UI Controls", &uiControls, WIDGET_TYPE_LABEL));

			SeparatorWidget separator;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

			DropdownWidget DropDown;
			DropDown.pData = &gUIData.mStandalone.mSelectedDropdownItemValue;
			for (uint32_t i = 0; i < 6; ++i)
			{
				DropDown.mNames.push_back((char*)dropDownItemNames[i]);
				DropDown.mValues.push_back(dropDownItemValues[i]);
			}

			IWidget* pDropDown = addGuiWidget(pStandaloneControlsGUIWindow, "[Drop Down] Select Text Color", &DropDown, WIDGET_TYPE_DROPDOWN);
			// Add a callback
			pDropDown->pOnDeactivatedAfterEdit = ColorDropDownCallback;
			// Register lua
			addWidgetLua(pDropDown);

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

			// Button
			ButtonWidget Button;
			IWidget* pButton = addGuiWidget(pStandaloneControlsGUIWindow, "[Button] Fill the Progress Bar!", &Button, WIDGET_TYPE_BUTTON);
			pButton->pOnDeactivatedAfterEdit = ButtonCallback;

			// Progress Bar
			gUIData.mStandalone.mProgressBarValue = 0;
			gUIData.mStandalone.mProgressBarValueMax = 100;
			ProgressBarWidget ProgressBar;
			ProgressBar.pData = &gUIData.mStandalone.mProgressBarValue;
			ProgressBar.mMaxProgress = gUIData.mStandalone.mProgressBarValueMax;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "[ProgressBar]", &ProgressBar, WIDGET_TYPE_PROGRESS_BAR));

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

			// Checkbox
			CheckboxWidget Checkbox;
			Checkbox.pData = &gUIData.mStandalone.mCheckboxToggle;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "[Checkbox]", &Checkbox, WIDGET_TYPE_CHECKBOX));

			// Radio Buttons
			RadioButtonWidget RadioButton0;
			RadioButton0.pData = &gUIData.mStandalone.mRadioButtonToggle;
			RadioButton0.mRadioId = 0;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "[Radio Button] 0", &RadioButton0, WIDGET_TYPE_RADIO_BUTTON));

			RadioButtonWidget RadioButton1;
			RadioButton1.pData = &gUIData.mStandalone.mRadioButtonToggle;
			RadioButton1.mRadioId = 1;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "[Radio Button] 1", &RadioButton1, WIDGET_TYPE_RADIO_BUTTON));

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

			// Textbox
			strcpy(gUIData.mStandalone.mText, "Edit Here!");
			TextboxWidget Textbox;
			Textbox.pData = gUIData.mStandalone.mText;
			Textbox.mLength = UserInterfaceUnitTestingData::STRING_SIZE;
			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "[Textbox]", &Textbox, WIDGET_TYPE_TEXTBOX));

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

			// Grouping UI Elements:
			// This is done via CollapsingHeaderWidget.
			CollapsingHeaderWidget CollapsingSliderWidgets;

			// Slider<int>
			const int intValMin = -10;
			const int intValMax = +10;
			const int sliderStepSizeI = 1;

			SliderIntWidget sliderInt;
			sliderInt.pData = &gUIData.mStandalone.mSliderInt;
			sliderInt.mMin = intValMin;
			sliderInt.mMax = intValMax;
			sliderInt.mStep = sliderStepSizeI;
			addCollapsingHeaderSubWidget(&CollapsingSliderWidgets, "[Slider<int>]", &sliderInt, WIDGET_TYPE_SLIDER_INT);

			// Slider<unsigned>
			const unsigned uintValMin = 0;
			const unsigned uintValMax = 100;
			const unsigned sliderStepSizeUint = 5;

			SliderUintWidget sliderUint;
			sliderUint.pData = &gUIData.mStandalone.mSliderUint;
			sliderUint.mMin = uintValMin;
			sliderUint.mMax = uintValMax;
			sliderUint.mStep = sliderStepSizeUint;
			addCollapsingHeaderSubWidget(&CollapsingSliderWidgets, "[Slider<uint>]", &sliderUint, WIDGET_TYPE_SLIDER_UINT);

			// Slider<float w/ step size>
			const float fValMin = 0;
			const float fValMax = 100;
			const float sliderStepSizeF = 0.1f;

			SliderFloatWidget sliderFloat;
			sliderFloat.pData = &gUIData.mStandalone.mSliderFloat;
			sliderFloat.mMin = fValMin;
			sliderFloat.mMax = fValMax;
			sliderFloat.mStep = sliderStepSizeF;
			addCollapsingHeaderSubWidget(&CollapsingSliderWidgets, "[Slider<float>] Step Size=0.1f", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

			// Slider<float w/ step count>
			const float _fValMin = -100.0f;
			const float _fValMax = +100.0f;
			const int   stepCount = 6;

			sliderFloat.pData = &gUIData.mStandalone.mSliderFloatSteps;
			sliderFloat.mMin = _fValMin;
			sliderFloat.mMax = _fValMax;
			sliderFloat.mStep = (_fValMax - _fValMin) / stepCount;
			addCollapsingHeaderSubWidget(&CollapsingSliderWidgets, "[Slider<float>] Step Count=6", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "SLIDERS", &CollapsingSliderWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "", &separator, WIDGET_TYPE_SEPARATOR));

			// Color Slider & Picker
			CollapsingHeaderWidget CollapsingColorWidgets;

			gUIData.mStandalone.mColorForSlider = packColorF32(0.067f, 0.153f, 0.329f, 1.0f);    // dark blue

			ColorSliderWidget colorSlider;
			colorSlider.pData = &gUIData.mStandalone.mColorForSlider;
			addCollapsingHeaderSubWidget(&CollapsingColorWidgets, "[Color Slider]", &colorSlider, WIDGET_TYPE_COLOR_SLIDER);

			ColorPickerWidget colorPicker;
			colorPicker.pData = &gUIData.mStandalone.mColorForSlider;
			addCollapsingHeaderSubWidget(&CollapsingColorWidgets, "[Color Picker]", &colorPicker, WIDGET_TYPE_COLOR_PICKER);

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "COLOR WIDGETS", &CollapsingColorWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

			// Texture preview
			CollapsingHeaderWidget CollapsingTexPreviewWidgets;

			DebugTexturesWidget dbgTexWidget;
			eastl::vector<Texture*> texPreviews;
			texPreviews.push_back(pSpriteTexture);
			dbgTexWidget.mTextures = texPreviews;
			dbgTexWidget.mTextureDisplaySize = float2(441, 64);

			addCollapsingHeaderSubWidget(&CollapsingTexPreviewWidgets, "Texture Preview", &dbgTexWidget, WIDGET_TYPE_DEBUG_TEXTURES);

			addWidgetLua(addGuiWidget(pStandaloneControlsGUIWindow, "TEXTURE PREVIEW", &CollapsingTexPreviewWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
		}
		
		if (!initInputSystem(pWindow))
			return false;

		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		actionDesc =
		{
			InputBindings::BUTTON_ANY, [](InputActionContext* ctx)
			{
				static uint8_t virtualKeyboard = 0;
				bool capture = appUIOnButton(pAppUI, ctx->mBinding, ctx->mBool, ctx->pPosition);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				if (appUIWantTextInput(pAppUI) != virtualKeyboard)
				{
					virtualKeyboard = appUIWantTextInput(pAppUI);
					setVirtualKeyboard(virtualKeyboard);
				}
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::TEXT, [](InputActionContext* ctx) { return appUIOnText(pAppUI, ctx->pText); } };
		addInputAction(&actionDesc);

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		for (char* item : pStandaloneControlsGUIWindow->mContextualMenuLabels)
			tf_free(item);

		exitAppUI(pAppUI);

		removeResource(pSpriteTexture);

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
		exitRenderer(pRenderer);
	}

	bool Load()
	{
		// INITIALIZE SWAP-CHAIN AND DEPTH BUFFER
		//
		if (!addSwapChain())
			return false;

		// LOAD USER INTERFACE
		//
		if (!addAppGUIDriver(pAppUI, pSwapChain->ppRenderTargets))
			return false;

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		removeAppGUIDriver(pAppUI);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);
		/************************************************************************/
		// GUI
		/************************************************************************/
		updateAppUI(pAppUI, deltaTime);
		gProgressBarAnim.Update(deltaTime);
	}

	void Draw()
	{
		static HiresTimer gTimer;
		const vec4        backgroundColor = unpackColorU32(gUIData.mStandalone.mColorForSlider);
		ClearValue  clearVal;
		clearVal.r = backgroundColor.getX();
		clearVal.g = backgroundColor.getY();
		clearVal.b = backgroundColor.getZ();
		clearVal.a = backgroundColor.getW();

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

		//
		// world's draw logic goes here - this is a UI unit test so we don't do any world rendering.
		//

		// DRAW THE USER INTERFACE
		//
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		gTimer.GetUSec(true);

		char text[64];
		sprintf(text, "CPU %f ms", gTimer.GetUSecAverage() / 1000.0f);

		appUIGui(pAppUI, pStandaloneControlsGUIWindow);    // adds the gui element to AppUI::ComponentsToUpdate list

		float2 screenCoords(8, 15);
		drawAppUIText(pAppUI, 
			cmd, &screenCoords, text, &gFrameTimeDraw);
		drawAppUI(pAppUI, cmd);

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

	const char* GetName() { return "13_UserInterface"; }

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

DEFINE_APPLICATION_MAIN(UserInterfaceUnitTest)
