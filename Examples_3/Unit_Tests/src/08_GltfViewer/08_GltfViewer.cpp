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


#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "../../../../Common_3/Tools/AssetPipeline/src/AssetLoader.h"

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

 //Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be last include in cpp file

//--------------------------------------------------------------------------------------------
// GLOBAL DEFINTIONS
//--------------------------------------------------------------------------------------------

uint32_t			gFrameIndex = 0;

const uint32_t		gImageCount = 3;
const uint32_t		gLightCount = 3;
const uint32_t		gTotalLightCount = gLightCount + 1;

// Model Quantization Settings
int					gPosBits = 16;
int					gTexBits = 16;
int					gNrmBits = 8;

int					gCurrentLod = 0;
int					gMaxLod = 5;

bool				bToggleMicroProfiler = false;
bool				bPrevToggleMicroProfiler = false;
bool				bToggleFXAA = true;
bool				bVignetting = true;
bool				bToggleVSync = false;
bool				bScreenShotMode = false;

//--------------------------------------------------------------------------------------------
// PRE PROCESSORS
//--------------------------------------------------------------------------------------------

#define SHADOWMAP_MSAA_SAMPLES 1

#if defined(TARGET_IOS) || defined(__ANDROID__)
#define SHADOWMAP_RES 1024u
#else
#define SHADOWMAP_RES 2048u
#endif

#if !defined(TARGET_IOS) && !defined(__ANDROID__)
#define USE_BASIS 1
#endif

//--------------------------------------------------------------------------------------------
// STRUCT DEFINTIONS
//--------------------------------------------------------------------------------------------

struct UniformBlock
{
	mat4 mProjectView;
	vec4 mCameraPosition;
	vec4 mLightColor[gTotalLightCount];
	vec4 mLightDirection[gLightCount];
	int4 mQuantizationParams;
};

struct UniformBlock_Shadow
{
	mat4 ViewProj;
};

struct UniformBlock_Floor
{
	mat4	worldMat;
	mat4	projViewMat;
	vec4	screenSize;
};

// Have a uniform for object data
struct UniformBlock_ObjData
{
	mat4  mWorldMat;
	mat4  InvTranspose;
	int   unlit;
	int   hasAlbedoMap;
	int   hasNormalMap;
	int   hasMetallicRoughnessMap;
	int   hasAOMap;
	int   hasEmissiveMap;
	vec4  centerOffset;
	vec4  posOffset;
	vec2  uvOffset;
	vec2  uvScale;
	float posScale;
	float padding0;
};

//Structure of Array for vertex/index data
struct MeshBatch
{
	Buffer* pPositionStream;
	Buffer* pNormalStream;
	Buffer* pUVStream;
	Buffer* pIndicesStream;
	Buffer* pBaseColorStream;
	Buffer* pMetallicRoughnessStream;
	Buffer* pAlphaStream;
	Buffer* pConstantBuffer;
	int     NoofVertices;
	int     NoofIndices;
	int     NoofInstances;
	int     MaterialIndex;
	int     SamplerIndex[5];
	int     NoofUVSets;
	int     ShadowSetIndex;
	int     DemoSetIndex;
};

struct PropData
{
	mat4                      WorldMatrix;
	eastl::vector<MeshBatch*> MeshBatches;
};

struct FXAAINFO
{
	vec2 ScreenSize;
	uint Use;
	uint padding00;
};


//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------

Renderer*			pRenderer = NULL;

Queue*				pGraphicsQueue = NULL;

CmdPool*			pCmdPool = NULL;
Cmd**				ppCmds = NULL;

SwapChain*			pSwapChain = NULL;

RenderTarget*		pForwardRT = NULL;
RenderTarget*		pPostProcessRT = NULL;
RenderTarget*		pDepthBuffer = NULL;
RenderTarget*		pShadowRT = NULL;

Fence*				pRenderCompleteFences[gImageCount] = { NULL };

Semaphore*			pImageAcquiredSemaphore = NULL;
Semaphore*			pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*				pShaderZPass = NULL;
Shader*				pShaderZPass_NonOptimized = NULL;
Shader*				pMeshOptDemoShader = NULL;
Shader*				pFloorShader = NULL;
Shader*				pVignetteShader = NULL;
Shader*				pFXAAShader = NULL;
Shader*				pWaterMarkShader = NULL;

Pipeline*			pPipelineShadowPass = NULL;
Pipeline*			pPipelineShadowPass_NonOPtimized = NULL;
Pipeline*			pMeshOptDemoPipeline = NULL;
Pipeline*			pFloorPipeline = NULL;
Pipeline*			pVignettePipeline = NULL;
Pipeline*			pFXAAPipeline = NULL;
Pipeline*			pWaterMarkPipeline = NULL;

RootSignature*		pRootSignature = NULL;
RootSignature*		pRootSignatureDemo = NULL;

DescriptorSet*      pDescriptorSet[3] = { NULL };
DescriptorSet*      pDescriptorSetDemo[3] = { NULL };

VirtualJoystickUI   gVirtualJoystick = {};

RasterizerState*	pRasterizerStateCullBack = NULL;

DepthState*			pDepthStateForRendering = NULL;

BlendState*			pBlendStateAlphaBlend = NULL;

Buffer*				pUniformBuffer[gImageCount] = { NULL };
Buffer*				pShadowUniformBuffer[gImageCount] = { NULL };
Buffer*				pFloorUniformBuffer[gImageCount] = { NULL };
Buffer*				pFloorShadowUniformBuffer = NULL;

Buffer*				TriangularVB = NULL;
Buffer*				pFloorVB = NULL;
Buffer*				pFloorIB = NULL;
Buffer*				WaterMarkVB = NULL;

Sampler*			pBilinearClampSampler = NULL;

UniformBlock		gUniformData;
UniformBlock_Floor	gFloorUniformBlock;
UniformBlock_Shadow gShadowUniformData;

//--------------------------------------------------------------------------------------------
// THE FORGE OBJECTS
//--------------------------------------------------------------------------------------------

ICameraController*	pCameraController = NULL;
ICameraController*	pLightView = NULL;

GuiComponent*		pGuiWindow;
GuiComponent*		pGuiGraphics;

IWidget*			pSelectLodWidget = NULL;

GpuProfiler*		pGpuProfiler = NULL;

UIApp				gAppUI;
eastl::vector<uint32_t>	gDropDownWidgetData;
eastl::vector<PathHandle> gModelFiles;

#if defined(__ANDROID__) || defined(__LINUX__)
uint32_t			modelToLoadIndex = 0;
uint32_t			guiModelToLoadIndex = 0;
#endif

bool				gLoadOptimizedModel = false;

const wchar_t*		gMissingTextureString = L"MissingTexture";
Texture*			pTextureBlack = NULL;

const char*                     gDefaultModelFile = "Lantern.gltf";
PathHandle					    gModelFile;
PathHandle					    gGuiModelToLoad;

const uint			gBackroundColor = { 0xb2b2b2ff };
static uint			gLightColor[gTotalLightCount] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffff66 };
static float		gLightColorIntensity[gTotalLightCount] = { 2.0f, 0.2f, 0.2f, 0.25f };
static float2		gLightDirection = { -122.0f, 222.0f };



//Model
struct LOD
{
	Model model;
	PropData modelProp;
	eastl::vector<Texture*> pMaterialTextures;
	eastl::vector<Sampler*> pSamplers;
	eastl::vector<uint32_t> textureIndexforMaterial;
	eastl::unordered_map<uint32_t, uint32_t> materialIDMap;
	eastl::unordered_map<uint64_t, uint32_t> samplerIDMap;
};


