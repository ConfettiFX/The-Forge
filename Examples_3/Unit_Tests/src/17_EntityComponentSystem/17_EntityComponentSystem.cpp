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

// ECS
#include "../../../../Middleware_3/ECS/EntityManager.h"

// COMPONENTS
#include "../17_EntityComponentSystem/Components/WorldBoundsComponent.h"
#include "../17_EntityComponentSystem/Components/PositionComponent.h"
#include "../17_EntityComponentSystem/Components/SpriteComponent.h"
#include "../17_EntityComponentSystem/Components/MoveComponent.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
//#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Core/ThreadSystem.h"

#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be the last include in a cpp file

// Profilers
GuiComponent*			pGuiWindow;
GpuProfiler* pGpuProfiler = NULL;
HiresTimer   gTimer;

struct VsParams
{
	float aspect;
};

struct SpriteData
{
	float posX, posY;
	float scale;
	float pad;
	float colR, colG, colB;
	float sprite;
};

const uint32_t gImageCount = 3;
bool           bToggleMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain*    pSwapChain = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pSpriteShader = NULL;
Buffer*   pSpriteVertexBuffers[gImageCount] = { NULL };
Buffer*   pSpriteIndexBuffer = NULL;
Pipeline* pSpritePipeline = NULL;

RootSignature*    pRootSignature = NULL;
DescriptorBinder* pDescriptorBinder = NULL;
Sampler*          pLinearClampSampler = NULL;
DepthState*       pDepthState = NULL;
RasterizerState*  pRasterizerStateCullNone = NULL;
BlendState*       pBlendState = NULL;

Buffer* pParamsUbo[gImageCount] = { NULL };

Texture* pSpriteTexture = NULL;

uint32_t gFrameIndex = 0;

SpriteData* gSpriteData = NULL;
uint        gDrawSpriteCount = 0;

/// UI
UIApp gAppUI;

