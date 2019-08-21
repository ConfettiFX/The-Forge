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
* The Forge - ANIMATION - BLENDING UNIT TEST
*
* The purpose of this demo is to show how to blend multiple clips using the
* animnation middleware and play them back on a rig
*
*********************************************************************************************************/

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"

// Rendering
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

// Middleware packages
#include "../../../../Middleware_3/Animation/SkeletonBatcher.h"
#include "../../../../Middleware_3/Animation/AnimatedObject.h"
#include "../../../../Middleware_3/Animation/Animation.h"
#include "../../../../Middleware_3/Animation/Clip.h"
#include "../../../../Middleware_3/Animation/ClipController.h"
#include "../../../../Middleware_3/Animation/Rig.h"

#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

// tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

// Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Memory
#include "../../../../Common_3/OS/Interfaces/IMemory.h"

const char* pszBases[FSR_Count] = {
	"../../../src/19_Blending/",            // FSR_BinShaders
	"../../../src/19_Blending/",            // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/19_Blending/",            // FSR_GpuConfig
	"../../../UnitTestResources/",          // FSR_Animation
	"",                                     // FSR_Audio
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t gImageCount = 3;
bool           bToggleMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

uint32_t       gFrameIndex = 0;
Renderer*      pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

#if defined(TARGET_IOS) || defined(__ANDROID__)
VirtualJoystickUI gVirtualJoystick;
#endif
DepthState* pDepth = NULL;

RasterizerState* pPlaneRast = NULL;
RasterizerState* pSkeletonRast = NULL;

Shader*   pSkeletonShader = NULL;
Buffer*   pJointVertexBuffer = NULL;
Buffer*   pBoneVertexBuffer = NULL;
Pipeline* pSkeletonPipeline = NULL;
int       gNumberOfJointPoints;
int       gNumberOfBonePoints;

Shader*        pPlaneDrawShader = NULL;
Buffer*        pPlaneVertexBuffer = NULL;
Pipeline*      pPlaneDrawPipeline = NULL;
RootSignature* pRootSignature = NULL;
DescriptorBinder* pDescriptorBinderPlane = NULL;

struct UniformBlockPlane
{
	mat4 mProjectView;
	mat4 mToWorldMat;
};
UniformBlockPlane gUniformDataPlane;

Buffer* pPlaneUniformBuffer[gImageCount] = { NULL };

//--------------------------------------------------------------------------------------------
// CAMERA CONTROLLER & SYSTEMS (File/Log/UI)
//--------------------------------------------------------------------------------------------

ICameraController* pCameraController = NULL;
UIApp         gAppUI;
GuiComponent* pStandaloneControlsGUIWindow = NULL;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
GpuProfiler* pGpuProfiler = NULL;

//--------------------------------------------------------------------------------------------
// ANIMATION DATA
//--------------------------------------------------------------------------------------------

// AnimatedObjects
AnimatedObject gStickFigureAnimObject;

// Animations
Animation gBlendedAnimation;

// Clip Controllers
ClipController gWalkClipController;
ClipController gJogClipController;
ClipController gRunClipController;

// Clips
Clip gWalkClip;
Clip gJogClip;
Clip gRunClip;

// Rigs
Rig gStickFigureRig;

// SkeletonBatcher
SkeletonBatcher gSkeletonBatcher;

// Filenames
const char* gStickFigureName = "stickFigure/skeleton.ozz";
const char* gWalkClipName = "stickFigure/animations/walk.ozz";
const char* gJogClipName = "stickFigure/animations/jog.ozz";
const char* gRunClipName = "stickFigure/animations/run.ozz";
const char* pPlaneImageFileName = "Skybox_right1";

const int   gSphereResolution = 30;                   // Increase for higher resolution joint spheres
const float gBoneWidthRatio = 0.2f;                   // Determines how far along the bone to put the max width [0,1]
const float gJointRadius = gBoneWidthRatio * 0.5f;    // set to replicate Ozz skeleton

// Timer to get animationsystem update time
static HiresTimer gAnimationUpdateTimer;

//--------------------------------------------------------------------------------------------
// UI DATA
//--------------------------------------------------------------------------------------------
struct UIData
{
	struct BlendParamsData
	{
		float* mBlendRatio;
		bool*  mAutoSetBlendParams;
		float* mWalkClipWeight;
		float* mJogClipWeight;
		float* mRunClipWeight;
		float* mThreshold;
	};
	BlendParamsData mBlendParams;

	// To preserve effects of BlendRatio, mPlay and mLoop will default as true and
	// can only get set if mAutoBlendParams is off
	struct ClipData
	{
		bool   mPlay = true;
		bool   mLoop = true;
		float  mAnimationTime;
		float* mPlaybackSpeed;
	};
	ClipData mWalkClip;
	ClipData mJogClip;
	ClipData mRunClip;

	struct GeneralSettingsData
	{
		bool mShowBindPose = false;
		bool mDrawPlane = true;
	};
	GeneralSettingsData mGeneralSettings;
};
UIData gUIData;

// For this sample mPlay and mLoop must have a callback as a layer of separation
// so that we can control if they can be set by the GUI or not
void WalkClipPlayCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gWalkClipController.SetPlay(gUIData.mWalkClip.mPlay);
	}
	else
	{
		gUIData.mWalkClip.mPlay = true;
	}
}
void WalkClipLoopCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gWalkClipController.SetLoop(gUIData.mWalkClip.mLoop);
	}
	else
	{
		gUIData.mWalkClip.mLoop = true;
	}
}
void JogClipPlayCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gJogClipController.SetPlay(gUIData.mJogClip.mPlay);
	}
	else
	{
		gUIData.mJogClip.mPlay = true;
	}
}
void JogClipLoopCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gJogClipController.SetLoop(gUIData.mJogClip.mLoop);
	}
	else
	{
		gUIData.mJogClip.mLoop = true;
	}
}
void RunClipPlayCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gRunClipController.SetPlay(gUIData.mRunClip.mPlay);
	}
	else
	{
		gUIData.mRunClip.mPlay = true;
	}
}
void RunClipLoopCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gRunClipController.SetLoop(gUIData.mRunClip.mLoop);
	}
	else
	{
		gUIData.mRunClip.mLoop = true;
	}
}

