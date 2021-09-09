/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

// Unit Test for distributing heavy gpu workload such as Split Frame Rendering to Multiple Identical GPUs
// GPU 0 Renders Left Eye and finally does a composition pass to present Left and Right eye textures to screen
// GPU 1 Renders Right Eye

#define MAX_PLANETS 20    // Does not affect test, just for allocating space in uniform block. Must match with shader.
#define MAX_GPU_PROFILER_NAME_LENGTH 128

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IScripting.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/OS/Interfaces/IUI.h"
#include "../../../../Common_3/OS/Interfaces/IFont.h"

#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Middleware_3/PaniniProjection/Panini.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

/// Demo structures
struct PlanetInfoStruct
{
	mat4  mTranslationMat;
	mat4  mScaleMat;
	mat4  mSharedMat;    // Matrix to pass down to children
	vec4  mColor;
	uint  mParentIndex;
	float mYOrbitSpeed;    // Rotation speed around parent
	float mZOrbitSpeed;
	float mRotationSpeed;    // Rotation speed around self
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

const uint32_t gViewCount = 2;
bool           gToggleVSync = false;
// Simulate heavy gpu workload by rendering high resolution spheres
const int   gSphereResolution = 1024;    // Increase for higher resolution spheres
const float gSphereDiameter = 0.5f;
const uint  gNumPlanets = 11;        // Sun, Mercury -> Neptune, Pluto, Moon
const uint  gTimeOffset = 600000;    // For visually better starting locations
const float gRotSelfScale = 0.0004f;
const float gRotOrbitYScale = 0.001f;
const float gRotOrbitZScale = 0.00001f;

Renderer* pRenderer = NULL;

Queue* pGraphicsQueue[gViewCount] = { NULL };
CmdPool* pCmdPools[gImageCount][gViewCount] = { NULL };
Cmd* pCmds[gImageCount][gViewCount] = { NULL };
Fence* pRenderCompleteFences[gImageCount][gViewCount] = { { NULL } };
Semaphore* pRenderCompleteSemaphores[gImageCount][gViewCount] = { { NULL } };
Buffer* pSphereVertexBuffer[gViewCount] = { NULL };
Buffer* pSkyBoxVertexBuffer[gViewCount] = { NULL };
Texture* pSkyBoxTextures[gViewCount][6];
RenderTarget* pRenderTargets[gImageCount][gViewCount] = { { NULL } };
RenderTarget* pDepthBuffers[gViewCount] = { NULL };

Semaphore* pImageAcquiredSemaphore = NULL;
SwapChain* pSwapChain = NULL;

Shader* pSphereShader = NULL;
Pipeline* pSpherePipeline = NULL;

Shader* pSkyBoxDrawShader = NULL;
Pipeline* pSkyBoxDrawPipeline = NULL;
RootSignature* pRootSignature = NULL;
Sampler* pSamplerSkyBox = NULL;
DescriptorSet* pDescriptorSetTexture[gViewCount] = { NULL };
DescriptorSet* pDescriptorSetUniforms[gViewCount] = { NULL };

Buffer* pProjViewUniformBuffer[gImageCount] = { NULL };
Buffer* pSkyboxUniformBuffer[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;

int              gNumberOfSpherePoints;
UniformBlock     gUniformData;
UniformBlock     gUniformDataSky;
PlanetInfoStruct gPlanetInfoData[gNumPlanets];

ICameraController* pCameraController = NULL;

/// UI
UIComponent* pGui;

const char* pSkyBoxImageFileNames[] = { "Skybox_right1",  "Skybox_left2",  "Skybox_top3",
										"Skybox_bottom4", "Skybox_front5", "Skybox_back6" };
const char* pGpuProfilerNames[gViewCount] = { NULL };
char gGpuProfilerNames[gViewCount][MAX_GPU_PROFILER_NAME_LENGTH]{};
ProfileToken gGpuProfilerTokens[gViewCount];

FontDrawDesc     gFrameTimeDraw;
uint32_t         gFontID = 0; 
ClearValue       gClearColor; // initialization in Init
ClearValue       gClearDepth;
Panini           gPanini = {};
PaniniParameters gPaniniParams = {};
bool             gMultiGPU = true;
bool             gMultiGPURestart = false;
float* pSpherePoints;

const char* gTestScripts[] = { "Test0.lua", "Test1.lua" };
uint32_t gScriptIndexes[] = { 0, 1 };
uint32_t gCurrentScriptIndex = 0;

void RunScript()
{
	LuaScriptDesc runDesc = {};
	runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
	luaQueueScriptToRun(&runDesc);
}

class MultiGPU : public IApp
{
public:
	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		gClearColor.r = 0.0f;
		gClearColor.g = 0.0f;
		gClearColor.b = 0.0f;
		gClearColor.a = 0.0f;

		gClearDepth.depth = 1.0f;
		gClearDepth.stencil = 0;

		// Generate sphere vertex buffer
		if (!pSpherePoints)
			generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution, gSphereDiameter);

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

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mD3D11Unsupported = true;
		settings.mGLESUnsupported = true;
		settings.mGpuMode = gMultiGPU ? GPU_MODE_LINKED : GPU_MODE_SINGLE;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		initResourceLoaderInterface(pRenderer);

		if (pRenderer->mGpuMode == GPU_MODE_SINGLE && gMultiGPU)
		{
			LOGF(LogLevel::eWARNING, "Multi GPU will be disabled since the system only has one GPU");
			gMultiGPU = false;
		}
		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			QueueDesc queueDesc = {};
			queueDesc.mType = QUEUE_TYPE_GRAPHICS;
			queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
			queueDesc.mNodeIndex = i;

			if (!gMultiGPU && i > 0)
				pGraphicsQueue[i] = pGraphicsQueue[0];
			else
				addQueue(pRenderer, &queueDesc, &pGraphicsQueue[i]);

			sprintf(gGpuProfilerNames[i], "Graphics");

			char index[10];
			sprintf(index, "%u", i);
			strcat(gGpuProfilerNames[i], index);

			pGpuProfilerNames[i] = gGpuProfilerNames[i];
		}

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
		LuaScriptDesc scriptDescs[numScripts] = {};
		for (uint32_t i = 0; i < numScripts; ++i)
			scriptDescs[i].pScriptFileName = gTestScripts[i];
		luaDefineScripts(scriptDescs, numScripts);

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);
		uiCreateComponent(GetName(), &guiDesc, &pGui);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.ppQueues = pGraphicsQueue; 
		profiler.ppProfilerNames = pGpuProfilerNames; 
		profiler.pProfileTokens = gGpuProfilerTokens;
		profiler.mGpuProfilerCount = gViewCount; 
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gViewCount; ++j)
			{
				CmdPoolDesc cmdPoolDesc = {};
				cmdPoolDesc.pQueue = pGraphicsQueue[j];
				addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i][j]);
				CmdDesc cmdDesc = {};
				cmdDesc.pPool = pCmdPools[i][j];
				addCmd(pRenderer, &cmdDesc, &pCmds[i][j]);

				addFence(pRenderer, &pRenderCompleteFences[i][j]);
				addSemaphore(pRenderer, &pRenderCompleteSemaphores[i][j]);
			}
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0 };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0 };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0 };
		basicShader.mStages[1] = { "basic.frag", NULL, 0 };

		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &basicShader, &pSphereShader);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSamplerSkyBox);

		Shader* shaders[] = { pSphereShader, pSkyBoxDrawShader };
		const char* pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerSkyBox;
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		for (uint32_t i = 0; i < gViewCount; i++)
		{
			if (!gMultiGPU && i > 0)
			{
				pDescriptorSetTexture[i] = pDescriptorSetTexture[0];
				pDescriptorSetUniforms[i] = pDescriptorSetUniforms[0];
			}
			else
			{
				DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1, i };
				addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture[i]);
				setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2, i };
				addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms[i]);
			}
		}

		uint64_t       sphereDataSize = gNumberOfSpherePoints * sizeof(float);
		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mSize = sphereDataSize;
		sphereVbDesc.pData = pSpherePoints;

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
		skyboxVbDesc.pData = skyBoxPoints;

		TextureLoadDesc textureDesc = {};

		for (uint32_t view = 0; view < gViewCount; ++view)
		{
			textureDesc.mNodeIndex = view;

			for (int i = 0; i < 6; ++i)
			{
				textureDesc.pFileName = pSkyBoxImageFileNames[i];
				textureDesc.ppTexture = &pSkyBoxTextures[view][i];

				if (!gMultiGPU && view > 0)
					pSkyBoxTextures[view][i] = pSkyBoxTextures[0][i];
				else
					addResource(&textureDesc, NULL);
			}

			sphereVbDesc.mDesc.mNodeIndex = view;
			sphereVbDesc.ppBuffer = &pSphereVertexBuffer[view];

			skyboxVbDesc.mDesc.mNodeIndex = view;
			skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer[view];

			if (!gMultiGPU && view > 0)
			{
				pSphereVertexBuffer[view] = pSphereVertexBuffer[0];
				pSkyBoxVertexBuffer[view] = pSkyBoxVertexBuffer[0];
			}
			else
			{
				addResource(&sphereVbDesc, NULL);
				addResource(&skyboxVbDesc, NULL);
			}
		}

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

		waitForAllResourceLoads();

