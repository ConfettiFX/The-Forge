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
* The Forge - ANIMATION - SKINNING UNIT TEST
*
* The purpose of this demo is to show how to use the asset pipeline and how to do GPU skinning.
*
*********************************************************************************************************/

#include "Shaders/Shared.h"

// Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

// Rendering
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Middleware packages
#include "../../../../Common_3/Resources/AnimationSystem/Animation/SkeletonBatcher.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/AnimatedObject.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Animation.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Clip.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/ClipController.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Rig.h"

// tiny stl
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

// Memory
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

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

Sampler* pDefaultSampler = NULL;

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

Shader*           pShaderSkinning = NULL;
RootSignature*    pRootSignatureSkinning = NULL;
Pipeline*         pPipelineSkinning = NULL;

DescriptorSet*    pDescriptorSet = NULL;
DescriptorSet*    pDescriptorSetSkinning[2] = { NULL };

struct Vertex
{
	float3 mPosition;
	float3 mNormal;
	float2 mUV;
	float4 mBoneWeights;
	uint4  mBoneIndices;
};

struct UniformDataBones
{
	mat4 mBoneMatrix[MAX_NUM_BONES];
};

VertexLayout     gVertexLayoutSkinned = {};
Geometry*        pGeom = NULL;
Buffer*          pUniformBufferBones[gImageCount] = { NULL };
UniformDataBones gUniformDataBones;
Texture*         pTextureDiffuse = NULL;

struct shadow_cap
{
	Vector4 a;
	Vector4 b;
};


static shadow_cap generate_cap(const Vector3& a, const Vector3& b, float r)
{
	Vector3 ab = a - b;
	ab = normalize(ab);

	return shadow_cap{
		Vector4(a - (ab * r * 0.5f)),
		Vector4(b + (ab * r * 0.5f), r)
	};
}

struct UniformBlockPlane
{
	CameraMatrix mProjectView;
	mat4 mToWorldMat;
	shadow_cap capsules[MAX_NUM_BONES];
	uint capsules_count;
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
Animation gAnimation;

// ClipControllers
ClipController gClipController;

// Clips
Clip gClip;

// Rigs
Rig gStickFigureRig;

// SkeletonBatcher
SkeletonBatcher gSkeletonBatcher;

// Filenames
const char* gStickFigureName = "stormtrooper/skeleton.ozz";
const char* gClipName = "stormtrooper/animations/dance.ozz";
const char* gDiffuseTexture = "Stormtrooper_D";

float* pJointPoints = 0;
float* pBonePoints = 0;

const int   gSphereResolution = 8;                   // Increase for higher resolution joint spheres
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
	struct ClipData
	{
		bool*  mPlay;
		bool*  mLoop;
		float  mAnimationTime;    // will get set by clip controller
		float* mPlaybackSpeed;
	};
	ClipData mClip;

	struct GeneralSettingsData
	{
		bool mShowBindPose = false;
		bool mDrawBones = false;
		bool mDrawPlane = true;
		bool mDrawShadows = true;
	};
	GeneralSettingsData mGeneralSettings;
};
UIData gUIData;

