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

#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER


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
	"../../../src/06_BRDF/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/06_BRDF/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"",													// FSR_BinShaders_Common
	"",													// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",				// FSR_Textures
	"../../../UnitTestResources/Meshes/",				// FSR_Meshes
	"../../../UnitTestResources/Fonts/",				// FSR_Builtin_Fonts
	"",													// FSR_OtherFiles
};

LogManager gLogManager;

// Have a uniform for camera data
struct UniformCamData
{
	mat4 mProjectView;
	vec3 mCamPos;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4 mWorldMat;
	float mRoughness = 0.0f;
	float mMetallic = 0.0f;
};

struct Light
{
	vec4 mPos; 
	vec4 mCol;
	float mRadius;
	float mIntensity;
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present 
	int mCurrAmountOfLights = 0;
	int pad0;
	int pad1;
	int pad2;
	Light mLights[16]; // array of lights seem to be broken so just a single light for now
};

ICameraController* pCameraController = nullptr;

const uint32_t   gImageCount = 3;

Renderer*    pRenderer = nullptr;
UIManager*       pUIManager = nullptr;

Queue*           pGraphicsQueue = nullptr;
CmdPool*         pCmdPool = nullptr;
Cmd**            ppCmds = nullptr;

SwapChain*       pSwapChain = nullptr;

RenderTarget*    pDepthBuffer = nullptr;
Fence*           pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*       pImageAcquiredSemaphore = nullptr;
Semaphore*       pRenderCompleteSemaphores[gImageCount] = { nullptr };

Shader*          pShaderBRDF = nullptr;
Pipeline*        pPipelineBRDF = nullptr;
RootSignature*   pRootSigBRDF = nullptr;


UniformObjData   pUniformDataMVP;

Buffer*          pBufferUniformCamera;
UniformCamData   pUniformDataCamera;

Buffer*          pBufferUniformLights;
UniformLightData pUniformDataLights;

Shader*          pShaderPostProc = nullptr;
Pipeline*		 pPipelinePostProc = nullptr;

DepthState*      pDepth = nullptr;
RasterizerState* pRasterstateDefault = nullptr;

// Vertex buffers
Buffer*          pSphereVertexBuffer = nullptr;

// The input variables we use for the application startup
uint16_t         gWindowWidthDesired = 1920;
uint16_t         gWindowHeightDesired = 1080;

// Actual window width and height
uint32_t         gWindowWidth;
uint32_t         gWindowHeight;

uint32_t         gFrameIndex = 0;

const int        gSphereResolution = 30; // Increase for higher resolution spheres
const float      gPi = 3.141592654f;
int			     gNumOfSpherePoints;


// How many objects in x and y direction
const int        gAmountObjectsinX = 6;
const int        gAmountObjectsinY = 6;



tinystl::vector<Buffer*>        sphereBuffers;

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

// Camera controller functionality
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
    depthRT.mFormat = ImageFormat::D32F;
	depthRT.mHeight = gWindowHeight;
	depthRT.mSampleCount = SAMPLE_COUNT_1;
	depthRT.mSampleQuality = 0;
	depthRT.mType = RENDER_TARGET_TYPE_2D;
	depthRT.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
	depthRT.mWidth = gWindowWidth;
	addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
}

void transitionRenderTargets()
{
	// Transition render targets to desired state
	const uint32_t numBarriers = gImageCount + 1;
	TextureBarrier rtBarriers[numBarriers] = {};
	for (uint32_t i = 0; i < gImageCount; ++i)
		rtBarriers[i] = { pSwapChain->ppSwapchainRenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
	rtBarriers[numBarriers - 1] = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
	beginCmd(ppCmds[0]);
	cmdResourceBarrier(ppCmds[0], 0, 0, numBarriers, rtBarriers, false);
	endCmd(ppCmds[0]);
	queueSubmit(pGraphicsQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0]);
}

