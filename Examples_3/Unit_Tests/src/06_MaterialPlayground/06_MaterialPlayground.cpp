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

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[] =
{
	"../../../src/06_MaterialPlayground/",							// FSR_BinShaders
	"../../../src/06_MaterialPlayground/",							// FSR_SrcShaders
	"",																// FSR_BinShaders_Common
	"",																// FSR_SrcShaders_Common
	"../../../UnitTestResources/",									// FSR_Textures
	"../../../UnitTestResources/",									// FSR_Meshes
	"../../../UnitTestResources/",									// FSR_Builtin_Fonts
	"../../../src/06_MaterialPlayground/",							// FSR_GpuConfig
	"",																// FSR_OtherFiles
};

#define LOAD_MATERIAL_BALL

//--------------------------------------------------------------------------------------------
// ENUM DEFINTIONS
//--------------------------------------------------------------------------------------------
typedef enum MaterialTypes
{
	MATERIAL_DEFAULT = 0,
	MATERIAL_METAL,
	MATERIAL_COUNT
} MaterialTypes;

typedef enum MetalTypes
{
	METAL_NONE = -1,
	METAL_RUSTED_IRON,
	METAL_COPPER,
	METAL_TITANIUM,
	METAL_GREASED_METAL,
	METAL_GOLD,
	METAL_COUNT
} MetalTypes;

typedef enum MeshResource
{
	MESH_MAT_BALL,
	MESH_CUBE,
	MESH_COUNT,
} MeshResource;

typedef enum MaterialTextures
{
	MATERIAL_TEXTURE_ALBEDO,
	MATERIAL_TEXTURE_NORMAL,
	MATERIAL_TEXTURE_METALLIC,
	MATERIAL_TEXTURE_ROUGHNESS,
	MATERIAL_TEXTURE_OCCLUSION,
	MATERIAL_TEXTURE_COUNT
} MaterialTextures;

//--------------------------------------------------------------------------------------------
// STRUCT DEFINTIONS
//--------------------------------------------------------------------------------------------
struct Vertex
{
	float3 mPos;
	float3 mNormal;
	float2 mUv;
};

typedef struct MeshData
{
	Buffer* pVertexBuffer = NULL;
	uint	mVertexCount = 0;
	Buffer* pIndexBuffer = NULL;
	uint	mIndexCount = 0;
} MeshData;

struct UniformCamData
{
	mat4 mProjectView;
	vec3 mCamPos;
};

struct UniformObjData
{
	mat4 mWorldMat;
	float mRoughness = 0.04f;
	float mMetallic = 0.0f;
	int objectId = -1;
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
	Light mLights[16];
	int mCurrAmountOfLights = 0;
};

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
const uint32_t		gImageCount = 3;
Renderer*			pRenderer = NULL;
Queue*				pGraphicsQueue = NULL;
CmdPool*			pCmdPool = NULL;
Cmd**				ppCmds = NULL;
CmdPool*			pUICmdPool = NULL;
Cmd**				ppUICmds = NULL;
SwapChain*			pSwapChain = NULL;
RenderTarget*		pRenderTargetDepth = NULL;
Fence*				pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*			pImageAcquiredSemaphore = NULL;
Semaphore*			pRenderCompleteSemaphores[gImageCount] = { NULL };
uint32_t			gFrameIndex = 0;

//--------------------------------------------------------------------------------------------
// THE FORGE OBJECTS
//--------------------------------------------------------------------------------------------
LogManager			gLogManager;
UIApp				gAppUI;
ICameraController*	pCameraController = NULL;
TextDrawDesc		gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
GpuProfiler*		pGpuProfiler = NULL;
GuiComponent*		pGuiWindow = NULL;
#ifdef TARGET_IOS
VirtualJoystickUI	gVirtualJoystick;
#endif

//--------------------------------------------------------------------------------------------
// RASTERIZER STATES
//--------------------------------------------------------------------------------------------
RasterizerState*	pRasterizerStateCullNone = NULL;

//--------------------------------------------------------------------------------------------
// DEPTH STATES
//--------------------------------------------------------------------------------------------
DepthState*			pDepthStateEnable = NULL;

//--------------------------------------------------------------------------------------------
// BLEND STATES
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// SAMPLERS
//--------------------------------------------------------------------------------------------
Sampler*			pSamplerBilinear = NULL;
Sampler*			pSamplerBilinearClamped = NULL;

//--------------------------------------------------------------------------------------------
// SHADERS
//--------------------------------------------------------------------------------------------
Shader*				pShaderSkybox = NULL;
Shader*				pShaderBRDF = NULL;

//--------------------------------------------------------------------------------------------
// ROOT SIGNATURES
//--------------------------------------------------------------------------------------------
RootSignature*		pRootSignatureSkybox = NULL;
RootSignature*		pRootSignatureBRDF = NULL;

//--------------------------------------------------------------------------------------------
// PIPELINES
//--------------------------------------------------------------------------------------------
Pipeline*			pPipelineSkybox = NULL;
Pipeline*			pPipelineBRDF = NULL;

//--------------------------------------------------------------------------------------------
// VERTEX BUFFERS
//--------------------------------------------------------------------------------------------
Buffer*				pVertexBufferSkybox = NULL;
Buffer*				pVertexBufferSphere = NULL;

//--------------------------------------------------------------------------------------------
// INDEX BUFFERS
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// MESHES
//--------------------------------------------------------------------------------------------
MeshData* pMeshes[MESH_COUNT] = { NULL };

