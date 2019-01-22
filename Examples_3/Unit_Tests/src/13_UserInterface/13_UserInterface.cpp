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
* The Forge - USER INTERFACE UNIT TEST
*
* The purpose of this demo is to show how to use the UI layer of The Forge,
* covering every feature the UI library has to offer.
*
*********************************************************************************************************/

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"

// Rendering
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Middleware packages
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[FSR_Count] = {
	"../../../src/13_UserInterface/",       // FSR_BinShaders
	"../../../src/13_UserInterface/",       // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/13_UserInterface/",       // FSR_GpuConfig
	"",                                     // FSR_Animation
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

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif
DepthState* pDepth = NULL;

//Buffer*			pProjViewUniformBuffer[gImageCount] = { NULL };
uint32_t gFrameIndex = 0;

//--------------------------------------------------------------------------------------------
// CAMERA CONTROLLER & SYSTEMS (File/Log/UI)
//--------------------------------------------------------------------------------------------
ICameraController* pCameraController = NULL;
FileSystem         gFileSystem;
LogManager         gLogManager;

UIApp         gAppUI;
GuiComponent* pStandaloneControlsGUIWindow = NULL;
GuiComponent* pGroupedGUIWindow = NULL;

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
tinystl::string sContextMenuItems[7] = { "Random Background Color",  "Random Profiler Color",    "Dummy Context Menu Item3",
										 "Dummy Context Menu Item4", "Dummy Context Menu Item5", "Dummy Context Menu Item6",
										 "Dummy Context Menu Item7" };
void            fnItem1Callback()    // sets slider color value: RGBA
{
	// LOGINFO("Contextual: Menu Item 1 Function called.");
	gUIData.mStandalone.mColorForSlider = packColorF32(
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // r
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // g
		2.0f * rand() / (float)RAND_MAX - 1.0f,    // b
		1.0f                                       // a
	);
}
void fnItem2Callback()    // sets debug text's font color: ABGR
{
	//LOGINFO("Contextual: Menu Item 2 Function called.");
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
class UserInterfaceUnitTest: public IApp
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

		// INITIALIZE RESOURCE/DEBUG SYSTEMS
		//
		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif
		finishResourceLoading();

		// INITIALIZE PIPILINE STATES
		//
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		// SETUP THE MAIN CAMERA
		//
		const CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		const vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		const vec3                   lookAt{ 0 };
		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		pCameraController->setMotionParameters(cmp);

		requestMouseCapture(true);
		InputSystem::RegisterInputEvent(cameraInputEvent);

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
		float   dpiScale = getDpiScale().x;
		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.05f };
		vec2    UIPanelSize = vec2(650.f, 1000.f) / dpiScale;
		GuiDesc guiDesc(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		pStandaloneControlsGUIWindow = gAppUI.AddGuiComponent("Right-click me for context menu :)", &guiDesc);

		// Contextual (Context Menu)
		for (int i = 0; i < 7; i++)
			pStandaloneControlsGUIWindow->mContextualMenuLabels.emplace_back(sContextMenuItems[i]);
		pStandaloneControlsGUIWindow->mContextualMenuCallbacks.push_back(fnItem1Callback);
		pStandaloneControlsGUIWindow->mContextualMenuCallbacks.push_back(fnItem2Callback);

		// Let's show the UI demo window
		gAppUI.mShowDemoUiWindow = true;

		{
			// Drop Down
			gFrameTimeDraw.mFontColor = dropDownItemValues[5];    // initial value
			gUIData.mStandalone.mSelectedDropdownItemValue = 5u;
			DropdownWidget DropDown(
				"[Drop Down] Select Text Color", &gUIData.mStandalone.mSelectedDropdownItemValue, dropDownItemNames, dropDownItemValues, 6);
			// Add a callback
			DropDown.pOnDeactivatedAfterEdit = ColorDropDownCallback;

			// Button
			ButtonWidget Button("[Button] Fill the Progress Bar!");
			Button.pOnDeactivatedAfterEdit = ButtonCallback;

			// Progress Bar
			gUIData.mStandalone.mProgressBarValue = 0;
			gUIData.mStandalone.mProgressBarValueMax = 100;
			ProgressBarWidget ProgressBar(
				"[ProgressBar]", &gUIData.mStandalone.mProgressBarValue, gUIData.mStandalone.mProgressBarValueMax);

			// Checkbox
			CheckboxWidget Checkbox("[Checkbox]", &gUIData.mStandalone.mCheckboxToggle);

			// Radio Buttons
			RadioButtonWidget RadioButton0("[Radio Button] 0", &gUIData.mStandalone.mRadioButtonToggle, 0);
			RadioButtonWidget RadioButton1("[Radio Button] 1", &gUIData.mStandalone.mRadioButtonToggle, 1);

			// Grouping UI Elements:
			// This is done via CollapsingHeaderWidget.
			CollapsingHeaderWidget CollapsingSliderWidgets("SLIDERS");

			// Slider<int>
			const int intValMin = -10;
			const int intValMax = +10;
			const int sliderStepSizeI = 1;
			CollapsingSliderWidgets.AddSubWidget(
				SliderIntWidget("[Slider<int>]", &gUIData.mStandalone.mSliderInt, intValMin, intValMax, sliderStepSizeI));

			// Slider<unsigned>
			const unsigned uintValMin = 0;
			const unsigned uintValMax = 100;
			const unsigned sliderStepSizeUint = 5;
			CollapsingSliderWidgets.AddSubWidget(
				SliderUintWidget("[Slider<uint>]", &gUIData.mStandalone.mSliderUint, uintValMin, uintValMax, sliderStepSizeUint));

			// Slider<float w/ step size>
			const float fValMin = 0;
			const float fValMax = 100;
			const float sliderStepSizeF = 0.1f;
			CollapsingSliderWidgets.AddSubWidget(
				SliderFloatWidget("[Slider<float>] Step Size=0.1f", &gUIData.mStandalone.mSliderFloat, fValMin, fValMax, sliderStepSizeF));

			// Slider<float w/ step count>
			const float _fValMin = -100.0f;
			const float _fValMax = +100.0f;
			const int   stepCount = 6;
			CollapsingSliderWidgets.AddSubWidget(SliderFloatWidget(
				"[Slider<float>] Step Count=6", &gUIData.mStandalone.mSliderFloatSteps, _fValMin, _fValMax,
				(_fValMax - _fValMin) / stepCount));

			// Textbox
			strcpy(gUIData.mStandalone.mText, "Edit Here!");
			TextboxWidget Textbox("[Textbox]", gUIData.mStandalone.mText, UserInterfaceUnitTestingData::STRING_SIZE);

			// Color Slider & Picker
			CollapsingHeaderWidget CollapsingColorWidgets("COLOR WIDGETS");

			gUIData.mStandalone.mColorForSlider = packColorF32(0.067f, 0.153f, 0.329f, 1.0f);    // dark blue
			CollapsingColorWidgets.AddSubWidget(ColorSliderWidget("[Color Slider]", &gUIData.mStandalone.mColorForSlider));
			CollapsingColorWidgets.AddSubWidget(ColorPickerWidget("[Color Picker]", &gUIData.mStandalone.mColorForSlider));

			// Register the GUI elements to the Window
			pStandaloneControlsGUIWindow->AddWidget(LabelWidget("[Label] UI Controls"));
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(DropDown);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(Button);
			pStandaloneControlsGUIWindow->AddWidget(ProgressBar);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(Checkbox);
			pStandaloneControlsGUIWindow->AddWidget(RadioButton0);
			pStandaloneControlsGUIWindow->AddWidget(RadioButton1);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(Textbox);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(CollapsingSliderWidgets);
			pStandaloneControlsGUIWindow->AddWidget(SeparatorWidget());
			pStandaloneControlsGUIWindow->AddWidget(CollapsingColorWidgets);
		}

		return true;
	}

	void Exit()
	{
		// wait for rendering to finish before freeing resources
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		destroyCameraController(pCameraController);

		removeDebugRendererInterface();

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

		//for (uint32_t i = 0; i < gImageCount; ++i)
		//{
		//  removeResource(pProjViewUniformBuffer[i]);
		//}

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

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], pDepthBuffer->mDesc.mFormat))
			return false;
