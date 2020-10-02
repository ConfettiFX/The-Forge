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

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/utility.h"


//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

#include "../../../../Common_3/OS/Interfaces/IInput.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"


/// Demo structures

struct UniformBlock
{
	mat4 mProjectView;
	mat4 mModelMatCapsule;
	mat4 mModelMatCube;
};

struct Vertex
{
	float3 mPosition;
	float3 mNormal;
	float2 mUV;
};

struct MeshData
{
	Buffer* pVertexBuffer = NULL;
	uint    mVertexCount = 0;
	Buffer* pIndexBuffer = NULL;
	uint    mIndexCount = 0;
};

const uint32_t gImageCount = 3;
ProfileToken   gGpuProfileToken;
Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd* pCmds[gImageCount];

SwapChain* pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader* pBasicShader = NULL;
Pipeline* pBasicPipeline = NULL;

Shader* pSkyboxShader = NULL;
Buffer* pSkyboxVertexBuffer = NULL;
Pipeline* pPipelineSkybox = NULL;
RootSignature* pRootSignature = NULL;
Sampler* pSamplerSkybox = NULL;
Texture* pSkyboxTextures[6];

VirtualJoystickUI gVirtualJoystick;

//Zip File Test Texture
Texture* pZipTexture[1];
Shader* pZipTextureShader = NULL;
Buffer* pZipTextureVertexBuffer = NULL;
Pipeline* pZipTexturePipeline = NULL;

Buffer* pProjViewUniformBuffer[gImageCount] = { NULL };

DescriptorSet* pDescriptorSetFrameUniforms = NULL;
DescriptorSet* pDescriptorSetTextures = NULL;

uint32_t gFrameIndex = 0;

int					gNumberOfBasicPoints;
int					gNumberOfCubiodPoints;
UniformBlock		gUniformData;

ICameraController* pCameraController = NULL;

/// UI
UIApp gAppUI;

const char* pSkyboxImageFileNames[] = { "Skybox/Skybox_right1",  "Skybox/Skybox_left2",  "Skybox/Skybox_top3",
										"Skybox/Skybox_bottom4", "Skybox/Skybox_front5", "Skybox/Skybox_back6" };

const char* pCubeTextureName[] = { "Test_2" };

const char* pTextFileName[] = { "TestDoc.txt" };

const char* pModelFileName[] = { "capsule.gltf" };

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

GuiComponent* pGui_TextData = NULL;

GuiComponent* pGui_ZipData = NULL;

//Zip file for testing
const char* pZipFiles = "28-ZipFileSystem.zip";

eastl::vector<eastl::string> gTextDataVector;

//structures for loaded model 
Geometry* pMesh;
VertexLayout gVertexLayoutDefault = {};

const char* pMTunerOut = "testout.txt";

bool fsOpenZipFile(const ResourceDirectory resourceDir, const char* fileName, FileMode mode, IFileSystem* pOut);
bool fsCloseZipFile(IFileSystem* pZip);

class FileSystemUnitTest : public IApp
{
public:

	bool Init()
	{
		ResourceDirectory RD_ZIP_TEXT = RD_MIDDLEWARE_3;

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "ZipFiles");

		IFileSystem zipFile = {};
		if (!fsOpenZipFile(RD_OTHER_FILES, pZipFiles, FM_READ, &zipFile))
		{
			ASSERT("Failed to Open zip file");
			return false;
		}

		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES,"CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,     "GPUCfg");
		fsSetPathForResourceDir(&zipFile,      RM_CONTENT, RD_ZIP_TEXT,       "");
		fsSetPathForResourceDir(&zipFile,      RM_CONTENT, RD_TEXTURES,       "Textures");
		fsSetPathForResourceDir(&zipFile,      RM_CONTENT, RD_MESHES,         "Meshes");
		fsSetPathForResourceDir(&zipFile,      RM_CONTENT, RD_FONTS,          "Fonts");

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

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Initialize microprofiler and its UI.
		initProfiler();

		// Gpu profiler can only be added after initProfile.
		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

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
		gVertexLayoutDefault.mAttribs[2].mOffset = 6 * sizeof(float);

		FileStream textFile0Handle = {};
		if (!fsOpenStreamFromPath(RD_ZIP_TEXT, pTextFileName[0], FM_READ, &textFile0Handle))
		{
			LOGF(LogLevel::eERROR, "\"%s\": ERROR in searching for file.", pTextFileName[0]);
			return false;
		}

		ssize_t textFile0Size = fsGetStreamFileSize(&textFile0Handle);
		char* pDataOfFile = (char*)tf_malloc((textFile0Size + 1) * sizeof(char));
		ssize_t bytesRead = fsReadFromStream(&textFile0Handle, pDataOfFile, textFile0Size);
		fsCloseStream(&textFile0Handle);