#if !defined(TARGET_IOS)
		CheckboxWidget vSyncCheckbox;
		vSyncCheckbox.pData = &gToggleVSync;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Toggle VSync", &vSyncCheckbox, WIDGET_TYPE_CHECKBOX));
#endif
		// Reset graphics with a button.

// Show this checkbox only when multiple GPUs are present
		if (pRenderer->mLinkedNodeCount > 1)
		{
			CheckboxWidget multiGpuCheckbox;
			multiGpuCheckbox.pData = &gMultiGPU;
			luaRegisterWidget(uiCreateComponentWidget(pGui, "Enable Multi GPU", &multiGpuCheckbox, WIDGET_TYPE_CHECKBOX));
		}

		SliderFloatWidget camHorFovSlider;
		camHorFovSlider.pData = &gPaniniParams.FoVH;
		camHorFovSlider.mMin = 30.0f;
		camHorFovSlider.mMax = 179.0f;
		camHorFovSlider.mStep = 1.0f;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Camera Horizontal FoV", &camHorFovSlider, WIDGET_TYPE_SLIDER_FLOAT));

		SliderFloatWidget paniniSliderD;
		paniniSliderD.pData = &gPaniniParams.D;
		paniniSliderD.mMin = 0.0f;
		paniniSliderD.mMax = 1.0f;
		paniniSliderD.mStep = 0.001f;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Panini D Parameter", &paniniSliderD, WIDGET_TYPE_SLIDER_FLOAT));

		SliderFloatWidget paniniSliderS;
		paniniSliderS.pData = &gPaniniParams.S;
		paniniSliderS.mMin = 0.0f;
		paniniSliderS.mMax = 1.0f;
		paniniSliderS.mStep = 0.001f;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Panini S Parameter", &paniniSliderS, WIDGET_TYPE_SLIDER_FLOAT));

		SliderFloatWidget screenScaleSlider;
		screenScaleSlider.pData = &gPaniniParams.scale;
		screenScaleSlider.mMin = 1.0f;
		screenScaleSlider.mMax = 10.0f;
		screenScaleSlider.mStep = 0.01f;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Screen Scale", &screenScaleSlider, WIDGET_TYPE_SLIDER_FLOAT));

		DropdownWidget ddTestScripts;
		ddTestScripts.pData = &gCurrentScriptIndex;
		for (uint32_t i = 0; i < sizeof(gTestScripts) / sizeof(gTestScripts[0]); ++i)
		{
			ddTestScripts.mValues.push_back(gScriptIndexes[i]);
			ddTestScripts.mNames.push_back((char*)gTestScripts[i]);
		}
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

		ButtonWidget bRunScript;
		UIWidget* pRunScript = uiCreateComponentWidget(pGui, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pRunScript, RunScript);
		luaRegisterWidget(pRunScript);

		if (!gPanini.Init(pRenderer))
			return false;

		gPanini.SetMaxDraws(gImageCount * 2);

		// Prepare descriptor sets
		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			if (i > 0 && pDescriptorSetTexture[i] == pDescriptorSetTexture[0])
				continue;

			DescriptorData params[6] = {};
			params[0].pName = "RightText";
			params[0].ppTextures = &pSkyBoxTextures[i][0];
			params[1].pName = "LeftText";
			params[1].ppTextures = &pSkyBoxTextures[i][1];
			params[2].pName = "TopText";
			params[2].ppTextures = &pSkyBoxTextures[i][2];
			params[3].pName = "BotText";
			params[3].ppTextures = &pSkyBoxTextures[i][3];
			params[4].pName = "FrontText";
			params[4].ppTextures = &pSkyBoxTextures[i][4];
			params[5].pName = "BackText";
			params[5].ppTextures = &pSkyBoxTextures[i][5];
			updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture[i], 6, params);

			for (uint32_t f = 0; f < gImageCount; ++f)
			{
				DescriptorData params[1] = {};
				params[0].pName = "uniformBlock";
				params[0].ppBuffers = &pSkyboxUniformBuffer[f];
				updateDescriptorSet(pRenderer, f * 2 + 0, pDescriptorSetUniforms[i], 1, params);
				params[0].ppBuffers = &pProjViewUniformBuffer[f];
				updateDescriptorSet(pRenderer, f * 2 + 1, pDescriptorSetUniforms[i], 1, params);
			}
		}

		CameraMotionParameters cmp{ 160.0f, 600.0f, 600.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
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
				bool capture = uiOnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		typedef bool (*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!uiIsFocused() && *ctx->pCaptured)
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		gFrameIndex = 0;

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		exitCameraController(pCameraController);

		exitProfiler();

		gPanini.Exit();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
			removeResource(pSkyboxUniformBuffer[i]);
		}

		for (uint32_t view = 0; view < gViewCount; ++view)
		{
			if (!gMultiGPU && view > 0)
				continue;

			removeDescriptorSet(pRenderer, pDescriptorSetTexture[view]);
			removeDescriptorSet(pRenderer, pDescriptorSetUniforms[view]);

			removeResource(pSphereVertexBuffer[view]);
			removeResource(pSkyBoxVertexBuffer[view]);

			for (uint i = 0; i < 6; ++i)
				removeResource(pSkyBoxTextures[view][i]);
		}

		removeSampler(pRenderer, pSamplerSkyBox);
		removeShader(pRenderer, pSphereShader);
		removeShader(pRenderer, pSkyBoxDrawShader);
		removeRootSignature(pRenderer, pRootSignature);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gViewCount; ++j)
			{
				removeFence(pRenderer, pRenderCompleteFences[i][j]);
				removeSemaphore(pRenderer, pRenderCompleteSemaphores[i][j]);
				removeCmd(pRenderer, pCmds[i][j]);
				removeCmdPool(pRenderer, pCmdPools[i][j]);
			}
		}

		for (uint32_t view = 0; view < gViewCount; ++view)
		{
			if (!gMultiGPU && view > 0)
				break;
			removeQueue(pRenderer, pGraphicsQueue[view]);
		}

		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		exitResourceLoaderInterface(pRenderer);

		exitRenderer(pRenderer);
		pRenderer = NULL; 

		if (!gMultiGPURestart)
		{
			// Need to free memory;
			tf_free(pSpherePoints);
			pSpherePoints = 0;
		}
	}

	bool Load()
	{
		gFrameIndex = 0;

		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		RenderTarget* ppPipelineRenderTargets[] = {
			pSwapChain->ppRenderTargets[0],
			pDepthBuffers[0]
		};

		if (!addFontSystemPipelines(ppPipelineRenderTargets, 2, NULL))
			return false;

		if (!addUserInterfacePipelines(ppPipelineRenderTargets[0]))
			return false;

		if (!gPanini.Load(pSwapChain->ppRenderTargets))
			return false;

		//layout and pipeline for sphere draw
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

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffers[0]->mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSphereShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pSpherePipeline);

		//layout and pipeline for skybox draw
		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		addPipeline(pRenderer, &desc, &pSkyBoxDrawPipeline);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t view = 0; view < gViewCount; ++view)
			{
				gPanini.SetSourceTexture(pRenderTargets[i][view]->pTexture, i * gViewCount + view);
			}
		}

		return true;
	}

	void Unload()
	{
		for (uint32_t i = 0; i < gViewCount; ++i)
			waitQueueIdle(pGraphicsQueue[i]);

		gPanini.Unload();
		removeUserInterfacePipelines();

		removeFontSystemPipelines(); 

		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pSpherePipeline);

		removeSwapChain(pRenderer, pSwapChain);
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			for (uint32_t i = 0; i < gViewCount; ++i)
				removeRenderTarget(pRenderer, pRenderTargets[frameIdx][i]);

		for (uint32_t i = 0; i < gViewCount; ++i)
			removeRenderTarget(pRenderer, pDepthBuffers[i]);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mHeight, mSettings.mHeight);

