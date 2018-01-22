/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#define MAX_PLANETS 20 // Does not affect test, just for allocating space in uniform block. Must match with shader.

//tiny stl
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../Common_3/OS/Interfaces/IUIManager.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../Common_3/OS/Math/MathTypes.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

/// Camera Controller
#define GUI_CAMERACONTROLLER 1
#define FPS_CAMERACONTROLLER 2

#if !defined(TARGET_IOS)
#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER
#else
#define USE_CAMERACONTROLLER 0
#endif

ICameraController* pCameraController = nullptr;

/// UI
UIManager* pUIManager = NULL;

/// Demo structures
struct PlanetInfoStruct
{
	uint mParentIndex;
	vec4 mColor;
	float mYOrbitSpeed; // Rotation speed around parent
	float mZOrbitSpeed;
	float mRotationSpeed; // Rotation speed around self
	mat4 mTranslationMat;
	mat4 mScaleMat;
	mat4 mSharedMat;    // Matrix to pass down to children
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

FileSystem gFileSystem;
ThreadPool gThreadSystem;
LogManager gLogManager;
Timer accumTimer;

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#elif defined(_DURANGO)
#define RESOURCE_DIR "PCDX12"
#else
#error PLATFORM NOT SUPPORTED
#endif

#ifdef _DURANGO
// Durango load assets from 'Layout\Image\Loose'
const char* pszRoots[] =
{
	"Shaders/Binary/",	// FSR_BinShaders
	"Shaders/",		// FSR_SrcShaders
	"Shaders/Binary/",			// FSR_BinShaders_Common
	"Shaders/",					// FSR_SrcShaders_Common
	"Textures/",						// FSR_Textures
	"Meshes/",						// FSR_Meshes
	"Fonts/",						// FSR_Builtin_Fonts
	"",															// FSR_OtherFiles
};
#else
//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
    "../../../src/01_Transformations/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
    "../../../src/01_Transformations/" RESOURCE_DIR "/",		// FSR_SrcShaders
    "../../../src/00_Common/" RESOURCE_DIR "/Binary/",			// FSR_BinShaders_Common
    "../../../src/00_Common/" RESOURCE_DIR "/",					// FSR_SrcShaders_Common
    "../../../UnitTestResources/Textures/",						// FSR_Textures
    "../../../UnitTestResources/Meshes/",						// FSR_Meshes
    "../../../UnitTestResources/Fonts/",						// FSR_Builtin_Fonts
    "",															// FSR_OtherFiles
};
#endif

const uint32_t gImageCount = 3;

Renderer*    pRenderer = nullptr;

Queue*           pGraphicsQueue = nullptr;
CmdPool*         pCmdPool = nullptr;
Cmd**            ppCmds = nullptr;
DepthState*      pDepth = nullptr;

SwapChain*       pSwapChain = nullptr;
RenderTarget*    pDepthBuffer = nullptr;
Fence*           pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*       pImageAcquiredSemaphore = nullptr;
Semaphore*       pRenderCompleteSemaphores[gImageCount] = { nullptr };

Shader*          pSphereShader = nullptr;
Buffer*          pSphereVertexBuffer = nullptr;
Pipeline*        pSpherePipeline = nullptr;

Shader*          pSkyBoxDrawShader = nullptr;
Buffer*          pSkyBoxVertexBuffer = nullptr;
Pipeline*        pSkyBoxDrawPipeline = nullptr;
RootSignature*   pRootSignature = nullptr;
Sampler*         pSamplerSkyBox = nullptr;
Texture*         pSkyBoxTextures[6];
RasterizerState* pSkyboxRast = nullptr;

Buffer*          pProjViewUniformBuffer = nullptr;
Buffer*          pSkyboxUniformBuffer = nullptr;

uint32_t         gWindowWidth;
uint32_t         gWindowHeight;
uint32_t         gFrameIndex = 0;

const int        gSphereResolution = 30; // Increase for higher resolution spheres
const uint       gNumPlanets = 11;       // Sun, Mercury -> Neptune, Pluto, Moon
const uint       gTimeOffset = 600000;   // For visually better starting locations 
const float      gRotSelfScale = 0.0004f;
const float      gRotOrbitYScale = 0.001f;
const float      gRotOrbitZScale = 0.00001f;
const float      gPi = 3.141592654f;

int              gNumberOfSpherePoints;
UniformBlock     gUniformData;
PlanetInfoStruct gPlanetInfoData[gNumPlanets];