		if (bytesRead != textFile0Size)
		{
			LOGF(LogLevel::eERROR, "\"%s\": Error in reading file.", pTextFileName[0]);
			return false;
		}
		pDataOfFile[textFile0Size] = 0;
		gTextDataVector.clear();
		gTextDataVector.push_back(pDataOfFile);

		//Free the data buffer which was malloc'ed
		if (pDataOfFile != NULL)
		{
			tf_free(pDataOfFile);
		}

		//Load Zip file texture
		TextureLoadDesc textureDescZip = {};
		textureDescZip.pFileName = pCubeTextureName[0];
		textureDescZip.ppTexture = &pZipTexture[0];
		addResource(&textureDescZip, NULL);

		// Loads Skybox Textures
		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.pFileName = pSkyboxImageFileNames[i];
			textureDesc.ppTexture = &pSkyboxTextures[i];
			addResource(&textureDesc, NULL);
		}

		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = pModelFileName[0];
		loadDesc.ppGeometry = &pMesh;
		loadDesc.pVertexLayout = &gVertexLayoutDefault;
		addResource(&loadDesc, NULL);

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0 };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0 };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0 };
		basicShader.mStages[1] = { "basic.frag", NULL, 0 };
		ShaderLoadDesc zipTextureShader = {};
		zipTextureShader.mStages[0] = { "zipTexture.vert", NULL, 0 };
		zipTextureShader.mStages[1] = { "zipTexture.frag", NULL, 0 };

		addShader(pRenderer, &skyShader, &pSkyboxShader);
		addShader(pRenderer, &basicShader, &pBasicShader);
		addShader(pRenderer, &zipTextureShader, &pZipTextureShader);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSamplerSkybox);

		Shader* shaders[] = { pSkyboxShader, pBasicShader, pZipTextureShader };

		const char* pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = pStaticSamplers;
		skyboxRootDesc.ppStaticSamplers = &pSamplerSkybox;
		skyboxRootDesc.mShaderCount = 3;
		skyboxRootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignature);

		// Generate Cuboid Vertex Buffer

		float widthCube = 1.0f;
		float heightCube = 1.0f;
		float depthCube = 1.0f;

		float CubePoints[] =
		{
			//Position				        //Normals				//TexCoords
			-widthCube, -heightCube, -depthCube  ,	 0.0f,  0.0f, -1.0f,    0.0f, 0.0f,
			 widthCube, -heightCube, -depthCube  ,	 0.0f,  0.0f, -1.0f,    1.0f, 0.0f,
			 widthCube,  heightCube, -depthCube  ,	 0.0f,  0.0f, -1.0f,    1.0f, 1.0f,
			 widthCube,  heightCube, -depthCube  ,	 0.0f,  0.0f, -1.0f,    1.0f, 1.0f,
			-widthCube,  heightCube, -depthCube  ,	 0.0f,  0.0f, -1.0f,    0.0f, 1.0f,
			-widthCube, -heightCube, -depthCube  ,	 0.0f,  0.0f, -1.0f,    0.0f, 0.0f,

			-widthCube, -heightCube,  depthCube  ,	 0.0f,  0.0f,  1.0f,	0.0f, 0.0f,
			 widthCube, -heightCube,  depthCube  ,	 0.0f,  0.0f,  1.0f,	1.0f, 0.0f,
			 widthCube,  heightCube,  depthCube  ,	 0.0f,  0.0f,  1.0f,	1.0f, 1.0f,
			 widthCube,  heightCube,  depthCube  ,	 0.0f,  0.0f,  1.0f,	1.0f, 1.0f,
			-widthCube,  heightCube,  depthCube  ,	 0.0f,  0.0f,  1.0f,	0.0f, 1.0f,
			-widthCube, -heightCube,  depthCube  ,	 0.0f,  0.0f,  1.0f,	0.0f, 0.0f,

			-widthCube,  heightCube,  depthCube  ,	-1.0f,  0.0f,  0.0f,	1.0f, 0.0f,
			-widthCube,  heightCube, -depthCube  ,	-1.0f,  0.0f,  0.0f,	1.0f, 1.0f,
			-widthCube, -heightCube, -depthCube  ,	-1.0f,  0.0f,  0.0f,	0.0f, 1.0f,
			-widthCube, -heightCube, -depthCube  ,	-1.0f,  0.0f,  0.0f,	0.0f, 1.0f,
			-widthCube, -heightCube,  depthCube  ,	-1.0f,  0.0f,  0.0f,	0.0f, 0.0f,
			-widthCube,  heightCube,  depthCube  ,	-1.0f,  0.0f,  0.0f,	1.0f, 0.0f,

			 widthCube,  heightCube,  depthCube  ,  	1.0f,  0.0f,  0.0f,		1.0f, 0.0f,
			 widthCube,  heightCube, -depthCube  ,  	1.0f,  0.0f,  0.0f,		1.0f, 1.0f,
			 widthCube, -heightCube, -depthCube  ,  	1.0f,  0.0f,  0.0f,		0.0f, 1.0f,
			 widthCube, -heightCube, -depthCube  ,  	1.0f,  0.0f,  0.0f,		0.0f, 1.0f,
			 widthCube, -heightCube,  depthCube  ,  	1.0f,  0.0f,  0.0f,		0.0f, 0.0f,
			 widthCube,  heightCube,  depthCube  ,  	1.0f,  0.0f,  0.0f,		1.0f, 0.0f,

			-widthCube, -heightCube, -depthCube  ,	 0.0f,  -1.0f,  0.0f,	0.0f, 1.0f,
			 widthCube, -heightCube, -depthCube  ,	 0.0f,  -1.0f,  0.0f,	1.0f, 1.0f,
			 widthCube, -heightCube,  depthCube  ,	 0.0f,  -1.0f,  0.0f,	1.0f, 0.0f,
			 widthCube, -heightCube,  depthCube  ,	 0.0f,  -1.0f,  0.0f,	1.0f, 0.0f,
			-widthCube, -heightCube,  depthCube  ,	 0.0f,  -1.0f,  0.0f,	0.0f, 0.0f,
			-widthCube, -heightCube, -depthCube  ,	 0.0f,  -1.0f,  0.0f,	0.0f, 1.0f,

			-widthCube,  heightCube, -depthCube  ,	 0.0f,  1.0f,  0.0f,	0.0f, 1.0f,
			 widthCube,  heightCube, -depthCube  ,	 0.0f,  1.0f,  0.0f,	1.0f, 1.0f,
			 widthCube,  heightCube,  depthCube  ,	 0.0f,  1.0f,  0.0f,	1.0f, 0.0f,
			 widthCube,  heightCube,  depthCube  ,	 0.0f,  1.0f,  0.0f,	1.0f, 0.0f,
			-widthCube,  heightCube,  depthCube  ,	 0.0f,  1.0f,  0.0f,	0.0f, 0.0f,
			-widthCube,  heightCube, -depthCube  ,	 0.0f,  1.0f,  0.0f,	0.0f, 1.0f
		};

		uint64_t       cubiodDataSize = 288 * sizeof(float);
		BufferLoadDesc cubiodVbDesc = {};
		cubiodVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		cubiodVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		cubiodVbDesc.mDesc.mSize = cubiodDataSize;
		cubiodVbDesc.pData = CubePoints;
		cubiodVbDesc.ppBuffer = &pZipTextureVertexBuffer;
		addResource(&cubiodVbDesc, NULL);

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
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc, NULL);

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
		}
		waitForAllResourceLoads();

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.15f);

		//--------------------------------

		//Gui for Showing the Text of the File
		pGui_TextData = gAppUI.AddGuiComponent("Opened Document", &guiDesc);
		pGui_TextData->AddWidget(LabelWidget(gTextDataVector[0]));

		//--------------------------------



		//CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);
		//requestMouseCapture(true);

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
		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
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

		// Close the Zip file
		fsCloseZipFile(&zipFile);

		return true;
	}

	void Exit()
	{
		gTextDataVector.clear();
		gTextDataVector.set_capacity(0);

		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		destroyCameraController(pCameraController);

		gVirtualJoystick.Exit();

		gAppUI.Exit();

		// Exit profile
		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
		}

		removeResource(pSkyboxVertexBuffer);
		removeResource(pZipTextureVertexBuffer);

		for (uint i = 0; i < 6; ++i)
			removeResource(pSkyboxTextures[i]);

		//remove loaded zip test texture
		removeResource(pZipTexture[0]);

		//remove loaded zip test models
		removeResource(pMesh);

		removeSampler(pRenderer, pSamplerSkybox);
		removeShader(pRenderer, pBasicShader);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pZipTextureShader);
		removeRootSignature(pRenderer, pRootSignature);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		removeRenderer(pRenderer);
	}


	void CreateDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTextures);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFrameUniforms);
	}

	void DestroyDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetTextures);
		removeDescriptorSet(pRenderer, pDescriptorSetFrameUniforms);
	}

	void PrepareDescriptorSets()
	{
		// Skybox
		{
			// Prepare descriptor sets
			DescriptorData params[7] = {};
			params[0].pName = "RightText";
			params[0].ppTextures = &pSkyboxTextures[0];
			params[1].pName = "LeftText";
			params[1].ppTextures = &pSkyboxTextures[1];
			params[2].pName = "TopText";
			params[2].ppTextures = &pSkyboxTextures[2];
			params[3].pName = "BotText";
			params[3].ppTextures = &pSkyboxTextures[3];
			params[4].pName = "FrontText";
			params[4].ppTextures = &pSkyboxTextures[4];
			params[5].pName = "BackText";
			params[5].ppTextures = &pSkyboxTextures[5];

			params[6].pName = "ZipTexture";
			params[6].ppTextures = pZipTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetTextures, 7, params);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[1] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pProjViewUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetFrameUniforms, 1, params);
		}
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

		//layout and pipeline for zip model draw
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc cubeRasterizerStateDesc = {};
		cubeRasterizerStateDesc = {};
		cubeRasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc sphereRasterizerStateDesc = {};
		sphereRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

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
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pBasicShader;
		pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
		pipelineSettings.pRasterizerState = &sphereRasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pBasicPipeline);

		//layout and pipeline for skybox draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pShaderProgram = pSkyboxShader;
		addPipeline(pRenderer, &desc, &pPipelineSkybox);


		//layout and pipeline for the zip test texture

		vertexLayout = {};
		vertexLayout.mAttribCount = 3;
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
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[2].mBinding = 0;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pRasterizerState = &cubeRasterizerStateDesc;
		pipelineSettings.pShaderProgram = pZipTextureShader;
		addPipeline(pRenderer, &desc, &pZipTexturePipeline);

		CreateDescriptorSets();
		PrepareDescriptorSets();

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);
		unloadProfilerUI();
		gAppUI.Unload();

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Unload();
#endif
		DestroyDescriptorSets();

		removePipeline(pRenderer, pZipTexturePipeline);
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pBasicPipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);

		/************************************************************************/
		// Scene Update
		/****************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		// Projection and View Matrix
		gUniformData.mProjectView = projMat * viewMat;

		//Model Matrix
		mat4 trans = mat4::translation(vec3(15.0f, 0.0f, 22.0f));
		mat4 scale = mat4::scale(vec3(5.0f));
		gUniformData.mModelMatCapsule = trans * scale;


		//********************************
		//Uniform buffer data of the cube with zip texture
		//********************************

		mat4  mTranslationMat_Zip;
		mat4  mScaleMat_Zip;

		mTranslationMat_Zip = mat4::translation(vec3(10.5f, 1.0f, 3.0f));
		mScaleMat_Zip = mat4::scale(vec3(10.5f));
		gUniformData.mModelMatCube = mTranslationMat_Zip * mScaleMat_Zip;

		viewMat.setTranslation(vec3(0));

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
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(UniformBlock*)viewProjCbv.pMappedData = gUniformData;
		endUpdateResource(&viewProjCbv, NULL);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		//// draw skybox
#pragma region Skybox_Draw
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw skybox");
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
		cmdBindPipeline(cmd, pPipelineSkybox);

		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetFrameUniforms);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTextures);

		const uint32_t skyboxStride = sizeof(float) * 4;
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &skyboxStride, NULL);
		cmdDraw(cmd, 36, 0);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
#pragma endregion

		////// draw Zip Model
#pragma region Zip_Model_Draw
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Zip Model");
		cmdBindPipeline(cmd, pBasicPipeline);

		cmdBindVertexBuffer(cmd, 1, &pMesh->pVertexBuffers[0], &pMesh->mVertexStrides[0], NULL);
		cmdBindIndexBuffer(cmd, pMesh->pIndexBuffer, pMesh->mIndexType, NULL);
		cmdDrawIndexed(cmd, pMesh->mIndexCount, 0, 0);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
#pragma endregion


		////draw Cube with Zip texture
#pragma region Cube_Zip_Texture_Draw
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Zip File Texture");
		cmdBindPipeline(cmd, pZipTexturePipeline);

		const uint32_t cubeStride = sizeof(float) * 8;
		cmdBindVertexBuffer(cmd, 1, &pZipTextureVertexBuffer, &cubeStride, NULL);
		cmdDraw(cmd, 36, 0);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
#pragma endregion

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");
		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

			gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

#if !defined(__ANDROID__)
			float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
			cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 30.f), gGpuProfileToken);
#else
			cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
#endif

			gAppUI.Gui(pGui_TextData);

			cmdDrawProfilerUI();

			gAppUI.Draw(cmd);
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		}
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

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

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "12_ZipFileSystem"; }

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
		swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
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

DEFINE_APPLICATION_MAIN(FileSystemUnitTest)
