/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

#define MAX_PLANETS 20 // Does not affect test, just for allocating space in uniform block. Must match with shader.

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

// fsl
#include "../../../../Common_3/Graphics/FSL/defaults.h"
#include "./Shaders/FSL/Resources.h.fsl"
#include "./Shaders/FSL/Global.srt.h"

#define MAX_COMMANDS 2000

/// Demo structures
typedef struct PlanetInfoStruct
{
    mat4  mTranslationMat;
    mat4  mScaleMat;
    mat4  mSharedMat; // Matrix to pass down to children
    vec4  mColor;
    uint  mParentIndex;
    float mYOrbitSpeed; // Rotation speed around parent
    float mZOrbitSpeed;
    float mRotationSpeed; // Rotation speed around self
    float mMorphingSpeed; // Speed of morphing betwee cube and sphere
} PlanetInfoStruct;

// But we only need Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;
const uint     gNumPlanets = 11;     // Sun, Mercury -> Neptune, Pluto, Moon
const uint     gTimeOffset = 600000; // For visually better starting locations
const float    gRotSelfScale = 0.0004f;
const float    gRotOrbitYScale = 0.001f;
const float    gRotOrbitZScale = 0.00001f;

Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Semaphore*    pImageAcquiredSemaphore = NULL;

Shader*      pSphereShader = NULL;
Buffer*      pSphereVertexBuffer = NULL;
Buffer*      pSphereIndexBuffer = NULL;
uint32_t     gSphereIndexCount = 0;
Pipeline*    pSpherePipeline = NULL;
VertexLayout gSphereVertexLayout = {};
uint32_t     gSphereLayoutType = 0;

Shader*        pSkyBoxDrawShader = NULL;
Buffer*        pSkyBoxVertexBuffer = NULL;
Pipeline*      pSkyBoxDrawPipeline = NULL;
Texture*       pSkyBoxTextures[6];
Sampler*       pSkyBoxSampler = {};
DescriptorSet* pDescriptorSetTexture = { NULL };
DescriptorSet* pDescriptorSetUniforms = { NULL };
DescriptorSet* pDescriptorSetPlanetGroupData = NULL;

Buffer*       pUniformBuffer[gDataBufferCount] = { NULL };
GPURingBuffer gPlanetGroupBuffer[gDataBufferCount] = {};

uint32_t     gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;
ProfileToken gWaitForFenceCpuToken = PROFILE_INVALID_TOKEN;
ProfileToken gRecordingCpuToken = PROFILE_INVALID_TOKEN;

int              gNumberOfSpherePoints;
UniformData      gUniformData;
PlanetInfoStruct gPlanetInfoData[gNumPlanets];

ICameraController* pCameraController = NULL;