void initApp(const WindowsDesc* pWindow)
{
	// If fullscreen, get fullscreen rect otherwise get windowed rect
	int width = pWindow->fullScreen ? getRectWidth(pWindow->fullscreenRect) : getRectWidth(pWindow->windowedRect);
	int height = pWindow->fullScreen ? getRectHeight(pWindow->fullscreenRect) : getRectHeight(pWindow->windowedRect);

	gWindowHeight = (uint32_t)(height);
	gWindowWidth = (uint32_t)(width);

	RendererDesc settings = { 0 };
	initRenderer("BRDF", &settings, &pRenderer);

	QueueDesc queueDesc = {};
	queueDesc.mType = CMD_POOL_DIRECT;
	addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
	// Create command pool and create a cmd buffer for each swapchain image
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

	ShaderDesc brdfRenderSceneShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };

#if defined(DIRECT3D12)
	// prepare dx12 shaders
	File hlslFile = {}; hlslFile.Open("renderSceneBRDF.hlsl", FM_Read, FSR_SrcShaders);
	String hlsl = hlslFile.ReadText();
	brdfRenderSceneShaderDesc = { brdfRenderSceneShaderDesc.mStages, {hlslFile.GetName(), hlsl, "VSMain"}, {hlslFile.GetName(), hlsl, "PSMain"} };

