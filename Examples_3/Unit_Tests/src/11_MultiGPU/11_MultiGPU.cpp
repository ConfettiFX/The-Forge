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

// Unit Test for distributing heavy gpu workload such as Split Frame Rendering to Multiple Identical GPUs
// GPU 0 Renders Left Eye and finally does a composition pass to present Left and Right eye textures to screen
// GPU 1 Renders Right Eye

#define MAX_PLANETS 20    // Does not affect test, just for allocating space in uniform block. Must match with shader.

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Middleware_3/PaniniProjection/Panini.h"

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

const uint32_t gImageCount = 3;
bool           gMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

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

Queue*        pGraphicsQueue[gViewCount] = { NULL };
CmdPool*      pCmdPool[gViewCount] = { NULL };
Cmd**         ppCmds[gViewCount] = { NULL };
Fence*        pRenderCompleteFences[gImageCount][gViewCount] = { { NULL } };
Semaphore*    pRenderCompleteSemaphores[gImageCount][gViewCount] = {{ NULL }};
Buffer*       pSphereVertexBuffer[gViewCount] = { NULL };
Buffer*       pSkyBoxVertexBuffer[gViewCount] = { NULL };
Texture*      pSkyBoxTextures[gViewCount][6];
GpuProfiler*  pGpuProfilers[gViewCount] = { NULL };
RenderTarget* pRenderTargets[gImageCount][gViewCount] = {{ NULL }};
RenderTarget* pDepthBuffers[gViewCount] = { NULL };

Semaphore* pImageAcquiredSemaphore = NULL;
SwapChain* pSwapChain = NULL;

Shader*   pSphereShader = NULL;
Pipeline* pSpherePipeline = NULL;

Shader*           pSkyBoxDrawShader = NULL;
Pipeline*         pSkyBoxDrawPipeline = NULL;
RootSignature*    pRootSignature = NULL;
Sampler*          pSamplerSkyBox = NULL;
DescriptorSet*    pDescriptorSetTexture[gViewCount] = { NULL };
DescriptorSet*    pDescriptorSetUniforms[gViewCount] = { NULL };

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

const char* pSkyBoxImageFileNames[] = { "Skybox_right1",  "Skybox_left2",  "Skybox_top3",
										"Skybox_bottom4", "Skybox_front5", "Skybox_back6" };

TextDrawDesc     gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
ClearValue       gClearColor; // initialization in Init
ClearValue       gClearDepth;
Panini           gPanini = {};
PaniniParameters gPaniniParams = {};
bool             gMultiGPU = true;
bool             gMultiGPURestart = false;
float*           pSpherePoints;

class MultiGPU: public IApp
{
public:
	bool Init()
	{
		// file paths
		PathHandle programDirectory = fsCopyProgramDirectoryPath();
		FileSystem* fileSystem = fsGetPathFileSystem(programDirectory);
		if (!fsPlatformUsesBundledResources())
		{
			PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/11_MultiGPU");
			fsSetResourceDirectoryRootPath(resourceDirRoot);

			fsSetRelativePathForResourceDirectory(RD_TEXTURES, "../../UnitTestResources/Textures");
			fsSetRelativePathForResourceDirectory(RD_MESHES, "../../UnitTestResources/Meshes");
			fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS, "../../UnitTestResources/Fonts");
			fsSetRelativePathForResourceDirectory(RD_ANIMATIONS, "../../UnitTestResources/Animation");
			fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT, "../../../../Middleware_3/Text");
			fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI, "../../../../Middleware_3/UI");
