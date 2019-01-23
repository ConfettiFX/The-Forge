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
* The Forge - ANIMATION - PARTIAL BLENDING UNIT TEST
*
* The purpose of this demo is to show how to blend clips using the
* animnation middleware, having them only effect certain joints
*
*********************************************************************************************************/

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"

// Rendering
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"

// Middleware packages
#include "../../../../Middleware_3/Animation/SkeletonBatcher.h"
#include "../../../../Middleware_3/Animation/AnimatedObject.h"
#include "../../../../Middleware_3/Animation/Animation.h"
#include "../../../../Middleware_3/Animation/Clip.h"
#include "../../../../Middleware_3/Animation/ClipController.h"
#include "../../../../Middleware_3/Animation/Rig.h"

#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

// tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

// Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Memory
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[FSR_Count] = {
	"../../../src/04_PartialBlending/",     // FSR_BinShaders
	"../../../src/04_PartialBlending/",     // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/04_PartialBlending/",     // FSR_GpuConfig
	"../../../UnitTestResources/",          // FSR_Animtion
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t gImageCount = 3;
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

#ifdef TARGET_IOS
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
FileSystem         gFileSystem;
LogManager         gLogManager;

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

// ClipMasks
ClipMask gStandClipMask;
ClipMask gWalkClipMask;

// ClipControllers
ClipController gStandClipController;
ClipController gWalkClipController;

// Clips
Clip gStandClip;
Clip gWalkClip;

// Rigs
Rig gStickFigureRig;

// SkeletonBatcher
SkeletonBatcher gSkeletonBatcher;

// Filenames
const char* gStickFigureName = "stickFigure/skeleton.ozz";
const char* gStandClipName = "stickFigure/animations/stand.ozz";
const char* gWalkClipName = "stickFigure/animations/walk.ozz";
const char* pPlaneImageFileName = "Skybox_right1.png";

const int   gSphereResolution = 30;                   // Increase for higher resolution joint spheres
const float gBoneWidthRatio = 0.2f;                   // Determines how far along the bone to put the max width [0,1]
const float gJointRadius = gBoneWidthRatio * 0.5f;    // set to replicate Ozz skeleton

// Timer to get animationsystem update time
static HiresTimer gAnimationUpdateTimer;

//--------------------------------------------------------------------------------------------
// UI DATA
//--------------------------------------------------------------------------------------------

const float kDefaultUpperBodyWeight = 1.0f;      // sets mStandJointsWeight and mWalkJointsWeight to their default values
const float kDefaultStandJointsWeight = 1.0f;    // stand clip will only effect children of UpperBodyJointIndex
const float kDefaultWalkJointsWeight = 0.0f;     // walk clip will only effect non-children of UpperBodyJointIndex

const unsigned int kSpineJointIndex = 3;    // index of the spine joint in this specific skeleton

struct UIData
{
	struct BlendParamsData
	{
		float  mUpperBodyWeight = kDefaultUpperBodyWeight;
		bool*  mAutoSetBlendParams;
		float* mStandClipWeight;
		float  mStandJointsWeight = kDefaultStandJointsWeight;
		float* mWalkClipWeight;
		float  mWalkJointsWeight = kDefaultWalkJointsWeight;
		float* mThreshold;
	};
	BlendParamsData mBlendParams;

	unsigned int mUpperBodyJointIndex = kSpineJointIndex;

	struct ClipData
	{
		bool*  mPlay;
		bool*  mLoop;
		float  mAnimationTime;
		float* mPlaybackSpeed;
	};
	ClipData mStandClip;
	ClipData mWalkClip;

	struct GeneralSettingsData
	{
		bool mShowBindPose = false;
		bool mDrawPlane = true;
	};
	GeneralSettingsData mGeneralSettings;
};
UIData gUIData;

// Helper functions for setting the clip masks based on the UI
void SetStandClipJointsWeightWithUIValues()
{
	gStandClipMask.DisableAllJoints();
	gStandClipMask.SetAllChildrenOf(gUIData.mUpperBodyJointIndex, gUIData.mBlendParams.mStandJointsWeight);
}
void SetWalkClipJointsWeightWithUIValues()
{
	gWalkClipMask.EnableAllJoints();
	gWalkClipMask.SetAllChildrenOf(gUIData.mUpperBodyJointIndex, gUIData.mBlendParams.mWalkJointsWeight);
}

// When the upper body weight parameter is changed update the clip mask's joint weights
void UpperBodyWeightCallback()
{
	if (*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gUIData.mBlendParams.mStandJointsWeight = gUIData.mBlendParams.mUpperBodyWeight;
		gUIData.mBlendParams.mWalkJointsWeight = 1.0f - gUIData.mBlendParams.mUpperBodyWeight;

		SetStandClipJointsWeightWithUIValues();
		SetWalkClipJointsWeightWithUIValues();
	}
}

// When the joints weight is changed recreate all the joint weights for the clip mask
void StandClipJointsWeightCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		SetStandClipJointsWeightWithUIValues();
	}
	else
	{
		gUIData.mBlendParams.mStandJointsWeight = gUIData.mBlendParams.mUpperBodyWeight;
	}
}
void WalkClipJointsWeightCallback()
{
	if (!*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		SetWalkClipJointsWeightWithUIValues();
	}
	else
	{
		gUIData.mBlendParams.mWalkJointsWeight = 1.0f - gUIData.mBlendParams.mUpperBodyWeight;
	}
}

