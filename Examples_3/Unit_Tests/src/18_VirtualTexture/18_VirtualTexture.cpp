/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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
//#define GARUANTEE_PAGE_SYNC

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

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

struct UniformVirtualTextureInfo
{
	uint Width;
	uint Height;
	uint pageWidth;
	uint pageHeight;

	uint DebugMode;
	uint ID;
	uint pad1;
	uint pad2;
	
	vec4 CameraPos;
};

const uint32_t			    gImageCount = 3;
ProfileToken                gGpuProfileToken;
const int					gSphereResolution = 30;    // Increase for higher resolution spheres
const float					gSphereDiameter = 0.5f;
const uint					gNumPlanets = 8;        // Sun, Mercury -> Neptune, Pluto, Moon
const uint					gTimeOffset = 1000000;    // For visually better starting locations
const float					gRotSelfScale = 0.0004f;
const float					gRotOrbitYScale = 0.001f;
const float					gRotOrbitZScale = 0.00001f;

uint								gFrequency = 1;
bool								gDebugMode = false;
bool								gShowUI = true;
float								gTimeScale = 1.0f;
bool								gPlay = true;

Renderer*						pRenderer = NULL;

Queue*							pGraphicsQueue = NULL;
CmdPool*						pCmdPools[gImageCount];
Cmd*							pCmds[gImageCount];

Queue*							pComputeQueue = NULL;
CmdPool*						pComputeCmdPools[gImageCount];
Cmd*							pComputeCmds[gImageCount];

CmdPool*						pVirtualTextureCmdPools[gImageCount];
Cmd*							pVirtualTextureCmds[gImageCount];

SwapChain*					pSwapChain = NULL;
RenderTarget*				pDepthBuffer = NULL;
Fence*							pRenderCompleteFences[gImageCount] = { NULL };
Fence*							pComputeCompleteFences[gImageCount] = { NULL };

Semaphore*					pImageAcquiredSemaphore = NULL;
Semaphore*					pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*							pSunShader = NULL;
Pipeline*						pSunPipeline = NULL;
Pipeline*						pSaturnPipeline = NULL;

Shader*							pSphereShader = NULL;
Pipeline*						pSpherePipeline = NULL;

Shader*							pClearPageCountsShader = NULL;
Pipeline*						pClearPageCountsPipeline = NULL;

Shader*							pFillPageShader = NULL;
Pipeline*						pFillPagePipeline = NULL;

Shader*							pDebugShader = NULL;
Pipeline*						pDebugPipeline = NULL;

Shader*							pSkyBoxDrawShader = NULL;
Pipeline*						pSkyBoxDrawPipeline = NULL;
RootSignature*					pRootSignature = NULL;
Sampler*						pSamplerSkyBox = NULL;
Texture*						pSkyBoxTextures[6];

Texture*						pVirtualTexture[gNumPlanets];
Buffer*							pVirtualTextureInfo[gNumPlanets] = { NULL };
Buffer*							pPageCountInfo[gNumPlanets] = { NULL };

Buffer*							pDebugInfo = NULL;

DescriptorSet*			pDescriptorSetTexture = { NULL };
DescriptorSet*			pDescriptorSetUniforms = { NULL };
VirtualJoystickUI		gVirtualJoystick;

Buffer*							pProjViewUniformBuffer[gImageCount] = { NULL };
Buffer*							pSkyboxUniformBuffer[gImageCount] = { NULL };

DescriptorSet*			pDescriptorSetComputePerFrame = { NULL };
DescriptorSet*			pDescriptorSetComputeFix = { NULL };
RootSignature*			pRootSignatureCompute = NULL;

static uint32_t		  gAccuFrameIndex = 0;
uint32_t						gFrameIndex = 0;

int									gNumberOfSpherePoints;
UniformBlock				gUniformData;
UniformBlock				gUniformDataSky;
eastl::vector<PlanetInfoStruct>		gPlanetInfoData;
UniformVirtualTextureInfo			gUniformVirtualTextureInfo[gNumPlanets];

const char*								gPlanetName[] = {"8k_sun", "8k_mercury", "8k_venus", "8k_earth", "16k_moon", "8k_mars", "8k_jupiter", "8k_saturn" };

Geometry*			pSphere;
Geometry*			pSaturn;
ICameraController*	pCameraController = NULL;

