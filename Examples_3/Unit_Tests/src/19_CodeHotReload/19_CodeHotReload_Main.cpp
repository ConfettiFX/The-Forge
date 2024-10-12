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
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

// Code Reload
#include "IGamePlugin.h"
#if FORGE_CODE_HOT_RELOAD
#define CR_HOST
#ifdef __cplusplus
extern "C"
{
#endif
    FORGE_API int systemRun(const char* command, const char** arguments, size_t argumentCount, FileStream* pFs);
#ifdef __cplusplus
}
#endif
#endif
#include "../../../../Common_3/Application/ThirdParty/OpenSource/cr/cr.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h" // Must be the last include in a cpp file

// When we don't have hot-reloading we compile the game code to a static library,
// we need the foward declaration to call the game code and later the linker will link everything.
#if !FORGE_CODE_HOT_RELOAD
int tfMainCodeReload(cr_plugin* ctx, cr_op operation);
#endif

#if defined(_WINDOWS) && FORGE_CODE_HOT_RELOAD
#define VS_WHERE_PATH __FILE__ "/../../../../../Tools/vswhere.exe"
// VisualStudio doesn't set any environment variable, to locate it's install path, you can have several VS installations.
// This function uses a utility that comes with the VS installer, vswhere.exe, to get information about all the VS installation in this PC.
static const char* findMsBuildPath()
{
    // User can't change the installation while running VS, safe to cache it for the entire lifetime of the debug session.
    static char cachedPath[FS_MAX_PATH] = { 0 };

    if (cachedPath[0] != 0)
    {
        // already there
        return cachedPath;
    }

    const char* msBuildSubpath = "/MSBuild/Current/Bin/MSBuild.exe";
    const char* params[] = { "-latest",   "-version",        "\"[16, 17)\"", "-requires", "Microsoft.Component.MSBuild",
                             "-property", "installationPath" };

    const char* tempFilename = "tempMsBuildPath.txt";

    // Run vswhere and output the installation path of VS to our temp file
    FileStream fs{};
    fsOpenStreamFromPath(RD_DEBUG, tempFilename, FM_WRITE, &fs);
    const int result = systemRun(VS_WHERE_PATH, params, TF_ARRAY_COUNT(params), &fs);
    fsCloseStream(&fs);

    if (fsOpenStreamFromPath(RD_DEBUG, tempFilename, FM_READ, &fs))
    {
        char         buffer[FS_MAX_PATH] = { 0 };
        const size_t readSize = fsReadFromStream(&fs, buffer, FS_MAX_PATH);
        if (result == 0)
        {
            if (buffer[readSize - 1] == '\r' || buffer[readSize - 1] == '\n')
                buffer[readSize - 1] = '\0';

            if (buffer[readSize - 2] == '\r' || buffer[readSize - 2] == '\n')
                buffer[readSize - 2] = '\0';

            strcat_s(cachedPath, buffer);
            strcat_s(cachedPath, msBuildSubpath);
        }
        else
        {
            LOGF(eERROR, "Error running vswhere: %s", buffer);
        }
        fsCloseStream(&fs);
    }
    else
    {
        LOGF(eERROR, "vswhere didn't write to file RD_DEBUG/%s or can't open the file", tempFilename);
    }

    return cachedPath;
}
#endif

struct SpriteData
{
    mat4  modelMtx;
    float colR, colG, colB;
    float sprite;
};

// COMPONENTS
ECS_COMPONENT_DECLARE(AppDataComponent);
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

Texture* pSpriteTexture = NULL;

uint32_t gFrameIndex = 0;

SpriteData* gSpriteData = NULL;
uint        gDrawSpriteCount = 0;

ecs_world_t* gECSWorld = NULL;

ecs_query_t* gECSSpriteQuery = NULL;
ecs_query_t* gECSAvoidQuery = NULL;

#define GAME_PLUGIN_NAME                "19a_CodeHotReload_Game"
#define GAME_PLUGIN_ADD_EXTENSION(name) CR_PLUGIN(name)