#elif defined(VULKAN)
	// prepare vulkan shaders
	File vertFile = {};
	File fragFile = {};

	vertFile.Open("renderSceneBRDF.vert.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	brdfRenderSceneShaderDesc.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	fragFile.Open("renderSceneBRDF.frag.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	brdfRenderSceneShaderDesc.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	vertFile.Close();
	fragFile.Close();

#elif defined(METAL)
	// prepare metal shaders
    File metalFile = {}; metalFile.Open("renderSceneBRDF.metal", FM_Read, FSRoot::FSR_SrcShaders);
    String metal = metalFile.ReadText();
    brdfRenderSceneShaderDesc = { brdfRenderSceneShaderDesc.mStages, {metalFile.GetName(), metal, "VSMain" }, {metalFile.GetName(), metal, "PSMain"} };
#endif

	addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);

	addDepthBuffer();

	Shader* shaders[] = { pShaderBRDF };
	addRootSignature(pRenderer, 1, shaders, &pRootSigBRDF);

	// Create depth state and rasterizer state
	addDepthState(pRenderer, &pDepth, true, true);
	addRasterizerState(&pRasterstateDefault, CULL_MODE_NONE);


	float* pSPherePoints;
	generateSpherePoints(&pSPherePoints, &gNumOfSpherePoints, gSphereResolution);

	uint64_t sphereDataSize = gNumOfSpherePoints * sizeof(float);
	
	BufferLoadDesc sphereVbDesc = {};
	sphereVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	sphereVbDesc.mDesc.mSize = sphereDataSize;
	sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6; // 3 for vertex, 3 for normal
	sphereVbDesc.pData = pSPherePoints;
	sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
	addResource(&sphereVbDesc);

	conf_free(pSPherePoints);

	// Create vertex layout
	VertexLayout vertexLayoutSphere = {};
	vertexLayoutSphere.mAttribCount = 2;

	vertexLayoutSphere.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutSphere.mAttribs[0].mFormat = ImageFormat::RGB32F;
	vertexLayoutSphere.mAttribs[0].mBinding = 0;
	vertexLayoutSphere.mAttribs[0].mLocation = 0;
	vertexLayoutSphere.mAttribs[0].mOffset = 0;

	vertexLayoutSphere.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
	vertexLayoutSphere.mAttribs[1].mFormat = ImageFormat::RGB32F;
	vertexLayoutSphere.mAttribs[1].mBinding = 0;
	vertexLayoutSphere.mAttribs[1].mLocation = 1;
	vertexLayoutSphere.mAttribs[1].mOffset = 3 * sizeof(float); // first attribute contains 3 floats

	GraphicsPipelineDesc pipelineSettings = { 0 };
	pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	pipelineSettings.mRenderTargetCount = 1;
	pipelineSettings.pDepthState = pDepth;
	pipelineSettings.pDepthStencil = pDepthBuffer;
	pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	pipelineSettings.pRootSignature = pRootSigBRDF;
	pipelineSettings.pShaderProgram = pShaderBRDF;
	pipelineSettings.pVertexLayout = &vertexLayoutSphere;
	pipelineSettings.pRasterizerState = pRasterstateDefault;
	addPipeline(pRenderer, &pipelineSettings, &pPipelineBRDF);

	// Create a uniform buffer per obj
	for (int y = 0; y < gAmountObjectsinY; ++y)
	{
		for (int x = 0; x < gAmountObjectsinX; ++x)
		{
			Buffer* tBuffer = nullptr;

			BufferLoadDesc buffDesc = {};
			buffDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
			buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			buffDesc.mDesc.mSize = sizeof(UniformObjData);
			buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
			buffDesc.pData = NULL;
			buffDesc.ppBuffer = &tBuffer;
			addResource(&buffDesc);

			sphereBuffers.push_back(tBuffer);
		}
	}

	// Uniform buffer for camera data
	BufferLoadDesc ubCamDesc = {};
	ubCamDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
	ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
	ubCamDesc.pData = NULL;
	ubCamDesc.ppBuffer = &pBufferUniformCamera;
	addResource(&ubCamDesc);
	
	// Uniform buffer for light data
	BufferLoadDesc ubLightsDesc = {};
	ubLightsDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
	ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; // not sure if persistent mapping is needed here
	ubLightsDesc.pData = NULL;
	ubLightsDesc.ppBuffer = &pBufferUniformLights;
	addResource(&ubLightsDesc);


	finishResourceLoading();

	// prepare resources

	// Update the uniform buffer for the objects
	for (int y = 0; y < gAmountObjectsinY; ++y)
	{
		for (int x = 0; x < gAmountObjectsinX; ++x)
		{
			mat4 modelmat = mat4::translation(vec3((float)x , (float)y, 0.0f));
			pUniformDataMVP.mWorldMat = modelmat;
			pUniformDataMVP.mMetallic = x / (float)gAmountObjectsinX + 0.001f;
			pUniformDataMVP.mRoughness = y / (float)gAmountObjectsinY + 0.001f;

			BufferUpdateDesc objBuffUpdateDesc = { sphereBuffers[(x + y * gAmountObjectsinY)], &pUniformDataMVP };
			updateResource(&objBuffUpdateDesc);

			int comb = x + y * gAmountObjectsinY;
		}
	}

	// Add light to scene
	Light light;
	light.mCol = vec4(1.0f, 1.0f, 1.0f, 0.0f);
	light.mPos = vec4(0.0f, 0.0f, 2.0f, 0.0f);
	light.mRadius = 10.0f;
	light.mIntensity = 40.0f;

	pUniformDataLights.mLights[0] = light;
	
	light.mCol = vec4(0.0f, 0.0f, 1.0f, 0.0f);
	light.mPos = vec4(6.0f, 0.0f, 0.0f, 0.0f);
	light.mRadius = 10.0f;
	light.mIntensity = 40.0f;

	pUniformDataLights.mLights[1] = light;

	// Add light to scene
	light.mCol = vec4(0.0f, 1.0f, 0.0f, 0.0f);
	light.mPos = vec4(6.0f, 6.0f, 2.0f, 0.0f);
	light.mRadius = 10.0f;
	light.mIntensity = 40.0f;

	pUniformDataLights.mLights[2] = light;

	light.mCol = vec4(1.0f, 0.0f, 0.0f, 0.0f);
	light.mPos = vec4(0.0f, 6.0f, 2.0f, 0.0f);
	light.mRadius = 10.0f;
	light.mIntensity = 40.0f;

	pUniformDataLights.mLights[3] = light;

	
	pUniformDataLights.mCurrAmountOfLights = 4;
	BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights, &pUniformDataLights };
	updateResource(&lightBuffUpdateDesc);


	sizeof(pUniformDataLights);

	// Create UI
	UISettings uiSettings = {};
	uiSettings.pDefaultFontName = "TitilliumText/TitilliumText-Bold.ttf";
	addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

#if USE_CAMERACONTROLLER
	CameraMotionParameters camParameters{ 160.0f, 600.0f, 200.0f };
	vec3 camPos{ 0.0f, 0.0f, 10.0f };
	vec3 lookat{ 0 };

	CreateCameraController(camPos, lookat, camParameters);
#endif
	
#if defined(VULKAN)
	transitionRenderTargets();
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
    
    // Update camera
    mat4 viewMat = pCameraController->getViewMatrix();
    
    const float aspectInverse = (float)gWindowHeight / (float)gWindowWidth;
    const float horizontal_fov = gPi / 2.0f;
    mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
    pUniformDataCamera.mProjectView = projMat * viewMat * mat4::identity();
    pUniformDataCamera.mCamPos = pCameraController->getViewPosition();
    BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera, &pUniformDataCamera };
    updateResource(&camBuffUpdateDesc);
}