const char*      pSkyBoxImageFileNames[] =
{
	"Skybox_right1.png",
	"Skybox_left2.png",
	"Skybox_top3.png",
	"Skybox_bottom4.png",
	"Skybox_front5.png",
	"Skybox_back6.png"
};

#if !USE_CAMERACONTROLLER
struct CameraProperty
{
	Point3              mCameraPosition;
	vec3                mCameraRight;
	vec3                mCameraDirection;
	vec3                mCameraUp;
	vec3                mCameraForward;
	float               mCameraPitch;
	float               mCamearYaw;
} gCameraProp;
#endif
float               gCameraYRotateScale;   // decide how fast camera rotate 

WindowsDesc			gWindow = {};

/// Camera controller functionality
#if USE_CAMERACONTROLLER
#ifndef _DURANGO
bool cameraMouseMove(const MouseMoveEventData* data)
{
	pCameraController->onMouseMove(data);
	return true;
}

bool cameraMouseButton(const MouseButtonEventData* data)
{
	pCameraController->onMouseButton(data);
	return true;
}

bool cameraMouseWheel(const MouseWheelEventData* data)
{
	pCameraController->onMouseWheel(data);
	return true;
}
#endif

void CreateCameraController(const vec3& camPos, const vec3& lookAt, const CameraMotionParameters& motion)
{
#if USE_CAMERACONTROLLER == FPS_CAMERACONTROLLER
	pCameraController = createFpsCameraController(camPos, lookAt);
	requestMouseCapture(true);
#elif USE_CAMERACONTROLLER == GUI_CAMERACONTROLLER
	pCameraController = createGuiCameraController(camPos, lookAt);
#endif

	pCameraController->setMotionParameters(motion);

#ifndef _DURANGO
	registerMouseMoveEvent(cameraMouseMove);
	registerMouseButtonEvent(cameraMouseButton);
	registerMouseWheelEvent(cameraMouseWheel);
#endif
}

void RecenterCameraView(float maxDistance, vec3 lookAt=vec3(0))
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
#else
#endif

// Generates an array of vertices and normals for a sphere
void generateSpherePoints(float **ppPoints, int *pNumberOfPoints, int numberOfDivisions)
{
	float numStacks = (float)numberOfDivisions;
	float numSlices = (float)numberOfDivisions;
	float radius = 0.5f; // Diameter of 1

	tinystl::vector<vec3> vertices;
	tinystl::vector<vec3> normals;

	for (int i = 0; i < numberOfDivisions; i++)
	{
		for (int j = 0; j < numberOfDivisions; j++)
		{
			// Sectioned into quads, utilizing two triangles
			vec3 topLeftPoint = { (float)(-cos(2.0f * gPi * i / numStacks) * sin(gPi * (j + 1.0f) / numSlices)),
				(float)(-cos(gPi * (j + 1.0f) / numSlices)),
				(float)(sin(2.0f * gPi * i / numStacks) * sin(gPi * (j + 1.0f) / numSlices)) };
			vec3 topRightPoint = { (float)(-cos(2.0f * gPi * (i + 1.0) / numStacks) * sin(gPi * (j + 1.0) / numSlices)),
				(float)(-cos(gPi * (j + 1.0) / numSlices)),
				(float)(sin(2.0f * gPi * (i + 1.0) / numStacks) * sin(gPi * (j + 1.0) / numSlices)) };
			vec3 botLeftPoint = { (float)(-cos(2.0f * gPi * i / numStacks) * sin(gPi * j / numSlices)),
				(float)(-cos(gPi * j / numSlices)),
				(float)(sin(2.0f * gPi * i / numStacks) * sin(gPi * j / numSlices)) };
			vec3 botRightPoint = { (float)(-cos(2.0f * gPi * (i + 1.0) / numStacks) * sin(gPi * j / numSlices)),
				(float)(-cos(gPi * j / numSlices)),
				(float)(sin(2.0f * gPi * (i + 1.0) / numStacks) * sin(gPi * j / numSlices)) };

			// Top right triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botRightPoint);
			vertices.push_back(radius * topRightPoint);
			normals.push_back(normalize(topLeftPoint));
			normals.push_back(normalize(botRightPoint));
			normals.push_back(normalize(topRightPoint));

			// Bot left triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botLeftPoint);
			vertices.push_back(radius * botRightPoint);
			normals.push_back(normalize(topLeftPoint));
			normals.push_back(normalize(botLeftPoint));
			normals.push_back(normalize(botRightPoint));
		}
	}

	*pNumberOfPoints = vertices.getCount() * 3 * 2;
	(*ppPoints) = (float *)conf_malloc(sizeof(float) * (*pNumberOfPoints));

	for (uint32_t i = 0; i < vertices.getCount(); i++)
	{
		vec3 vertex = vertices[i];
		vec3 normal = normals[i];
		(*ppPoints)[i * 6 + 0] = vertex.getX();
		(*ppPoints)[i * 6 + 1] = vertex.getY();
		(*ppPoints)[i * 6 + 2] = vertex.getZ();
		(*ppPoints)[i * 6 + 3] = normal.getX();
		(*ppPoints)[i * 6 + 4] = normal.getY();
		(*ppPoints)[i * 6 + 5] = normal.getZ();
	}
}

