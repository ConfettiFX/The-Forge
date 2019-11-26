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

#define MAX_PLANETS 20    // Does not affect test, just for allocating space in uniform block. Must match with shader.
//#define GARUANTEE_PAGE_SYNC

//assimp
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

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
#include "../../../../Common_3/Renderer/ResourceLoader.h"

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

const uint32_t			gImageCount = 3;
bool								gMicroProfiler = false;
bool								bPrevToggleMicroProfiler = false;
const int						gSphereResolution = 30;    // Increase for higher resolution spheres
const float					gSphereDiameter = 0.5f;
const uint					gNumPlanets = 8;        // Sun, Mercury -> Neptune, Pluto, Moon
const uint					gTimeOffset = 2400000;    // For visually better starting locations
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
CmdPool*						pCmdPool = NULL;
Cmd**								ppCmds = NULL;

Queue*						  pComputeQueue = NULL;
CmdPool*						pComputeCmdPool = NULL;
Cmd**								ppComputeCmds = NULL;

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

Shader*							pFillPageShader = NULL;
Pipeline*						pFillPagePipeline = NULL;

Shader*							pDebugShader = NULL;
Pipeline*						pDebugPipeline = NULL;

Shader*							pSkyBoxDrawShader = NULL;
Buffer*							pSkyBoxVertexBuffer = NULL;
Pipeline*						pSkyBoxDrawPipeline = NULL;
RootSignature*			pRootSignature = NULL;
Sampler*						pSamplerSkyBox = NULL;
Texture*						pSkyBoxTextures[6];

Texture*						pVirtualTexture[gNumPlanets];
Buffer*							pVirtualTextureInfo[gNumPlanets] = { NULL };
Buffer*							pPageCountInfo[gNumPlanets] = { NULL };

Buffer*							pDebugInfo = NULL;

DescriptorSet*			pDescriptorSetTexture = { NULL };
DescriptorSet*			pDescriptorSetUniforms = { NULL };
VirtualJoystickUI		gVirtualJoystick;
DepthState*					pDepth = NULL;
RasterizerState*		pSkyboxRast = NULL;
RasterizerState*		pSphereRast = NULL;

BlendState*					pSunBlend;
BlendState*					pSaturnBlend;

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

char*								gPlanetName[] = {"8k_sun.svt", "8k_mercury.svt", "8k_venus.svt", "8k_earth.svt", "16k_moon.svt", "8k_mars.svt", "8k_jupiter.svt", "8k_saturn.svt" };

struct MeshVertex
{
	float3 mPos;
	float3 mNormal;
	float2 mUv;
};

struct Mesh
{
	Buffer*                   pVertexBuffer = NULL;
	Buffer*                   pIndexBuffer = NULL;
	eastl::vector<uint32_t> materialID;
	struct CmdParam
	{
		uint32_t indexCount, startIndex, startVertex;
	};
	eastl::vector<CmdParam> cmdArray;
};

Mesh        gSphere;
Mesh				gSaturn;
ICameraController*	pCameraController = NULL;

/// UI
UIApp								gAppUI;
GpuProfiler*				pGpuProfiler = NULL;

//const char*					pSkyBoxImageFileNames[] = { "Skybox_right1", "Skybox_left2", "Skybox_top3", "Skybox_bottom4", "Skybox_front5", "Skybox_back6" };

TextDrawDesc				gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

GuiComponent*				pGui = NULL;

class VirtualTexture : public IApp
{
public:
	bool Init()
	{
        // FILE PATHS
        PathHandle programDirectory = fsCopyProgramDirectoryPath();
        if (!fsPlatformUsesBundledResources())
        {
            PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/18_VirtualTexture");
            fsSetResourceDirectoryRootPath(resourceDirRoot);
            
            fsSetRelativePathForResourceDirectory(RD_TEXTURES,				"../../../../Art/SparseTextures");
            fsSetRelativePathForResourceDirectory(RD_MESHES,					"../../UnitTestResources/Meshes");
            fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS,		"../../UnitTestResources/Fonts");
            fsSetRelativePathForResourceDirectory(RD_ANIMATIONS,			"../../UnitTestResources/Animation");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT,	"../../../../Middleware_3/Text");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI,		"../../../../Middleware_3/UI");
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

		queueDesc = {};
		queueDesc.mType = CMD_POOL_COMPUTE;
		addQueue(pRenderer, &queueDesc, &pComputeQueue);
		addCmdPool(pRenderer, pComputeQueue, false, &pComputeCmdPool);
		addCmd_n(pComputeCmdPool, false, gImageCount, &ppComputeCmds);

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
			PathHandle textureFilePath = fsCopyPathInResourceDirectory(RD_TEXTURES, pSkyBoxImageFileNames[i]);
			TextureLoadDesc textureDesc = {};
			textureDesc.pFilePath = textureFilePath;
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			addResource(&textureDesc, true);
		}