//--------------------------------------------------------------------------------------------
// UNIFORM BUFFERS
//--------------------------------------------------------------------------------------------
Buffer*				pUniformBufferCamera[gImageCount] = { NULL };
Buffer*				pUniformBufferCameraSkybox[gImageCount] = { NULL };
Buffer*				pUniformBufferSurface = NULL;
Buffer*				pUniformBufferMatBall[gImageCount][METAL_COUNT];
Buffer*				pUniformBufferNamePlates[METAL_COUNT];
Buffer*				pUniformBufferLights = NULL;

//--------------------------------------------------------------------------------------------
// TEXTURES
//--------------------------------------------------------------------------------------------
const int gMaterialTextureCount = METAL_COUNT * MATERIAL_TEXTURE_COUNT;

Texture*			pTextureSkybox = NULL;
Texture*			pTextureBRDFIntegrationMap = NULL;
Texture*			pTextureIrradianceMap = NULL;
Texture*			pTextureSpecularMap = NULL;
Texture*			pTextureMaterialMaps[gMaterialTextureCount];

//--------------------------------------------------------------------------------------------
// UNIFORM DATA
//--------------------------------------------------------------------------------------------
UniformCamData		gUniformDataCamera;
UniformCamData		gUniformDataCameraSkybox;
UniformLightData	gUniformDataLights;
UniformObjData		gUniformDataObject;
UniformObjData		gUniformDataMatBall[METAL_COUNT];

//--------------------------------------------------------------------------------------------
// OTHER
//--------------------------------------------------------------------------------------------
bool				gVSyncEnabled = false;
uint32_t			gMaterialType = MATERIAL_METAL;

const int			gSphereResolution = 30; // Increase for higher resolution spheres
const float			gSphereDiameter = 0.5f;
int					gNumOfSpherePoints;
TextDrawDesc gMaterialPropDraw = TextDrawDesc(0, 0xff00ffff, 25);

const char*	 pTextureName[] =
{
	"albedoMap",
	"normalMap",
	"metallicMap",
	"roughnessMap",
	"aoMap"
};

static const char* metalEnumNames[] =
{
	"Rusted Iron",
	"Copper",
	"Greased Metal",
	"Gold",
	"Titanium",
	NULL
};

uint32_t gMetalMaterial = METAL_RUSTED_IRON;

mat4 gTextProjView;
tinystl::vector<mat4> gTextWorldMats;

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
			float theta = atan2f(normalize(topLeftPoint).getY(), normalize(topLeftPoint).getX());
			float phi = acosf(normalize(topLeftPoint).getZ());
			Vector3 textcoord1 = { (theta / (2 * PI)),(phi / PI),0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(botRightPoint));
			theta = atan2f(normalize(botRightPoint).getY(), normalize(botRightPoint).getX());
			phi = acosf(normalize(botRightPoint).getZ());
			textcoord1 = { (theta / (2 * PI)),(phi / PI),0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(topRightPoint));
			theta = atan2f(normalize(topRightPoint).getY(), normalize(topRightPoint).getX());
			phi = acosf(normalize(topRightPoint).getZ());
			textcoord1 = { (theta / (2 * PI)),(phi / PI),0.0f };
			uvs.push_back(textcoord1);


			// Bot left triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botLeftPoint);
			vertices.push_back(radius * botRightPoint);

			normals.push_back(normalize(topLeftPoint));
			theta = atan2f(normalize(topLeftPoint).getY(), normalize(topLeftPoint).getX());
			phi = acosf(normalize(topLeftPoint).getZ());
			textcoord1 = { (theta / (2 * PI)),(phi / PI),0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(botLeftPoint));
			theta = atan2f(normalize(botLeftPoint).getY(), normalize(botLeftPoint).getX());
			phi = acosf(normalize(botLeftPoint).getZ());
			textcoord1 = { (theta / (2 * PI)),(phi / PI),0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(botRightPoint));
			theta = atan2f(normalize(botRightPoint).getY(), normalize(botRightPoint).getX());
			phi = acosf(normalize(botRightPoint).getZ());
			textcoord1 = { (theta / (2 * PI)),(phi / PI),0.0f };
			uvs.push_back(textcoord1);
		}
	}

	*pNumberOfPoints = (uint32_t)vertices.size();
	(*ppPoints) = (Vertex *)conf_malloc(sizeof(Vertex) * (*pNumberOfPoints));

	for (uint32_t i = 0; i < (uint32_t)vertices.size(); i++)
	{
		Vertex vertex;
		vertex.mPos = float3(vertices[i].getX(), vertices[i].getY(), vertices[i].getZ());
		vertex.mNormal = float3(normals[i].getX(), normals[i].getY(), normals[i].getZ());

		float theta = atan2f(normals[i].getY(), normals[i].getX());
		float phi = acosf(normals[i].getZ());

		vertex.mUv.x = (theta / (2 * PI));
		vertex.mUv.y = (phi / PI);

		(*ppPoints)[i] = vertex;
	}
}

struct GuiController
{
	static void AddGui();
	static void UpdateDynamicUI();

	static MaterialTypes currentMaterialType;
};
MaterialTypes	GuiController::currentMaterialType;

class MaterialPlayground : public IApp
{
public:
	bool Init()
	{
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		// Create command pool and create a cmd buffer for each swapchain image
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

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts);


#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		CreateRasterizerStates();
		CreateDepthStates();
		CreateBlendStates();
		CreateSamplers();

		CreateShaders();
		CreateRootSignatures();

		CreatePBRMaps();
		LoadModels();
		LoadTextures();
		CreateResources();
		CreateUniformBuffers();

		finishResourceLoading();

		InitializeUniformBuffers();

		// Create UI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(5, 200.0f);
		guiDesc.mStartSize = vec2(450, 600);
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
		GuiController::AddGui();

		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };

#ifdef LOAD_MATERIAL_BALL
		vec3 camPos{ 0.0f, 8.0f, 30.0f };
#else
		vec3 camPos{ 0.0f, 0.0f, 10.0f };
#endif
		vec3 lookat{ 0 };

		pCameraController = createFpsCameraController(camPos, lookat);
		requestMouseCapture(true);

		pCameraController->setMotionParameters(camParameters);

		InputSystem::RegisterInputEvent(cameraInputEvent);
		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		destroyCameraController(pCameraController);

		removeDebugRendererInterface();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		DestroyUniformBuffers();
		DestroyResources();
		DestroyTextures();
		DestroyModels();
		DestroyPBRMaps();

		DestroyRootSignatures();
		DestroyShaders();

		DestroySamplers();
		DestroyBlendStates();
		DestroyDepthStates();
		DestroyRasterizerStates();

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
		removeGpuProfiler(pRenderer, pGpuProfiler);

		gAppUI.Exit();

		// Remove commands and command pool&
		removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
		removeCmdPool(pRenderer, pUICmdPool);

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

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], pRenderTargetDepth->mDesc.mFormat))
			return false;
#endif

		CreatePipelines();

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		DestroyPipelines();

		removeRenderTarget(pRenderer, pRenderTargetDepth);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(170.0f);
		}

		GuiController::UpdateDynamicUI();

		pCameraController->update(deltaTime);

		// Update camera
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformDataCamera.mProjectView = projMat * viewMat;
		gTextProjView = gUniformDataCamera.mProjectView;
		gUniformDataCamera.mCamPos = pCameraController->getViewPosition();

		viewMat.setTranslation(vec3(0));
		gUniformDataCameraSkybox = gUniformDataCamera;
		gUniformDataCameraSkybox.mProjectView = projMat * viewMat;


		int val = -1;
		gMaterialType == MATERIAL_DEFAULT ? (val = -1) : (val = 1);
		for (size_t totalBuf = 0; totalBuf < METAL_COUNT; ++totalBuf) 
		{
			gUniformDataMatBall[totalBuf].objectId = val;
		}

		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		// This will acquire the next swapchain image
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/
		for (size_t totalBuf = 0; totalBuf < METAL_COUNT; ++totalBuf)
		{
			BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[gFrameIndex][totalBuf], &gUniformDataMatBall[totalBuf] };
			updateResource(&objBuffUpdateDesc);
		}

		BufferUpdateDesc camBuffUpdateDesc = { pUniformBufferCamera[gFrameIndex], &gUniformDataCamera };
		updateResource(&camBuffUpdateDesc);

		BufferUpdateDesc skyboxViewProjCbv = { pUniformBufferCameraSkybox[gFrameIndex], &gUniformDataCameraSkybox };
		updateResource(&skyboxViewProjCbv);
		/************************************************************************/
		/************************************************************************/
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		tinystl::vector<Cmd*> allCmds;
		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barriers[] = 
		{
			{pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET},
			{pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE}
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		// Draw skybox
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineSkybox);
		DescriptorData skyParams[2] = {};
		skyParams[0].pName = "uniformBlock";
		skyParams[0].ppBuffers = &pUniformBufferCameraSkybox[gFrameIndex];
		skyParams[1].pName = "skyboxTex";
		skyParams[1].ppTextures = &pTextureSkybox;
		cmdBindDescriptors(cmd, pRootSignatureSkybox, 2, skyParams);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferSkybox, NULL);
		cmdDraw(cmd, 36, 0);

		// Draw the spheres.
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
		cmdBindPipeline(cmd, pPipelineBRDF);

		DescriptorData params[13] = {};
		params[0].pName = "cbCamera";
		params[0].ppBuffers = &pUniformBufferCamera[gFrameIndex];
		params[1].pName = "cbLights";
		params[1].ppBuffers = &pUniformBufferLights;
		params[2].pName = "brdfIntegrationMap";
		params[2].ppTextures = &pTextureBRDFIntegrationMap;
		params[3].pName = "irradianceMap";
		params[3].ppTextures = &pTextureIrradianceMap;
		params[4].pName = "specularMap";
		params[4].ppTextures = &pTextureSpecularMap;

		int matId = gMetalMaterial == METAL_NONE ? 0 : gMetalMaterial;
		int textureIndex = 0;

#ifdef LOAD_MATERIAL_BALL
		cmdBindVertexBuffer(cmd, 1, &pMeshes[MESH_MAT_BALL]->pVertexBuffer, NULL);
		cmdBindIndexBuffer(cmd, pMeshes[MESH_MAT_BALL]->pIndexBuffer, NULL);
#endif

		for (int i = 0; i < METAL_COUNT; ++i)
		{
			// Add the uniform buffer for eveWry sphere
			params[5].pName = "cbObject";
			params[5].ppBuffers = &pUniformBufferMatBall[gFrameIndex][i];

			//binding pbr material textures
			for (int j = 0; j <5; ++j) 
			{
				int index = j + 5 * i;
				textureIndex = matId + index;
				params[6 + j].pName = pTextureName[j];
				if (textureIndex >= gMaterialTextureCount)
				{
					LOGERROR("texture index greater than array size, setting it to default texture");
					textureIndex = matId + j;
				}
				params[6 + j].ppTextures = &pTextureMaterialMaps[textureIndex];
			}

			cmdBindDescriptors(cmd, pRootSignatureBRDF, 11, params);

#ifdef LOAD_MATERIAL_BALL

			cmdDrawIndexed(cmd, pMeshes[MESH_MAT_BALL]->mIndexCount, 0, 0);

#else
			cmdBindVertexBuffer(cmd, 1, &pVertexBufferSphere, NULL);
			cmdDrawInstanced(cmd, gNumOfSpherePoints, 0, 1, 0);
#endif
		}

