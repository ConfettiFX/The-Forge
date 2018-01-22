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

#define _USE_MATH_DEFINES

// Unit Test for testing Compute Shaders and Tessellation
// using Responsive Real-Time Grass Rendering for General 3D Scenes

//tiny stl
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../Common_3/OS/Math/Noise.h"
#include "../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../Common_3/OS/Interfaces/IUIManager.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

/// Camera Controller
#define GUI_CAMERACONTROLLER 1
#define FPS_CAMERACONTROLLER 2

#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER



ICameraController* pCameraController = nullptr;



// Grass Parameters
static unsigned int NUM_BLADES = 1 << 16;
#define MIN_HEIGHT 1.3f
#define MAX_HEIGHT 3.5f
#define MIN_WIDTH 0.1f
#define MAX_WIDTH 0.14f
#define MIN_BEND 7.0f
#define MAX_BEND 13.0f
#define PLANE_SIZE 100.0f

enum
{
	WindMode_Straight = 0,
	WindMode_Radial = 1,
};

enum
{
	FillMode_Solid = 0,
	FillMode_Wireframe = 1
};


static int gFillMode = FillMode_Solid;
static int gWindMode = WindMode_Straight;
static int gMaxTessellationLevel = 5;

static float gWindSpeed = 25.0f;
static float gWindWidth = 6.0f;
static float gWindStrength = 15.0f;

struct GrassUniformBlock
{
	mat4 mWorld;
	mat4 mView;
	mat4 mInvView;
	mat4 mProj;
	mat4 mViewProj;

	float deltaTime;
	float totalTime;
	
	int gWindMode;
	int gMaxTessellationLevel;

 	float windSpeed;
 	float windWidth;
 	float windStrength;
};

struct Blade
{
	// Position and direction
	vec4 v0;
	// Bezier point and height
	vec4 v1;
	// Physical model guide and width
	vec4 v2;
	// Up vector and stiffness coefficient
	vec4 up;
};

struct BladeDrawIndirect
{
	uint32_t vertexCount;
	uint32_t instanceCount;
	uint32_t firstVertex;
	uint32_t firstInstance;
};

tinystl::vector<Blade> blades;

FileSystem gFileSystem;
ThreadPool gThreadSystem;
LogManager gLogManager;
Timer gAccumTimer;
HiresTimer gTimer;

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#else
#error PLATFORM NOT SUPPORTED
#endif

//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
	"../../../src/07_Tessellation/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/07_Tessellation/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"../../../src/00_Common/" RESOURCE_DIR "/Binary/",			// FSR_BinShaders_Common
	"../../../src/00_Common/" RESOURCE_DIR "/",					// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",						// FSR_Textures
	"../../../UnitTestResources/Meshes/",						// FSR_Meshes
	"../../../UnitTestResources/Fonts/",						// FSR_Builtin_Fonts
	"",															// FSR_OtherFiles
};

const uint32_t	 gImageCount = 3;

Renderer*    pRenderer = nullptr;

Queue*           pGraphicsQueue = nullptr;
CmdPool*         pCmdPool = nullptr;
Cmd**            ppCmds = nullptr;

CmdPool*         pUICmdPool = nullptr;
Cmd**            ppUICmds = nullptr;

SwapChain*       pSwapChain = nullptr;
RenderTarget*    pDepthBuffer = nullptr;
Fence*           pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*       pImageAcquiredSemaphore = nullptr;
Semaphore*       pRenderCompleteSemaphores[gImageCount] = { nullptr };




Sampler*         pSampler = nullptr;
DepthState*      pDepth = nullptr;
RasterizerState* pRast = nullptr;
RasterizerState* pWireframeRast = nullptr;

RenderTarget*	 PGrassRenderTarget = nullptr;

Buffer*          pGrassUniformBuffer = nullptr;
Buffer*          pBladeStorageBuffer = nullptr;
Buffer*          pCulledBladeStorageBuffer = nullptr;

Buffer*          pBladeNumBuffer = nullptr;

CommandSignature* pIndirectCommandSignature = nullptr;

Shader*          pGrassShader = nullptr;
Pipeline*        pGrassPipeline = nullptr;

Pipeline*        pGrassPipelineForWireframe = nullptr;

RootSignature*   pGrassRootSignature = nullptr;

Shader*          pComputeShader = nullptr;
Pipeline*        pComputePipeline = nullptr;
RootSignature*   pComputeRootSignature = nullptr;