cr_plugin       gCrGamePlugin = {};
EngineCallbacks gEngineCallbacks = {};
GameCallbacks   gGameCallbacks = {};
GamePlugin      gGamePlugin = {};

// Based on: https://github.com/aras-p/dod-playground

const uint AvoidCount = 20;
#if defined(__ANDROID__)
const uint MaxSpriteCount = 50;
#else
const uint MaxSpriteCount = 11000;
#endif
const uint SpriteEntityCount = MaxSpriteCount - AvoidCount;

static bool gMultiThread = true;

UIComponent* pGUIWindow = nullptr;

uint32_t gFontID = 0;

const char* gTestScripts[] = { "19_CodeHotReload/Test_HotReload.lua" };
uint32_t    gCurrentScriptIndex = 0;
void        RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

struct AvoidComponent
{
    float distance;
};
ECS_COMPONENT_DECLARE(AvoidComponent);

static void MoveSystem(ecs_iter_t* it)
{
    PositionComponent* positions = ecs_term(it, PositionComponent, 1);
    MoveComponent*     moves = ecs_term(it, MoveComponent, 2);

    const WorldBoundsComponent* bounds = ecs_singleton_get(it->world, WorldBoundsComponent);

    for (int i = 0; i < it->count; i++)
    {
        PositionComponent& pos = positions[i];
        MoveComponent&     move = moves[i];

        // update position based on movement velocity & delta time
        pos.pos += move.vel * it->delta_time;

        // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
        if (pos.pos.getX() < bounds->xMin)
        {
            move.vel.setX(-move.vel.getX());
            pos.pos.setX(bounds->xMin);
        }
        if (pos.pos.getX() > bounds->xMax)
        {
            move.vel.setX(-move.vel.getX());
            pos.pos.setX(bounds->xMax);
        }
        if (pos.pos.getY() < bounds->yMin)
        {
            move.vel.setY(-move.vel.getY());
            pos.pos.setY(bounds->yMin);
        }
        if (pos.pos.getY() > bounds->yMax)
        {
            move.vel.setY(-move.vel.getY());
            pos.pos.setY(bounds->yMax);
        }
    }
}

static void SpriteSystem(ecs_iter_t* it)
{
    SpriteComponent* sprites = ecs_term(it, SpriteComponent, 1);

    const AppDataComponent* appData = ecs_singleton_get(it->world, AppDataComponent);

    for (int i = 0; i < it->count; i++)
    {
        appData->mGamePlugin->mGame->UpdateSprite(sprites + i, it->delta_time);
    }
}

static void AvoidanceSystem(ecs_iter_t* it)
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

                Vector2     v = pos.pos - avoidPosition.pos;
                const float sumRadius = avoidDistance.distance + sprite.scale * 0.5f;

                if (lengthSqr(v) < sumRadius * sumRadius)
                {
                    const float len = length(v);
                    v = v / len;
                    const float penetration = sumRadius - len;

                    // Resolve the collision
                    pos.pos += v * penetration;

                    // Move to the collision resolve direction at the same velocity we were moving before.
                    move.vel = length(move.vel) * v;

                    sprite.colorR = avoidSprite.colorR;
                    sprite.colorG = avoidSprite.colorG;
                    sprite.colorB = avoidSprite.colorB;
                }
            }
        }
    }
}

static void FillRenderData(ecs_iter_t* it, uint32_t baseIndex)
{
    PositionComponent* positions = ecs_term(it, PositionComponent, 1);
    SpriteComponent*   sprites = ecs_term(it, SpriteComponent, 2);

    const AppDataComponent* appData = ecs_singleton_get(it->world, AppDataComponent);
    const Vector3           camPos(appData->cameraPos.getX(), appData->cameraPos.getY(), 0.f);
    const float             globalScale = 1.f / appData->cameraScale;
    const float             aspect = appData->aspectRatio;

    for (int i = 0; i < it->count; i++)
    {
        PositionComponent& position = positions[i];
        SpriteComponent&   sprite = sprites[i];

        const uint32_t spriteIdx = baseIndex + it->offset + i;
        ASSERT(spriteIdx < MaxSpriteCount);
        SpriteData& spriteData = gSpriteData[spriteIdx];
        spriteData.modelMtx = mat4::translation((Vector3(position.pos.getX(), position.pos.getY(), 0.f) - camPos) * globalScale) *
                              mat4::scale(Vector3(sprite.scale, sprite.scale * aspect, 0.f) * globalScale) * mat4::rotationZ(sprite.angle);
        spriteData.colR = sprite.colorR;
        spriteData.colG = sprite.colorG;
        spriteData.colB = sprite.colorB;
        spriteData.sprite = (float)sprite.spriteIndex;
    }
}

