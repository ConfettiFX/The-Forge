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

// ECS
#include "../../../../Middleware_3/ECS/EntityManager.h"
#include "../../../../Middleware_3/ECS/ComponentRepresentation.h"

// REPRESENTATIONS
#include "../17_EntityComponentSystem/Representations/WorldBoundsRepresentation.h"
#include "../17_EntityComponentSystem/Representations/PositionRepresentation.h"
#include "../17_EntityComponentSystem/Representations/SpriteRepresentation.h"
#include "../17_EntityComponentSystem/Representations/MoveRepresentation.h"

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
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"
#include "../../../../Common_3/OS/Core/ThreadSystem.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be the last include in a cpp file

struct SpriteData
{
	float posX, posY;
	float scale;
	float pad;
	float colR, colG, colB;
	float sprite;
};

const uint32_t gImageCount = 3;
ProfileToken   gGpuProfileToken;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain*    pSwapChain = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pSpriteShader = NULL;
Buffer*   pSpriteVertexBuffers[gImageCount] = { NULL };
Buffer*   pSpriteIndexBuffer = NULL;
Pipeline* pSpritePipeline = NULL;

RootSignature*    pRootSignature = NULL;
DescriptorSet*    pDescriptorSetTexture = NULL;
DescriptorSet*    pDescriptorSetUniforms = NULL;
Sampler*          pLinearClampSampler = NULL;

Texture* pSpriteTexture = NULL;

uint32_t gFrameIndex = 0;

SpriteData* gSpriteData = NULL;
uint        gDrawSpriteCount = 0;

/// UI
UIApp gAppUI;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

// Based on: https://github.com/aras-p/dod-playground
static float RandomFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandomFloat(float from, float to) { return RandomFloat01() * (to - from) + from; }

const uint MaxSpriteCount = 11000;
const uint SpriteEntityCount = 10000;
const uint AvoidCount = 20;

static Entity* worldBoundsEntity;
static Entity* spriteEntities[SpriteEntityCount];
static Entity* avoidEntities[AvoidCount];

EntityManager* pEntityManager = nullptr;