VertexLayout		gVertexLayoutDefault = {};

/// UI
UIApp								gAppUI;

//const char*					pSkyBoxImageFileNames[] = { "Skybox_right1", "Skybox_left2", "Skybox_top3", "Skybox_bottom4", "Skybox_front5", "Skybox_back6" };

TextDrawDesc				gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

GuiComponent*				pGui = NULL;

class VirtualTextureTest : public IApp
{
public:
	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,          "Meshes");

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

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

		queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_COMPUTE;
		addQueue(pRenderer, &queueDesc, &pComputeQueue);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pComputeQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pComputeCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pComputeCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pComputeCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pVirtualTextureCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pVirtualTextureCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pVirtualTextureCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addFence(pRenderer, &pComputeCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Loads Skybox Textures
/*
		for (int i = 0; i < 6; ++i)
		{
			PathHandle textureFilePath = fsGetPathInResourceDirEnum(RD_TEXTURES, pSkyBoxImageFileNames[i]);
			TextureLoadDesc textureDesc = {};
			textureDesc.pFilePath = textureFilePath;
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			addResource(&textureDesc, true);
		}
*/
		for (int i = 1; i < gNumPlanets; ++i)
		{
			TextureLoadDesc textureLoadDesc = {};
			textureLoadDesc.mContainer = TEXTURE_CONTAINER_SVT;
			textureLoadDesc.pFileName = gPlanetName[i];
			textureLoadDesc.ppTexture = &pVirtualTexture[i];
			addResource(&textureLoadDesc, NULL);
		}
		

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		//ShaderLoadDesc skyShader = {};
		//skyShader.mStages[0] = { "skybox.vert", NULL, 0, RD_SHADER_SOURCES };
		//skyShader.mStages[1] = { "skybox.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0 };
		basicShader.mStages[1] = { "basic.frag", NULL, 0 };
		ShaderLoadDesc debugShader = {};
		debugShader.mStages[0] = { "debug.vert", NULL, 0 };
		debugShader.mStages[1] = { "debug.frag", NULL, 0 };
		ShaderLoadDesc sunShader = {};
		sunShader.mStages[0] = { "basic.vert", NULL, 0 };
		sunShader.mStages[1] = { "sun.frag", NULL, 0 };

		//addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &basicShader, &pSphereShader);
		addShader(pRenderer, &debugShader, &pDebugShader);		
		addShader(pRenderer, &sunShader, &pSunShader);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSamplerSkyBox);

		Shader*           shaders[] = { pSphereShader, pDebugShader, pSunShader };
		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerSkyBox;
		rootDesc.mShaderCount = 3;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gNumPlanets };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetTexture);
		desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gNumPlanets * gImageCount + gImageCount };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetUniforms);


		ShaderLoadDesc clearPageCountsShader = {};
		clearPageCountsShader.mStages[0] = { "clearPageCounts.comp", NULL, 0 };

		addShader(pRenderer, &clearPageCountsShader, &pClearPageCountsShader);

		ShaderLoadDesc fillPageShader = {};
		fillPageShader.mStages[0] = { "fillPage.comp", NULL, 0 };

		addShader(pRenderer, &fillPageShader, &pFillPageShader);

		Shader*           computeShaders[] = { pClearPageCountsShader, pFillPageShader };
		rootDesc = {};
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = computeShaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureCompute);		

		desc = { pRootSignatureCompute, DESCRIPTOR_UPDATE_FREQ_NONE, gNumPlanets };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetComputeFix);
		desc = { pRootSignatureCompute, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * gNumPlanets };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetComputePerFrame);

		// Generate sphere vertex buffer
		/*
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
		tf_free(pSpherePoints);
		*/

		gVertexLayoutDefault.mAttribCount = 3;
		gVertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutDefault.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutDefault.mAttribs[0].mBinding = 0;
		gVertexLayoutDefault.mAttribs[0].mLocation = 0;
		gVertexLayoutDefault.mAttribs[0].mOffset = 0;

		gVertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutDefault.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutDefault.mAttribs[1].mBinding = 0;
		gVertexLayoutDefault.mAttribs[1].mLocation = 1;
		gVertexLayoutDefault.mAttribs[1].mOffset = 3 * sizeof(float);

		gVertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutDefault.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutDefault.mAttribs[2].mBinding = 0;
		gVertexLayoutDefault.mAttribs[2].mLocation = 2;
		gVertexLayoutDefault.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats
/////////////////////////////////////
//						Load Models
/////////////////////////////////////
		{
			GeometryLoadDesc loadDesc = {};
			loadDesc.pFileName = "sphereHires.gltf";
			loadDesc.ppGeometry = &pSphere;
			loadDesc.pVertexLayout = &gVertexLayoutDefault;
			addResource(&loadDesc, NULL);

			loadDesc.pFileName = "saturn.gltf";
			loadDesc.ppGeometry = &pSaturn;
			addResource(&loadDesc, NULL);
		}
///////////////////////////////////

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
			addResource(&ubDesc, NULL);
			ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		for (uint32_t i = 0; i < gNumPlanets; ++i)
		{
			BufferLoadDesc vtInfoDesc = {};
			vtInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			vtInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			vtInfoDesc.mDesc.mSize = sizeof(UniformVirtualTextureInfo);
			//vtInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			vtInfoDesc.pData = NULL;
			vtInfoDesc.ppBuffer = &pVirtualTextureInfo[i];
			addResource(&vtInfoDesc, NULL);

			BufferLoadDesc ptInfoDesc = {};
			ptInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ptInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			ptInfoDesc.mDesc.mSize = sizeof(uint)*4;
			//ptInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			ptInfoDesc.pData = NULL;
			ptInfoDesc.ppBuffer = &pPageCountInfo[i];
			addResource(&ptInfoDesc, NULL);
		}

		BufferLoadDesc debugInfoDesc = {};
		debugInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		debugInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		debugInfoDesc.mDesc.mSize = sizeof(uint) * 4;
		debugInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		debugInfoDesc.pData = NULL;
		debugInfoDesc.ppBuffer = &pDebugInfo;
		addResource(&debugInfoDesc, NULL);

		// Setup planets (Rotation speeds are relative to Earth's, some values randomly given)

		// Sun
		PlanetInfoStruct PlanetInfo;

		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 0;    // Earth years for one orbit
		PlanetInfo.mZOrbitSpeed = 0;
		PlanetInfo.mRotationSpeed = 24.0f;    // Earth days for one rotation
		PlanetInfo.mTranslationMat = mat4::identity();
		PlanetInfo.mScaleMat = mat4::scale(vec3(10.0f));
		PlanetInfo.mColor = vec4(0.9f, 0.6f, 0.1f, 0.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Mercury
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 0.24f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 58.7f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(12.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(1.0f));
		PlanetInfo.mColor = vec4(0.7f, 0.3f, 0.1f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Venus
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 0.68f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 243.0f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(20.0f, 0, 5));
		PlanetInfo.mScaleMat = mat4::scale(vec3(2));
		PlanetInfo.mColor = vec4(0.8f, 0.6f, 0.1f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Earth
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 1.0f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 1.0f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(30.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(4));
		PlanetInfo.mColor = vec4(0.3f, 0.2f, 0.8f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Moon
		PlanetInfo.mParentIndex = 3;
		PlanetInfo.mYOrbitSpeed = 1.0f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 27.3f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(6.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(1));
		PlanetInfo.mColor = vec4(0.3f, 0.3f, 0.4f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);


		// Mars
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 1.9f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 1.03f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(40.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(3));
		PlanetInfo.mColor = vec4(0.9f, 0.3f, 0.1f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Jupiter
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 12.0f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 0.4f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(55.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(8));
		PlanetInfo.mColor = vec4(0.6f, 0.4f, 0.4f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Saturn
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 29.0f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 0.45f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(70.0f, 0, 0)) * mat4::rotationZ(0.5f);
		PlanetInfo.mScaleMat = mat4::scale(vec3(6));
		PlanetInfo.mColor = vec4(0.7f, 0.7f, 0.5f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Uranus
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 84.07f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 0.8f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(70.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(7));
		PlanetInfo.mColor = vec4(0.4f, 0.4f, 0.6f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Neptune
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 164.81f;
		PlanetInfo.mZOrbitSpeed = 0.0f;
		PlanetInfo.mRotationSpeed = 0.9f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(80.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(8));
		PlanetInfo.mColor = vec4(0.5f, 0.2f, 0.9f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		// Pluto - Not a planet XDD
		PlanetInfo.mParentIndex = 0;
		PlanetInfo.mYOrbitSpeed = 247.7f;
		PlanetInfo.mZOrbitSpeed = 1.0f;
		PlanetInfo.mRotationSpeed = 7.0f;
		PlanetInfo.mTranslationMat = mat4::translation(vec3(90.0f, 0, 0));
		PlanetInfo.mScaleMat = mat4::scale(vec3(1.0f));
		PlanetInfo.mColor = vec4(0.7f, 0.5f, 0.5f, 1.0f);

		gPlanetInfoData.push_back(PlanetInfo);

		
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);

		pGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);

		CheckboxWidget DeMode("Show Pages' Border", &gDebugMode);
		pGui->AddWidget(DeMode);

		SliderUintWidget DSampling("Virtual Texture Update Frequency", &gFrequency, 1, 40, 1);
		pGui->AddWidget(DSampling);

		CheckboxWidget Play("Play Planet's movement", &gPlay);
		pGui->AddWidget(Play);		

		SliderFloatWidget TimeScale("Time Scale", &gTimeScale, 1.0f, 10.0f, 0.0001f);
		pGui->AddWidget(TimeScale);

		CameraMotionParameters cmp{ 16.0f, 60.0f, 20.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		if (!initInputSystem(pWindow))
			return false;

        // Initialize microprofiler and it's UI.
        initProfiler();
    
        // Gpu profiler can only be added after initProfile.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics" );

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
			if (!gAppUI.IsFocused() && *ctx->pCaptured)
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
		actionDesc = { InputBindings::BUTTON_L3, [](InputActionContext* ctx) { gShowUI = !gShowUI; return true; } };
		addInputAction(&actionDesc);

		waitForAllResourceLoads();
		
		// Prepare descriptor sets
		for (uint32_t j = 0; j < gNumPlanets; ++j)
		{
			DescriptorData params[3] = {};
/*
			params[0].pName = "RightText";
			params[0].ppTextures = &pSkyBoxTextures[0];
			params[1].pName = "LeftText";
			params[1].ppTextures = &pSkyBoxTextures[1];
			params[2].pName = "TopText";
			params[2].ppTextures = &pSkyBoxTextures[2];
			params[3].pName = "BotText";
			params[3].ppTextures = &pSkyBoxTextures[3];
			params[4].pName = "FrontText";
			params[4].ppTextures = &pSkyBoxTextures[4];
			params[5].pName = "BackText";
			params[5].ppTextures = &pSkyBoxTextures[5];
*/			
			params[0].pName = "SparseTextureInfo";
			params[0].ppBuffers = &pVirtualTextureInfo[j];
			params[1].pName = "MipLevel";
			params[1].ppBuffers = &pDebugInfo;

			if (j == 0)
			{
				updateDescriptorSet(pRenderer, j, pDescriptorSetTexture, 2, params);
			}
			else
			{
				params[2].pName = "SparseTexture";
				params[2].ppTextures = &pVirtualTexture[j];
				updateDescriptorSet(pRenderer, j, pDescriptorSetTexture, 3, params);
			}
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[2] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pSkyboxUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);

			for (uint32_t j = 0; j < gNumPlanets; ++j)
			{
				params[0].ppBuffers = &pProjViewUniformBuffer[i];

				if(j == 0)
				{
					updateDescriptorSet(pRenderer, gNumPlanets * i + j + gImageCount, pDescriptorSetUniforms, 1, params);
				}
				else
				{
					params[1].pName = "VisibilityBuffer";
					params[1].ppBuffers = &pVirtualTexture[j]->pSvt->mVisibility;
					updateDescriptorSet(pRenderer, gNumPlanets * i + j + gImageCount, pDescriptorSetUniforms, 2, params);
				}
			}
		}


		for (uint32_t i = 0; i < gNumPlanets; ++i)
		{
			DescriptorData computeParams[1] = {};
			computeParams[0].pName = "PageCountInfo";
			computeParams[0].ppBuffers = &pPageCountInfo[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetComputeFix, 1, computeParams);
		}


		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 1; j < gNumPlanets; ++j)
			{
				DescriptorData computeParams[6] = {};
				computeParams[0].pName = "PrevPageTableBuffer";
				computeParams[0].ppBuffers = &pVirtualTexture[j]->pSvt->mPrevVisibility;

				computeParams[1].pName = "PageTableBuffer";
				computeParams[1].ppBuffers = &pVirtualTexture[j]->pSvt->mVisibility;

				computeParams[2].pName = "RemovePageTableBuffer";
				computeParams[2].ppBuffers = &pVirtualTexture[j]->pSvt->mRemovePage;

				computeParams[3].pName = "AlivePageTableBuffer";
				computeParams[3].ppBuffers = &pVirtualTexture[j]->pSvt->mAlivePage;

				computeParams[4].pName = "PageCountsBuffer";
				computeParams[4].ppBuffers = &pVirtualTexture[j]->pSvt->mPageCounts;

				updateDescriptorSet(pRenderer, i * gNumPlanets + j, pDescriptorSetComputePerFrame, 5, computeParams);
			}
		}

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		destroyCameraController(pCameraController);

		gVirtualJoystick.Exit();

		gAppUI.Exit();

		// Exit profile
		exitProfiler();

		gPlanetInfoData.set_capacity(0);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
			removeResource(pSkyboxUniformBuffer[i]);
		}

		for (uint32_t i = 0; i < gNumPlanets; ++i)
		{
			removeResource(pVirtualTextureInfo[i]);
			removeResource(pPageCountInfo[i]);
		}
		removeResource(pDebugInfo);

		removeDescriptorSet(pRenderer, pDescriptorSetTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);

		removeDescriptorSet(pRenderer, pDescriptorSetComputePerFrame);
		removeDescriptorSet(pRenderer, pDescriptorSetComputeFix);

		for (int i = 1; i < gNumPlanets; ++i)
		{
			removeResource(pVirtualTexture[i]);
		}

		removeResource(pSphere);
		removeResource(pSaturn);
/*
		for (uint i = 0; i < 6; ++i)
			removeResource(pSkyBoxTextures[i]);
*/

		removeSampler(pRenderer, pSamplerSkyBox);

		removeShader(pRenderer, pDebugShader);
		removeShader(pRenderer, pSphereShader);
		//removeShader(pRenderer, pSkyBoxDrawShader);
		removeShader(pRenderer, pSunShader);

		removeShader(pRenderer, pFillPageShader);
		removeShader(pRenderer, pClearPageCountsShader);

		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pRootSignatureCompute);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeFence(pRenderer, pComputeCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);

			removeCmd(pRenderer, pComputeCmds[i]);
			removeCmdPool(pRenderer, pComputeCmdPools[i]);

			removeCmd(pRenderer, pVirtualTextureCmds[i]);
			removeCmdPool(pRenderer, pVirtualTextureCmdPools[i]);
		}

		exitResourceLoaderInterface(pRenderer);

		removeQueue(pRenderer, pGraphicsQueue);
		removeQueue(pRenderer, pComputeQueue);

		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppRenderTargets[0]))
			return false;

		loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateDesc.mSrcFactors[0] = BC_ONE;
		blendStateDesc.mDstFactors[0] = BC_ONE;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;

		BlendStateDesc blendStateSaturnDesc = {};
		blendStateSaturnDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateSaturnDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateSaturnDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateSaturnDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateSaturnDesc.mMasks[0] = ALL;
		blendStateSaturnDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateSaturnDesc.mIndependentBlend = false;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc sphereRasterizerStateDesc = {};
		sphereRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		//layout and pipeline for sphere draw
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
		pipelineSettings.pShaderProgram = pSphereShader;
		pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
		pipelineSettings.pRasterizerState = &sphereRasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pSpherePipeline);

		{
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pShaderProgram = pDebugShader;
			addPipeline(pRenderer, &desc, &pDebugPipeline);
		}

		{
			pipelineSettings.pDepthState = &depthStateDesc;
			pipelineSettings.pShaderProgram = pSunShader;
			pipelineSettings.pBlendState = &blendStateDesc;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			addPipeline(pRenderer, &desc, &pSunPipeline);
		}

		{
			pipelineSettings.pDepthState = &depthStateDesc;
			pipelineSettings.pShaderProgram = pSphereShader;
			pipelineSettings.pBlendState = &blendStateSaturnDesc;
			pipelineSettings.pRasterizerState = &rasterizerStateDesc;
			addPipeline(pRenderer, &desc, &pSaturnPipeline);
		}

		//layout and pipeline for skybox draw
