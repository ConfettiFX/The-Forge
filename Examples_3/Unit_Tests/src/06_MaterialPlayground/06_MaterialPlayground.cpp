/*
 * 
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

// Unit Test for testing materials and pbr.

//asimp importer
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

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
	"",								// FSR_GpuConfig
	"",															// FSR_OtherFiles
};
#else
//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
	"../../../src/06_MaterialPlayground/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/06_MaterialPlayground/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"",													// FSR_BinShaders_Common
	"",													// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",				// FSR_Textures
	"../../../UnitTestResources/Meshes/",				// FSR_Meshes
	"../../../UnitTestResources/Fonts/",				// FSR_Builtin_Fonts
	"../../../src/06_MaterialPlayground/GPUCfg/",				// FSR_GpuConfig
	"",													// FSR_OtherFiles
};
#endif

LogManager gLogManager;


#define MAX_IN_ROW  4
#define TOTAL_SPHERE 16

#define LOAD_MATERIAL_BALL

struct Vertex {
    float3 mPos;
    float3 mNormal;
    float2 mUv;
};

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
	int pbrMaterials = -1;
};

struct Light
{
	vec4 mPos; 
	vec4 mCol;
	float mRadius;
	float mIntensity;
	float _pad0;
	float _pad1;
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present 
	Light mLights[16]; // array of lights seem to be broken so just a single light for now
	int mCurrAmountOfLights = 0;
};


const uint32_t				gImageCount = 3;

Renderer*					pRenderer = NULL;
UIApp						gAppUI;

Queue*						pGraphicsQueue = NULL;
CmdPool*					pCmdPool = NULL;
Cmd**						ppCmds = NULL;

SwapChain*					pSwapChain = NULL;

RenderTarget*				pDepthBuffer = NULL;
Fence*						pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*					pImageAcquiredSemaphore = NULL;
Semaphore*					pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*						pShaderBRDF = NULL;
Pipeline*					pPipelineBRDF = NULL;
RootSignature*				pRootSigBRDF = NULL;

Buffer*						pSkyboxVertexBuffer = NULL;
Shader*						pSkyboxShader = NULL;
Pipeline*					pSkyboxPipeline = NULL;
RootSignature*				pSkyboxRootSignature = NULL;

Texture*					pSkybox = NULL;
Texture*					pBRDFIntegrationMap = NULL;
Texture*					pIrradianceMap = NULL;
Texture*					pSpecularMap = NULL;

//added
Texture*			        pMaterialTextures[5*TOTAL_SPHERE];

#ifdef TARGET_IOS
Texture*					pVirtualJoystickTex = NULL;
#endif

UniformObjData				pUniformDataMVP;


/************************************************************************/
// Vertex buffers for the model
/************************************************************************/
Buffer*							pVertexBufferPosition = NULL;
Buffer*							pIndexBufferAll = NULL;

Buffer*						pSurfaceVertexBufferPosition = NULL;
Buffer*						pSurfaceIndexBuffer = NULL;
Buffer* 					pSurfaceBuffer = NULL;

Buffer*						pBufferUniformCamera;
Buffer*                     pBufferUniformCameraSky;
UniformCamData				pUniformDataCamera;

Buffer*						pBufferUniformLights;
UniformLightData			pUniformDataLights;

Shader*						pShaderPostProc = NULL;
Pipeline*					pPipelinePostProc = NULL;

DepthState*					pDepth = NULL;
RasterizerState*			pRasterstateDefault = NULL;
Sampler*					pSamplerBilinear = NULL;
Sampler*                    pSamplerLinear = NULL;

// Vertex buffers
Buffer*						pSphereVertexBuffer = NULL;

uint32_t					gFrameIndex = 0;


GpuProfiler*				pGpuProfiler = NULL;

BlendState*						pBlendStateOneZero = nullptr;


const int					gSphereResolution = 30; // Increase for higher resolution spheres
const float					gSphereDiameter = 0.5f;
int							gNumOfSpherePoints;

// How many objects in x and y direction
const int					gAmountObjectsinX = 4;//TOTAL_SPHERE < MAX_IN_ROW ? TOTAL_SPHERE : MAX_IN_ROW;
const int					gAmountObjectsinY = 4;//TOTAL_SPHERE <=MAX_IN_ROW ? 1 : ceil(TOTAL_SPHERE/(float)MAX_IN_ROW);