ThreadSystem* pThreadSystem = nullptr;
bool multiThread			= true;

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
	struct Task
	{
		size_t         start;
		size_t         end;
		timeAndBounds* data;
	};

	Task          tasks[MAX_LOAD_THREADS + 2] = {};

	void Update(float deltaTime)
	{
		const WorldBoundsComponent& bounds = *worldBoundsEntity->getComponent<WorldBoundsComponent>();

		timeAndBounds moveData = { spriteEntities, deltaTime, &bounds };
		timeAndBounds avoidData = { avoidEntities, deltaTime, &bounds };
		
		// 1 thread used by resource loader
		const uint32_t numThreads = max(1u, getThreadSystemThreadCount(pThreadSystem) - 1);
		const uint32_t entitiesPerThread = SpriteEntityCount / (numThreads + 1);

		// Make sure there is enough workload for parallel processing
		if (multiThread && entitiesPerThread < SpriteEntityCount / 2)
		{
			uint32_t taskCount = 0;

			for (taskCount = 0; taskCount < numThreads; ++taskCount)
			{
				Task* task = &tasks[taskCount];
				task->start = taskCount * entitiesPerThread;
				task->end = min((size_t)SpriteEntityCount, task->start + entitiesPerThread);
				task->data = &moveData;
				addThreadSystemTask(pThreadSystem, &memberTaskFunc<MoveSystem, &MoveSystem::threadedUpdate>, this, taskCount);
			}

			// Remaining entities on main thread
			tasks[taskCount] = { tasks[taskCount - 1].end, SpriteEntityCount, &moveData };
			threadedUpdate(taskCount++);
			
			tasks[taskCount] = { 0, AvoidCount, &avoidData };
			threadedUpdate(taskCount++);

			waitThreadSystemIdle(pThreadSystem);
		}
		else
		{
			tasks[0] = { 0, SpriteEntityCount, &moveData };
			threadedUpdate(0);

			tasks[1] = { 0, AvoidCount, &avoidData };
			threadedUpdate(1);
		}
	}

	void threadedUpdate(uintptr_t id)
	{
		const Task* task = &tasks[id];
		for (uintptr_t i = task->start; i < task->end; ++i)
		{
			Entity* pEntity = (task->data->entities)[i];
			PositionComponent& position = *(pEntity->getComponent<PositionComponent>());
			MoveComponent& move = *(pEntity->getComponent<MoveComponent>());

			MoveEntities(position, move, task->data->deltaTime, *task->data->bounds);
		}
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
	struct Task
	{
		size_t         start;
		size_t         end;
		timeAndBounds* data;
	};

	Task tasks[MAX_LOAD_THREADS + 1] = {};

	static eastl::vector<float>    avoidDistanceList;

	Mutex emplaceMutex;
	
	bool init()
	{
		return emplaceMutex.Init();
	}
	
	void exit()
	{
		emplaceMutex.Destroy();
	}

	void addAvoidThisObjectToSystem(Entity* entity, float distance)
	{
		{
			MutexLock lock(emplaceMutex);
			avoidDistanceList.emplace_back(distance * distance);
		}
	}

	static void removeAllObjects()
	{
		avoidDistanceList.set_capacity(0);
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

	void Update(float deltaTime)
	{
		const WorldBoundsComponent& bounds = *worldBoundsEntity->getComponent<WorldBoundsComponent>();

		timeAndBounds data = { spriteEntities, deltaTime, &bounds };
		
		// 1 thread used by resource loader
		const uint32_t numThreads = max(1u, getThreadSystemThreadCount(pThreadSystem) - 1);
		const uint32_t entitiesPerThread = SpriteEntityCount / (numThreads + 1);

		// Make sure there is enough workload for parallel processing
		if (multiThread && entitiesPerThread < SpriteEntityCount / 2)
		{
			uint32_t taskCount = 0;

			for (taskCount = 0; taskCount < numThreads; ++taskCount)
			{
				Task* task = &tasks[taskCount];
				task->start = taskCount * entitiesPerThread;
				task->end = min((size_t)SpriteEntityCount, task->start + entitiesPerThread);
				task->data = &data;
				addThreadSystemTask(pThreadSystem, &memberTaskFunc<AvoidanceSystem, &AvoidanceSystem::threadedUpdate>, this, taskCount);
			}

			// Remaining entities on main thread
			tasks[taskCount] = { tasks[taskCount - 1].end, SpriteEntityCount, &data };
			threadedUpdate(taskCount++);

			waitThreadSystemIdle(pThreadSystem);
		}
		else
		{
			tasks[0] = { 0, SpriteEntityCount, &data };
			threadedUpdate(0);
		}
	}

	void threadedUpdate(uintptr_t id)
	{
		const Task* task = &tasks[id];
		const timeAndBounds& data = *task->data;

		for (uintptr_t i = task->start; i < task->end; ++i)
		{
			Entity* pEntity = spriteEntities[i];
			PositionComponent& position = *(pEntity->getComponent<PositionComponent>());

			for (size_t j = 0; j < AvoidCount; ++j)
			{
				Entity*					pAvoidEntity = avoidEntities[j];
				float                    avDistance = avoidDistanceList[j];
				PositionComponent&	  avoidPosition = *(pAvoidEntity->getComponent<PositionComponent>());

				// is our position closer to "thing to avoid" position than the avoid distance?
				if (DistanceSq(position, avoidPosition) < avDistance)
				{
					resolveCollision(pEntity, data.deltaTime);
					// also make our sprite take the color of the thing we just bumped into
					SpriteComponent& avoidSprite = *(pAvoidEntity->getComponent<SpriteComponent>());
					SpriteComponent& mySprite = *(pEntity->getComponent<SpriteComponent>());
					mySprite.colorR = avoidSprite.colorR;
					mySprite.colorG = avoidSprite.colorG;
					mySprite.colorB = avoidSprite.colorB;
				}
			}
		}
	}
};

eastl::vector<float>    AvoidanceSystem::avoidDistanceList;

static MoveSystem*      pMoveSystem;
static AvoidanceSystem* pAvoidanceSystem;

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
		pAvoidanceSystem->addAvoidThisObjectToSystem((data.entities)[i], 1.3f);
		
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
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");

		SpriteComponentRepresentation::BUILD_VAR_REPRESENTATIONS();
		MoveComponentRepresentation::BUILD_VAR_REPRESENTATIONS();
		PositionComponentRepresentation::BUILD_VAR_REPRESENTATIONS();
		WorldBoundsComponentRepresentation::BUILD_VAR_REPRESENTATIONS();

		pEntityManager = tf_new(EntityManager);

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

		if (!gAppUI.Init(pRenderer))
		  return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");

		initProfiler();

		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		// TODO: rename to sprite
		ShaderLoadDesc spriteShader = {};
		spriteShader.mStages[0] = { "basic.vert", NULL, 0 };
		spriteShader.mStages[1] = { "basic.frag", NULL, 0 };

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

		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);

		gSpriteData = (SpriteData*)tf_malloc(MaxSpriteCount * sizeof(SpriteData));

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
			addResource(&spriteVbDesc, NULL);
		}

		// Index buffer
		uint16_t indices[] =
		{
			0, 1, 2, 2, 1, 3,
		};
		BufferLoadDesc spriteIBDesc = {};
		spriteIBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		spriteIBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		spriteIBDesc.mDesc.mSize = sizeof(indices);
		spriteIBDesc.pData = indices;
		spriteIBDesc.ppBuffer = &pSpriteIndexBuffer;
		addResource(&spriteIBDesc, NULL);

		// Sprites texture
		TextureLoadDesc textureDesc = {};
		textureDesc.ppTexture = &pSpriteTexture;
		textureDesc.pFileName = "sprites";
		addResource(&textureDesc, NULL);

		initThreadSystem(&pThreadSystem);

	/************************************************************************/
	// GUI
	/************************************************************************/
		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.1f);
		GUIWindow = gAppUI.AddGuiComponent("MT", &guiDesc);
		
		CheckboxWidget Checkbox("Threading", &multiThread);
		GUIWindow->AddWidget(Checkbox);


		// Create entities
		pAvoidanceSystem = tf_new(AvoidanceSystem);
		pAvoidanceSystem->init();
		
		pMoveSystem = tf_new(MoveSystem);

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
		
		for (size_t i = 0; i < SpriteEntityCount; ++i)
		{
			createEntities(&data, i);
		}

		for (size_t i = 0; i < AvoidCount; ++i)
		{
			createEntities(&avoidData, i);
		}

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

		waitForAllResourceLoads();
		
		// Prepare descriptor sets
		DescriptorData params[1] = {};
		params[0].pName = "uTexture0";
		params[0].ppTextures = &pSpriteTexture;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 1, params);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			params[0].pName = "instanceBuffer";
			params[0].ppBuffers = &pSpriteVertexBuffers[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
		}

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		shutdownThreadSystem(pThreadSystem);
		
		pAvoidanceSystem->exit();
		tf_delete(pAvoidanceSystem);
		tf_delete(pMoveSystem);
		
		tf_delete(pEntityManager);

		waitQueueIdle(pGraphicsQueue);

		exitProfiler();

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pSpriteVertexBuffers[i]);
		}
		removeResource(pSpriteTexture);
		removeShader(pRenderer, pSpriteShader);
		removeResource(pSpriteIndexBuffer);

		removeDescriptorSet(pRenderer, pDescriptorSetTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		removeSampler(pRenderer, pLinearClampSampler);
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

		tf_free(gSpriteData);
		gSpriteData = NULL;

		AvoidanceSystem::removeAllObjects();
		SpriteComponentRepresentation::DESTROY_VAR_REPRESENTATIONS();
		MoveComponentRepresentation::DESTROY_VAR_REPRESENTATIONS();
		PositionComponentRepresentation::DESTROY_VAR_REPRESENTATIONS();
		WorldBoundsComponentRepresentation::DESTROY_VAR_REPRESENTATIONS();
	}

	bool Load()
	{
		if (!addMainSwapChain())
			return false;

		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = false;
		depthStateDesc.mDepthWrite = false;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;

		// VertexLayout for sprite drawing.
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSpriteShader;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pBlendState = &blendStateDesc;
		addPipeline(pRenderer, &desc, &pSpritePipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfilerUI();
		gAppUI.Unload();

		removePipeline(pRenderer, pSpritePipeline);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

		// Scene Update
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		// update object systems
		pMoveSystem->Update(deltaTime * 3.0f);
		pAvoidanceSystem->Update(deltaTime * 3.0f);

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

		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		// Update uniform buffers.
		const float w = (float)mSettings.mWidth;
		const float h = (float)mSettings.mHeight;
		float aspect = w / h;

		// Update vertex buffer
		SyncToken updateComplete = {};
		ASSERT(gDrawSpriteCount >= 0 && gDrawSpriteCount <= MaxSpriteCount);
		BufferUpdateDesc vboUpdateDesc = { pSpriteVertexBuffers[gFrameIndex] };
		beginUpdateResource(&vboUpdateDesc);
		memcpy(vboUpdateDesc.pMappedData, gSpriteData, gDrawSpriteCount * sizeof(SpriteData));
		endUpdateResource(&vboUpdateDesc, &updateComplete);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
		{
			waitForFences(pRenderer, 1, &pNextFence);
		}

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		// Draw Sprites
		if (gDrawSpriteCount > 0)
		{
			cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Sprites");
			cmdBindPipeline(cmd, pSpritePipeline);
			cmdBindPushConstants(cmd, pRootSignature, "RootConstant", &aspect);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
			cmdBindIndexBuffer(cmd, pSpriteIndexBuffer, INDEX_TYPE_UINT16, 0);
			cmdDrawIndexedInstanced(cmd, 6, 0, gDrawSpriteCount, 0, 0);
			cmdEndDebugMarker(cmd);
		}

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");

		gAppUI.Gui(GUIWindow);

		TextDrawDesc uiTextDesc;    // default
		uiTextDesc.mFontColor = 0xff00cc00;
		uiTextDesc.mFontSize = 18;
		 
#if !defined(__ANDROID__)
		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(8.0f, txtSize.y + 30.f), gGpuProfileToken, &uiTextDesc);
#else
		cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
#endif
		cmdDrawProfilerUI();

        gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
		endCmd(cmd);

		waitForToken(&updateComplete);

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
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mColorClearValue = { { 0.1f, 0.1f, 0.1f, 1.0f } };
		swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
		addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}
};

DEFINE_APPLICATION_MAIN(EntityComponentSystem)
