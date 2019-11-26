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

#define MAX_NUM_BONES 200

// Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"

#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

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
// tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

// Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#ifdef __APPLE__
#include "../../../../Common_3/Tools/AssetPipeline/src/AssetPipeline.h"
#endif

// Memory
#include "../../../../Common_3/OS/Interfaces/IMemory.h"

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t gImageCount = 3;
bool           gMicroProfiler = false;
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

VirtualJoystickUI gVirtualJoystick;
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
const char* gDiffuseTexture = "Stormtrooper_D";

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
        // FILE PATHS
        PathHandle programDirectory = fsCopyProgramDirectoryPath();
        if (!fsPlatformUsesBundledResources())
        {
            PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/28_Skinning");
            fsSetResourceDirectoryRootPath(resourceDirRoot);
            
            fsSetRelativePathForResourceDirectory(RD_TEXTURES,        "../../UnitTestResources/Textures");
            fsSetRelativePathForResourceDirectory(RD_MESHES,             "../../UnitTestResources/Meshes");
            fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS,     "../../UnitTestResources/Fonts");
            fsSetRelativePathForResourceDirectory(RD_ANIMATIONS,         "../../UnitTestResources/Animation");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT,     "../../../../Middleware_3/Text");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI,     "../../../../Middleware_3/UI");
        }
        