// PBR Texture values (these values are mirrored on the shaders).
const uint32_t gBRDFIntegrationSize = 512;
const uint32_t gSkyboxSize = 1024;
const uint32_t gSkyboxMips = 11;
const uint32_t gIrradianceSize = 32;
const uint32_t gSpecularSize = 128;
const uint32_t gSpecularMips = 5;

tinystl::vector<Buffer*>	gSphereBuffers;

ICameraController*			pCameraController = NULL;

DebugTextDrawDesc gFrameTimeDraw = DebugTextDrawDesc(0, 0xff00ffff, 18);

int gTotalIndices = 0;
int gSurfaceIndices =0;

const char*		pTextureName[] =
{
	
	"albedoMap",
	"normalMap",
	"metallicMap",
	"roughnessMap",
	"aoMap"
};
				
const char*		gModelName =  "matBall.obj";
const char*		gSurfaceModelName =  "cube.obj";

#define ATTACH_TEXTURES
//5 textures per pbr material
#define TOTAL_IMGS 5
const char*		pMaterialImageFileNames[] =
{
	"rusted_iron/albedo.png",
	"rusted_iron/normal.png",
	"rusted_iron/metallic.png",
	"rusted_iron/roughness.png",
	"rusted_iron/ao.png"
};

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
    Texture* pPanoSkybox = NULL;
    Buffer* pSkyBuffer = NULL;
    Buffer* pIrrBuffer = NULL;
    Buffer* pSpecBuffer = NULL;
    
    Shader* pPanoToCubeShader = NULL;
    RootSignature* pPanoToCubeRootSignature = NULL;
    Pipeline* pPanoToCubePipeline = NULL;
    Shader* pBRDFIntegrationShader = NULL;
    RootSignature* pBRDFIntegrationRootSignature = NULL;
    Pipeline* pBRDFIntegrationPipeline = NULL;
    Shader* pIrradianceShader = NULL;
    RootSignature* pIrradianceRootSignature = NULL;
    Pipeline* pIrradiancePipeline = NULL;
    Shader* pSpecularShader = NULL;
    RootSignature* pSpecularRootSignature = NULL;
    Pipeline* pSpecularPipeline = NULL;
    Sampler* pSkyboxSampler = NULL;
    
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
    skyboxBufferDesc.pData = NULL;
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
    irradianceBufferDesc.pData = NULL;
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
	
    specularBufferDesc.pData = NULL;
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
	params[2].pName = "skyboxSampler";
	params[2].ppSamplers = &pSkyboxSampler;
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


//loadModels
void loadModels()
{
	Model model;
	String sceneFullPath = FileSystem::FixPath(gModelName, FSRoot::FSR_Meshes);
#ifdef TARGET_IOS
    //TODO: need to unify this using filsystem interface
    //iOS requires path using bundle identifier
    NSString * fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:gModelName] ofType:@""];
    sceneFullPath = [fileUrl fileSystemRepresentation];
#endif
	AssimpImporter::ImportModel(sceneFullPath.c_str(),&model);
	Mesh mesh = model.mMeshArray[0];

	size_t size = mesh.mIndices.size();
    tinystl::vector<Vertex> meshVertices;
	int index = 0;
    for(int i =0 ; i < size;i++)
    {
		index = mesh.mIndices[i];
        Vertex toAdd = {mesh.mPositions[index],mesh.mNormals[index], mesh.mUvs[index]};
        meshVertices.push_back(toAdd);
    }
    
	gTotalIndices = (int)meshVertices.size();
	
	// Vertex position buffer for the scene
	BufferLoadDesc vbPosDesc = {};
	vbPosDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbPosDesc.mDesc.mVertexStride = sizeof(Vertex);
	vbPosDesc.mDesc.mSize = meshVertices.size() * vbPosDesc.mDesc.mVertexStride;
	vbPosDesc.pData = meshVertices.data();
	vbPosDesc.ppBuffer = &pVertexBufferPosition;
	vbPosDesc.mDesc.pDebugName = L"Vertex Position Buffer Desc";
	addResource(&vbPosDesc);

	Model surface;
	sceneFullPath = FileSystem::FixPath(gSurfaceModelName, FSRoot::FSR_Meshes);