// Hard set the controller's time ratio via callback when it is set in the UI
void WalkClipTimeChangeCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gWalkClipController.SetTimeRatioHard(gUIData.mWalkClip.mAnimationTime);
		gUIData.mWalkClip.mPlay = false;
	}
}
void JogClipTimeChangeCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gJogClipController.SetTimeRatioHard(gUIData.mJogClip.mAnimationTime);
		gUIData.mJogClip.mPlay = false;
	}
}
void RunClipTimeChangeCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gRunClipController.SetTimeRatioHard(gUIData.mRunClip.mAnimationTime);
		gUIData.mRunClip.mPlay = false;
	}
}

// When mAutoSetBlendParams is turned on we need to reset the clip controllers
void AutoSetBlendParamsCallback()
{
	if (*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		// Reset the internal values
		gWalkClipController.Reset();
		gWalkClipController.SetLoop(true);
		gJogClipController.Reset();
		gJogClipController.SetLoop(true);
		gRunClipController.Reset();
		gRunClipController.SetLoop(true);

		// Reset the UI values
		gUIData.mWalkClip.mPlay = true;
		gUIData.mJogClip.mPlay = true;
		gUIData.mRunClip.mPlay = true;
		gUIData.mWalkClip.mLoop = true;
		gUIData.mJogClip.mLoop = true;
		gUIData.mRunClip.mLoop = true;
	}
}

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class Blending: public IApp
{
	public:
	bool Init()
	{
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
		initResourceLoaderInterface(pRenderer);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", FSR_Textures))
			return false;
#endif

		initProfiler(pRenderer);
		profileRegisterInput();

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		// INITIALIZE PIPILINE STATES
		//
		ShaderLoadDesc planeShader = {};
		planeShader.mStages[0] = { "plane.vert", NULL, 0, FSR_SrcShaders };
		planeShader.mStages[1] = { "plane.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &planeShader, &pPlaneDrawShader);
		addShader(pRenderer, &basicShader, &pSkeletonShader);

		Shader*           shaders[] = { pSkeletonShader, pPlaneDrawShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDescPlane = { pRootSignature, 0, 1 };
		addDescriptorBinder(pRenderer, 0, 1, &descriptorBinderDescPlane, &pDescriptorBinderPlane);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pPlaneRast);

		RasterizerStateDesc skeletonRasterizerStateDesc = {};
		skeletonRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &skeletonRasterizerStateDesc, &pSkeletonRast);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		// GENERATE VERTEX BUFFERS
		//

		// Generate joint vertex buffer
		float* pJointPoints;
		generateSpherePoints(&pJointPoints, &gNumberOfJointPoints, gSphereResolution, gJointRadius);

		uint64_t       jointDataSize = gNumberOfJointPoints * sizeof(float);
		BufferLoadDesc jointVbDesc = {};
		jointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		jointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		jointVbDesc.mDesc.mSize = jointDataSize;
		jointVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		jointVbDesc.pData = pJointPoints;
		jointVbDesc.ppBuffer = &pJointVertexBuffer;
		addResource(&jointVbDesc);

		// Need to free memory;
		conf_free(pJointPoints);

		// Generate bone vertex buffer
		float* pBonePoints;
		generateBonePoints(&pBonePoints, &gNumberOfBonePoints, gBoneWidthRatio);

		uint64_t       boneDataSize = gNumberOfBonePoints * sizeof(float);
		BufferLoadDesc boneVbDesc = {};
		boneVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		boneVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boneVbDesc.mDesc.mSize = boneDataSize;
		boneVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		boneVbDesc.pData = pBonePoints;
		boneVbDesc.ppBuffer = &pBoneVertexBuffer;
		addResource(&boneVbDesc);

		// Need to free memory;
		conf_free(pBonePoints);

		//Generate plane vertex buffer
		float planePoints[] = { -10.0f, 0.0f, -10.0f, 1.0f, 0.0f, 0.0f, -10.0f, 0.0f, 10.0f,  1.0f, 1.0f, 0.0f,
								10.0f,  0.0f, 10.0f,  1.0f, 1.0f, 1.0f, 10.0f,  0.0f, 10.0f,  1.0f, 1.0f, 1.0f,
								10.0f,  0.0f, -10.0f, 1.0f, 0.0f, 1.0f, -10.0f, 0.0f, -10.0f, 1.0f, 0.0f, 0.0f };

		uint64_t       planeDataSize = 6 * 6 * sizeof(float);
		BufferLoadDesc planeVbDesc = {};
		planeVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		planeVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		planeVbDesc.mDesc.mSize = planeDataSize;
		planeVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		planeVbDesc.pData = planePoints;
		planeVbDesc.ppBuffer = &pPlaneVertexBuffer;
		addResource(&planeVbDesc);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlockPlane);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pPlaneUniformBuffer[i];
			addResource(&ubDesc);
		}

		/************************************************************************/
		// SETUP ANIMATION STRUCTURES
		/************************************************************************/

		// SKELETON RENDERER
		//

		// Set up details for rendering the skeletons
		SkeletonRenderDesc skeletonRenderDesc = {};
		skeletonRenderDesc.mRenderer = pRenderer;
		skeletonRenderDesc.mSkeletonPipeline = pSkeletonPipeline;
		skeletonRenderDesc.mRootSignature = pRootSignature;
		skeletonRenderDesc.mJointVertexBuffer = pJointVertexBuffer;
		skeletonRenderDesc.mNumJointPoints = gNumberOfJointPoints;
		skeletonRenderDesc.mDrawBones = true;
		skeletonRenderDesc.mBoneVertexBuffer = pBoneVertexBuffer;
		skeletonRenderDesc.mNumBonePoints = gNumberOfBonePoints;

		gSkeletonBatcher.Initialize(skeletonRenderDesc);

		// RIGS
		//
		eastl::string fullPath = FileSystem::FixPath(gStickFigureName, FSR_Animation);

		// Initialize the rig with the path to its ozz file
		gStickFigureRig.Initialize(fullPath.c_str());

		// Add the rig to the list of skeletons to render
		gSkeletonBatcher.AddRig(&gStickFigureRig);

		// CLIPS
		//
		fullPath = FileSystem::FixPath(gWalkClipName, FSR_Animation);

		gWalkClip.Initialize(fullPath.c_str(), &gStickFigureRig);

		fullPath = FileSystem::FixPath(gJogClipName, FSR_Animation);

		gJogClip.Initialize(fullPath.c_str(), &gStickFigureRig);

		fullPath = FileSystem::FixPath(gRunClipName, FSR_Animation);

		gRunClip.Initialize(fullPath.c_str(), &gStickFigureRig);

		// CLIP CONTROLLERS
		//

		// Initialize with the length of the animation they are controlling and an
		// optional external time to set based on their updating
		gWalkClipController.Initialize(gWalkClip.GetDuration(), &gUIData.mWalkClip.mAnimationTime);
		gJogClipController.Initialize(gJogClip.GetDuration(), &gUIData.mJogClip.mAnimationTime);
		gRunClipController.Initialize(gRunClip.GetDuration(), &gUIData.mRunClip.mAnimationTime);

		// ANIMATIONS
		//

		// Set up the description of how these clips parameters will be auto blended
		AnimationDesc animationDesc{};
		animationDesc.mRig = &gStickFigureRig;
		animationDesc.mNumLayers = 3;

		animationDesc.mLayerProperties[0].mClip = &gWalkClip;
		animationDesc.mLayerProperties[0].mClipController = &gWalkClipController;

		animationDesc.mLayerProperties[1].mClip = &gJogClip;
		animationDesc.mLayerProperties[1].mClipController = &gJogClipController;

		animationDesc.mLayerProperties[2].mClip = &gRunClip;
		animationDesc.mLayerProperties[2].mClipController = &gRunClipController;

		animationDesc.mBlendType = BlendType::CROSS_DISSOLVE_SYNC;

		gBlendedAnimation.Initialize(animationDesc);

		// ANIMATED OBJECTS
		//
		gStickFigureAnimObject.Initialize(&gStickFigureRig, &gBlendedAnimation);

		/************************************************************************/

		finishResourceLoading();

		// SETUP THE MAIN CAMERA
		//
		CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
		vec3                   camPos{ -1.3f, 1.8f, 3.8f };
		vec3                   lookAt{ 1.2f, 0.0f, 0.4f };

		pCameraController = createFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);
