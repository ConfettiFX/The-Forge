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
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"

//Renderer
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"


/// Camera Controller
#define GUI_CAMERACONTROLLER 1
#define FPS_CAMERACONTROLLER 2

#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
	#if defined(_WIN32)
	#define RESOURCE_DIR "PCVulkan"
	#elif defined(LINUX)
	#define RESOURCE_DIR "LINUXVulkan"
	#endif
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
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
	"../../../src/06_BRDF/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/06_BRDF/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"",													// FSR_BinShaders_Common
	"",													// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",				// FSR_Textures
	"../../../UnitTestResources/Meshes/",				// FSR_Meshes
	"../../../UnitTestResources/Fonts/",				// FSR_Builtin_Fonts
	"",													// FSR_OtherFiles
};
#endif

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
	float mRoughness = 0.04f;
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

const uint32_t				gImageCount = 3;

Renderer*					pRenderer = nullptr;
UIApp						gAppUI;

Queue*						pGraphicsQueue = nullptr;
CmdPool*					pCmdPool = nullptr;
Cmd**						ppCmds = nullptr;

SwapChain*					pSwapChain = nullptr;

RenderTarget*				pDepthBuffer = nullptr;
Fence*						pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*					pImageAcquiredSemaphore = nullptr;
Semaphore*					pRenderCompleteSemaphores[gImageCount] = { nullptr };

Shader*						pShaderBRDF = nullptr;
Pipeline*					pPipelineBRDF = nullptr;
RootSignature*				pRootSigBRDF = nullptr;

Buffer*						pSkyboxVertexBuffer = nullptr;
Shader*						pSkyboxShader = nullptr;
Pipeline*					pSkyboxPipeline = nullptr;
RootSignature*				pSkyboxRootSignature = nullptr;

Texture*					pSkybox = nullptr;
Texture*					pBRDFIntegrationMap = nullptr;
Texture*					pIrradianceMap = nullptr;
Texture*					pSpecularMap = nullptr;

#ifdef TARGET_IOS
Texture*					pVirtualJoystickTex = nullptr;
#endif

UniformObjData				pUniformDataMVP;

Buffer*						pBufferUniformCamera;
Buffer*                     pBufferUniformCameraSky;
UniformCamData				pUniformDataCamera;

Buffer*						pBufferUniformLights;
UniformLightData			pUniformDataLights;

Shader*						pShaderPostProc = nullptr;
Pipeline*					pPipelinePostProc = nullptr;

DepthState*					pDepth = nullptr;
RasterizerState*			pRasterstateDefault = nullptr;
Sampler*					pSamplerBilinear = nullptr;

// Vertex buffers
Buffer*						pSphereVertexBuffer = nullptr;

uint32_t					gFrameIndex = 0;


GpuProfiler*				pGpuProfiler = nullptr;


const int					gSphereResolution = 30; // Increase for higher resolution spheres
const float					gSphereDiameter = 0.5f;
int							gNumOfSpherePoints;

// How many objects in x and y direction
const int					gAmountObjectsinX = 6;
const int					gAmountObjectsinY = 6;

// PBR Texture values (these values are mirrored on the shaders).
const uint32_t gBRDFIntegrationSize = 512;
const uint32_t gSkyboxSize = 1024;
const uint32_t gSkyboxMips = 11;
const uint32_t gIrradianceSize = 32;
const uint32_t gSpecularSize = 128;
const uint32_t gSpecularMips = 5;

tinystl::vector<Buffer*>	gSphereBuffers;

ICameraController*			pCameraController = nullptr;

DebugTextDrawDesc gFrameTimeDraw = DebugTextDrawDesc(0, 0xff00ffff, 18);

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
	waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0], false);
}