#ifdef TARGET_IOS
    
    fileUrl = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:gSurfaceModelName] ofType:@""];
    sceneFullPath = [fileUrl fileSystemRepresentation];
#endif
	AssimpImporter::ImportModel(sceneFullPath.c_str(),&surface);
	Mesh surfaceMesh = surface.mMeshArray[0];

	size_t size1 = surfaceMesh.mIndices.size();
    tinystl::vector<Vertex> surfaceVertices;
	index = 0;
    for(int i =0 ; i < size1;i++)
    {

		index = surfaceMesh.mIndices[i];
        Vertex toAdd = {surfaceMesh.mPositions[index],surfaceMesh.mNormals[index],surfaceMesh.mUvs[index]};
        surfaceVertices.push_back(toAdd);
    }


	gSurfaceIndices = (int)surfaceVertices.size();

	// Vertex position buffer for the scene
	BufferLoadDesc vbPosDesc1 = {};
	vbPosDesc1.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbPosDesc1.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbPosDesc1.mDesc.mVertexStride = sizeof(Vertex);
    vbPosDesc1.mDesc.mElementCount = surfaceVertices.size();
	vbPosDesc1.mDesc.mSize = surfaceVertices.size() * vbPosDesc1.mDesc.mVertexStride;
	vbPosDesc1.pData = surfaceVertices.data();
	vbPosDesc1.ppBuffer = &pSurfaceVertexBufferPosition;
	vbPosDesc1.mDesc.pDebugName = L"Surface Vertex Position Buffer Desc";
	addResource(&vbPosDesc1);

}

// Generates an array of vertices and normals for a sphere
void createSpherePoints(Vertex **ppPoints, int *pNumberOfPoints, int numberOfDivisions, float radius = 1.0f)
{
	tinystl::vector<Vector3> vertices;
	tinystl::vector<Vector3> normals;
  tinystl::vector<Vector3> uvs;
    
	float numStacks = (float)numberOfDivisions;
	float numSlices = (float)numberOfDivisions;

	for (int i = 0; i < numberOfDivisions; i++)
	{
		for (int j = 0; j < numberOfDivisions; j++)
		{

			// Sectioned into quads, utilizing two triangles
			Vector3 topLeftPoint = { (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)),
				(float)(-cos(PI * (j + 1.0f) / numSlices)),
				(float)(sin(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)) };
			Vector3 topRightPoint = { (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)),
				(float)(-cos(PI * (j + 1.0) / numSlices)),
				(float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)) };
			Vector3 botLeftPoint = { (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)),
				(float)(-cos(PI * j / numSlices)),
				(float)(sin(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)) };
			Vector3 botRightPoint = { (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)),
				(float)(-cos(PI * j / numSlices)),
				(float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)) };

			// Top right triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botRightPoint);
			vertices.push_back(radius * topRightPoint);
			
			normals.push_back(normalize(topLeftPoint));
			float theta = atan2f(normalize(topLeftPoint).getY(),normalize(topLeftPoint).getX());
			float phi   = acosf(normalize(topLeftPoint).getZ());
            Vector3 textcoord1 = {(theta/(2*PI)),(phi/ PI),0.0f};
            uvs.push_back(textcoord1);
			
			normals.push_back(normalize(botRightPoint));
			theta = atan2f(normalize(botRightPoint).getY(),normalize(botRightPoint).getX());
			phi   = acosf(normalize(botRightPoint).getZ());
            textcoord1 = {(theta/(2*PI)),(phi/ PI),0.0f};
            uvs.push_back(textcoord1);
			
			normals.push_back(normalize(topRightPoint));
			theta = atan2f(normalize(topRightPoint).getY(),normalize(topRightPoint).getX());
			phi   = acosf(normalize(topRightPoint).getZ());
            textcoord1 = {(theta/(2*PI)),(phi/ PI),0.0f};
            uvs.push_back(textcoord1);
	

			// Bot left triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botLeftPoint);
			vertices.push_back(radius * botRightPoint);
			
			normals.push_back(normalize(topLeftPoint));
			theta = atan2f(normalize(topLeftPoint).getY(),normalize(topLeftPoint).getX());
			phi   = acosf(normalize(topLeftPoint).getZ());
            textcoord1 = {(theta/(2*PI)),(phi/ PI),0.0f};
			uvs.push_back(textcoord1);
			
			normals.push_back(normalize(botLeftPoint));
			theta = atan2f(normalize(botLeftPoint).getY(),normalize(botLeftPoint).getX());
			phi   = acosf(normalize(botLeftPoint).getZ());
            textcoord1 = {(theta/(2*PI)),(phi/ PI),0.0f};
			uvs.push_back(textcoord1);
			
			normals.push_back(normalize(botRightPoint));
			theta = atan2f(normalize(botRightPoint).getY(),normalize(botRightPoint).getX());
			phi   = acosf(normalize(botRightPoint).getZ());
            textcoord1 = {(theta/(2*PI)),(phi/ PI),0.0f};
			uvs.push_back(textcoord1);
		}
	}

	*pNumberOfPoints = (uint32_t)vertices.size();
	(*ppPoints) = (Vertex *)conf_malloc(sizeof(Vertex) * (*pNumberOfPoints));

	for (uint32_t i = 0; i < (uint32_t)vertices.size(); i++)
	{
		Vertex vertex; 
		vertex.mPos = float3(vertices[i].getX(), vertices[i].getY(), vertices[i].getZ());
		vertex.mNormal= float3(normals[i].getX(), normals[i].getY(), normals[i].getZ());
		
		/*  (*ppPoints)[i * 8 + 6] = uv.getX();
		(*ppPoints)[i * 8 + 7] = uv.getY();*/


		/*    θ = atan 2(N .Y ,N . X)
		ϕ = acos(N .Z)
		(u , v) = (θ/2π ,ϕ / π)
		*/

		float theta = atan2f(normals[i].getY(), normals[i].getX());
		float phi = acosf(normals[i].getZ());

		vertex.mUv.x = (theta / (2 * PI));
		vertex.mUv.y = (phi / PI);

		(*ppPoints)[i] = vertex;
		//uv for each vertex
		// (*ppPoints)[i * 8 + 6] = normal.getX()/2 + 0.5f;
		//(*ppPoints)[i * 8 + 7] = normal.getY()/2 + 0.5f;

	}
}