eastl::vector<LOD> gLODs(0);

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class MeshOptimization : public IApp
{
public:
	MeshOptimization()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	static bool InitShaderResources()
	{
		// shader

		ShaderLoadDesc zPassShaderDesc = {};

		zPassShaderDesc.mStages[0] = { "zPass.vert", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &zPassShaderDesc, &pShaderZPass);

		zPassShaderDesc = {};

		zPassShaderDesc.mStages[0] = { "zPassFloor.vert", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &zPassShaderDesc, &pShaderZPass_NonOptimized);

		ShaderLoadDesc FloorShader = {};

		FloorShader.mStages[0] = { "floor.vert", NULL, 0, RD_SHADER_SOURCES };
#if defined(__ANDROID__)
		FloorShader.mStages[1] = { "floorMOBILE.frag", NULL, 0, RD_SHADER_SOURCES };
#else
		FloorShader.mStages[1] = { "floor.frag", NULL, 0, RD_SHADER_SOURCES };
#endif

		addShader(pRenderer, &FloorShader, &pFloorShader);

		ShaderLoadDesc MeshOptDemoShader = {};

		MeshOptDemoShader.mStages[0] = { "basic.vert", NULL, 0, RD_SHADER_SOURCES };
#if defined(__ANDROID__)
		MeshOptDemoShader.mStages[1] = { "basicMOBILE.frag", NULL, 0, RD_SHADER_SOURCES };
#else
		MeshOptDemoShader.mStages[1] = { "basic.frag", NULL, 0, RD_SHADER_SOURCES };
#endif

		addShader(pRenderer, &MeshOptDemoShader, &pMeshOptDemoShader);

		ShaderLoadDesc VignetteShader = {};

		VignetteShader.mStages[0] = { "Triangular.vert", NULL, 0, RD_SHADER_SOURCES };
		VignetteShader.mStages[1] = { "vignette.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &VignetteShader, &pVignetteShader);

		ShaderLoadDesc FXAAShader = {};

		FXAAShader.mStages[0] = { "Triangular.vert", NULL, 0, RD_SHADER_SOURCES };
		FXAAShader.mStages[1] = { "FXAA.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &FXAAShader, &pFXAAShader);

		ShaderLoadDesc WaterMarkShader = {};

		WaterMarkShader.mStages[0] = { "watermark.vert", NULL, 0, RD_SHADER_SOURCES };
		WaterMarkShader.mStages[1] = { "watermark.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &WaterMarkShader, &pWaterMarkShader);

		const char* pStaticSamplerNames[] = { "clampMiplessLinearSampler" };
		Sampler* pStaticSamplers[] = { pBilinearClampSampler };
		Shader*           shaders[] = { pShaderZPass, pShaderZPass_NonOptimized, pVignetteShader, pFloorShader, pFXAAShader, pWaterMarkShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		rootDesc.ppStaticSamplers = pStaticSamplers;
		rootDesc.mShaderCount = 6;
		rootDesc.ppShaders = shaders;

		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		Shader*  demoShaders[] = { pMeshOptDemoShader };

		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = demoShaders;

		addRootSignature(pRenderer, &rootDesc, &pRootSignatureDemo);

		if (!AddDescriptorSets())
			return false;

		return true;
	}

	static bool InitModelDependentResources()
	{
		if (!LoadModel(gLODs, gModelFile))
			return false;

		if (!InitShaderResources())
			return false;

		PrepareDescriptorSets();

		return true;
	}

	static void setRenderTarget(Cmd* cmd, uint32_t count, RenderTarget** pDestinationRenderTargets, RenderTarget* pDepthStencilTarget, LoadActionsDesc* loadActions)
	{
		if (count == 0 && pDestinationRenderTargets == NULL && pDepthStencilTarget == NULL)
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		else
		{
			cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
			// sets the rectangles to match with first attachment, I know that it's not very portable.
			RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0];
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSizeTarget->mDesc.mWidth, (float)pSizeTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSizeTarget->mDesc.mWidth, pSizeTarget->mDesc.mHeight);
		}
	}

	static void drawShadowMap(Cmd* cmd)
	{
		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pShadowUniformBuffer[gFrameIndex], &gShadowUniformData };
		updateResource(&shaderCbv);

		TextureBarrier barriers[] =
		{
			{ pShadowRT->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};

		cmdResourceBarrier(cmd, 0, NULL, 1, barriers);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pShadowRT->mDesc.mClearValue;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Shadow Map", true);
		// Start render pass and apply load actions
		setRenderTarget(cmd, 0, NULL, pShadowRT, &loadActions);

		cmdBindPipeline(cmd, pPipelineShadowPass_NonOPtimized);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet[1]);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSet[2]);

		cmdBindVertexBuffer(cmd, 1, &pFloorVB, NULL);
		cmdBindIndexBuffer(cmd, pFloorIB, 0);

		cmdDrawIndexed(cmd, 6, 0, 0);

		cmdBindPipeline(cmd, pPipelineShadowPass);

		for (MeshBatch* mesh : gLODs[gCurrentLod].modelProp.MeshBatches)
		{
			cmdBindDescriptorSet(cmd, mesh->ShadowSetIndex, pDescriptorSet[2]);

			Buffer* pVertexBuffers[] = { mesh->pPositionStream, mesh->pNormalStream, mesh->pUVStream,
				mesh->pBaseColorStream, mesh->pMetallicRoughnessStream, mesh->pAlphaStream };
			cmdBindVertexBuffer(cmd, 6, pVertexBuffers, NULL);

			cmdBindIndexBuffer(cmd, mesh->pIndicesStream, 0);

			cmdDrawIndexed(cmd, mesh->NoofIndices, 0, 0);
		}

		setRenderTarget(cmd, 0, NULL, NULL, NULL);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	bool Init()
	{
#if defined(TARGET_IOS)
		fsRegisterUTIForExtension("dyn.ah62d4rv4ge80s5dyq2", "gltf");
#endif

		// FILE PATHS
		PathHandle programDirectory = fsCopyProgramDirectoryPath();
		if (!fsPlatformUsesBundledResources())
		{
			PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/08_GltfViewer");
			fsSetResourceDirectoryRootPath(resourceDirRoot);

			fsSetRelativePathForResourceDirectory(RD_TEXTURES, "../../UnitTestResources/Textures");
			fsSetRelativePathForResourceDirectory(RD_MESHES, "../../UnitTestResources/Meshes");
			fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS, "../../UnitTestResources/Fonts");
			fsSetRelativePathForResourceDirectory(RD_ANIMATIONS, "../../UnitTestResources/Animation");
			fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT, "../../../../Middleware_3/Text");
			fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI, "../../../../Middleware_3/UI");
		}

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
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

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);

		initProfiler();

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

#if defined(__ANDROID__) || defined(TARGET_IOS)
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", RD_TEXTURES))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}
#endif

		SamplerDesc samplerClampDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
		};
		addSampler(pRenderer, &samplerClampDesc, &pBilinearClampSampler);

		float floorPoints[] = {
			-1.0f, 0.0f, 1.0f, -1.0f, -1.0f,
			-1.0f, 0.0f, -1.0f, -1.0f, 1.0f,
			1.0f, 0.0f, -1.0f, 1.0f, 1.0f,
			1.0f, 0.0f, 1.0f, 1.0f, -1.0f,
		};

		BufferLoadDesc floorVbDesc = {};
		floorVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		floorVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		floorVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		floorVbDesc.mDesc.mSize = floorVbDesc.mDesc.mVertexStride * 4;
		floorVbDesc.pData = floorPoints;
		floorVbDesc.ppBuffer = &pFloorVB;
		addResource(&floorVbDesc);

		uint32_t floorIndices[] = {
			0, 1, 3,
			3, 1, 2
		};

		BufferLoadDesc indexBufferDesc = {};
		indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		indexBufferDesc.mDesc.mSize = sizeof(uint32_t) * 6;
		indexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		indexBufferDesc.pData = floorIndices;
		indexBufferDesc.ppBuffer = &pFloorIB;
		addResource(&indexBufferDesc);

		float screenTriangularPoints[] = {
			-1.0f,  3.0f, 0.5f, 0.0f, -1.0f,
			-1.0f, -1.0f, 0.5f, 0.0f, 1.0f,
			3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		screenQuadVbDesc.mDesc.mSize = screenQuadVbDesc.mDesc.mVertexStride * 3;
		screenQuadVbDesc.pData = screenTriangularPoints;
		screenQuadVbDesc.ppBuffer = &TriangularVB;
		addResource(&screenQuadVbDesc);

		TextureDesc defaultTextureDesc = {};
		defaultTextureDesc.mArraySize = 1;
		defaultTextureDesc.mDepth = 1;
		defaultTextureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		defaultTextureDesc.mWidth = 4;
		defaultTextureDesc.mHeight = 4;
		defaultTextureDesc.mMipLevels = 1;
		defaultTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		defaultTextureDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		defaultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		defaultTextureDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		defaultTextureDesc.pDebugName = gMissingTextureString;
		TextureLoadDesc defaultLoadDesc = {};
		defaultLoadDesc.pDesc = &defaultTextureDesc;
		RawImageData idata;
		unsigned char blackData[64];
		memset(blackData, 0, sizeof(unsigned char) * 64);

		idata.mArraySize = 1;
		idata.mDepth = defaultTextureDesc.mDepth;
		idata.mWidth = defaultTextureDesc.mWidth;
		idata.mHeight = defaultTextureDesc.mHeight;
		idata.mFormat = defaultTextureDesc.mFormat;
		idata.mMipLevels = defaultTextureDesc.mMipLevels;
		idata.pRawData = blackData;

		defaultLoadDesc.pRawImageData = &idata;

		defaultLoadDesc.ppTexture = &pTextureBlack;
		addResource(&defaultLoadDesc);

#if defined(__ANDROID__) || defined(__LINUX__)
		// Get list of Models
		eastl::vector<eastl::string> filesToLoad;
		eastl::vector<PathHandle> filesToLoadFullPath;
		PathHandle meshDirectory = fsCopyPathForResourceDirectory(RD_MESHES);
		eastl::vector<PathHandle> filesInDirectory = fsGetFilesWithExtension(meshDirectory, "gltf");

		//reduce duplicated filenames
		for (size_t i = 0; i < filesInDirectory.size(); ++i)
		{
			const PathHandle & file = filesInDirectory[i];

			eastl::string tempfile(fsGetPathAsNativeString(file));

			const char* first = strstr(tempfile.c_str(), "PQPM");

			bool bAlreadyLoaded = false;

			if (first != NULL)
			{
				for (size_t j = 0; j < filesToLoad.size(); ++j)
				{
					if (strstr(tempfile.c_str(), filesToLoad[j].c_str()) != NULL)
					{
						bAlreadyLoaded = true;
						break;
					}
				}

				if (!bAlreadyLoaded)
				{
					int gap = first - tempfile.c_str();
					tempfile.resize(gap);
					filesToLoad.push_back(tempfile);
					filesToLoadFullPath.push_back(file);
				}
			}
			else
			{
				filesToLoad.push_back(tempfile);
				filesToLoadFullPath.push_back(file);
			}
		}

		size_t modelFileCount = filesToLoadFullPath.size();

		eastl::vector<const char*> modelFileNames(modelFileCount);
		gModelFiles.resize(modelFileCount);
		gDropDownWidgetData.resize(modelFileCount);

		for (size_t i = 0; i < modelFileCount; ++i)
		{
			const PathHandle& file = filesToLoadFullPath[i];

			gModelFiles[i] = file;
			modelFileNames[i] = fsGetPathFileName(gModelFiles[i]).buffer;
			gDropDownWidgetData[i] = (uint32_t)i;
		}

		gModelFile = gModelFiles[0];
		gGuiModelToLoad = gModelFiles[0];

#else
		PathHandle fullModelPath = fsCopyPathInResourceDirectory(RD_MESHES, gDefaultModelFile);
		gModelFile = fullModelPath;
		gGuiModelToLoad = fullModelPath;
#endif

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pUniformBuffer[i];
			addResource(&ubDesc);
		}

		BufferLoadDesc floorShadowDesc = {};
		floorShadowDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		floorShadowDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		floorShadowDesc.mDesc.mSize = sizeof(UniformBlock_ObjData);
		floorShadowDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;

		UniformBlock_ObjData meshConstantBufferData = {};
		meshConstantBufferData.mWorldMat = mat4::scale(vec3(3.0f));
		meshConstantBufferData.InvTranspose = transpose(inverse(meshConstantBufferData.mWorldMat));

		floorShadowDesc.pData = &meshConstantBufferData;
		floorShadowDesc.ppBuffer = &pFloorShadowUniformBuffer;
		addResource(&floorShadowDesc);

		BufferLoadDesc subDesc = {};
		subDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		subDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		subDesc.mDesc.mSize = sizeof(UniformBlock_Shadow);
		subDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		subDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			subDesc.ppBuffer = &pShadowUniformBuffer[i];
			addResource(&subDesc);
		}

		ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pFloorUniformBuffer[i];
			addResource(&ubDesc);
		}

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullBack);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepthStateForRendering);

		BlendStateDesc blendStateAlphaDesc = {};
		blendStateAlphaDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateAlphaDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateAlphaDesc.mBlendModes[0] = BM_ADD;
		blendStateAlphaDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateAlphaDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateAlphaDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateAlphaDesc.mMasks[0] = ALL;
		blendStateAlphaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateAlphaDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateAlphaDesc, &pBlendStateAlphaBlend);

		/************************************************************************/
		// GUI
		/************************************************************************/
		GuiDesc guiDesc = {};
		guiDesc.mStartSize = vec2(300.0f, 250.0f);
		guiDesc.mStartPosition = vec2(100.0f, guiDesc.mStartSize.getY());
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);

		pGuiWindow->AddWidget(CheckboxWidget("Enable Micro Profiler", &bToggleMicroProfiler));

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGuiWindow->AddWidget(CheckboxWidget("Toggle VSync", &bToggleVSync));
#endif