*/
		for (int i = 1; i < gNumPlanets; ++i)
		{
			TextureLoadDesc textureLoadDesc = {};
			PathHandle virtualTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, gPlanetName[i]);

			textureLoadDesc.pFilePath = virtualTexturePath;
			textureLoadDesc.ppTexture = &pVirtualTexture[i];
			addResource(&textureLoadDesc, true);
			virtualTexturePath.~PathHandle();
		}
		

		if (!gVirtualJoystick.Init(pRenderer, "circlepad", RD_TEXTURES))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		//ShaderLoadDesc skyShader = {};
		//skyShader.mStages[0] = { "skybox.vert", NULL, 0, RD_SHADER_SOURCES };
		//skyShader.mStages[1] = { "skybox.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc basicShader = {};
		basicShader.mStages[0] = { "basic.vert", NULL, 0, RD_SHADER_SOURCES };
		basicShader.mStages[1] = { "basic.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc debugShader = {};
		debugShader.mStages[0] = { "debug.vert", NULL, 0, RD_SHADER_SOURCES };
		debugShader.mStages[1] = { "debug.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc sunShader = {};
		sunShader.mStages[0] = { "basic.vert", NULL, 0, RD_SHADER_SOURCES };
		sunShader.mStages[1] = { "sun.frag", NULL, 0, RD_SHADER_SOURCES };

		//addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &basicShader, &pSphereShader);
		addShader(pRenderer, &debugShader, &pDebugShader);		
		addShader(pRenderer, &sunShader, &pSunShader);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateDesc.mSrcFactors[0] = BC_ONE;
		blendStateDesc.mDstFactors[0] = BC_ONE;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateDesc, &pSunBlend);

		blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateDesc, &pSaturnBlend);

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

		

		ShaderLoadDesc fillPageShader = {};
		fillPageShader.mStages[0] = { "fillPage.comp", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &fillPageShader, &pFillPageShader);

		Shader*           computeShaders[] = { pFillPageShader };
		rootDesc = {};
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = computeShaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureCompute);		

		desc = { pRootSignatureCompute, DESCRIPTOR_UPDATE_FREQ_NONE, gNumPlanets };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetComputeFix);
		desc = { pRootSignatureCompute, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * gNumPlanets };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetComputePerFrame);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pSkyboxRast);

		RasterizerStateDesc sphereRasterizerStateDesc = {};
		sphereRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &sphereRasterizerStateDesc, &pSphereRast);


		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

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
		conf_free(pSpherePoints);
		*/