void addSwapChain()
{
	SwapChainDesc swapChainDesc = {};
	swapChainDesc.pWindow = &gWindow;
	swapChainDesc.pQueue = pGraphicsQueue;
	swapChainDesc.mWidth = gWindowWidth;
	swapChainDesc.mHeight = gWindowHeight;
	swapChainDesc.mImageCount = gImageCount;
	swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
	swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
	swapChainDesc.mEnableVsync = false;
	addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
}

void addDepthBuffer()
{
	// Add depth buffer
	RenderTargetDesc depthRT = {};
	depthRT.mArraySize = 1;
	depthRT.mClearValue = { 1.0f, 0 };
	depthRT.mDepth = 1;
	depthRT.mFormat = ImageFormat::D32F;
	depthRT.mHeight = gWindowHeight;
	depthRT.mSampleCount = SAMPLE_COUNT_1;
	depthRT.mSampleQuality = 0;
	depthRT.mType = RENDER_TARGET_TYPE_2D;
	depthRT.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
	depthRT.mWidth = gWindowWidth;
	addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
}

void initApp(const WindowsDesc* window)
{
	// window and renderer setup
	int width = window->fullScreen ? getRectWidth(window->fullscreenRect) : getRectWidth(window->windowedRect);
	int height = window->fullScreen ? getRectHeight(window->fullscreenRect) : getRectHeight(window->windowedRect);
	gWindowWidth = (uint32_t)(width);
	gWindowHeight = (uint32_t)(height);

	RendererDesc settings = { 0 };
#if defined(TARGET_IOS)
    settings.pViewHandle = window->handle;
#endif
	initRenderer("Transformations", &settings, &pRenderer);

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

	addSwapChain();

    initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);

	// Loads Skybox Textures
	for (int i = 0; i < 6; ++i)
	{
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.mUseMipmaps = true;
		textureDesc.pFilename = pSkyBoxImageFileNames[i];
		textureDesc.ppTexture = &pSkyBoxTextures[i];
		addResource(&textureDesc, true);
	}

	ShaderDesc skyShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc basicShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };

#if defined(DIRECT3D12) || defined(_DURANGO)
	File hlslFile = {}; hlslFile.Open("skybox.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
	String hlsl = hlslFile.ReadText();
	skyShader = { skyShader.mStages, {hlslFile.GetName(), hlsl, "VSMain" }, {hlslFile.GetName(), hlsl, "PSMain"} };

	hlslFile.Open("basic.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	basicShader = { basicShader.mStages, { hlslFile.GetName(), hlsl, "VSMain" }, { hlslFile.GetName(), hlsl, "PSMain" } };
	hlslFile.Close();
#elif defined(VULKAN)
	File vertFile = {};
	File fragFile = {};

	vertFile.Open("skybox.vert.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	skyShader.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	fragFile.Open("skybox.frag.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	skyShader.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	vertFile.Open("basic.vert.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	basicShader.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	fragFile.Open("basic.frag.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	basicShader.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	vertFile.Close();
	fragFile.Close();
#elif defined(METAL)
    File metalFile = {}; metalFile.Open("skybox.metal", FM_Read, FSRoot::FSR_SrcShaders);
    String metal = metalFile.ReadText();
    skyShader = { skyShader.mStages, {metalFile.GetName(), metal, "VSMain" }, {metalFile.GetName(), metal, "PSMain"} };
    
    metalFile.Open("basic.metal", FM_Read, FSRoot::FSR_SrcShaders);
    metal = metalFile.ReadText();
    basicShader = { basicShader.mStages, { metalFile.GetName(), metal, "VSMain" }, { metalFile.GetName(), metal, "PSMain" } };
    metalFile.Close();
#endif

	addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
	addShader(pRenderer, &basicShader, &pSphereShader);

	addSampler(pRenderer, &pSamplerSkyBox, FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST,
		ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE);

	RootSignatureDesc rootDesc = {};
	rootDesc.mStaticSamplers["uSampler0"] = pSamplerSkyBox;
	Shader* shaders[] = { pSphereShader, pSkyBoxDrawShader };
	addRootSignature(pRenderer, 2, shaders, &pRootSignature, &rootDesc);

	addRasterizerState(&pSkyboxRast, CULL_MODE_NONE);
	addDepthState(pRenderer, &pDepth, true, true);

	addDepthBuffer();

	// Generate sphere vertex buffer
	float* pSpherePoints;
	generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution);

	uint64_t sphereDataSize = gNumberOfSpherePoints * sizeof(float);
	BufferLoadDesc sphereVbDesc = {};
	sphereVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	sphereVbDesc.mDesc.mSize = sphereDataSize;
	sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
	sphereVbDesc.pData = pSpherePoints;
	sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
	addResource(&sphereVbDesc);

	// Need to free memory;
	conf_free(pSpherePoints);

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
	pipelineSettings.pDepthStencil = pDepthBuffer;
	pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	pipelineSettings.pRootSignature = pRootSignature;
	pipelineSettings.pShaderProgram = pSphereShader;
	pipelineSettings.pVertexLayout = &vertexLayout;
	pipelineSettings.pRasterizerState = pSkyboxRast;
	addPipeline(pRenderer, &pipelineSettings, &pSpherePipeline);

	//Generate sky box vertex buffer
	float skyBoxPoints[] = {
		10.0f,  -10.0f, -10.0f,6.0f, // -z
		-10.0f, -10.0f, -10.0f,6.0f,
		-10.0f, 10.0f, -10.0f,6.0f,
		-10.0f, 10.0f, -10.0f,6.0f,
		10.0f,  10.0f, -10.0f,6.0f,
		10.0f,  -10.0f, -10.0f,6.0f,

		-10.0f, -10.0f,  10.0f,2.0f,  //-x
		-10.0f, -10.0f, -10.0f,2.0f,
		-10.0f,  10.0f, -10.0f,2.0f,
		-10.0f,  10.0f, -10.0f,2.0f,
		-10.0f,  10.0f,  10.0f,2.0f,
		-10.0f, -10.0f,  10.0f,2.0f,

		10.0f, -10.0f, -10.0f,1.0f, //+x
		10.0f, -10.0f,  10.0f,1.0f,
		10.0f,  10.0f,  10.0f,1.0f,
		10.0f,  10.0f,  10.0f,1.0f,
		10.0f,  10.0f, -10.0f,1.0f,
		10.0f, -10.0f, -10.0f,1.0f,

		-10.0f, -10.0f,  10.0f,5.0f,  // +z
		-10.0f,  10.0f,  10.0f,5.0f,
		10.0f,  10.0f,  10.0f,5.0f,
		10.0f,  10.0f,  10.0f,5.0f,
		10.0f, -10.0f,  10.0f,5.0f,
		-10.0f, -10.0f,  10.0f,5.0f,

		-10.0f,  10.0f, -10.0f, 3.0f,  //+y
		10.0f,  10.0f, -10.0f,3.0f,
		10.0f,  10.0f,  10.0f,3.0f,
		10.0f,  10.0f,  10.0f,3.0f,
		-10.0f,  10.0f,  10.0f,3.0f,
		-10.0f,  10.0f, -10.0f,3.0f,

		10.0f,  -10.0f, 10.0f, 4.0f,  //-y
		10.0f,  -10.0f, -10.0f,4.0f,
		-10.0f,  -10.0f,  -10.0f,4.0f,
		-10.0f,  -10.0f,  -10.0f,4.0f,
		-10.0f,  -10.0f,  10.0f,4.0f,
		10.0f,  -10.0f, 10.0f,4.0f,
	};

	uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
	BufferLoadDesc skyboxVbDesc = {};
	skyboxVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
	skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
	skyboxVbDesc.pData = skyBoxPoints;
	skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
	addResource(&skyboxVbDesc);

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

	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mSize = sizeof(UniformBlock);
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.pData = NULL;
	ubDesc.ppBuffer = &pProjViewUniformBuffer;
	addResource(&ubDesc);
	ubDesc.ppBuffer = &pSkyboxUniformBuffer;
	addResource(&ubDesc);

	finishResourceLoading();

	gCameraYRotateScale = 0.01f;

#if !USE_CAMERACONTROLLER
	// initial camera properties
	gCameraProp.mCameraPitch = -0.785398163f;
	gCameraProp.mCamearYaw = 1.5f*0.785398163f;
	gCameraProp.mCameraPosition = Point3(48.0f, 48.0f, 20.0f);
	gCameraProp.mCameraForward = vec3(0.0f, 0.0f, -1.0f);
	gCameraProp.mCameraUp = vec3(0.0f, 1.0f, 0.0f);

	vec3 camRot(gCameraProp.mCameraPitch, gCameraProp.mCamearYaw, 0.0f);
	mat3 trans;
	trans = mat3::rotationZYX(camRot);
	gCameraProp.mCameraDirection = trans* gCameraProp.mCameraForward;
	gCameraProp.mCameraRight = cross(gCameraProp.mCameraDirection, gCameraProp.mCameraUp);
	normalize(gCameraProp.mCameraRight);
#endif

	// setup planets (Rotation speeds are relative to Earth's, some values randomly given)

	// Sun
	gPlanetInfoData[0].mParentIndex = 0;
	gPlanetInfoData[0].mYOrbitSpeed = 0; // Earth years for one orbit
	gPlanetInfoData[0].mZOrbitSpeed = 0;
	gPlanetInfoData[0].mRotationSpeed = 24.0f; // Earth days for one rotation
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

	UISettings uiSettings = {};
	uiSettings.pDefaultFontName = "NeoSans-Bold.ttf";
	addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);
    
#if USE_CAMERACONTROLLER
    CameraMotionParameters cmp {160.0f, 600.0f, 200.0f};
	vec3 camPos { 48.0f, 48.0f, 20.0f };
	vec3 lookAt{ 0 };
    
    CreateCameraController(camPos, lookAt, cmp);
#endif
}

void ProcessInput(float deltaTime)
{
#if USE_CAMERACONTROLLER
#ifdef _DURANGO
	if (getJoystickButtonDown(BUTTON_A))
#else
	if (getKeyDown(KEY_F))
#endif
	{
		RecenterCameraView(170.0f);
	}

	pCameraController->update(deltaTime);
#endif
}

void update(float deltaTime)
{
    ProcessInput(deltaTime);

	unsigned int currentTime = accumTimer.GetMSec(false);

	// update camera with time 
#if USE_CAMERACONTROLLER
	mat4 viewMat = pCameraController->getViewMatrix();
#else
	mat4 rotMat = mat4::rotationY(gCameraYRotateScale*currentTime / 150.0f);
	mat4 viewMat = mat4::lookAt(gCameraProp.mCameraPosition, Point3(gCameraProp.mCameraPosition + gCameraProp.mCameraDirection), gCameraProp.mCameraUp);
	viewMat = viewMat * rotMat;
#endif
	const float aspectInverse = (float)gWindowHeight / (float)gWindowWidth;
	const float horizontal_fov = gPi / 2.0f;
	mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
	gUniformData.mProjectView = projMat * viewMat;

	// point light parameters
	gUniformData.mLightPosition = vec3(0, 0, 0);
	gUniformData.mLightColor = vec3(0.9f, 0.9f, 0.7f); // Pale Yellow

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
		gUniformData.mToWorldMat[i] = parentMat *  rotOrbitY * rotOrbitZ * trans * rotSelf * scale;
		gUniformData.mColor[i] = gPlanetInfoData[i].mColor;
	}

	BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer, &gUniformData };
	updateResource(&viewProjCbv);

	viewMat.setTranslation(vec3(0));
	gUniformData.mProjectView = projMat * viewMat;

	BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer, &gUniformData };
	updateResource(&skyboxViewProjCbv);
}