#ifdef LOAD_MATERIAL_BALL
		cmdBindVertexBuffer(cmd, 1, &pMeshes[MESH_CUBE]->pVertexBuffer, NULL);
		cmdBindIndexBuffer(cmd, pMeshes[MESH_CUBE]->pIndexBuffer, NULL);

		params[5].pName = "cbObject";
		params[5].ppBuffers = &pUniformBufferSurface;

		for (int j = 0; j <5; ++j) 
		{
			params[6 + j].pName = pTextureName[j];
			params[6 + j].ppTextures = &pTextureMaterialMaps[j];
		}

		cmdBindDescriptors(cmd, pRootSignatureBRDF, 11, params);

		cmdDrawIndexed(cmd, pMeshes[MESH_CUBE]->mIndexCount, 0, 0);
#endif

		//draw the label plates
		if (gMaterialType == MATERIAL_METAL) 
		{

#ifdef LOAD_MATERIAL_BALL

			for (int j = 0; j <5; ++j)
			{
				params[6 + j].pName = pTextureName[j];
				params[6 + j].ppTextures = &pTextureMaterialMaps[j + 4];
			}

			for (int j = 0; j < METAL_COUNT; ++j) 
			{
				params[5].pName = "cbObject";
				params[5].ppBuffers = &pUniformBufferNamePlates[j];

				cmdBindDescriptors(cmd, pRootSignatureBRDF, 11, params);

				cmdDrawIndexed(cmd, pMeshes[MESH_CUBE]->mIndexCount, 0, 0);
			}
#endif
		}


		endCmd(cmd);
		allCmds.push_back(cmd);

		cmd = ppUICmds[gFrameIndex];
		beginCmd(cmd);

		// Prepare UI command buffers
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);


		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, pCameraController, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif
		//draw text
#ifdef LOAD_MATERIAL_BALL
		if (gMaterialType == MATERIAL_METAL) 
		{
			int metalEnumIndex = 0;
			for (int i = 0; i< METAL_COUNT; ++i) 
			{
				//if there are more objects than metalEnumNames
				metalEnumIndex = i >= METAL_COUNT ? METAL_RUSTED_IRON : i;
				drawDebugText(cmd, gTextProjView, gTextWorldMats[i], tinystl::string::format(metalEnumNames[metalEnumIndex]), &gMaterialPropDraw);
			}
		}
#endif
		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#ifndef METAL // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
#endif

#ifndef TARGET_IOS
		gAppUI.Gui(pGuiWindow);