/*
		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = pSkyboxRast;
		pipelineSettings.pBlendState = NULL;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		addPipeline(pRenderer, &desc, &pSkyBoxDrawPipeline);		
*/
		{
			PipelineDesc computeDesc = {};
			computeDesc.mType = PIPELINE_TYPE_COMPUTE;
			ComputePipelineDesc& cpipelineSettings = computeDesc.mComputeDesc;

			cpipelineSettings.pShaderProgram = pClearPageCountsShader;
			cpipelineSettings.pRootSignature = pRootSignatureCompute;
			addPipeline(pRenderer, &computeDesc, &pClearPageCountsPipeline);

			cpipelineSettings.pShaderProgram = pFillPageShader;
			addPipeline(pRenderer, &computeDesc, &pFillPagePipeline);
		}

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfilerUI();
		gAppUI.Unload();

		gVirtualJoystick.Unload();

		//removePipeline(pRenderer, pSkyBoxDrawPipeline);		
		removePipeline(pRenderer, pSpherePipeline);
		removePipeline(pRenderer, pDebugPipeline);
		removePipeline(pRenderer, pSunPipeline);
		removePipeline(pRenderer, pSaturnPipeline);

		removePipeline(pRenderer, pFillPagePipeline);
		removePipeline(pRenderer, pClearPageCountsPipeline);

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
		static float currentTime = 0.0f;

		if(!gPlay)
			deltaTime =	0.0f;
		
		currentTime += deltaTime * gTimeScale * 130.0f;
		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.001f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat;

		// point light parameters
		gUniformData.mLightPosition = vec3(0, 0, 0);
		gUniformData.mLightColor = vec3(0.9f, 0.9f, 0.7f);    // Pale Yellow

		// update planet transformations
		for (unsigned int i = 0; i < gNumPlanets; i++)
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

		viewMat.setTranslation(vec3(0));
		gUniformDataSky = gUniformData;
		gUniformDataSky.mProjectView = projMat * viewMat;

		for (int i = 0; i < gNumPlanets; ++i)
		{
			if (i == 0)
			{
				gUniformVirtualTextureInfo[i].Width = 0;
				gUniformVirtualTextureInfo[i].Height = 0;
				gUniformVirtualTextureInfo[i].pageWidth = 0;
				gUniformVirtualTextureInfo[i].pageHeight = 0;
			}
			else
			{
				gUniformVirtualTextureInfo[i].Width = pVirtualTexture[i]->mWidth;
				gUniformVirtualTextureInfo[i].Height = pVirtualTexture[i]->mHeight;
				gUniformVirtualTextureInfo[i].pageWidth = (uint)pVirtualTexture[i]->pSvt->mSparseVirtualTexturePageWidth;
				gUniformVirtualTextureInfo[i].pageHeight = (uint)pVirtualTexture[i]->pSvt->mSparseVirtualTexturePageHeight;
			}

			gUniformVirtualTextureInfo[i].ID = i;
			gUniformVirtualTextureInfo[i].CameraPos = vec4(pCameraController->getViewPosition(), 0.0f);
			

			gUniformVirtualTextureInfo[i].DebugMode = gDebugMode ? 1: 0;

			BufferUpdateDesc virtualTextureInfoCbv = { pVirtualTextureInfo[i] };
			beginUpdateResource(&virtualTextureInfoCbv);
			*(UniformVirtualTextureInfo*)virtualTextureInfoCbv.pMappedData = gUniformVirtualTextureInfo[i];
			endUpdateResource(&virtualTextureInfoCbv, NULL);
		}

		
    /************************************************************************/
    // Update GUI
    /************************************************************************/
    gAppUI.Update(deltaTime);  
	}

	void Draw()
	{
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];		

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);
		resetCmdPool(pRenderer, pComputeCmdPools[gFrameIndex]);
		resetCmdPool(pRenderer, pVirtualTextureCmdPools[gFrameIndex]);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(UniformBlock*)viewProjCbv.pMappedData = gUniformData;
		endUpdateResource(&viewProjCbv, NULL);

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex] };
		beginUpdateResource(&skyboxViewProjCbv);
		*(UniformBlock*)skyboxViewProjCbv.pMappedData = gUniformDataSky;
		endUpdateResource(&skyboxViewProjCbv, NULL);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};

		TextureBarrier barriers2[] = {
			{ pVirtualTexture[1], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE },
			{ pVirtualTexture[2], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE },
			{ pVirtualTexture[3], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE },
			{ pVirtualTexture[4], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE },
			{ pVirtualTexture[5], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE },
			{ pVirtualTexture[6], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE },
			{ pVirtualTexture[7], RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE }
		};
		cmdResourceBarrier(cmd, 0, NULL, 7, barriers2, 1, barriers);