void drawFrame(float deltaTime)
{
	acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
	RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

	Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
	Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

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

	cmdBeginRender(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

	//// draw skybox
	cmdBeginDebugMarker(cmd, 0, 0, 1, "Draw skybox");
	cmdBindPipeline(cmd, pSkyBoxDrawPipeline);

	DescriptorData params[7] = {};
	params[0].pName = "uniformBlock";
	params[0].ppBuffers = &pSkyboxUniformBuffer;
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
	cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer);
	cmdDraw(cmd, 36, 0);
	cmdEndDebugMarker(cmd);

	////// draw planets
	cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Planets");
	cmdBindPipeline(cmd, pSpherePipeline);
	params[0].ppBuffers = &pProjViewUniformBuffer;
	cmdBindDescriptors(cmd, pRootSignature, 1, params);
	cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer);
	cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, gNumPlanets);
	cmdEndRender(cmd, 1, &pRenderTarget, NULL);
	cmdEndDebugMarker(cmd);

	cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
	cmdBeginRender(cmd, 1, &pRenderTarget, NULL, NULL);
	cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);
	static HiresTimer gTimer;
	cmdUIDrawFrameTime(cmd, pUIManager, { 8, 15 }, "CPU ", gTimer.GetUSec(true) / 1000.0f);
	cmdUIEndRender(cmd, pUIManager);
	cmdEndRender(cmd, 1, &pRenderTarget, NULL);
	cmdEndDebugMarker(cmd);

	barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
	cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
	endCmd(cmd);

	queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
	queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

	// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
	Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
	FenceStatus fenceStatus;
	getFenceStatus(pNextFence, &fenceStatus);
	if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		waitForFences(pGraphicsQueue, 1, &pNextFence);
}

