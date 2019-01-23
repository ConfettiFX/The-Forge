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

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

// entt: https://github.com/skypjack/entt
#include "../../../../Common_3/ThirdParty/OpenSource/entt/entt.hpp"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

// Profilers
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

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*   pSpriteShader = NULL;
Buffer*   pSpriteVertexBuffers[gImageCount] = { NULL };
Buffer*   pSpriteIndexBuffer = NULL;
Pipeline* pSpritePipeline = NULL;

RootSignature*   pRootSignature = NULL;
Sampler*         pLinearClampSampler = NULL;
DepthState*      pDepthState = NULL;
RasterizerState* pRasterizerStateCullNone = NULL;
BlendState*      pBlendState = NULL;

Buffer* pParamsUbo[gImageCount] = { NULL };

Texture* pSpriteTexture = NULL;

uint32_t gFrameIndex = 0;

SpriteData* gSpriteData = NULL;
uint        gDrawSpriteCount = 0;

/// UI
UIApp gAppUI;

FileSystem gFileSystem;
LogManager gLogManager;

const char* pszBases[FSR_Count] = {
	"../../../src/17_EntityComponentSystem/",    // FSR_BinShaders
	"../../../src/17_EntityComponentSystem/",    // FSR_SrcShaders
	"../../../UnitTestResources/",               // FSR_Textures
	"../../../UnitTestResources/",               // FSR_Meshes
	"../../../UnitTestResources/",               // FSR_Builtin_Fonts
	"../../../src/17_EntityComponentSystem/",    // FSR_GpuConfig
	"",                                          // FSR_Animation
	"",                                          // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",         // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",           // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

// Based on: https://github.com/aras-p/dod-playground
static float RandomFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandomFloat(float from, float to) { return RandomFloat01() * (to - from) + from; }

// 2D position: just x,y coordinates
struct PositionComponent
{
	float x, y;
};

// Sprite: color, sprite index (in the sprite atlas), and scale for rendering it
struct SpriteComponent
{
	float colorR, colorG, colorB;
	int   spriteIndex;
	float scale;
};

// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent
{
	float xMin, xMax, yMin, yMax;
};

// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent
{
	float velx, vely;

	void Initialize(float minSpeed, float maxSpeed)
	{
		// random angle
		float angle = RandomFloat01() * 3.1415926f * 2;
		// random movement speed between given min & max
		float speed = RandomFloat(minSpeed, maxSpeed);
		// velocity x & y components
		velx = cosf(angle) * speed;
		vely = sinf(angle) * speed;
	}
};

const uint MaxSpriteCount = 11000;
#ifdef _DEBUG
const uint SpriteEntityCount = 5000;
#else
const uint SpriteEntityCount = 10000;
#endif
const uint AvoidCount = 20;

using EntityID = uint;

entt::Registry<EntityID> registry;
EntityID                 worldBoundsEntity;
EntityID                 spriteEntities[SpriteEntityCount];
EntityID                 avoidEntities[AvoidCount];

struct MoveSystem
{
	void Update(float deltaTime)
	{
		const WorldBoundsComponent& bounds = registry.get<WorldBoundsComponent>(worldBoundsEntity);

		// go through all the objects
		entt::View<EntityID, PositionComponent, MoveComponent> view = registry.view<PositionComponent, MoveComponent>();
		for (EntityID entity : view)
		{
			PositionComponent& position = view.get<PositionComponent>(entity);
			MoveComponent&     move = view.get<MoveComponent>(entity);

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
	tinystl::vector<float>    avoidDistanceList;
	tinystl::vector<uint32_t> avoidList;

	void AddAvoidThisObjectToSystem(uint32_t id, float distance)
	{
		avoidList.emplace_back(id);
		avoidDistanceList.emplace_back(distance * distance);
	}

	void ResolveCollision(EntityID id, float deltaTime)
	{
		PositionComponent& pos = registry.get<PositionComponent>(id);
		MoveComponent&     move = registry.get<MoveComponent>(id);

		// flip velocity
		move.velx = -move.velx;
		move.vely = -move.vely;

		// move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
		pos.x += move.velx * deltaTime * 1.1f;
		pos.y += move.vely * deltaTime * 1.1f;
	}

	void Update(float deltaTime)
	{
		entt::View<EntityID, PositionComponent> view = registry.view<PositionComponent>();
		for (EntityID entity : view)
		{
			PositionComponent& position = view.get(entity);

			// check each thing in avoid list
			for (size_t ia = 0, na = avoidList.size(); ia != na; ++ia)
			{
				float                    avDistance = avoidDistanceList[ia];
				uint32_t                 avoidEntity = avoidList[ia];
				const PositionComponent& avoidposition = registry.get<PositionComponent>(avoidEntity);

				// is our position closer to "thing to avoid" position than the avoid distance?
				if (DistanceSq(position, avoidposition) < avDistance)
				{
					ResolveCollision(entity, deltaTime);

					// also make our sprite take the color of the thing we just bumped into
					SpriteComponent& avoidSprite = registry.get<SpriteComponent>(avoidEntity);
					SpriteComponent& mySprite = registry.get<SpriteComponent>(entity);
					mySprite.colorR = avoidSprite.colorR;
					mySprite.colorG = avoidSprite.colorG;
					mySprite.colorB = avoidSprite.colorB;
				}
			}
		}
	}
};

static MoveSystem      moveSystem;
static AvoidanceSystem avoidanceSystem;

class EntityComponentSystem: public IApp
{
	public:
	bool Init()
	{
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

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

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

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullNone);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
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

		//
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
		textureDesc.mUseMipmaps = false;
		textureDesc.pFilename = "sprites.png";
		addResource(&textureDesc);

		finishResourceLoading();

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		// Create sprite entities and components.
		worldBoundsEntity = registry.create();
		WorldBoundsComponent& bounds = registry.assign<WorldBoundsComponent>(worldBoundsEntity);
		bounds.xMin = -80.0f;
		bounds.xMax = 80.0f;
		bounds.yMin = -50.0f;
		bounds.yMax = 50.0f;

		for (uint i = 0; i < SpriteEntityCount; i++)
		{
			spriteEntities[i] = registry.create();
			float x = RandomFloat(bounds.xMin, bounds.xMax);
			float y = RandomFloat(bounds.yMin, bounds.yMax);
			registry.assign<PositionComponent>(spriteEntities[i], x, y);

			SpriteComponent& sprite = registry.assign<SpriteComponent>(spriteEntities[i]);
			sprite.colorR = 1.0f;
			sprite.colorG = 1.0f;
			sprite.colorB = 1.0f;
			sprite.spriteIndex = rand() % 5;
			sprite.scale = 1.0f;

			// Move component
			MoveComponent& move = registry.assign<MoveComponent>(spriteEntities[i]);
			move.Initialize(0.7f, 1.0f);
		}

		for (uint i = 0; i < AvoidCount; ++i)
		{
			avoidEntities[i] = registry.create();
			float x = RandomFloat(bounds.xMin, bounds.xMax) * 0.2f;
			float y = RandomFloat(bounds.yMin, bounds.yMax) * 0.2f;
			registry.assign<PositionComponent>(avoidEntities[i], x, y);

			SpriteComponent& sprite = registry.assign<SpriteComponent>(avoidEntities[i]);
			sprite.colorR = RandomFloat(0.5f, 1.0f);
			sprite.colorG = RandomFloat(0.5f, 1.0f);
			sprite.colorB = RandomFloat(0.5f, 1.0f);
			sprite.spriteIndex = 5;
			sprite.scale = 2.0f;

			// Move component
			MoveComponent& move = registry.assign<MoveComponent>(avoidEntities[i]);
			move.Initialize(0.3f, 0.6f);

			// add to avoidance this as "Avoid This" object
			avoidanceSystem.AddAvoidThisObjectToSystem(avoidEntities[i], 1.3f);
		}

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);
		removeDebugRendererInterface();

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

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		// VertexLayout for sprite drawing.
		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthState;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSpriteShader;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = pBlendState;
		addPipeline(pRenderer, &pipelineSettings, &pSpritePipeline);

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);

		gAppUI.Unload();

		removePipeline(pRenderer, pSpritePipeline);

		removeSwapChain(pRenderer, pSwapChain);
		removeRenderTarget(pRenderer, pDepthBuffer);
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

		entt::View<EntityID, PositionComponent, SpriteComponent> view = registry.view<PositionComponent, SpriteComponent>();
		for (EntityID entity : view)
		{
			PositionComponent& position = view.get<PositionComponent>(entity);
			SpriteComponent&   sprite = view.get<SpriteComponent>(entity);

			SpriteData& spriteData = gSpriteData[gDrawSpriteCount++];
			spriteData.posX = position.x * globalScale;
			spriteData.posY = position.y * globalScale;
			spriteData.scale = sprite.scale * globalScale;
			spriteData.colR = sprite.colorR;
			spriteData.colG = sprite.colorG;
			spriteData.colB = sprite.colorB;
			spriteData.sprite = (float)sprite.spriteIndex;
		}
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
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);
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
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
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

			cmdBindDescriptors(cmd, pRootSignature, NUM_BUFFERS, params);
			cmdBindIndexBuffer(cmd, pSpriteIndexBuffer, 0);
			cmdDrawIndexedInstanced(cmd, 6, 0, gDrawSpriteCount, 0, 0);
			cmdEndDebugMarker(cmd);
		}

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");

		TextDrawDesc uiTextDesc;    // default
		uiTextDesc.mFontColor = 0xff00cc00;
		uiTextDesc.mFontSize = 18;
		drawDebugText(cmd, 8.0f, 15.0f, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &uiTextDesc);
#ifndef METAL
		drawDebugText(cmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &uiTextDesc);
#endif
		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "17_EntityComponentSystem"; }

	bool addMainSwapChain()
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
		swapChainDesc.mEnableVsync = false;
		addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

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
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}
};

DEFINE_APPLICATION_MAIN(EntityComponentSystem)