// VR 2D layer transform (positioned at -1 along the Z axis, default rotation, default scale)
VR2DLayerDesc gVR2DLayer{ { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, 1.0f };

UIComponent* pGuiWindow = NULL;

uint32_t gFontID = 0;

QueryPool* pPipelineStatsQueryPool[gDataBufferCount] = {};

const char* pSkyBoxImageFileNames[] = { "Skybox_right1.tex",  "Skybox_left2.tex",  "Skybox_top3.tex",
                                        "Skybox_bottom4.tex", "Skybox_front5.tex", "Skybox_back6.tex" };

FontDrawDesc gFrameTimeDraw;

uint32_t gTotalPlanetCommands = 100;

const float GOLDEN_ANGLE = PI * (3.f - sqrtf(5.f));

const int MAX_SUBMITS = MAX_GPU_CMDS_PER_POOL;

uint32_t gTotalSubmits = 2;

Semaphore* pWaitForPreviousCommandSemaphore[MAX_SUBMITS] = { nullptr };

// Divide commands into queues. if we have 500 commands, total submits count is 3, dividers are [150, 300, 500], then we'll submit (0 - 149)
// in queue 0, (150 - 299) in queue 1, (300 - 499) in queue 2
uint32_t gCmdDividers[MAX_SUBMITS] = { 0 };

float gPreCalculateMath[1000] = { 0.f };

// Generate sky box vertex buffer
const float gSkyBoxPoints[] = {
    10.0f,  -10.0f, -10.0f, 6.0f, // -z
    -10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
    -10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

    -10.0f, -10.0f, 10.0f,  2.0f, //-x
    -10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
    -10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

    10.0f,  -10.0f, -10.0f, 1.0f, //+x
    10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
    10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

    -10.0f, -10.0f, 10.0f,  5.0f, // +z
    -10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
    10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

    -10.0f, 10.0f,  -10.0f, 3.0f, //+y
    10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
    10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

    10.0f,  -10.0f, 10.0f,  4.0f, //-y
    10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
    -10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
};

static unsigned char gPipelineStatsCharArray[2048] = {};
static bstring       gPipelineStats = bfromarr(gPipelineStatsCharArray);

void reloadRequest(void*)
{
    ReloadDesc reload{ RELOAD_TYPE_SHADER };
    requestReload(&reload);
}

const char* gTestScripts[] = { "TestQueueSizes.lua" };
uint32_t    gCurrentScriptIndex = 0;

void RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

static void add_attribute(VertexLayout* layout, ShaderSemantic semantic, TinyImageFormat format, uint32_t offset)
{
    uint32_t n_attr = layout->mAttribCount++;

    VertexAttrib* attr = layout->mAttribs + n_attr;

    attr->mSemantic = semantic;
    attr->mFormat = format;
    attr->mBinding = 0;
    attr->mLocation = n_attr;
    attr->mOffset = offset;
}

static void copy_attribute(VertexLayout* layout, void* buffer_data, uint32_t offset, uint32_t size, uint32_t vcount, void* data)
{
    uint8_t* dst_data = static_cast<uint8_t*>(buffer_data);
    uint8_t* src_data = static_cast<uint8_t*>(data);
    for (uint32_t i = 0; i < vcount; ++i)
    {
        memcpy(dst_data + offset, src_data, size);

        dst_data += layout->mBindings[0].mStride;
        src_data += size;
    }
}

static void compute_normal(const float* src, float* dst)
{
    float len = sqrtf(src[0] * src[0] + src[1] * src[1] + src[2] * src[2]);
    if (len == 0)
    {
        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
    }
    else
    {
        dst[0] = src[0] / len;
        dst[1] = src[1] / len;
        dst[2] = src[2] / len;
    }
}

static void generate_complex_mesh()
{
    gSphereVertexLayout = {};

// number of vertices on a quad side, must be >= 2
#define DETAIL_LEVEL 12

    // static here to prevent stack overflow
    static float verts[6][DETAIL_LEVEL][DETAIL_LEVEL][3];
    static float sqNormals[6][DETAIL_LEVEL][DETAIL_LEVEL][3];
    static float sphNormals[6][DETAIL_LEVEL][DETAIL_LEVEL][3];

    for (int i = 0; i < 6; ++i)
    {
        for (int x = 0; x < DETAIL_LEVEL; ++x)
        {
            for (int y = 0; y < DETAIL_LEVEL; ++y)
            {
                float* vert = verts[i][x][y];
                float* sqNorm = sqNormals[i][x][y];

                sqNorm[0] = 0;
                sqNorm[1] = 0;
                sqNorm[2] = 0;

                float fx = 2 * (float(x) / float(DETAIL_LEVEL - 1)) - 1;
                float fy = 2 * (float(y) / float(DETAIL_LEVEL - 1)) - 1;

                switch (i)
                {
                case 0:
                    vert[0] = -1, vert[1] = fx, vert[2] = fy;
                    sqNorm[0] = -1;
                    break;
                case 1:
                    vert[0] = 1, vert[1] = -fx, vert[2] = fy;
                    sqNorm[0] = 1;
                    break;
                case 2:
                    vert[0] = -fx, vert[1] = fy, vert[2] = 1;
                    sqNorm[2] = 1;
                    break;
                case 3:
                    vert[0] = fx, vert[1] = fy, vert[2] = -1;
                    sqNorm[2] = -1;
                    break;
                case 4:
                    vert[0] = fx, vert[1] = 1, vert[2] = fy;
                    sqNorm[1] = 1;
                    break;
                case 5:
                    vert[0] = -fx, vert[1] = -1, vert[2] = fy;
                    sqNorm[1] = -1;
                    break;
                }

                compute_normal(vert, sphNormals[i][x][y]);
            }
        }
    }

    static uint8_t sqColors[6][DETAIL_LEVEL][DETAIL_LEVEL][3];
    static uint8_t spColors[6][DETAIL_LEVEL][DETAIL_LEVEL][3];
    for (int i = 0; i < 6; ++i)
    {
        for (int x = 0; x < DETAIL_LEVEL; ++x)
        {
            uint8_t spColorTemplate[3] = {
                uint8_t(randomInt(0, 256)),
                uint8_t(randomInt(0, 256)),
                uint8_t(randomInt(0, 256)),
            };

            float rx = 1 - abs((float(x) / DETAIL_LEVEL) * 2 - 1);

            for (int y = 0; y < DETAIL_LEVEL; ++y)
            {
                float    ry = 1 - abs((float(y) / DETAIL_LEVEL) * 2 - 1);
                uint32_t close_ratio = uint32_t(rx * ry * 255);

                uint8_t* sq_color = sqColors[i][x][y];
                uint8_t* sp_color = spColors[i][x][y];

                sq_color[0] = (uint8_t)((randomInt(0, 256) * close_ratio) / 255);
                sq_color[1] = (uint8_t)((randomInt(0, 256) * close_ratio) / 255);
                sq_color[2] = (uint8_t)((randomInt(0, 256) * close_ratio) / 255);

                sp_color[0] = (uint8_t)((spColorTemplate[0] * close_ratio) / 255);
                sp_color[1] = (uint8_t)((spColorTemplate[1] * close_ratio) / 255);
                sp_color[2] = (uint8_t)((spColorTemplate[2] * close_ratio) / 255);
            }
        }
    }

    static uint16_t indices[6][DETAIL_LEVEL - 1][DETAIL_LEVEL - 1][6];
    for (int i = 0; i < 6; ++i)
    {
        uint32_t o = DETAIL_LEVEL * DETAIL_LEVEL * i;
        for (int x = 0; x < DETAIL_LEVEL - 1; ++x)
        {
            for (int y = 0; y < DETAIL_LEVEL - 1; ++y)
            {
                uint16_t* quadIndices = indices[i][x][y];

#define vid(vx, vy) (o + (vx)*DETAIL_LEVEL + (vy))
                quadIndices[0] = (uint16_t)vid(x, y);
                quadIndices[1] = (uint16_t)vid(x, y + 1);
                quadIndices[2] = (uint16_t)vid(x + 1, y + 1);
                quadIndices[3] = (uint16_t)vid(x + 1, y + 1);
                quadIndices[4] = (uint16_t)vid(x + 1, y);
                quadIndices[5] = (uint16_t)vid(x, y);
#undef vid
            }
        }
    }

#undef DETAIL_LEVEL

    void*    bufferData = nullptr;
    uint32_t vertexCount = sizeof(verts) / 12;
    size_t   bufferSize;

    gSphereVertexLayout.mBindingCount = 1;

    switch (gSphereLayoutType)
    {
    default:
    case 0:
    {
        //  0-12 sq positions,
        // 12-16 sq colors
        // 16-28 sq normals
        // 28-32 sp colors
        // 32-44 sp positions + sp normals

        gSphereVertexLayout.mBindings[0].mStride = 44;
        size_t vsize = vertexCount * gSphereVertexLayout.mBindings[0].mStride;
        bufferSize = vsize;
        bufferData = tf_calloc(1, bufferSize);

        add_attribute(&gSphereVertexLayout, SEMANTIC_POSITION, TinyImageFormat_R32G32B32_SFLOAT, 0);
        add_attribute(&gSphereVertexLayout, SEMANTIC_NORMAL, TinyImageFormat_R32G32B32_SFLOAT, 16);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD1, TinyImageFormat_R32G32B32_SFLOAT, 32);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD3, TinyImageFormat_R32G32B32_SFLOAT, 32);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD0, TinyImageFormat_R8G8B8A8_UNORM, 12);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD2, TinyImageFormat_R8G8B8A8_UNORM, 28);

        copy_attribute(&gSphereVertexLayout, bufferData, 0, 12, vertexCount, verts);
        copy_attribute(&gSphereVertexLayout, bufferData, 12, 3, vertexCount, sqColors);
        copy_attribute(&gSphereVertexLayout, bufferData, 16, 12, vertexCount, sqNormals);
        copy_attribute(&gSphereVertexLayout, bufferData, 28, 3, vertexCount, spColors);
        copy_attribute(&gSphereVertexLayout, bufferData, 32, 12, vertexCount, sphNormals);
    }
    break;
    case 1:
    {
        //  0-12 sq positions,
        // 16-28 sq normals
        // 32-34 sq colors
        // 36-40 sp colors
        // 48-62 sp positions
        // 64-76 sp normals

        gSphereVertexLayout.mBindings[0].mStride = 80;
        size_t vsize = vertexCount * gSphereVertexLayout.mBindings[0].mStride;
        bufferSize = vsize;
        bufferData = tf_calloc(1, bufferSize);

        add_attribute(&gSphereVertexLayout, SEMANTIC_POSITION, TinyImageFormat_R32G32B32_SFLOAT, 0);
        add_attribute(&gSphereVertexLayout, SEMANTIC_NORMAL, TinyImageFormat_R32G32B32_SFLOAT, 16);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD1, TinyImageFormat_R32G32B32_SFLOAT, 48);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD3, TinyImageFormat_R32G32B32_SFLOAT, 64);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD0, TinyImageFormat_R8G8B8A8_UNORM, 32);
        add_attribute(&gSphereVertexLayout, SEMANTIC_TEXCOORD2, TinyImageFormat_R8G8B8A8_UNORM, 36);

        copy_attribute(&gSphereVertexLayout, bufferData, 0, 12, vertexCount, verts);
        copy_attribute(&gSphereVertexLayout, bufferData, 16, 12, vertexCount, sqNormals);
        copy_attribute(&gSphereVertexLayout, bufferData, 36, 3, vertexCount, spColors);
        copy_attribute(&gSphereVertexLayout, bufferData, 32, 3, vertexCount, sqColors);
        copy_attribute(&gSphereVertexLayout, bufferData, 48, 12, vertexCount, sphNormals);
        copy_attribute(&gSphereVertexLayout, bufferData, 64, 12, vertexCount, sphNormals);
    }
    break;
    }

    gSphereIndexCount = sizeof(indices) / sizeof(uint16_t);

    BufferLoadDesc sphereVbDesc = {};
    sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    sphereVbDesc.mDesc.mSize = bufferSize;
    sphereVbDesc.pData = bufferData;
    sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
    addResource(&sphereVbDesc, nullptr);

    BufferLoadDesc sphereIbDesc = {};
    sphereIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    sphereIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    sphereIbDesc.mDesc.mSize = sizeof(indices);
    sphereIbDesc.pData = indices;
    sphereIbDesc.ppBuffer = &pSphereIndexBuffer;
    addResource(&sphereIbDesc, nullptr);

    waitForAllResourceLoads();

    tf_free(bufferData);
}