class MaterialPlayground : public IApp
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
		
		tinystl::vector<Image> toLoad(TOTAL_IMGS);
        //adding material textures
        for(int i = 0 ; i <TOTAL_IMGS ; ++i) 
        {
            TextureLoadDesc textureDesc = {};
#ifndef TARGET_IOS
            textureDesc.mRoot = FSR_Textures;
#else
            textureDesc.mRoot = FSRoot::FSR_Absolute; // Resources on iOS are bundled with the application.
#endif
			toLoad[i].loadImage(pMaterialImageFileNames[i],true);
			
#ifdef TARGET_IOS
			if(toLoad[i].getFormat() == ImageFormat::RGBA8)
				toLoad[i].Convert(ImageFormat::BGRA8);
#endif
			textureDesc.mUseMipmaps = true;
			textureDesc.pImage = &toLoad[i];
            textureDesc.ppTexture = &pMaterialTextures[i];
            addResource(&textureDesc, true);
        }
        
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
			ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT
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

		const char* pStaticSamplerNames[] = { "envSampler", "defaultSampler" };
		Sampler* pStaticSamplers[] = { pSamplerBilinear, pSamplerBilinear };
		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 2;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		const char* pSkyboxamplerName = "skyboxSampler";
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

		Vertex* pSPherePoints;
		createSpherePoints(&pSPherePoints, &gNumOfSpherePoints, gSphereResolution, gSphereDiameter);
		
#ifdef LOAD_MATERIAL_BALL 
		loadModels();

		BufferLoadDesc buffDesc = {};
		buffDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		buffDesc.mDesc.mSize = sizeof(UniformObjData);
		buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		buffDesc.pData = NULL;
		buffDesc.ppBuffer = &pSurfaceBuffer;
		addResource(&buffDesc);