//#if defined(VULKAN)
		//cmdResourceBarrier(cmd, 0, NULL, (uint32_t)virtualTextureBarrier.size(), virtualTextureBarrier.data());
//#endif
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
    
	{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Planets");

			cmdBindPipeline(cmd, pSpherePipeline);

			//// draw planets
			for (uint32_t j = 1; j < gNumPlanets - 1; ++j)
			{
				cmdBindDescriptorSet(cmd, j, pDescriptorSetTexture);
				cmdBindDescriptorSet(cmd, gNumPlanets * gFrameIndex + j + gImageCount, pDescriptorSetUniforms);
			
				cmdBindVertexBuffer(cmd, 1, &pSphere->pVertexBuffers[0], pSphere->mVertexStrides, NULL);
				cmdBindIndexBuffer(cmd, pSphere->pIndexBuffer, pSphere->mIndexType, 0);
				cmdDrawIndexed(cmd, pSphere->mIndexCount, 0, 0);
			}		

			cmdBindPipeline(cmd, pSaturnPipeline);

			cmdBindDescriptorSet(cmd, gNumPlanets - 1, pDescriptorSetTexture);
			cmdBindDescriptorSet(cmd, gNumPlanets * gFrameIndex + gNumPlanets - 1 + gImageCount, pDescriptorSetUniforms);
		
			cmdBindVertexBuffer(cmd, 1, &pSaturn->pVertexBuffers[0], pSaturn->mVertexStrides, NULL);
			cmdBindIndexBuffer(cmd, pSaturn->pIndexBuffer, pSaturn->mIndexType, 0);
			cmdDrawIndexed(cmd, pSaturn->mIndexCount, 0, 0);

			cmdBindPipeline(cmd, pSunPipeline);

			cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
			cmdBindDescriptorSet(cmd, gNumPlanets * gFrameIndex + gImageCount, pDescriptorSetUniforms);
			cmdBindVertexBuffer(cmd, 1, &pSphere->pVertexBuffers[0], pSphere->mVertexStrides, NULL);
			cmdBindIndexBuffer(cmd, pSphere->pIndexBuffer, pSphere->mIndexType, 0);
			cmdDrawIndexed(cmd, pSphere->mIndexCount, 0, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
	}
	
	if(gShowUI)
	{
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

        float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

#if !defined(__ANDROID__)
		gAppUI.DrawText(
			cmd, float2(8.f, txtSize.y + 30.f), eastl::string().sprintf("Update Virtual Texture %f ms", getCpuAvgFrameTime() - (float)getGpuProfileTime(gGpuProfileToken)).c_str(),
			&gFrameTimeDraw);

    cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y * 2.f + 45.f), gGpuProfileToken, &gFrameTimeDraw);