#if defined(__ANDROID__) || defined(__LINUX__)
		pGuiWindow->AddWidget(DropdownWidget("Models", &guiModelToLoadIndex, modelFileNames.data(), gDropDownWidgetData.data(), (uint32_t)gModelFiles.size()));
#else		
		pGuiWindow->AddWidget(SeparatorWidget());

		ButtonWidget loadModelButtonWidget("Load Model                                      ");
		loadModelButtonWidget.pOnEdited = MeshOptimization::LoadNewModel;
		pGuiWindow->AddWidget(loadModelButtonWidget);

		pGuiWindow->AddWidget(SeparatorWidget());

		//ButtonWidget loadLODButtonWidget("Load Model LOD");
		//loadLODButtonWidget.pOnEdited = MeshOptimization::LoadLOD;
		//pGuiWindow->AddWidget(loadLODButtonWidget);
#endif

		pSelectLodWidget = pGuiWindow->AddWidget(SliderIntWidget("LOD", &gCurrentLod, 0, gMaxLod));

		////////////////////////////////////////////////////////////////////////////////////////////

		guiDesc = {};
		guiDesc.mStartSize = vec2(400.0f, 250.0f);
		guiDesc.mStartPosition = vec2(mSettings.mWidth - guiDesc.mStartSize.getX(), guiDesc.mStartSize.getY());
		pGuiGraphics = gAppUI.AddGuiComponent("Graphics Options", &guiDesc);

		pGuiGraphics->AddWidget(CheckboxWidget("Enable FXAA", &bToggleFXAA));
		pGuiGraphics->AddWidget(CheckboxWidget("Enable Vignetting", &bVignetting));

		pGuiGraphics->AddWidget(SeparatorWidget());

		CollapsingHeaderWidget LightWidgets("Light Options", false, false);
		LightWidgets.AddSubWidget(SliderFloatWidget("Light Azimuth", &gLightDirection.x, float(-180.0f), float(180.0f), float(0.001f)));
		LightWidgets.AddSubWidget(SliderFloatWidget("Light Elevation", &gLightDirection.y, float(210.0f), float(330.0f), float(0.001f)));

		LightWidgets.AddSubWidget(SeparatorWidget());

		CollapsingHeaderWidget LightColor1Picker("Main Light Color");
		LightColor1Picker.AddSubWidget(ColorPickerWidget("Main Light Color", &gLightColor[0]));
		LightWidgets.AddSubWidget(LightColor1Picker);

		CollapsingHeaderWidget LightColor1Intensity("Main Light Intensity");
		LightColor1Intensity.AddSubWidget(SliderFloatWidget("Main Light Intensity", &gLightColorIntensity[0], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor1Intensity);

		LightWidgets.AddSubWidget(SeparatorWidget());

		CollapsingHeaderWidget LightColor2Picker("Light2 Color");
		LightColor2Picker.AddSubWidget(ColorPickerWidget("Light2 Color", &gLightColor[1]));
		LightWidgets.AddSubWidget(LightColor2Picker);

		CollapsingHeaderWidget LightColor2Intensity("Light2 Intensity");
		LightColor2Intensity.AddSubWidget(SliderFloatWidget("Light2 Intensity", &gLightColorIntensity[1], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor2Intensity);

		LightWidgets.AddSubWidget(SeparatorWidget());

		CollapsingHeaderWidget LightColor3Picker("Light3 Color");
		LightColor3Picker.AddSubWidget(ColorPickerWidget("Light3 Color", &gLightColor[2]));
		LightWidgets.AddSubWidget(LightColor3Picker);

		CollapsingHeaderWidget LightColor3Intensity("Light3 Intensity");
		LightColor3Intensity.AddSubWidget(SliderFloatWidget("Light3 Intensity", &gLightColorIntensity[2], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor3Intensity);

		LightWidgets.AddSubWidget(SeparatorWidget());

		CollapsingHeaderWidget AmbientLightColorPicker("Ambient Light Color");
		AmbientLightColorPicker.AddSubWidget(ColorPickerWidget("Ambient Light Color", &gLightColor[3]));
		LightWidgets.AddSubWidget(AmbientLightColorPicker);

		CollapsingHeaderWidget LightColor4Intensity("Ambient Light Intensity");
		LightColor4Intensity.AddSubWidget(SliderFloatWidget("Light Intensity", &gLightColorIntensity[3], 0.0f, 5.0f, 0.001f));
		LightWidgets.AddSubWidget(LightColor4Intensity);

		LightWidgets.AddSubWidget(SeparatorWidget());

		pGuiGraphics->AddWidget(LightWidgets);

		CameraMotionParameters cmp{ 1.0f, 120.0f, 40.0f };
		vec3                   camPos{ 3.0f, 2.5f, -4.0f };
		vec3                   lookAt{ 0.0f, 0.4f, 0.0f };

		pLightView = createGuiCameraController(camPos, lookAt);
		pCameraController = createFpsCameraController(normalize(camPos) * 3.0f, lookAt);
		pCameraController->setMotionParameters(cmp);

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
			if (!bToggleMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
			{
#if defined(TARGET_IOS) || defined(__ANDROID__)
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
#endif				
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.25f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 0.25f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		return true;
	}

	static void getQuantizationSettings(eastl::string filename, int & posBits, int & texBits, int & normBits)
	{
		size_t searchPos = filename.find("_OPTIMIZED");
		size_t pBitsPos = filename.find("_p", searchPos);

		pBitsPos += 2;
		size_t uvBitsPos = filename.find_first_of("_uv", pBitsPos);
		int pBits = atoi(filename.substr(pBitsPos, uvBitsPos - pBitsPos).c_str());

		uvBitsPos += 3;
		size_t nBitsPos = filename.find_first_of("_n", uvBitsPos);
		int uvBits = atoi(filename.substr(uvBitsPos, nBitsPos - uvBitsPos).c_str());

		nBitsPos += 2;
		int nBits = atoi(filename.substr(nBitsPos, 1).c_str());

		posBits = pBits;
		texBits = uvBits;
		normBits = nBits;
	}

	static float3 getOffsetToCenterModel(eastl::vector<Mesh> & meshes, mat4 worldMat)
	{
		float maxZ = 0.0f;
		float minZ = 0.0f;
		float maxY = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float minX = 0.0f;

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			for (size_t j = 0; j < meshes[i].streams.size(); ++j)
			{
				Stream & stream = meshes[i].streams[j];

				if (stream.type == cgltf_attribute_type_position)
				{
					for (size_t k = 0; k < stream.data.size(); ++k)
					{
						vec4 pos;
						pos.setX(stream.data[k].f[0]);
						pos.setY(stream.data[k].f[1]);
						pos.setZ(stream.data[k].f[2]);

						//pos = worldMat * pos;

						if (pos.getZ() > maxZ)
							maxZ = pos.getZ();
						if (pos.getZ() < minZ)
							minZ = pos.getZ();
						if (pos.getY() > maxY)
							maxY = pos.getY();
						if (pos.getY() < minY)
							minY = pos.getY();
						if (pos.getX() > maxX)
							maxX = pos.getX();
						if (pos.getX() < minX)
							minX = pos.getX();
					}
				}
			}
		}

		vec3 minPos(minX, minY, minZ);
		vec3 maxPos(maxX, maxY, maxZ);
		vec3 diagVec = maxPos - minPos;

		vec3 aabbCenter = minPos + diagVec * 0.5f;

		vec3 offset = -aabbCenter;

		return float3(offset.getX(), -minY, offset.getZ());
	}

	static bool LoadModel(eastl::vector<LOD> & lod, const Path* modelFilePath)
	{
		eastl::string modelFileName = fsPathComponentToString(fsGetPathFileName(modelFilePath));
		if (-1 != modelFileName.find("_OPTIMIZED"))
		{
			gLoadOptimizedModel = true;
			getQuantizationSettings(modelFileName, gPosBits, gTexBits, gNrmBits);
		}
		else
		{
			gLoadOptimizedModel = false;
			gPosBits = 16;
			gTexBits = 16;
			gNrmBits = 8;
		}

		::Settings settings = {};
		settings.pos_bits = gPosBits;
		settings.tex_bits = gTexBits;
		settings.nrm_bits = gNrmBits;
		settings.anim_freq = 30;

		unsigned int loadFlags = alGEN_MATERIAL_ID | alGEN_NORMALS | alMAKE_LEFT_HANDED;
		if (!gLoadOptimizedModel)
			loadFlags |= alOPTIMIZE;
		else
			loadFlags |= alIS_QUANTIZED;

		eastl::vector<PathHandle> validFileLists;
		eastl::vector<PathHandle> intermediateFileLists;

#if defined(TARGET_IOS)
		PathHandle meshDirectory = fsCopyParentPath(modelFilePath);
#else
		PathHandle meshDirectory = fsCopyPathForResourceDirectory(RD_MESHES);
#endif
		eastl::vector<PathHandle> fileLists = fsGetFilesWithExtension(meshDirectory, "gltf");

		eastl::string fileNameOnly = modelFileName;

		// IKEA name convention
		bool bUseNameConvention = false;
		if (strstr(fileNameOnly.c_str(), "PQPM") != NULL)
		{
			fileNameOnly.resize(fileNameOnly.size() - 4);

			// Find all LOD models based on modelFileName
			for (int32_t i = (int32_t)fileLists.size() - 1; i >= 0; --i)
			{
				if (strstr(fsGetPathAsNativeString(fileLists[i]), fileNameOnly.c_str()) != NULL)
				{
					intermediateFileLists.push_back(fileLists[i]);
				}
			}

			bUseNameConvention = true;
		}
		else
			validFileLists.push_back(fsCopyPath(modelFilePath));

		eastl::vector<eastl::pair<int, int>> Orderlist;

		// Sort them
		if (bUseNameConvention)
		{
			for (int32_t i = 0; i < (int32_t)intermediateFileLists.size(); ++i)
			{
				PathComponent fileName = fsGetPathFileName(intermediateFileLists[i]);
				char suffix[4];
				strncpy(suffix, fileName.buffer + max((ssize_t)fileName.length - 4, (ssize_t)0), 4);

				int lodNumber = atoi(suffix);

				if (Orderlist.size() == 0)
				{
					Orderlist.push_back(eastl::pair<int, int>(i, lodNumber));
				}
				else
				{
					eastl::pair<int, int> tempData(i, lodNumber);

					bool inserted = false;
					eastl::pair<int, int> *iter = Orderlist.begin();

					for (int32_t j = 0; j < (int32_t)Orderlist.size(); ++j)
					{
						if (Orderlist[j].second < lodNumber)
						{
							Orderlist.insert(iter, tempData);
							inserted = true;
							break;
						}

						iter++;
					}

					if (!inserted)
						Orderlist.push_back(tempData);
				}
			}

			validFileLists.clear();

			for (int32_t i = 0; i < (int32_t)Orderlist.size(); ++i)
			{
				int index = Orderlist[i].first;
				validFileLists.push_back(intermediateFileLists[index]);
			}
		}


		for (uint32_t j = 0; j < (uint32_t)validFileLists.size(); ++j)
		{
			LOD currentMesh;

			Model & model = currentMesh.model;
			PropData & modelProp = currentMesh.modelProp;
			eastl::vector<Sampler*> & pSamplers = currentMesh.pSamplers;
			eastl::unordered_map<uint64_t, uint32_t> & samplerIDMap = currentMesh.samplerIDMap;
			eastl::vector<uint32_t> & textureIndexforMaterial = currentMesh.textureIndexforMaterial;
			eastl::unordered_map<uint32_t, uint32_t> & materialIDMap = currentMesh.materialIDMap;
			eastl::vector<Texture*> & pMaterialTextures = currentMesh.pMaterialTextures;

			bool result = AssetLoader::LoadModel(validFileLists[j], &model, loadFlags);

			if (result == false)
			{
				return result;
			}

			// Add samplers
			SamplerDesc samplerDescDefault = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
				ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
			// Set sampler desc based on glTF specification
			pSamplers.resize(model.data->samplers_count);

			if (model.data->samplers_count > 0)
			{
				for (size_t i = 0; i < model.data->samplers_count; ++i)
				{
					SamplerDesc samplerDesc = samplerDescDefault;

					cgltf_sampler sampler = model.data->samplers[i];

					switch (sampler.mag_filter)
					{
					case 9728:
						samplerDesc.mMagFilter = FILTER_NEAREST;
						break;
					case 9729:
						samplerDesc.mMagFilter = FILTER_LINEAR;
						break;
					default:
						samplerDesc.mMagFilter = (FilterType)sampler.mag_filter;
					}

					switch (sampler.min_filter)
					{
					case 9728:
						samplerDesc.mMinFilter = FILTER_NEAREST;
						break;
					case 9729:
						samplerDesc.mMinFilter = FILTER_LINEAR;
						break;
					case 9984:
						samplerDesc.mMinFilter = FILTER_NEAREST;
						samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
						break;
					case 9985:
						samplerDesc.mMinFilter = FILTER_LINEAR;
						samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
						break;
					case 9986:
						samplerDesc.mMinFilter = FILTER_NEAREST;
						samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
						break;
					case 9987:
						samplerDesc.mMinFilter = FILTER_LINEAR;
						samplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
						break;
					default:
						samplerDesc.mMinFilter = (FilterType)sampler.mag_filter;
					}

					switch (sampler.wrap_s)
					{
					case 33071:
						samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
						break;
					case 33648:
						samplerDesc.mAddressU = ADDRESS_MODE_MIRROR;
						break;
					case 10497:
						samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
						break;
					default:
						samplerDesc.mAddressU = (AddressMode)sampler.wrap_s;
					}

					switch (sampler.wrap_t)
					{
					case 33071:
						samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
						break;
					case 33648:
						samplerDesc.mAddressV = ADDRESS_MODE_MIRROR;
						break;
					case 10497:
						samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
						break;
					default:
						samplerDesc.mAddressV = (AddressMode)sampler.wrap_t;
					}

					addSampler(pRenderer, &samplerDesc, &pSamplers[i]);

					uint64_t samplerID = (uint64_t)&model.data->samplers[i];
					samplerIDMap[samplerID] = (uint32_t)i;
				}
			}

			// add default sampler
			pSamplers.push_back(NULL);
			addSampler(pRenderer, &samplerDescDefault, &pSamplers.back());
			samplerIDMap[0] = uint32_t(pSamplers.size() - 1);

			size_t meshCount = model.mMeshArray.size();
			if (meshCount == 0)
				return false;

			modelProp.MeshBatches.reserve(meshCount);
			modelProp.WorldMatrix = mat4::identity();

			eastl::vector<Mesh> transparentMeshArray;
			eastl::vector<Mesh> opaqueMeshArray;
			transparentMeshArray.reserve(meshCount);
			opaqueMeshArray.reserve(meshCount);

			for (int i = 0; i < meshCount; i++)
			{
				const cgltf_material * subMeshMaterial = model.mMeshArray[i].material;
				if (subMeshMaterial && subMeshMaterial->alpha_mode != 0)
				{
					transparentMeshArray.push_back(model.mMeshArray[i]);
				}
				else
				{
					opaqueMeshArray.push_back(model.mMeshArray[i]);
				}
			}

			transparentMeshArray.resize(transparentMeshArray.size());
			opaqueMeshArray.resize(opaqueMeshArray.size());

			eastl::vector<Mesh> modelMeshArray = opaqueMeshArray;
			modelMeshArray.insert(modelMeshArray.end(), transparentMeshArray.begin(), transparentMeshArray.end());

			transparentMeshArray.clear();
			opaqueMeshArray.clear();

			QuantizationParams qp = prepareQuantization(modelMeshArray, settings);

			float3 centerPosOffset = getOffsetToCenterModel(modelMeshArray, modelProp.WorldMatrix);

			for (int i = 0; i < meshCount; i++)
			{
				Mesh & subMesh = modelMeshArray[i];
				quantizeMesh(subMesh, settings, qp);
			}

			for (cgltf_size i = 0; i < model.data->materials_count; ++i)
			{
				quantizeMaterial(&model.data->materials[i]);
			}

			model.CenterPosition = vec3(0.0f, qp.pos_offset[1] / qp.pos_scale, 0.0f);

			for (int i = 0; i < meshCount; i++)
			{
				Mesh & subMesh = modelMeshArray[i];

				UniformBlock_ObjData meshConstantBufferData = {};
				meshConstantBufferData.mWorldMat = modelProp.WorldMatrix;
				meshConstantBufferData.InvTranspose = transpose(inverse(modelProp.WorldMatrix));
				meshConstantBufferData.unlit = 0;
				meshConstantBufferData.hasAlbedoMap = 0;
				meshConstantBufferData.hasNormalMap = 0;
				meshConstantBufferData.hasMetallicRoughnessMap = 0;
				meshConstantBufferData.hasAOMap = 0;
				meshConstantBufferData.hasEmissiveMap = 0;
				meshConstantBufferData.centerOffset[0] = centerPosOffset[0];
				meshConstantBufferData.centerOffset[1] = centerPosOffset[1];
				meshConstantBufferData.centerOffset[2] = centerPosOffset[2];
				meshConstantBufferData.centerOffset[3] = 0.0f;
				meshConstantBufferData.posOffset[0] = qp.pos_offset[0];
				meshConstantBufferData.posOffset[1] = qp.pos_offset[1];
				meshConstantBufferData.posOffset[2] = qp.pos_offset[2];
				meshConstantBufferData.posOffset[3] = 0.0f;
				meshConstantBufferData.posScale = qp.pos_scale;
				meshConstantBufferData.uvOffset[0] = qp.uv_offset[0];
				meshConstantBufferData.uvOffset[1] = qp.uv_offset[1];
				meshConstantBufferData.uvScale[0] = qp.uv_scale[0];
				meshConstantBufferData.uvScale[1] = qp.uv_scale[1];

				MeshBatch* pMeshBatch = (MeshBatch*)conf_placement_new<MeshBatch>(conf_calloc(1, sizeof(MeshBatch)));

				modelProp.MeshBatches.push_back(pMeshBatch);

				pMeshBatch->NoofIndices = (int)subMesh.indices.size();
				pMeshBatch->NoofUVSets = 0;

				cgltf_material * subMeshMaterial = subMesh.material;

				if (gLoadOptimizedModel)
				{
					meshConstantBufferData.uvOffset[0] =
						subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.offset[0];
					meshConstantBufferData.uvOffset[1] =
						subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.offset[1];
					meshConstantBufferData.uvScale[0] =
						subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.scale[0];
					meshConstantBufferData.uvScale[1] =
						subMeshMaterial->pbr_metallic_roughness.base_color_texture.transform.scale[1];
					meshConstantBufferData.uvScale[0] *= float((1 << gTexBits) - 1);
					meshConstantBufferData.uvScale[1] *= float((1 << gTexBits) - 1);
				}

				eastl::vector<uint16_t> vertexBaseColors;
				eastl::vector<uint16_t> vertexMetallicRoughness;
				eastl::vector<uint16_t> vertexAlphaSettings;

				eastl::vector<uint16_t> meshUVs;

				uint16_t materialBaseColor[4] = { 255, 255, 255, 255 };
				uint16_t materialMetallicRoughness[2] = { 255, 255 };
				uint16_t materialAlphaSettings[2] = { 0, 255 };

				// Vertex buffers for mesh
				{
					BufferLoadDesc desc = {};
					desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
					desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;

					if (subMeshMaterial)
					{
						meshConstantBufferData.unlit = subMeshMaterial->unlit;

						materialBaseColor[0] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[0]);
						materialBaseColor[1] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[1]);
						materialBaseColor[2] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[2]);
						materialBaseColor[3] = uint16_t(subMeshMaterial->pbr_metallic_roughness.base_color_factor[3]);

						materialMetallicRoughness[0] = uint16_t(subMeshMaterial->pbr_metallic_roughness.metallic_factor);
						materialMetallicRoughness[1] = uint16_t(subMeshMaterial->pbr_metallic_roughness.roughness_factor);

						materialAlphaSettings[0] = subMeshMaterial->alpha_mode;
						materialAlphaSettings[1] = uint16_t(subMeshMaterial->alpha_cutoff);
					}

					for (size_t h = 0; h < subMesh.streams.size(); ++h)
					{
						Stream const & stream = subMesh.streams[h];

						eastl::vector<uint16_t> float4Data;
						eastl::vector<uint16_t> float2Data;
						float4Data.resize(stream.data.size() * 4);
						float2Data.resize(stream.data.size() * 2);

						vertexMetallicRoughness.resize(stream.data.size() * 2);
						vertexAlphaSettings.resize(stream.data.size() * 2);

						for (size_t k = 0; k < stream.data.size(); ++k)
						{
							uint16_t f0 = (uint16_t)stream.data[k].f[0];
							uint16_t f1 = (uint16_t)stream.data[k].f[1];
							uint16_t f2 = (uint16_t)stream.data[k].f[2];
							uint16_t f3 = (uint16_t)stream.data[k].f[3];

							float4Data[k * 4 + 0] = f0;
							float4Data[k * 4 + 1] = f1;
							float4Data[k * 4 + 2] = f2;
							float4Data[k * 4 + 3] = f3;

							float2Data[k * 2 + 0] = f0;
							float2Data[k * 2 + 1] = f1;

							vertexMetallicRoughness[k * 2 + 0] = materialMetallicRoughness[0];
							vertexMetallicRoughness[k * 2 + 1] = materialMetallicRoughness[1];
							vertexAlphaSettings[k * 2 + 0] = materialAlphaSettings[0];
							vertexAlphaSettings[k * 2 + 1] = materialAlphaSettings[1];
						}

						if (stream.type == cgltf_attribute_type::cgltf_attribute_type_position)
						{
							pMeshBatch->NoofVertices = (int)stream.data.size();

							desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
							desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
							desc.pData = float4Data.data();
							desc.ppBuffer = &pMeshBatch->pPositionStream;
							addResource(&desc);

							// Used to store per vertex color if a color stream does not exist
							vertexBaseColors.resize(stream.data.size() * 4);

							desc.mDesc.mVertexStride = sizeof(uint16_t) * 2;
							desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
							desc.pData = vertexMetallicRoughness.data();
							desc.ppBuffer = &pMeshBatch->pMetallicRoughnessStream;
							addResource(&desc);

							desc.mDesc.mVertexStride = sizeof(uint16_t) * 2;
							desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
							desc.pData = vertexAlphaSettings.data();
							desc.ppBuffer = &pMeshBatch->pAlphaStream;
							addResource(&desc);
						}
						else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_normal)
						{
							pMeshBatch->NoofVertices = (int)stream.data.size();

							desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
							desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
							desc.pData = float4Data.data();
							desc.ppBuffer = &pMeshBatch->pNormalStream;
							addResource(&desc);
						}
						else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_texcoord)
						{
							pMeshBatch->NoofVertices = (int)stream.data.size();

							meshUVs.clear();
							meshUVs.insert(meshUVs.end(), float2Data.begin(), float2Data.end());
						}
						else if (stream.type == cgltf_attribute_type::cgltf_attribute_type_color)
						{
							pMeshBatch->NoofVertices = (int)stream.data.size();

							desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
							desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
							desc.pData = float4Data.data();
							desc.ppBuffer = &pMeshBatch->pBaseColorStream;
							addResource(&desc);
						}
					}
				}

				// Index buffer for mesh
				{
					// Index buffer for the scene
					BufferLoadDesc desc = {};
					desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
					desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					desc.mDesc.mIndexType = INDEX_TYPE_UINT32;
					desc.mDesc.mSize = sizeof(uint) * (uint)subMesh.indices.size();
					desc.pData = subMesh.indices.data();
					desc.ppBuffer = &pMeshBatch->pIndicesStream;
					addResource(&desc);
				}

				// Textures for mesh
				{
					if (!pMeshBatch->pUVStream)
					{
						BufferLoadDesc desc = {};
						desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
						desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
						desc.mDesc.mVertexStride = sizeof(uint16_t) * 2;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						if (meshUVs.empty())
						{
							meshUVs.resize(pMeshBatch->NoofVertices * 2);
						}
						desc.pData = meshUVs.data();
						desc.ppBuffer = &pMeshBatch->pUVStream;
						addResource(&desc);
					}

					if (!pMeshBatch->pBaseColorStream)
					{
						for (size_t k = 0; k < vertexBaseColors.size(); k += 4)
						{
							vertexBaseColors[k] = materialBaseColor[0];
							vertexBaseColors[k + 1] = materialBaseColor[1];
							vertexBaseColors[k + 2] = materialBaseColor[2];
							vertexBaseColors[k + 3] = materialBaseColor[3];
						}

						BufferLoadDesc desc = {};
						desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
						desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
						desc.mDesc.mVertexStride = sizeof(uint16_t) * 4;
						desc.mDesc.mSize = pMeshBatch->NoofVertices * desc.mDesc.mVertexStride;
						desc.pData = vertexBaseColors.data();
						desc.ppBuffer = &pMeshBatch->pBaseColorStream;
						addResource(&desc);
					}

					{
						uint32_t materialIndex = (uint32_t)textureIndexforMaterial.size();

						if (materialIDMap.find(subMesh.materialID) == materialIDMap.end())
						{
							materialIDMap[subMesh.materialID] = materialIndex;

							TextureLoadDesc textureLoadDesc = {};

							pMaterialTextures.push_back(pTextureBlack);
							textureIndexforMaterial.push_back(materialIndex);
							pMeshBatch->SamplerIndex[0] = samplerIDMap[0];

							if (subMeshMaterial && subMeshMaterial->pbr_metallic_roughness.base_color_texture.texture)
							{
								cgltf_texture* texture = subMeshMaterial->pbr_metallic_roughness.base_color_texture.texture;

								pMeshBatch->SamplerIndex[0] = samplerIDMap[(uint64_t)texture->sampler];

								eastl::string baseColorTextureName(texture->image->uri);
								eastl_size_t extensionPos = baseColorTextureName.find_last_of('.');
								baseColorTextureName.resize(extensionPos);
#if USE_BASIS					
								baseColorTextureName.append(".basis");
#endif
								pMaterialTextures.back() = NULL;
								PathHandle baseColorTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, baseColorTextureName.c_str());
								textureLoadDesc.pFilePath = baseColorTexturePath;
								textureLoadDesc.ppTexture = &pMaterialTextures.back();
								addResource(&textureLoadDesc, true);
							}

							pMaterialTextures.push_back(pTextureBlack);
							textureIndexforMaterial.push_back(materialIndex + 1);
							pMeshBatch->SamplerIndex[1] = samplerIDMap[0];

							if (subMeshMaterial && subMeshMaterial->normal_texture.texture)
							{
								cgltf_texture* texture = subMeshMaterial->normal_texture.texture;

								pMeshBatch->SamplerIndex[1] = samplerIDMap[(uint64_t)texture->sampler];

								eastl::string normalTextureName(texture->image->uri);
								eastl_size_t extensionPos = normalTextureName.find_last_of('.');
								normalTextureName.resize(extensionPos);
#if USE_BASIS				
								normalTextureName.append(".basis");
#endif
								PathHandle mrTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, normalTextureName.c_str());

								pMaterialTextures.back() = NULL;
								textureLoadDesc.pFilePath = mrTexturePath;
								textureLoadDesc.ppTexture = &pMaterialTextures.back();
								addResource(&textureLoadDesc, true);
							}

							pMaterialTextures.push_back(pTextureBlack);
							textureIndexforMaterial.push_back(materialIndex + 2);
							pMeshBatch->SamplerIndex[2] = samplerIDMap[0];

							if (subMeshMaterial && subMeshMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture)
							{
								cgltf_texture* texture = subMeshMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture;

								pMeshBatch->SamplerIndex[2] = samplerIDMap[(uint64_t)texture->sampler];

								eastl::string mrTextureName(texture->image->uri);
								eastl_size_t extensionPos = mrTextureName.find_last_of('.');
								mrTextureName.resize(extensionPos);
#if USE_BASIS						
								mrTextureName.append(".basis");
#endif
								PathHandle mrTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, mrTextureName.c_str());

								pMaterialTextures.back() = NULL;
								textureLoadDesc.pFilePath = mrTexturePath;
								textureLoadDesc.ppTexture = &pMaterialTextures.back();
								addResource(&textureLoadDesc, true);
							}

							pMaterialTextures.push_back(pTextureBlack);
							textureIndexforMaterial.push_back(materialIndex + 3);
							pMeshBatch->SamplerIndex[3] = samplerIDMap[0];

							if (subMeshMaterial && subMeshMaterial->occlusion_texture.texture)
							{
								cgltf_texture* texture = subMeshMaterial->occlusion_texture.texture;

								pMeshBatch->SamplerIndex[3] = samplerIDMap[(uint64_t)texture->sampler];

								eastl::string aoTextureName(texture->image->uri);
								eastl_size_t extensionPos = aoTextureName.find_last_of('.');
								aoTextureName.resize(extensionPos);
#if USE_BASIS						
								aoTextureName.append(".basis");
#endif
								PathHandle aoTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, aoTextureName.c_str());

								pMaterialTextures.back() = NULL;
								textureLoadDesc.pFilePath = aoTexturePath;
								textureLoadDesc.ppTexture = &pMaterialTextures.back();
								addResource(&textureLoadDesc, true);
							}

							pMaterialTextures.push_back(pTextureBlack);
							textureIndexforMaterial.push_back(materialIndex + 4);
							pMeshBatch->SamplerIndex[4] = samplerIDMap[0];

							if (subMeshMaterial && subMeshMaterial->emissive_texture.texture)
							{
								cgltf_texture* texture = subMeshMaterial->emissive_texture.texture;

								pMeshBatch->SamplerIndex[4] = samplerIDMap[(uint64_t)texture->sampler];

								eastl::string emmisiveTextureName(texture->image->uri);
								eastl_size_t extensionPos = emmisiveTextureName.find_last_of('.');
								emmisiveTextureName.resize(extensionPos);
#if USE_BASIS						
								emmisiveTextureName.append(".basis");
#endif
								PathHandle emissiveTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, emmisiveTextureName.c_str());

								pMaterialTextures.back() = NULL;
								textureLoadDesc.pFilePath = emissiveTexturePath;
								textureLoadDesc.ppTexture = &pMaterialTextures.back();
								addResource(&textureLoadDesc, true);
							}

						}

						pMeshBatch->MaterialIndex = materialIDMap[subMesh.materialID];
					}
				}

				//set constant buffer for mesh
				{
					int matIndex = pMeshBatch->MaterialIndex;

					if (pMaterialTextures[matIndex] != pTextureBlack)
						meshConstantBufferData.hasAlbedoMap = 1;
					if (pMaterialTextures[matIndex + 1] != pTextureBlack)
						meshConstantBufferData.hasNormalMap = 1;
					if (pMaterialTextures[matIndex + 2] != pTextureBlack)
						meshConstantBufferData.hasMetallicRoughnessMap = 1;
					if (pMaterialTextures[matIndex + 3] != pTextureBlack)
						meshConstantBufferData.hasAOMap = 1;
					if (pMaterialTextures[matIndex + 4] != pTextureBlack)
						meshConstantBufferData.hasEmissiveMap = 1;

					BufferLoadDesc desc = {};
					desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
					desc.mDesc.mSize = sizeof(UniformBlock_ObjData);
					desc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
					desc.pData = &meshConstantBufferData;
					desc.ppBuffer = &pMeshBatch->pConstantBuffer;
					addResource(&desc);
				}
			}

			lod.push_back(currentMesh);
		}

		pGuiWindow->RemoveWidget(pSelectLodWidget);
		gMaxLod = max((int)validFileLists.size() - 1, 0);
		pSelectLodWidget = pGuiWindow->AddWidget(SliderIntWidget("LOD", &gCurrentLod, 0, gMaxLod));
		finishResourceLoading();
		return true;
	}

	static bool AddDescriptorSets()
	{
		uint32_t totalSetCount = 0;
		for (uint32_t i = 0; i < (uint32_t)gLODs.size(); ++i)
			for (uint32_t j = 0; j < (uint32_t)gLODs[i].modelProp.MeshBatches.size(); ++j, ++totalSetCount);

		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 4 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet[0]);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet[1]);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, totalSetCount + 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet[2]);

		setDesc = { pRootSignatureDemo, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDemo[0]);
		setDesc = { pRootSignatureDemo, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDemo[1]);
		setDesc = { pRootSignatureDemo, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, totalSetCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDemo[2]);

		return true;
	}

	static void RemoveDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSet[0]);
		removeDescriptorSet(pRenderer, pDescriptorSet[1]);
		removeDescriptorSet(pRenderer, pDescriptorSet[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetDemo[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetDemo[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetDemo[2]);
	}

	static void PrepareDescriptorSets()
	{
		// Shadow
		{
			DescriptorData params[1] = {};
			params[0].pName = "ShadowTexture";
			params[0].ppTextures = &pShadowRT->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSet[0], 1, params);

			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pForwardRT->pTexture;
			updateDescriptorSet(pRenderer, 1, pDescriptorSet[0], 1, params);

			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pPostProcessRT->pTexture;
			updateDescriptorSet(pRenderer, 2, pDescriptorSet[0], 1, params);

			params[0].pName = "sceneTexture";
			params[0].ppTextures = &pTextureBlack;
			updateDescriptorSet(pRenderer, 3, pDescriptorSet[0], 1, params);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData params[2] = {};
				params[0].pName = "cbPerFrame";
				params[0].ppBuffers = &pFloorUniformBuffer[i];
				params[1].pName = "ShadowUniformBuffer";
				params[1].ppBuffers = &pShadowUniformBuffer[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSet[1], 2, params);
			}

			params[0].pName = "cbPerProp";
			params[0].ppBuffers = &pFloorShadowUniformBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSet[2], 1, params);

			uint32_t setIndex = 1;
			for (uint32_t i = 0; i < (uint32_t)gLODs.size(); ++i)
			{
				for (uint32_t j = 0; j < (uint32_t)gLODs[i].modelProp.MeshBatches.size(); ++j, ++setIndex)
				{
					params[0].ppBuffers = &gLODs[i].modelProp.MeshBatches[j]->pConstantBuffer;
					updateDescriptorSet(pRenderer, setIndex, pDescriptorSet[2], 1, params);
					gLODs[i].modelProp.MeshBatches[j]->ShadowSetIndex = setIndex;
				}
			}
		}
		// Shading
		{
			DescriptorData params[2] = {};
			params[0].pName = "ShadowTexture";
			params[0].ppTextures = &pShadowRT->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetDemo[0], 1, params);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cbPerPass";
				params[0].ppBuffers = &pUniformBuffer[i];
				params[1].pName = "ShadowUniformBuffer";
				params[1].ppBuffers = &pShadowUniformBuffer[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetDemo[1], 2, params);
			}

			//bind textures
			const char* texNames[5] =
			{
				"albedoMap",
				"normalMap",
				"metallicRoughnessMap",
				"aoMap",
				"emissiveMap"
			};

			const char* samplerNames[5] =
			{
				"samplerAlbedo",
				"samplerNormal",
				"samplerMR",
				"samplerAO",
				"samplerEmissive"
			};

			uint32_t setIndex = 0;
			for (uint32_t i = 0; i < (uint32_t)gLODs.size(); ++i)
			{
				for (uint32_t j = 0; j < (uint32_t)gLODs[i].modelProp.MeshBatches.size(); ++j, ++setIndex)
				{
					MeshBatch* mesh = gLODs[i].modelProp.MeshBatches[j];

					DescriptorData params[15] = {};
					params[0].pName = "cbPerProp";
					params[0].ppBuffers = &mesh->pConstantBuffer;

					int materialIndex = mesh->MaterialIndex;

					for (int j = 0; j < 5; ++j)
					{
						params[1 + j].pName = texNames[j];
						uint textureId = gLODs[i].textureIndexforMaterial[materialIndex + j];
						params[1 + j].ppTextures = &gLODs[i].pMaterialTextures[textureId];

						params[6 + j].pName = samplerNames[j];
						params[6 + j].ppSamplers = &gLODs[i].pSamplers[mesh->SamplerIndex[j]];
					}

					updateDescriptorSet(pRenderer, setIndex, pDescriptorSetDemo[2], 11, params);

					mesh->DemoSetIndex = setIndex;
				}
			}
		}
	}

	static void RemoveShaderResources()
	{
		RemoveDescriptorSets();

		removeShader(pRenderer, pShaderZPass);
		removeShader(pRenderer, pShaderZPass_NonOptimized);
		removeShader(pRenderer, pVignetteShader);
		removeShader(pRenderer, pFloorShader);
		removeShader(pRenderer, pMeshOptDemoShader);
		removeShader(pRenderer, pFXAAShader);
		removeShader(pRenderer, pWaterMarkShader);

		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pRootSignatureDemo);
	}

	static void RemoveModelDependentResources()
	{
		RemoveShaderResources();

		for (size_t i = 0; i < gLODs.size(); ++i)
		{
			Model & model = gLODs[i].model;
			PropData & modelProp = gLODs[i].modelProp;
			eastl::vector<Sampler*> & pSamplers = gLODs[i].pSamplers;
			eastl::unordered_map<uint64_t, uint32_t> & samplerIDMap = gLODs[i].samplerIDMap;
			eastl::vector<uint32_t> & textureIndexforMaterial = gLODs[i].textureIndexforMaterial;
			eastl::unordered_map<uint32_t, uint32_t> & materialIDMap = gLODs[i].materialIDMap;
			eastl::vector<Texture*> & pMaterialTextures = gLODs[i].pMaterialTextures;

			for (MeshBatch* meshBatch : modelProp.MeshBatches)
			{
				removeResource(meshBatch->pConstantBuffer);
				removeResource(meshBatch->pIndicesStream);
				removeResource(meshBatch->pNormalStream);
				removeResource(meshBatch->pPositionStream);
				removeResource(meshBatch->pUVStream);
				removeResource(meshBatch->pBaseColorStream);
				removeResource(meshBatch->pMetallicRoughnessStream);
				removeResource(meshBatch->pAlphaStream);
				meshBatch->~MeshBatch();
				conf_free(meshBatch);
			}

			modelProp.MeshBatches.clear();

			for (uint i = 0; i < pMaterialTextures.size(); ++i)
			{
				if (pMaterialTextures[i] && pMaterialTextures[i] != pTextureBlack)
				{
					removeResource(pMaterialTextures[i]);
				}
			}

			pMaterialTextures.clear();
			materialIDMap.clear();
			textureIndexforMaterial.clear();

			for (uint i = 0; i < pSamplers.size(); ++i)
			{
				removeSampler(pRenderer, pSamplers[i]);
			}

			pSamplers.clear();
			samplerIDMap.clear();

			model.mMeshArray.clear();
			cgltf_free(model.data);
		}

		gLODs.clear();// resize(1);
		//gCurrentLod = 0;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		exitProfiler();

		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pShadowUniformBuffer[i]);
			removeResource(pUniformBuffer[i]);
			removeResource(pFloorUniformBuffer[i]);
		}

		removeResource(pFloorShadowUniformBuffer);

		removeDepthState(pDepthStateForRendering);
		removeBlendState(pBlendStateAlphaBlend);
		removeRasterizerState(pRasterizerStateCullBack);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeSampler(pRenderer, pBilinearClampSampler);

		removeResource(TriangularVB);

		removeResource(pFloorVB);
		removeResource(pFloorIB);

		removeResource(pTextureBlack);
		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);