void FillRenderDataSpritesSystem(ecs_iter_t* it)
{
    FillRenderData(it, AvoidCount); // Put Sprite objects after avoidance objects
}

void FillRenderDataAvoidersSystem(ecs_iter_t* it)
{
    FillRenderData(it,
                   0); // Put Avoidance objects first, when running on with GLES (Android) we might not have enough size in the instance
                       // UBO to hold all objects, this way we ensure the number of objects that might not be visible are sprites
}

struct CreationData
{
    WorldBoundsComponent* bounds = nullptr;
    bool                  bIsAvoider = false;
};

static void createEntity(ecs_world_t* world, CreationData* pData, uintptr_t i)
{
    UNREF_PARAM(i);
    ecs_entity_t entityId = ecs_new_id(world);

    float x = randomFloat(pData->bounds->xMin, pData->bounds->xMax);
    float y = randomFloat(pData->bounds->yMin, pData->bounds->yMax);
    float angle = randomFloat(0.f, PI * 2);
    float speed = randomFloat(0.3f, 0.6f);

    PositionComponent position = { Vector2(x, y) };
    MoveComponent     move = { Vector2(cosf(angle), sinf(angle)) * speed };
    SpriteComponent   sprite = {};

    if (pData->bIsAvoider)
    {
        AvoidComponent avoid = { 1.f };
        ecs_set(world, entityId, AvoidComponent, avoid);

        position.pos *= 0.2f;
        sprite.colorR = randomFloat(0.5f, 1.0f);
        sprite.colorG = randomFloat(0.5f, 1.0f);
        sprite.colorB = randomFloat(0.5f, 1.0f);
        sprite.scale = 2.0f;
        sprite.spriteIndex = 5;
    }
    else
    {
        sprite.colorR = 1.0f;
        sprite.colorG = 1.0f;
        sprite.colorB = 1.0f;
        sprite.scale = 1.0f;
        sprite.spriteIndex = randomInt(0, 5);
    }

    ecs_set(world, entityId, PositionComponent, position);
    ecs_set(world, entityId, MoveComponent, move);
    ecs_set(world, entityId, SpriteComponent, sprite);
}

class CodeHotReload: public IApp
{
public:
    bool Init()
    {
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage("Failed To Initialize renderer!");
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

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
        initProfiler(&profiler);

        gGpuProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pLinearClampSampler);

        gSpriteData = (SpriteData*)tf_malloc(MaxSpriteCount * sizeof(SpriteData));

        // Instance buffer
        BufferLoadDesc spriteVbDesc = {};
        spriteVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        spriteVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        spriteVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        spriteVbDesc.mDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        spriteVbDesc.mDesc.mFirstElement = 0;
        spriteVbDesc.mDesc.mElementCount = MaxSpriteCount;
        spriteVbDesc.mDesc.mStructStride = sizeof(SpriteData);
        spriteVbDesc.mDesc.mSize = MaxSpriteCount * spriteVbDesc.mDesc.mStructStride;
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

        // Hot Reloading initialization
        gEngineCallbacks.Log = [](LogLevel logLevel, const char* message) { LOGF(logLevel, "%s", message); };

        gGamePlugin.mEngine = &gEngineCallbacks;
        gGamePlugin.mGame = &gGameCallbacks;