uint32_t         gWindowWidth;
uint32_t         gWindowHeight;
uint32_t         gFrameIndex = 0;

GrassUniformBlock gGrassUniformData;

const float      gPi = 3.141592654f;

// UI
Gui*	   pGuiWindow = nullptr;
UIManager* pUIManager = nullptr;

GpuProfiler* pGpuProfilerforGrass = nullptr;
GpuProfiler* pGpuProfiler = nullptr;


unsigned StartTime = 0;

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
float					gCameraYRotateScale;   // decide how fast camera rotate 

struct ObjectProperty
{
	float rotX = 0, rotY = 0;
} gObjSettings;

WindowsDesc				gWindow = {};

/// Camera controller functionality
#if USE_CAMERACONTROLLER
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

void CreateCameraController(const vec3& camPos, const vec3& lookAt, const CameraMotionParameters& motion)
{
#if USE_CAMERACONTROLLER == FPS_CAMERACONTROLLER
	pCameraController = createFpsCameraController(camPos, lookAt);
	requestMouseCapture(true);
#elif USE_CAMERACONTROLLER == GUI_CAMERACONTROLLER
	pCameraController = createGuiCameraController(camPos, lookAt);
#endif

	pCameraController->setMotionParameters(motion);

	registerMouseMoveEvent(cameraMouseMove);
	registerMouseButtonEvent(cameraMouseButton);
	registerMouseWheelEvent(cameraMouseWheel);
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
#else
void DestroyCameraController() {}
#endif

float generateRandomFloat()
{
	return rand() / (float)RAND_MAX;
}

void initBlades()
{
	blades.reserve(NUM_BLADES);

	for (unsigned int i = 0; i < NUM_BLADES; i++) {
		Blade currentBlade;// = Blade();

		vec3 bladeUp(0.0f, 1.0f, 0.0f);

		// Generate positions and direction (v0)
		float x = (generateRandomFloat() - 0.5f) * PLANE_SIZE;
		float y = 0.0f;
		float z = (generateRandomFloat() - 0.5f) * PLANE_SIZE;
		float direction = generateRandomFloat() * 2.f * 3.14159265f;
		vec3 bladePosition(x, y, z);
		currentBlade.v0 = vec4(bladePosition, direction);

		// Bezier point and height (v1)
		float height = MIN_HEIGHT + (generateRandomFloat() * (MAX_HEIGHT - MIN_HEIGHT));
		currentBlade.v1 = vec4(bladePosition + bladeUp * height, height);

		// Physical model guide and width (v2)
		float width = MIN_WIDTH + (generateRandomFloat() * (MAX_WIDTH - MIN_WIDTH));
		currentBlade.v2 = vec4(bladePosition + bladeUp * height, width);

		// Up vector and stiffness coefficient (up)
		float stiffness = MIN_BEND + (generateRandomFloat() * (MAX_BEND - MIN_BEND));
		currentBlade.up = vec4(bladeUp, stiffness);

		blades.push_back(currentBlade);
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
	swapChainDesc.mColorFormat = ImageFormat::BGRA8;
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
	depthRT.mFormat = ImageFormat::D24;
	depthRT.mHeight = gWindowHeight;
	depthRT.mSampleCount = SAMPLE_COUNT_1;
	depthRT.mSampleQuality = 0;
	depthRT.mType = RENDER_TARGET_TYPE_2D;
	depthRT.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
	depthRT.mWidth = gWindowWidth;
	addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
}

#if defined(VULKAN)
void transitionRenderTargets()
{
	TextureBarrier barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
	beginCmd(ppCmds[0]);
	cmdResourceBarrier(ppCmds[0], 0, NULL, 1, &barrier, false);
	endCmd(ppCmds[0]);
	queueSubmit(pGraphicsQueue, 1, ppCmds, pRenderCompleteFences[0], 0, NULL, 0, NULL);
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0]);
}
#endif

bool load()
{
	addSwapChain();
	addDepthBuffer();

#if defined(VULKAN)
	transitionRenderTargets();
#endif
	return true;
}

void unload()
{
	removeSwapChain(pRenderer, pSwapChain);
	removeRenderTarget(pRenderer, pDepthBuffer);
}

