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

// Unit Test for testing transformations using a solar system.
// Tests the basic mat4 transformations, such as scaling, rotation, and translation.

//Assimp Related
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "../../../../Common_3/Tools/AssetPipeline/src/TFXImporter.h"
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
#include "../../../../Common_3/Renderer/ResourceLoader.h"

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
bool           gMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pBasicShader = NULL;
Pipeline* pBasicPipeline = NULL;

Shader*        pSkyboxShader = NULL;
Buffer*        pSkyboxVertexBuffer = NULL;
Pipeline*      pPipelineSkybox = NULL;
RootSignature* pRootSignature = NULL;
Sampler*       pSamplerSkybox = NULL;
Texture*       pSkyboxTextures[6];

VirtualJoystickUI gVirtualJoystick;

//Zip File Test Texture
Texture *		pZipTexture[1];
Shader*			pZipTextureShader = NULL;
Buffer*			pZipTextureVertexBuffer = NULL;
Pipeline*		pZipTexturePipeline = NULL;


DepthState*      pDepth = NULL;
RasterizerState* pSkyboxRast = NULL;
RasterizerState* pBasicRast = NULL;
RasterizerState* pZipTextureRast = NULL;

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
GpuProfiler*       pGpuProfiler = NULL;

const char* pSkyboxImageFileNames[] = { "Skybox/Skybox_right1",  "Skybox/Skybox_left2",  "Skybox/Skybox_top3",
										"Skybox/Skybox_bottom4", "Skybox/Skybox_front5", "Skybox/Skybox_back6" };

const char* pCubeTextureName[] = { "Test_2" };

const char* pTextFileName[] = { "TestDoc.txt" };

const char* pModelFileName[] = { "capsule.obj" };

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

GuiComponent* pGui = NULL;

GuiComponent* pGui_TextData = NULL;

GuiComponent* pGui_ZipData = NULL;

//Zip file for testing
const char* pZipFiles = "28-ZipFileSystem.zip";

eastl::vector<eastl::string> gTextDataVector;

//structures for loaded model 
eastl::vector<MeshData*> pMeshes;

eastl::vector<Vertex>	vertices = {};
eastl::vector<uint>		indices = {};