// Hard set the controller's time ratio via callback when it is set in the UI
void ClipTimeChangeCallback(void* pUserData) { gClipController.SetTimeRatioHard(gUIData.mClip.mAnimationTime); }
eastl::vector<mat4> inverseBindMatrices;

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class Skinning: public IApp
{
	public:
	bool Init()
	{
		initHiresTimer(&gAnimationUpdateTimer);
        // FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,          "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_ANIMATIONS,      "Animation");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,		   "Scripts");

		/************************************************************************/
		// SETUP ANIMATION STRUCTURES
		/************************************************************************/

		// RIGS
		//
		// Initialize the rig with the path to its ozz file
		gStickFigureRig.Initialize(RD_ANIMATIONS, gStickFigureName, NULL);

		// CLIPS
		//
		gClip.Initialize(RD_ANIMATIONS, gClipName, NULL, &gStickFigureRig);

		// CLIP CONTROLLERS
		//
		// Initialize with the length of the clip they are controlling and an
		// optional external time to set based on their updating
		gClipController.Initialize(gClip.GetDuration(), &gUIData.mClip.mAnimationTime);

		// ANIMATIONS
		//
		AnimationDesc animationDesc{};
		animationDesc.mRig = &gStickFigureRig;
		animationDesc.mNumLayers = 1;
		animationDesc.mLayerProperties[0].mClip = &gClip;
		animationDesc.mLayerProperties[0].mClipController = &gClipController;

		gAnimation.Initialize(animationDesc);

		// ANIMATED OBJECTS
		//
		gStickFigureAnimObject.Initialize(&gStickFigureRig, &gAnimation);

		// GENERATE VERTEX BUFFERS
		//

		// Generate joint vertex buffer
		generateSpherePoints(&pJointPoints, &gNumberOfJointPoints, gSphereResolution, gJointRadius);

		// Generate bone vertex buffer
		generateBonePoints(&pBonePoints, &gNumberOfBonePoints, gBoneWidthRatio);

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

		// INITIALIZE SAMPLERS
	//
		SamplerDesc defaultSamplerDesc = {};
		defaultSamplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
		defaultSamplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
		defaultSamplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
		defaultSamplerDesc.mMinFilter = FILTER_LINEAR;
		defaultSamplerDesc.mMagFilter = FILTER_LINEAR;
		defaultSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		addSampler(pRenderer, &defaultSamplerDesc, &pDefaultSampler);

		// INITIALIZE PIPILINE STATES
		//
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

		/************************************************************************/
		// LOAD SKINNED MESH
		/************************************************************************/
		gVertexLayoutSkinned.mAttribCount = 5;
		gVertexLayoutSkinned.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutSkinned.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutSkinned.mAttribs[0].mBinding = 0;
		gVertexLayoutSkinned.mAttribs[0].mLocation = 0;
		gVertexLayoutSkinned.mAttribs[0].mOffset = 0;
		gVertexLayoutSkinned.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutSkinned.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutSkinned.mAttribs[1].mBinding = 0;
		gVertexLayoutSkinned.mAttribs[1].mLocation = 1;
		gVertexLayoutSkinned.mAttribs[1].mOffset = 3 * sizeof(float);
		gVertexLayoutSkinned.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutSkinned.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutSkinned.mAttribs[2].mBinding = 0;
		gVertexLayoutSkinned.mAttribs[2].mLocation = 2;
		gVertexLayoutSkinned.mAttribs[2].mOffset = 6 * sizeof(float);
		gVertexLayoutSkinned.mAttribs[3].mSemantic = SEMANTIC_WEIGHTS;
		gVertexLayoutSkinned.mAttribs[3].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		gVertexLayoutSkinned.mAttribs[3].mBinding = 0;
		gVertexLayoutSkinned.mAttribs[3].mLocation = 3;
		gVertexLayoutSkinned.mAttribs[3].mOffset = 8 * sizeof(float);
		gVertexLayoutSkinned.mAttribs[4].mSemantic = SEMANTIC_JOINTS;
		gVertexLayoutSkinned.mAttribs[4].mFormat = TinyImageFormat_R16G16B16A16_UINT;
		gVertexLayoutSkinned.mAttribs[4].mBinding = 0;
		gVertexLayoutSkinned.mAttribs[4].mLocation = 4;
		gVertexLayoutSkinned.mAttribs[4].mOffset = 12 * sizeof(float);

		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = "stormtrooper/riggedMesh.gltf";
		loadDesc.pVertexLayout = &gVertexLayoutSkinned;
		loadDesc.ppGeometry = &pGeom;
		loadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED;
		addResource(&loadDesc, NULL);

		BufferLoadDesc boneBufferDesc = {};
		boneBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		boneBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		boneBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ASSERT(MAX_NUM_BONES >= gStickFigureRig.mNumJoints);
		boneBufferDesc.mDesc.mSize = sizeof(mat4) * MAX_NUM_BONES;
		boneBufferDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			boneBufferDesc.ppBuffer = &pUniformBufferBones[i];
			addResource(&boneBufferDesc, NULL);
		}

		TextureLoadDesc diffuseTextureDesc = {};
		diffuseTextureDesc.pFileName = gDiffuseTexture;
		diffuseTextureDesc.ppTexture = &pTextureDiffuse;
		// Textures representing color should be stored in SRGB or HDR format
		diffuseTextureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		addResource(&diffuseTextureDesc, NULL);
		/************************************************************************/

		// SKELETON RENDERER
		//

		// Set up details for rendering the skeletons
		SkeletonRenderDesc skeletonRenderDesc = {};
		skeletonRenderDesc.mRenderer = pRenderer;
		skeletonRenderDesc.mSkeletonPipeline = pSkeletonPipeline;
		skeletonRenderDesc.mJointVertexBuffer = pJointVertexBuffer;
		skeletonRenderDesc.mNumJointPoints = gNumberOfJointPoints;
		skeletonRenderDesc.mDrawBones = true;
		skeletonRenderDesc.mBoneVertexBuffer = pBoneVertexBuffer;
		skeletonRenderDesc.mNumBonePoints = gNumberOfBonePoints;
		skeletonRenderDesc.mBoneVertexStride = sizeof(float) * 6;
		skeletonRenderDesc.mJointVertexStride = sizeof(float) * 6;
		skeletonRenderDesc.mMaxAnimatedObjects = 1;
		gSkeletonBatcher.Initialize(skeletonRenderDesc);

		// Add the rig to the list of skeletons to render
		gSkeletonBatcher.AddAnimatedObject(&gStickFigureAnimObject);

		// Add the GUI Panels/Windows

		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.15f };
		vec2    UIPanelSize = { 650, 1000 };
		UIComponentDesc guiDesc;
		guiDesc.mStartPosition = UIPosition;
		guiDesc.mStartSize = UIPanelSize;
		guiDesc.mFontID = 0; 
		guiDesc.mFontSize = 16.0f; 
		uiCreateComponent("Animation", &guiDesc, &pStandaloneControlsGUIWindow);

		// SET gUIData MEMBERS THAT NEED POINTERS TO ANIMATION DATA
		//

		// Clip
		gUIData.mClip.mPlay = &gClipController.mPlay;
		gUIData.mClip.mLoop = &gClipController.mLoop;
		gUIData.mClip.mPlaybackSpeed = &gClipController.mPlaybackSpeed;

		// SET UP GUI BASED ON gUIData STRUCT
		//
		{
			enum
			{
				CLIP_PARAM_SEPARATOR_0,
				CLIP_PARAM_PLAY,
				CLIP_PARAM_SEPARATOR_1,
				CLIP_PARAM_LOOP,
				CLIP_PARAM_SEPARATOR_2,
				CLIP_PARAM_ANIMATION_TIME,
				CLIP_PARAM_SEPARATOR_3,
				CLIP_PARAM_PLAYBACK_SPEED,
				CLIP_PARAM_SEPARATOR_4,

				CLIP_PARAM_COUNT
			};

			enum
			{
				GENERAL_PARAM_SEPARATOR_0,
				GENERAL_PARAM_SHOW_BIND_POS,
				GENERAL_PARAM_SEPARATOR_1,
				GENERAL_PARAM_DRAW_BONES,
				GENERAL_PARAM_SEPARATOR_2,
				GENERAL_PARAM_DRAW_PLANE,
				GENERAL_PARAM_SEPARATOR_3,
				GENERAL_PARAM_DRAW_SHADOWS,

				GENERAL_PARAM_COUNT
			};

			static const uint32_t maxWidgetCount = max((uint32_t)CLIP_PARAM_COUNT, (uint32_t)GENERAL_PARAM_COUNT);

			UIWidget widgetBases[maxWidgetCount] = {};
			UIWidget* widgets[maxWidgetCount];
			for (uint32_t i = 0; i < maxWidgetCount; ++i)
				widgets[i] = &widgetBases[i];

			// Set all separators
			SeparatorWidget separator;
			for (uint32_t i = 0; i < maxWidgetCount; i += 2)
			{
				widgets[i]->mType = WIDGET_TYPE_SEPARATOR;
				widgets[i]->mLabel[0] = '\0';
				widgets[i]->pWidget = &separator;
			}

			// STAND CLIP
			//
			CollapsingHeaderWidget collapsingClipWidgets;
			collapsingClipWidgets.pGroupedWidgets = widgets;
			collapsingClipWidgets.mWidgetsCount = CLIP_PARAM_COUNT;

			// Play/Pause - Checkbox
			CheckboxWidget playCheckbox = {};
			playCheckbox.pData = gUIData.mClip.mPlay;
			widgets[CLIP_PARAM_PLAY]->mType = WIDGET_TYPE_CHECKBOX;
			strcpy(widgets[CLIP_PARAM_PLAY]->mLabel, "Play");
			widgets[CLIP_PARAM_PLAY]->pWidget = &playCheckbox;

			// Loop - Checkbox
			CheckboxWidget loopCheckbox = {};
			loopCheckbox.pData = gUIData.mClip.mLoop;
			widgets[CLIP_PARAM_LOOP]->mType = WIDGET_TYPE_CHECKBOX;
			strcpy(widgets[CLIP_PARAM_LOOP]->mLabel, "Loop");
			widgets[CLIP_PARAM_LOOP]->pWidget = &loopCheckbox;

			// Animation Time - Slider
			float             fValMin = 0.0f;
			float             fValMax = gClipController.mDuration;
			float             sliderStepSize = 0.01f;

			SliderFloatWidget animationTime;
			animationTime.pData = &gUIData.mClip.mAnimationTime;
			animationTime.mMin = fValMin;
			animationTime.mMax = fValMax;
			animationTime.mStep = sliderStepSize;
			widgets[CLIP_PARAM_ANIMATION_TIME]->mType = WIDGET_TYPE_SLIDER_FLOAT;
			strcpy(widgets[CLIP_PARAM_ANIMATION_TIME]->mLabel, "Animation Time");
			widgets[CLIP_PARAM_ANIMATION_TIME]->pWidget = &animationTime;
			uiSetWidgetOnActiveCallback(widgets[CLIP_PARAM_ANIMATION_TIME], nullptr, ClipTimeChangeCallback);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			SliderFloatWidget playbackSpeed;
			playbackSpeed.pData = gUIData.mClip.mPlaybackSpeed;
			playbackSpeed.mMin = fValMin;
			playbackSpeed.mMax = fValMax;
			playbackSpeed.mStep = sliderStepSize;
			widgets[CLIP_PARAM_PLAYBACK_SPEED]->mType = WIDGET_TYPE_SLIDER_FLOAT;
			strcpy(widgets[CLIP_PARAM_PLAYBACK_SPEED]->mLabel, "Playback Speed");
			widgets[CLIP_PARAM_PLAYBACK_SPEED]->pWidget = &playbackSpeed;

			luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "Clip", &collapsingClipWidgets, WIDGET_TYPE_COLLAPSING_HEADER));
			
			// GENERAL SETTINGS
			//
			CollapsingHeaderWidget collapsingGeneralSettingsWidgets;
			collapsingGeneralSettingsWidgets.pGroupedWidgets = widgets;
			collapsingGeneralSettingsWidgets.mWidgetsCount = GENERAL_PARAM_COUNT;

			// ShowBindPose - Checkbox
			CheckboxWidget showBindPose;
			showBindPose.pData = &gUIData.mGeneralSettings.mShowBindPose;
			widgets[GENERAL_PARAM_SHOW_BIND_POS]->mType = WIDGET_TYPE_CHECKBOX;
			strcpy(widgets[GENERAL_PARAM_SHOW_BIND_POS]->mLabel, "Show Bind Pose");
			widgets[GENERAL_PARAM_SHOW_BIND_POS]->pWidget = &showBindPose;

			// DrawBones - Checkbox
			CheckboxWidget drawBones;
			drawBones.pData = &gUIData.mGeneralSettings.mDrawBones;
			widgets[GENERAL_PARAM_DRAW_BONES]->mType = WIDGET_TYPE_CHECKBOX;
			strcpy(widgets[GENERAL_PARAM_DRAW_BONES]->mLabel, "DrawBones");
			widgets[GENERAL_PARAM_DRAW_BONES]->pWidget = &drawBones;

			// DrawPlane - Checkbox
			CheckboxWidget drawPlane;
			drawPlane.pData = &gUIData.mGeneralSettings.mDrawPlane;
			widgets[GENERAL_PARAM_DRAW_PLANE]->mType = WIDGET_TYPE_CHECKBOX;
			strcpy(widgets[GENERAL_PARAM_DRAW_PLANE]->mLabel, "Draw Plane");
			widgets[GENERAL_PARAM_DRAW_PLANE]->pWidget = &drawPlane;
			widgets[GENERAL_PARAM_DRAW_PLANE]->pOnActive = NULL;  // Clear pointer to &ClipTimeChangeCallback

			// DrawShadows - Checkbox
			CheckboxWidget drawShadows;
			drawShadows.pData = &gUIData.mGeneralSettings.mDrawShadows;
			widgets[GENERAL_PARAM_DRAW_SHADOWS]->mType = WIDGET_TYPE_CHECKBOX;
			strcpy(widgets[GENERAL_PARAM_DRAW_SHADOWS]->mLabel, "Draw Shadows");
			widgets[GENERAL_PARAM_DRAW_SHADOWS]->pWidget = &drawShadows;

			luaRegisterWidget(uiCreateComponentWidget(pStandaloneControlsGUIWindow, "General Settings", &collapsingGeneralSettingsWidgets, WIDGET_TYPE_COLLAPSING_HEADER));

		}

		waitForAllResourceLoads();

		// SETUP THE MAIN CAMERA
		//
		CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
		vec3                   camPos{ -3.0f, 3.0f, 5.0f };
		vec3                   lookAt{ 0.0f, 1.0f, 0.0f };

		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; }};
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
			{
				uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			}
			return true;
		};

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);
		
		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		gStickFigureRig.Exit();
		gClip.Exit();
		gAnimation.Exit();
		gStickFigureAnimObject.Exit();

		exitCameraController(pCameraController);

		// Need to free memory;
		tf_free(pBonePoints);
		tf_free(pJointPoints);

		exitProfiler();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pPlaneUniformBuffer[i]);
		}

		removeResource(pGeom);
		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pUniformBufferBones[i]);
		removeResource(pTextureDiffuse);

		removeResource(pJointVertexBuffer);
		removeResource(pBoneVertexBuffer);
		removeResource(pPlaneVertexBuffer);

		removeSampler(pRenderer, pDefaultSampler);

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

		// Animation data
		gSkeletonBatcher.Exit();

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			if (!addSwapChain())
				return false;

			if (!addDepthBuffer())
				return false;
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

		prepareDescriptorSets();

		UserInterfaceLoadDesc uiLoad = {};
		uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		uiLoad.mHeight = mSettings.mHeight;
		uiLoad.mWidth = mSettings.mWidth;
		uiLoad.mLoadType = pReloadDesc->mType;
		loadUserInterface(&uiLoad);

		FontSystemLoadDesc fontLoad = {};
		fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		fontLoad.mHeight = mSettings.mHeight;
		fontLoad.mWidth = mSettings.mWidth;
		fontLoad.mLoadType = pReloadDesc->mType;
		loadFontSystem(&fontLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTarget(pRenderer, pDepthBuffer);
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime)
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

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

		if (!gUIData.mGeneralSettings.mShowBindPose)
		{
			// Pose the rig based on the animated object's updated values
			gStickFigureAnimObject.ComputePose(gStickFigureAnimObject.mRootTransform);
		}
		else
		{
			// Ignore the updated values and pose in bind
			gStickFigureAnimObject.ComputeBindPose(gStickFigureAnimObject.mRootTransform);
		}

		// Record animation update time
		getHiresTimerUSec(&gAnimationUpdateTimer, true);

		// Update uniforms that will be shared between all skeletons
		gSkeletonBatcher.SetSharedUniforms(projViewMat, lightPos, lightColor);

		for (uint i = 0; i < pGeom->mJointCount; ++i)
		{
			gUniformDataBones.mBoneMatrix[i] = //mat4::scale(vec3(1, 1, -1)) *
				gStickFigureAnimObject.mJointWorldMats[pGeom->pJointRemaps[i]] *
				pGeom->pInverseBindPoses[i];

		}

		/************************************************************************/
		// Shadow Capsules
		/************************************************************************/

		if (gUIData.mGeneralSettings.mDrawShadows)
		{
			gUniformDataPlane.capsules_count = 8;
			//head
			gUniformDataPlane.capsules[0] = generate_cap(getBonePos(19), getBonePos(76), 0.2f);

			//spine
			gUniformDataPlane.capsules[1] = generate_cap(getBonePos(1), getBonePos(19), 0.2f);

			//upleg
			gUniformDataPlane.capsules[2] = generate_cap(getBonePos(2), getBonePos(5), 0.15f);
			gUniformDataPlane.capsules[3] = generate_cap(getBonePos(3), getBonePos(10), 0.15f); 

			//leg
			gUniformDataPlane.capsules[4] = generate_cap(getBonePos(5), getBonePos(6), 0.1f);
			gUniformDataPlane.capsules[5] = generate_cap(getBonePos(10), getBonePos(11), 0.1f);

			//foot
			gUniformDataPlane.capsules[6] = generate_cap(getBonePos(6), getBonePos(8), 0.1f); 
			gUniformDataPlane.capsules[7] = generate_cap(getBonePos(11), getBonePos(13), 0.1f);

			//looks better without the arms as capsules look weird when they align with light vector
			////arm
			//gUniformDataPlane.capsules[8] = generate_cap(getBonePos(20), getBonePos(21), 0.08f);
			//gUniformDataPlane.capsules[9] = generate_cap(getBonePos(48), getBonePos(49), 0.08f);

			////forearm
			//gUniformDataPlane.capsules[8] = generate_cap(getBonePos(21), getBonePos(22), 0.08f);
			//gUniformDataPlane.capsules[9] = generate_cap(getBonePos(49), getBonePos(50), 0.08f);
		}
		else
		{
			gUniformDataPlane.capsules_count = 0;
		}

		
		/************************************************************************/
		// Plane
		/************************************************************************/
		gUniformDataPlane.mProjectView = projViewMat;
		gUniformDataPlane.mToWorldMat = mat4::identity();
	}

	Vector3 getBonePos(uint i)
	{
		return gStickFigureAnimObject.mJointWorldMats[i].getCol3().getXYZ();
	};

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

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

		// UPDATE UNIFORM BUFFERS
		//

		// Update all the instanced uniform data for each batch of joints and bones
		gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

		BufferUpdateDesc planeViewProjCbv = { pPlaneUniformBuffer[gFrameIndex] };
		beginUpdateResource(&planeViewProjCbv);
		*(UniformBlockPlane*)planeViewProjCbv.pMappedData = gUniformDataPlane;
		endUpdateResource(&planeViewProjCbv, NULL);

		BufferUpdateDesc boneBufferUpdateDesc = { pUniformBufferBones[gFrameIndex] };
		beginUpdateResource(&boneBufferUpdateDesc);
		memcpy(boneBufferUpdateDesc.pMappedData, &gUniformDataBones, sizeof(mat4) * gStickFigureRig.mNumJoints);
		endUpdateResource(&boneBufferUpdateDesc, NULL);

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

		//// draw plane
		if (gUIData.mGeneralSettings.mDrawPlane)
		{
			const uint32_t stride = sizeof(float) * 6;
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Plane");
			cmdBindPipeline(cmd, pPlaneDrawPipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet);
			cmdBindVertexBuffer(cmd, 1, &pPlaneVertexBuffer, &stride, NULL);
			cmdDraw(cmd, 6, 0);
			cmdEndDebugMarker(cmd);
		}

		//// draw the skeleton of the rig
		if (gUIData.mGeneralSettings.mDrawBones)
		{
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skeletons");
			gSkeletonBatcher.Draw(cmd, gFrameIndex);
			cmdEndDebugMarker(cmd);
		}

		//// draw skinned mesh
		if (!gUIData.mGeneralSettings.mDrawBones)
		{
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Skinned Mesh");
			cmdBindPipeline(cmd, pPipelineSkinning);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkinning[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkinning[1]);
			cmdBindVertexBuffer(cmd, 1, &pGeom->pVertexBuffers[0], pGeom->mVertexStrides, (uint64_t*)NULL);
			cmdBindIndexBuffer(cmd, pGeom->pIndexBuffer, pGeom->mIndexType, (uint64_t)NULL);
			cmdDrawIndexed(cmd, pGeom->mIndexCount, 0, 0);
			cmdEndDebugMarker(cmd);
		}

		//// draw the UI
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID; 
		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

		snprintf(gAnimationUpdateText,  64, "Animation Update %f ms", getHiresTimerUSecAverage(&gAnimationUpdateTimer) / 1000.0f);

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

	const char* GetName() { return "28_Skinning"; }

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
		swapChainDesc.mColorClearValue = { { 0.15f, 0.15f, 0.15f, 1.0f } };
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
		setDesc = { pRootSignatureSkinning, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkinning[0]);
		setDesc = { pRootSignatureSkinning, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkinning[1]);

		gSkeletonBatcher.AddDescriptorSets(pRootSignature);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSet);
		removeDescriptorSet(pRenderer, pDescriptorSetSkinning[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkinning[1]);

		gSkeletonBatcher.RemoveDescriptorSets();
	}

	void addRootSignatures()
	{
		Shader*           shaders[] = { pSkeletonShader, pPlaneDrawShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		const char* staticSamplers[] = { "DefaultSampler" };

		RootSignatureDesc skinningRootSignatureDesc = {};
		skinningRootSignatureDesc.mShaderCount = 1;
		skinningRootSignatureDesc.ppShaders = &pShaderSkinning;
		skinningRootSignatureDesc.mStaticSamplerCount = 1;
		skinningRootSignatureDesc.ppStaticSamplerNames = staticSamplers;
		skinningRootSignatureDesc.ppStaticSamplers = &pDefaultSampler;
		addRootSignature(pRenderer, &skinningRootSignatureDesc, &pRootSignatureSkinning);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignatureSkinning);
		removeRootSignature(pRenderer, pRootSignature);
	}

	void addShaders()
	{
		ShaderLoadDesc planeShader = {};
		planeShader.mStages[0] = { "plane.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		planeShader.mStages[1] = { "plane.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		ShaderLoadDesc skinningShader = {};
		skinningShader.mStages[0] = { "skinning.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skinningShader.mStages[1] = { "skinning.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &planeShader, &pPlaneDrawShader);
		addShader(pRenderer, &basicShader, &pSkeletonShader);
		addShader(pRenderer, &skinningShader, &pShaderSkinning);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShaderSkinning);
		removeShader(pRenderer, pSkeletonShader);
		removeShader(pRenderer, pPlaneDrawShader);
	}

	void addPipelines()
	{
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

		// layout and pipeline for skinning
		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureSkinning;
		pipelineSettings.pShaderProgram = pShaderSkinning;
		pipelineSettings.pVertexLayout = &gVertexLayoutSkinned;
		pipelineSettings.pRasterizerState = &skeletonRasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPipelineSkinning);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipelineSkinning);
		removePipeline(pRenderer, pPlaneDrawPipeline);
		removePipeline(pRenderer, pSkeletonPipeline);
	}

	void prepareDescriptorSets()
	{
		gSkeletonBatcher.PrepareDescriptorSets();

		DescriptorData params[1] = {};
		params[0].pName = "DiffuseTexture";
		params[0].ppTextures = &pTextureDiffuse;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSkinning[0], 1, params);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[2] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pPlaneUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSet, 1, params);

			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pPlaneUniformBuffer[i];
			params[1].pName = "boneMatrices";
			params[1].ppBuffers = &pUniformBufferBones[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetSkinning[1], 2, params);
		}
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
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
};

DEFINE_APPLICATION_MAIN(Skinning)