void initApp(const WindowsDesc* window)
{
	HiresTimer StartTime;

	initNoise();

	initBlades();

	BladeDrawIndirect indirectDraw;
	indirectDraw.vertexCount = NUM_BLADES;
	indirectDraw.instanceCount = 1;
	indirectDraw.firstVertex = 0;
	indirectDraw.firstInstance = 0;

	// window and renderer setup
	int width = window->fullScreen ? getRectWidth(window->fullscreenRect) : getRectWidth(window->windowedRect);
	int height = window->fullScreen ? getRectHeight(window->fullscreenRect) : getRectHeight(window->windowedRect);
	gWindowWidth = (uint32_t)(width);
	gWindowHeight = (uint32_t)(height);

	// renderer for swapchains
	RendererDesc settings = { 0 };
	initRenderer("Tessellation", &settings, &pRenderer);

	QueueDesc queueDesc = {};
	queueDesc.mType = CMD_POOL_DIRECT;
	addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
	addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
	addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

	addCmdPool(pRenderer, pGraphicsQueue, false, &pUICmdPool);
	addCmd_n(pUICmdPool, false, gImageCount, &ppUICmds);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		addFence(pRenderer, &pRenderCompleteFences[i]);
		addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
	}
	addSemaphore(pRenderer, &pImageAcquiredSemaphore);

	addSwapChain();

	initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET);
	addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

	ShaderDesc computeShader = { SHADER_STAGE_COMP };
	ShaderDesc grassShader = { SHADER_STAGE_VERT | SHADER_STAGE_TESC | SHADER_STAGE_TESE | SHADER_STAGE_FRAG};