        gCrGamePlugin.userdata = &gGamePlugin;

#if FORGE_CODE_HOT_RELOAD
        // assuming it's next to the executable
        // this will need to be customized depending on
        // whether we're distributing (should be in bundle)
        // or development (should be external to support hot reloading)
        char gamePluginPath[FS_MAX_PATH] = "./" GAME_PLUGIN_ADD_EXTENSION(GAME_PLUGIN_NAME);
        if (!cr_plugin_open(gCrGamePlugin, gamePluginPath))
        {
            ASSERT(false);
            return false;
        }
        // Plugin open doesn't load the dll, we need to call update for that to happen
        const int crPluginUpdateResult = cr_plugin_update(gCrGamePlugin);
        ASSERT(crPluginUpdateResult == 0);
#else
        tfMainCodeReload(&gCrGamePlugin, CR_LOAD);
#endif

        initEntityComponentSystem();
        ecs_log_set_level(-2);

        gECSWorld = ecs_init();

        // Set threads before creating entities to make sure we implemented properly the atomic operations from TheForge in Flecs.
        ecs_set_threads(gECSWorld, gMultiThread ? max<uint32_t>(getNumCPUCores() - 1, 1) : 1);

        ECS_COMPONENT_DEFINE(gECSWorld, AppDataComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, SpriteComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, MoveComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, PositionComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, WorldBoundsComponent);
        ECS_COMPONENT_DEFINE(gECSWorld, AvoidComponent);

        ECS_SYSTEM(gECSWorld, MoveSystem, EcsOnUpdate, PositionComponent, MoveComponent);
        ECS_SYSTEM(gECSWorld, AvoidanceSystem, EcsOnUpdate, PositionComponent, MoveComponent, SpriteComponent, !AvoidComponent);
        ECS_SYSTEM(gECSWorld, SpriteSystem, EcsOnUpdate, SpriteComponent, PositionComponent, MoveComponent, !AvoidComponent);
        ECS_SYSTEM(gECSWorld, FillRenderDataSpritesSystem, EcsPostUpdate, PositionComponent, SpriteComponent, !AvoidComponent);
        ECS_SYSTEM(gECSWorld, FillRenderDataAvoidersSystem, EcsPostUpdate, PositionComponent, SpriteComponent, AvoidComponent);

        ecs_query_desc_t queryDesc = {};
        queryDesc.filter.terms[0].id = { ecs_id(PositionComponent) };
        queryDesc.filter.terms[1].id = { ecs_id(MoveComponent) };
        queryDesc.filter.terms[2].id = { ecs_id(SpriteComponent) };
        queryDesc.filter.terms[3].id = { ecs_id(AvoidComponent) };
        queryDesc.filter.terms[3].oper = EcsNot;

        gECSSpriteQuery = ecs_query_init(gECSWorld, &queryDesc);

        queryDesc.filter.terms[3].oper = EcsAnd;
        gECSAvoidQuery = ecs_query_init(gECSWorld, &queryDesc);

        {
            AppDataComponent* appData = ecs_singleton_get_mut(gECSWorld, AppDataComponent);
            ASSERT(appData);
            memset((void*)appData, 0, sizeof(AppDataComponent));
            appData->aspectRatio = mSettings.mWidth / (float)mSettings.mHeight;
            appData->cameraPos = Vector2(0.f);
            appData->cameraScale = 30.f;
            appData->mGamePlugin = &gGamePlugin;

            gGamePlugin.mAppData = appData;
            ecs_singleton_modified(gECSWorld, AppDataComponent);
        }

        WorldBoundsComponent* bounds = ecs_singleton_get_mut(gECSWorld, WorldBoundsComponent);
        ASSERT(bounds);
        gGamePlugin.mGame->UpdateWorldBounds(bounds, 0.f);
        ecs_singleton_modified(gECSWorld, WorldBoundsComponent);

        CreationData data = { bounds, false };
        CreationData avoidData = { bounds, true };

        for (size_t i = 0; i < SpriteEntityCount; ++i)
        {
            createEntity(gECSWorld, &data, i);
        }

        for (size_t i = 0; i < AvoidCount; ++i)
        {
            createEntity(gECSWorld, &avoidData, i);
        }

        gDrawSpriteCount = SpriteEntityCount + AvoidCount;