#endif

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);

		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		/************************************************************************/
		// Input
		/************************************************************************/
		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);

		/************************************************************************/
		// Scene Update
		/************************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

#if 0    // quick code template in case we decide to user viewProj matrices \
		 // update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat;
#endif

		/************************************************************************/
		// GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
		gProgressBarAnim.Update(deltaTime);
	}

	void Draw()
	{
		static HiresTimer gTimer;
		const vec4        backgroundColor = unpackColorU32(gUIData.mStandalone.mColorForSlider);
		const ClearValue  clearVal = { backgroundColor.getX(), backgroundColor.getY(), backgroundColor.getZ(), backgroundColor.getW() };

		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
		//
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);

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
		loadActions.mClearDepth = { 1.0f, 0 };
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		//
		// world's draw logic goes here - this is a UI unit test so we don't do any world rendering.
		//

		// DRAW THE USER INTERFACE
		//
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		gTimer.GetUSec(true);
#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.Gui(pStandaloneControlsGUIWindow);    // adds the gui element to AppUI::ComponentsToUpdate list
		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
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

	tinystl::string GetName() { return "13_UserInterface"; }

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

	bool addDepthBuffer()
	{
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { 1.0f, 0 };
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > (maxDistance * maxDistance))
		{
			d *= (maxDistance / sqrtf(lenSqr));
		}

		p = d + lookAt;
		pCameraController->moveTo(p);
		pCameraController->lookAt(lookAt);
	}

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(UserInterfaceUnitTest)