class FileSystemUnitTest : public IApp
{
public:

		
	bool Init()
	{	

        // FILE PATHS
        PathHandle programDirectory = fsCopyProgramDirectoryPath();
        if (!fsPlatformUsesBundledResources())
        {
            PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/12_ZipFileSystem");
            fsSetResourceDirectoryRootPath(resourceDirRoot);
            
            fsSetRelativePathForResourceDirectory(RD_TEXTURES,        "../../UnitTestResources/Textures");
            fsSetRelativePathForResourceDirectory(RD_MESHES,          "../../UnitTestResources/Meshes");
            fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS,    "../../UnitTestResources/Fonts");
            fsSetRelativePathForResourceDirectory(RD_ANIMATIONS,      "../../UnitTestResources/Animation");
            fsSetRelativePathForResourceDirectory(RD_OTHER_FILES,      "../../UnitTestResources/ZipFiles");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT,  "../../../../Middleware_3/Text");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI,    "../../../../Middleware_3/UI");
        }
        
		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

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
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

        PathHandle zipFilePath = fsCopyPathInResourceDirectory(RD_OTHER_FILES, pZipFiles);
        
        FileSystem* zipFileSystem = fsCreateFileSystemFromFileAtPath(zipFilePath, FSF_READ_ONLY);
		if (!zipFileSystem)
		{
			ASSERT("Failed to Open zip file");
			return false;
		}

		PathHandle modelFilePath = fsCreatePath(zipFileSystem, pModelFileName[0]);
        FileStream* modelFile0FH = fsOpenFile(modelFilePath, FM_READ_BINARY);
		
		if (!modelFile0FH)
		{
			LOGF(LogLevel::eERROR, "\"%s\": ERROR in searching for file.", pModelFileName[0]);
			return false;
		}

        ssize_t modelFile0Size = fsGetStreamFileSize(modelFile0FH);
		char* pDataOfModel  = (char*)conf_malloc(modelFile0Size);
        size_t bytesRead = fsReadFromStream(modelFile0FH, pDataOfModel, modelFile0Size);

		//Check if unable to read model
		if (bytesRead != modelFile0Size)
		{
			LOGF(LogLevel::eERROR, "\"%s\": Error in reading file.", pModelFileName[0]);
			return false;
		}

        fsCloseStream(modelFile0FH);
        
		gTextDataVector.clear();

		PathHandle textFile0Path = fsCreatePath(zipFileSystem, pTextFileName[0]);
        FileStream* textFile0Handle = fsOpenFile(textFile0Path, FM_READ);

		if (!textFile0Handle)
		{
			LOGF(LogLevel::eERROR, "\"%s\": ERROR in searching for file.", pTextFileName[0]);
			return false;
		}
        
        ssize_t textFile0Size = fsGetStreamFileSize(textFile0Handle);
		char *pDataOfFile = (char*)conf_malloc((textFile0Size + 1) * sizeof(char));
        bytesRead = fsReadFromStream(textFile0Handle, pDataOfFile, textFile0Size);
		fsCloseStream(textFile0Handle);

		if (bytesRead != textFile0Size)
		{
			LOGF(LogLevel::eERROR, "\"%s\": Error in reading file.", pTextFileName[0]);
			return false;
		}
        pDataOfFile[textFile0Size] = 0;

		gTextDataVector.push_back(pDataOfFile);

		//Free the data buffer which was malloc'ed
		if (pDataOfFile != NULL)
		{
			conf_free(pDataOfFile);
		}

        PathHandle textureDescZipPath = fsCreatePath(zipFileSystem, pCubeTextureName[0]);
		//Load Zip file texture
		TextureLoadDesc textureDescZip = {};
        textureDescZip.pFilePath		= textureDescZipPath;
		textureDescZip.ppTexture		= &pZipTexture[0];
		addResource(&textureDescZip, true);
		
		// Loads Skybox Textures
		for (int i = 0; i < 6; ++i)
		{
            PathHandle textureDescPath = fsCreatePath(zipFileSystem, pSkyboxImageFileNames[i]);
            
			TextureLoadDesc textureDesc = {};
			textureDesc.pFilePath		= textureDescPath;
			textureDesc.ppTexture		= &pSkyboxTextures[i];
			addResource(&textureDesc, true);
		}

		//Load Zip File Model/Models
		LoadModel(pDataOfModel, modelFile0Size, pMeshes);

		//Free model data
		if (pDataOfModel != NULL)
		{
			conf_free(pDataOfModel);
		}

		// Close the Zip file
        fsFreeFileSystem(zipFileSystem);
		
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", RD_TEXTURES))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, RD_SHADER_SOURCES };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, RD_SHADER_SOURCES };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc zipTextureShader = {};
		zipTextureShader.mStages[0] = { "zipTexture.vert", NULL, 0, RD_SHADER_SOURCES };
		zipTextureShader.mStages[1] = { "zipTexture.frag", NULL, 0, RD_SHADER_SOURCES };
		
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
		
		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = pStaticSamplers;
		skyboxRootDesc.ppStaticSamplers = &pSamplerSkybox;
		skyboxRootDesc.mShaderCount = 3;
		skyboxRootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignature);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pSkyboxRast);
		
		RasterizerStateDesc cubeRasterizerStateDesc = {};
		cubeRasterizerStateDesc = {};
		cubeRasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &cubeRasterizerStateDesc, &pZipTextureRast);

		RasterizerStateDesc sphereRasterizerStateDesc = {};
		sphereRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &sphereRasterizerStateDesc, &pBasicRast);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		
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
		cubiodVbDesc.mDesc.mVertexStride = sizeof(float) * 8;
		cubiodVbDesc.pData = CubePoints;
		cubiodVbDesc.ppBuffer = &pZipTextureVertexBuffer;
		addResource(&cubiodVbDesc);

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
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc);

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
		}
		finishResourceLoading();

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(140.0f / dpiScale, 320.0f / dpiScale);
		guiDesc.mStartPosition = vec2( mSettings.mWidth - guiDesc.mStartSize.getX() * 1.1f, guiDesc.mStartSize.getY() * 0.5f);

		pGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);

		pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &gMicroProfiler));
		//--------------------------------

		//Gui for Showing the Text of the File
		guiDesc = {};
		
		guiDesc.mStartSize = vec2((float)mSettings.mWidth / 4, (float)mSettings.mHeight / 4);
		guiDesc.mStartPosition = vec2(guiDesc.mStartSize.getX() * 0.6f, guiDesc.mStartSize.getY() * 0.2f);

		pGui_TextData = gAppUI.AddGuiComponent("Opened Document", &guiDesc);

		pGui_TextData->AddWidget(LabelWidget(gTextDataVector[0]));
		
		//--------------------------------



		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
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
			if (!gMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
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

		vertices.clear();
		vertices.set_capacity(0);

		indices.clear();
		indices.set_capacity(0);


		//remove loaded zip test models
		for (int i = 0; i < pMeshes.size(); ++i)
		{
			removeResource(pMeshes[i]->pVertexBuffer);
			removeResource(pMeshes[i]->pIndexBuffer);
			conf_delete(pMeshes[i]);
		}
		pMeshes.clear();
		pMeshes.set_capacity(0);

		removeSampler(pRenderer, pSamplerSkybox);
		removeShader(pRenderer, pBasicShader);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pZipTextureShader);
		removeRootSignature(pRenderer, pRootSignature);

		removeDepthState(pDepth);
		removeRasterizerState(pBasicRast);
		removeRasterizerState(pSkyboxRast);
		removeRasterizerState(pZipTextureRast);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
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

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		//layout and pipeline for zip model draw
		VertexLayout vertexLayout = {};
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
		

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pBasicShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pBasicRast;
		addPipeline(pRenderer, &desc, &pBasicPipeline);

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
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pRasterizerState = pZipTextureRast;
		pipelineSettings.pShaderProgram = pZipTextureShader;
		addPipeline(pRenderer, &desc, &pZipTexturePipeline);

		CreateDescriptorSets();
		PrepareDescriptorSets();
		
		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

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
		mat4 trans = mat4::translation(vec3(15.0f,0.0f,22.0f));
		mat4 scale = mat4::scale(vec3(5.0f));
		gUniformData.mModelMatCapsule = trans * scale;


		//********************************
		//Uniform buffer data of the cube with zip texture
		//********************************

		mat4  mTranslationMat_Zip;
		mat4  mScaleMat_Zip;
		
		mTranslationMat_Zip = mat4::translation(vec3(10.5f, 1.0f, 3.0f));
		mScaleMat_Zip = mat4::scale(vec3(10.5f));
		gUniformData.mModelMatCube		= mTranslationMat_Zip * mScaleMat_Zip;

		viewMat.setTranslation(vec3(0));
		/************************************************************************/
		/************************************************************************/

    if(gMicroProfiler != bPrevToggleMicroProfiler)
    {
       toggleProfiler();
       bPrevToggleMicroProfiler = gMicroProfiler;
    }

    /************************************************************************/
    // Update GUI
    /************************************************************************/
    gAppUI.Update(deltaTime);  

	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&viewProjCbv);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 1.0f;
		loadActions.mClearColorValues[0].g = 1.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		Cmd* cmd = ppCmds[gFrameIndex];
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


		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetFrameUniforms);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTextures);
		
		//// draw skybox