#endif
		gAppUI.Draw(cmd);

		// Transition our texture to present state
		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);
		allCmds.push_back(cmd);

		queueSubmit(pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

		Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);
		}

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	tinystl::string GetName()
	{
		return "06_MaterialPlayground";
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
		swapChainDesc.mColorClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		swapChainDesc.mSrgb = false;
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
		depthRT.mWidth = mSettings.mWidth;

		addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);

		return pRenderTargetDepth != NULL;
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

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}


	//--------------------------------------------------------------------------------------------
	// INIT FUNCTIONS
	//--------------------------------------------------------------------------------------------
	void CreateRasterizerStates()
	{
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullNone);
	}

	void DestroyRasterizerStates()
	{
		removeRasterizerState(pRasterizerStateCullNone);
	}

	void CreateDepthStates()
	{
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepthStateEnable);
	}

	void DestroyDepthStates()
	{
		removeDepthState(pDepthStateEnable);
	}

	void CreateBlendStates()
	{

	}

	void DestroyBlendStates()
	{

	}

	void CreateSamplers()
	{
		SamplerDesc bilinearSamplerDesc = {};
		bilinearSamplerDesc.mMinFilter = FILTER_LINEAR;
		bilinearSamplerDesc.mMagFilter = FILTER_LINEAR;
		bilinearSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		bilinearSamplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
		bilinearSamplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
		bilinearSamplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
		addSampler(pRenderer, &bilinearSamplerDesc, &pSamplerBilinear);

		SamplerDesc bilinearClampedSamplerDesc = {};
		bilinearClampedSamplerDesc.mMinFilter = FILTER_LINEAR;
		bilinearClampedSamplerDesc.mMagFilter = FILTER_LINEAR;
		bilinearClampedSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		bilinearClampedSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		bilinearClampedSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		bilinearClampedSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		addSampler(pRenderer, &bilinearClampedSamplerDesc, &pSamplerBilinearClamped);
	}

	void DestroySamplers()
	{
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerBilinearClamped);
	}

	void CreateShaders()
	{
		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0, FSR_SrcShaders };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);
	}

	void DestroyShaders()
	{
		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderBRDF);
	}

	void CreateRootSignatures()
	{
		const char* pStaticSamplerNames[] = { "envSampler", "defaultSampler", "skyboxSampler" };
		Sampler* pStaticSamplers[] = { pSamplerBilinear, pSamplerBilinearClamped, pSamplerBilinear };
		uint numStaticSamplers = sizeof(pStaticSamplerNames) / sizeof(pStaticSamplerNames[0]);

		RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
		skyboxRootDesc.mStaticSamplerCount = numStaticSamplers;
		skyboxRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		skyboxRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignatureSkybox);

		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = numStaticSamplers;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSignatureBRDF);
	}

	void DestroyRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignatureSkybox);
		removeRootSignature(pRenderer, pRootSignatureBRDF);
	}

	void CreatePBRMaps()
	{
		// PBR Texture values (these values are mirrored on the shaders).
		const uint32_t			gBRDFIntegrationSize = 512;
		const uint32_t			gSkyboxSize = 1024;
		const uint32_t			gSkyboxMips = 11;
		const uint32_t			gIrradianceSize = 32;
		const uint32_t			gSpecularSize = 128;
		const uint32_t			gSpecularMips = (uint)log2(gSpecularSize) + 1;

		// Temporary resources that will be loaded on PBR preprocessing.
		Texture* pPanoSkybox = NULL;
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

		SamplerDesc samplerDesc =
		{
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		// Load the skybox panorama texture.
		TextureLoadDesc panoDesc = {};
		panoDesc.mRoot = FSR_Textures;
		panoDesc.mUseMipmaps = true;
		panoDesc.pFilename = "LA_Helipad.hdr";
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = ImageFormat::RGBA32F;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mSrgb = false;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pDebugName = L"skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pTextureSkybox;
		addResource(&skyboxLoadDesc);

		TextureDesc irrImgDesc = {};
		irrImgDesc.mArraySize = 6;
		irrImgDesc.mDepth = 1;
		irrImgDesc.mFormat = ImageFormat::RGBA32F;
		irrImgDesc.mHeight = gIrradianceSize;
		irrImgDesc.mWidth = gIrradianceSize;
		irrImgDesc.mMipLevels = 1;
		irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
		irrImgDesc.mSrgb = false;
		irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irrImgDesc.pDebugName = L"irrImgBuff";

		TextureLoadDesc irrLoadDesc = {};
		irrLoadDesc.pDesc = &irrImgDesc;
		irrLoadDesc.ppTexture = &pTextureIrradianceMap;
		addResource(&irrLoadDesc);

		TextureDesc specImgDesc = {};
		specImgDesc.mArraySize = 6;
		specImgDesc.mDepth = 1;
		specImgDesc.mFormat = ImageFormat::RGBA32F;
		specImgDesc.mHeight = gSpecularSize;
		specImgDesc.mWidth = gSpecularSize;
		specImgDesc.mMipLevels = gSpecularMips;
		specImgDesc.mSampleCount = SAMPLE_COUNT_1;
		specImgDesc.mSrgb = false;
		specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		specImgDesc.pDebugName = L"specImgBuff";

		TextureLoadDesc specImgLoadDesc = {};
		specImgLoadDesc.pDesc = &specImgDesc;
		specImgLoadDesc.ppTexture = &pTextureSpecularMap;
		addResource(&specImgLoadDesc);

		// Create empty texture for BRDF integration map.
		TextureLoadDesc brdfIntegrationLoadDesc = {};
		TextureDesc brdfIntegrationDesc = {};
		brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
		brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
		brdfIntegrationDesc.mDepth = 1;
		brdfIntegrationDesc.mArraySize = 1;
		brdfIntegrationDesc.mMipLevels = 1;
		brdfIntegrationDesc.mFormat = ImageFormat::RG32F;
		brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
		brdfIntegrationDesc.mHostVisible = false;
		brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
		brdfIntegrationLoadDesc.ppTexture = &pTextureBRDFIntegrationMap;
		addResource(&brdfIntegrationLoadDesc);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

		GPUPresetLevel presetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;
		uint32_t importanceSampleCounts[GPUPresetLevel::GPU_PRESET_COUNT] = { 0, 0, 64, 128, 256, 1024 };
		uint32_t importanceSampleCount = importanceSampleCounts[presetLevel];
		ShaderMacro importanceSampleMacro = { "IMPORTANCE_SAMPLE_COUNT", tinystl::string::format("%u", importanceSampleCount) };

		ShaderLoadDesc brdfIntegrationShaderDesc = {};
		brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

		ShaderLoadDesc irradianceShaderDesc = {};
		irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc specularShaderDesc = {};
		specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
		addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);
		addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
		addShader(pRenderer, &specularShaderDesc, &pSpecularShader);

		const char* pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc brdfRootDesc = { &pBRDFIntegrationShader, 1 };
		brdfRootDesc.mStaticSamplerCount = 1;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc irradianceRootDesc = { &pIrradianceShader, 1 };
		irradianceRootDesc.mStaticSamplerCount = 1;
		irradianceRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		irradianceRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc specularRootDesc = { &pSpecularShader, 1 };
		specularRootDesc.mStaticSamplerCount = 1;
		specularRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		specularRootDesc.ppStaticSamplers = &pSkyboxSampler;
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

		TextureBarrier uavBarriers[4] =
		{
			{ pTextureSkybox, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pTextureIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pTextureSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pTextureBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS },
		};
		cmdResourceBarrier(cmd, 0, NULL, 4, uavBarriers, false);

		cmdBindPipeline(cmd, pBRDFIntegrationPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "dstTexture";
		params[0].ppTextures = &pTextureBRDFIntegrationMap;
		cmdBindDescriptors(cmd, pBRDFIntegrationRootSignature, 1, params);
		const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(cmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

		TextureBarrier srvBarrier[1] =
		{
			{ pTextureBRDFIntegrationMap, RESOURCE_STATE_SHADER_RESOURCE }
		};

		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarrier, true);

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(cmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 1, params);

		struct Data
		{
			uint mip;
			uint textureSize;
		} data = { 0, gSkyboxSize };

		for (int i = 0; i < gSkyboxMips; i++)
		{
			data.mip = i;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &data;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pTextureSkybox;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 2, params);

			pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(cmd, max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] =
		{
			{ pTextureSkybox, RESOURCE_STATE_SHADER_RESOURCE }
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers, false);
		/************************************************************************/
		// Compute sky irradiance
		/************************************************************************/
		params[0] = {};
		params[1] = {};
		cmdBindPipeline(cmd, pIrradiancePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pTextureSkybox;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pTextureIrradianceMap;
		cmdBindDescriptors(cmd, pIrradianceRootSignature, 2, params);
		pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(cmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);
		/************************************************************************/
		// Compute specular sky
		/************************************************************************/
		cmdBindPipeline(cmd, pSpecularPipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pTextureSkybox;
		cmdBindDescriptors(cmd, pSpecularRootSignature, 1, params);

		struct PrecomputeSkySpecularData
		{
			uint mipSize;
			float roughness;
		};

		for (uint i = 0; i < gSpecularMips; i++)
		{
			PrecomputeSkySpecularData data = {};
			data.roughness = (float)i / (float)(gSpecularMips - 1);
			data.mipSize = gSpecularSize >> i;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &data;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pTextureSpecularMap;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(cmd, pSpecularRootSignature, 2, params);
			pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(cmd, max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]), max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
		}
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[2] =
		{
			{ pTextureIrradianceMap, RESOURCE_STATE_SHADER_RESOURCE },
			{ pTextureSpecularMap, RESOURCE_STATE_SHADER_RESOURCE }
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, srvBarriers2, false);

		endCmd(cmd);
		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 0, 0, 0, 0);
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

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

		removeSampler(pRenderer, pSkyboxSampler);
	}

	void DestroyPBRMaps()
	{
		removeResource(pTextureSpecularMap);
		removeResource(pTextureIrradianceMap);
		removeResource(pTextureSkybox);
		removeResource(pTextureBRDFIntegrationMap);
	}

	void LoadModels()
	{
		tinystl::vector<Vertex> vertices = {};
		tinystl::vector<uint> indices = {};

		const char* modelNames[MESH_COUNT] = { "matBall.obj", "cube.obj" };

		for (int m = 0; m < MESH_COUNT; ++m)
		{
			Model model;
			if (AssimpImporter::ImportModel(FileSystem::FixPath(modelNames[m], FSR_Meshes).c_str(), &model))
			{
				vertices.clear();
				indices.clear();

				for (size_t i = 0; i < model.mMeshArray.size(); ++i)
				{
					Mesh* mesh = &model.mMeshArray[i];
					vertices.reserve(vertices.size() + mesh->mPositions.size());
					indices.reserve(indices.size() + mesh->mIndices.size());

					for (size_t v = 0; v < mesh->mPositions.size(); ++v)
					{
						Vertex vertex = { float3(0.0f), float3(0.0f, 1.0f, 0.0f) };
						vertex.mPos = mesh->mPositions[v];
						vertex.mNormal = mesh->mNormals[v];
						vertex.mUv = mesh->mUvs[v];
						vertices.push_back(vertex);
					}

					for (size_t j = 0; j < mesh->mIndices.size(); ++j)
						indices.push_back(mesh->mIndices[j]);
			}

				MeshData* meshData = conf_placement_new<MeshData>(conf_malloc(sizeof(MeshData)));
				meshData->mVertexCount = (uint)vertices.size();
				meshData->mIndexCount = (uint)indices.size();

				BufferLoadDesc vertexBufferDesc = {};
				vertexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
				vertexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				vertexBufferDesc.mDesc.mSize = sizeof(Vertex) * meshData->mVertexCount;
				vertexBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
				vertexBufferDesc.pData = vertices.data();
				vertexBufferDesc.ppBuffer = &meshData->pVertexBuffer;
				addResource(&vertexBufferDesc);

				if (meshData->mIndexCount > 0)
				{
					BufferLoadDesc indexBufferDesc = {};
					indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
					indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
					indexBufferDesc.mDesc.mSize = sizeof(uint) * meshData->mIndexCount;
					indexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
					indexBufferDesc.pData = indices.data();
					indexBufferDesc.ppBuffer = &meshData->pIndexBuffer;
					addResource(&indexBufferDesc);
		}

				pMeshes[m] = meshData;
	}
			else
				ErrorMsg("Failed to load model.");
		}
	}

	void DestroyModels()
	{
		for (int i = 0; i < MESH_COUNT; ++i)
		{
			removeResource(pMeshes[i]->pVertexBuffer);
			if (pMeshes[i]->pIndexBuffer)
				removeResource(pMeshes[i]->pIndexBuffer);
			conf_free(pMeshes[i]);
		}
	}

	void LoadTextures()
	{
		const char* textureNames[] =
		{
			"rusted_iron/albedo.png",
			"rusted_iron/normal.png",
			"rusted_iron/metallic.png",
			"rusted_iron/roughness.png",
			"rusted_iron/ao.png",
			"copper/albedo.png",
			"copper/normal.png",
			"copper/metallic.png",
			"copper/roughness.png",
			"copper/ao.png",
			"grease/albedo.png",
			"grease/normal.png",
			"grease/metallic.png",
			"grease/roughness.png",
			"grease/ao.png",
			"gold/albedo.png",
			"gold/normal.png",
			"gold/metallic.png",
			"gold/roughness.png",
			"gold/ao.png",
			"titanium/albedo.png",
			"titanium/normal.png",
			"titanium/metallic.png",
			"titanium/roughness.png",
			"titanium/ao.png"
		};

		for (int i = 0; i < gMaterialTextureCount; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.ppTexture = &pTextureMaterialMaps[i];
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = textureNames[i];
			addResource(&textureDesc, true);
		}
	}

	void DestroyTextures()
	{
		for (uint i = 0; i < gMaterialTextureCount; ++i)
			removeResource(pTextureMaterialMaps[i]);
	}

	void CreateResources()
	{
		//Generate skybox vertex buffer
		float skyBoxPoints[] =
		{
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
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pVertexBufferSkybox;
		addResource(&skyboxVbDesc);

#ifndef LOAD_MATERIAL_BALL
		// Create sphere vertex buffer
		Vertex* pSphereVertices;
		createSpherePoints(&pSphereVertices, &gNumOfSpherePoints, gSphereResolution, gSphereDiameter);

		uint64_t sphereDataSize = gNumOfSpherePoints * sizeof(Vertex);

		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mElementCount = gNumOfSpherePoints;
		sphereVbDesc.mDesc.mVertexStride = sizeof(Vertex); // 3 for vertex, 3 for normal, 2 for textures
		sphereVbDesc.mDesc.mSize = sphereVbDesc.mDesc.mVertexStride  * sphereVbDesc.mDesc.mElementCount;
		sphereVbDesc.pData = pSphereVertices;
		sphereVbDesc.ppBuffer = &pVertexBufferSphere;
		addResource(&sphereVbDesc);

		conf_free(pSphereVertices);
#endif
	}

	void DestroyResources()
	{
		removeResource(pVertexBufferSkybox);
#ifndef LOAD_MATERIAL_BALL
		removeResource(pVertexBufferSphere);
#endif
	}

	void CreateUniformBuffers()
	{
#ifdef LOAD_MATERIAL_BALL
		// Surface uniform buffer
		BufferLoadDesc surfaceUBDesc = {};
		surfaceUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		surfaceUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		surfaceUBDesc.mDesc.mSize = sizeof(UniformObjData);
		surfaceUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		surfaceUBDesc.pData = NULL;
		surfaceUBDesc.ppBuffer = &pUniformBufferSurface;
		addResource(&surfaceUBDesc);

		// Nameplate uniform buffers
		BufferLoadDesc nameplateUBDesc = {};
		nameplateUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		nameplateUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		nameplateUBDesc.mDesc.mSize = sizeof(UniformObjData);
		nameplateUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		nameplateUBDesc.pData = NULL;
		for (int i = 0; i < METAL_COUNT; ++i)
		{
			nameplateUBDesc.ppBuffer = &pUniformBufferNamePlates[i];
			addResource(&nameplateUBDesc);
		}
#endif

		// Create a uniform buffer per mat ball
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			for (int i = 0; i < METAL_COUNT; ++i)
			{
				BufferLoadDesc matBallUBDesc = {};
				matBallUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				matBallUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				matBallUBDesc.mDesc.mSize = sizeof(UniformObjData);
				matBallUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				matBallUBDesc.pData = NULL;
				matBallUBDesc.ppBuffer = &pUniformBufferMatBall[frameIdx][i];
				addResource(&matBallUBDesc);
			}
		}

		// Uniform buffer for camera data
		BufferLoadDesc cameraUBDesc = {};
		cameraUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		cameraUBDesc.mDesc.mSize = sizeof(UniformCamData);
		cameraUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		cameraUBDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			cameraUBDesc.ppBuffer = &pUniformBufferCamera[i];
			addResource(&cameraUBDesc);
			cameraUBDesc.ppBuffer = &pUniformBufferCameraSkybox[i];
			addResource(&cameraUBDesc);
		}

		// Uniform buffer for light data
		BufferLoadDesc lightsUBDesc = {};
		lightsUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightsUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightsUBDesc.mDesc.mSize = sizeof(UniformLightData);
		lightsUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightsUBDesc.pData = NULL;
		lightsUBDesc.ppBuffer = &pUniformBufferLights;
		addResource(&lightsUBDesc);
	}

	void DestroyUniformBuffers()
	{
		removeResource(pUniformBufferLights);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBufferCameraSkybox[i]);
			removeResource(pUniformBufferCamera[i]);
		}

		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			for (int i = 0; i < METAL_COUNT; ++i)
				removeResource(pUniformBufferMatBall[frameIdx][i]);