#else
		uint64_t sphereDataSize = gNumOfSpherePoints * sizeof(Vertex);

		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mElementCount = gNumOfSpherePoints;
		sphereVbDesc.mDesc.mVertexStride = sizeof(Vertex); // 3 for vertex, 3 for normal, 2 for textures
		sphereVbDesc.mDesc.mSize = sphereVbDesc.mDesc.mVertexStride  * sphereVbDesc.mDesc.mElementCount;
		sphereVbDesc.pData = pSPherePoints;
		sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
		addResource(&sphereVbDesc);

		conf_free(pSPherePoints);
#endif

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
				Buffer* tBuffer = NULL;

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
        float baseX = 2.5f;
        float baseY = -4.5f;
		float baseZ = 9.0f;
		float offsetX = 0.1f;
		float offsetY = 0.0f;
		float offsetZ = 5.0f;
		float scaleVal = 1.0f;
		int y =0;
		int x = 0;
#ifdef LOAD_MATERIAL_BALL
        baseX = 8.0f;
        offsetX = 5.0f;
		offsetY = 1.5f;
		scaleVal = 1.5f;//0.2
#endif
		for (y= 0; y < gAmountObjectsinY; ++y)
		{
			for (x = 0; x < gAmountObjectsinX; ++x)
			{
				
#ifdef LOAD_MATERIAL_BALL
				mat4 modelmat = mat4::translation(vec3(baseX - x - offsetX * x, baseY, baseZ - y - offsetZ * y)) * mat4::scale(vec3(scaleVal)) * mat4::rotationY(PI);
#else
				mat4 modelmat = mat4::translation(vec3(baseX - x - offsetX * x, baseY + y + offsetY * y, 0.0f)) * mat4::scale(vec3(scaleVal));
#endif
				pUniformDataMVP.mWorldMat = modelmat;
				pUniformDataMVP.mMetallic = x / (float)gAmountObjectsinX;
				pUniformDataMVP.mRoughness = y / (float)gAmountObjectsinY + 0.04f;
				pUniformDataMVP.pbrMaterials = 1;
				int comb = x + y * gAmountObjectsinX;
				//if not enough materials specified then set pbrMaterials to -1
				if(comb>=TOTAL_IMGS/5) {
					pUniformDataMVP.pbrMaterials = -1;
				}
#if !defined(ATTACH_TEXTURES)
	pUniformDataMVP.pbrMaterials = -1;
#endif
				BufferUpdateDesc objBuffUpdateDesc = { gSphereBuffers[(x + y * gAmountObjectsinX)], &pUniformDataMVP };
				updateResource(&objBuffUpdateDesc);
				
			}
		}
		
#ifdef LOAD_MATERIAL_BALL
		mat4 modelmat = mat4::translation(vec3(0.0f, -6.0f, 0.0f)) * mat4::scale(vec3(50.0f, 0.2f, 40.0f));
		pUniformDataMVP.mWorldMat = modelmat;
		pUniformDataMVP.mMetallic = 0;
		pUniformDataMVP.mRoughness =  0.04f;
		pUniformDataMVP.pbrMaterials = 2;
		BufferUpdateDesc objBuffUpdateDesc = { pSurfaceBuffer, &pUniformDataMVP };
		updateResource(&objBuffUpdateDesc);