#endif

    cmdDrawProfilerUI();

    gAppUI.Gui(pGui);

		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
	}
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

		// Get visibility info
		if (gAccuFrameIndex % gFrequency == 0)
		{
			// Extract only alive page
			Cmd* pCmdCompute = NULL;

			pCmdCompute = pComputeCmds[gFrameIndex];
			beginCmd(pCmdCompute);

			for (int i = 1; i < gNumPlanets; ++i)
			{
				eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pVirtualTexture[i]->pSvt->pPages;

				struct PageCountSt
				{
					uint maxPageCount;
					uint pageOffset;
					uint pad1;
					uint pad2;
				}pageCountSt;

	#if defined(GARUANTEE_PAGE_SYNC)
				const uint32_t* pThreadGroupSize = pFillPageShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;

				const uint32_t Dispatch[3] = { pThreadGroupSize[0],
																			 pThreadGroupSize[1],
																			 pThreadGroupSize[2] };

				pageCountSt.maxPageCount = pThreadGroupSize[0];
				pageCountSt.pageOffset = 0;

				int PageCount = (uint)pPageTable->size();

				while (PageCount > 0)
				{				
					BufferUpdateDesc pageCountInfoCbv = { pPageCountInfo, &pageCountSt };
					beginUpdateResource(&pageCountInfoCbv);
					endUpdateResource(&pageCountInfoCbv, NULL);

					pCmdCompute = ppComputeCmds[gFrameIndex];
					beginCmd(pCmdCompute);

					cmdBindPipeline(pCmdCompute, pClearPageCountsPipeline);
					cmdBindDescriptorSet(pCmdCompute, 0, pDescriptorSetComputeFix);
					cmdBindDescriptorSet(pCmdCompute, gFrameIndex, pDescriptorSetComputePerFrame);

					cmdDispatch(pCmdCompute, 1, 1, 1);

					cmdBindPipeline(pCmdCompute, pFillPagePipeline);
					cmdDispatch(pCmdCompute, 1, 1, 1);

					endCmd(pCmdCompute);
					QueueSubmitDesc submitDesc = {};
					submitDesc.mCmdCount = 1;
					submitDesc.ppCmds = &pCmdCompute;
					queueSubmit(pComputeQueue, &submitDesc);
					waitQueueIdle(pComputeQueue);

					PageCount -= pageCountSt.maxPageCount;
					pageCountSt.pageOffset += pageCountSt.maxPageCount;
					pageCountSt.maxPageCount = min((uint)PageCount, pageCountSt.maxPageCount);
				}
	#else
				pageCountSt.maxPageCount = pVirtualTexture[i]->pSvt->mVirtualPageTotalCount;
				pageCountSt.pageOffset = 0;

				BufferUpdateDesc pageCountInfoCbv = { pPageCountInfo[i] };
				beginUpdateResource(&pageCountInfoCbv);
				*(PageCountSt*)pageCountInfoCbv.pMappedData = pageCountSt;
				endUpdateResource(&pageCountInfoCbv, NULL);

				cmdBindPipeline(pCmdCompute, pClearPageCountsPipeline);

				cmdBindDescriptorSet(pCmdCompute, i, pDescriptorSetComputeFix);
				cmdBindDescriptorSet(pCmdCompute, gFrameIndex * gNumPlanets + i, pDescriptorSetComputePerFrame);

				cmdDispatch(pCmdCompute, 1, 1, 1);

				cmdBindPipeline(pCmdCompute, pFillPagePipeline);
				const uint32_t* pThreadGroupSize = pFillPageShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;

				const uint32_t Dispatch[3] = { (uint32_t)ceil((float)pageCountSt.maxPageCount / (float)pThreadGroupSize[0]),
																			 pThreadGroupSize[1],
																			 pThreadGroupSize[2] };

				cmdDispatch(pCmdCompute, Dispatch[0], Dispatch[1], Dispatch[2]);				
	#endif				
			}

			endCmd(pCmdCompute);

			QueueSubmitDesc submitDesc = {};
			submitDesc.mCmdCount = 1;
			submitDesc.ppCmds = &pCmdCompute;
			queueSubmit(pComputeQueue, &submitDesc);
			waitQueueIdle(pComputeQueue);

			Cmd* pCmdVirtualTexture = pVirtualTextureCmds[gFrameIndex];
			beginCmd(pCmdVirtualTexture);

			TextureBarrier barriers2[] = {
				{ pVirtualTexture[1], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST },
				{ pVirtualTexture[2], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST },
				{ pVirtualTexture[3], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST },
				{ pVirtualTexture[4], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST },
				{ pVirtualTexture[5], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST },
				{ pVirtualTexture[6], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST },
				{ pVirtualTexture[7], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_COPY_DEST }
			};
			cmdResourceBarrier(pCmdVirtualTexture, 0, NULL, 7, barriers2, 0, NULL);

			for (int i = 1; i < gNumPlanets; ++i)
			{
				cmdUpdateVirtualTexture(pCmdVirtualTexture, pVirtualTexture[i]);
			}

			endCmd(pCmdVirtualTexture);

			submitDesc.ppCmds = &pCmdVirtualTexture;
			queueSubmit(pGraphicsQueue, &submitDesc);
			waitQueueIdle(pGraphicsQueue);
		}

		gAccuFrameIndex++;

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "18_VirtualTexture"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
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
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
};

DEFINE_APPLICATION_MAIN(VirtualTextureTest)