class QueueSubmitProfiling: public IApp
{
public:
    bool Init()
    {
        // window and renderer setup
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage(getUnsupportedGPUMsg());
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        if (pRenderer->pGpu->mPipelineStatsQueries)
        {
            QueryPoolDesc poolDesc = {};
            poolDesc.mQueryCount = MAX_SUBMITS; // Use a dedicated query for each submit
            poolDesc.mType = QUERY_TYPE_PIPELINE_STATISTICS;
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                initQueryPool(pRenderer, &poolDesc, &pPipelineStatsQueryPool[i]);
            }
        }

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = MAX_SUBMITS;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

        for (uint32_t i = 0; i < TF_ARRAY_COUNT(pWaitForPreviousCommandSemaphore); i++)
            initSemaphore(pRenderer, &pWaitForPreviousCommandSemaphore[i]);

        initResourceLoaderInterface(pRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(pRenderer, &rootDesc);

        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_LINEAR,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pSkyBoxSampler);

        // Loads Skybox Textures
        for (int i = 0; i < 6; ++i)
        {
            TextureLoadDesc textureDesc = {};
            textureDesc.pFileName = pSkyBoxImageFileNames[i];
            textureDesc.ppTexture = &pSkyBoxTextures[i];
            // Textures representing color should be stored in SRGB or HDR format
            textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&textureDesc, NULL);
        }

        uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
        BufferLoadDesc skyboxVbDesc = {};
        skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
        skyboxVbDesc.pData = gSkyBoxPoints;
        skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
        addResource(&skyboxVbDesc, NULL);

        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.mDesc.pName = "UniformBlock";
            ubDesc.mDesc.mSize = sizeof(UniformData);
            ubDesc.ppBuffer = &pUniformBuffer[i];
            addResource(&ubDesc, NULL);

            addUniformGPURingBuffer(pRenderer, sizeof(PlanetGroupData) * MAX_COMMANDS, &gPlanetGroupBuffer[i], true);
        }

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

        // Gpu profiler can only be added after initProfile.
        gGpuProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gWaitForFenceCpuToken = getCpuProfileToken("CPU", "Wait for fence", 0xff00ffff);
        gRecordingCpuToken = getCpuProfileToken("CPU", "Record and submit commands", 0xff00ffff);

        const uint32_t numScripts = TF_ARRAY_COUNT(gTestScripts);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        DEFINE_LUA_SCRIPTS(scriptDescs, numScripts);

        waitForAllResourceLoads();

        // Setup planets (Rotation speeds are relative to Earth's, some values randomly given)
        // Sun
        gPlanetInfoData[0].mParentIndex = 0;
        gPlanetInfoData[0].mYOrbitSpeed = 0; // Earth years for one orbit
        gPlanetInfoData[0].mZOrbitSpeed = 0;
        gPlanetInfoData[0].mRotationSpeed = 24.0f; // Earth days for one rotation
        gPlanetInfoData[0].mTranslationMat = mat4::identity();
        gPlanetInfoData[0].mScaleMat = mat4::scale(vec3(10.0f));
        gPlanetInfoData[0].mColor = vec4(0.97f, 0.38f, 0.09f, 0.0f);
        gPlanetInfoData[0].mMorphingSpeed = 0.2f;

        // Mercury
        gPlanetInfoData[1].mParentIndex = 0;
        gPlanetInfoData[1].mYOrbitSpeed = 0.5f;
        gPlanetInfoData[1].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[1].mRotationSpeed = 58.7f;
        gPlanetInfoData[1].mTranslationMat = mat4::translation(vec3(10.0f, 0, 0));
        gPlanetInfoData[1].mScaleMat = mat4::scale(vec3(1.0f));
        gPlanetInfoData[1].mColor = vec4(0.45f, 0.07f, 0.006f, 1.0f);
        gPlanetInfoData[1].mMorphingSpeed = 5;

        // Venus
        gPlanetInfoData[2].mParentIndex = 0;
        gPlanetInfoData[2].mYOrbitSpeed = 0.8f;
        gPlanetInfoData[2].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[2].mRotationSpeed = 243.0f;
        gPlanetInfoData[2].mTranslationMat = mat4::translation(vec3(20.0f, 0, 5));
        gPlanetInfoData[2].mScaleMat = mat4::scale(vec3(2));
        gPlanetInfoData[2].mColor = vec4(0.6f, 0.32f, 0.006f, 1.0f);
        gPlanetInfoData[2].mMorphingSpeed = 1;

        // Earth
        gPlanetInfoData[3].mParentIndex = 0;
        gPlanetInfoData[3].mYOrbitSpeed = 1.0f;
        gPlanetInfoData[3].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[3].mRotationSpeed = 1.0f;
        gPlanetInfoData[3].mTranslationMat = mat4::translation(vec3(30.0f, 0, 0));
        gPlanetInfoData[3].mScaleMat = mat4::scale(vec3(4));
        gPlanetInfoData[3].mColor = vec4(0.07f, 0.028f, 0.61f, 1.0f);
        gPlanetInfoData[3].mMorphingSpeed = 1;

        // Mars
        gPlanetInfoData[4].mParentIndex = 0;
        gPlanetInfoData[4].mYOrbitSpeed = 2.0f;
        gPlanetInfoData[4].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[4].mRotationSpeed = 1.1f;
        gPlanetInfoData[4].mTranslationMat = mat4::translation(vec3(40.0f, 0, 0));
        gPlanetInfoData[4].mScaleMat = mat4::scale(vec3(3));
        gPlanetInfoData[4].mColor = vec4(0.79f, 0.07f, 0.006f, 1.0f);
        gPlanetInfoData[4].mMorphingSpeed = 1;

        // Jupiter
        gPlanetInfoData[5].mParentIndex = 0;
        gPlanetInfoData[5].mYOrbitSpeed = 11.0f;
        gPlanetInfoData[5].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[5].mRotationSpeed = 0.4f;
        gPlanetInfoData[5].mTranslationMat = mat4::translation(vec3(50.0f, 0, 0));
        gPlanetInfoData[5].mScaleMat = mat4::scale(vec3(8));
        gPlanetInfoData[5].mColor = vec4(0.32f, 0.13f, 0.13f, 1);
        gPlanetInfoData[5].mMorphingSpeed = 6;

        // Saturn
        gPlanetInfoData[6].mParentIndex = 0;
        gPlanetInfoData[6].mYOrbitSpeed = 29.4f;
        gPlanetInfoData[6].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[6].mRotationSpeed = 0.5f;
        gPlanetInfoData[6].mTranslationMat = mat4::translation(vec3(60.0f, 0, 0));
        gPlanetInfoData[6].mScaleMat = mat4::scale(vec3(6));
        gPlanetInfoData[6].mColor = vec4(0.45f, 0.45f, 0.21f, 1.0f);
        gPlanetInfoData[6].mMorphingSpeed = 1;

        // Uranus
        gPlanetInfoData[7].mParentIndex = 0;
        gPlanetInfoData[7].mYOrbitSpeed = 84.07f;
        gPlanetInfoData[7].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[7].mRotationSpeed = 0.8f;
        gPlanetInfoData[7].mTranslationMat = mat4::translation(vec3(70.0f, 0, 0));
        gPlanetInfoData[7].mScaleMat = mat4::scale(vec3(7));
        gPlanetInfoData[7].mColor = vec4(0.13f, 0.13f, 0.32f, 1.0f);
        gPlanetInfoData[7].mMorphingSpeed = 1;

        // Neptune
        gPlanetInfoData[8].mParentIndex = 0;
        gPlanetInfoData[8].mYOrbitSpeed = 164.81f;
        gPlanetInfoData[8].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[8].mRotationSpeed = 0.9f;
        gPlanetInfoData[8].mTranslationMat = mat4::translation(vec3(80.0f, 0, 0));
        gPlanetInfoData[8].mScaleMat = mat4::scale(vec3(8));
        gPlanetInfoData[8].mColor = vec4(0.21f, 0.028f, 0.79f, 1.0f);
        gPlanetInfoData[8].mMorphingSpeed = 1;

        // Pluto - Not a planet XDD
        gPlanetInfoData[9].mParentIndex = 0;
        gPlanetInfoData[9].mYOrbitSpeed = 247.7f;
        gPlanetInfoData[9].mZOrbitSpeed = 1.0f;
        gPlanetInfoData[9].mRotationSpeed = 7.0f;
        gPlanetInfoData[9].mTranslationMat = mat4::translation(vec3(90.0f, 0, 0));
        gPlanetInfoData[9].mScaleMat = mat4::scale(vec3(1.0f));
        gPlanetInfoData[9].mColor = vec4(0.45f, 0.21f, 0.21f, 1.0f);
        gPlanetInfoData[9].mMorphingSpeed = 1;

        // Moon
        gPlanetInfoData[10].mParentIndex = 3;
        gPlanetInfoData[10].mYOrbitSpeed = 1.0f;
        gPlanetInfoData[10].mZOrbitSpeed = 200.0f;
        gPlanetInfoData[10].mRotationSpeed = 27.0f;
        gPlanetInfoData[10].mTranslationMat = mat4::translation(vec3(5.0f, 0, 0));
        gPlanetInfoData[10].mScaleMat = mat4::scale(vec3(1));
        gPlanetInfoData[10].mColor = vec4(0.07f, 0.07f, 0.13f, 1.0f);
        gPlanetInfoData[10].mMorphingSpeed = 1;

        CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
        vec3                   camPos{ 48.0f, 48.0f, 20.0f };
        vec3                   lookAt{ vec3(0) };

        pCameraController = initFpsCameraController(camPos, lookAt);

        pCameraController->setMotionParameters(cmp);

        AddCustomInputBindings();
        initScreenshotCapturer(pRenderer, pGraphicsQueue, GetName());
        gFrameIndex = 0;

        return true;
    }

    void Exit()
    {
        exitScreenshotCapturer();

        exitCameraController(pCameraController);

        exitUserInterface();

        exitFontSystem();

        // Exit profile
        exitProfiler();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            // removeResource(pPlanetGroupBuffer[i]);
            removeGPURingBuffer(&gPlanetGroupBuffer[i]);
            removeResource(pUniformBuffer[i]);
            if (pRenderer->pGpu->mPipelineStatsQueries)
            {
                exitQueryPool(pRenderer, pPipelineStatsQueryPool[i]);
            }
        }

        removeResource(pSkyBoxVertexBuffer);

        for (uint32_t i = 0; i < 6; ++i)
            removeResource(pSkyBoxTextures[i]);

        removeSampler(pRenderer, pSkyBoxSampler);

        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(pWaitForPreviousCommandSemaphore); i++)
            exitSemaphore(pRenderer, pWaitForPreviousCommandSemaphore[i]);

        exitRootSignature(pRenderer);
        exitResourceLoaderInterface(pRenderer);

        exitQueue(pRenderer, pGraphicsQueue);

        exitRenderer(pRenderer);
        exitGPUConfiguration();
        pRenderer = NULL;

        bdestroy(&gPipelineStats);
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addDescriptorSets();
        }

        uint32_t cmdPerSubmit = gTotalPlanetCommands / gTotalSubmits;
        for (uint32_t i = 0; i < gTotalSubmits; i++)
        {
            gCmdDividers[i] = i == gTotalSubmits - 1 ? gTotalPlanetCommands : (i + 1) * cmdPerSubmit;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            // we only need to reload gui when the size of window changed
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent(GetName(), &guiDesc, &pGuiWindow);

            DropdownWidget ddTestScripts;
            ddTestScripts.pData = &gCurrentScriptIndex;
            ddTestScripts.pNames = gTestScripts;
            ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);

            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

            ButtonWidget bRunScript;
            UIWidget*    pRunScript = uiAddComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
            luaRegisterWidget(pRunScript);

            SliderUintWidget vertexLayoutWidget;
            vertexLayoutWidget.mMin = 0;
            vertexLayoutWidget.mMax = 1;
            vertexLayoutWidget.mStep = 1;
            vertexLayoutWidget.pData = &gSphereLayoutType;
            UIWidget* pVLw = uiAddComponentWidget(pGuiWindow, "Vertex Layout", &vertexLayoutWidget, WIDGET_TYPE_SLIDER_UINT);
            uiSetWidgetOnEditedCallback(pVLw, nullptr, reloadRequest);
            luaRegisterWidget(pVLw);

            // UI for tweaking max command per submit
            SliderUintWidget maxCommandPerSubmitWidget;
            maxCommandPerSubmitWidget.mMin = 1;
            maxCommandPerSubmitWidget.mMax = MAX_SUBMITS;
            maxCommandPerSubmitWidget.mStep = 1;
            maxCommandPerSubmitWidget.pData = &gTotalSubmits;
            UIWidget* pTSw = uiAddComponentWidget(pGuiWindow, "Total Submit Count", &maxCommandPerSubmitWidget, WIDGET_TYPE_SLIDER_UINT);
            uiSetWidgetOnEditedCallback(pTSw, nullptr, reloadRequest);
            luaRegisterWidget(pTSw);

            // UI for total commands number
            SliderUintWidget totalCommandWidget;
            totalCommandWidget.mMin = 0;
            totalCommandWidget.mMax = MAX_COMMANDS;
            totalCommandWidget.mStep = 50;
            totalCommandWidget.pData = &gTotalPlanetCommands;
            UIWidget* pTCw = uiAddComponentWidget(pGuiWindow, "Total Commands", &totalCommandWidget, WIDGET_TYPE_SLIDER_UINT);
            uiSetWidgetOnEditedCallback(pTCw, nullptr, reloadRequest);
            luaRegisterWidget(pTCw);

            if (pRenderer->pGpu->mPipelineStatsQueries)
            {
                static float4     color = { 1.0f, 1.0f, 1.0f, 1.0f };
                DynamicTextWidget statsWidget;
                statsWidget.pText = &gPipelineStats;
                statsWidget.pColor = &color;
                uiAddComponentWidget(pGuiWindow, "Pipeline Stats", &statsWidget, WIDGET_TYPE_DYNAMIC_TEXT);
            }

            if (!addSwapChain())
                return false;

            if (!addDepthBuffer())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            generate_complex_mesh();
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        uiLoad.mHeight = mSettings.mHeight;
        uiLoad.mWidth = mSettings.mWidth;
        uiLoad.mLoadType = pReloadDesc->mType;
        uiLoad.mVR2DLayer.mPosition = float3(gVR2DLayer.m2DLayerPosition.x, gVR2DLayer.m2DLayerPosition.y, gVR2DLayer.m2DLayerPosition.z);
        uiLoad.mVR2DLayer.mScale = gVR2DLayer.m2DLayerScale;
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
            removeResource(pSphereVertexBuffer);
            removeResource(pSphereIndexBuffer);
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
            removeRenderTarget(pRenderer, pDepthBuffer);
            uiRemoveComponent(pGuiWindow);
            unloadProfilerUI();
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeShaders();
        }
    }

    void Update(float deltaTime)
    {
        if (!uiIsFocused())
        {
            pCameraController->onMove({ inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) });
            pCameraController->onRotate({ inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) });
            pCameraController->onMoveY(inputGetValue(0, CUSTOM_MOVE_UP));
            if (inputGetValue(0, CUSTOM_RESET_VIEW))
            {
                pCameraController->resetView();
            }
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

        pCameraController->update(deltaTime);
        /************************************************************************/
        // Scene Update
        /************************************************************************/
        static float currentTime = 0.0f;
        currentTime += deltaTime * 1000.0f;

        // update camera with time
        mat4 viewMat = pCameraController->getViewMatrix().mCamera;

        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
        CameraMatrix mvp = projMat * viewMat;
        COMPILE_ASSERT(sizeof(mvp) == sizeof(gUniformData.mvp));
        memcpy((void*)&gUniformData.mvp, (void*)&mvp, sizeof(mvp));

        // point light parameters
        gUniformData.lightPosition = float4(0, 0, 0, 0);
        gUniformData.lightColor = float4(0.9f, 0.9f, 0.7f, 1.0f); // Pale Yellow

        // update planet transformations
        for (unsigned int i = 0; i < gNumPlanets; i++)
        {
            mat4 rotSelf, rotOrbitY, rotOrbitZ, trans, scale, parentMat;
            rotSelf = rotOrbitY = rotOrbitZ = parentMat = mat4::identity();
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

            scale[0][0] /= 2;
            scale[1][1] /= 2;
            scale[2][2] /= 2;

            gPlanetInfoData[i].mSharedMat = parentMat * rotOrbitY * trans;
            gUniformData.toWorld[i] = parentMat * rotOrbitY * rotOrbitZ * trans * rotSelf * scale;
            gUniformData.color[i] = v4ToF4(gPlanetInfoData[i].mColor);

            float step;
            float phase = modf(currentTime * gPlanetInfoData[i].mMorphingSpeed / 2000.f, &step);
            if (phase > 0.5f)
                phase = 2 - phase * 2;
            else
                phase = phase * 2;

            gUniformData.geometryWeight[i][0] = phase;
        }

        viewMat.setTranslation(vec3(0));
        mvp = projMat * viewMat;
        COMPILE_ASSERT(sizeof(mvp) == sizeof(gUniformData.skyMvp));
        memcpy((void*)&gUniformData.skyMvp, (void*)&mvp, sizeof(mvp));
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

        RenderTarget*     pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, MAX_SUBMITS);

        uint64_t         tick = cpuProfileEnter(gRecordingCpuToken);
        // Update uniform buffers
        BufferUpdateDesc viewProjCbv = { pUniformBuffer[gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        memcpy(viewProjCbv.pMappedData, &gUniformData, sizeof(gUniformData));
        endUpdateResource(&viewProjCbv);

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        if (pRenderer->pGpu->mPipelineStatsQueries)
        {
            bdestroy(&gPipelineStats);
            gPipelineStats = bempty();
            for (uint32_t i = 0; i < gTotalSubmits; i++)
            {
                QueryData data = {};
                getQueryData(pRenderer, pPipelineStatsQueryPool[gFrameIndex], i, &data);
                bformata(&gPipelineStats,
                         "\n"
                         "Pipeline Stats Submit %u:\n"
                         "    VS invocations:      %u\n"
                         "    PS invocations:      %u\n"
                         "    Clipper invocations: %u\n"
                         "    IA primitives:       %u\n"
                         "    Clipper primitives:  %u\n",
                         i, data.mPipelineStats.mVSInvocations, data.mPipelineStats.mPSInvocations, data.mPipelineStats.mCInvocations,
                         data.mPipelineStats.mIAPrimitives, data.mPipelineStats.mCPrimitives);
            }
        }

        for (uint32_t i = 0; i < gTotalSubmits; i++)
        {
            Cmd* cmd = elem.pCmds[i];
            beginCmd(cmd);
            cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

            if (i == 0)
            {
                // reset query for the first submit
                if (pRenderer->pGpu->mPipelineStatsQueries)
                {
                    cmdResetQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], 0, gTotalSubmits);
                }
                // set resource barrier
                RenderTargetBarrier barriers[] = {
                    { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
                };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
            }

            // begin query
            if (pRenderer->pGpu->mPipelineStatsQueries)
            {
                QueryDesc queryDesc = { i };
                cmdBeginQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
            }

            bstring text = bempty();
            bformat(&text, "Draw submit %u", i);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, (const char*)text.data);
            bdestroy(&text);

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pRenderTarget, i == 0 ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD };
            bindRenderTargets.mDepthStencil = { pDepthBuffer, i == 0 ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD };
            cmdBindRenderTargets(cmd, &bindRenderTargets);

            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

            if (i == 0)
            {
                const uint32_t skyboxVbStride = sizeof(float) * 4;
                // draw skybox
                cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Skybox");
                cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
                cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
                cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
                cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
                cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, &skyboxVbStride, NULL);
                cmdDraw(cmd, 36, 0);
                cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
            }

            // draw planets
            text = bempty();
            uint32_t start = i == 0 ? 0 : gCmdDividers[i - 1];
            uint32_t end = gCmdDividers[i] - 1;
            bformat(&text, "Draw planets: %u - %u", start, end);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, (const char*)text.data);
            bdestroy(&text);
            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
            cmdBindPipeline(cmd, pSpherePipeline);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
            cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer, &gSphereVertexLayout.mBindings[0].mStride, nullptr);
            cmdBindIndexBuffer(cmd, pSphereIndexBuffer, INDEX_TYPE_UINT16, 0);
            for (uint32_t j = start; j < gCmdDividers[i]; j++)
            {
                float           r = sqrtf(float(j) + 0.5f) / sqrtf(float(gTotalPlanetCommands)) * 50.f;
                float           theta = float(j) * GOLDEN_ANGLE + float(j);
                float           x = r * cosf(theta);
                float           z = r * sinf(theta);
                PlanetGroupData groupData = { { x, float(j) * 0.001f, z }, complicatedMath(j) };

                GPURingBufferOffset uniformBlock = getGPURingBufferOffset(&gPlanetGroupBuffer[gFrameIndex], sizeof(PlanetGroupData));
                BufferUpdateDesc    updateDesc = { uniformBlock.pBuffer, uniformBlock.mOffset };
                beginUpdateResource(&updateDesc);
                memcpy(updateDesc.pMappedData, &groupData, sizeof(PlanetGroupData));
                endUpdateResource(&updateDesc);

                DescriptorDataRange range = { (uint32_t)uniformBlock.mOffset, sizeof(PlanetGroupData) };
                DescriptorData      params[1] = {};
                params[0].mIndex = SRT_RES_IDX(SrtData, PerDraw, gPlanetGroupData);
                params[0].ppBuffers = &uniformBlock.pBuffer;
                params[0].pRanges = &range;
                updateDescriptorSet(pRenderer, j, pDescriptorSetPlanetGroupData, 1, params);
                cmdBindDescriptorSet(cmd, j, pDescriptorSetPlanetGroupData);

                cmdDrawIndexedInstanced(cmd, gSphereIndexCount, 0, gNumPlanets, 0, 0);
            }
            cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

            if (i == gTotalSubmits - 1) // draw UI
            {
                cmdBeginDrawingUserInterface(cmd, pSwapChain, pRenderTarget);
                text = bempty();
                bformat(&text, "Draw UI at submit %u", i);
                cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, (const char*)text.data);
                bdestroy(&text);

                gFrameTimeDraw.mFontColor = 0xff00ffff;
                gFrameTimeDraw.mFontSize = 18.0f;
                gFrameTimeDraw.mFontID = gFontID;
                float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(400.f, 15.f), &gFrameTimeDraw);
                cmdDrawGpuProfile(cmd, float2(400.f, txtSizePx.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

                cmdDrawUserInterface(cmd);
                cmdEndDrawingUserInterface(cmd, pSwapChain);
                cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

                RenderTargetBarrier barriers[] = {
                    { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT },
                };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
            }

            cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
            cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
            if (pRenderer->pGpu->mPipelineStatsQueries)
            {
                QueryDesc queryDesc = { i };
                cmdEndQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
                if (i == gTotalSubmits - 1)
                {
                    cmdResolveQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], 0, gTotalSubmits);
                }
            }

            endCmd(cmd);

            Semaphore* waitSemaphores[2] = {};
            if (i == 0)
            {
                FlushResourceUpdateDesc flushUpdateDesc = {};
                flushUpdateDesc.mNodeIndex = 0;
                flushResourceUpdates(&flushUpdateDesc);
                waitSemaphores[0] = flushUpdateDesc.pOutSubmittedSemaphore;
                waitSemaphores[1] = pImageAcquiredSemaphore;
            }
            else
                waitSemaphores[0] = pWaitForPreviousCommandSemaphore[i];

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.mSignalSemaphoreCount = 1;
            submitDesc.mWaitSemaphoreCount = i == 0 ? TF_ARRAY_COUNT(waitSemaphores) : 1;
            submitDesc.ppCmds = &cmd;
            submitDesc.ppSignalSemaphores = i == gTotalSubmits - 1 ? &elem.pSemaphore : &pWaitForPreviousCommandSemaphore[i + 1];
            submitDesc.ppWaitSemaphores = waitSemaphores;
            if (i == gTotalSubmits - 1)
                submitDesc.pSignalFence = elem.pFence;
            queueSubmit(pGraphicsQueue, &submitDesc);
        }

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.mSubmitDone = true;

        queuePresent(pGraphicsQueue, &presentDesc);
        cpuProfileLeave(gRecordingCpuToken, tick);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);

        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
        {
            uint64_t tick1 = cpuProfileEnter(gWaitForFenceCpuToken);
            waitForFences(pRenderer, 1, &elem.pFence);
            cpuProfileLeave(gWaitForFenceCpuToken, tick1);
        }
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() { return "04_QueueSubmitProfiling"; }

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
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_2D_VR_LAYER;
        swapChainDesc.mVR.m2DLayer = gVR2DLayer;

        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    bool addDepthBuffer()
    {
        // Add depth buffer
        ESRAM_BEGIN_ALLOC(pRenderer, "Depth", 0);

        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue.depth = 0.0f;
        depthRT.mClearValue.stencil = 0;
        depthRT.mDepth = 1;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = gTotalSubmits > 1 ? TEXTURE_CREATION_FLAG_NONE : TEXTURE_CREATION_FLAG_ON_TILE;
        depthRT.mFlags |= TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        ESRAM_END_ALLOC(pRenderer);

        return pDepthBuffer != NULL;
    }

    void addDescriptorSets()
    {
        const DescriptorSetDesc desc = SRT_SET_DESC(SrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetTexture);
        const DescriptorSetDesc uniformDesc = SRT_SET_DESC(SrtData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &uniformDesc, &pDescriptorSetUniforms);
        const DescriptorSetDesc planetGroupDataDesc = SRT_SET_DESC(SrtData, PerDraw, MAX_COMMANDS, 0);
        addDescriptorSet(pRenderer, &planetGroupDataDesc, &pDescriptorSetPlanetGroupData);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
        removeDescriptorSet(pRenderer, pDescriptorSetTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetPlanetGroupData);
    }

    void addShaders()
    {
        ShaderLoadDesc skyShader = {};
        skyShader.mVert.pFileName = "skybox.vert";
        skyShader.mFrag.pFileName = "skybox.frag";

        ShaderLoadDesc basicShader = {};
        basicShader.mVert.pFileName = "basic.vert";
        basicShader.mFrag.pFileName = "basic.frag";

        addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
        addShader(pRenderer, &basicShader, &pSphereShader);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pSphereShader);
        removeShader(pRenderer, pSkyBoxDrawShader);
    }

    void addPipelines()
    {
        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        RasterizerStateDesc sphereRasterizerStateDesc = {};
        sphereRasterizerStateDesc.mCullMode = CULL_MODE_FRONT;

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;

        PipelineDesc desc = {};
        PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(SrtData, Persistent), NULL, SRT_LAYOUT_DESC(SrtData, PerBatch),
                             SRT_LAYOUT_DESC(SrtData, PerDraw));
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pShaderProgram = pSphereShader;
        pipelineSettings.pVertexLayout = &gSphereVertexLayout;
        pipelineSettings.pRasterizerState = &sphereRasterizerStateDesc;
        addPipeline(pRenderer, &desc, &pSpherePipeline);

        // layout and pipeline for skybox draw
        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mBindings[0].mStride = sizeof(float4);
        vertexLayout.mAttribCount = 1;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        pipelineSettings.pVertexLayout = &vertexLayout;

        pipelineSettings.pDepthState = NULL;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.pShaderProgram = pSkyBoxDrawShader; //-V519
        addPipeline(pRenderer, &desc, &pSkyBoxDrawPipeline);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pSkyBoxDrawPipeline);
        removePipeline(pRenderer, pSpherePipeline);
    }

    void prepareDescriptorSets()
    {
        // Prepare descriptor sets
        DescriptorData params[7] = {};
        params[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gRightTexture);
        params[0].ppTextures = &pSkyBoxTextures[0];
        params[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gLeftTexture);
        params[1].ppTextures = &pSkyBoxTextures[1];
        params[2].mIndex = SRT_RES_IDX(SrtData, Persistent, gTopTexture);
        params[2].ppTextures = &pSkyBoxTextures[2];
        params[3].mIndex = SRT_RES_IDX(SrtData, Persistent, gBotTexture);
        params[3].ppTextures = &pSkyBoxTextures[3];
        params[4].mIndex = SRT_RES_IDX(SrtData, Persistent, gFrontTexture);
        params[4].ppTextures = &pSkyBoxTextures[4];
        params[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gBackTexture);
        params[5].ppTextures = &pSkyBoxTextures[5];
        params[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gSampler);
        params[6].ppSamplers = &pSkyBoxSampler;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 7, params);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData uParams[1] = {};
            uParams[0].mIndex = SRT_RES_IDX(SrtData, PerBatch, gUniformBlock);
            uParams[0].ppBuffers = &pUniformBuffer[i];

            updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, uParams);
        }
    }

    float complicatedMath(uint32_t n)
    {
        float res = 0.f;
        for (uint32_t i = 0; i < n; i++)
        {
            res += sinf(float(i)) * float(i);
        }
        return res;
    }
};
DEFINE_APPLICATION_MAIN(QueueSubmitProfiling)