#endif
		
		// Add light to scene
		Light light;
		light.mCol = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		light.mPos = vec4(0.0f, 0.0f, 2.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		pUniformDataLights.mLights[0] = light;

		light.mCol = vec4(0.0f, 0.0f, 1.0f, 0.0f);
		light.mPos = vec4(6.0f, 0.0f, 0.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		pUniformDataLights.mLights[1] = light;

		// Add light to scene
		light.mCol = vec4(0.0f, 1.0f, 0.0f, 0.0f);
		light.mPos = vec4(6.0f, 6.0f, 2.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		pUniformDataLights.mLights[2] = light;

		light.mCol = vec4(1.0f, 0.0f, 0.0f, 0.0f);
		light.mPos = vec4(0.0f, 6.0f, 2.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		pUniformDataLights.mLights[3] = light;

		pUniformDataLights.mCurrAmountOfLights = 4;
		BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights, &pUniformDataLights };
		updateResource(&lightBuffUpdateDesc);

		// Create UI
		if (!gAppUI.Init(pRenderer))
	
		gAppUI.LoadFont(FileSystem::FixPath("TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts));

#if USE_CAMERACONTROLLER
		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		
#ifdef LOAD_MATERIAL_BALL
		vec3 camPos{ 0.0f, 0.0f, 25.0f };
#else
		vec3 camPos{ 0.0f, 0.0f, 10.0f };
#endif
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
        

        //sphere buffers
#ifdef LOAD_MATERIAL_BALL
        removeResource(pVertexBufferPosition);
		
		removeResource(pSurfaceBuffer);
		removeResource(pSurfaceVertexBufferPosition);
		
#else
		removeResource(pSphereVertexBuffer);

#endif
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
		
		
		for (uint i = 0; i < TOTAL_IMGS; ++i)
            removeResource(pMaterialTextures[i]);

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
		vertexLayoutSphere.mAttribCount = 3;

		vertexLayoutSphere.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSphere.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[0].mBinding = 0;
		vertexLayoutSphere.mAttribs[0].mLocation = 0;
		vertexLayoutSphere.mAttribs[0].mOffset = 0;

		//normals
		vertexLayoutSphere.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutSphere.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[1].mLocation = 1;

		vertexLayoutSphere.mAttribs[1].mBinding = 0;
		vertexLayoutSphere.mAttribs[1].mOffset = 3 * sizeof(float);

    //texture
    vertexLayoutSphere.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
    vertexLayoutSphere.mAttribs[2].mFormat = ImageFormat::RG32F;
    vertexLayoutSphere.mAttribs[2].mLocation = 2;
    vertexLayoutSphere.mAttribs[2].mBinding = 0;
    vertexLayoutSphere.mAttribs[2].mOffset = 6 * sizeof(float); // first attribute contains 3 floats

		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		
		//pipelineSettings.pBlendState = pBlendStateOneZero;
		
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
		DescriptorData params[13] = {};
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
        
#ifdef METAL
        //bind samplers for metal
        params[11].pName = "defaultSampler";
        params[11].ppSamplers = &pSamplerBilinear;
        params[12].pName = "envSampler";
        params[12].ppSamplers = &pSamplerBilinear;
#endif
        //since there are 5 texturesmaps 
        int totalMaterials = TOTAL_IMGS/5;
		
#ifdef LOAD_MATERIAL_BALL
        Buffer* pVertexBuffers[] = { pVertexBufferPosition };
        cmdBindVertexBuffer(cmd, 1, pVertexBuffers);
#endif

        for (int i = 0; i < gSphereBuffers.size(); ++i)
        {
            // Add the uniform buffer for every sphere
            params[5].pName = "cbObject";
            params[5].ppBuffers = &gSphereBuffers[i];

            //binding pbr material textures
            for(int j = 0 ; j <5 && i<totalMaterials; ++j) {

                int index = j+5*i;
                //added
                params[6+j].pName = pTextureName[j];
                params[6+j].ppTextures = &pMaterialTextures[index];
            }
            
//13 entries on apple becuase we need to bind samplers (2 extra)
#ifdef METAL
            //draw sphere
            cmdBindDescriptors(cmd, pRootSigBRDF, 13, params);
#else
            cmdBindDescriptors(cmd, pRootSigBRDF, 11, params);
#endif
            
#ifdef LOAD_MATERIAL_BALL

			cmdDraw(cmd, gTotalIndices, 0);
#else
            cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer);
            cmdDrawInstanced(cmd, gNumOfSpherePoints, 0, 1);
#endif
        }
	
#ifdef LOAD_MATERIAL_BALL
		Buffer* pSurfaceVertexBuffers[] = { pSurfaceVertexBufferPosition};
		cmdBindVertexBuffer(cmd, 1, pSurfaceVertexBuffers);

		params[5].pName = "cbObject";
		params[5].ppBuffers = &pSurfaceBuffer;
        
//13 entries on apple becuase we need to bind samplers (2 extra)
#ifdef METAL
        cmdBindDescriptors(cmd, pRootSigBRDF, 13, params);
#else
        cmdBindDescriptors(cmd, pRootSigBRDF, 11, params);
#endif
       
		cmdDraw(cmd, gSurfaceIndices, 0);
#endif
		
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
		return "UnitTest_06_MaterialPlayground";
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

DEFINE_APPLICATION_MAIN(MaterialPlayground)
