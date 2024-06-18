/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

// ECS
#include "../../../../Common_3/Game/ThirdParty/OpenSource/flecs/flecs.h"

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h" // Must be the last include in a cpp file

struct SpriteData
{
    float posX, posY;
    float scale;
    float pad;
    float colR, colG, colB;
    float sprite;
};

// COMPONENTS
struct WorldBoundsComponent
{
    float xMin, xMax, yMin, yMax;
};

struct PositionComponent
{
    float x, y;
};

struct SpriteComponent
{
    float colorR, colorG, colorB;
    int   spriteIndex;
    float scale;
};

struct MoveComponent
{
    float velx, vely;
};

ECS_COMPONENT_DECLARE(WorldBoundsComponent);
ECS_COMPONENT_DECLARE(PositionComponent);
ECS_COMPONENT_DECLARE(SpriteComponent);
ECS_COMPONENT_DECLARE(MoveComponent);

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

ProfileToken gGpuProfileToken;

Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain* pSwapChain = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;

Shader*   pSpriteShader = NULL;
Buffer*   pSpriteVertexBuffers[gDataBufferCount] = { NULL };
Buffer*   pSpriteIndexBuffer = NULL;
Buffer*   pSpriteVertexBuffer = NULL;
Pipeline* pSpritePipeline = NULL;

RootSignature* pRootSignature = NULL;
DescriptorSet* pDescriptorSetTexture = NULL;
DescriptorSet* pDescriptorSetUniforms = NULL;
Sampler*       pLinearClampSampler = NULL;
uint32_t       gAspectRootConstantIndex = 0;

Texture* pSpriteTexture = NULL;

uint32_t gFrameIndex = 0;

uint32_t gDrawSpriteCount = 0;
uint32_t gAvailableCores = 1;

ecs_world_t* gECSWorld = NULL;

ecs_query_t* gECSSpriteQuery = NULL;
ecs_query_t* gECSAvoidQuery = NULL;

// Based on: https://github.com/aras-p/dod-playground

#if defined(__ANDROID__)
const uint32_t gSpriteEntityCount = 108;
const uint32_t gAvoidEntityCount = 20;
#else
const uint32_t gSpriteEntityCount = 50000;
const uint32_t gAvoidEntityCount = 100;
#endif
const uint32_t gMaxSpriteCount = gAvoidEntityCount + gSpriteEntityCount;

SpriteData gSpriteData[gMaxSpriteCount] = {};

static bool gMultiThread = true;

UIComponent* pGUIWindow = nullptr;

uint32_t gFontID = 0;

MoveComponent createMoveComponent(float minSpeed, float maxSpeed)
{
    MoveComponent move;

    // random angle
    float angle = randomFloat01() * 3.1415926f * 2;
    // random movement speed between given min & max
    float speed = randomFloat(minSpeed, maxSpeed);
    // velocity x & y components
    move.velx = cosf(angle) * speed;
    move.vely = sinf(angle) * speed;

    return move;
}

struct AvoidComponent
{
    float distanceSq;
};
ECS_COMPONENT_DECLARE(AvoidComponent);

void MoveSystem(ecs_iter_t* it)
{
    PositionComponent* positions = ecs_term(it, PositionComponent, 1);
    MoveComponent*     moves = ecs_term(it, MoveComponent, 2);

    const WorldBoundsComponent* bounds = ecs_singleton_get(it->world, WorldBoundsComponent);

    for (int i = 0; i < it->count; i++)
    {
        PositionComponent& pos = positions[i];
        MoveComponent&     move = moves[i];

        // update position based on movement velocity & delta time
        pos.x += move.velx * it->delta_time;
        pos.y += move.vely * it->delta_time;

        // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
        if (pos.x < bounds->xMin)
        {
            move.velx = -move.velx;
            pos.x = bounds->xMin;
        }
        if (pos.x > bounds->xMax)
        {
            move.velx = -move.velx;
            pos.x = bounds->xMax;
        }
        if (pos.y < bounds->yMin)
        {
            move.vely = -move.vely;
            pos.y = bounds->yMin;
        }
        if (pos.y > bounds->yMax)
        {
            move.vely = -move.vely;
            pos.y = bounds->yMax;
        }
    }
}

