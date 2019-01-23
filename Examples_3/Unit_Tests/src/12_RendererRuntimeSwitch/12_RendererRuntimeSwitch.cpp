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

// Unit Test for testing transformations using a solar system.
// Tests the basic mat4 transformations, such as scaling, rotation, and translation.

#define MAX_PLANETS 20    // Does not affect test, just for allocating space in uniform block. Must match with shader.

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

/// Camera Controller

/// Demo structures
struct PlanetInfoStruct
{
	uint  mParentIndex;
	vec4  mColor;
	float mYOrbitSpeed;    // Rotation speed around parent
	float mZOrbitSpeed;
	float mRotationSpeed;    // Rotation speed around self
	mat4  mTranslationMat;
	mat4  mScaleMat;
	mat4  mSharedMat;    // Matrix to pass down to children
};

struct UniformBlock
{
	mat4 mProjectView;
	mat4 mToWorldMat[MAX_PLANETS];
	vec4 mColor[MAX_PLANETS];

	// Point Light Information
	vec3 mLightPosition;
	vec3 mLightColor;
};

const uint32_t gImageCount = 3;
const int      gSphereResolution = 30;    // Increase for higher resolution spheres
const float    gSphereDiameter = 0.5f;
const uint     gNumPlanets = 11;        // Sun, Mercury -> Neptune, Pluto, Moon
const uint     gTimeOffset = 600000;    // For visually better starting locations
const float    gRotSelfScale = 0.0004f;
const float    gRotOrbitYScale = 0.001f;
const float    gRotOrbitZScale = 0.00001f;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pSphereShader = NULL;
Buffer*   pSphereVertexBuffer = NULL;
Pipeline* pSpherePipeline = NULL;

Shader*        pSkyBoxDrawShader = NULL;
Buffer*        pSkyBoxVertexBuffer = NULL;
Pipeline*      pSkyBoxDrawPipeline = NULL;
RootSignature* pRootSignature = NULL;
Sampler*       pSamplerSkyBox = NULL;
Texture*       pSkyBoxTextures[6];
#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif
DepthState*      pDepth = NULL;
RasterizerState* pSkyboxRast = NULL;