        // App Actions
        extern bool gVirtualJoystickEnable;
        gVirtualJoystickEnable = false;
        AddCustomInputBindings();
        initScreenshotInterface(pRenderer, pGraphicsQueue);
        gFrameIndex = 0;
        waitForAllResourceLoads();

        return true;
    }

    void Exit()
    {
        exitScreenshotInterface();
#if FORGE_CODE_HOT_RELOAD
        cr_plugin_close(gCrGamePlugin);
#else
        tfMainCodeReload(&gCrGamePlugin, CR_CLOSE);
#endif

        gGamePlugin = {};

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

        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);

        tf_free(gSpriteData);

        gSpriteData = NULL;

        exitResourceLoaderInterface(pRenderer);
        exitQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        exitGPUConfiguration();
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
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent("CodeHotReload", &guiDesc, &pGUIWindow);

#if FORGE_CODE_HOT_RELOAD
#if !defined(__APPLE__)
            const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
            LuaScriptDesc  scriptDescs[numScripts] = {};
            for (uint32_t i = 0; i < numScripts; ++i)
                scriptDescs[i].pScriptFileName = gTestScripts[i];
            luaDefineScripts(scriptDescs, numScripts);

            DropdownWidget ddTestScripts;
            ddTestScripts.pData = &gCurrentScriptIndex;
            ddTestScripts.pNames = gTestScripts;
            ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
            luaRegisterWidget(uiAddComponentWidget(pGUIWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

            ButtonWidget bRunScript;
            UIWidget*    pRunScript = uiAddComponentWidget(pGUIWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
            luaRegisterWidget(pRunScript);
#endif
#endif

            CheckboxWidget Checkbox;
            Checkbox.pData = &gMultiThread;
            luaRegisterWidget(uiAddComponentWidget(pGUIWindow, "Threading", &Checkbox, WIDGET_TYPE_CHECKBOX));

#if FORGE_CODE_HOT_RELOAD
#if defined(__APPLE__)
            LabelWidget labelWidget;
            uiAddComponentWidget(pGUIWindow, "Use Command+B in XCode to rebuild CodeHotReload_Main.", &labelWidget, WIDGET_TYPE_LABEL);
#else
            ButtonWidget       button;
            UIWidget*          widget = uiAddComponentWidget(pGUIWindow, "Rebuild Game", &button, WIDGET_TYPE_BUTTON);
            static WindowDesc* pWindowDesc = pWindow;
            widget->pOnEdited = [](void* pUserData)
            {
                UNREF_PARAM(pUserData);

                const char* buildCommand = NULL;
                const char* args[4] = {};
                size_t      buildArgUsed;

#if defined(_WINDOWS)
                buildCommand = findMsBuildPath();

                if (buildCommand == nullptr) // -V547
                {
                    errorMessagePopup(
                        "Couldn't find vswhere.exe",
                        "We couldn't find the Visual Studio installer in the default installation path. Without that we can't "
                        "locate msbuild.exe",
                        &pWindowDesc->handle, NULL);
                    return;
                }

                // The build settup for The Forge for PC contains all project files in the same directory,
                // this is why we don't need any path in here, Visual Studio uses the project directory as
                // the Working Directory when launching programs. Use relative path from executable during automated testing.
                args[0] = "../../../" GAME_PLUGIN_NAME ".vcxproj";
                args[1] = "-property:Platform=x64";
#ifdef FORGE_DEBUG
                args[2] = "-property:Configuration=Debug";
#else
                args[2] = "-property:Configuration=Release";
#endif
                buildArgUsed = 3;
#elif defined(__linux__)

                // No need to set the configuration in linux CodeLite updates the makefile based on the configuration,
                // running with this makefile will compile for the same configuration we are currently working on.

                buildCommand = "make";
                args[0] = "all";
                args[1] = "--jobs=8";
                // args[0] = "all -j 8";                      // Build using 8 jobs (magic number)
#if defined(AUTOMATED_TESTING)
                args[2] = "--directory=19_CodeHotReload/"; // Makefile is stored one down the codelite directory
#else
                args[2] = "--directory=../"; // Makefile is stored one up the debug directory
#endif
                args[3] = "--file=" GAME_PLUGIN_NAME ".mk";
                buildArgUsed = 4;
#endif

                char       buildLogFile[FS_MAX_PATH] = "CodeReloadBuild.log";
                FileStream fs{};
                fsOpenStreamFromPath(RD_DEBUG, buildLogFile, FM_WRITE, &fs);
                const int buildResult = systemRun(buildCommand, args, buildArgUsed, &fs);
                fsCloseStream(&fs);

                if (buildResult != 0)
                {
                    LOGF(eWARNING, "Couldn't rebuild the project. Build log file is RD_DEBUG/'%s'", buildLogFile);

                    char message[1024] = {};
                    snprintf(message, sizeof(message),
                             "Compilation of the Hot Reloadable module failed.\nPlease check the log file in: RD_DEBUG/%s", buildLogFile);

                    errorMessagePopup("Build Failed", message, &pWindowDesc->handle, NULL);
                }
            };
            luaRegisterWidget(widget);
#endif // defined(__APPLE__)
#endif // FORGE_CODE_HOT_RELOAD

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
            uiRemoveComponent(pGUIWindow);
            unloadProfilerUI();
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }
    }

    void Update(float deltaTime)
    {
#if FORGE_CODE_HOT_RELOAD
        if (cr_plugin_update(gCrGamePlugin) != 0)
        {
            ASSERT(false);
        }
#else
        tfMainCodeReload(&gCrGamePlugin, CR_STEP);
#endif

        if (!uiIsFocused())
        {
            gGamePlugin.mAppData->cameraMovementDir = { inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) };
            gGamePlugin.mAppData->cameraZoom = -inputGetValue(0, CUSTOM_LOOK_Y);
            if (inputGetValue(0, CUSTOM_TOGGLE_FULLSCREEN))
            {
                toggleFullscreen(pWindow);
            }
            if (inputGetValue(0, CUSTOM_TOGGLE_UI))
            {
                uiToggleActive();
            }
            if (inputGetValue(0, CUSTOM_DUMP_PROFILE))
            {
                dumpProfileData(GetName());
            }
            if (inputGetValue(0, CUSTOM_EXIT))
            {
                requestShutdown();
            }
        }

        gGamePlugin.mGame->UpdateCamera(&gGamePlugin, deltaTime);
        gGamePlugin.mAppData->cameraMovementDir = {};
        gGamePlugin.mAppData->cameraZoom = {};

        WorldBoundsComponent* bounds = ecs_singleton_get_mut(gECSWorld, WorldBoundsComponent);
        gGamePlugin.mGame->UpdateWorldBounds(bounds, deltaTime);
        ecs_singleton_modified(gECSWorld, WorldBoundsComponent);

        static bool oldMultiThread = gMultiThread;
        if (oldMultiThread != gMultiThread)
        {
            oldMultiThread = gMultiThread;
            ecs_set_threads(gECSWorld, gMultiThread ? max<uint32_t>(getNumCPUCores() - 1, 1) : 1);
        }

        // We expose a pointer to AppDataComponent in GamePlugin so that we can hadle inputs in the hot-reloadable code.
        // We lazily notify flecs about possible changes, ideally we should have this call in the hot reloadable module by compiling
        // flecs to dll.
        ecs_singleton_modified(gECSWorld, AppDataComponent);

        ecs_progress(gECSWorld, deltaTime * 3.0f);
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

        // Update vertex buffer
        ASSERT(gDrawSpriteCount >= 0 && gDrawSpriteCount <= MaxSpriteCount);
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

    const char* GetName() { return "19_CodeHotReload_Main"; }

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
    }

    void removeRootSignatures() { removeRootSignature(pRenderer, pRootSignature); }

    void addShaders()
    {
        // TODO: rename to sprite
        ShaderLoadDesc spriteShader = {};
        spriteShader.mVert.pFileName = "basic.vert";
        spriteShader.mFrag.pFileName = "basic.frag";

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

DEFINE_APPLICATION_MAIN(CodeHotReload)