#pragma region Skybox_Draw
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw skybox", true);
		cmdBindPipeline(cmd, pPipelineSkybox);

		
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, NULL);
		cmdDraw(cmd, 36, 0);
    cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
#pragma endregion

	////// draw Zip Model
#pragma region Zip_Model_Draw
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Zip Model", true);
	cmdBindPipeline(cmd, pBasicPipeline);
		
	cmdBindVertexBuffer(cmd, 1, &pMeshes[0]->pVertexBuffer, NULL);
	cmdBindIndexBuffer(cmd, pMeshes[0]->pIndexBuffer, NULL);
	cmdDrawIndexed(cmd, pMeshes[0]->mIndexCount, 0, 0);
	cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
#pragma endregion


	////draw Cube with Zip texture
#pragma region Cube_Zip_Texture_Draw
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Zip File Texture", true);
	cmdBindPipeline(cmd, pZipTexturePipeline);

	cmdBindVertexBuffer(cmd, 1, &pZipTextureVertexBuffer, NULL);
	cmdDraw(cmd, 36, 0);
	cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
#pragma endregion
	
    cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		gAppUI.DrawText(cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)
    gAppUI.DrawText(
      cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
      &gFrameTimeDraw);
    gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#endif

    gAppUI.Gui(pGui);
    
	gAppUI.Gui(pGui_TextData);
			
	cmdDrawProfiler();

		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}
    cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers);

    cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
    flipProfiler();
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
		depthRT.mClearValue.depth = 1.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
	
	

	void LoadModel(char* modelData, size_t asizeModels, eastl::vector<MeshData*> &pMeshes)
	{
		AssimpImporter          importer;
		
		int modelsCount = 1;

		for (int m = 0; m < modelsCount; ++m)
		{
			AssimpImporter::Model model;
			
			if (importer.ImportModelFromMemory(modelData, &model, pModelFileName[0], asizeModels))
			{
				vertices.clear();
				indices.clear();

				for (size_t i = 0; i < model.mMeshArray.size(); ++i)
				{
					AssimpImporter::Mesh* mesh = &model.mMeshArray[i];
					vertices.reserve(vertices.size() + mesh->mPositions.size());
					indices.reserve(indices.size() + mesh->mIndices.size());

					for (size_t v = 0; v < mesh->mPositions.size(); ++v)
					{
						Vertex vertex = { float3(0.0f), float3(0.0f, 1.0f, 0.0f) };
						vertex.mPosition = mesh->mPositions[v];
						vertex.mNormal = mesh->mNormals[v];
						vertex.mUV = mesh->mUvs[v];
						vertices.push_back(vertex);

					}

					for (size_t j = 0; j < mesh->mIndices.size(); ++j)
						indices.push_back(mesh->mIndices[j]);
				}

				//Mesh Data
				MeshData* meshData;
				meshData = conf_placement_new<MeshData>(conf_malloc(sizeof(MeshData)));
				meshData->mVertexCount = (uint)vertices.size();
				meshData->mIndexCount = (uint)indices.size();

				BufferLoadDesc	gVertexBufferDesc = {};
				gVertexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
				gVertexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				gVertexBufferDesc.mDesc.mSize = sizeof(Vertex) * meshData->mVertexCount;
				gVertexBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
				gVertexBufferDesc.pData = vertices.data();
				gVertexBufferDesc.ppBuffer = &meshData->pVertexBuffer;
				addResource(&gVertexBufferDesc);
				
				if (meshData->mIndexCount > 0)
				{
					BufferLoadDesc	gIndexBufferDesc = {};
					gIndexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
					gIndexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					gIndexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
					gIndexBufferDesc.mDesc.mSize = sizeof(uint) * meshData->mIndexCount;
					gIndexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
					gIndexBufferDesc.pData = indices.data();
					gIndexBufferDesc.ppBuffer = &meshData->pIndexBuffer; 
					addResource(&gIndexBufferDesc);
				}

				pMeshes.push_back(meshData);
				
			}
			else
				LOGF(LogLevel::eERROR ,"Failed to load model.");
		}
	}

};

DEFINE_APPLICATION_MAIN(FileSystemUnitTest)
