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
#include "../../Common_3/Renderer/GpuProfiler.h"

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
	"../../../src/08_Procedural/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/08_Procedural/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"",														// FSR_BinShaders_Common
	"",														// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",					// FSR_Textures
	"../../../UnitTestResources/Meshes/",					// FSR_Meshes
	"../../../UnitTestResources/Fonts/",					// FSR_Builtin_Fonts
	"",														// FSR_OtherFiles
};

LogManager gLogManager;

// Have a uniform for camera data
struct UniformCamData
{
	mat4 mProjectView;
	vec3 mCamPos;
	float pad0;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4 mWorldMat;
	mat4 mInvWorldMat;

	vec4 mOceanColor;
	vec4 mShorelineColor;
	vec4 mFoliageColor;
	vec4 mMountainsColor;
	vec4 mSnowColor;
	vec4 mPolarCapsColor;
	vec4 mAtmosphereColor;
	vec4 mHeightsInfo; // x : Ocean, y : Shore, z : Snow, w : Polar
	vec4 mTimeInfo;
};

struct ScreenSize
{
	vec4 mScreenSize;	
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

Texture*         pEnvTex = nullptr;
Sampler*         pSamplerEnv = nullptr;
//Image*		 pEnvTextureImage = nullptr;

Renderer*		 pRenderer = nullptr;

Gui*			 pGuiWindow = nullptr;
UIManager*       pUIManager = nullptr;

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

Shader*          pShaderBRDF = nullptr;
Pipeline*        pPipelineBRDF = nullptr;
RootSignature*   pRootSigBRDF = nullptr;

Shader*          pShaderBG = nullptr;
Pipeline*        pPipelineBG = nullptr;
RootSignature*   pRootSigBG = nullptr;

UniformObjData   gUniformDataMVP;
ScreenSize		 gScreenSizeData;


Buffer*          pBufferUniformCamera[gImageCount];
UniformCamData   gUniformDataCamera;

Buffer*          pBufferUniformLights[gImageCount];
UniformLightData gUniformDataLights;

Shader*          pShaderPostProc = nullptr;
Pipeline*		 pPipelinePostProc = nullptr;

DepthState*      pDepth = nullptr;
RasterizerState* pRasterstateDefault = nullptr;

// Vertex buffers
Buffer*          pSphereVertexBuffer = nullptr;
Buffer*          pBGVertexBuffer = nullptr;

// Actual window width and height
uint32_t         gWindowWidth;
uint32_t         gWindowHeight;

uint32_t         gFrameIndex = 0;

const int        gSphereResolution = 1600; // Increase for higher resolution spheres
const float      gPi = 3.141592654f;

const char*      pEnvImageFileNames[] =
{
	"environment_sky.png"
};


int			 gNumOfSpherePoints;

static float eplasedTime = 0.0;

static float gSunDirX = -1.0f;
static float gSunDirY = 1.0f;
static float gSunDirZ = 1.0f;

static float gOceanHeight = 1.0f;
static float gShoreHeight = 0.02f;
static float gSnowHeight = 1.1f;
static float gPolarCapsAttitude = 1.1f;
static float gTerrainExp = 0.35f;
static float gTerrainSeed = 0.0f;

float bgVertex[256];

tinystl::vector<Buffer*> gSphereBuffers[gImageCount];
Buffer* pScreenSizeBuffer;

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
GpuProfiler*		pGpuProfiler = nullptr;

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
	float radius = 1.0f; // Diameter of 1

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

#if defined(VULKAN)
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
#endif

void initApp(const WindowsDesc* pWindow)
{
	// If fullscreen, get fullscreen rect otherwise get windowed rect
	int width = pWindow->fullScreen ? getRectWidth(pWindow->fullscreenRect) : getRectWidth(pWindow->windowedRect);
	int height = pWindow->fullScreen ? getRectHeight(pWindow->fullscreenRect) : getRectHeight(pWindow->windowedRect);

	gWindowHeight = (uint32_t)(height);
	gWindowWidth = (uint32_t)(width);

	RendererDesc settings = { 0 };
	initRenderer("08_Procedural", &settings, &pRenderer);

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

	initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, false);
	addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

	TextureLoadDesc textureDesc = {};
	textureDesc.mRoot = FSR_Textures;
	textureDesc.mUseMipmaps = true;
	textureDesc.pFilename = pEnvImageFileNames[0];
	textureDesc.ppTexture = &pEnvTex;
	addResource(&textureDesc);