/////////////////////////////////////
//						Load Sphere			     
/////////////////////////////////////

		AssimpImporter        importer;

	{
			AssimpImporter::Model model;
			PathHandle sceneFullPath = fsCopyPathInResourceDirectory(RD_MESHES, "sphereHires.obj");

			if (!importer.ImportModel(sceneFullPath, &model))
			{
				LOGF(LogLevel::eERROR, "Failed to load %s", fsGetPathFileName(sceneFullPath).buffer);
				return 0;
			}

			uint32_t meshCount = (uint32_t)model.mMeshArray.size();

			Mesh& mesh = gSphere;
			MeshVertex* pVertices = NULL;
			uint32_t*   pIndices = NULL;

			mesh.materialID.resize(meshCount);
			mesh.cmdArray.resize(meshCount);

			uint32_t totalVertexCount = 0;
			uint32_t totalIndexCount = 0;
			for (uint32_t i = 0; i < meshCount; i++)
			{
				AssimpImporter::Mesh subMesh = model.mMeshArray[i];

				mesh.materialID[i] = subMesh.mMaterialId;

				uint32_t vertexCount = (uint32_t)subMesh.mPositions.size();
				uint32_t indexCount = (uint32_t)subMesh.mIndices.size();

				mesh.cmdArray[i] = { indexCount, totalIndexCount, totalVertexCount };

				pVertices = (MeshVertex*)conf_realloc(pVertices, sizeof(MeshVertex) * (totalVertexCount + vertexCount));
				pIndices = (uint32_t*)conf_realloc(pIndices, sizeof(uint32_t) * (totalIndexCount + indexCount));

				for (uint32_t j = 0; j < vertexCount; j++)
				{
					pVertices[totalVertexCount++] = { subMesh.mPositions[j], subMesh.mNormals[j], subMesh.mUvs[j] };
				}

				for (uint32_t j = 0; j < indexCount; j++)
				{
					pIndices[totalIndexCount++] = subMesh.mIndices[j];
				}
			}

			// Vertex position buffer for the scene
			BufferLoadDesc vbPosDesc = {};
			vbPosDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			vbPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			vbPosDesc.mDesc.mVertexStride = sizeof(MeshVertex);
			vbPosDesc.mDesc.mSize = totalVertexCount * sizeof(MeshVertex);
			vbPosDesc.pData = pVertices;
			vbPosDesc.ppBuffer = &mesh.pVertexBuffer;
			vbPosDesc.mDesc.pDebugName = L"Vertex Position Buffer Desc for Sponza";
			addResource(&vbPosDesc);

			// Index buffer for the scene
			BufferLoadDesc ibDesc = {};
			ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
			ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
			ibDesc.mDesc.mSize = sizeof(uint32_t) * totalIndexCount;
			ibDesc.pData = pIndices;
			ibDesc.ppBuffer = &mesh.pIndexBuffer;
			ibDesc.mDesc.pDebugName = L"Index Buffer Desc for Sponza";
			addResource(&ibDesc);

			conf_free(pVertices);
			conf_free(pIndices);
	}

	{
		AssimpImporter::Model model;
		PathHandle sceneFullPath = fsCopyPathInResourceDirectory(RD_MESHES, "saturn.obj");

		if (!importer.ImportModel(sceneFullPath, &model))
		{
			LOGF(LogLevel::eERROR, "Failed to load %s", fsGetPathFileName(sceneFullPath).buffer);
			return 0;
		}

		uint32_t meshCount = (uint32_t)model.mMeshArray.size();

		Mesh& mesh = gSaturn;
		MeshVertex* pVertices = NULL;
		uint32_t*   pIndices = NULL;

		mesh.materialID.resize(meshCount);
		mesh.cmdArray.resize(meshCount);

		uint32_t totalVertexCount = 0;
		uint32_t totalIndexCount = 0;
		for (uint32_t i = 0; i < meshCount; i++)
		{
			AssimpImporter::Mesh subMesh = model.mMeshArray[i];

			mesh.materialID[i] = subMesh.mMaterialId;

			uint32_t vertexCount = (uint32_t)subMesh.mPositions.size();
			uint32_t indexCount = (uint32_t)subMesh.mIndices.size();

			mesh.cmdArray[i] = { indexCount, totalIndexCount, totalVertexCount };

			pVertices = (MeshVertex*)conf_realloc(pVertices, sizeof(MeshVertex) * (totalVertexCount + vertexCount));
			pIndices = (uint32_t*)conf_realloc(pIndices, sizeof(uint32_t) * (totalIndexCount + indexCount));

			for (uint32_t j = 0; j < vertexCount; j++)
			{
				pVertices[totalVertexCount++] = { subMesh.mPositions[j], subMesh.mNormals[j], subMesh.mUvs[j] };
			}

			for (uint32_t j = 0; j < indexCount; j++)
			{
				pIndices[totalIndexCount++] = subMesh.mIndices[j];
			}
		}

		// Vertex position buffer for the scene
		BufferLoadDesc vbPosDesc = {};
		vbPosDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbPosDesc.mDesc.mVertexStride = sizeof(MeshVertex);
		vbPosDesc.mDesc.mSize = totalVertexCount * sizeof(MeshVertex);
		vbPosDesc.pData = pVertices;
		vbPosDesc.ppBuffer = &mesh.pVertexBuffer;
		vbPosDesc.mDesc.pDebugName = L"Vertex Position Buffer Desc for Sponza";
		addResource(&vbPosDesc);

		// Index buffer for the scene
		BufferLoadDesc ibDesc = {};
		ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		ibDesc.mDesc.mSize = sizeof(uint32_t) * totalIndexCount;
		ibDesc.pData = pIndices;
		ibDesc.ppBuffer = &mesh.pIndexBuffer;
		ibDesc.mDesc.pDebugName = L"Index Buffer Desc for Sponza";
		addResource(&ibDesc);

		conf_free(pVertices);
		conf_free(pIndices);
	}