#if defined(__linux__) || defined(__ANDROID__)
		gModelFiles.set_capacity(0);
		gDropDownWidgetData.set_capacity(0);
#endif

		gModelFile = NULL;
		gGuiModelToLoad = NULL;
		gLODs.set_capacity(0);
	}

	static void LoadPipelines()
	{
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 6;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R16G16B16A16_UINT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R16G16B16A16_SINT;
		vertexLayout.mAttribs[1].mBinding = 1;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 0;// 3 * sizeof(float);
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R16G16_UINT;
		vertexLayout.mAttribs[2].mBinding = 2;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 0;// 6 * sizeof(float);

		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_COLOR;
		vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R16G16B16A16_UINT;
		vertexLayout.mAttribs[3].mBinding = 3;
		vertexLayout.mAttribs[3].mLocation = 3;
		vertexLayout.mAttribs[3].mOffset = 0;// 3 * sizeof(float);
		vertexLayout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD1;
		vertexLayout.mAttribs[4].mFormat = TinyImageFormat_R16G16_UINT;
		vertexLayout.mAttribs[4].mBinding = 4;
		vertexLayout.mAttribs[4].mLocation = 4;
		vertexLayout.mAttribs[4].mOffset = 0;// 6 * sizeof(float);
		vertexLayout.mAttribs[5].mSemantic = SEMANTIC_TEXCOORD2;
		vertexLayout.mAttribs[5].mFormat = TinyImageFormat_R16G16_UINT;
		vertexLayout.mAttribs[5].mBinding = 5;
		vertexLayout.mAttribs[5].mLocation = 5;
		vertexLayout.mAttribs[5].mOffset = 0;// 6 * sizeof(float);

		PipelineDesc desc = {};

		/************************************************************************/
		// Setup the resources needed for shadow map
		/************************************************************************/
		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
			shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			shadowMapPipelineSettings.mRenderTargetCount = 0;
			shadowMapPipelineSettings.pDepthState = pDepthStateForRendering;
			shadowMapPipelineSettings.mDepthStencilFormat = pShadowRT->mDesc.mFormat;
			shadowMapPipelineSettings.mSampleCount = pShadowRT->mDesc.mSampleCount;
			shadowMapPipelineSettings.mSampleQuality = pShadowRT->mDesc.mSampleQuality;
			shadowMapPipelineSettings.pRootSignature = pRootSignature;
			shadowMapPipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			shadowMapPipelineSettings.pShaderProgram = pShaderZPass;
			shadowMapPipelineSettings.pVertexLayout = &vertexLayout;
			addPipeline(pRenderer, &desc, &pPipelineShadowPass);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthStateForRendering;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
			pipelineSettings.pBlendState = pBlendStateAlphaBlend;
			pipelineSettings.pColorFormats = &pForwardRT->mDesc.mFormat;
			pipelineSettings.mSampleCount = pForwardRT->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pForwardRT->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignatureDemo;
			pipelineSettings.pVertexLayout = &vertexLayout;
			pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			pipelineSettings.pShaderProgram = pMeshOptDemoShader;
			addPipeline(pRenderer, &desc, &pMeshOptDemoPipeline);
		}

		VertexLayout screenTriangle_VertexLayout = {};

		screenTriangle_VertexLayout.mAttribCount = 2;
		screenTriangle_VertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		screenTriangle_VertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		screenTriangle_VertexLayout.mAttribs[0].mBinding = 0;
		screenTriangle_VertexLayout.mAttribs[0].mLocation = 0;
		screenTriangle_VertexLayout.mAttribs[0].mOffset = 0;
		screenTriangle_VertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		screenTriangle_VertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		screenTriangle_VertexLayout.mAttribs[1].mBinding = 0;
		screenTriangle_VertexLayout.mAttribs[1].mLocation = 1;
		screenTriangle_VertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
			shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			shadowMapPipelineSettings.mRenderTargetCount = 0;
			shadowMapPipelineSettings.pDepthState = pDepthStateForRendering;
			shadowMapPipelineSettings.mDepthStencilFormat = pShadowRT->mDesc.mFormat;
			shadowMapPipelineSettings.mSampleCount = pShadowRT->mDesc.mSampleCount;
			shadowMapPipelineSettings.mSampleQuality = pShadowRT->mDesc.mSampleQuality;
			shadowMapPipelineSettings.pRootSignature = pRootSignature;
			shadowMapPipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			shadowMapPipelineSettings.pShaderProgram = pShaderZPass_NonOptimized;
			shadowMapPipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			addPipeline(pRenderer, &desc, &pPipelineShadowPass_NonOPtimized);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = pDepthStateForRendering;
			pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
			pipelineSettings.pBlendState = pBlendStateAlphaBlend;
			pipelineSettings.pColorFormats = &pForwardRT->mDesc.mFormat;
			pipelineSettings.mSampleCount = pForwardRT->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pForwardRT->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignature;
			pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			pipelineSettings.pShaderProgram = pFloorShader;
			addPipeline(pRenderer, &desc, &pFloorPipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pBlendState = pBlendStateAlphaBlend;
			pipelineSettings.pColorFormats = &pPostProcessRT->mDesc.mFormat;
			pipelineSettings.mSampleCount = pPostProcessRT->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pPostProcessRT->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignature;
			pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			pipelineSettings.pShaderProgram = pVignetteShader;
			addPipeline(pRenderer, &desc, &pVignettePipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pBlendState = NULL;
			pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignature;
			pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			pipelineSettings.pShaderProgram = pFXAAShader;
			addPipeline(pRenderer, &desc, &pFXAAPipeline);
		}

		{
			desc = {};
			desc.mType = PIPELINE_TYPE_GRAPHICS;
			GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
			pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			pipelineSettings.mRenderTargetCount = 1;
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pBlendState = pBlendStateAlphaBlend;
			pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
			pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
			pipelineSettings.pRootSignature = pRootSignature;
			pipelineSettings.pVertexLayout = &screenTriangle_VertexLayout;
			pipelineSettings.pRasterizerState = pRasterizerStateCullBack;
			pipelineSettings.pShaderProgram = pWaterMarkShader;
			addPipeline(pRenderer, &desc, &pWaterMarkPipeline);
		}
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addRenderTargets())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#if defined(TARGET_IOS) || defined(__ANDROID__)
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		InitModelDependentResources();

		LoadPipelines();

		float wmHeight = min(mSettings.mWidth, mSettings.mHeight) * 0.09f;
		float wmWidth = wmHeight * 2.8077f;

		float widthGap = wmWidth * 2.0f / (float)mSettings.mWidth;
		float heightGap = wmHeight * 2.0f / (float)mSettings.mHeight;

		float pixelGap = 80.0f;
		float widthRight = 1.0f - pixelGap / (float)mSettings.mWidth;
		float heightDown = -1.0f + pixelGap / (float)mSettings.mHeight;

		float screenWaterMarkPoints[] = {
			widthRight - widthGap,	heightDown + heightGap, 0.5f, 0.0f, 0.0f,
			widthRight - widthGap,	heightDown,				0.5f, 0.0f, 1.0f,
			widthRight,				heightDown + heightGap, 0.5f, 1.0f, 0.0f,

			widthRight,				heightDown + heightGap, 0.5f, 1.0f, 0.0f,
			widthRight - widthGap,	heightDown,				0.5f, 0.0f, 1.0f,
			widthRight,				heightDown,				0.5f, 1.0f, 1.0f
		};

		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		screenQuadVbDesc.mDesc.mSize = screenQuadVbDesc.mDesc.mVertexStride * 6;
		screenQuadVbDesc.pData = screenWaterMarkPoints;
		screenQuadVbDesc.ppBuffer = &WaterMarkVB;
		addResource(&screenQuadVbDesc);

		return true;
	}

	static void RemovePipelines()
	{
		removePipeline(pRenderer, pPipelineShadowPass_NonOPtimized);
		removePipeline(pRenderer, pPipelineShadowPass);
		removePipeline(pRenderer, pVignettePipeline);
		removePipeline(pRenderer, pFloorPipeline);
		removePipeline(pRenderer, pMeshOptDemoPipeline);
		removePipeline(pRenderer, pFXAAPipeline);
		removePipeline(pRenderer, pWaterMarkPipeline);
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);
		waitForFences(pRenderer, gImageCount, pRenderCompleteFences);

		RemoveModelDependentResources();

		unloadProfiler();
		gAppUI.Unload();

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Unload();
#endif

		removeResource(WaterMarkVB);

		RemovePipelines();

		removeSwapChain(pRenderer, pSwapChain);

		removeRenderTarget(pRenderer, pPostProcessRT);
		removeRenderTarget(pRenderer, pForwardRT);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pShadowRT);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