#ifdef LOAD_MATERIAL_BALL
		removeResource(pUniformBufferSurface);
		for (int j = 0; j < METAL_COUNT; ++j)
			removeResource(pUniformBufferNamePlates[j]);
#endif
	}

	void InitializeUniformBuffers()
	{
		// Update the uniform buffer for the objects
		float baseX = 4.5f;
		float baseY = -4.5f;
		float baseZ = 9.0f;
		float offsetX = 0.1f;
		float offsetY = 0.0f;
		float offsetZ = 10.0f;
		float scaleVal = 1.0f;
		float roughDelta = 1.0f;
#ifdef LOAD_MATERIAL_BALL
		baseX = 17.0f;
		offsetX = 8.0f;
		offsetY = 1.5f;
		scaleVal = 1.5f;
#endif
		for (int i = 0; i < METAL_COUNT; ++i)
		{

#ifdef LOAD_MATERIAL_BALL
			mat4 modelmat = mat4::translation(vec3(baseX - i - offsetX * i, baseY, baseZ)) * mat4::scale(vec3(scaleVal)) * mat4::rotationY(PI);
#else
			mat4 modelmat = mat4::translation(vec3(baseX - i - offsetX * i, baseY, 0.0f)) * mat4::scale(vec3(scaleVal));
#endif
			gUniformDataObject.mWorldMat = modelmat;
			gUniformDataObject.mMetallic = i / (float)METAL_COUNT;
			gUniformDataObject.mRoughness = 0.04f + roughDelta;
			gUniformDataObject.objectId = -1;
			//if not enough materials specified then set pbrMaterials to -1

			gUniformDataMatBall[i] = gUniformDataObject;
			for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			{
				BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[frameIdx][i], &gUniformDataObject };
				updateResource(&objBuffUpdateDesc);
			}
			roughDelta -= .25f;
#ifdef LOAD_MATERIAL_BALL
			{
				//plates
				modelmat = mat4::translation(vec3(baseX - i - offsetX * i, -5.9f, baseZ + 3)) * mat4::scale(vec3(3.0f, 0.1f, 1.0f));
				gUniformDataObject.mWorldMat = modelmat;
				gUniformDataObject.mMetallic = 1;
				gUniformDataObject.mRoughness = 1.0f;
				gUniformDataObject.objectId = 3;
				BufferUpdateDesc objBuffUpdateDesc1 = { pUniformBufferNamePlates[i], &gUniformDataObject };
				updateResource(&objBuffUpdateDesc1);

				//text
				gTextWorldMats.push_back(mat4::translation(vec3(baseX - i - offsetX * i, -6.8f, baseZ + 3))*mat4::rotationX(-PI / 2)*mat4::scale(vec3(16.0f, 10.0f, 1.0f)));
			}
#endif
		}