#if defined(__APPLE__) && !defined(TARGET_IOS)
		ProcessAssetsSettings animationSettings = {};
		animationSettings.quiet = false;
		animationSettings.force = false;
		animationSettings.minLastModifiedTime = 0;
        PathHandle fbxDir = fsCopyPathInResourceDirectory(RD_ANIMATIONS, "fbx");
        PathHandle animationDir = fsCopyPathForResourceDirectory(RD_ANIMATIONS);
		AssetPipeline::ProcessAnimations(fbxDir, animationDir, &animationSettings);
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
		initResourceLoaderInterface(pRenderer);

		if (!gVirtualJoystick.Init(pRenderer, "circlepad", RD_TEXTURES))
			return false;

    // INITIALIZE THE USER INTERFACE
    //
    if (!gAppUI.Init(pRenderer))
      return false;

    gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);

		initProfiler();

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		/************************************************************************/
		// SETUP ANIMATION STRUCTURES
		/************************************************************************/

		// RIGS
		//
        PathHandle fullPath = fsCopyPathInResourceDirectory(RD_ANIMATIONS, gStickFigureName);

		// Initialize the rig with the path to its ozz file
		gStickFigureRig.Initialize(fullPath);

		// Add the rig to the list of skeletons to render
		gSkeletonBatcher.AddRig(&gStickFigureRig);

		// CLIPS
		//
		fullPath = fsCopyPathInResourceDirectory(RD_ANIMATIONS, gClipName);

		gClip.Initialize(fullPath, &gStickFigureRig);

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
		planeShader.mStages[0] = { "plane.vert", NULL, 0, RD_SHADER_SOURCES };
		planeShader.mStages[1] = { "plane.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, RD_SHADER_SOURCES };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, RD_SHADER_SOURCES };

		char           maxNumBonesMacroBuffer[4] = {};
		sprintf(maxNumBonesMacroBuffer, "%i", gStickFigureRig.GetNumJoints());
		ShaderMacro    maxNumBonesMacro = { "MAX_NUM_BONES", maxNumBonesMacroBuffer };
		ShaderLoadDesc skinningShader = {};
		skinningShader.mStages[0] = { "skinning.vert", &maxNumBonesMacro, 1, RD_SHADER_SOURCES };
		skinningShader.mStages[1] = { "skinning.frag", &maxNumBonesMacro, 1, RD_SHADER_SOURCES };

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

		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
		setDesc = { pRootSignatureSkinning, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkinning[0]);
		setDesc = { pRootSignatureSkinning, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkinning[1]);

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
		fullPath = fsCopyPathInResourceDirectory(RD_ANIMATIONS, "fbx/stormtrooper/riggedMesh.fbx");

		AssimpImporter        importer;
		AssimpImporter::Model model = {};
		if (!importer.ImportModel(fullPath, &model))
			return false;

		eastl::vector<Vertex> vertices;
		eastl::vector<uint>   indices;
		eastl::vector<mat4>   boneOffsetMatrices(gStickFigureRig.GetNumJoints(), mat4::identity());
		for (uint i = 0; i < (uint)model.mMeshArray.size(); ++i)
		{
			AssimpImporter::Mesh* mesh = &model.mMeshArray[i];
			uint                  startVertices = (uint)vertices.size();
			uint                  vertexCount = (uint)mesh->mPositions.size();
            mat4 localToWorldMat = mat4::identity();
            const AssimpImporter::Node* node = importer.FindMeshNode(&model.mRootNode, i);
            if (node)
                localToWorldMat = node->mTransform;

			vertices.resize(startVertices + vertexCount);
			for (uint j = 0; j < vertexCount; ++j)
			{
                vec4 pos = vec4(mesh->mPositions[j].x, mesh->mPositions[j].y, mesh->mPositions[j].z, 1);
                vec4 normal = vec4(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z, 0);
                vertices[startVertices + j].mPosition = v3ToF3((localToWorldMat * pos).getXYZ());
				vertices[startVertices + j].mNormal = v3ToF3(normalize((localToWorldMat * normal).getXYZ()));
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
                        boneOffsetMatrices[jointIndex] = mesh->mBones[j].mOffsetMatrix * inverse(localToWorldMat);
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

        PathHandle texturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, gDiffuseTexture);
		TextureLoadDesc diffuseTextureDesc = {};
		diffuseTextureDesc.pFilePath = texturePath;
		diffuseTextureDesc.ppTexture = &pTextureDiffuse;
		addResource(&diffuseTextureDesc);

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

		finishResourceLoading();

		// SETUP THE MAIN CAMERA
		//
		CameraMotionParameters cmp{ 50.0f, 75.0f, 150.0f };
		vec3                   camPos{ 3.0f, 3.0f, -5.0f };
		vec3                   lookAt{ 0.0f, 1.0f, 0.0f };

		pCameraController = createFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		// Add the GUI Panels/Windows
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 16 };

		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.05f };
		vec2    UIPanelSize = { 650, 1000 };
		GuiDesc guiDesc(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		pStandaloneControlsGUIWindow = gAppUI.AddGuiComponent("Animation", &guiDesc);
    pStandaloneControlsGUIWindow->AddWidget(CheckboxWidget("Toggle Micro Profiler", &gMicroProfiler));

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
				bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		typedef bool (*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!gMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.5f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);
		
		// Prepare descriptor sets
		DescriptorData params[2] = {};
		params[0].pName = "boneOffsetMatrices";
		params[0].ppBuffers = &pBoneOffsetBuffer;
		params[1].pName = "DiffuseTexture";
		params[1].ppTextures = &pTextureDiffuse;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSkinning[0], 2, params);

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

		return true;
	}

	void Exit()
	{
		// wait for rendering to finish before freeing resources
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		exitProfiler();

		// Animation data
		gSkeletonBatcher.Destroy();
		gStickFigureRig.Destroy();
		gClip.Destroy();
		gAnimation.Destroy();
		gStickFigureAnimObject.Destroy();

		destroyCameraController(pCameraController);

		gVirtualJoystick.Exit();

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
		removeDescriptorSet(pRenderer, pDescriptorSet);
		removeDescriptorSet(pRenderer, pDescriptorSetSkinning[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkinning[1]);

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

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

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

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
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
		pipelineSettings.pRasterizerState = pPlaneRast;
		pipelineSettings.pShaderProgram = pPlaneDrawShader;
		addPipeline(pRenderer, &desc, &pPlaneDrawPipeline);

		// layout and pipeline for skinning
		vertexLayout = {};
		vertexLayout.mAttribCount = 5;
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
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[2].mBinding = 0;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);
		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TEXCOORD1;
		vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[3].mBinding = 0;
		vertexLayout.mAttribs[3].mLocation = 3;
		vertexLayout.mAttribs[3].mOffset = 8 * sizeof(float);
		vertexLayout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD2;
		vertexLayout.mAttribs[4].mFormat = TinyImageFormat_R32G32B32A32_UINT;
		vertexLayout.mAttribs[4].mBinding = 0;
		vertexLayout.mAttribs[4].mLocation = 4;
		vertexLayout.mAttribs[4].mOffset = 12 * sizeof(float);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureSkinning;
		pipelineSettings.pShaderProgram = pShaderSkinning;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pSkeletonRast;
		addPipeline(pRenderer, &desc, &pPipelineSkinning);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

    unloadProfiler();

		gAppUI.Unload();

		gVirtualJoystick.Unload();

		removePipeline(pRenderer, pPipelineSkinning);
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
			LOGF(eINFO, "Animation NOT Updating!");

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

    if (gMicroProfiler != bPrevToggleMicroProfiler)
    {
      toggleProfiler();
      bPrevToggleMicroProfiler = gMicroProfiler;
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

		// FRAME SYNC & ACQUIRE SWAPCHAIN RENDER TARGET
		//
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

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
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

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
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet);
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
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkinning[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkinning[1]);
			cmdBindVertexBuffer(cmd, 1, &pVertexBuffer, (uint64_t*)NULL);
			cmdBindIndexBuffer(cmd, pIndexBuffer, (uint64_t)NULL);
			cmdDrawIndexed(cmd, gIndexCount, 0, 0);
			cmdEndDebugMarker(cmd);
		}

		//// draw the UI
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		gTimer.GetUSec(true);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		gAppUI.Gui(pStandaloneControlsGUIWindow);    // adds the gui element to AppUI::ComponentsToUpdate list
		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);
		gAppUI.DrawText(
			cmd, float2(8, 65), eastl::string().sprintf("Animation Update %f ms", gAnimationUpdateTimer.GetUSecAverage() / 1000.0f).c_str(),
			&gFrameTimeDraw);

		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);

		cmdDrawProfiler();
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		// PRESENT THE GRPAHICS QUEUE
		//
		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
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
        depthRT.mClearValue.depth = 1.0f;
        depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
};

DEFINE_APPLICATION_MAIN(Skinning)