	ShaderDesc brdfRenderSceneShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc bgRenderSceneShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };

#if defined(DIRECT3D12)
	// prepare dx12 shaders
	File hlslFile = {};
	hlslFile.Open("proceduralPlanet.hlsl", FM_Read, FSR_SrcShaders);
	String hlsl = hlslFile.ReadText();
	brdfRenderSceneShaderDesc = { brdfRenderSceneShaderDesc.mStages, {hlslFile.GetName(), hlsl, "VSMain"}, {hlslFile.GetName(), hlsl, "PSMain"} };
	bgRenderSceneShaderDesc = { bgRenderSceneShaderDesc.mStages,{ hlslFile.GetName(), hlsl, "VSBGMain" },{ hlslFile.GetName(), hlsl, "PSBGMain" } };

	hlslFile.Close();

#elif defined(VULKAN)
	// prepare vulkan shaders
	File vertFile = {};
	File fragFile = {};

	vertFile.Open("proceduralPlanet.vert.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);
	fragFile.Open("proceduralPlanet.frag.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);

	brdfRenderSceneShaderDesc.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };	
	brdfRenderSceneShaderDesc.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	vertFile.Close();
	fragFile.Close();


	File vertBGFile = {};
	File fragBGFile = {};

	vertBGFile.Open("backGround.vert.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);	
	fragBGFile.Open("backGround.frag.spv", FM_ReadBinary, FSRoot::FSR_BinShaders);

	bgRenderSceneShaderDesc.mVert = { vertBGFile.GetName(), vertBGFile.ReadText(), "main" };
	bgRenderSceneShaderDesc.mFrag = { fragBGFile.GetName(), fragBGFile.ReadText(), "main" };

	vertBGFile.Close();
	fragBGFile.Close();

#elif defined(METAL)
	// prepare metal shaders
    File metalFile = {}; metalFile.Open("renderSceneBRDF.metal", FM_Read, FSRoot::FSR_SrcShaders);
    String metal = metalFile.ReadText();
    brdfRenderSceneShaderDesc = { brdfRenderSceneShaderDesc.mStages, {metalFile.GetName(), metal, "VSMain" }, {metalFile.GetName(), metal, "PSMain"} };
#endif

	addSampler(pRenderer, &pSamplerEnv, FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
		ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT);
	
	addShader(pRenderer, &bgRenderSceneShaderDesc, &pShaderBG);
	addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);
	
	addRootSignature(pRenderer, 1, &pShaderBG, &pRootSigBG);
	addRootSignature(pRenderer, 1, &pShaderBRDF, &pRootSigBRDF);

	//RootSignatureDesc rootDesc = {};
	//rootDesc.mStaticSamplers["uSampler0"] = pSamplerEnv;
	
	// Create depth state and rasterizer state
	addRasterizerState(&pRasterstateDefault, CULL_MODE_FRONT);
	addDepthState(pRenderer, &pDepth, true, true);	

	addDepthBuffer();	
	
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

	uint64_t bgDataSize = 6 * sizeof(float) * 6;

	BufferLoadDesc bgVbDesc = {};
	bgVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	bgVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bgVbDesc.mDesc.mSize = bgDataSize;
	bgVbDesc.mDesc.mVertexStride = sizeof(float) * 6; // 3 for vertex, 3 for normal
	bgVbDesc.pData = bgVertex;
	bgVbDesc.ppBuffer = &pBGVertexBuffer;
	addResource(&bgVbDesc);

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



	// Create vertex layout
	VertexLayout vertexLayoutBG = {};
	vertexLayoutBG.mAttribCount = 2;

	vertexLayoutBG.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutBG.mAttribs[0].mFormat = ImageFormat::RGB32F;
	vertexLayoutBG.mAttribs[0].mBinding = 0;
	vertexLayoutBG.mAttribs[0].mLocation = 0;
	vertexLayoutBG.mAttribs[0].mOffset = 0;

	vertexLayoutBG.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
	vertexLayoutBG.mAttribs[1].mFormat = ImageFormat::RGB32F;
	vertexLayoutBG.mAttribs[1].mBinding = 0;
	vertexLayoutBG.mAttribs[1].mLocation = 1;
	vertexLayoutBG.mAttribs[1].mOffset = 3 * sizeof(float); // first attribute contains 3 floats

	GraphicsPipelineDesc pipelineSettingsBG = { 0 };
	pipelineSettingsBG.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	pipelineSettingsBG.mRenderTargetCount = 1;
	pipelineSettingsBG.pDepthState = pDepth;
	pipelineSettingsBG.pDepthStencil = pDepthBuffer;
	pipelineSettingsBG.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	pipelineSettingsBG.pRootSignature = pRootSigBG;
	pipelineSettingsBG.pShaderProgram = pShaderBG;
	pipelineSettingsBG.pVertexLayout = &vertexLayoutBG;
	pipelineSettingsBG.pRasterizerState = pRasterstateDefault;
	addPipeline(pRenderer, &pipelineSettingsBG, &pPipelineBG);
	
	// Create a screenSize uniform buffer

	Buffer* screeSizeBuffer = nullptr;

	BufferLoadDesc buffDesc = {};
	buffDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	buffDesc.mDesc.mSize = sizeof(ScreenSize);
	buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	buffDesc.pData = NULL;
	buffDesc.ppBuffer = &screeSizeBuffer;
	addResource(&buffDesc);

	pScreenSizeBuffer = screeSizeBuffer;

	// Create a uniform buffer per obj
	BufferLoadDesc buffObjDesc = {};
	buffObjDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	buffObjDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	buffObjDesc.mDesc.mSize = sizeof(UniformObjData);
	buffObjDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	buffObjDesc.pData = NULL;

	for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
	{
		Buffer* tBuffer = nullptr;
		buffObjDesc.ppBuffer = &tBuffer;
		addResource(&buffObjDesc);
		gSphereBuffers[frameIdx].push_back(tBuffer);
	}

	// Uniform buffer for camera data
	BufferLoadDesc ubCamDesc = {};
	ubCamDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
	ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT; 
	ubCamDesc.pData = NULL;
	for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
	{
		ubCamDesc.ppBuffer = &pBufferUniformCamera[frameIdx];
		addResource(&ubCamDesc);
	}
	
	// Uniform buffer for light data
	BufferLoadDesc ubLightsDesc = {};
	ubLightsDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
	ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubLightsDesc.pData = NULL;
	for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
	{
		ubLightsDesc.ppBuffer = &pBufferUniformLights[frameIdx];
		addResource(&ubLightsDesc);
	}

	finishResourceLoading();

	// prepare resources

	gUniformDataMVP.mAtmosphereColor = vec4(0.251f, 0.3451f, 0.6745f, 0.0f);
	gUniformDataMVP.mFoliageColor = vec4(0.043f, 0.560784f, 0.043f, 0.0f);
	gUniformDataMVP.mMountainsColor = vec4(0.243f, 0.13725f, 0.0118f, 0.0f);
	gUniformDataMVP.mOceanColor = vec4(0.15f, 0.596f, 0.9098f, 0.0f);
	gUniformDataMVP.mPolarCapsColor = vec4(0.60784f, 0.83921f, 0.92549f, 0.0f);
	gUniformDataMVP.mShorelineColor = vec4(0.91372f, 0.784313f, 0.56078f, 0.0f);
	gUniformDataMVP.mSnowColor = vec4(1.0f, 1.0f, 1.0f, 0.0f);

	// Update the uniform buffer for the objects
	mat4 modelmat = mat4::translation(vec3(0.0f , 0.0f, 0.0f));
	gUniformDataMVP.mWorldMat = modelmat;

	// Add light to scene
	Light light;
	light.mCol = vec4(1.0f, 1.0f, 1.0f, 0.0f);
	light.mPos = vec4( normalize(vec3(gSunDirX, gSunDirY, gSunDirZ)) * 1000.0f, 0.0f);
	light.mRadius = 10.0f;
	light.mIntensity = 40.0f;

	gUniformDataLights.mLights[0] = light;	
	
	gUniformDataLights.mCurrAmountOfLights = 1;

	// Create UI
	UISettings uiSettings = {};
	uiSettings.pDefaultFontName = "TitilliumText/TitilliumText-Bold.ttf";
	addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

	GuiDesc guiDesc = {};

	guiDesc.mStartSize = vec2(300.0f, 360.0f);
	guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());
	addGui(pUIManager, &guiDesc, &pGuiWindow);

	UIProperty sunX = UIProperty("Sunlight X", gSunDirX, -1.0f, 1.0f, 0.01f);
	UIProperty sunY = UIProperty("Sunlight Y", gSunDirY, -1.0f, 1.0f, 0.01f);
	UIProperty sunZ = UIProperty("Sunlight Z", gSunDirZ, -1.0f, 1.0f, 0.01f);

	UIProperty OceanHeight = UIProperty("Ocean Level : ", gOceanHeight, 0.0f, 1.5f, 0.01f);
	UIProperty ShoreHeight = UIProperty("Shoreline Height : ", gShoreHeight, 0.0f, 0.04f, 0.01f);
	UIProperty SnowHeight = UIProperty("Snowy Land Height : ", gSnowHeight, 0.0f, 2.00f, 0.01f);
	UIProperty PolarCapsAttitude = UIProperty("PolarCaps Attitude : ", gPolarCapsAttitude, 0.0f, 3.0f, 0.01f);
	UIProperty TerrainExp = UIProperty("Terrain Exp : ", gTerrainExp, 0.0f, 1.0f, 0.01f);
	UIProperty TerrainSeed = UIProperty("Terrain Seed : ", gTerrainSeed, 0.0f, 100.0f, 1.0f);

	addProperty(pGuiWindow, &sunX);
	addProperty(pGuiWindow, &sunY);
	addProperty(pGuiWindow, &sunZ);

	addProperty(pGuiWindow, &OceanHeight);
	addProperty(pGuiWindow, &ShoreHeight);
	addProperty(pGuiWindow, &SnowHeight);
	addProperty(pGuiWindow, &PolarCapsAttitude);
	addProperty(pGuiWindow, &TerrainExp);
	addProperty(pGuiWindow, &TerrainSeed);