#if !defined(__ANDROID__) && !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != bToggleVSync)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 3.0f;
		mat4 projMat = mat4::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.001f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat;
		gUniformData.mCameraPosition = vec4(pCameraController->getViewPosition(), 1.0f);

		for (uint i = 0; i < gTotalLightCount; ++i)
		{
			gUniformData.mLightColor[i] = vec4(float((gLightColor[i] >> 24) & 0xff),
				float((gLightColor[i] >> 16) & 0xff),
				float((gLightColor[i] >> 8) & 0xff),
				float((gLightColor[i] >> 0) & 0xff)) / 255.0f;

			gUniformData.mLightColor[i].setW(gLightColorIntensity[i]);
		}

		float Azimuth = (PI / 180.0f) * gLightDirection.x;
		float Elevation = (PI / 180.0f) * (gLightDirection.y - 180.0f);

		vec3 sunDirection = normalize(vec3(cosf(Azimuth)*cosf(Elevation), sinf(Elevation), sinf(Azimuth)*cosf(Elevation)));

		gUniformData.mLightDirection[0] = vec4(sunDirection, 0.0f);
		// generate 2nd, 3rd light from the main light
		gUniformData.mLightDirection[1] = vec4(-sunDirection.getX(), sunDirection.getY(), -sunDirection.getZ(), 0.0f);
		gUniformData.mLightDirection[2] = vec4(-sunDirection.getX(), -sunDirection.getY(), -sunDirection.getZ(), 0.0f);

		gUniformData.mQuantizationParams = int4(gPosBits, gTexBits, gNrmBits, 0);

		gFloorUniformBlock.projViewMat = gUniformData.mProjectView;
		gFloorUniformBlock.worldMat = mat4::scale(vec3(3.0f));
		gFloorUniformBlock.screenSize = vec4((float)mSettings.mWidth, (float)mSettings.mHeight, 1.0f / mSettings.mWidth, bVignetting ? 1.0f : 0.0f);


		/************************************************************************/
		// Light Matrix Update - for shadow map
		/************************************************************************/

		vec3 lightPos = sunDirection * 4.0f;
		pLightView->moveTo(lightPos);
		pLightView->lookAt(vec3(0.0f));

		mat4 lightView = pLightView->getViewMatrix();
		//perspective as spotlight, for future use. TODO: Use a frustum fitting algorithm to maximise effective resolution!
		const float shadowRange = 2.7f;
		const float shadowHalfRange = shadowRange * 0.5f;
		mat4 lightProjMat = mat4::orthographicReverseZ(-shadowRange, shadowRange, -shadowRange, shadowRange, shadowRange * 0.5f, shadowRange * 4.0f);

		gShadowUniformData.ViewProj = lightProjMat * lightView;

		/************************************************************************/
		/************************************************************************/


		if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
		{
			toggleProfiler();
			bPrevToggleMicroProfiler = bToggleMicroProfiler;
		}

		gAppUI.Update(deltaTime);
	}

	void PostDrawUpdate()
	{

#if defined(__ANDROID__) || defined(__LINUX__)
		if (guiModelToLoadIndex != modelToLoadIndex)
		{
			modelToLoadIndex = guiModelToLoadIndex;
			gGuiModelToLoad = gModelFiles[modelToLoadIndex];
		}
#endif
		if (!fsPathsEqual(gGuiModelToLoad, gModelFile))
		{
			if (fsFileExists(gGuiModelToLoad))
			{
				gModelFile = gGuiModelToLoad;

				Unload();
				Load();
			}
			else
			{
				gGuiModelToLoad = gModelFile;
			}
		}
		gCurrentLod = min(gCurrentLod, gMaxLod);

		const LOD &currentLodMesh = gLODs[gCurrentLod];
		const Model &model = currentLodMesh.model;

		float distanceFromCamera = length(pCameraController->getViewPosition() - model.CenterPosition);

		if (distanceFromCamera < 1.0f)
			gCurrentLod = 0;
		else
			gCurrentLod = min((int)log2(pow((float)distanceFromCamera, 0.6f) + 1.0f), gMaxLod);
	}

	static void SelectModelFunc(const Path* path, void* pathPtr) {
		PathHandle *outputPath = (PathHandle*)pathPtr;

		if (path) {
			*outputPath = fsCopyPath(path);
		}
	}

	static void LoadNewModel()
	{
		eastl::vector<const char*> extFilter;
		extFilter.push_back("gltf");
		extFilter.push_back("glb");

		PathHandle meshDir = fsCopyPathForResourceDirectory(RD_MESHES);

		fsShowOpenFileDialog("Select model to load", meshDir, SelectModelFunc, &gGuiModelToLoad, "Model File", &extFilter[0], extFilter.size());
	}

	static void LoadLOD()
	{
		waitQueueIdle(pGraphicsQueue);

		eastl::vector<const char*> extFilter;
		extFilter.push_back("gltf");
		extFilter.push_back("glb");

		PathHandle meshDir = fsCopyPathForResourceDirectory(RD_MESHES);

		fsShowOpenFileDialog("Select model to load", meshDir, SelectModelFunc, &gGuiModelToLoad, "Model File", &extFilter[0], extFilter.size());
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&shaderCbv);

		RenderTarget* pRenderTarget = NULL;

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		vec4 bgColor = vec4(float((gBackroundColor >> 24) & 0xff),
			float((gBackroundColor >> 16) & 0xff),
			float((gBackroundColor >> 8) & 0xff),
			float((gBackroundColor >> 0) & 0xff)) / 255.0f;


		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		drawShadowMap(cmd);

		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0].r = bgColor.getX();
			loadActions.mClearColorValues[0].g = bgColor.getY();
			loadActions.mClearColorValues[0].b = bgColor.getZ();
			loadActions.mClearColorValues[0].a = bgColor.getW();
			loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
			loadActions.mClearDepth.depth = 0.0f;
			loadActions.mClearDepth.stencil = 0;

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Floor", true);

			pRenderTarget = pForwardRT;

			TextureBarrier barriers[] =
			{
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
				{ pShadowRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
			};

			cmdResourceBarrier(cmd, 0, NULL, 3, barriers);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pFloorPipeline);

			// Update uniform buffers
			BufferUpdateDesc Cb = { pFloorUniformBuffer[gFrameIndex], &gFloorUniformBlock };
			updateResource(&Cb);

			cmdBindDescriptorSet(cmd, 0, pDescriptorSet[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet[1]);

			cmdBindVertexBuffer(cmd, 1, &pFloorVB, NULL);
			cmdBindIndexBuffer(cmd, pFloorIB, 0);

			cmdDrawIndexed(cmd, 6, 0, 0);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		//// draw scene

		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Scene", true);

			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pMeshOptDemoPipeline);

			cmdBindDescriptorSet(cmd, 0, pDescriptorSetDemo[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetDemo[1]);

			for (MeshBatch* mesh : gLODs[gCurrentLod].modelProp.MeshBatches)
			{
				cmdBindDescriptorSet(cmd, mesh->DemoSetIndex, pDescriptorSetDemo[2]);

				Buffer* pVertexBuffers[] = { mesh->pPositionStream,
					mesh->pNormalStream,
					mesh->pUVStream,
					mesh->pBaseColorStream,
					mesh->pMetallicRoughnessStream,
					mesh->pAlphaStream };

				cmdBindVertexBuffer(cmd, 6, pVertexBuffers, NULL);

				cmdBindIndexBuffer(cmd, mesh->pIndicesStream, 0);

				cmdDrawIndexed(cmd, mesh->NoofIndices, 0, 0);
			}

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Vignetting", true);

			pRenderTarget = pPostProcessRT;

			TextureBarrier barriers[] = {
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pForwardRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
			};

			cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pVignettePipeline);

			cmdBindDescriptorSet(cmd, 1, pDescriptorSet[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet[1]);

			cmdBindVertexBuffer(cmd, 1, &TriangularVB, NULL);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		{
			TextureBarrier barriers[] =
			{
				{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pPostProcessRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
			};

			cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0].r = 0.0f;
			loadActions.mClearColorValues[0].g = 0.0f;
			loadActions.mClearColorValues[0].b = 0.0f;
			loadActions.mClearColorValues[0].a = 0.0f;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "FXAA", true);

			cmdBindPipeline(cmd, pFXAAPipeline);

			FXAAINFO FXAAinfo;
			FXAAinfo.ScreenSize = vec2((float)mSettings.mWidth, (float)mSettings.mHeight);
			FXAAinfo.Use = bToggleFXAA ? 1 : 0;

			cmdBindDescriptorSet(cmd, 2, pDescriptorSet[0]);
			cmdBindPushConstants(cmd, pRootSignature, "FXAARootConstant", &FXAAinfo);

			cmdBindVertexBuffer(cmd, 1, &TriangularVB, NULL);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}


		if (bScreenShotMode)
		{
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Water Mark", true);

			cmdBindPipeline(cmd, pWaterMarkPipeline);

			cmdBindDescriptorSet(cmd, 3, pDescriptorSet[0]);

			cmdBindVertexBuffer(cmd, 1, &WaterMarkVB, NULL);
			cmdDraw(cmd, 6, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		if (!bScreenShotMode)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
			static HiresTimer gTimer;
			gTimer.GetUSec(true);

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
#if defined(TARGET_IOS) || defined(__ANDROID__)
			//gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

			gAppUI.DrawText(
				cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)
			gAppUI.DrawText(
				cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
				&gFrameTimeDraw);
			gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#endif

			cmdDrawProfiler();

			gAppUI.Gui(pGuiWindow);
			gAppUI.Draw(cmd);

			gAppUI.Gui(pGuiGraphics);
			gAppUI.Draw(cmd);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		TextureBarrier Finalbarriers[] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, Finalbarriers);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		static int frameCount = 0;
		frameCount++;
		flipProfiler();

		PostDrawUpdate();
	}

	const char* GetName() { return "08_GltfViewer"; }

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

	bool addRenderTargets()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mFormat = TinyImageFormat_R8G8B8A8_UNORM;

		vec4 bgColor = vec4(float((gBackroundColor >> 24) & 0xff),
			float((gBackroundColor >> 16) & 0xff),
			float((gBackroundColor >> 8) & 0xff),
			float((gBackroundColor >> 0) & 0xff)) / 255.0f;

		RT.mClearValue.r = bgColor.getX();
		RT.mClearValue.g = bgColor.getY();
		RT.mClearValue.b = bgColor.getZ();
		RT.mClearValue.a = bgColor.getW();

		RT.mWidth = mSettings.mWidth;
		RT.mHeight = mSettings.mHeight;

		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.pDebugName = L"Render Target";
		addRenderTarget(pRenderer, &RT, &pForwardRT);

		RT = {};
		RT.mArraySize = 1;
		RT.mDepth = 1;
		RT.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		RT.mWidth = mSettings.mWidth;
		RT.mHeight = mSettings.mHeight;
		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.pDebugName = L"Post Process Render Target";
		addRenderTarget(pRenderer, &RT, &pPostProcessRT);

		return pForwardRT != NULL && pPostProcessRT != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		/************************************************************************/
		// Shadow Map Render Target
		/************************************************************************/

		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue.depth = 0.0f;
		shadowRTDesc.mClearValue.stencil = 0;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
		shadowRTDesc.mWidth = SHADOWMAP_RES;
		shadowRTDesc.mHeight = SHADOWMAP_RES;
		shadowRTDesc.mSampleCount = (SampleCount)SHADOWMAP_MSAA_SAMPLES;
		shadowRTDesc.mSampleQuality = 0;    // don't need higher quality sample patterns as the texture will be blurred heavily
		shadowRTDesc.pDebugName = L"Shadow Map RT";

		addRenderTarget(pRenderer, &shadowRTDesc, &pShadowRT);

		return pDepthBuffer != NULL && pShadowRT != NULL;
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
};

DEFINE_APPLICATION_MAIN(MeshOptimization)