#if !defined(TARGET_IOS)
		if (pSwapChain->mEnableVsync != gToggleVSync)
		{
			waitQueueIdle(pGraphicsQueue[0]);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif
		/************************************************************************/
		// Update GUI
		/************************************************************************/
		static bool prevMultiGPU = gMultiGPU;
		if (prevMultiGPU != gMultiGPU)
		{
			bool temp = gMultiGPU;
			gMultiGPU = prevMultiGPU;
			gMultiGPURestart = true;

			Unload();
			Exit();

			gMultiGPU = temp;
			gMultiGPURestart = false;

			Init();
			Load();

			prevMultiGPU = gMultiGPU;
		}

		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		// update camera with time
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / ((float)mSettings.mWidth * 0.5f);
		const float horizontal_fov = gPaniniParams.FoVH * PI / 180.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat;

		// point light parameters
		gUniformData.mLightPosition = vec3(0, 0, 0);
		gUniformData.mLightColor = vec3(0.9f, 0.9f, 0.7f);    // Pale Yellow

		// update planet transformations
		for (int i = 0; i < gNumPlanets; i++)
		{
			mat4 rotSelf, rotOrbitY, rotOrbitZ, trans, scale, parentMat;
			rotSelf = rotOrbitY = rotOrbitZ = parentMat = mat4::identity();
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

		gPanini.SetParams(gPaniniParams);
		gPanini.Update(deltaTime);
	}

	void Draw()
	{
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(UniformBlock*)viewProjCbv.pMappedData = gUniformData;
		endUpdateResource(&viewProjCbv, NULL);

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex] };
		beginUpdateResource(&skyboxViewProjCbv);
		*(UniformBlock*)skyboxViewProjCbv.pMappedData = gUniformDataSky;
		endUpdateResource(&skyboxViewProjCbv, NULL);

		for (int i = gViewCount - 1; i >= 0; --i)
		{
			RenderTarget* pRenderTarget = pRenderTargets[gFrameIndex][i];
			RenderTarget* pDepthBuffer = pDepthBuffers[i];
			Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex][i];
			Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex][i];
			Cmd* cmd = pCmds[gFrameIndex][i];

			// simply record the screen cleaning command
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = gClearColor;
			loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
			loadActions.mClearDepth = gClearDepth;

			beginCmd(cmd);
			cmdBeginGpuFrameProfile(cmd, gGpuProfilerTokens[i]);

			RenderTargetBarrier barriers[] = {
				{ pRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);

			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			//// draw skybox
			const uint32_t skyboxStride = sizeof(float) * 4;
			cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[i], "Draw skybox");
			cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture[i]);
			cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 0, pDescriptorSetUniforms[i]);
			cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer[i], &skyboxStride, NULL);
			cmdDraw(cmd, 36, 0);
			cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[i]);

			////// draw planets
			const uint32_t sphereStride = sizeof(float) * 6;
			cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[i], "Draw Planets");
			cmdBindPipeline(cmd, pSpherePipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 1, pDescriptorSetUniforms[i]);
			cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer[i], &sphereStride, NULL);
			cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, gNumPlanets, 0);
			cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[i]);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			RenderTargetBarrier srvBarriers[] = {
				{ pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, srvBarriers);

			if (i == 0)
			{
				cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[i], "Draw Results");
				loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

				RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
				RenderTargetBarrier barriers[] = { { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET } };
				cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

				cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

				cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[i], "Panini Projection");

				cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth * 0.5f, (float)mSettings.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
				gPanini.Draw(cmd);

				cmdSetViewport(
					cmd, (float)mSettings.mWidth * 0.5f, 0.0f, (float)mSettings.mWidth * 0.5f, (float)mSettings.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
				gPanini.Draw(cmd);

				cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[i]);

				cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);

				const float txtIndentY = 12.f;
				float txtOrigY = txtIndentY;
				float2 screenCoords = float2(8.0f, txtOrigY);

				gFrameTimeDraw.mFontColor = 0xff00ffff;
				gFrameTimeDraw.mFontSize = 18.0f;
				gFrameTimeDraw.mFontID = gFontID;
				float2 txtSize = cmdDrawCpuProfile(cmd, screenCoords, &gFrameTimeDraw);

				txtOrigY += txtSize.y + txtIndentY;
				for (uint32_t j = 0; j < gViewCount; ++j)
				{
					screenCoords = float2(8.f, txtOrigY);
					txtSize = cmdDrawGpuProfile(cmd, screenCoords, gGpuProfilerTokens[j], &gFrameTimeDraw);
					txtOrigY += txtSize.y + txtIndentY;
				}

				cmdDrawUserInterface(cmd);

				cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

				barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
				cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
				cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[i]);
			}

			cmdEndGpuFrameProfile(cmd, gGpuProfilerTokens[i]);
			endCmd(cmd);

			if (i == 0)
			{
				Semaphore* pWaitSemaphores[] = { pImageAcquiredSemaphore, pRenderCompleteSemaphores[gFrameIndex][1] };

				QueueSubmitDesc submitDesc = {};
				submitDesc.mCmdCount = 1;
				submitDesc.ppCmds = &cmd;
				submitDesc.pSignalFence = pRenderCompleteFence;
				submitDesc.mSignalSemaphoreCount = 1;
				submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
				submitDesc.mWaitSemaphoreCount = 1;
				submitDesc.ppWaitSemaphores = pWaitSemaphores;
				queueSubmit(pGraphicsQueue[i], &submitDesc);
				QueuePresentDesc presentDesc = {};
				presentDesc.mIndex = swapchainImageIndex;
				presentDesc.mWaitSemaphoreCount = 1;
				presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
				presentDesc.pSwapChain = pSwapChain;
				presentDesc.mSubmitDone = true;
				queuePresent(pGraphicsQueue[i], &presentDesc);
				flipProfiler();
			}
			else
			{
				QueueSubmitDesc submitDesc = {};
				submitDesc.mCmdCount = 1;
				submitDesc.ppCmds = &cmd;
				submitDesc.pSignalFence = pRenderCompleteFence;
				submitDesc.mSignalSemaphoreCount = 1;
				submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
				queueSubmit(pGraphicsQueue[i], &submitDesc);
			}
		}

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount][i];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pNextFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			{
				waitForFences(pRenderer, 1, &pNextFence);
			}

			resetCmdPool(pRenderer, pCmdPools[(gFrameIndex + 1) % gImageCount][i]);
		}
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "11_MultiGPU"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue[0];
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
		// Add color buffer
		RenderTargetDesc colorRT = {};
		colorRT.mArraySize = 1;
		colorRT.mClearValue = gClearColor;
		colorRT.mDepth = 1;
		colorRT.mFormat = getRecommendedSwapchainFormat(true);
		colorRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		colorRT.mHeight = mSettings.mHeight;
		colorRT.mSampleCount = SAMPLE_COUNT_1;
		colorRT.mSampleQuality = 0;
		colorRT.mWidth = mSettings.mWidth / 2;

		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = gClearDepth;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D16_UNORM;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth / 2;

		uint32_t sharedIndices[] = { 0 };

		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			if (gMultiGPU)
			{
				colorRT.mNodeIndex = i;
				depthRT.mNodeIndex = i;

				if (i > 0)
				{
					colorRT.pSharedNodeIndices = sharedIndices;
					colorRT.mSharedNodeIndexCount = 1;
				}
			}

			addRenderTarget(pRenderer, &depthRT, &pDepthBuffers[i]);

			for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			{
				addRenderTarget(pRenderer, &colorRT, &pRenderTargets[frameIdx][i]);
			}
		}

		return pDepthBuffers[0] != NULL;
	}
};

DEFINE_APPLICATION_MAIN(MultiGPU)