#if USE_CAMERACONTROLLER
	CameraMotionParameters camParameters{ 10.0f, 600.0f, 200.0f };
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
	eplasedTime += deltaTime;

    ProcessInput(deltaTime);
    
    // Update camera
    mat4 viewMat = pCameraController->getViewMatrix();
    
    const float aspectInverse = (float)gWindowHeight / (float)gWindowWidth;
    const float horizontal_fov = gPi / 2.0f;
    mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
    gUniformDataCamera.mProjectView = projMat * viewMat * mat4::identity();
    gUniformDataCamera.mCamPos = pCameraController->getViewPosition();

	// Update screen buffer
	gScreenSizeData.mScreenSize.setX((float)(gWindowWidth));
	gScreenSizeData.mScreenSize.setY((float)(gWindowHeight));

	// Update object buffer
	float rotSpeed = 0.01f;
	gUniformDataMVP.mWorldMat = mat4::rotationY(eplasedTime *rotSpeed);
	gUniformDataMVP.mInvWorldMat = mat4::rotationY(-eplasedTime * rotSpeed);
	gUniformDataMVP.mHeightsInfo = vec4(gOceanHeight, gShoreHeight, gSnowHeight, gPolarCapsAttitude);
	gUniformDataMVP.mTimeInfo = vec4(eplasedTime * 60.0f, 0.0f, gTerrainExp, gTerrainSeed * 39.0f);

	// Update light buffer
	gUniformDataLights.mLights[0].mPos = vec4( normalize(vec3(gSunDirX, gSunDirY, gSunDirZ))* 1000.0f, 0.0);

	updateGui(pUIManager, pGuiWindow, deltaTime);
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
	/************************************************************************/
	// Upload uniform data to GPU
	/************************************************************************/
	BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera[gFrameIndex], &gUniformDataCamera };
	updateResource(&camBuffUpdateDesc);

	BufferUpdateDesc bgBuffUpdateDesc = { pScreenSizeBuffer , &gScreenSizeData };
	updateResource(&bgBuffUpdateDesc);

	BufferUpdateDesc objBuffUpdateDesc = { gSphereBuffers[gFrameIndex][0], &gUniformDataMVP };
	updateResource(&objBuffUpdateDesc);

	BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights[gFrameIndex], &gUniformDataLights };
	updateResource(&lightBuffUpdateDesc);
	/************************************************************************/
	// Record commmand buffers
	/************************************************************************/
	tinystl::vector<Cmd*> allCmds;
    Cmd* cmd = ppCmds[gFrameIndex];
    beginCmd(cmd);
	cmdBeginGpuFrameProfile(cmd, pGpuProfiler);
    
    // Transfer our render target to a render target state
    TextureBarrier barriers[] = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
	cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);
    
    cmdBeginRender(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions);
    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
    cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
    /************************************************************************/
	//Draw BG
    /************************************************************************/
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw BG");
	
	cmdBindPipeline(cmd, pPipelineBG);
	
	DescriptorData paramsBG[2] = {};
	paramsBG[0].pName = "cbObject";
	paramsBG[0].ppBuffers = &gSphereBuffers[gFrameIndex][0];
	paramsBG[1].pName = "cbScreen";
	paramsBG[1].ppBuffers = &pScreenSizeBuffer;

	cmdBindDescriptors(cmd, pRootSigBG, 2, paramsBG);
	cmdBindVertexBuffer(cmd, 1, &pBGVertexBuffer);
	
	cmdDraw(cmd, 6, 0);
	cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	/************************************************************************/
	//Draw Planet
	/************************************************************************/
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Planet");

	cmdBindPipeline(cmd, pPipelineBRDF);

    // These params stays the same, we alternate our next param
    DescriptorData params[5] = {};
    params[0].pName = "cbCamera";
    params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];
	
	params[1].pName = "cbObject";
	params[1].ppBuffers = &gSphereBuffers[gFrameIndex][0];

    params[2].pName = "cbLights";
    params[2].ppBuffers = &pBufferUniformLights[gFrameIndex];

	params[3].pName = "uEnvTex0";
	params[3].ppTextures = &pEnvTex;

	params[4].pName = "uSampler0";
	params[4].ppSamplers = &pSamplerEnv;

	cmdBindDescriptors(cmd, pRootSigBRDF, 5, params);
	cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer);

	cmdDrawInstanced(cmd, gNumOfSpherePoints / 6, 0, 1);
	cmdEndRender(cmd, 1, &pRenderTarget, pDepthBuffer);

	cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	/************************************************************************/
	/************************************************************************/
	cmdEndGpuFrameProfile(cmd, pGpuProfiler);
	endCmd(cmd);
	allCmds.push_back(cmd);

	cmd = ppUICmds[gFrameIndex];
	beginCmd(cmd);

    // Prepare UI command buffers

	cmdBeginRender(cmd, 1, &pRenderTarget, NULL);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

    cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);
    static HiresTimer gTimer;
	
    cmdUIDrawFrameTime(cmd, pUIManager, { 8, 15 }, "CPU ", gTimer.GetUSec(true) / 1000.0f);
	cmdUIDrawFrameTime(cmd, pUIManager, { 8, 40 }, "GPU ", (float)pGpuProfiler->mCumulativeTime * 1000.0f);
	cmdUIDrawGpuProfileData(cmd, pUIManager, { 8, 65 }, pGpuProfiler);

	cmdUIDrawGUI(cmd, pUIManager, pGuiWindow);

    cmdUIEndRender(cmd, pUIManager);
    cmdEndRender(cmd, 1, &pRenderTarget, NULL);
	
	// Transition our texture to present state
	barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
	cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);

	endCmd(cmd);
	allCmds.push_back(cmd);
    
    queueSubmit(pGraphicsQueue, allCmds.getCount(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
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

	removeResource(pSphereVertexBuffer);
	removeResource(pBGVertexBuffer);
	for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
	{
		removeResource(pBufferUniformCamera[frameIdx]);
		removeResource(pBufferUniformLights[frameIdx]);

		for (uint32_t i = 0; i < (uint32_t)gSphereBuffers[frameIdx].size(); ++i)
		{
			removeResource(gSphereBuffers[frameIdx][i]);
		}
	}

	removeResource(pScreenSizeBuffer);

	removeGui(pUIManager, pGuiWindow);
	removeUIManagerInterface(pRenderer, pUIManager);

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
	}
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	// Remove commands and command pool&
	removeCmd_n(pCmdPool, gImageCount, ppCmds);
	removeCmdPool(pRenderer, pCmdPool);

	removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
	(pRenderer, pUICmdPool);
	
	removeDepthState(pDepth);
	removeResource(pEnvTex);

	removeRenderTarget(pRenderer, pDepthBuffer);
	removeRasterizerState(pRasterstateDefault);

	removeSampler(pRenderer, pSamplerEnv);

	removeShader(pRenderer, pShaderBRDF);
	removeShader(pRenderer, pShaderBG);

	removePipeline(pRenderer, pPipelineBRDF);
	removePipeline(pRenderer, pPipelineBG);

	removeRootSignature(pRenderer, pRootSigBRDF);
	removeRootSignature(pRenderer, pRootSigBG);
	
	removeGpuProfiler(pRenderer, pGpuProfiler);

	// Remove resource loader and renderer
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
    
	getRecommendedResolution(&gWindow.windowedRect);
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
#endif