// Compute PBR maps (skybox, BRDF Integration Map, Irradiance Map and Specular Map).
void computePBRMaps()
{
    // Temporary resources that will be loaded on PBR preprocessing.
    Texture* pPanoSkybox = nullptr;
    Buffer* pSkyBuffer = nullptr;
    Buffer* pIrrBuffer = nullptr;
    Buffer* pSpecBuffer = nullptr;
    
    Shader* pPanoToCubeShader = nullptr;
    RootSignature* pPanoToCubeRootSignature = nullptr;
    Pipeline* pPanoToCubePipeline = nullptr;
    Shader* pBRDFIntegrationShader = nullptr;
    RootSignature* pBRDFIntegrationRootSignature = nullptr;
    Pipeline* pBRDFIntegrationPipeline = nullptr;
    Shader* pIrradianceShader = nullptr;
    RootSignature* pIrradianceRootSignature = nullptr;
    Pipeline* pIrradiancePipeline = nullptr;
    Shader* pSpecularShader = nullptr;
    RootSignature* pSpecularRootSignature = nullptr;
    Pipeline* pSpecularPipeline = nullptr;
    Sampler* pSkyboxSampler = nullptr;
    
	SamplerDesc samplerDesc = {
		FILTER_TRILINEAR, FILTER_TRILINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
	};
    addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);
    
    // Load the skybox panorama texture.
    TextureLoadDesc panoDesc = {};
#ifndef TARGET_IOS
    panoDesc.mRoot = FSR_Textures;
#else
    panoDesc.mRoot = FSRoot::FSR_Absolute; // Resources on iOS are bundled with the application.
#endif
    panoDesc.mUseMipmaps = true;
    panoDesc.pFilename = "LA_Helipad.hdr";
    panoDesc.ppTexture = &pPanoSkybox;
    addResource(&panoDesc);
    
    // Create empty images for each PBR texture.
    Image skyboxImg, irrImg, specImg;
    unsigned char* skyboxImgBuff = skyboxImg.Create(ImageFormat::RGBA32F, gSkyboxSize, gSkyboxSize, 0, gSkyboxMips);
    unsigned char* irrImgBuff = irrImg.Create(ImageFormat::RGBA32F, gIrradianceSize, gIrradianceSize, 0, 1);
    unsigned char* specImgBuff = specImg.Create(ImageFormat::RGBA32F, gSpecularSize, gSpecularSize, 0, gSpecularMips);
    
    // Get the images buffer size.
    uint32_t skyboxSize = skyboxImg.GetMipMappedSize(0, gSkyboxMips, ImageFormat::RGBA32F);
    uint32_t irrSize = irrImg.GetMipMappedSize(0, 1, ImageFormat::RGBA32F);
    uint32_t specSize = specImg.GetMipMappedSize(0, gSpecularMips, ImageFormat::RGBA32F);
    
    // Create empty texture for BRDF integration map.
    TextureLoadDesc brdfIntegrationLoadDesc = {};
    TextureDesc brdfIntegrationDesc = {};
    brdfIntegrationDesc.mType = TEXTURE_TYPE_2D;
    brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
    brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
    brdfIntegrationDesc.mDepth = 1;
    brdfIntegrationDesc.mArraySize = 1;
    brdfIntegrationDesc.mMipLevels = 1;
    brdfIntegrationDesc.mFormat = ImageFormat::RG32F;
    brdfIntegrationDesc.mUsage = (TextureUsage)(TEXTURE_USAGE_SAMPLED_IMAGE | TEXTURE_USAGE_UNORDERED_ACCESS);
    brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
    brdfIntegrationDesc.mHostVisible = false;
    brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
    brdfIntegrationLoadDesc.ppTexture = &pBRDFIntegrationMap;
    addResource(&brdfIntegrationLoadDesc);
    
    // Add empty buffer resource for storing the skybox cubemap texture values.
    BufferLoadDesc skyboxBufferDesc = {};
    skyboxBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
    skyboxBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    skyboxBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#ifndef METAL
    skyboxBufferDesc.mDesc.mStructStride = sizeof(float) * 4;
    skyboxBufferDesc.mDesc.mElementCount = skyboxSize;
    skyboxBufferDesc.mDesc.mSize = skyboxBufferDesc.mDesc.mStructStride * skyboxBufferDesc.mDesc.mElementCount;
#else
    skyboxBufferDesc.mDesc.mStructStride = sizeof(float);
    skyboxBufferDesc.mDesc.mElementCount = skyboxSize;
    skyboxBufferDesc.mDesc.mSize = skyboxSize;
