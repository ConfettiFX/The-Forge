/*
* Copyright (c) 2017-2022 The Forge Interactive Inc.
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
* The Forge - ANIMATION - AIM IK UNIT TEST
*
* The purpose of this demo is to show how to do aim inverse kinematics using the
* animnation middleware
*
*********************************************************************************************************/

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IScripting.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/OS/Interfaces/IUI.h"
#include "../../../../Common_3/OS/Interfaces/IFont.h"

// Rendering
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

// Middleware packages
#include "../../../../Middleware_3/Animation/SkeletonBatcher.h"
#include "../../../../Middleware_3/Animation/AnimatedObject.h"
#include "../../../../Middleware_3/Animation/Animation.h"
#include "../../../../Middleware_3/Animation/Clip.h"
#include "../../../../Middleware_3/Animation/ClipController.h"
#include "../../../../Middleware_3/Animation/Rig.h"

// tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

// Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

// Memory
#include "../../../../Common_3/OS/Interfaces/IMemory.h"

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t gImageCount = 3;
ProfileToken   gGpuProfileToken;

uint32_t       gFrameIndex = 0;
Renderer*      pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pSkeletonShader = NULL;
Buffer*   pJointVertexBuffer = NULL;
Buffer*   pBoneVertexBuffer = NULL;
Pipeline* pSkeletonPipeline = NULL;
int       gNumberOfJointPoints;
int       gNumberOfBonePoints;

Shader*           pPlaneDrawShader = NULL;
Buffer*           pPlaneVertexBuffer = NULL;
Pipeline*         pPlaneDrawPipeline = NULL;
RootSignature*    pRootSignature = NULL;
DescriptorSet*    pDescriptorSetPlane = NULL;
DescriptorSet*    pDescriptorSetTarget = NULL;

struct UniformBlockPlane
{
	CameraMatrix mProjectView;
	mat4 mToWorldMat;
};
UniformBlockPlane gUniformDataPlane;

Buffer* pPlaneUniformBuffer[gImageCount] = { NULL };

//--------------------------------------------------------------------------------------------
// CAMERA CONTROLLER & SYSTEMS (File/Log/UI)
//--------------------------------------------------------------------------------------------

ICameraController* pCameraController = NULL;
UIComponent* pStandaloneControlsGUIWindow = NULL;

FontDrawDesc gFrameTimeDraw; 
uint32_t     gFontID = 0; 

//--------------------------------------------------------------------------------------------
// ANIMATION DATA
//--------------------------------------------------------------------------------------------

// AnimatedObjects
AnimatedObject gStickFigureAnimObject;

// Animations
Animation gStandAnimation;

// ClipControllers
ClipController gStandClipController;

// Clips
Clip gStandClip;

// Rigs
Rig gStickFigureRig;

// SkeletonBatcher
SkeletonBatcher gSkeletonBatcher;

// parameters for aim IK
AimIKDesc     gAimIKDesc;
TwoBonesIKDesc gTwoBonesIKDesc;
int           gJointChain[4];
const Vector3 gJointUpVectors[4] = { Vector3::xAxis(), Vector3::xAxis(), Vector3::xAxis(), Vector3::xAxis() };
//--------------------------------------------------------------------------------------------
// TARGET
//--------------------------------------------------------------------------------------------

UniformSkeletonBlock gUniformDataTarget;

Buffer* pTargetUniformBuffer[gImageCount] = { NULL };

// Filenames
const char* gStickFigureName = "stickFigure/skeleton.ozz";
const char* gStandClipName = "stickFigure/animations/stand.ozz";

float* pJointPoints = 0;
float* pBonePoints = 0;

const int   gSphereResolution = 30;                   // Increase for higher resolution joint spheres
const float gBoneWidthRatio = 0.2f;                   // Determines how far along the bone to put the max width [0,1]
const float gJointRadius = gBoneWidthRatio * 0.5f;    // set to replicate Ozz skeleton

// Timer to get animationsystem update time
static HiresTimer gAnimationUpdateTimer;
char gAnimationUpdateText[64] = { 0 };

