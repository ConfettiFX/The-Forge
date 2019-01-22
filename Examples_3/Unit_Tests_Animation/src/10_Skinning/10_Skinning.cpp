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
* The Forge - ANIMATION - SKINNING UNIT TEST
*
* The purpose of this demo is to show how to use the asset pipeline and how to do GPU skinning.
*
*********************************************************************************************************/

#define MAX_NUM_BONES 1024

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

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

#ifdef __APPLE__
#include "../../../../Common_3/Tools/AssetPipeline/AssetPipeline.h"
#endif

// Memory
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[FSR_Count] = {
	"../../../src/10_Skinning/",            // FSR_BinShaders
	"../../../src/10_Skinning/",            // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/10_Skinning/",            // FSR_GpuConfig
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

Sampler* pDefaultSampler = NULL;

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

Shader*        pShaderSkinning = NULL;
RootSignature* pRootSignatureSkinning = NULL;
Pipeline*      pPipelineSkinning = NULL;

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

Buffer*          pVertexBuffer = NULL;
Buffer*          pIndexBuffer = NULL;
uint             gIndexCount = 0;
Buffer*          pBoneOffsetBuffer = NULL;
Buffer*          pUniformBufferBones[gImageCount] = { NULL };
UniformDataBones gUniformDataBones;
Texture*         pTextureDiffuse = NULL;

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
const char* gDiffuseTexture = "Stormtrooper_D.png";

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
	};
	GeneralSettingsData mGeneralSettings;
};
UIData gUIData;

// Hard set the controller's time ratio via callback when it is set in the UI
void ClipTimeChangeCallback() { gClipController.SetTimeRatioHard(gUIData.mClip.mAnimationTime); }