#endif
    skyboxBufferDesc.pData = nullptr;
    skyboxBufferDesc.ppBuffer = &pSkyBuffer;
    addResource(&skyboxBufferDesc);
    
    // Add empty buffer resource for storing the irradiance cubemap texture values.
    BufferLoadDesc irradianceBufferDesc = {};
    irradianceBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
    irradianceBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    irradianceBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#ifndef METAL
    irradianceBufferDesc.mDesc.mStructStride = sizeof(float) * 4;
    irradianceBufferDesc.mDesc.mElementCount = irrSize;
    irradianceBufferDesc.mDesc.mSize = irradianceBufferDesc.mDesc.mStructStride * irradianceBufferDesc.mDesc.mElementCount;
#else
    irradianceBufferDesc.mDesc.mStructStride = sizeof(float);
    irradianceBufferDesc.mDesc.mElementCount = irrSize;
    irradianceBufferDesc.mDesc.mSize = irrSize;
#endif
    irradianceBufferDesc.pData = nullptr;
    irradianceBufferDesc.ppBuffer = &pIrrBuffer;
    addResource(&irradianceBufferDesc);
    
    // Add empty buffer resource for storing the specular cubemap texture values.
    BufferLoadDesc specularBufferDesc = {};
    specularBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_UAV;
    specularBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    specularBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#ifndef METAL
    specularBufferDesc.mDesc.mStructStride = sizeof(float) * 4;
    specularBufferDesc.mDesc.mElementCount = specSize;
    specularBufferDesc.mDesc.mSize = specularBufferDesc.mDesc.mStructStride * specularBufferDesc.mDesc.mElementCount;
#else
    specularBufferDesc.mDesc.mStructStride = sizeof(float);
    specularBufferDesc.mDesc.mElementCount = specSize;
    specularBufferDesc.mDesc.mSize = specSize;