// When the upper body root index is changed, update the clip mask's joint weights
void UpperBodyJointIndexCallback()
{
	SetStandClipJointsWeightWithUIValues();
	SetWalkClipJointsWeightWithUIValues();
}

// Hard set the controller's time ratio via callback when it is set in the UI
void StandClipTimeChangeCallback() { gStandClipController.SetTimeRatioHard(gUIData.mStandClip.mAnimationTime); }
void WalkClipTimeChangeCallback() { gWalkClipController.SetTimeRatioHard(gUIData.mWalkClip.mAnimationTime); }

// When mAutoSetBlendParams is turned on we need to reset the joint weights
void AutoSetBlendParamsCallback()
{
	if (*gUIData.mBlendParams.mAutoSetBlendParams)
	{
		gUIData.mBlendParams.mUpperBodyWeight = kDefaultUpperBodyWeight;
		gUIData.mBlendParams.mStandJointsWeight = kDefaultStandJointsWeight;
		gUIData.mBlendParams.mWalkJointsWeight = kDefaultWalkJointsWeight;

		SetStandClipJointsWeightWithUIValues();
		SetWalkClipJointsWeightWithUIValues();
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
		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Absolute))
			return false;
#endif

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

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
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

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
		tinystl::string fullPath = FileSystem::FixPath(gStickFigureName, FSR_Animation);

		// Initialize the rig with the path to its ozz file
		gStickFigureRig.Initialize(fullPath.c_str());

		// Add the rig to the list of skeletons to render
		gSkeletonBatcher.AddRig(&gStickFigureRig);

		// CLIPS
		//
		fullPath = FileSystem::FixPath(gStandClipName, FSR_Animation);

		gStandClip.Initialize(fullPath.c_str(), &gStickFigureRig);

		fullPath = FileSystem::FixPath(gWalkClipName, FSR_Animation);

		gWalkClip.Initialize(fullPath.c_str(), &gStickFigureRig);

		// CLIP CONTROLLERS
		//

		// Initialize with the length of the animation they are controlling and an
		// optional external time to set based on their updating
		gStandClipController.Initialize(gStandClip.GetDuration(), &gUIData.mStandClip.mAnimationTime);
		gWalkClipController.Initialize(gWalkClip.GetDuration(), &gUIData.mWalkClip.mAnimationTime);

		// CLIP MASKS
		//
		gStandClipMask.Initialize(&gStickFigureRig);
		gWalkClipMask.Initialize(&gStickFigureRig);

		// Initialize the masks with their default values
		gStandClipMask.DisableAllJoints();
		gStandClipMask.SetAllChildrenOf(kSpineJointIndex, kDefaultStandJointsWeight);

		gWalkClipMask.EnableAllJoints();
		gWalkClipMask.SetAllChildrenOf(kSpineJointIndex, kDefaultWalkJointsWeight);

		// ANIMATIONS
		//

		// Set up the description of how these clips parameters will be auto blended
		AnimationDesc animationDesc{};
		animationDesc.mRig = &gStickFigureRig;
		animationDesc.mNumLayers = 2;

		animationDesc.mLayerProperties[0].mClip = &gStandClip;
		animationDesc.mLayerProperties[0].mClipController = &gStandClipController;
		animationDesc.mLayerProperties[0].mClipMask = &gStandClipMask;

		animationDesc.mLayerProperties[1].mClip = &gWalkClip;
		animationDesc.mLayerProperties[1].mClipController = &gWalkClipController;
		animationDesc.mLayerProperties[1].mClipMask = &gWalkClipMask;

		animationDesc.mBlendType = BlendType::EQUAL;

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

		requestMouseCapture(true);
		InputSystem::RegisterInputEvent(cameraInputEvent);

		// INITIALIZE THE USER INTERFACE
		//
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		// Add the GUI Panels/Windows
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 14 };

		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.01f };
		vec2    UIPanelSize = { 650, 1000 };
		GuiDesc guiDesc(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		pStandaloneControlsGUIWindow = gAppUI.AddGuiComponent("Partially Blended Animation", &guiDesc);

		// SET gUIData MEMBERS THAT NEED POINTERS TO ANIMATION DATA
		//

		// Blend Params
		gUIData.mBlendParams.mAutoSetBlendParams = gBlendedAnimation.GetAutoSetBlendParamsPtr();

		gUIData.mBlendParams.mStandClipWeight = gStandClipController.GetWeightPtr();
		gUIData.mBlendParams.mWalkClipWeight = gWalkClipController.GetWeightPtr();
		gUIData.mBlendParams.mThreshold = gBlendedAnimation.GetThresholdPtr();

		// Stand Clip
		gUIData.mStandClip.mPlay = gStandClipController.GetPlayPtr();
		gUIData.mStandClip.mLoop = gStandClipController.GetLoopPtr();
		gUIData.mStandClip.mPlaybackSpeed = gStandClipController.GetPlaybackSpeedPtr();

		// Walk Clip
		gUIData.mWalkClip.mPlay = gWalkClipController.GetPlayPtr();
		gUIData.mWalkClip.mLoop = gWalkClipController.GetLoopPtr();
		gUIData.mWalkClip.mPlaybackSpeed = gWalkClipController.GetPlaybackSpeedPtr();

		// SET UP GUI BASED ON gUIData STRUCT
		//
		{
			// BLEND PARAMETERS
			//
			CollapsingHeaderWidget CollapsingBlendParamsWidgets("Blend Parameters");

			// UpperBodyWeight - Slider
			float             fValMin = 0.0f;
			float             fValMax = 1.0f;
			float             sliderStepSize = 0.01f;
			SliderFloatWidget SliderUpperBodyWeight(
				"Upper Body Weight", &gUIData.mBlendParams.mUpperBodyWeight, fValMin, fValMax, sliderStepSize);
			SliderUpperBodyWeight.pOnEdited = UpperBodyWeightCallback;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(SliderUpperBodyWeight);

			// AutoSetBlendParams - Checkbox
			CheckboxWidget CheckboxAutoSetBlendParams("Auto Set Blend Params", gUIData.mBlendParams.mAutoSetBlendParams);
			CheckboxAutoSetBlendParams.pOnEdited = AutoSetBlendParamsCallback;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(CheckboxAutoSetBlendParams);

			// Stand Clip Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Clip Weight [Stand]", gUIData.mBlendParams.mStandClipWeight, fValMin, fValMax, sliderStepSize));

			// Stand Joints Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderStandJointsWeight(
				"Joints Weight [Stand]", &gUIData.mBlendParams.mStandJointsWeight, fValMin, fValMax, sliderStepSize);
			SliderStandJointsWeight.pOnEdited = StandClipJointsWeightCallback;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(SliderStandJointsWeight);

			// Walk Clip Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Clip Weight [Walk]", gUIData.mBlendParams.mWalkClipWeight, fValMin, fValMax, sliderStepSize));

			// Walk Joints Weight - Slider
			fValMin = 0.0f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderWalkJointsWeight(
				"Joints Weight [Walk]", &gUIData.mBlendParams.mWalkJointsWeight, fValMin, fValMax, sliderStepSize);
			SliderWalkJointsWeight.pOnEdited = WalkClipJointsWeightCallback;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(SliderWalkJointsWeight);

			// Threshold - Slider
			fValMin = 0.01f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingBlendParamsWidgets.AddSubWidget(
				SliderFloatWidget("Threshold", gUIData.mBlendParams.mThreshold, fValMin, fValMax, sliderStepSize));
			CollapsingBlendParamsWidgets.AddSubWidget(SeparatorWidget());

			// UPPER BODY ROOT
			//
			CollapsingHeaderWidget CollapsingUpperBodyRootWidgets("Upper Body Root");

			// UpperBodyJointIndex - Slider
			unsigned         uintValMin = 0;
			unsigned         uintValMax = gStickFigureRig.GetNumJoints() - 1;
			unsigned         sliderStepSizeUint = 1;
			SliderUintWidget SliderUpperBodyJointIndex(
				"Joint Index", &gUIData.mUpperBodyJointIndex, uintValMin, uintValMax, sliderStepSizeUint);
			SliderUpperBodyJointIndex.pOnEdited = UpperBodyJointIndexCallback;

			CollapsingUpperBodyRootWidgets.AddSubWidget(SeparatorWidget());
			CollapsingUpperBodyRootWidgets.AddSubWidget(SliderUpperBodyJointIndex);

			// STAND CLIP
			//
			CollapsingHeaderWidget CollapsingStandClipWidgets("Stand Clip (Upper Body)");

			// Play/Pause - Checkbox
			CollapsingStandClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingStandClipWidgets.AddSubWidget(CheckboxWidget("Play", gUIData.mStandClip.mPlay));

			// Loop - Checkbox
			CollapsingStandClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingStandClipWidgets.AddSubWidget(CheckboxWidget("Loop", gUIData.mStandClip.mLoop));

			// Animation Time - Slider
			fValMin = 0.0f;
			fValMax = gStandClipController.GetDuration();
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderStandClipAnimationTime(
				"Animation Time", &gUIData.mStandClip.mAnimationTime, fValMin, fValMax, sliderStepSize);
			SliderStandClipAnimationTime.pOnActive = StandClipTimeChangeCallback;

			CollapsingStandClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingStandClipWidgets.AddSubWidget(SliderStandClipAnimationTime);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			CollapsingStandClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingStandClipWidgets.AddSubWidget(
				SliderFloatWidget("Playback Speed", gUIData.mStandClip.mPlaybackSpeed, fValMin, fValMax, sliderStepSize));
			CollapsingStandClipWidgets.AddSubWidget(SeparatorWidget());

			// WALK CLIP
			//
			CollapsingHeaderWidget CollapsingWalkClipWidgets("Walk Clip (Lower Body)");

			// Play/Pause - Checkbox
			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(CheckboxWidget("Play", gUIData.mWalkClip.mPlay));

			// Loop - Checkbox
			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(CheckboxWidget("Loop", gUIData.mWalkClip.mLoop));

			// Animation Time - Slider
			fValMin = 0.0f;
			fValMax = gWalkClipController.GetDuration();
			sliderStepSize = 0.01f;
			SliderFloatWidget SliderWalkClipAnimationTime(
				"Animation Time", &gUIData.mWalkClip.mAnimationTime, fValMin, fValMax, sliderStepSize);
			SliderWalkClipAnimationTime.pOnActive = WalkClipTimeChangeCallback;

			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(SliderWalkClipAnimationTime);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingWalkClipWidgets.AddSubWidget(
				SliderFloatWidget("Playback Speed", gUIData.mWalkClip.mPlaybackSpeed, fValMin, fValMax, sliderStepSize));
			CollapsingWalkClipWidgets.AddSubWidget(SeparatorWidget());

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
			pStandaloneControlsGUIWindow->AddWidget(CollapsingUpperBodyRootWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingStandClipWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingWalkClipWidgets);
			pStandaloneControlsGUIWindow->AddWidget(CollapsingGeneralSettingsWidgets);
		}

		return true;
	}

	void Exit()
	{
		// wait for rendering to finish before freeing resources
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		// Animation data
		gSkeletonBatcher.Destroy();
		gStickFigureRig.Destroy();
		gStandClip.Destroy();
		gWalkClip.Destroy();
		gStandClipMask.Destroy();
		gWalkClipMask.Destroy();
		gBlendedAnimation.Destroy();
		gStickFigureAnimObject.Destroy();

		destroyCameraController(pCameraController);

		removeDebugRendererInterface();

#ifdef TARGET_IOS
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

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], pDepthBuffer->mDesc.mFormat))
			return false;