#if !defined(TARGET_IOS)
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_PANINI,  "../../../../Middleware_3/PaniniProjection");
#endif
		}

		gClearColor.r = 0.0f;
		gClearColor.g = 0.0f;
		gClearColor.b = 0.0f;
		gClearColor.a = 0.0f;

		gClearDepth.depth = 1.0f;
		gClearDepth.stencil = 0;

		// window and renderer setup
		RendererDesc settings = { 0 };
		settings.mGpuMode = gMultiGPU ? GPU_MODE_LINKED : GPU_MODE_SINGLE;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		initResourceLoaderInterface(pRenderer);

		if (pRenderer->mSettings.mGpuMode == GPU_MODE_SINGLE && gMultiGPU)
		{
			LOGF(LogLevel::eWARNING, "Multi GPU will be disabled since the system only has one GPU");
			gMultiGPU = false;
		}

		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			QueueDesc queueDesc = {};
			queueDesc.mType = CMD_POOL_DIRECT;
			queueDesc.mNodeIndex = i;

			if (!gMultiGPU && i > 0)
				pGraphicsQueue[i] = pGraphicsQueue[0];
			else
				addQueue(pRenderer, &queueDesc, &pGraphicsQueue[i]);
		}

    if (!gAppUI.Init(pRenderer))
      return false;

    gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);
    GuiDesc guiDesc = {};
    pGui = gAppUI.AddGuiComponent(GetName(), &guiDesc);

    initProfiler();
		char gpu_profile_name[16] = { 0 };

		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			addCmdPool(pRenderer, pGraphicsQueue[i], false, &pCmdPool[i]);
			addCmd_n(pCmdPool[i], false, gImageCount, &ppCmds[i]);

			for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			{
				addFence(pRenderer, &pRenderCompleteFences[frameIdx][i]);
				addSemaphore(pRenderer, &pRenderCompleteSemaphores[frameIdx][i]);
			}
			sprintf(gpu_profile_name, "GpuProfiler%d", i);
			addGpuProfiler(pRenderer, pGraphicsQueue[i], &pGpuProfilers[i], gpu_profile_name);
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, RD_SHADER_SOURCES };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, RD_SHADER_SOURCES };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, RD_SHADER_SOURCES };

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

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pSkyboxRast);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		// Generate sphere vertex buffer
		if (!pSpherePoints)
			generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution, gSphereDiameter);

		uint64_t       sphereDataSize = gNumberOfSpherePoints * sizeof(float);
		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mSize = sphereDataSize;
		sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
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
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;

		TextureLoadDesc textureDesc = {};

		for (uint32_t view = 0; view < gViewCount; ++view)
		{
			textureDesc.mNodeIndex = view;

			for (int i = 0; i < 6; ++i)
			{
				PathHandle filePath = fsCopyPathInResourceDirectory(RD_TEXTURES, pSkyBoxImageFileNames[i]);
				textureDesc.pFilePath = filePath;
				textureDesc.ppTexture = &pSkyBoxTextures[view][i];

				if (!gMultiGPU && view > 0)
					pSkyBoxTextures[view][i] = pSkyBoxTextures[0][i];
				else
					addResource(&textureDesc);
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
				addResource(&sphereVbDesc);
				addResource(&skyboxVbDesc);
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

		pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &gMicroProfiler));

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGui->AddWidget(CheckboxWidget("Toggle VSync", &gToggleVSync));
#endif

		pGui->AddWidget(CheckboxWidget("Enable Multi GPU", &gMultiGPU));
		pGui->AddWidget(SliderFloatWidget("Camera Horizontal FoV", &gPaniniParams.FoVH, 30.0f, 179.0f, 1.0f));
		pGui->AddWidget(SliderFloatWidget("Panini D Parameter", &gPaniniParams.D, 0.0f, 1.0f, 0.001f));
		pGui->AddWidget(SliderFloatWidget("Panini S Parameter", &gPaniniParams.S, 0.0f, 1.0f, 0.001f));
		pGui->AddWidget(SliderFloatWidget("Screen Scale", &gPaniniParams.scale, 1.0f, 10.0f, 0.01f));

		CameraMotionParameters cmp{ 160.0f, 600.0f, 600.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		if (!gPanini.Init(pRenderer))
			return false;
		
		gPanini.SetMaxDraws(gImageCount * 2);

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
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);
		
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

		return true;
	}

	void Exit()
	{
		for (uint32_t i = 0; i < gViewCount; ++i)
			waitQueueIdle(pGraphicsQueue[i]);

		exitInputSystem();

		destroyCameraController(pCameraController);

		exitProfiler();

		if (!gMultiGPURestart)
		{
			// Need to free memory;
			conf_free(pSpherePoints);
		}

		gPanini.Exit();
		gAppUI.Exit();

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

		removeDepthState(pDepth);
		removeRasterizerState(pSkyboxRast);

		for (uint32_t view = 0; view < gViewCount; ++view)
		{
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				removeFence(pRenderer, pRenderCompleteFences[i][view]);
				removeSemaphore(pRenderer, pRenderCompleteSemaphores[i][view]);
			}

			removeCmd_n(pCmdPool[view], gImageCount, ppCmds[view]);
			removeCmdPool(pRenderer, pCmdPool[view]);
			removeGpuProfiler(pRenderer, pGpuProfilers[view]);
		}

		for (uint32_t view = 0; view < gViewCount; ++view)
		{
			if (!gMultiGPU && view > 0)
				break;
			removeQueue(pGraphicsQueue[view]);
		}

		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeResourceLoaderInterface(pRenderer);

		removeRenderer(pRenderer);
	}

	bool Load()
	{
		gFrameIndex = 0;

		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		if (!gPanini.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

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

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffers[0]->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSphereShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pSkyboxRast;
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
		pipelineSettings.pRasterizerState = pSkyboxRast;
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

		unloadProfiler();
		gPanini.Unload();
		gAppUI.Unload();

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

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
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
		/************************************************************************/
		/************************************************************************/
		// ProfileSetDisplayMode()
		// TODO: need to change this better way 
		if (gMicroProfiler != bPrevToggleMicroProfiler)
		{
		  toggleProfiler();
		  bPrevToggleMicroProfiler = gMicroProfiler;
		}
		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);

		gPanini.SetParams(gPaniniParams);
		gPanini.Update(deltaTime);
	}

	void Draw()
	{
		static HiresTimer gTimer;

		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&viewProjCbv);

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex], &gUniformDataSky };
		updateResource(&skyboxViewProjCbv);

		for (int i = gViewCount - 1; i >= 0; --i)
		{
			RenderTarget* pRenderTarget = pRenderTargets[gFrameIndex][i];
			RenderTarget* pDepthBuffer = pDepthBuffers[i];
			Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex][i];
			Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex][i];
			Cmd*          cmd = ppCmds[i][gFrameIndex];
			GpuProfiler*  pGpuProfiler = pGpuProfilers[i];

			// simply record the screen cleaning command
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = gClearColor;
			loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
			loadActions.mClearDepth = gClearDepth;

			beginCmd(cmd);
			cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

			TextureBarrier barriers[] = {
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 2, barriers);
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);

			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			//// draw skybox
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw skybox", true);
			cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture[i]);
			cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 0, pDescriptorSetUniforms[i]);
			cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer[i], NULL);
			cmdDraw(cmd, 36, 0);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

			////// draw planets
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Planets", true);
			cmdBindPipeline(cmd, pSpherePipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 1, pDescriptorSetUniforms[i]);
			cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer[i], NULL);
			cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, gNumPlanets, 0);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			TextureBarrier srvBarriers[] = {
				{ pRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers);

			if (i == 0)
			{
				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Results");
				loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

				RenderTarget*  pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
				TextureBarrier barriers[1 + gViewCount] = {};
				for (uint32_t i = 0; i < gViewCount; ++i)
					barriers[i] = { pRenderTargets[gFrameIndex][i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
				barriers[gViewCount] = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
				cmdResourceBarrier(cmd, 0, NULL, 1 + gViewCount, barriers);

				cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Panini Projection");

				cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth * 0.5f, (float)mSettings.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
				gPanini.Draw(cmd);

				cmdSetViewport(
					cmd, (float)mSettings.mWidth * 0.5f, 0.0f, (float)mSettings.mWidth * 0.5f, (float)mSettings.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
				gPanini.Draw(cmd);

				cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

				cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);

				gAppUI.Gui(pGui);

				gAppUI.DrawText(
					cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

				if (gMultiGPU)
				{
					gAppUI.DrawText(
						cmd, float2(8, 40),
						eastl::string()
							.sprintf("GPU %f ms", max(pGpuProfilers[0]->mCumulativeTime, pGpuProfilers[1]->mCumulativeTime) * 1000.0)
							.c_str(),
						&gFrameTimeDraw);

					gAppUI.DrawText(
						cmd, float2(8, 75), eastl::string().sprintf("First GPU %f ms", pGpuProfilers[0]->mCumulativeTime * 1000.0).c_str(),
						&gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8, 100), pGpuProfilers[0], NULL);

					gAppUI.DrawText(
						cmd, float2(8, 275),
						eastl::string().sprintf("Second GPU %f ms", pGpuProfilers[1]->mCumulativeTime * 1000.0).c_str(), &gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8, 300), pGpuProfilers[1], NULL);
				}
				else
				{
					gAppUI.DrawText(
						cmd, float2(8, 40),
						eastl::string()
							.sprintf("GPU %f ms", (pGpuProfilers[0]->mCumulativeTime + pGpuProfilers[1]->mCumulativeTime) * 1000.0)
							.c_str(),
						&gFrameTimeDraw);

					gAppUI.DrawText(
						cmd, float2(8, 75), eastl::string().sprintf("First CMD %f ms", pGpuProfilers[0]->mCumulativeTime * 1000.0).c_str(),
						&gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8, 100), pGpuProfilers[0], NULL);

					gAppUI.DrawText(
						cmd, float2(8, 275),
						eastl::string().sprintf("Second CMD %f ms", pGpuProfilers[1]->mCumulativeTime * 1000.0).c_str(), &gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8, 300), pGpuProfilers[1], NULL);
				}

				cmdDrawProfiler();

				gAppUI.Draw(cmd);

				cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

				barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
				cmdResourceBarrier(cmd, 0, NULL, 1, barriers);
				cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
			}

			cmdEndGpuFrameProfile(cmd, pGpuProfiler);
			endCmd(cmd);

			if (i == 0)
			{
				Semaphore* pWaitSemaphores[] = { pImageAcquiredSemaphore, pRenderCompleteSemaphores[gFrameIndex][1] };
				queueSubmit(pGraphicsQueue[i], 1, &cmd, pRenderCompleteFence, 2, pWaitSemaphores, 1, &pRenderCompleteSemaphore);
			}
			else
			{
				queueSubmit(pGraphicsQueue[i], 1, &cmd, pRenderCompleteFence, 0, NULL, 1, &pRenderCompleteSemaphore);
			}
		}

		queuePresent(pGraphicsQueue[0], pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphores[gFrameIndex][0]);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		for (uint32_t i = 0; i < gViewCount; ++i)
		{
			Fence*      pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount][i];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pNextFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			{
				waitForFences(pRenderer, 1, &pNextFence);
			}
		}
		flipProfiler();

		gTimer.GetUSec(true);
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
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
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