static float DistanceSq(PositionComponent a, PositionComponent b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void AvoidanceSystem(ecs_iter_t* it)
{
    PositionComponent* positions = ecs_term(it, PositionComponent, 1);
    MoveComponent*     moves = ecs_term(it, MoveComponent, 2);
    SpriteComponent*   sprites = ecs_term(it, SpriteComponent, 3);

    for (int i = 0; i < it->count; i++)
    {
        PositionComponent& pos = positions[i];
        MoveComponent&     move = moves[i];
        SpriteComponent&   sprite = sprites[i];

        ecs_iter_t avoidIter = ecs_query_iter(it->world, gECSAvoidQuery);
        while (ecs_query_next(&avoidIter))
        {
            PositionComponent* avoidPositions = ecs_term(&avoidIter, PositionComponent, 1);
            SpriteComponent*   avoidSprites = ecs_term(&avoidIter, SpriteComponent, 3);
            AvoidComponent*    avoidDistances = ecs_term(&avoidIter, AvoidComponent, 4);

            for (int j = 0; j < avoidIter.count; j++)
            {
                const PositionComponent& avoidPosition = avoidPositions[j];
                const SpriteComponent&   avoidSprite = avoidSprites[j];
                const AvoidComponent&    avoidDistance = avoidDistances[j];

                if (DistanceSq(pos, avoidPosition) < avoidDistance.distanceSq)
                {
                    // flip velocity
                    move.velx = -move.velx;
                    move.vely = -move.vely;

                    // move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
                    pos.x += move.velx * it->delta_time * 1.1f;
                    pos.y += move.vely * it->delta_time * 1.1f;

                    sprite.colorR = avoidSprite.colorR;
                    sprite.colorG = avoidSprite.colorG;
                    sprite.colorB = avoidSprite.colorB;
                }
            }
        }
    }
}

struct CreationData
{
    WorldBoundsComponent* bounds;
    const char*           entityTypeName;
};

static void createEntities(void* pData, uintptr_t i)
{
    UNREF_PARAM(i);
    CreationData data = *(CreationData*)pData;

    ecs_entity_t entityId = ecs_new_id(gECSWorld);

    float x = randomFloat(data.bounds->xMin, data.bounds->xMax);
    float y = randomFloat(data.bounds->yMin, data.bounds->yMax);

    PositionComponent position = { x, y };
    MoveComponent     move = createMoveComponent(0.3f, 0.6f);
    SpriteComponent   sprite = {};

    if (!strcmp(data.entityTypeName, "avoid"))
    {
        AvoidComponent avoid = { 1.3f * 1.3f };
        ecs_set(gECSWorld, entityId, AvoidComponent, avoid);

        position.x *= 0.2f;
        position.y *= 0.2f;
        sprite.colorR = randomFloat(0.5f, 1.0f);
        sprite.colorG = randomFloat(0.5f, 1.0f);
        sprite.colorB = randomFloat(0.5f, 1.0f);
        sprite.scale = 1.0f;
        sprite.spriteIndex = 5;
    }
    else
    {
        sprite.colorR = 1.0f;
        sprite.colorG = 1.0f;
        sprite.colorB = 1.0f;
        sprite.scale = 0.5f;
        sprite.spriteIndex = randomInt(0, 5);
    }

    ecs_set(gECSWorld, entityId, PositionComponent, position);
    ecs_set(gECSWorld, entityId, MoveComponent, move);
    ecs_set(gECSWorld, entityId, SpriteComponent, sprite);
}

class EntityComponentSystem: public IApp
{
public:
    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.mGLESSupported = true;
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);

        initResourceLoaderInterface(pRenderer);

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

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pLinearClampSampler);

        // Instance buffer
        BufferLoadDesc spriteVbDesc = {};
        spriteVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        spriteVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        spriteVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        spriteVbDesc.mDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        spriteVbDesc.mDesc.mFirstElement = 0;
        spriteVbDesc.mDesc.mElementCount = gMaxSpriteCount;
        spriteVbDesc.mDesc.mStructStride = sizeof(SpriteData);
        spriteVbDesc.mDesc.mSize = gMaxSpriteCount * spriteVbDesc.mDesc.mStructStride;
        spriteVbDesc.pData = gSpriteData;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            spriteVbDesc.ppBuffer = &pSpriteVertexBuffers[i];
            addResource(&spriteVbDesc, NULL);
        }

        // Index buffer
        uint16_t indices[] = {
            0, 1, 2, 2, 1, 3,
        };
        BufferLoadDesc spriteIBDesc = {};
        spriteIBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
        spriteIBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        spriteIBDesc.mDesc.mSize = sizeof(indices);
        spriteIBDesc.pData = indices;
        spriteIBDesc.ppBuffer = &pSpriteIndexBuffer;
        addResource(&spriteIBDesc, NULL);

        // Vertex buffer
        float vertices[] = {
            0,
            1.0,
            2.0,
            3.0,
        };
        BufferLoadDesc spriteVBDesc = {};
        spriteVBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        spriteVBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        spriteVBDesc.mDesc.mSize = sizeof(vertices);
        spriteVBDesc.pData = vertices;
        spriteVBDesc.ppBuffer = &pSpriteVertexBuffer;
        addResource(&spriteVBDesc, NULL);

        // Sprites texture
        TextureLoadDesc textureDesc = {};
        textureDesc.ppTexture = &pSpriteTexture;
        // Textures representing color should be stored in SRGB or HDR format
        textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        textureDesc.pFileName = "sprites.tex";
        addResource(&textureDesc, NULL);

        /************************************************************************/
        // GUI
        /************************************************************************/
        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.1f);
        uiCreateComponent("MT", &guiDesc, &pGUIWindow);

        CheckboxWidget Checkbox;
        Checkbox.pData = &gMultiThread;
        luaRegisterWidget(uiCreateComponentWidget(pGUIWindow, "Threading", &Checkbox, WIDGET_TYPE_CHECKBOX));

        initEntityComponentSystem();
        ecs_log_set_level(0);

        gECSWorld = ecs_init();
        gAvailableCores = getNumCPUCores();
        // Set threads before creating entities to make sure we implemented properly the atomic operations from TheForge in Flecs.
        ecs_set_threads(gECSWorld, gMultiThread ? gAvailableCores : 1);

        ECS_COMPONENT_DEFINE(gECSWorld, SpriteComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, MoveComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, PositionComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, WorldBoundsComponent);

        ECS_COMPONENT_DEFINE(gECSWorld, AvoidComponent);

        ecs_system_desc_t moveSystemDesc = {};
        moveSystemDesc.callback = MoveSystem;
        moveSystemDesc.entity.add[0] = EcsOnUpdate;
        moveSystemDesc.entity.name = "MoveSystem";
        moveSystemDesc.query.filter.terms[0] = { ecs_id(PositionComponent), EcsInOut };
        moveSystemDesc.query.filter.terms[1] = { ecs_id(MoveComponent), EcsInOut };
        moveSystemDesc.multi_threaded = false;
        ecs_system_init(gECSWorld, &moveSystemDesc);

        ecs_system_desc_t avoidanceSystemDesc = {};
        avoidanceSystemDesc.callback = AvoidanceSystem;
        avoidanceSystemDesc.entity.add[0] = EcsPostUpdate;
        avoidanceSystemDesc.entity.name = "AvoidanceSystem";
        avoidanceSystemDesc.query.filter.terms[0] = { ecs_id(PositionComponent), EcsInOut };
        avoidanceSystemDesc.query.filter.terms[1] = { ecs_id(MoveComponent), EcsInOut };
        avoidanceSystemDesc.query.filter.terms[2] = { ecs_id(SpriteComponent), EcsOut };
        avoidanceSystemDesc.query.filter.terms[3] = { ecs_id(AvoidComponent), EcsIn };
        avoidanceSystemDesc.query.filter.terms[3].oper = EcsNot;
        avoidanceSystemDesc.multi_threaded = true;
        ecs_system_init(gECSWorld, &avoidanceSystemDesc);

        ecs_query_desc_t queryDesc = {};
        queryDesc.filter.terms[0].id = { ecs_id(PositionComponent) };
        queryDesc.filter.terms[1].id = { ecs_id(MoveComponent) };
        queryDesc.filter.terms[2].id = { ecs_id(SpriteComponent) };
        queryDesc.filter.terms[3].id = { ecs_id(AvoidComponent) };
        queryDesc.filter.terms[3].oper = EcsNot;

        gECSSpriteQuery = ecs_query_init(gECSWorld, &queryDesc);

        queryDesc.filter.terms[3].oper = EcsAnd;
        gECSAvoidQuery = ecs_query_init(gECSWorld, &queryDesc);

        WorldBoundsComponent* bounds = ecs_singleton_get_mut(gECSWorld, WorldBoundsComponent);
        ASSERT(bounds);
        bounds->xMin = -80.0f;
        bounds->xMax = 80.0f;
        bounds->yMin = -50.0f;
        bounds->yMax = 50.0f;
        ecs_singleton_modified(gECSWorld, WorldBoundsComponent);

        CreationData data = { bounds, "sprite" };
        CreationData avoidData = { bounds, "avoid" };

        for (size_t i = 0; i < gSpriteEntityCount; ++i)
        {
            createEntities(&data, i);
        }

        for (size_t i = 0; i < gAvoidEntityCount; ++i)
        {
            createEntities(&avoidData, i);
        }

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = NULL; // Disable Virtual Joystick
        if (!initInputSystem(&inputDesc))
            return false;

        // App Actions
        InputActionDesc actionDesc = { DefaultInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       this };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::EXIT, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           requestShutdown();
                           return true;
                       } };
        addInputAction(&actionDesc);
        InputActionCallback onUIInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }
            return true;
        };
        actionDesc = { DefaultInputActions::CAPTURE_INPUT,
                       [](InputActionContext* ctx)
                       {
                           setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                           return true;
                       },
                       NULL };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameIndex = 0;
        waitForAllResourceLoads();

        return true;
    }

    void Exit()
    {
        ecs_query_fini(gECSAvoidQuery);
        ecs_query_fini(gECSSpriteQuery);
        ecs_fini(gECSWorld);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pSpriteVertexBuffers[i]);
        }
        removeResource(pSpriteTexture);
        removeResource(pSpriteVertexBuffer);
        removeResource(pSpriteIndexBuffer);

        removeSampler(pRenderer, pLinearClampSampler);

        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);

        exitInputSystem();

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        uiLoad.mHeight = mSettings.mHeight;
        uiLoad.mWidth = mSettings.mWidth;
        uiLoad.mLoadType = pReloadDesc->mType;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        initScreenshotInterface(pRenderer, pGraphicsQueue);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

        static bool oldMultiThread = gMultiThread;
        if (oldMultiThread != gMultiThread)
        {
            oldMultiThread = gMultiThread;
            ecs_set_threads(gECSWorld, gMultiThread ? gAvailableCores : 1);
        }

        // Scene Update
        ecs_progress(gECSWorld, deltaTime * 3.0f);

        // Iterate all entities with transform and plane component
        gDrawSpriteCount = 0;
        float globalScale = 0.05f;

        ecs_iter_t spriteIter = ecs_query_iter(gECSWorld, gECSSpriteQuery);
        while (ecs_query_next(&spriteIter))
        {
            PositionComponent* positions = ecs_term(&spriteIter, PositionComponent, 1);
            SpriteComponent*   sprites = ecs_term(&spriteIter, SpriteComponent, 3);
            for (int i = 0; i < spriteIter.count; i++)
            {
                const PositionComponent& position = positions[i];
                const SpriteComponent&   sprite = sprites[i];
                SpriteData&              spriteData = gSpriteData[gDrawSpriteCount++];
                spriteData.posX = position.x * globalScale;
                spriteData.posY = position.y * globalScale;
                spriteData.scale = sprite.scale * globalScale;
                spriteData.colR = sprite.colorR;
                spriteData.colG = sprite.colorG;
                spriteData.colB = sprite.colorB;
                spriteData.sprite = (float)sprite.spriteIndex;
            }
        }

        ecs_iter_t avoidIter = ecs_query_iter(gECSWorld, gECSAvoidQuery);
        while (ecs_query_next(&avoidIter))
        {
            PositionComponent* positions = ecs_term(&avoidIter, PositionComponent, 1);
            SpriteComponent*   sprites = ecs_term(&avoidIter, SpriteComponent, 3);
            for (int i = 0; i < avoidIter.count; i++)
            {
                const PositionComponent& position = positions[i];
                const SpriteComponent&   sprite = sprites[i];

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
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        // Update uniform buffers.
        const float w = (float)mSettings.mWidth;
        const float h = (float)mSettings.mHeight;
        float       aspect = w / h;

        // Update vertex buffer
        ASSERT(gDrawSpriteCount >= 0 && gDrawSpriteCount <= gMaxSpriteCount);
        BufferUpdateDesc vboUpdateDesc = { pSpriteVertexBuffers[gFrameIndex] };
        vboUpdateDesc.mCurrentState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        beginUpdateResource(&vboUpdateDesc);
        memcpy(vboUpdateDesc.pMappedData, gSpriteData, gDrawSpriteCount * sizeof(SpriteData));
        endUpdateResource(&vboUpdateDesc);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        {
            waitForFences(pRenderer, 1, &elem.pFence);
        }

        resetCmdPool(pRenderer, elem.pCmdPool);

        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        // simply record the screen cleaning command
        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);
        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        // Draw Sprites
        if (gDrawSpriteCount > 0)
        {
            cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Sprites");
            cmdBindPipeline(cmd, pSpritePipeline);
            cmdBindPushConstants(cmd, pRootSignature, gAspectRootConstantIndex, &aspect);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
            uint32_t vertexStride = sizeof(float);
            cmdBindVertexBuffer(cmd, 1, &pSpriteVertexBuffer, &vertexStride, NULL);
            cmdBindIndexBuffer(cmd, pSpriteIndexBuffer, INDEX_TYPE_UINT16, 0);
            cmdDrawIndexedInstanced(cmd, 6, 0, gDrawSpriteCount, 0, 0);
            cmdEndDebugMarker(cmd);
        }

        cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");

        FontDrawDesc uiTextDesc; // default
        uiTextDesc.mFontColor = 0xff00cc00;
        uiTextDesc.mFontSize = 18;
        uiTextDesc.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &uiTextDesc);
        cmdDrawGpuProfile(cmd, float2(8.0f, txtSize.y + 75.f), gGpuProfileToken, &uiTextDesc);

        cmdDrawUserInterface(cmd);
        cmdBindRenderTargets(cmd, NULL);
        cmdEndDebugMarker(cmd);

        barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
        endCmd(cmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);
        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() { return "17_EntityComponentSystem"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mColorClearValue = { { 0.02f, 0.02f, 0.02f, 1.0f } };
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
    }

    void addRootSignatures()
    {
        const char*       pStaticSamplers[] = { "uSampler0" };
        RootSignatureDesc rootDesc = { &pSpriteShader, 1 };
        rootDesc.mStaticSamplerCount = 1;
        rootDesc.ppStaticSamplerNames = pStaticSamplers;
        rootDesc.ppStaticSamplers = &pLinearClampSampler;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);
        gAspectRootConstantIndex = getDescriptorIndexFromName(pRootSignature, "RootConstant");
    }

    void removeRootSignatures() { removeRootSignature(pRenderer, pRootSignature); }

    void addShaders()
    {
        // TODO: rename to sprite
        ShaderLoadDesc spriteShader = {};
        spriteShader.mStages[0].pFileName = "basic.vert";
        spriteShader.mStages[1].pFileName = "basic.frag";

        addShader(pRenderer, &spriteShader, &pSpriteShader);
    }

    void removeShaders() { removeShader(pRenderer, pSpriteShader); }

    void addPipelines()
    {
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
        blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
        blendStateDesc.mIndependentBlend = false;

        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 1;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;

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
        pipelineSettings.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &desc, &pSpritePipeline);
    }

    void removePipelines() { removePipeline(pRenderer, pSpritePipeline); }

    void prepareDescriptorSets()
    {
        DescriptorData params[1] = {};
        params[0].pName = "uTexture0";
        params[0].ppTextures = &pSpriteTexture;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 1, params);
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].pName = "instanceBuffer";
            params[0].ppBuffers = &pSpriteVertexBuffers[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
        }
    }
};

DEFINE_APPLICATION_MAIN(EntityComponentSystem)