///////////////////////////////////

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
		skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
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
			ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&ubDesc);
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
			addResource(&vtInfoDesc);

			BufferLoadDesc ptInfoDesc = {};
			ptInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ptInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			ptInfoDesc.mDesc.mSize = sizeof(uint)*4;
			//ptInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			ptInfoDesc.pData = NULL;
			ptInfoDesc.ppBuffer = &pPageCountInfo[i];
			addResource(&ptInfoDesc);
		}

		BufferLoadDesc debugInfoDesc = {};
		debugInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		debugInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		debugInfoDesc.mDesc.mSize = sizeof(uint) * 4;
		debugInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		debugInfoDesc.pData = NULL;
		debugInfoDesc.ppBuffer = &pDebugInfo;
		addResource(&debugInfoDesc);		

		finishResourceLoading();

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

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(140.0f, 320.0f);
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY() * 0.5f);

		pGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);

		pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &gMicroProfiler));

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
    addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

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
					params[1].ppBuffers = &pVirtualTexture[j]->mVisibility;
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
				computeParams[0].ppBuffers = &pVirtualTexture[j]->mPrevVisibility;

				computeParams[1].pName = "PageTableBuffer";
				computeParams[1].ppBuffers = &pVirtualTexture[j]->mVisibility;

				computeParams[2].pName = "RemovePageTableBuffer";
				computeParams[2].ppBuffers = &pVirtualTexture[j]->mRemovePage;

				computeParams[3].pName = "AlivePageTableBuffer";
				computeParams[3].ppBuffers = &pVirtualTexture[j]->mAlivePage;

				computeParams[4].pName = "RemovePageCountBuffer";
				computeParams[4].ppBuffers = &pVirtualTexture[j]->mRemovePageCount;

				computeParams[5].pName = "AlivePageCountBuffer";
				computeParams[5].ppBuffers = &pVirtualTexture[j]->mAlivePageCount;

				updateDescriptorSet(pRenderer, i * gNumPlanets + j, pDescriptorSetComputePerFrame, 6, computeParams);
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

		removeResource(pSkyBoxVertexBuffer);

		for (int i = 1; i < gNumPlanets; ++i)
		{
			removeResource(pVirtualTexture[i]);
		}

		removeResource(gSphere.pIndexBuffer);
		removeResource(gSphere.pVertexBuffer);
		
		removeResource(gSaturn.pIndexBuffer);
		removeResource(gSaturn.pVertexBuffer);		

		removeBlendState(pSunBlend);
		removeBlendState(pSaturnBlend);

		gSphere.materialID.set_capacity(0);
		gSphere.cmdArray.set_capacity(0);

		gSaturn.materialID.set_capacity(0);
		gSaturn.cmdArray.set_capacity(0);
		
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

		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pRootSignatureCompute);

		removeDepthState(pDepth);
		removeRasterizerState(pSphereRast);
		removeRasterizerState(pSkyboxRast);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeFence(pRenderer, pComputeCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeCmd_n(pComputeCmdPool, gImageCount, ppComputeCmds);
		removeCmdPool(pRenderer, pComputeCmdPool);		

		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);

		removeQueue(pGraphicsQueue);
		removeQueue(pComputeQueue);

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

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		//layout and pipeline for sphere draw
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
		vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats

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
		pipelineSettings.pShaderProgram = pSphereShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pSphereRast;
		addPipeline(pRenderer, &desc, &pSpherePipeline);

		{
			pipelineSettings.pDepthState = NULL;
			pipelineSettings.pShaderProgram = pDebugShader;
			addPipeline(pRenderer, &desc, &pDebugPipeline);
		}

		{
			pipelineSettings.pDepthState = pDepth;
			pipelineSettings.pShaderProgram = pSunShader;
			pipelineSettings.pBlendState = pSunBlend;
			pipelineSettings.pRasterizerState = pSkyboxRast;
			addPipeline(pRenderer, &desc, &pSunPipeline);
		}

		{
			pipelineSettings.pDepthState = pDepth;
			pipelineSettings.pShaderProgram = pSphereShader;
			pipelineSettings.pBlendState = pSaturnBlend;
			pipelineSettings.pRasterizerState = pSkyboxRast;
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
			cpipelineSettings.pShaderProgram = pFillPageShader;
			cpipelineSettings.pRootSignature = pRootSignatureCompute;
			addPipeline(pRenderer, &computeDesc, &pFillPagePipeline);
		}

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfiler();
		gAppUI.Unload();

		gVirtualJoystick.Unload();

		//removePipeline(pRenderer, pSkyBoxDrawPipeline);		
		removePipeline(pRenderer, pSpherePipeline);
		removePipeline(pRenderer, pDebugPipeline);
		removePipeline(pRenderer, pSunPipeline);
		removePipeline(pRenderer, pSaturnPipeline);

		removePipeline(pRenderer, pFillPagePipeline);		

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
		static float currentTime = gTimeOffset;

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
				gUniformVirtualTextureInfo[i].Width = pVirtualTexture[i]->mDesc.mWidth;
				gUniformVirtualTextureInfo[i].Height = pVirtualTexture[i]->mDesc.mHeight;
				gUniformVirtualTextureInfo[i].pageWidth = (uint)pVirtualTexture[i]->mSparseVirtualTexturePageWidth;
				gUniformVirtualTextureInfo[i].pageHeight = (uint)pVirtualTexture[i]->mSparseVirtualTexturePageHeight;
			}

			gUniformVirtualTextureInfo[i].ID = i;
			gUniformVirtualTextureInfo[i].CameraPos = vec4(pCameraController->getViewPosition(), 0.0f);
			

			gUniformVirtualTextureInfo[i].DebugMode = gDebugMode ? 1: 0;

			BufferUpdateDesc virtualTextureInfoCbv = { pVirtualTextureInfo[i], &gUniformVirtualTextureInfo[i] };
			updateResource(&virtualTextureInfoCbv);
		}

		
	
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

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex], &gUniformDataSky };
		updateResource(&skyboxViewProjCbv);

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

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE }
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

		eastl::vector<BufferBarrier> virtualBufferBarrier;
		eastl::vector<TextureBarrier> virtualTextureBarrier;

		TextureBarrier barriers2[] = {
					{ pVirtualTexture[1], RESOURCE_STATE_SHADER_RESOURCE },
					{ pVirtualTexture[2], RESOURCE_STATE_SHADER_RESOURCE },
					{ pVirtualTexture[3], RESOURCE_STATE_SHADER_RESOURCE },
					{ pVirtualTexture[4], RESOURCE_STATE_SHADER_RESOURCE },
					{ pVirtualTexture[5], RESOURCE_STATE_SHADER_RESOURCE },
					{ pVirtualTexture[6], RESOURCE_STATE_SHADER_RESOURCE },
					{ pVirtualTexture[7], RESOURCE_STATE_SHADER_RESOURCE }
		};
		cmdResourceBarrier(cmd, 0, NULL, 7, barriers2);