const char* pszBases[FSR_Count] = {
	"../../../src/17_EntityComponentSystem/",    // FSR_BinShaders
	"../../../src/17_EntityComponentSystem/",    // FSR_SrcShaders
	"../../../UnitTestResources/",               // FSR_Textures
	"../../../UnitTestResources/",               // FSR_Meshes
	"../../../UnitTestResources/",               // FSR_Builtin_Fonts
	"../../../src/17_EntityComponentSystem/",    // FSR_GpuConfig
	"",                                          // FSR_Animation
	"",                                          // FSR_Audio
	"../../../src/17_EntityComponentSystem/",    // FSR_OtherFiles - mentioned entities directory
	"../../../../../Middleware_3/Text/",         // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",           // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

// Based on: https://github.com/aras-p/dod-playground
static float RandomFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandomFloat(float from, float to) { return RandomFloat01() * (to - from) + from; }

const uint MaxSpriteCount = 11000;
#ifdef _DEBUG
const uint SpriteEntityCount = 5000;
#else
const uint SpriteEntityCount = 10000;
#endif
const uint AvoidCount = 20;

static Entity* worldBoundsEntity;
static Entity* spriteEntities[SpriteEntityCount];
static Entity* avoidEntities[AvoidCount];

File* pFileSystem			  = nullptr;
EntityManager* pEntityManager = nullptr;

ThreadSystem* pThreadSystem = nullptr;
bool multiThread			= false;

GuiComponent* GUIWindow = nullptr;

void MoveEntities(PositionComponent& position, MoveComponent& move, float& deltaTime, const WorldBoundsComponent& bounds)
{
	// update position based on movement velocity & delta time
	position.x += move.velx * deltaTime;
	position.y += move.vely * deltaTime;

	// check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
	if (position.x < bounds.xMin)
	{
		move.velx = -move.velx;
		position.x = bounds.xMin;
	}
	if (position.x > bounds.xMax)
	{
		move.velx = -move.velx;
		position.x = bounds.xMax;
	}
	if (position.y < bounds.yMin)
	{
		move.vely = -move.vely;
		position.y = bounds.yMin;
	}
	if (position.y > bounds.yMax)
	{
		move.vely = -move.vely;
		position.y = bounds.yMax;
	}
}

struct timeAndBounds {
	Entity** entities;
	float deltaTime;
	const WorldBoundsComponent* bounds;
};

struct MoveSystem
{
	static void Update(float deltaTime)
	{
		const WorldBoundsComponent& bounds = *worldBoundsEntity->getComponent<WorldBoundsComponent>();

		timeAndBounds data		= { spriteEntities, deltaTime, &bounds};
		timeAndBounds avoidData = { avoidEntities, deltaTime, &bounds };
		
		if (multiThread)
		{
			for (size_t i = 0; i < SpriteEntityCount; ++i)
			{
				addThreadSystemTask(pThreadSystem, &MoveSystem::threadedUpdate, &data, i);
			}

			for (size_t i = 0; i < AvoidCount; ++i)
			{
				addThreadSystemTask(pThreadSystem, &MoveSystem::threadedUpdate, &avoidData, i);
			}

			waitThreadSystemIdle(pThreadSystem);
		}
		else
		{
			for (size_t i = 0; i < SpriteEntityCount; ++i)
			{
				threadedUpdate(&data, i);
			}

			for (size_t i = 0; i < AvoidCount; ++i)
			{
				threadedUpdate(&avoidData, i);
			}
		}
	}

	static void threadedUpdate(void* pData, uintptr_t id)
	{	
		timeAndBounds& data			= *(timeAndBounds*)pData;
		Entity* pEntity				= (data.entities)[id];
		PositionComponent& position = *(pEntity->getComponent<PositionComponent>());
		MoveComponent& move			= *(pEntity->getComponent<MoveComponent>());

		MoveEntities(position, move, data.deltaTime, *data.bounds);
	}
};

static float DistanceSq(const PositionComponent& a, const PositionComponent& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx * dx + dy * dy;
}

struct AvoidanceSystem
{
	static eastl::vector<float>    avoidDistanceList;
	eastl::vector<Entity*>		   avoidList;

	Mutex emplaceMutex;

	void addAvoidThisObjectToSystem(Entity* entity, float distance)
	{
		{
			MutexLock lock(emplaceMutex);
			avoidList.emplace_back(entity);
			avoidDistanceList.emplace_back(distance * distance);
		}
	}

	static void resolveCollision(Entity* pEntity, float deltaTime)
	{
		PositionComponent& pos  = *(pEntity->getComponent<PositionComponent>());
		MoveComponent&     move = *(pEntity->getComponent<MoveComponent>());

		// flip velocity
		move.velx = -move.velx;
		move.vely = -move.vely;

		// move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
		pos.x += move.velx * deltaTime * 1.1f;
		pos.y += move.vely * deltaTime * 1.1f;
	}

	static void Update(float deltaTime)
	{
		const WorldBoundsComponent& bounds = *worldBoundsEntity->getComponent<WorldBoundsComponent>();

		timeAndBounds data = { spriteEntities, deltaTime, &bounds };
		
		if (multiThread)
		{
			for (size_t i = 0; i < SpriteEntityCount; ++i)
			{
				addThreadSystemTask(pThreadSystem, &AvoidanceSystem::threadedUpdate, &data, i);
			}

			waitThreadSystemIdle(pThreadSystem);
		}
		else
		{
			for (size_t i = 0; i < SpriteEntityCount; ++i)
			{
				threadedUpdate(&data, i);
			}
		}
	}

	static void threadedUpdate(void* pData, uintptr_t i)
	{
		timeAndBounds& data			= *(timeAndBounds*)pData;
		Entity* pEntity				= spriteEntities[i];
		PositionComponent& position = *(pEntity->getComponent<PositionComponent>());

		for (size_t j = 0; j < AvoidCount; ++j)
		{
			Entity*					pAvoidEntity = avoidEntities[j];
			float                    avDistance  = avoidDistanceList[j];
			PositionComponent&	  avoidPosition  = *(pAvoidEntity->getComponent<PositionComponent>());

			// is our position closer to "thing to avoid" position than the avoid distance?
			if (DistanceSq(position, avoidPosition) < avDistance)
			{
				resolveCollision(pEntity, data.deltaTime);
				// also make our sprite take the color of the thing we just bumped into
				SpriteComponent& avoidSprite = *(pAvoidEntity->getComponent<SpriteComponent>());
				SpriteComponent& mySprite	 = *(pEntity->getComponent<SpriteComponent>());
				mySprite.colorR = avoidSprite.colorR;
				mySprite.colorG = avoidSprite.colorG;
				mySprite.colorB = avoidSprite.colorB;
			}
		}
	}
};

eastl::vector<float>    AvoidanceSystem::avoidDistanceList;

static MoveSystem      moveSystem;
static AvoidanceSystem avoidanceSystem;

struct CreationData
{
	Entity** entities;
	WorldBoundsComponent* bounds;
	const char* entityTypeName;
};

static void createEntities(void* pData, uintptr_t i)
{
	// NOT DESERIALIZED WAY TO CREATE ENTITIES
	//spriteEntities[i] = pEntityManager->createEntity();
	
	CreationData data = *(CreationData*)pData;

	// DESERIALIZED WAY
	EntityId entityId = pEntityManager->createEntity();
	(data.entities)[i] = pEntityManager->getEntityById(entityId);

	float x = RandomFloat(data.bounds->xMin, data.bounds->xMax);
	float y = RandomFloat(data.bounds->yMin, data.bounds->yMax);

	PositionComponent* position = (data.entities)[i]->getComponent<PositionComponent>();
	if (!position)
		position = &(pEntityManager->addComponentToEntity<PositionComponent>(entityId));	// ADD CUSTOM COMPONENTS
	position->x = x;
	position->y = y;
	
	MoveComponent* move = (data.entities)[i]->getComponent<MoveComponent>();
	if (!move)
		move = &( pEntityManager->addComponentToEntity<MoveComponent>(entityId) );
	move->Initialize(0.3f, 0.6f);

	SpriteComponent* sprite = (data.entities)[i]->getComponent<SpriteComponent>();
	if (!sprite)
		sprite = &( pEntityManager->addComponentToEntity<SpriteComponent>(entityId) );

	if (strcmp(data.entityTypeName, "avoid")) {
		avoidanceSystem.addAvoidThisObjectToSystem((data.entities)[i], 1.3f);
		
		sprite->colorR = 1.0f;
		sprite->colorG = 1.0f;
		sprite->colorB = 1.0f;
		sprite->scale = 1.0f;
		sprite->spriteIndex = rand() % 5;
	}
	else {
		position->x *= 0.2f;
		position->y *= 0.2f;
		sprite->colorR = RandomFloat(0.5f, 1.0f);
		sprite->colorG = RandomFloat(0.5f, 1.0f);
		sprite->colorB = RandomFloat(0.5f, 1.0f);
		sprite->scale = 2.0f;
		sprite->spriteIndex = 5;
	}
}

class EntityComponentSystem: public IApp
{
	public:
	bool Init()
	{
		pEntityManager = conf_new(EntityManager);
		pFileSystem	   = conf_new(File);

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

		initProfiler(pRenderer);
		profileRegisterInput();

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

		// TODO: rename to sprite
		ShaderLoadDesc spriteShader = {};
		spriteShader.mStages[0] = { "basic.vert", NULL, 0, FSR_SrcShaders };
		spriteShader.mStages[1] = { "basic.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &spriteShader, &pSpriteShader);

		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_LINEAR,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pLinearClampSampler);

		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc rootDesc = { &pSpriteShader, 1 };
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pLinearClampSampler;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDesc = { pRootSignature };
		addDescriptorBinder(pRenderer, 0, 1, &descriptorBinderDesc, &pDescriptorBinder);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullNone);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = false;
		depthStateDesc.mDepthWrite = false;
		addDepthState(pRenderer, &depthStateDesc, &pDepthState);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateDesc, &pBlendState);

		gSpriteData = (SpriteData*)conf_malloc(MaxSpriteCount * sizeof(SpriteData));

		// Instance buffer
		BufferLoadDesc spriteVbDesc = {};
		spriteVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		spriteVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		spriteVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		spriteVbDesc.mDesc.mFirstElement = 0;
		spriteVbDesc.mDesc.mElementCount = MaxSpriteCount;
		spriteVbDesc.mDesc.mStructStride = sizeof(SpriteData);
		spriteVbDesc.mDesc.mSize = MaxSpriteCount * spriteVbDesc.mDesc.mStructStride;
		spriteVbDesc.pData = gSpriteData;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			spriteVbDesc.ppBuffer = &pSpriteVertexBuffers[i];
			addResource(&spriteVbDesc);
		}

		// Index buffer
		uint16_t indices[] = {
			0, 1, 2, 2, 1, 3,
		};
		BufferLoadDesc spriteIBDesc = {};
		spriteIBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		spriteIBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		spriteIBDesc.mDesc.mSize = sizeof(indices);
		spriteIBDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
		spriteIBDesc.pData = indices;
		spriteIBDesc.ppBuffer = &pSpriteIndexBuffer;
		addResource(&spriteIBDesc);

		// Ubo
		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(VsParams);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pParamsUbo[i];
			addResource(&ubDesc);
		}

		// Sprites texture
		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.ppTexture = &pSpriteTexture;
		textureDesc.pFilename = "sprites";
		addResource(&textureDesc);

		finishResourceLoading();

		initThreadSystem(&pThreadSystem);

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

	/************************************************************************/
	// GUI
	/************************************************************************/

		GuiDesc guiDesc = {};
		guiDesc.mStartSize = vec2(300.0f, 250.0f);
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);

		pGuiWindow->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler)); 
		
		const TextDrawDesc UIPanelWindowTitleTextDesc = { 0, 0xffff00ff, 16 };

		float   dpiScale = getDpiScale().x;
		vec2    UIPosition = { mSettings.mWidth * 0.01f, mSettings.mHeight * 0.5f };
		vec2    UIPanelSize = vec2(650.f, 1000.f) / dpiScale;
		GuiDesc guiDesc2(UIPosition, UIPanelSize, UIPanelWindowTitleTextDesc);
		GUIWindow = gAppUI.AddGuiComponent("MultiThread", &guiDesc2);
		
		CheckboxWidget Checkbox("Threading", &multiThread);
		GUIWindow->AddWidget(Checkbox);


		// Create entities
		EntityId worldBoundsEntityId = pEntityManager->createEntity();
		worldBoundsEntity = pEntityManager->getEntityById(worldBoundsEntityId);
		WorldBoundsComponent* bounds = &(pEntityManager->addComponentToEntity<WorldBoundsComponent>(worldBoundsEntityId));
		bounds->xMin = -80.0f;
		bounds->xMax = 80.0f;
		bounds->yMin = -50.0f;
		bounds->yMax = 50.0f;

		// THIS IS HOW YOU SERIALIZE AN ENTITY
		//pSerializer->SerializeEntity(worldBoundsEntityId, "serializedWorldBounds", "../../../src/17_EntityComponentSystem/Entities/");

		CreationData data	   = { spriteEntities, bounds, "sprite" };
		CreationData avoidData = { avoidEntities,  bounds, "avoid" };
		
		if (multiThread)
		{
			for (size_t i = 0; i < SpriteEntityCount; ++i) {
				addThreadSystemTask(pThreadSystem, &createEntities, &data, i);
			}
			
			for (size_t i = 0; i < AvoidCount; ++i) {
				addThreadSystemTask(pThreadSystem, &createEntities, &avoidData, i);
			}

			waitThreadSystemIdle(pThreadSystem);
		}
		else
		{
			for (size_t i = 0; i < SpriteEntityCount; ++i) {
				createEntities(&data, i);
			}

			for (size_t i = 0; i < AvoidCount; ++i) {
				createEntities(&avoidData, i);
			}
		}

		return true;
	}

	void Exit()
	{
		shutdownThreadSystem(pThreadSystem);
		
		conf_delete(pEntityManager);
		conf_delete(pFileSystem);

		waitQueueIdle(pGraphicsQueue);

		exitProfiler();

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pSpriteVertexBuffers[i]);
			removeResource(pParamsUbo[i]);
		}
		removeResource(pSpriteTexture);
		removeShader(pRenderer, pSpriteShader);
		removeResource(pSpriteIndexBuffer);

		removeSampler(pRenderer, pLinearClampSampler);
		removeRootSignature(pRenderer, pRootSignature);
		removeDescriptorBinder(pRenderer, pDescriptorBinder);
		
		removeDepthState(pDepthState);
		removeRasterizerState(pRasterizerStateCullNone);
		removeBlendState(pBlendState);

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

		conf_free(gSpriteData);
		gSpriteData = NULL;
	}

	bool Load()
	{
		if (!addMainSwapChain())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);

		// VertexLayout for sprite drawing.
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthState;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSpriteShader;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = pBlendState;
		addPipeline(pRenderer, &desc, &pSpritePipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfiler();
		gAppUI.Unload();

		removePipeline(pRenderer, pSpritePipeline);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		// Scene Update
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		// update object systems
		moveSystem.Update(deltaTime * 3.0f);
		avoidanceSystem.Update(deltaTime * 3.0f);

		// Iterate all entities with transform and plane component
		gDrawSpriteCount = 0;
		float globalScale = 0.05f;

		for (size_t i = 0; i < SpriteEntityCount; ++i) {
			Entity* pEntity				= spriteEntities[i];
			PositionComponent& position = *(pEntity->getComponent<PositionComponent>());
			SpriteComponent&     sprite = *(pEntity->getComponent<SpriteComponent>());
			
			SpriteData& spriteData = gSpriteData[gDrawSpriteCount++];
			spriteData.posX   = position.x * globalScale;
			spriteData.posY   = position.y * globalScale;
			spriteData.scale  = sprite.scale * globalScale;
			spriteData.colR   = sprite.colorR;
			spriteData.colG   = sprite.colorG;
			spriteData.colB   = sprite.colorB;
			spriteData.sprite = (float)sprite.spriteIndex;
		}

		for (size_t i = 0; i < AvoidCount; ++i) {
			Entity* pEntity				= avoidEntities[i];
			PositionComponent& position = *(pEntity->getComponent<PositionComponent>());
			SpriteComponent&     sprite = *(pEntity->getComponent<SpriteComponent>());

			SpriteData& spriteData = gSpriteData[gDrawSpriteCount++];
			spriteData.posX   = position.x * globalScale;
			spriteData.posY   = position.y * globalScale;
			spriteData.scale  = sprite.scale * globalScale;
			spriteData.colR   = sprite.colorR;
			spriteData.colG   = sprite.colorG;
			spriteData.colB   = sprite.colorB;
			spriteData.sprite = (float)sprite.spriteIndex;
		}

		// ProfileSetDisplayMode()
		// TODO: need to change this better way 
		if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
		{
			Profile& S = *ProfileGet();
			int nValue = bToggleMicroProfiler ? 1 : 0;
			nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
			S.nDisplay = nValue;

			bPrevToggleMicroProfiler = bToggleMicroProfiler;
		}

		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		gTimer.GetUSec(true);
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// Update uniform buffers.
		const float w = (float)mSettings.mWidth;
		const float h = (float)mSettings.mHeight;
		VsParams    vs_params;
		vs_params.aspect = w / h;
		BufferUpdateDesc uboUpdateDesc = { pParamsUbo[gFrameIndex], &vs_params };
		updateResource(&uboUpdateDesc);

		// Update vertex buffer
		assert(gDrawSpriteCount >= 0 && gDrawSpriteCount <= MaxSpriteCount);
		BufferUpdateDesc vboUpdateDesc = { pSpriteVertexBuffers[gFrameIndex], gSpriteData };
		vboUpdateDesc.mSize = gDrawSpriteCount * sizeof(SpriteData);
		updateResource(&vboUpdateDesc);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			waitForFences(pRenderer, 1, &pNextFence);
		}

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.1f;
		loadActions.mClearColorValues[0].g = 0.1f;
		loadActions.mClearColorValues[0].b = 0.1f;
		loadActions.mClearColorValues[0].a = 1.0f;

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		// Draw Sprites
		if (gDrawSpriteCount > 0)
		{
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Sprites");
			cmdBindPipeline(cmd, pSpritePipeline);

#define NUM_BUFFERS 3
			DescriptorData params[NUM_BUFFERS] = {};
			params[0].pName = "VsParams";
			params[0].ppBuffers = &pParamsUbo[gFrameIndex];
			params[1].pName = "uTexture0";
			params[1].ppTextures = &pSpriteTexture;
			params[2].pName = "instanceBuffer";
			params[2].ppBuffers = &pSpriteVertexBuffers[gFrameIndex];

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, NUM_BUFFERS, params);
			cmdBindIndexBuffer(cmd, pSpriteIndexBuffer, 0);
			cmdDrawIndexedInstanced(cmd, 6, 0, gDrawSpriteCount, 0, 0);
			cmdEndDebugMarker(cmd);
		}

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");

		gAppUI.Gui(GUIWindow);

		TextDrawDesc uiTextDesc;    // default
		uiTextDesc.mFontColor = 0xff00cc00;
		uiTextDesc.mFontSize = 18;
		 
		gAppUI.DrawText(
			cmd, float2(8.0f, 15.0f), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &uiTextDesc);
		
		gAppUI.DrawText(
			cmd, float2(8.0f, 40.0f), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&uiTextDesc);

		cmdDrawProfiler(cmd);

		gAppUI.Gui(pGuiWindow);

        gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
	}

	const char* GetName() { return "17_EntityComponentSystem"; }

	bool addMainSwapChain()
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
		addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}
};

DEFINE_APPLICATION_MAIN(EntityComponentSystem)