//--------------------------------------------------------------------------------------------
// APP CODE
//--------------------------------------------------------------------------------------------
class Skinning: public IApp
{
	public:
	bool Init()
	{
#if defined(__APPLE__) && !defined(TARGET_IOS)
		ProcessAssetsSettings animationSettings = {};
		animationSettings.quiet = false;
		animationSettings.force = false;
		animationSettings.minLastModifiedTime = 0;
		AssetPipeline::ProcessAnimations(
			FileSystem::FixPath("fbx", FSR_Animation), FileSystem::FixPath("", FSR_Animation), &animationSettings);
#endif
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

		/************************************************************************/
		// SETUP ANIMATION STRUCTURES
		/************************************************************************/

		// RIGS
		//
		tinystl::string fullPath = FileSystem::FixPath(gStickFigureName, FSR_Animation);

		// Initialize the rig with the path to its ozz file
		gStickFigureRig.Initialize(fullPath.c_str());

		// Add the rig to the list of skeletons to render
		gSkeletonBatcher.AddRig(&gStickFigureRig);

		// CLIPS
		//
		fullPath = FileSystem::FixPath(gClipName, FSR_Animation);

		gClip.Initialize(fullPath.c_str(), &gStickFigureRig);

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
		ShaderLoadDesc planeShader = {};
		planeShader.mStages[0] = { "plane.vert", NULL, 0, FSR_SrcShaders };
		planeShader.mStages[1] = { "plane.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };

		ShaderMacro    maxNumBonesMacro = { "MAX_NUM_BONES", tinystl::string::format("%i", gStickFigureRig.GetNumJoints()) };
		ShaderLoadDesc skinningShader = {};
		skinningShader.mStages[0] = { "skinning.vert", &maxNumBonesMacro, 1, FSR_SrcShaders };
		skinningShader.mStages[1] = { "skinning.frag", &maxNumBonesMacro, 1, FSR_SrcShaders };

		addShader(pRenderer, &planeShader, &pPlaneDrawShader);
		addShader(pRenderer, &basicShader, &pSkeletonShader);
		addShader(pRenderer, &skinningShader, &pShaderSkinning);

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
		// LOAD SKINNED MESH
		/************************************************************************/
		fullPath = FileSystem::FixPath("fbx/stormtrooper/riggedMesh.fbx", FSR_Animation);

		AssimpImporter        importer;
		AssimpImporter::Model model = {};
		if (!importer.ImportModel(fullPath.c_str(), &model))
			return false;

		tinystl::vector<Vertex> vertices;
		tinystl::vector<uint>   indices;
		tinystl::vector<mat4>   boneOffsetMatrices(gStickFigureRig.GetNumJoints(), mat4::identity());
		for (uint i = 0; i < (uint)model.mMeshArray.size(); ++i)
		{
			AssimpImporter::Mesh* mesh = &model.mMeshArray[i];
			uint                  startVertices = (uint)vertices.size();
			uint                  vertexCount = (uint)mesh->mPositions.size();

			vertices.resize(startVertices + vertexCount);
			for (uint j = 0; j < vertexCount; ++j)
			{
				vertices[startVertices + j].mPosition = mesh->mPositions[j];
				vertices[startVertices + j].mNormal = mesh->mNormals[j];
				vertices[startVertices + j].mUV = mesh->mUvs[j];
			}

			if (!mesh->mBoneNames.empty() && !mesh->mBoneWeights.empty())
			{
				for (uint j = 0; j < vertexCount; ++j)
				{
					vertices[startVertices + j].mBoneWeights = mesh->mBoneWeights[j];
					for (uint k = 0; k < 4; ++k)
					{
						if (mesh->mBoneWeights[j][k] > 0.0f)
						{
							int boneIndex = gStickFigureRig.FindJoint(mesh->mBoneNames[j].mNames[k].c_str());
							vertices[startVertices + j].mBoneIndices[k] = (uint)boneIndex;
						}
					}
				}

				for (size_t j = 0; j < mesh->mBones.size(); ++j)
				{
					int jointIndex = gStickFigureRig.FindJoint(mesh->mBones[j].mName.c_str());
					if (jointIndex >= 0)
						boneOffsetMatrices[jointIndex] = mesh->mBones[j].mOffsetMatrix;
				}
			}

			uint startIndices = (uint)indices.size();
			uint indexCount = (uint)mesh->mIndices.size();
			indices.resize(startIndices + indexCount);
			for (uint j = 0; j < indexCount; ++j)
				indices[startIndices + j] = mesh->mIndices[j] + startVertices;
		}

		gIndexCount = (uint)indices.size();

		BufferLoadDesc vertexBufferDesc = {};
		vertexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vertexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vertexBufferDesc.mDesc.mSize = sizeof(Vertex) * vertices.size();
		vertexBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
		vertexBufferDesc.pData = vertices.data();
		vertexBufferDesc.ppBuffer = &pVertexBuffer;
		addResource(&vertexBufferDesc);

		BufferLoadDesc indexBufferDesc = {};
		indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mDesc.mSize = sizeof(uint) * indices.size();
		indexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		indexBufferDesc.pData = indices.data();
		indexBufferDesc.ppBuffer = &pIndexBuffer;
		addResource(&indexBufferDesc);

		BufferLoadDesc boneOffsetBufferDesc = {};
		boneOffsetBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		boneOffsetBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boneOffsetBufferDesc.mDesc.mSize = sizeof(mat4) * gStickFigureRig.GetNumJoints();
		boneOffsetBufferDesc.pData = boneOffsetMatrices.data();
		boneOffsetBufferDesc.ppBuffer = &pBoneOffsetBuffer;
		addResource(&boneOffsetBufferDesc);

		BufferLoadDesc boneBufferDesc = {};
		boneBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		boneBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		boneBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		boneBufferDesc.mDesc.mSize = sizeof(mat4) * gStickFigureRig.GetNumJoints();
		boneBufferDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			boneBufferDesc.ppBuffer = &pUniformBufferBones[i];
			addResource(&boneBufferDesc);
		}

		TextureLoadDesc diffuseTextureDesc = {};
		diffuseTextureDesc.mRoot = FSR_Textures;
		diffuseTextureDesc.mUseMipmaps = true;
		diffuseTextureDesc.pFilename = gDiffuseTexture;
		diffuseTextureDesc.ppTexture = &pTextureDiffuse;
		addResource(&diffuseTextureDesc);

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

		finishResourceLoading();

		// SETUP THE MAIN CAMERA
		//
		CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
		vec3                   camPos{ 3.0f, 3.0f, -5.0f };
		vec3                   lookAt{ 0.0f, 1.0f, 0.0f };

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
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 16 };

		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.05f };
		vec2    UIPanelSize = { 650, 1000 };
		GuiDesc guiDesc(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		pStandaloneControlsGUIWindow = gAppUI.AddGuiComponent("Animation", &guiDesc);

		// SET gUIData MEMBERS THAT NEED POINTERS TO ANIMATION DATA
		//

		// Clip
		gUIData.mClip.mPlay = gClipController.GetPlayPtr();
		gUIData.mClip.mLoop = gClipController.GetLoopPtr();
		gUIData.mClip.mPlaybackSpeed = gClipController.GetPlaybackSpeedPtr();

		// SET UP GUI BASED ON gUIData STRUCT
		//
		{
			// STAND CLIP
			//
			CollapsingHeaderWidget CollapsingClipWidgets("Clip");

			// Play/Pause - Checkbox
			CollapsingClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingClipWidgets.AddSubWidget(CheckboxWidget("Play", gUIData.mClip.mPlay));

			// Loop - Checkbox
			CollapsingClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingClipWidgets.AddSubWidget(CheckboxWidget("Loop", gUIData.mClip.mLoop));

			// Animation Time - Slider
			float             fValMin = 0.0f;
			float             fValMax = gClipController.GetDuration();
			float             sliderStepSize = 0.01f;
			SliderFloatWidget SliderAnimationTime("Animation Time", &gUIData.mClip.mAnimationTime, fValMin, fValMax, sliderStepSize);
			SliderAnimationTime.pOnActive = ClipTimeChangeCallback;

			CollapsingClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingClipWidgets.AddSubWidget(SliderAnimationTime);

			// Playback Speed - Slider
			fValMin = -5.0f;
			fValMax = 5.0f;
			sliderStepSize = 0.1f;

			CollapsingClipWidgets.AddSubWidget(SeparatorWidget());
			CollapsingClipWidgets.AddSubWidget(
				SliderFloatWidget("Playback Speed", gUIData.mClip.mPlaybackSpeed, fValMin, fValMax, sliderStepSize));
			CollapsingClipWidgets.AddSubWidget(SeparatorWidget());

			// GENERAL SETTINGS
			//
			CollapsingHeaderWidget CollapsingGeneralSettingsWidgets("General Settings");

			// ShowBindPose - Checkbox
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingGeneralSettingsWidgets.AddSubWidget(CheckboxWidget("Show Bind Pose", &gUIData.mGeneralSettings.mShowBindPose));

			// DrawBones - Checkbox
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingGeneralSettingsWidgets.AddSubWidget(CheckboxWidget("DrawBones", &gUIData.mGeneralSettings.mDrawBones));

			// DrawPlane - Checkbox
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());
			CollapsingGeneralSettingsWidgets.AddSubWidget(CheckboxWidget("Draw Plane", &gUIData.mGeneralSettings.mDrawPlane));
			CollapsingGeneralSettingsWidgets.AddSubWidget(SeparatorWidget());

			// Add all widgets to the window
			pStandaloneControlsGUIWindow->AddWidget(CollapsingClipWidgets);
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
		gClip.Destroy();
		gAnimation.Destroy();
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

		removeResource(pVertexBuffer);
		removeResource(pIndexBuffer);
		for (int i = 0; i < gImageCount; ++i)
			removeResource(pUniformBufferBones[i]);
		removeResource(pBoneOffsetBuffer);
		removeResource(pTextureDiffuse);

		removeResource(pJointVertexBuffer);
		removeResource(pBoneVertexBuffer);
		removeResource(pPlaneVertexBuffer);

		removeShader(pRenderer, pShaderSkinning);
		removeShader(pRenderer, pSkeletonShader);
		removeShader(pRenderer, pPlaneDrawShader);
		removeRootSignature(pRenderer, pRootSignatureSkinning);
		removeRootSignature(pRenderer, pRootSignature);

		removeDepthState(pDepth);

		removeRasterizerState(pSkeletonRast);
		removeRasterizerState(pPlaneRast);

		removeSampler(pRenderer, pDefaultSampler);

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

		// layout and pipeline for skinning
		vertexLayout = {};
		vertexLayout.mAttribCount = 5;
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
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[2].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[2].mBinding = 0;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);
		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TEXCOORD1;
		vertexLayout.mAttribs[3].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[3].mBinding = 0;
		vertexLayout.mAttribs[3].mLocation = 3;
		vertexLayout.mAttribs[3].mOffset = 8 * sizeof(float);
		vertexLayout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD2;
		vertexLayout.mAttribs[4].mFormat = ImageFormat::RGBA32UI;
		vertexLayout.mAttribs[4].mBinding = 0;
		vertexLayout.mAttribs[4].mLocation = 4;
		vertexLayout.mAttribs[4].mOffset = 12 * sizeof(float);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureSkinning;
		pipelineSettings.pShaderProgram = pShaderSkinning;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pSkeletonRast;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineSkinning);

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);

		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		removePipeline(pRenderer, pPipelineSkinning);
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

		vec3 lightPos = vec3(0.0f, 1000.0f, 0.0f);
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

		for (uint i = 0; i < gStickFigureRig.GetNumJoints(); ++i)
			gUniformDataBones.mBoneMatrix[i] = gStickFigureRig.GetJointWorldMat(i);

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

		// FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
		//
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);

		// UPDATE UNIFORM BUFFERS
		//

		// Update all the instanced uniform data for each batch of joints and bones
		gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

		BufferUpdateDesc planeViewProjCbv = { pPlaneUniformBuffer[gFrameIndex], &gUniformDataPlane };
		updateResource(&planeViewProjCbv);

		BufferUpdateDesc boneBufferUpdateDesc = { pUniformBufferBones[gFrameIndex], &gUniformDataBones };
		updateResource(&boneBufferUpdateDesc);

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
			DescriptorData params[4] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pPlaneUniformBuffer[gFrameIndex];
			params[1].pName = "boneMatrices";
			params[1].ppBuffers = &pUniformBufferBones[gFrameIndex];
			params[2].pName = "boneOffsetMatrices";
			params[2].ppBuffers = &pBoneOffsetBuffer;
			params[3].pName = "DiffuseTexture";
			params[3].ppTextures = &pTextureDiffuse;
			cmdBindDescriptors(cmd, pRootSignatureSkinning, 4, params);
			cmdBindVertexBuffer(cmd, 1, &pVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, pIndexBuffer, NULL);
			cmdDrawIndexed(cmd, gIndexCount, 0, 0);
			cmdEndDebugMarker(cmd);
		}

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

	tinystl::string GetName() { return "10_Skinning"; }

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

DEFINE_APPLICATION_MAIN(Skinning)