#endif

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

		GraphicsPipelineDesc pipelineSettings = { 0 };
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
		addPipeline(pRenderer, &pipelineSettings, &pSkeletonPipeline);

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
		addPipeline(pRenderer, &pipelineSettings, &pPlaneDrawPipeline);

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);

		gAppUI.Unload();

#ifdef TARGET_IOS
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
		if (getKeyDown(KEY_BUTTON_X))
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
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);

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
		loadActions.mClearColorValues[0] = { 0.39f, 0.41f, 0.37f, 1.0f };
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0 };
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
			cmdBindDescriptors(cmd, pRootSignature, 1, params);
			cmdBindVertexBuffer(cmd, 1, &pPlaneVertexBuffer, NULL);
			cmdDraw(cmd, 6, 0);
			cmdEndDebugMarker(cmd);
		}

		//// draw the skeleton of the rigs
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons");
		gSkeletonBatcher.Draw(cmd, gFrameIndex);
		cmdEndDebugMarker(cmd);

		//// draw the UI
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		gTimer.GetUSec(true);
#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.Gui(pStandaloneControlsGUIWindow);    // adds the gui element to AppUI::ComponentsToUpdate list
		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
		drawDebugText(
			cmd, 8, 65, tinystl::string::format("Animation Update %f ms", gAnimationUpdateTimer.GetUSecAverage() / 1000.0f),
			&gFrameTimeDraw);
#ifndef METAL    // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
#endif
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
	}

	tinystl::string GetName() { return "04_PartialBlending"; }

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
		// Add depth buffer
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

DEFINE_APPLICATION_MAIN(Blending)