#if defined(DIRECT3D12)
	File hlslFile = {};
	
	hlslFile.Open("compute.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
	computeShader.mComp = { hlslFile.GetName(), hlslFile.ReadText(), "CSMain" };

	hlslFile.Open("grass.hlsl", FM_Read, FSRoot::FSR_SrcShaders);
	String hlsl = hlslFile.ReadText();
	//grassShader = { grassShader.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMain" },{ hlslFile.GetName(), hlsl, "HSMain" },{ hlslFile.GetName(), hlsl, "DSMain" }, };

	grassShader.mVert.mName = hlslFile.GetName();
	grassShader.mVert.mCode = hlsl;
	grassShader.mVert.mEntryPoint = "VSMain";

	grassShader.mFrag.mName = hlslFile.GetName();
	grassShader.mFrag.mCode = hlsl;
	grassShader.mFrag.mEntryPoint = "PSMain";

	grassShader.mHull.mName = hlslFile.GetName();
	grassShader.mHull.mCode = hlsl;
	grassShader.mHull.mEntryPoint = "HSMain";

	grassShader.mDomain.mName = hlslFile.GetName();
	grassShader.mDomain.mCode = hlsl;
	grassShader.mDomain.mEntryPoint = "DSMain";
	
	hlslFile.Close();
#elif defined(VULKAN)

	File vertGrassFile = {};
	File fragGrassFile = {};

	vertGrassFile.Open("grass.vert.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	grassShader.mVert = { vertGrassFile.GetName(), vertGrassFile.ReadText(), "main" };
	fragGrassFile.Open("grass.frag.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	grassShader.mFrag = { fragGrassFile.GetName(), fragGrassFile.ReadText(), "main" };
	
	vertGrassFile.Close();
	fragGrassFile.Close();


	File tescGrassFile = {};
	File teseGrassFile = {};

	tescGrassFile.Open("grass.tesc.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	grassShader.mHull = { tescGrassFile.GetName(), tescGrassFile.ReadText(), "main" };
	teseGrassFile.Open("grass.tese.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	grassShader.mDomain = { teseGrassFile.GetName(), teseGrassFile.ReadText(), "main" };

	tescGrassFile.Close();
	teseGrassFile.Close();


	File computeFile = {};

	computeFile.Open("compute.comp.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	computeShader.mComp = { computeFile.GetName(), computeFile.ReadText(), "main" };

	computeFile.Close();

#elif defined(METAL)
	File metalFile = {};
	metalFile.Open("display.metal", FM_Read, FSRoot::FSR_SrcShaders);
	String metal = metalFile.ReadText();
	displayShader = { displayShader.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMain" } };

	metalFile.Open("compute.metal", FM_Read, FSRoot::FSR_SrcShaders);
	computeShader.mComp = { metalFile.GetName(), metalFile.ReadText(), "CSMain" };
	metalFile.Close();
#endif

	addShader(pRenderer, &computeShader, &pComputeShader);
	addRootSignature(pRenderer, 1, &pComputeShader, &pComputeRootSignature);

	addShader(pRenderer, &grassShader, &pGrassShader);
	addRootSignature(pRenderer, 1, &pGrassShader, &pGrassRootSignature);
	
	ComputePipelineDesc computePipelineDesc = { 0 };
	computePipelineDesc.pRootSignature = pComputeRootSignature;
	computePipelineDesc.pShaderProgram = pComputeShader;
	addComputePipeline(pRenderer, &computePipelineDesc, &pComputePipeline);
	
	addDepthState(pRenderer, &pDepth, true, true);
	addRasterizerState(&pRast, CULL_MODE_NONE, 0, 0.0f, FILL_MODE_SOLID);
	addSampler(pRenderer, &pSampler, FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST,
		ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE);

	addRasterizerState(&pWireframeRast, CULL_MODE_NONE, 0, 0.0f, FILL_MODE_WIREFRAME);
	
	addDepthBuffer();

	VertexLayout vertexLayout = {};
	vertexLayout.mAttribCount = 4;

	//v0
	vertexLayout.mAttribs[0].mSemantic = SEMANTIC_TEXCOORD0;
	vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
	vertexLayout.mAttribs[0].mBinding = 0;
	vertexLayout.mAttribs[0].mLocation = 0;
	vertexLayout.mAttribs[0].mOffset = 0;

	//v1
	vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD1;
	vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA32F;
	vertexLayout.mAttribs[1].mBinding = 0;
	vertexLayout.mAttribs[1].mLocation = 1;
	vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

	//v2
	vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD2;
	vertexLayout.mAttribs[2].mFormat = ImageFormat::RGBA32F;
	vertexLayout.mAttribs[2].mBinding = 0;
	vertexLayout.mAttribs[2].mLocation = 2;
	vertexLayout.mAttribs[2].mOffset = 8 * sizeof(float);

	//up
	vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TEXCOORD3;
	vertexLayout.mAttribs[3].mFormat = ImageFormat::RGBA32F;
	vertexLayout.mAttribs[3].mBinding = 0;
	vertexLayout.mAttribs[3].mLocation = 3;
	vertexLayout.mAttribs[3].mOffset = 12 * sizeof(float);

	GraphicsPipelineDesc pipelineSettings = { 0 };
	pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_PATCH_LIST;
	pipelineSettings.pRasterizerState = pRast;
	pipelineSettings.pDepthState = pDepth;
	pipelineSettings.mRenderTargetCount = 1;
	pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	pipelineSettings.pDepthStencil = pDepthBuffer;
	pipelineSettings.pVertexLayout = &vertexLayout;
	pipelineSettings.pRootSignature = pGrassRootSignature;
	pipelineSettings.pShaderProgram = pGrassShader;
	addPipeline(pRenderer, &pipelineSettings, &pGrassPipeline);

	pipelineSettings.pRasterizerState = pWireframeRast;
	addPipeline(pRenderer, &pipelineSettings, &pGrassPipelineForWireframe);	

	BufferLoadDesc ubGrassDesc = {};
	ubGrassDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubGrassDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubGrassDesc.mDesc.mSize = sizeof(GrassUniformBlock);
	ubGrassDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubGrassDesc.pData = NULL;
	ubGrassDesc.ppBuffer = &pGrassUniformBuffer;
	addResource(&ubGrassDesc);


	BufferLoadDesc sbBladeDesc = {};
	sbBladeDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
	sbBladeDesc.mDesc.mFirstElement = 0;
	sbBladeDesc.mDesc.mElementCount = NUM_BLADES;
	sbBladeDesc.mDesc.mVertexStride = sizeof(Blade);
	sbBladeDesc.mDesc.mStructStride = sizeof(Blade);
	sbBladeDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	sbBladeDesc.mDesc.mSize = NUM_BLADES * sizeof(Blade);
	
	sbBladeDesc.pData = blades.data();
	sbBladeDesc.ppBuffer = &pBladeStorageBuffer;
	addResource(&sbBladeDesc);
	

	BufferLoadDesc sbCulledBladeDesc = {};
	sbCulledBladeDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV | BUFFER_USAGE_VERTEX;
	sbCulledBladeDesc.mDesc.mFirstElement = 0;
	sbCulledBladeDesc.mDesc.mElementCount = NUM_BLADES;
	sbCulledBladeDesc.mDesc.mVertexStride = sizeof(Blade);
	sbCulledBladeDesc.mDesc.mStructStride = sizeof(Blade);
	sbCulledBladeDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	sbCulledBladeDesc.mDesc.mSize = NUM_BLADES * sizeof(Blade);

	sbCulledBladeDesc.pData = blades.data();
	sbCulledBladeDesc.ppBuffer = &pCulledBladeStorageBuffer;
	addResource(&sbCulledBladeDesc);
	

	BufferLoadDesc sbBladeNumDesc = {};	
	sbBladeNumDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV | BUFFER_USAGE_INDIRECT;
	sbBladeNumDesc.mDesc.mFirstElement = 0;
	sbBladeNumDesc.mDesc.mElementCount = 1;
	sbBladeNumDesc.mDesc.mStructStride = sizeof(BladeDrawIndirect);
	sbBladeNumDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	sbBladeNumDesc.mDesc.mSize = sizeof(BladeDrawIndirect);

	sbBladeNumDesc.pData = &indirectDraw;
	sbBladeNumDesc.ppBuffer = &pBladeNumBuffer;
	addResource(&sbBladeNumDesc);

	tinystl::vector<IndirectArgumentDescriptor> indirectArgDescs(1);
	indirectArgDescs[0] = {};
	indirectArgDescs[0].mType = INDIRECT_DRAW; // Indirect Index Draw Arguments
	CommandSignatureDesc cmdDesc = {};
	cmdDesc.mIndirectArgCount = (uint32_t)indirectArgDescs.size();
	cmdDesc.pArgDescs = indirectArgDescs.data();
	cmdDesc.pCmdPool = pCmdPool;
	cmdDesc.pRootSignature = pGrassRootSignature;
	addIndirectCommandSignature(pRenderer, &cmdDesc, &pIndirectCommandSignature);
		
	gGrassUniformData.totalTime = 0.0f;
	gGrassUniformData.gMaxTessellationLevel = gMaxTessellationLevel;
	gGrassUniformData.gWindMode = gWindMode;

#if !USE_CAMERACONTROLLER
	// initial camera properties
	gCameraProp.mCameraPitch = -0.785398163f;
	gCameraProp.mCamearYaw = 1.5f*0.785398163f;
	gCameraProp.mCameraPosition = Point3(48.0f, 48.0f, 20.0f);
	gCameraProp.mCameraForward = vec3(0.0f, 0.0f, -1.0f);
	gCameraProp.mCameraUp = vec3(0.0f, 1.0f, 0.0f);

	vec3 camRot(gCameraProp.mCameraPitch, gCameraProp.mCamearYaw, 0.0f);
	mat3 trans = mat3::rotationZYX(camRot);
	gCameraProp.mCameraDirection = trans* gCameraProp.mCameraForward;
	gCameraProp.mCameraRight = cross(gCameraProp.mCameraDirection, gCameraProp.mCameraUp);
	normalize(gCameraProp.mCameraRight);
#endif

	UISettings uiSettings = {};
	uiSettings.pDefaultFontName = "NeoSans-Bold.ttf";

	addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

	GuiDesc guiDesc = {};

	guiDesc.mStartSize = vec2(300.0f, 250.0f);
	guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());
	addGui(pUIManager, &guiDesc, &pGuiWindow);

	static const char* enumNames[] = {
		"SOLID",
		"WIREFRAME",
		NULL
	};
	static const int enumValues[] = {
		FillMode_Solid,
		FillMode_Wireframe,
		0
	};

	static const char* enumWindNames[] = {
		"STRAIGHT",
		"RADIAL",
		NULL
	};
	static const int enumWindValues[] = {
		WindMode_Straight,
		WindMode_Radial,
		0
	};

	UIProperty fillModeProp = UIProperty("Fill Mode : ", gFillMode, enumNames, enumValues);
	UIProperty windModeProp = UIProperty("Wind Mode : ", gWindMode, enumWindNames, enumWindValues);
		
	UIProperty windSpeedProp = UIProperty("Wind Speed : ", gWindSpeed, 1.0f, 100.0f);
	UIProperty windWidthProp = UIProperty("Wave Width : ", gWindWidth, 1.0f, 20.0f);
	UIProperty windStrProp = UIProperty("Wind Strength : ", gWindStrength, 1.0f, 100.0f);
	
	UIProperty maxLevelProp = UIProperty("Max Tessellation Level : ", gMaxTessellationLevel, 1, 10);

	addProperty(pGuiWindow, &fillModeProp);
	addProperty(pGuiWindow, &windModeProp);
	
	addProperty(pGuiWindow, &windSpeedProp);
	addProperty(pGuiWindow, &windWidthProp);
	addProperty(pGuiWindow, &windStrProp);	

	addProperty(pGuiWindow, &maxLevelProp);

#if USE_CAMERACONTROLLER
	CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
	vec3 camPos{ 48.0f, 48.0f, 20.0f };
	vec3 lookAt{ 0 };

	CreateCameraController(camPos, lookAt, cmp);
#endif

#if defined(VULKAN)
	transitionRenderTargets();
#endif

	LOGINFOF("Load time %f ms", StartTime.GetUSec(true) / 1000.0f);
}

void ProcessInput(float deltaTime)
{
#if USE_CAMERACONTROLLER
	if (getKeyDown(KEY_F))
	{
		RecenterCameraView(170.0f);
	}

	pCameraController->update(deltaTime);
#endif
}


void updateUniformBuffer(float deltaTime)
{

#if USE_CAMERACONTROLLER	
	mat4 viewMat = pCameraController->getViewMatrix();
#else	
	mat4 viewMat = mat4::lookAt(gCameraProp.mCameraPosition, Point3(gCameraProp.mCameraPosition + gCameraProp.mCameraDirection), gCameraProp.mCameraUp);
#endif
	const float aspectInverse = (float)gWindowHeight / (float)gWindowWidth;
	const float horizontal_fov = gPi / 2.0f;
	mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
	
	gGrassUniformData.deltaTime = deltaTime;
	gGrassUniformData.mProj = projMat;
	gGrassUniformData.mView = viewMat;
	gGrassUniformData.mViewProj = gGrassUniformData.mProj * gGrassUniformData.mView;
	gGrassUniformData.mInvView = inverse(viewMat);
	gGrassUniformData.mWorld = mat4::identity();

	gGrassUniformData.totalTime = (float)(getSystemTime() - StartTime) / 1000.0f;;

	gGrassUniformData.gMaxTessellationLevel = gMaxTessellationLevel;
	gGrassUniformData.gWindMode = gWindMode;

	gGrassUniformData.windSpeed = gWindSpeed;
	gGrassUniformData.windWidth = gWindWidth;
	gGrassUniformData.windStrength = gWindStrength;

	BufferUpdateDesc cbvUpdate = { pGrassUniformBuffer, &gGrassUniformData };
	updateResource(&cbvUpdate);
}

void update(float deltaTime)
{
	ProcessInput(deltaTime);
	updateUniformBuffer(deltaTime);
	updateGui(pUIManager, pGuiWindow, deltaTime);
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
	loadActions.mClearColorValues[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
	loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
	loadActions.mClearDepth = { 1.0f, 0.0f }; // Clear depth to the far plane and stencil to 0

	tinystl::vector<Cmd*> allCmds;

	Cmd* cmd = ppCmds[gFrameIndex];
	beginCmd(cmd);

	cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

	
	const uint32_t* pThreadGroupSize = pComputeShader->mNumThreadsPerGroup;

	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Compute Pass");

	DescriptorData computeParams[4] = {};
	cmdBindPipeline(cmd, pComputePipeline);


	computeParams[0].pName = "GrassUniformBlock";
	computeParams[0].ppBuffers = &pGrassUniformBuffer;

	computeParams[1].pName = "Blades";
	computeParams[1].ppBuffers = &pBladeStorageBuffer;

	computeParams[2].pName = "CulledBlades";
	computeParams[2].ppBuffers = &pCulledBladeStorageBuffer;

	computeParams[3].pName = "NumBlades";
	computeParams[3].ppBuffers = &pBladeNumBuffer;

	cmdBindDescriptors(cmd, pComputeRootSignature, 4, computeParams);
	cmdDispatch(cmd, (int)ceil(NUM_BLADES / pThreadGroupSize[0]), pThreadGroupSize[1], pThreadGroupSize[2]);
	cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	
	TextureBarrier barriers[] = {
		{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
	};
	BufferBarrier srvBarriers[] = {
		{ pBladeNumBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT },
		{ pCulledBladeStorageBuffer, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER },
	};
	cmdResourceBarrier(cmd, 2, srvBarriers, 1, barriers, false);

	cmdBeginRender(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);	

	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Pass");
	
	// Draw computed results
	if(gFillMode == FILL_MODE_SOLID)
		cmdBindPipeline(cmd, pGrassPipeline);
	else if(gFillMode == FILL_MODE_WIREFRAME)
		cmdBindPipeline(cmd, pGrassPipelineForWireframe);

	DescriptorData grassParams[1] = {};
	grassParams[0].pName = "GrassUniformBlock";
	grassParams[0].ppBuffers = &pGrassUniformBuffer;
	cmdBindDescriptors(cmd, pGrassRootSignature, 1, grassParams);

	cmdBindVertexBuffer(cmd, 1, &pCulledBladeStorageBuffer);
	cmdExecuteIndirect(cmd, pIndirectCommandSignature, 1, pBladeNumBuffer, 0, nullptr, 0);
	
	cmdEndRender(cmd, 1, &pRenderTarget, pDepthBuffer);

	BufferBarrier uavBarriers[] = {
		{ pBladeNumBuffer, RESOURCE_STATE_UNORDERED_ACCESS },
		{ pCulledBladeStorageBuffer, RESOURCE_STATE_UNORDERED_ACCESS },
	};
	cmdResourceBarrier(cmd, 2, uavBarriers, 0, NULL, true);

	cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

	cmdEndGpuFrameProfile(cmd, pGpuProfiler);

	endCmd(cmd);

	allCmds.push_back(cmd);

	// Draw UI
	cmd = ppUICmds[gFrameIndex];
	beginCmd(cmd);

	cmdBeginRender(cmd, 1, &pRenderTarget, NULL);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

	static HiresTimer timer;
	cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);

	cmdUIDrawFrameTime(cmd, pUIManager, { 8, 15 }, "CPU ", timer.GetUSec(true) / 1000.0f);
#ifndef METAL // Metal doesn't support GPU profilers
	cmdUIDrawFrameTime(cmd, pUIManager, { 8, 40 }, "GPU ", (float)pGpuProfiler->mCumulativeTime * 1000.0f);
	cmdUIDrawGUI(cmd, pUIManager, pGuiWindow);
#endif

	cmdUIDrawGpuProfileData(cmd, pUIManager, { 8, 65 }, pGpuProfiler);
	cmdUIEndRender(cmd, pUIManager);
	cmdEndRender(cmd, 1, &pRenderTarget, NULL);
	
	barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
	cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
	
	endCmd(cmd);
	allCmds.push_back(cmd);

	queueSubmit(pGraphicsQueue, allCmds.getCount(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
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

	destroyCameraController(pCameraController);

	removeResource(pBladeStorageBuffer);
	removeResource(pCulledBladeStorageBuffer);

	removeResource(pBladeNumBuffer);

	removeGui(pUIManager, pGuiWindow);
	removeUIManagerInterface(pRenderer, pUIManager);

	removeIndirectCommandSignature(pRenderer, pIndirectCommandSignature);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	}

	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	removeCmd_n(pCmdPool, gImageCount, ppCmds);
	removeCmdPool(pRenderer, pCmdPool);

	removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
	removeCmdPool(pRenderer, pUICmdPool);

	removeSampler(pRenderer, pSampler);
	removeDepthState(pDepth);

	removeRenderTarget(pRenderer, pDepthBuffer);

	removeRasterizerState(pRast);
	removeRasterizerState(pWireframeRast);
	removeResource(pGrassUniformBuffer);

	removeShader(pRenderer, pGrassShader);
	removeShader(pRenderer, pComputeShader);

	removePipeline(pRenderer, pGrassPipeline);
	removePipeline(pRenderer, pGrassPipelineForWireframe);
	removePipeline(pRenderer, pComputePipeline);

	removeRootSignature(pRenderer, pGrassRootSignature);
	removeRootSignature(pRenderer, pComputeRootSignature);
	
	removeGpuProfiler(pRenderer, pGpuProfiler);
	removeResourceLoaderInterface(pRenderer);
	removeSwapChain(pRenderer, pSwapChain);
	removeQueue(pGraphicsQueue);
	removeRenderer(pRenderer);
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

	getSystemTime();

	while (isRunning())
	{
		float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		handleMessages();
		update((float)deltaTime);
		
		drawFrame((float)deltaTime);
	}

	exitApp();
	closeWindow(&gWindow);

	return 0;
}
#endif