//--------------------------------------------------------------------------------------------
// UI DATA
//--------------------------------------------------------------------------------------------
struct UIData
{
	struct IKParamsData
	{
		bool mAim = true;
		float mFoot = 0.0f;
	};
	IKParamsData mIKParams;

	struct BlendParamsData
	{
		bool*  mAutoSetBlendParams;
		float* mStandClipWeight;
		float* mThreshold;
	};
	BlendParamsData mBlendParams;

	struct ClipData
	{
		bool*  mPlay;
		bool*  mLoop;
		float  mAnimationTime;    // will get set by clip controller
		float* mPlaybackSpeed;
	};
	ClipData mStandClip;

	struct GeneralSettingsData
	{
		bool mShowBindPose = false;    //false;
		bool mDrawPlane = true;
	};
	GeneralSettingsData mGeneralSettings;
};
UIData gUIData;

// Hard set the controller's time ratio via callback when it is set in the UI
void StandClipTimeChangeCallback() { gStandClipController.SetTimeRatioHard(gUIData.mStandClip.mAnimationTime); }

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class AimIK: public IApp
{
	public:
	bool Init()
	{
		initHiresTimer(&gAnimationUpdateTimer);

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,          "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_ANIMATIONS,      "Animation");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,			"Scripts");

		// GENERATE VERTEX BUFFERS
		//

		// Generate joint vertex buffer
		generateSpherePoints(&pJointPoints, &gNumberOfJointPoints, gSphereResolution, gJointRadius);

		// Generate bone vertex buffer
		generateBonePoints(&pBonePoints, &gNumberOfBonePoints, gBoneWidthRatio);

		
		// RIGS
		//
		// Initialize the rig with the path to its ozz file
		gStickFigureRig.Initialize(RD_ANIMATIONS, gStickFigureName, NULL);

		// CLIPS
		//
		gStandClip.Initialize(RD_ANIMATIONS, gStandClipName, NULL, &gStickFigureRig);

		// CLIP CONTROLLERS
		//
		// Initialize with the length of the clip they are controlling and an
		// optional external time to set based on their updating
		gStandClipController.Initialize(gStandClip.GetDuration(), &gUIData.mStandClip.mAnimationTime);

		// ANIMATIONS
		//
		AnimationDesc animationDesc{};
		animationDesc.mRig = &gStickFigureRig;
		animationDesc.mNumLayers = 1;
		animationDesc.mLayerProperties[0].mClip = &gStandClip;
		animationDesc.mLayerProperties[0].mClipController = &gStandClipController;

		gStandAnimation.Initialize(animationDesc);

		// ANIMATED OBJECTS
		//
		gStickFigureAnimObject.Initialize(&gStickFigureRig, &gStandAnimation);

		const char* aimJointNames[4] = { "Head", "Spine3", "Spine2", "Spine1" };
		gAimIKDesc.mForward = Vector3::yAxis();
		gAimIKDesc.mOffset = Vector3(.07f, .1f, 0.f);
		gAimIKDesc.mPoleVector = Vector3::yAxis();
		gAimIKDesc.mTwistAngle = 0.0f;
		gAimIKDesc.mJointWeight = 0.5f;
		gAimIKDesc.mJointChainLength = 4;
		gAimIKDesc.mJointChain = gJointChain;
		gAimIKDesc.mJointUpVectors = gJointUpVectors;
		gStickFigureRig.FindJointChain(aimJointNames, gAimIKDesc.mJointChainLength, gJointChain);

		const char* twoBonesJointNames[] = { "RightUpLeg", "RightLeg", "RightFoot" };
		gTwoBonesIKDesc.mSoften = 1.0f;
		gTwoBonesIKDesc.mWeight = 1.0f;
		gTwoBonesIKDesc.mTwistAngle = 0.0f;
		gTwoBonesIKDesc.mPoleVector = Vector3::zAxis();
		gTwoBonesIKDesc.mMidAxis = Vector3::zAxis();
		gStickFigureRig.FindJointChain(twoBonesJointNames, 3, gTwoBonesIKDesc.mJointChain);

		// WINDOW AND RENDERER SETUP
		//
		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mD3D11Supported = true;
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

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		// INITIALIZE PIPILINE STATES
		//
		ShaderLoadDesc planeShader = {};
		planeShader.mStages[0] = { "plane.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		planeShader.mStages[1] = { "plane.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &planeShader, &pPlaneDrawShader);
		addShader(pRenderer, &basicShader, &pSkeletonShader);

		Shader*           shaders[] = { pSkeletonShader, pPlaneDrawShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPlane);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTarget);

		uint64_t       jointDataSize = gNumberOfJointPoints * sizeof(float);
		BufferLoadDesc jointVbDesc = {};
		jointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		jointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		jointVbDesc.mDesc.mSize = jointDataSize;
		jointVbDesc.pData = pJointPoints;
		jointVbDesc.ppBuffer = &pJointVertexBuffer;
		addResource(&jointVbDesc, NULL);

		uint64_t       boneDataSize = gNumberOfBonePoints * sizeof(float);
		BufferLoadDesc boneVbDesc = {};
		boneVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		boneVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boneVbDesc.mDesc.mSize = boneDataSize;
		boneVbDesc.pData = pBonePoints;
		boneVbDesc.ppBuffer = &pBoneVertexBuffer;
		addResource(&boneVbDesc, NULL);

		//Generate plane vertex buffer
		float planePoints[] = { -10.0f, 0.0f, -10.0f, 1.0f, 0.0f, 0.0f, -10.0f, 0.0f, 10.0f,  1.0f, 1.0f, 0.0f,
								10.0f,  0.0f, 10.0f,  1.0f, 1.0f, 1.0f, 10.0f,  0.0f, 10.0f,  1.0f, 1.0f, 1.0f,
								10.0f,  0.0f, -10.0f, 1.0f, 0.0f, 1.0f, -10.0f, 0.0f, -10.0f, 1.0f, 0.0f, 0.0f };

		uint64_t       planeDataSize = 6 * 6 * sizeof(float);
		BufferLoadDesc planeVbDesc = {};
		planeVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		planeVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		planeVbDesc.mDesc.mSize = planeDataSize;
		planeVbDesc.pData = planePoints;
		planeVbDesc.ppBuffer = &pPlaneVertexBuffer;
		addResource(&planeVbDesc, NULL);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlockPlane);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pPlaneUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}
		ubDesc.mDesc.mSize = sizeof(UniformSkeletonBlock);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pTargetUniformBuffer[i];
			addResource(&ubDesc, NULL);
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
		skeletonRenderDesc.mBoneVertexStride = sizeof(float) * 6;
		skeletonRenderDesc.mJointVertexStride = sizeof(float) * 6;
		gSkeletonBatcher.Initialize(skeletonRenderDesc);

		// Add the rig to the list of skeletons to render
		gSkeletonBatcher.AddRig(&gStickFigureRig);

		// Add the GUI Panels/Windows

		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.15f };
		vec2    UIPanelSize = { 650, 1000 };
		UIComponentDesc guiDesc;
		guiDesc.mStartPosition = UIPosition;
		guiDesc.mStartSize = UIPanelSize;
		guiDesc.mFontID = 0; 
		guiDesc.mFontSize = 16.0f; 
		uiCreateComponent("Stand Animation", &guiDesc, &pStandaloneControlsGUIWindow);

		// SET gUIData MEMBERS THAT NEED POINTERS TO ANIMATION DATA
		//

		// Blend Params
		gUIData.mBlendParams.mAutoSetBlendParams = gStandAnimation.GetAutoSetBlendParamsPtr();

		gUIData.mBlendParams.mStandClipWeight = gStandClipController.GetWeightPtr();
		gUIData.mBlendParams.mThreshold = gStandAnimation.GetThresholdPtr();

		// Stand Clip
		gUIData.mStandClip.mPlay = gStandClipController.GetPlayPtr();
		gUIData.mStandClip.mLoop = gStandClipController.GetLoopPtr();
		gUIData.mStandClip.mPlaybackSpeed = gStandClipController.GetPlaybackSpeedPtr();

		// SET UP GUI BASED ON gUIData STRUCT
		//
		{
			SeparatorWidget separator;
			CheckboxWidget checkbox;
			SliderFloatWidget sliderFloat;

			// INVERSE KINEMATICS
			//
			CollapsingHeaderWidget CollapsingIKWidgets;
			uiSetCollapsingHeaderWidgetCollapsed(&CollapsingIKWidgets, false);

			checkbox.pData = &gUIData.mIKParams.mAim;
			uiCreateCollapsingHeaderSubWidget(&CollapsingIKWidgets, "Aim IK", &checkbox, WIDGET_TYPE_CHECKBOX);

			float             sliderStepSize = 0.01f;

			sliderFloat.pData = &gUIData.mIKParams.mFoot;
			sliderFloat.mMin = 0.0f;
			sliderFloat.mMax = 0.5f;
			sliderFloat.mStep = sliderStepSize;
			uiCreateCollapsingHeaderSubWidget(&CollapsingIKWidgets, "Foot two bone IK", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

			// BLEND PARAMETERS
			//
			CollapsingHeaderWidget CollapsingBlendParamsWidgets;

			// AutoSetBlendParams - Checkbox
			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			checkbox.pData = gUIData.mBlendParams.mAutoSetBlendParams;
			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "Auto Set Blend Params", &checkbox, WIDGET_TYPE_CHECKBOX);

			// Stand Clip Weight - Slider
			float fValMin = 0.0f;
			float fValMax = 1.0f;

			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			sliderFloat.pData = gUIData.mBlendParams.mStandClipWeight;
			sliderFloat.mMin = fValMin;
			sliderFloat.mMax = fValMax;
			sliderFloat.mStep = sliderStepSize;
			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "Clip Weight [Stand]", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

			// Threshold - Slider
			fValMin = 0.01f;
			fValMax = 1.0f;
			sliderStepSize = 0.01f;

			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			sliderFloat.pData = gUIData.mBlendParams.mThreshold;
			sliderFloat.mMin = fValMin;
			sliderFloat.mMax = fValMax;
			sliderFloat.mStep = sliderStepSize;
			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "Threshold", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

			uiCreateCollapsingHeaderSubWidget(&CollapsingBlendParamsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			// STAND CLIP
			//
			CollapsingHeaderWidget CollapsingStandClipWidgets;

			// Play/Pause - Checkbox
			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			checkbox.pData = gUIData.mStandClip.mPlay;
			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "Play", &checkbox, WIDGET_TYPE_CHECKBOX);

			// Loop - Checkbox
			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			checkbox.pData = gUIData.mStandClip.mLoop;
			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "Loop", &checkbox, WIDGET_TYPE_CHECKBOX);

			// Animation Time - Slider
			fValMin = 0.0f;
			fValMax = gStandClipController.GetDuration();
			sliderStepSize = 0.01f;

			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			sliderFloat.pData = &gUIData.mStandClip.mAnimationTime;
			sliderFloat.mMin = fValMin;
			sliderFloat.mMax = fValMax;
			sliderFloat.mStep = sliderStepSize;
			uiSetWidgetOnActiveCallback(uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "Animation Time", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT), StandClipTimeChangeCallback);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			sliderFloat.pData = gUIData.mStandClip.mPlaybackSpeed;
			sliderFloat.mMin = fValMin;
			sliderFloat.mMax = fValMax;
			sliderFloat.mStep = sliderStepSize;
			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "Playback Speed", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

			uiCreateCollapsingHeaderSubWidget(&CollapsingStandClipWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			// GENERAL SETTINGS
			//
			CollapsingHeaderWidget CollapsingGeneralSettingsWidgets;

			// ShowBindPose - Checkbox
			uiCreateCollapsingHeaderSubWidget(&CollapsingGeneralSettingsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			checkbox.pData = &gUIData.mGeneralSettings.mShowBindPose;
			uiCreateCollapsingHeaderSubWidget(&CollapsingGeneralSettingsWidgets, "Show Bind Pose", &checkbox, WIDGET_TYPE_CHECKBOX);

			// DrawPlane - Checkbox
			uiCreateCollapsingHeaderSubWidget(&CollapsingGeneralSettingsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			checkbox.pData = &gUIData.mGeneralSettings.mDrawPlane;
			uiCreateCollapsingHeaderSubWidget(&CollapsingGeneralSettingsWidgets, "Draw Plane", &checkbox, WIDGET_TYPE_CHECKBOX);

			uiCreateCollapsingHeaderSubWidget(&CollapsingGeneralSettingsWidgets, "", &separator, WIDGET_TYPE_SEPARATOR);

			// Add all widgets to the window

			luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "Inverse Kinematics", &CollapsingIKWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
			luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "Blend Parameters", &CollapsingBlendParamsWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
			luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "Stand Clip", &CollapsingStandClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
			luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "General Settings", &CollapsingGeneralSettingsWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
		}

		waitForAllResourceLoads();

		// Prepare descriptor sets
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[1] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pPlaneUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetPlane, 1, params);
			params[0].ppBuffers = &pTargetUniformBuffer[gFrameIndex];
			updateDescriptorSet(pRenderer, i, pDescriptorSetTarget, 1, params);
		}

		/************************************************************************/
		// SETUP THE MAIN CAMERA
		//
		CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
		vec3                   camPos{ -3.0f, 3.0f, 3.0f };
		vec3                   lookAt{ 0.0f, 1.0f, 0.0f };

		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			bool capture = uiOnInput(ctx->mBinding, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			if(ctx->mBinding != InputBindings::FLOAT_LEFTSTICK)
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
			return true;
		};
		actionDesc = { InputBindings::BUTTON_ANY, onUIInput, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, onUIInput, this, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		typedef bool (*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*ctx->pCaptured)
			{
				float2 val = uiIsFocused() ? float2(0.0f) : ctx->mFloat2;
				index ? pCameraController->onRotate(val) : pCameraController->onMove(val);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.5f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();
		gStickFigureRig.Exit();
		gStandClip.Exit();
		gStandAnimation.Exit();
		gStickFigureAnimObject.Exit();

		exitCameraController(pCameraController);

		// Need to free memory;
		tf_free(pJointPoints);
		tf_free(pBonePoints);

		exitProfiler();

		gSkeletonBatcher.Exit();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pPlaneUniformBuffer[i]);
		}
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pTargetUniformBuffer[i]);
		}

		removeResource(pJointVertexBuffer);
		removeResource(pBoneVertexBuffer);
		removeResource(pPlaneVertexBuffer);

		removeShader(pRenderer, pSkeletonShader);
		removeShader(pRenderer, pPlaneDrawShader);
		removeDescriptorSet(pRenderer, pDescriptorSetPlane);
		removeDescriptorSet(pRenderer, pDescriptorSetTarget);
		removeRootSignature(pRenderer, pRootSignature);

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
		pRenderer = NULL; 
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
		RenderTarget* ppPipelineRenderTargets[] = {
			pSwapChain->ppRenderTargets[0],
			pDepthBuffer
		};

		if (!addFontSystemPipelines(ppPipelineRenderTargets, 2, NULL))
			return false;

		if (!addUserInterfacePipelines(ppPipelineRenderTargets[0]))
			return false;

		//layout and pipeline for skeleton draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc skeletonRasterizerStateDesc = {};
		skeletonRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSkeletonShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = &skeletonRasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pSkeletonPipeline);

		// Update the mSkeletonPipeline pointer now that the pipeline has been loaded
		gSkeletonBatcher.LoadPipeline(pSkeletonPipeline);

		//layout and pipeline for plane draw
		vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pShaderProgram = pPlaneDrawShader;
		addPipeline(pRenderer, &desc, &pPlaneDrawPipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		removeUserInterfacePipelines();

		removeFontSystemPipelines(); 

		removePipeline(pRenderer, pPlaneDrawPipeline);
		removePipeline(pRenderer, pSkeletonPipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);

		/************************************************************************/
		// Scene Update
		/************************************************************************/

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		CameraMatrix projViewMat = projMat * viewMat;

		vec3 lightPos = vec3(0.0f, 1000.0f, 0.0f);
		vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

		/************************************************************************/
		// Animation
		/************************************************************************/
		resetHiresTimer(&gAnimationUpdateTimer);

		// Update the animated object for this frame
		if (!gStickFigureAnimObject.Update(deltaTime))
			LOGF(eINFO, "Animation NOT Updating!");

		static float time = 0.0f;
		time += 2.0f * deltaTime;

		const Point3 aimTarget = Vector3(.2f, 1.5f, -.3f) + Point3(sin(time * .5f), cos(time * .25f), cos(time) * .5f + .5f);

		gUniformDataTarget.mProjectView = projViewMat;
		gUniformDataTarget.mLightPosition = Vector4(lightPos);
		gUniformDataTarget.mLightColor = Vector4(lightColor);
		gUniformDataTarget.mToWorldMat[0] = Matrix4(Matrix3::identity() * 0.25f, Vector3(aimTarget));
		gUniformDataTarget.mColor[0] = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

		if (gUIData.mIKParams.mAim)
		{
			if (!gStickFigureAnimObject.AimIK(&gAimIKDesc, aimTarget))
				LOGF(eINFO, "Aim IK failed!");
		}

		Matrix4 mat = gStickFigureRig.GetJointModelMats()[gTwoBonesIKDesc.mJointChain[2]];
		Point3  twoBoneTarget = Point3(mat.getCol3()) + Vector3(0.0f, gUIData.mIKParams.mFoot, 0.0f);

		if (!gStickFigureAnimObject.TwoBonesIK(&gTwoBonesIKDesc, twoBoneTarget))
			LOGF(eINFO, "Two bone IK failed!");

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
		getHiresTimerUSec(&gAnimationUpdateTimer, true);

		// Update uniforms that will be shared between all skeletons
		gSkeletonBatcher.SetSharedUniforms(projViewMat, lightPos, lightColor);

		/************************************************************************/
		// Plane
		/************************************************************************/
		gUniformDataPlane.mProjectView = projViewMat;
		gUniformDataPlane.mToWorldMat = mat4::identity();
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		// UPDATE UNIFORM BUFFERS
		//

		// Update all the instanced uniform data for each batch of joints and bones
		gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

		BufferUpdateDesc planeViewProjCbv = { pPlaneUniformBuffer[gFrameIndex] };
		beginUpdateResource(&planeViewProjCbv);
		*(UniformBlockPlane*)planeViewProjCbv.pMappedData = gUniformDataPlane;
		endUpdateResource(&planeViewProjCbv, NULL);
		BufferUpdateDesc targetViewProjCbv = { pTargetUniformBuffer[gFrameIndex] };
		beginUpdateResource(&targetViewProjCbv);
		*(UniformSkeletonBlock*)targetViewProjCbv.pMappedData = gUniformDataTarget;
		endUpdateResource(&targetViewProjCbv, NULL);

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

		// start gpu frame profiler
		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] =    // wait for resource transition
		{
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		// bind and clear the render target
		LoadActionsDesc loadActions = {};    // render target clean command
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		const uint32_t stride = sizeof(float) * 6;
		//// draw plane
		if (gUIData.mGeneralSettings.mDrawPlane)
		{
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Plane");
			cmdBindPipeline(cmd, pPlaneDrawPipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPlane);
			cmdBindVertexBuffer(cmd, 1, &pPlaneVertexBuffer, &stride, NULL);
			cmdDraw(cmd, 6, 0);
			cmdEndDebugMarker(cmd);
		}

		//// draw the skeleton of the rig
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons");
		gSkeletonBatcher.Draw(cmd, gFrameIndex);
		cmdEndDebugMarker(cmd);

		cmdBindVertexBuffer(cmd, 1, &pJointVertexBuffer, &stride, NULL);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetTarget);
		cmdDrawInstanced(cmd, gNumberOfJointPoints / 6, 0, 1, 0);

		//// draw the UI
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID;
		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

		sprintf(gAnimationUpdateText, "Animation Update %f ms", getHiresTimerUSecAverage(&gAnimationUpdateTimer) / 1000.0f);

		gFrameTimeDraw.pText = gAnimationUpdateText;
		cmdDrawTextWithFont(cmd, float2(8.f, txtSize.y + 75.f), &gFrameTimeDraw);

		cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y * 2.f + 100.f), gGpuProfileToken, &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		// PRESENT THE GRPAHICS QUEUE
		//
		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
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

	const char* GetName() { return "29_InverseKinematic"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mColorClearValue = { { 0.39f, 0.41f, 0.37f, 1.0f } };
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 1.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
};

DEFINE_APPLICATION_MAIN(AimIK)