#endif
	
    specularBufferDesc.pData = nullptr;
    specularBufferDesc.ppBuffer = &pSpecBuffer;
    addResource(&specularBufferDesc);
    
    // Load pre-processing shaders.
	ShaderLoadDesc panoToCubeShaderDesc = {};
	panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc brdfIntegrationShaderDesc = {};
	brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc irradianceShaderDesc = {};
	irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", NULL, 0, FSR_SrcShaders };

	ShaderLoadDesc specularShaderDesc = {};
	specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", NULL, 0, FSR_SrcShaders };

    addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
    addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);
    addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
    addShader(pRenderer, &specularShaderDesc, &pSpecularShader);

	RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
	RootSignatureDesc brdfRootDesc = { &pBRDFIntegrationShader, 1 };
	RootSignatureDesc irradianceRootDesc = { &pIrradianceShader, 1 };
	RootSignatureDesc specularRootDesc = { &pSpecularShader, 1 };
    addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);
    addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);
    addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
    addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);
    
    ComputePipelineDesc pipelineSettings = { 0 };
    pipelineSettings.pShaderProgram = pPanoToCubeShader;
    pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
    addComputePipeline(pRenderer, &pipelineSettings, &pPanoToCubePipeline);
    pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
    pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
    addComputePipeline(pRenderer, &pipelineSettings, &pBRDFIntegrationPipeline);
    pipelineSettings.pShaderProgram = pIrradianceShader;
    pipelineSettings.pRootSignature = pIrradianceRootSignature;
    addComputePipeline(pRenderer, &pipelineSettings, &pIrradiancePipeline);
    pipelineSettings.pShaderProgram = pSpecularShader;
    pipelineSettings.pRootSignature = pSpecularRootSignature;
    addComputePipeline(pRenderer, &pipelineSettings, &pSpecularPipeline);
    
    // Since this happens on iniatilization, use the first cmd/fence pair available.
    Cmd* cmd = ppCmds[0];
    Fence* pRenderCompleteFence = pRenderCompleteFences[0];
    
    // Compute the BRDF Integration map.
    beginCmd(cmd);
    cmdBindPipeline(cmd, pBRDFIntegrationPipeline);
    DescriptorData params[3] = {};
    params[0].pName = "dstTexture";
    params[0].ppTextures = &pBRDFIntegrationMap;
    cmdBindDescriptors(cmd, pBRDFIntegrationRootSignature, 1, params);
    const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
    cmdDispatch(cmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

	TextureBarrier srvBarrier = { pBRDFIntegrationMap, RESOURCE_STATE_SHADER_RESOURCE };
	cmdResourceBarrier(cmd, 0, NULL, 1, &srvBarrier, true);
    
    // Store the panorama texture inside a cubemap.
    cmdBindPipeline(cmd, pPanoToCubePipeline);
    params[0].pName = "srcTexture";
    params[0].ppTextures = &pPanoSkybox;
    params[1].pName = "dstBuffer";
    params[1].ppBuffers = &pSkyBuffer;
    cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 2, params);
    pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
    cmdDispatch(cmd, gSkyboxSize / pThreadGroupSize[0], gSkyboxSize / pThreadGroupSize[1], 6);
    endCmd(cmd);
    queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 0, 0, 0, 0);
    waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);
    
    // Upload the cubemap skybox's CPU image contents to the GPU.
    memcpy(skyboxImgBuff, pSkyBuffer->pCpuMappedAddress, skyboxSize);
	TextureLoadDesc skyboxUpload = {};
    skyboxUpload.pImage = &skyboxImg;
    skyboxUpload.ppTexture = &pSkybox;
    addResource(&skyboxUpload);
    
    // After the skybox cubemap is on GPU memory, we can precompute the irradiance and specular PBR maps.
    beginCmd(cmd);
    cmdBindPipeline(cmd, pIrradiancePipeline);
    params[0].pName = "srcTexture";
    params[0].ppTextures = &pSkybox;
    params[1].pName = "dstBuffer";
    params[1].ppBuffers = &pIrrBuffer;
    params[2].pName = "skyboxSampler";
    params[2].ppSamplers = &pSkyboxSampler;
    cmdBindDescriptors(cmd, pIrradianceRootSignature, 3, params);
    pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
    cmdDispatch(cmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);
    cmdBindPipeline(cmd, pSpecularPipeline);
    params[0].pName = "srcTexture";
    params[0].ppTextures = &pSkybox;
    params[1].pName = "dstBuffer";
    params[1].ppBuffers = &pSpecBuffer;
    params[2].pName = "skyboxSampler";
    params[2].ppSamplers = &pSkyboxSampler;
    cmdBindDescriptors(cmd, pSpecularRootSignature, 3, params);
    pThreadGroupSize = pSpecularShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
    cmdDispatch(cmd, gSpecularSize / pThreadGroupSize[0], gSpecularSize / pThreadGroupSize[1], 6);
    endCmd(cmd);
    queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 0, 0, 0, 0);
    waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);
    
    // Upload both the irradiance and specular maps to GPU.
    memcpy(irrImgBuff, pIrrBuffer->pCpuMappedAddress, irrSize);
    memcpy(specImgBuff, pSpecBuffer->pCpuMappedAddress, specSize);
	TextureLoadDesc irrUpload = {};
    irrUpload.pImage = &irrImg;
    irrUpload.ppTexture = &pIrradianceMap;
    addResource(&irrUpload);
	TextureLoadDesc specUpload = {};
    specUpload.pImage = &specImg;
    specUpload.ppTexture = &pSpecularMap;
    addResource(&specUpload);
    
    // Remove temporary resources.
    removePipeline(pRenderer, pSpecularPipeline);
    removeRootSignature(pRenderer, pSpecularRootSignature);
    removeShader(pRenderer, pSpecularShader);
    
    removePipeline(pRenderer, pIrradiancePipeline);
    removeRootSignature(pRenderer, pIrradianceRootSignature);
    removeShader(pRenderer, pIrradianceShader);
    
    removePipeline(pRenderer, pBRDFIntegrationPipeline);
    removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
    removeShader(pRenderer, pBRDFIntegrationShader);
    
    removePipeline(pRenderer, pPanoToCubePipeline);
    removeRootSignature(pRenderer, pPanoToCubeRootSignature);
    removeShader(pRenderer, pPanoToCubeShader);
    
    removeResource(pPanoSkybox);
    removeResource(pIrrBuffer);
    removeResource(pSpecBuffer);
    removeResource(pSkyBuffer);
	
	removeSampler(pRenderer, pSkyboxSampler);

    skyboxImg.Destroy();
    irrImg.Destroy();
    specImg.Destroy();
}