void drawFrame(float deltaTime)
{
    // This will acquire the next swapchain image
    acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
    RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
    
    Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
    Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];
    
    LoadActionsDesc loadActions = {};
    loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
    loadActions.mClearColorValues[0] = { 0.2109f, 0.6470f, 0.8470f, 1.0f }; // Light blue cclear
    loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
    loadActions.mClearDepth = { 1.0f, 0.0f }; // Clear depth to the far plane and stencil to 0
    
    Cmd* cmd = ppCmds[gFrameIndex];
    beginCmd(cmd);
    
    // Transfer our render target to a render target state
    TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
    cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
    
    
    cmdBeginRender(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions);
    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
    cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
    
    cmdBindPipeline(cmd, pPipelineBRDF);
    
    // These params stays the same, we alternate our next param
    DescriptorData params[3] = {};
    params[0].pName = "cbCamera";
    params[0].ppBuffers = &pBufferUniformCamera;
    params[2].pName = "cbLights";
    params[2].ppBuffers = &pBufferUniformLights;
    
    for (int i = 0; i < sphereBuffers.size(); ++i)
    {
        // Add the uniform buffer for every sphere
        params[1].pName = "cbObject";
        params[1].ppBuffers = &sphereBuffers[i];
        
        cmdBindDescriptors(cmd, pRootSigBRDF, 3, params);
        cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer);
        cmdDrawInstanced(cmd, gNumOfSpherePoints / 6, 0, 1);
    }
    
    
    cmdEndRender(cmd, 1, &pRenderTarget, pDepthBuffer);
    
    // Prepare UI command buffers
    cmdBeginRender(cmd, 1, &pRenderTarget, NULL, NULL);
    cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);
    static HiresTimer gTimer;
    cmdUIDrawFrameTime(cmd, pUIManager, { 8, 15 }, "CPU ", gTimer.GetUSec(true) / 1000.0f);
    cmdUIEndRender(cmd, pUIManager);
    cmdEndRender(cmd, 1, &pRenderTarget, NULL);
    
    
    // Transition our texture to present state
    barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
    cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
    endCmd(cmd);
    
    queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
    queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
    
    
    Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
    FenceStatus fenceStatus;
    getFenceStatus(pNextFence, &fenceStatus);
    if (fenceStatus == FENCE_STATUS_INCOMPLETE)
    {
        waitForFences(pGraphicsQueue, 1, &pNextFence);
    }
}

void exitApp()
{
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex]);

#if USE_CAMERACONTROLLER
	destroyCameraController(pCameraController);
#endif

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	}
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	removeResource(pSphereVertexBuffer);

	removeUIManagerInterface(pRenderer, pUIManager);

	removeShader(pRenderer, pShaderBRDF);

	removeRenderTarget(pRenderer, pDepthBuffer);

	removeResource(pBufferUniformCamera);
	removeResource(pBufferUniformLights);

	for (int i = 0; i < gAmountObjectsinY*gAmountObjectsinX; ++i)
	{
		removeResource(sphereBuffers[i]);
	}

	removeDepthState(pDepth);
	removeRasterizerState(pRasterstateDefault);
	removePipeline(pRenderer, pPipelineBRDF);

	removeRootSignature(pRenderer, pRootSigBRDF);

	removeSwapChain(pRenderer, pSwapChain);

	// Remove commands and command pool&
	removeCmd_n(pCmdPool, gImageCount, ppCmds);
	removeCmdPool(pRenderer, pCmdPool);
	removeQueue(pGraphicsQueue);

	// Remove resource loader and renderer
	removeResourceLoaderInterface(pRenderer);
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
	removeRenderTarget(pRenderer, pDepthBuffer);
	removeSwapChain(pRenderer, pSwapChain);
}

#ifndef __APPLE__
void onWindowResize(const WindowResizeEventData* pdata)
{
	waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);

	gWindowWidth = getRectWidth(pdata->rect);
	gWindowHeight = getRectHeight(pdata->rect);
	unload();
	load();

#if defined(VULKAN)
	transitionRenderTargets();
#endif
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
		{
			deltaTime = 0.05f;
		}

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