#ifdef LOAD_MATERIAL_BALL
		//surface
		mat4 modelmat = mat4::translation(vec3(0.0f, -6.0f, 0.0f)) * mat4::scale(vec3(50.0f, 0.2f, 40.0f));
		gUniformDataObject.mWorldMat = modelmat;
		gUniformDataObject.mMetallic = 0;
		gUniformDataObject.mRoughness = 0.04f;
		gUniformDataObject.objectId = 2;
		BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferSurface, &gUniformDataObject };
		updateResource(&objBuffUpdateDesc);

#endif

		// Add light to scene
		Light light;
		light.mCol = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		light.mPos = vec4(0.0f, 0.0f, 2.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		gUniformDataLights.mLights[0] = light;

		light.mCol = vec4(0.0f, 0.0f, 1.0f, 0.0f);
		light.mPos = vec4(6.0f, 0.0f, 0.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		gUniformDataLights.mLights[1] = light;

		// Add light to scene
		light.mCol = vec4(0.0f, 1.0f, 0.0f, 0.0f);
		light.mPos = vec4(6.0f, 6.0f, 2.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		gUniformDataLights.mLights[2] = light;

		light.mCol = vec4(1.0f, 0.0f, 0.0f, 0.0f);
		light.mPos = vec4(0.0f, 6.0f, 2.0f, 0.0f);
		light.mRadius = 100.0f;
		light.mIntensity = 40.0f;

		gUniformDataLights.mLights[3] = light;

		gUniformDataLights.mCurrAmountOfLights = 4;
		BufferUpdateDesc lightBuffUpdateDesc = { pUniformBufferLights, &gUniformDataLights };
		updateResource(&lightBuffUpdateDesc);
	}

	//--------------------------------------------------------------------------------------------
	// LOAD FUNCTIONS
	//--------------------------------------------------------------------------------------------
	void CreatePipelines()
	{
		// Create vertex layouts
		VertexLayout skyboxVertexLayout = {};
		skyboxVertexLayout.mAttribCount = 1;
		skyboxVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		skyboxVertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		skyboxVertexLayout.mAttribs[0].mBinding = 0;
		skyboxVertexLayout.mAttribs[0].mLocation = 0;
		skyboxVertexLayout.mAttribs[0].mOffset = 0;

		VertexLayout defaultVertexLayout = {};
		defaultVertexLayout.mAttribCount = 3;
		defaultVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		defaultVertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		defaultVertexLayout.mAttribs[0].mBinding = 0;
		defaultVertexLayout.mAttribs[0].mLocation = 0;
		defaultVertexLayout.mAttribs[0].mOffset = 0;
		defaultVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		defaultVertexLayout.mAttribs[1].mFormat = ImageFormat::RGB32F;
		defaultVertexLayout.mAttribs[1].mLocation = 1;
		defaultVertexLayout.mAttribs[1].mBinding = 0;
		defaultVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
		defaultVertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		defaultVertexLayout.mAttribs[2].mFormat = ImageFormat::RG32F;
		defaultVertexLayout.mAttribs[2].mLocation = 2;
		defaultVertexLayout.mAttribs[2].mBinding = 0;
		defaultVertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

		// Create pipelines
		GraphicsPipelineDesc pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pRootSignatureSkybox;
		pipelineSettings.pShaderProgram = pShaderSkybox;
		pipelineSettings.pVertexLayout = &skyboxVertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineSkybox);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateEnable;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &defaultVertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineBRDF);
	}

	void DestroyPipelines()
	{
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pPipelineBRDF);
	}
};

void GuiController::UpdateDynamicUI()
{
	if (gMaterialType != GuiController::currentMaterialType)
	{
		// Switch widgets
		GuiController::currentMaterialType = (MaterialTypes)gMaterialType;
	}

#if !defined(TARGET_IOS) && !defined(_DURANGO)
	if (pSwapChain->mDesc.mEnableVsync != gVSyncEnabled)
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
		::toggleVSync(pRenderer, &pSwapChain);
	}
#endif
}

void GuiController::AddGui()
{
	static const char* materialTypeNames[] =
	{
		"Default",
		"Metal",
		NULL//needed for unix
	};

	static const uint32_t materialTypeValues[] =
	{
		MATERIAL_DEFAULT,
		MATERIAL_METAL,
		0//needed for unix
	};

	const uint32_t dropDownCount = (sizeof(materialTypeNames) / sizeof(materialTypeNames[0])) - 1;

	pGuiWindow->AddWidget(DropdownWidget("Material Type", &gMaterialType, materialTypeNames, materialTypeValues, dropDownCount));
	pGuiWindow->AddWidget(CheckboxWidget("V-Sync", &gVSyncEnabled));
}

DEFINE_APPLICATION_MAIN(MaterialPlayground)