Buffer* pProjViewUniformBuffer[gImageCount] = { NULL };
Buffer* pSkyboxUniformBuffer[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;

int              gNumberOfSpherePoints;
UniformBlock     gUniformData;
UniformBlock     gUniformDataSky;
PlanetInfoStruct gPlanetInfoData[gNumPlanets];

ICameraController* pCameraController = NULL;

/// UI
UIApp         gAppUI;
GuiComponent* pGui;

FileSystem gFileSystem;
LogManager gLogManager;

const char* pSkyBoxImageFileNames[] = { "Skybox_right1.png",  "Skybox_left2.png",  "Skybox_top3.png",
										"Skybox_bottom4.png", "Skybox_front5.png", "Skybox_back6.png" };

const char* pszBases[FSR_Count] = {
	"../../../src/12_RendererRuntimeSwitch/",    // FSR_BinShaders
	"../../../src/12_RendererRuntimeSwitch/",    // FSR_SrcShaders
	"../../../UnitTestResources/",               // FSR_Textures
	"../../../UnitTestResources/",               // FSR_Meshes
	"../../../UnitTestResources/",               // FSR_Builtin_Fonts
	"../../../src/12_RendererRuntimeSwitch/",    // FSR_GpuConfig
	"",                                          // FSR_Animation
	"",                                          // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",         // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",           // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

RendererApi gTargetApi = RENDERER_API_D3D12;

class RendererRuntimeSwitch: public IApp
{
	public:
	bool Init()
	{
		loadRendererFunctions(gTargetApi);

		// window and renderer setup
		RendererDesc settings = { 0 };
		settings.mApi = gTargetApi;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		if (settings.mApi == RENDERER_API_VULKAN)
		{
			FileSystem::SetRootPath(FSR_SrcShaders, "Shaders/Vulkan/");
		}
		else if (settings.mApi == RENDERER_API_D3D12)
		{
			FileSystem::SetRootPath(FSR_SrcShaders, "Shaders/D3D12/");
		}

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

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		// Loads Skybox Textures
		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
#ifndef TARGET_IOS
			textureDesc.mRoot = FSR_Textures;
#else
			textureDesc.mRoot = FSRoot::FSR_Absolute;    // Resources on iOS are bundled with the application.
#endif
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pSkyBoxImageFileNames[i];
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			addResource(&textureDesc, true);
		}

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Absolute))
			return false;
#endif

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &basicShader, &pSphereShader);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSamplerSkyBox);

		Shader*           shaders[] = { pSphereShader, pSkyBoxDrawShader };
		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerSkyBox;
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pSkyboxRast);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		// Generate sphere vertex buffer
		float* pSpherePoints;
		generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution, gSphereDiameter);

		uint64_t       sphereDataSize = gNumberOfSpherePoints * sizeof(float);
		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mSize = sphereDataSize;
		sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		sphereVbDesc.pData = pSpherePoints;
		sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
		addResource(&sphereVbDesc);

		// Need to free memory;
		conf_free(pSpherePoints);

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
			10.0f,  -10.0f, -10.0f, 6.0f,    // -z
			-10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
			-10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

			-10.0f, -10.0f, 10.0f,  2.0f,    //-x
			-10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
			-10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

			10.0f,  -10.0f, -10.0f, 1.0f,    //+x
			10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
			10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

			-10.0f, -10.0f, 10.0f,  5.0f,    // +z
			-10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
			10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

			-10.0f, 10.0f,  -10.0f, 3.0f,    //+y
			10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
			10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

			10.0f,  -10.0f, 10.0f,  4.0f,    //-y
			10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
			-10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
		};

		uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
		addResource(&skyboxVbDesc);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
			addResource(&ubDesc);
			ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&ubDesc);
		}

		finishResourceLoading();

		// Setup planets (Rotation speeds are relative to Earth's, some values randomly given)

		// Sun
		gPlanetInfoData[0].mParentIndex = 0;
		gPlanetInfoData[0].mYOrbitSpeed = 0;    // Earth years for one orbit
		gPlanetInfoData[0].mZOrbitSpeed = 0;
		gPlanetInfoData[0].mRotationSpeed = 24.0f;    // Earth days for one rotation
		gPlanetInfoData[0].mTranslationMat = mat4::identity();
		gPlanetInfoData[0].mScaleMat = mat4::scale(vec3(10.0f));
		gPlanetInfoData[0].mColor = vec4(0.9f, 0.6f, 0.1f, 0.0f);

		// Mercury
		gPlanetInfoData[1].mParentIndex = 0;
		gPlanetInfoData[1].mYOrbitSpeed = 0.5f;
		gPlanetInfoData[1].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[1].mRotationSpeed = 58.7f;
		gPlanetInfoData[1].mTranslationMat = mat4::translation(vec3(10.0f, 0, 0));
		gPlanetInfoData[1].mScaleMat = mat4::scale(vec3(1.0f));
		gPlanetInfoData[1].mColor = vec4(0.7f, 0.3f, 0.1f, 1.0f);

		// Venus
		gPlanetInfoData[2].mParentIndex = 0;
		gPlanetInfoData[2].mYOrbitSpeed = 0.8f;
		gPlanetInfoData[2].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[2].mRotationSpeed = 243.0f;
		gPlanetInfoData[2].mTranslationMat = mat4::translation(vec3(20.0f, 0, 5));
		gPlanetInfoData[2].mScaleMat = mat4::scale(vec3(2));
		gPlanetInfoData[2].mColor = vec4(0.8f, 0.6f, 0.1f, 1.0f);

		// Earth
		gPlanetInfoData[3].mParentIndex = 0;
		gPlanetInfoData[3].mYOrbitSpeed = 1.0f;
		gPlanetInfoData[3].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[3].mRotationSpeed = 1.0f;
		gPlanetInfoData[3].mTranslationMat = mat4::translation(vec3(30.0f, 0, 0));
		gPlanetInfoData[3].mScaleMat = mat4::scale(vec3(4));
		gPlanetInfoData[3].mColor = vec4(0.3f, 0.2f, 0.8f, 1.0f);

		// Mars
		gPlanetInfoData[4].mParentIndex = 0;
		gPlanetInfoData[4].mYOrbitSpeed = 2.0f;
		gPlanetInfoData[4].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[4].mRotationSpeed = 1.1f;
		gPlanetInfoData[4].mTranslationMat = mat4::translation(vec3(40.0f, 0, 0));
		gPlanetInfoData[4].mScaleMat = mat4::scale(vec3(3));
		gPlanetInfoData[4].mColor = vec4(0.9f, 0.3f, 0.1f, 1.0f);

		// Jupiter
		gPlanetInfoData[5].mParentIndex = 0;
		gPlanetInfoData[5].mYOrbitSpeed = 11.0f;
		gPlanetInfoData[5].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[5].mRotationSpeed = 0.4f;
		gPlanetInfoData[5].mTranslationMat = mat4::translation(vec3(50.0f, 0, 0));
		gPlanetInfoData[5].mScaleMat = mat4::scale(vec3(8));
		gPlanetInfoData[5].mColor = vec4(0.6f, 0.4f, 0.4f, 1.0f);

		// Saturn
		gPlanetInfoData[6].mParentIndex = 0;
		gPlanetInfoData[6].mYOrbitSpeed = 29.4f;
		gPlanetInfoData[6].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[6].mRotationSpeed = 0.5f;
		gPlanetInfoData[6].mTranslationMat = mat4::translation(vec3(60.0f, 0, 0));
		gPlanetInfoData[6].mScaleMat = mat4::scale(vec3(6));
		gPlanetInfoData[6].mColor = vec4(0.7f, 0.7f, 0.5f, 1.0f);

		// Uranus
		gPlanetInfoData[7].mParentIndex = 0;
		gPlanetInfoData[7].mYOrbitSpeed = 84.07f;
		gPlanetInfoData[7].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[7].mRotationSpeed = 0.8f;
		gPlanetInfoData[7].mTranslationMat = mat4::translation(vec3(70.0f, 0, 0));
		gPlanetInfoData[7].mScaleMat = mat4::scale(vec3(7));
		gPlanetInfoData[7].mColor = vec4(0.4f, 0.4f, 0.6f, 1.0f);

		// Neptune
		gPlanetInfoData[8].mParentIndex = 0;
		gPlanetInfoData[8].mYOrbitSpeed = 164.81f;
		gPlanetInfoData[8].mZOrbitSpeed = 0.0f;
		gPlanetInfoData[8].mRotationSpeed = 0.9f;
		gPlanetInfoData[8].mTranslationMat = mat4::translation(vec3(80.0f, 0, 0));
		gPlanetInfoData[8].mScaleMat = mat4::scale(vec3(8));
		gPlanetInfoData[8].mColor = vec4(0.5f, 0.2f, 0.9f, 1.0f);

		// Pluto - Not a planet XDD
		gPlanetInfoData[9].mParentIndex = 0;
		gPlanetInfoData[9].mYOrbitSpeed = 247.7f;
		gPlanetInfoData[9].mZOrbitSpeed = 1.0f;
		gPlanetInfoData[9].mRotationSpeed = 7.0f;
		gPlanetInfoData[9].mTranslationMat = mat4::translation(vec3(90.0f, 0, 0));
		gPlanetInfoData[9].mScaleMat = mat4::scale(vec3(1.0f));
		gPlanetInfoData[9].mColor = vec4(0.7f, 0.5f, 0.5f, 1.0f);

		// Moon
		gPlanetInfoData[10].mParentIndex = 3;
		gPlanetInfoData[10].mYOrbitSpeed = 1.0f;
		gPlanetInfoData[10].mZOrbitSpeed = 200.0f;
		gPlanetInfoData[10].mRotationSpeed = 27.0f;
		gPlanetInfoData[10].mTranslationMat = mat4::translation(vec3(5.0f, 0, 0));
		gPlanetInfoData[10].mScaleMat = mat4::scale(vec3(1));
		gPlanetInfoData[10].mColor = vec4(0.3f, 0.3f, 0.4f, 1.0f);

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
		GuiDesc guiDesc = {};
		guiDesc.mStartSize = { guiDesc.mStartSize.getX() * 0.5f, guiDesc.mStartSize.getY() * 0.4f };
		pGui = gAppUI.AddGuiComponent(GetName(), &guiDesc);

		static const char*    enumNames[] = { "D3D12", "VULKAN", NULL };
		static const uint32_t enumValues[] = { RENDERER_API_D3D12, RENDERER_API_VULKAN, 0 };

		pGui->AddWidget(DropdownWidget("Renderer API : ", (uint32_t*)&gTargetApi, enumNames, enumValues, 2));

		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);
		requestMouseCapture(true);

		pCameraController->setMotionParameters(cmp);

		InputSystem::RegisterInputEvent(cameraInputEvent);
		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		destroyCameraController(pCameraController);
		removeDebugRendererInterface();

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
			removeResource(pSkyboxUniformBuffer[i]);
		}

		removeResource(pSphereVertexBuffer);
		removeResource(pSkyBoxVertexBuffer);

		for (uint i = 0; i < 6; ++i)
			removeResource(pSkyBoxTextures[i]);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		removeSampler(pRenderer, pSamplerSkyBox);
		removeShader(pRenderer, pSphereShader);
		removeShader(pRenderer, pSkyBoxDrawShader);
		removeRootSignature(pRenderer, pRootSignature);

		removeDepthState(pDepth);
		removeRasterizerState(pSkyboxRast);

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

		unloadRendererFunctions();
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif

		//layout and pipeline for sphere draw
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
		pipelineSettings.pShaderProgram = pSphereShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pSkyboxRast;
		addPipeline(pRenderer, &pipelineSettings, &pSpherePipeline);

		//layout and pipeline for skybox draw
		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = pSkyboxRast;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		addPipeline(pRenderer, &pipelineSettings, &pSkyBoxDrawPipeline);

		return true;
	}

	void Unload()
	{
		gFrameIndex = 0;

		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);

		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pSpherePipeline);

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

		// update camera with time
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat;

		// point light parameters
		gUniformData.mLightPosition = vec3(0, 0, 0);
		gUniformData.mLightColor = vec3(0.9f, 0.9f, 0.7f);    // Pale Yellow

		// update planet transformations
		for (int i = 0; i < gNumPlanets; i++)
		{
			mat4 rotSelf, rotOrbitY, rotOrbitZ, trans, scale, parentMat;
			rotSelf = rotOrbitY = rotOrbitZ = trans = scale = parentMat = mat4::identity();
			if (gPlanetInfoData[i].mRotationSpeed > 0.0f)
				rotSelf = mat4::rotationY(gRotSelfScale * (currentTime + gTimeOffset) / gPlanetInfoData[i].mRotationSpeed);
			if (gPlanetInfoData[i].mYOrbitSpeed > 0.0f)
				rotOrbitY = mat4::rotationY(gRotOrbitYScale * (currentTime + gTimeOffset) / gPlanetInfoData[i].mYOrbitSpeed);
			if (gPlanetInfoData[i].mZOrbitSpeed > 0.0f)
				rotOrbitZ = mat4::rotationZ(gRotOrbitZScale * (currentTime + gTimeOffset) / gPlanetInfoData[i].mZOrbitSpeed);
			if (gPlanetInfoData[i].mParentIndex > 0)
				parentMat = gPlanetInfoData[gPlanetInfoData[i].mParentIndex].mSharedMat;

			trans = gPlanetInfoData[i].mTranslationMat;
			scale = gPlanetInfoData[i].mScaleMat;

			gPlanetInfoData[i].mSharedMat = parentMat * rotOrbitY * trans;
			gUniformData.mToWorldMat[i] = parentMat * rotOrbitY * rotOrbitZ * trans * rotSelf * scale;
			gUniformData.mColor[i] = gPlanetInfoData[i].mColor;
		}

		gUniformDataSky = gUniformData;
		viewMat.setTranslation(vec3(0));
		gUniformDataSky.mProjectView = projMat * viewMat;

		gAppUI.Update(deltaTime);

		static RendererApi previousApi = gTargetApi;
		if (previousApi != gTargetApi)
		{
			Unload();
			Exit();
			closeWindow(pWindow);
			openWindow(GetName(), pWindow);
			Init();
			Load();

			previousApi = gTargetApi;
		}
		/************************************************************************/
		/************************************************************************/
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&viewProjCbv);

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex], &gUniformDataSky };
		updateResource(&skyboxViewProjCbv);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 1.0f, 1.0f, 0.0f, 0.0f };
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0 };

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		//// draw skybox
		cmdBeginDebugMarker(cmd, 0, 0, 1, "Draw skybox");
		cmdBindPipeline(cmd, pSkyBoxDrawPipeline);

		DescriptorData params[7] = {};
		params[0].pName = "uniformBlock";
		params[0].ppBuffers = &pSkyboxUniformBuffer[gFrameIndex];
		params[1].pName = "RightText";
		params[1].ppTextures = &pSkyBoxTextures[0];
		params[2].pName = "LeftText";
		params[2].ppTextures = &pSkyBoxTextures[1];
		params[3].pName = "TopText";
		params[3].ppTextures = &pSkyBoxTextures[2];
		params[4].pName = "BotText";
		params[4].ppTextures = &pSkyBoxTextures[3];
		params[5].pName = "FrontText";
		params[5].ppTextures = &pSkyBoxTextures[4];
		params[6].pName = "BackText";
		params[6].ppTextures = &pSkyBoxTextures[5];
		cmdBindDescriptors(cmd, pRootSignature, 7, params);
		cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, NULL);
		cmdDraw(cmd, 36, 0);
		cmdEndDebugMarker(cmd);

		////// draw planets
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Planets");
		cmdBindPipeline(cmd, pSpherePipeline);
		params[0].ppBuffers = &pProjViewUniformBuffer[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignature, 1, params);
		cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer, NULL);
		cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, gNumPlanets, 0);
		cmdEndDebugMarker(cmd);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
		gAppUI.Gui(pGui);
		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "12_RendererRuntimeSwitch"; }

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

DEFINE_APPLICATION_MAIN(RendererRuntimeSwitch)