#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif

		InputSystem::RegisterInputEvent(cameraInputEvent);

		// INITIALIZE THE USER INTERFACE
		//
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		// Add the GUI Panels/Windows
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 14 };

		vec2    UIPosition = { mSettings.mWidth * 0.1f, mSettings.mHeight * 0.01f };
		vec2    UIPanelSize = { 650, 1000 };
		GuiDesc guiDesc(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		pStandaloneControlsGUIWindow = gAppUI.AddGuiComponent("Blended Animation", &guiDesc);
    pStandaloneControlsGUIWindow->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler));

		// SET gUIData MEMBERS THAT NEED POINTERS TO ANIMATION DATA
		//

		// Blend Params
		gUIData.mBlendParams.mBlendRatio = gBlendedAnimation.GetBlendRatioPtr();
		gUIData.mBlendParams.mAutoSetBlendParams = gBlendedAnimation.GetAutoSetBlendParamsPtr();

		gUIData.mBlendParams.mWalkClipWeight = gWalkClipController.GetWeightPtr();
		gUIData.mBlendParams.mJogClipWeight = gJogClipController.GetWeightPtr();
		gUIData.mBlendParams.mRunClipWeight = gRunClipController.GetWeightPtr();

		gUIData.mBlendParams.mThreshold = gBlendedAnimation.GetThresholdPtr();

		// Walk Clip
		gUIData.mWalkClip.mPlaybackSpeed = gWalkClipController.GetPlaybackSpeedPtr();

		// Jog Clip
		gUIData.mJogClip.mPlaybackSpeed = gJogClipController.GetPlaybackSpeedPtr();

		// Run Clip
		gUIData.mRunClip.mPlaybackSpeed = gRunClipController.GetPlaybackSpeedPtr();

		// SET UP GUI BASED ON gUIData STRUCT
		//
		{
			// BLEND PARAMETERS
			//
			CollapsingHeaderWidget CollapsingBlendParamsWidgets("Blend Parameters");

			// BlendRatio - Slider
			float fValMin = 0.0f;
			float fValMax = 1.0f;
			float sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Blend Ratio", gUIData.mBlendParams.mBlendRatio, fValMin, fValMax, sliderStepSize));

			// AutoSetBlendParams - Checkbox
			CheckboxWidget CheckboxAutoSetBlendParams("Auto Set Blend Params", gUIData.mBlendParams.mAutoSetBlendParams);
			CheckboxAutoSetBlendParams.pOnEdited = AutoSetBlendParamsCallback;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(CheckboxAutoSetBlendParams);

			// Walk Clip Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Clip Weight [Walk]", gUIData.mBlendParams.mWalkClipWeight, fValMin, fValMax, sliderStepSize));

			// Jog Clip Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Clip Weight [Jog]", gUIData.mBlendParams.mJogClipWeight, fValMin, fValMax, sliderStepSize));

			// Run Clip Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Clip Weight [Run]", gUIData.mBlendParams.mRunClipWeight, fValMin, fValMax, sliderStepSize));

			// Threshold - Slider
			fValMin = 0.01f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Threshold", gUIData.mBlendParams.mThreshold, fValMin, fValMax, sliderStepSize));
			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());

			// WALK CLIP
			//
			CollapsingHeaderWidget CollapsingWalkClipWidgets("Walk Clip");

			// Play/Pause - Checkbox
			CheckboxWidget CheckboxWalkPlay("Play", &gUIData.mWalkClip.mPlay);
			CheckboxWalkPlay.pOnEdited = WalkClipPlayCallback;

			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(CheckboxWalkPlay);

			// Loop - Checkbox
			CheckboxWidget CheckboxWalkLoop("Loop", &gUIData.mWalkClip.mLoop);
			CheckboxWalkLoop.pOnEdited = WalkClipLoopCallback;

			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(CheckboxWalkLoop);

			// Animation Time - Slider
			fValMin = 0.0f;
			fValMax = gWalkClipController.GetDuration();
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderWalkAnimationTime(
				"Animation Time", &gUIData.mWalkClip.mAnimationTime, fValMin, fValMax, sliderStepSize);
			SliderWalkAnimationTime.pOnActive = WalkClipTimeChangeCallback;

			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(SliderWalkAnimationTime);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(
				SliderFloatWidget("Playback Speed", gUIData.mWalkClip.mPlaybackSpeed, fValMin, fValMax, sliderStepSize));

			// JOG CLIP
			//
			CollapsingHeaderWidget CollapsingJogClipWidgets("Jog Clip");

			// Play/Pause - Checkbox
			CheckboxWidget CheckboxJogPlay("Play", &gUIData.mJogClip.mPlay);
			CheckboxJogPlay.pOnEdited = JogClipPlayCallback;

			CollapsingJogClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingJogClipWidgets.AddSubWidget(CheckboxJogPlay);

			// Loop - Checkbox
			CheckboxWidget CheckboxJogLoop("Loop", &gUIData.mJogClip.mLoop);
			CheckboxJogLoop.pOnEdited = JogClipLoopCallback;

			CollapsingJogClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingJogClipWidgets.AddSubWidget(CheckboxJogLoop);

			// Animation Time - Slider
			fValMin = 0.0f;
			fValMax = gJogClipController.GetDuration();
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderJogAnimationTime("Animation Time", &gUIData.mJogClip.mAnimationTime, fValMin, fValMax, sliderStepSize);
			SliderJogAnimationTime.pOnActive = JogClipTimeChangeCallback;

			CollapsingJogClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingJogClipWidgets.AddSubWidget(SliderJogAnimationTime);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			CollapsingJogClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingJogClipWidgets.AddSubWidget(
				SliderFloatWidget("Playback Speed", gUIData.mJogClip.mPlaybackSpeed, fValMin, fValMax, sliderStepSize));

			// RUN CLIP
			//
			CollapsingHeaderWidget CollapsingRunClipWidgets("Run Clip");

			// Play/Pause - Checkbox
			CheckboxWidget CheckboxRunPlay("Play", &gUIData.mRunClip.mPlay);
			CheckboxRunPlay.pOnEdited = RunClipPlayCallback;

			CollapsingRunClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingRunClipWidgets.AddSubWidget(CheckboxRunPlay);

			// Loop - Checkbox
			CheckboxWidget CheckboxRunLoop("Loop", &gUIData.mRunClip.mLoop);
			CheckboxRunLoop.pOnEdited = RunClipLoopCallback;

			CollapsingRunClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingRunClipWidgets.AddSubWidget(CheckboxRunLoop);

			// Animation Time - Slider
			fValMin = 0.0f;
			fValMax = gRunClipController.GetDuration();
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderRunAnimationTime("Animation Time", &gUIData.mRunClip.mAnimationTime, fValMin, fValMax, sliderStepSize);
			SliderRunAnimationTime.pOnActive = RunClipTimeChangeCallback;

			CollapsingRunClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingRunClipWidgets.AddSubWidget(SliderRunAnimationTime);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			CollapsingRunClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingRunClipWidgets.AddSubWidget(
				SliderFloatWidget("Playback Speed", gUIData.mRunClip.mPlaybackSpeed, fValMin, fValMax, sliderStepSize));
			CollapsingRunClipWidgets.AddSubWidget(SeparatorWidget());

			// GENERAL SETTINGS
			//
			CollapsingHeaderWidget CollapsingGeneralSettingsWidgets("General Settings");

			// ShowBindPose - Checkbox
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingGeneralSettingsWidgets.AddSubWidget(CheckboxWidget("Show Bind Pose", &gUIData.mGeneralSettings.mShowBindPose));

			// DrawPlane - Checkbox
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingGeneralSettingsWidgets.AddSubWidget(CheckboxWidget("Draw Plane", &gUIData.mGeneralSettings.mDrawPlane));
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());

			// Add all widgets to the window
			pStandaloneControlsGUIWindow->AddWidget(CollapsingBlendParamsWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingWalkClipWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingJogClipWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingRunClipWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingGeneralSettingsWidgets);
		}

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitProfiler();

		// Animation data
		gSkeletonBatcher.Destroy();
		gStickFigureRig.Destroy();
		gWalkClip.Destroy();
		gJogClip.Destroy();
		gRunClip.Destroy();
		gBlendedAnimation.Destroy();
		gStickFigureAnimObject.Destroy();

		destroyCameraController(pCameraController);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Exit();