class BRDF : public IApp
{
public:
	bool Init()
	{
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);

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

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, FileSystem::FixPath("TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts));
        
#ifdef TARGET_IOS
        // Add virtual joystick texture.
        TextureLoadDesc textureDesc = {};
        textureDesc.mRoot = FSRoot::FSR_Absolute;
        textureDesc.mUseMipmaps = false;
        textureDesc.pFilename = "circlepad.png";
        textureDesc.ppTexture = &pVirtualJoystickTex;
        addResource(&textureDesc, true);
#endif

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);
		computePBRMaps();

		SamplerDesc samplerDesc = {
			FILTER_BILINEAR, FILTER_BILINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
		};
		addSampler(pRenderer, &samplerDesc, &pSamplerBilinear);
 
		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0, FSR_SrcShaders };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);
        addShader(pRenderer, &skyboxShaderDesc, &pSkyboxShader);

		const char* pBRDFSamplerName = "envSampler";
		const char* pSkyboxamplerName = "skyboxSampler";
		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 1;
		brdfRootDesc.ppStaticSamplerNames = &pBRDFSamplerName;
		brdfRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pSkyboxRootSignature);

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterstateDefault);

		float* pSPherePoints;
		generateSpherePoints(&pSPherePoints, &gNumOfSpherePoints, gSphereResolution, gSphereDiameter);

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
        
        //Generate sky box vertex buffer
        float skyBoxPoints[] = {
            0.5f,  -0.5f, -0.5f,1.0f, // -z
            -0.5f, -0.5f, -0.5f,1.0f,
            -0.5f, 0.5f, -0.5f,1.0f,
            -0.5f, 0.5f, -0.5f,1.0f,
            0.5f,  0.5f, -0.5f,1.0f,
            0.5f,  -0.5f, -0.5f,1.0f,
            
            -0.5f, -0.5f,  0.5f,1.0f,  //-x
            -0.5f, -0.5f, -0.5f,1.0f,
            -0.5f,  0.5f, -0.5f,1.0f,
            -0.5f,  0.5f, -0.5f,1.0f,
            -0.5f,  0.5f,  0.5f,1.0f,
            -0.5f, -0.5f,  0.5f,1.0f,
            
            0.5f, -0.5f, -0.5f,1.0f, //+x
            0.5f, -0.5f,  0.5f,1.0f,
            0.5f,  0.5f,  0.5f,1.0f,
            0.5f,  0.5f,  0.5f,1.0f,
            0.5f,  0.5f, -0.5f,1.0f,
            0.5f, -0.5f, -0.5f,1.0f,
            
            -0.5f, -0.5f,  0.5f,1.0f,  // +z
            -0.5f,  0.5f,  0.5f,1.0f,
            0.5f,  0.5f,  0.5f,1.0f,
            0.5f,  0.5f,  0.5f,1.0f,
            0.5f, -0.5f,  0.5f,1.0f,
            -0.5f, -0.5f,  0.5f,1.0f,
            
            -0.5f,  0.5f, -0.5f, 1.0f,  //+y
            0.5f,  0.5f, -0.5f,1.0f,
            0.5f,  0.5f,  0.5f,1.0f,
            0.5f,  0.5f,  0.5f,1.0f,
            -0.5f,  0.5f,  0.5f,1.0f,
            -0.5f,  0.5f, -0.5f,1.0f,
            
            0.5f,  -0.5f, 0.5f, 1.0f,  //-y
            0.5f,  -0.5f, -0.5f,1.0f,
            -0.5f,  -0.5f,  -0.5f,1.0f,
            -0.5f,  -0.5f,  -0.5f,1.0f,
            -0.5f,  -0.5f,  0.5f,1.0f,
            0.5f,  -0.5f, 0.5f,1.0f,
        };
        
        uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
        BufferLoadDesc skyboxVbDesc = {};
        skyboxVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
        skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
        skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
        skyboxVbDesc.pData = skyBoxPoints;
        skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
        addResource(&skyboxVbDesc);

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

				gSphereBuffers.push_back(tBuffer);
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
        ubCamDesc.ppBuffer = &pBufferUniformCameraSky;
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
        float baseX = -2.5f;
        float baseY = -2.5f;
		for (int y = 0; y < gAmountObjectsinY; ++y)
		{
			for (int x = 0; x < gAmountObjectsinX; ++x)
			{
				mat4 modelmat = mat4::translation(vec3(baseX + x, baseY + y, 0.0f));
				pUniformDataMVP.mWorldMat = modelmat;
				pUniformDataMVP.mMetallic = x / (float)gAmountObjectsinX;
				pUniformDataMVP.mRoughness = y / (float)gAmountObjectsinY + 0.04f;

				BufferUpdateDesc objBuffUpdateDesc = { gSphereBuffers[(x + y * gAmountObjectsinY)], &pUniformDataMVP };
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

		// Create UI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont(FileSystem::FixPath("TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts));

#if USE_CAMERACONTROLLER
		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		vec3 camPos{ 0.0f, 0.0f, 10.0f };
		vec3 lookat{ 0 };

#if USE_CAMERACONTROLLER == FPS_CAMERACONTROLLER
		pCameraController = createFpsCameraController(camPos, lookat);
		requestMouseCapture(true);
#elif USE_CAMERACONTROLLER == GUI_CAMERACONTROLLER
		pCameraController = createGuiCameraController(camPos, lookat);
#endif

		pCameraController->setMotionParameters(camParameters);

#if !defined(_DURANGO)
		registerRawMouseMoveEvent(cameraMouseMove);
		registerMouseButtonEvent(cameraMouseButton);
		registerMouseWheelEvent(cameraMouseWheel);
#endif

#ifdef TARGET_IOS
        registerTouchEvent(cameraTouch);
        registerTouchMoveEvent(cameraTouchMove);
#endif
#endif
		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

#if USE_CAMERACONTROLLER
		destroyCameraController(pCameraController);
#endif

		removeDebugRendererInterface();

		for (uint32_t i = 0; i < gImageCount; ++i)	
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        
        removeResource(pSpecularMap);
        removeResource(pIrradianceMap);
        removeResource(pSkybox);
        removeResource(pBRDFIntegrationMap);
        
#ifdef TARGET_IOS
        removeResource(pVirtualJoystickTex);
#endif

		removeGpuProfiler(pRenderer, pGpuProfiler);

        removeResource(pBufferUniformCameraSky);
        removeResource(pBufferUniformCamera);
		removeResource(pBufferUniformLights);
        removeResource(pSkyboxVertexBuffer);
		removeResource(pSphereVertexBuffer);

		gAppUI.Exit();

		removeShader(pRenderer, pShaderBRDF);
        removeShader(pRenderer, pSkyboxShader);

		for (int i = 0; i < gAmountObjectsinY*gAmountObjectsinX; ++i)
		{
			removeResource(gSphereBuffers[i]);
		}

		removeDepthState(pDepth);
		removeRasterizerState(pRasterstateDefault);
		removeSampler(pRenderer, pSamplerBilinear);

		removeRootSignature(pRenderer, pRootSigBRDF);
        removeRootSignature(pRenderer, pSkyboxRootSignature);

		// Remove commands and command pool&
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pGraphicsQueue);

		// Remove resource loader and renderer
		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

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
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSigBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &vertexLayoutSphere;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineBRDF);

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pSkyboxRootSignature;
		pipelineSettings.pShaderProgram = pSkyboxShader;
		pipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettings, &pSkyboxPipeline);