void exitApp()
{
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex]);

#if USE_CAMERACONTROLLER
	destroyCameraController(pCameraController);
#endif

	removeUIManagerInterface(pRenderer, pUIManager);

	removeRenderTarget(pRenderer, pDepthBuffer);

	removeResource(pProjViewUniformBuffer);
	removeResource(pSkyboxUniformBuffer);
	removeResource(pSphereVertexBuffer);
	removeResource(pSkyBoxVertexBuffer);

	for (uint i = 0; i < 6; ++i)
		removeResource(pSkyBoxTextures[i]);

	removeSampler(pRenderer, pSamplerSkyBox);
	removeShader(pRenderer, pSphereShader);
	removePipeline(pRenderer, pSpherePipeline);
	removeShader(pRenderer, pSkyBoxDrawShader);
	removePipeline(pRenderer, pSkyBoxDrawPipeline);
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
	removeSwapChain(pRenderer, pSwapChain);
	removeQueue(pGraphicsQueue);
	removeRenderer(pRenderer);
}

bool load()
{
	addSwapChain();
	addDepthBuffer();
	return true;
}

void unload()
{
	removeSwapChain(pRenderer, pSwapChain);
	removeRenderTarget(pRenderer, pDepthBuffer);
}

#ifndef __APPLE__
void onWindowResize(const WindowResizeEventData* pData)
{
	waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);

	gWindowWidth = getRectWidth(pData->rect);
	gWindowHeight = getRectHeight(pData->rect);
	unload();
	load();
}

int main(int argc, char **argv)
{
	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	Timer deltaTimer;

	gWindow.windowedRect = { 0, 0, 1920, 1080 };
	gWindow.fullScreen = false;
	gWindow.maximized = false;
	openWindow(FileSystem::GetFileName(argv[0]), &gWindow);
	initApp(&gWindow);

	registerWindowResizeEvent(onWindowResize);

	while (isRunning())
	{
        float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
        // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
        if (deltaTime > 0.15f)
            deltaTime = 0.05f;
        
		handleMessages();
        update(deltaTime);
		drawFrame(deltaTime);
	}

	exitApp();
	closeWindow(&gWindow);

	return 0;
}
#else

#import "MetalKitApplication.h"

// Timer used in the update function.
Timer deltaTimer;
float retinaScale = 1.0f;

// Metal application implementation.
@implementation MetalKitApplication {}
-(nonnull instancetype) initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
                                       view:(nonnull MTKView*)view
                        retinaScalingFactor:(CGFloat)retinaScalingFactor
{
    self = [super init];
    if(self)
    {
        FileSystem::SetCurrentDir(FileSystem::GetProgramDir());
        
        retinaScale = retinaScalingFactor;
        
        RectDesc resolution;
        getRecommendedResolution(&resolution);
        
        gWindow.windowedRect = resolution;
        gWindow.fullscreenRect = resolution;
        gWindow.fullScreen = false;
        gWindow.maximized = false;
        gWindow.handle = (void*)CFBridgingRetain(view);
        
        @autoreleasepool {
            const char * appName = "01_Transformations";
            openWindow(appName, &gWindow);
            initApp(&gWindow);
        }
    }
    
    return self;
}

- (void)drawRectResized:(CGSize)size
{
    waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);
    
    gWindowWidth = size.width * retinaScale;
    gWindowHeight = size.height * retinaScale;
    unload();
    load();
}

- (void)update
{
    float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
    // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
    if (deltaTime > 0.15f)
        deltaTime = 0.05f;
    
    update(deltaTime);
    drawFrame(deltaTime);
}
@end

#endif