#endif

		removeGpuProfiler(pRenderer, pGpuProfiler);

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pPlaneUniformBuffer[i]);
		}
		removeResource(pJointVertexBuffer);
		removeResource(pBoneVertexBuffer);
		removeResource(pPlaneVertexBuffer);

		removeShader(pRenderer, pSkeletonShader);
		removeShader(pRenderer, pPlaneDrawShader);
		removeRootSignature(pRenderer, pRootSignature);
		removeDescriptorBinder(pRenderer, pDescriptorBinderPlane);

		removeDepthState(pDepth);
		removeRasterizerState(pSkeletonRast);
		removeRasterizerState(pPlaneRast);

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

#if defined(TARGET_IOS) || defined(__ANDROID__)
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif
		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);

		//layout and pipeline for skeleton draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);


		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSkeletonShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pSkeletonRast;
		addPipeline(pRenderer, &desc, &pSkeletonPipeline);

		// Update the mSkeletonPipeline pointer now that the pipeline has been loaded
		gSkeletonBatcher.LoadPipeline(pSkeletonPipeline);

		//layout and pipeline for plane draw
		vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = pPlaneRast;
		pipelineSettings.pShaderProgram = pPlaneDrawShader;
		addPipeline(pRenderer, &desc, &pPlaneDrawPipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		unloadProfiler();
#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Unload();
#endif

		removePipeline(pRenderer, pPlaneDrawPipeline);
		removePipeline(pRenderer, pSkeletonPipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		/************************************************************************/
		// Input
		/************************************************************************/
		if (InputSystem::GetBoolInput(KEY_BUTTON_X_TRIGGERED))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);

		/************************************************************************/
		// Scene Update
		/************************************************************************/

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		mat4        projViewMat = projMat * viewMat;

		vec3 lightPos = vec3(0.0f, 10.0f, 2.0f);
		vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

		/************************************************************************/
		// Animation
		/************************************************************************/
		gAnimationUpdateTimer.Reset();

		// Update the animated object for this frame
		if (!gStickFigureAnimObject.Update(deltaTime))
			InfoMsg("Animation NOT Updating!");

		if (!gUIData.mGeneralSettings.mShowBindPose)
		{
			// Pose the rig based on the animated object's updated values
			gStickFigureAnimObject.PoseRig();
		}
		else
		{
			// Ignore the updated values and pose in bind
			gStickFigureAnimObject.PoseRigInBind();
		}

		// Record animation update time
		gAnimationUpdateTimer.GetUSec(true);

		// Update uniforms that will be shared between all skeletons
		gSkeletonBatcher.SetSharedUniforms(projViewMat, lightPos, lightColor);

		/************************************************************************/
		// Plane
		/************************************************************************/
		gUniformDataPlane.mProjectView = projViewMat;
		gUniformDataPlane.mToWorldMat = mat4::identity();


    // ProfileSetDisplayMode()
    // TODO: need to change this better way 
    if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
    {
      Profile& S = *ProfileGet();
      int nValue = bToggleMicroProfiler ? 1 : 0;
      nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
      S.nDisplay = nValue;

      bPrevToggleMicroProfiler = bToggleMicroProfiler;
    }

		/************************************************************************/
		// GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		// CPU profiler timer
		static HiresTimer gTimer;

		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// UPDATE UNIFORM BUFFERS
		//

		// Update all the instanced uniform data for each batch of joints and bones
		gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

		BufferUpdateDesc planeViewProjCbv = { pPlaneUniformBuffer[gFrameIndex], &gUniformDataPlane };
		updateResource(&planeViewProjCbv);

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

		// start gpu frame profiler
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barriers[] =    // wait for resource transition
			{
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
			};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		// bind and clear the render target
		LoadActionsDesc loadActions = {};    // render target clean command
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.39f;
		loadActions.mClearColorValues[0].g = 0.41f;
		loadActions.mClearColorValues[0].b = 0.37f;
		loadActions.mClearColorValues[0].a = 1.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		//// draw plane
		if (gUIData.mGeneralSettings.mDrawPlane)
		{
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Plane");
			cmdBindPipeline(cmd, pPlaneDrawPipeline);

			DescriptorData params[1] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pPlaneUniformBuffer[gFrameIndex];
			cmdBindDescriptors(cmd, pDescriptorBinderPlane, pRootSignature, 1, params);
			cmdBindVertexBuffer(cmd, 1, &pPlaneVertexBuffer, NULL);
			cmdDraw(cmd, 6, 0);
			cmdEndDebugMarker(cmd);
		}

		//// draw the skeleton of the rig
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons");
		gSkeletonBatcher.Draw(cmd, gFrameIndex);
		cmdEndDebugMarker(cmd);

		//// draw the UI
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		gTimer.GetUSec(true);
#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.Gui(pStandaloneControlsGUIWindow);    // adds the gui element to AppUI::ComponentsToUpdate list
		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);
		gAppUI.DrawText(
			cmd, float2(8, 65), eastl::string().sprintf("Animation Update %f ms", gAnimationUpdateTimer.GetUSecAverage() / 1000.0f).c_str(),
			&gFrameTimeDraw);

		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);

		cmdDrawProfiler(cmd);
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		// PRESENT THE GRPAHICS QUEUE
		//
		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
	}

	const char* GetName() { return "19_Blending"; }

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
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		//depthRT.mClearValue = { 1.0f, 0 };
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

DEFINE_APPLICATION_MAIN(Blending)