#if defined(VULKAN)
		transitionRenderTargets();
#endif

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pSkyboxPipeline);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
#if USE_CAMERACONTROLLER
#ifndef TARGET_IOS
#ifdef _DURANGO
		if (getJoystickButtonDown(BUTTON_A))
#else
		if (getKeyDown(KEY_F))
#endif
		{
			RecenterCameraView(170.0f);
		}
#endif

		pCameraController->update(deltaTime);
#endif

		// Update camera
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		pUniformDataCamera.mProjectView = projMat * viewMat;
		pUniformDataCamera.mCamPos = pCameraController->getViewPosition();
        
        BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera, &pUniformDataCamera };
        updateResource(&camBuffUpdateDesc);
        
        viewMat.setTranslation(vec3(0));
        pUniformDataCamera.mProjectView = projMat * viewMat;
        
        BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky, &pUniformDataCamera };
        updateResource(&skyboxViewProjCbv);
	}

	void Draw()
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

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		// Transfer our render target to a render target state
		TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
        
        // Draw the skybox.
        cmdBindPipeline(cmd, pSkyboxPipeline);
        DescriptorData skyParams[2] = {};
        skyParams[0].pName = "uniformBlock";
        skyParams[0].ppBuffers = &pBufferUniformCameraSky;
        skyParams[1].pName = "skyboxTex";
        skyParams[1].ppTextures = &pSkybox;
        cmdBindDescriptors(cmd, pSkyboxRootSignature, 2, skyParams);
        cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer);
        cmdDraw(cmd, 36, 0);
        
        // Draw the spheres.
        cmdBindPipeline(cmd, pPipelineBRDF);

		// These params stays the same, we alternate our next param
		DescriptorData params[6] = {};
		params[0].pName = "cbCamera";
		params[0].ppBuffers = &pBufferUniformCamera;
		params[1].pName = "cbLights";
		params[1].ppBuffers = &pBufferUniformLights;
        params[2].pName = "brdfIntegrationMap";
        params[2].ppTextures = &pBRDFIntegrationMap;
        params[3].pName = "irradianceMap";
        params[3].ppTextures = &pIrradianceMap;
        params[4].pName = "specularMap";
        params[4].ppTextures = &pSpecularMap;

		for (int i = 0; i < gSphereBuffers.size(); ++i)
		{
			// Add the uniform buffer for every sphere
			params[5].pName = "cbObject";
			params[5].ppBuffers = &gSphereBuffers[i];

			cmdBindDescriptors(cmd, pRootSigBRDF, 6, params);
			cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer);
			cmdDrawInstanced(cmd, gNumOfSpherePoints / 6, 0, 1);
		}

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		// Draw the camera controller's virtual joysticks.
		float extSide = min(mSettings.mHeight, mSettings.mWidth) * pCameraController->getVirtualJoystickExternalRadius();
		float intSide = min(mSettings.mHeight, mSettings.mWidth) * pCameraController->getVirtualJoystickInternalRadius();

		float2 joystickSize = float2(extSide);
		vec2 leftJoystickCenter = pCameraController->getVirtualLeftJoystickCenter();
		float2 leftJoystickPos = float2(leftJoystickCenter.getX() * mSettings.mWidth, leftJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, leftJoystickPos.x, leftJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);
		vec2 rightJoystickCenter = pCameraController->getVirtualRightJoystickCenter();
		float2 rightJoystickPos = float2(rightJoystickCenter.getX() * mSettings.mWidth, rightJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, rightJoystickPos.x, rightJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);

		joystickSize = float2(intSide);
		leftJoystickCenter = pCameraController->getVirtualLeftJoystickPos();
		leftJoystickPos = float2(leftJoystickCenter.getX() * mSettings.mWidth, leftJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, leftJoystickPos.x, leftJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);
		rightJoystickCenter = pCameraController->getVirtualRightJoystickPos();
		rightJoystickPos = float2(rightJoystickCenter.getX() * mSettings.mWidth, rightJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
		drawDebugTexture(cmd, rightJoystickPos.x, rightJoystickPos.y, joystickSize.x, joystickSize.y, pVirtualJoystickTex, 1.0f, 1.0f, 1.0f);
#endif

		drawDebugText(cmd, 8, 15, String::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#ifndef METAL // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, String::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
#endif

		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL);

		// Transition our texture to present state
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

		Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);
		}
	}

	String GetName()
	{
		return "UnitTest_06_BRDF";
	}

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
		depthRT.mType = RENDER_TARGET_TYPE_2D;
		depthRT.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
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

	// Camera controller functionality
#if USE_CAMERACONTROLLER
#if !defined(_DURANGO)
	static bool cameraMouseMove(const RawMouseMoveEventData* data)
	{
		pCameraController->onMouseMove(data);
		return true;
	}

	static bool cameraMouseButton(const MouseButtonEventData* data)
	{
		pCameraController->onMouseButton(data);
		return true;
	}

	static bool cameraMouseWheel(const MouseWheelEventData* data)
	{
		pCameraController->onMouseWheel(data);
		return true;
	}
    
#ifdef TARGET_IOS
    static bool cameraTouch(const TouchEventData* data)
    {
        pCameraController->onTouch(data);
        return true;
    }
    
    static bool cameraTouchMove(const TouchEventData* data)
    {
        pCameraController->onTouchMove(data);
        return true;
    }
#endif
#endif
#endif
};

DEFINE_APPLICATION_MAIN(BRDF)