//#if defined(VULKAN)
		//cmdResourceBarrier(cmd, 0, NULL, (uint32_t)virtualTextureBarrier.size(), virtualTextureBarrier.data());
//#endif
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
    
	{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Planets", true);

			cmdBindPipeline(cmd, pSpherePipeline);

			//// draw planets
			for (uint32_t j = 1; j < gNumPlanets - 1; ++j)
			{
				cmdBindDescriptorSet(cmd, j, pDescriptorSetTexture);			
				cmdBindDescriptorSet(cmd, gNumPlanets * gFrameIndex + j + gImageCount, pDescriptorSetUniforms);		
			
				cmdBindVertexBuffer(cmd, 1, &gSphere.pVertexBuffer, NULL);
				cmdBindIndexBuffer(cmd, gSphere.pIndexBuffer, 0);
				cmdDrawIndexed(cmd, gSphere.cmdArray[0].indexCount, 0, 0);
			}		

			cmdBindPipeline(cmd, pSaturnPipeline);

			cmdBindDescriptorSet(cmd, gNumPlanets - 1, pDescriptorSetTexture);
			cmdBindDescriptorSet(cmd, gNumPlanets * gFrameIndex + gNumPlanets - 1 + gImageCount, pDescriptorSetUniforms);		
		
			cmdBindVertexBuffer(cmd, 1, &gSaturn.pVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, gSaturn.pIndexBuffer, 0);
			cmdDrawIndexed(cmd, gSaturn.cmdArray[0].indexCount, 0, 0);

			cmdBindPipeline(cmd, pSunPipeline);

			cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
			cmdBindDescriptorSet(cmd, gNumPlanets * gFrameIndex + gImageCount, pDescriptorSetUniforms);
			cmdBindVertexBuffer(cmd, 1, &gSphere.pVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, gSphere.pIndexBuffer, 0);
			cmdDrawIndexed(cmd, gSphere.cmdArray[0].indexCount, 0, 0);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
	
	if(gShowUI)
	{
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
    cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		gAppUI.DrawText(cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)
		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("Update Virtual Texture %f ms", gTimer.GetUSecAverage() / 1000.0f - (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);

    gAppUI.DrawText(
      cmd, float2(8, 65), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
      &gFrameTimeDraw);

    gAppUI.DrawDebugGpuProfile(cmd, float2(8, 90), pGpuProfiler, NULL);
#endif

    cmdDrawProfiler();

    gAppUI.Gui(pGui);

		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
    cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers);
	}
    cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();	

		// Get visibility info
		if (gAccuFrameIndex % gFrequency == 0)
		{
			// Extract only alive page
			Cmd* pCmdCompute = NULL;

			pCmdCompute = ppComputeCmds[gFrameIndex];
			beginCmd(pCmdCompute);

			for (int i = 1; i < gNumPlanets; ++i)
			{
				eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)pVirtualTexture[i]->pPages;

				struct pageCountSt
				{
					uint maxPageCount;
					uint pageOffset;
					uint pad1;
					uint pad2;
				}pageCountSt;

	#if defined(GARUANTEE_PAGE_SYNC)
				const uint32_t* pThreadGroupSize = pFillPageShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;

				const uint32_t Dispatch[3] = { pThreadGroupSize[0],
																			 pThreadGroupSize[1],
																			 pThreadGroupSize[2] };

				pageCountSt.maxPageCount = pThreadGroupSize[0];
				pageCountSt.pageOffset = 0;

				int PageCount = (uint)pPageTable->size();

				while (PageCount > 0)
				{				
					BufferUpdateDesc pageCountInfoCbv = { pPageCountInfo, &pageCountSt };
					updateResource(&pageCountInfoCbv);

					pCmdCompute = ppComputeCmds[gFrameIndex];
					beginCmd(pCmdCompute);

					cmdBindPipeline(pCmdCompute, pFillPagePipeline);
					cmdBindDescriptorSet(pCmdCompute, 0, pDescriptorSetComputeFix);
					cmdBindDescriptorSet(pCmdCompute, gFrameIndex, pDescriptorSetComputePerFrame);

					cmdDispatch(pCmdCompute, 1, 1, 1);

					endCmd(pCmdCompute);
					queueSubmit(pComputeQueue, 1, &pCmdCompute, NULL, 0, NULL, 0, NULL);
					waitQueueIdle(pComputeQueue);

					PageCount -= pageCountSt.maxPageCount;
					pageCountSt.pageOffset += pageCountSt.maxPageCount;
					pageCountSt.maxPageCount = min((uint)PageCount, pageCountSt.maxPageCount);
				}
	#else
				pageCountSt.maxPageCount = (uint)pPageTable->size();
				pageCountSt.pageOffset = 0;

				BufferUpdateDesc pageCountInfoCbv = { pPageCountInfo[i], &pageCountSt };
				updateResource(&pageCountInfoCbv);

				cmdBindPipeline(pCmdCompute, pFillPagePipeline);
				cmdBindDescriptorSet(pCmdCompute, i, pDescriptorSetComputeFix);
				cmdBindDescriptorSet(pCmdCompute, gFrameIndex * gNumPlanets + i, pDescriptorSetComputePerFrame);

				const uint32_t* pThreadGroupSize = pFillPageShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;

				const uint32_t Dispatch[3] = { (uint32_t)ceil((float)pageCountSt.maxPageCount / (float)pThreadGroupSize[0]),
																			 pThreadGroupSize[1],
																			 pThreadGroupSize[2] };

				cmdDispatch(pCmdCompute, Dispatch[0], Dispatch[1], Dispatch[2]);				
	#endif				
			}

			endCmd(pCmdCompute);

			queueSubmit(pComputeQueue, 1, &pCmdCompute, NULL, 0, NULL, 0, NULL);
			waitQueueIdle(pComputeQueue);

			for (int i = 1; i < gNumPlanets; ++i)
			{
				// Then update
				TextureUpdateDesc virtualTextureUpdateDesc;
				virtualTextureUpdateDesc.pTexture = pVirtualTexture[i];
				//updateResource(&virtualTextureUpdateDesc, true);
				updateVirtualTexture(pRenderer, pGraphicsQueue, &virtualTextureUpdateDesc);
			}
		}

		gAccuFrameIndex++;
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
};

DEFINE_APPLICATION_MAIN(VirtualTexture)