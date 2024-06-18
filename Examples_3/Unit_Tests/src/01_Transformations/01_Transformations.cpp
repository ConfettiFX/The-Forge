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

// Unit Test for testing transformations using a solar system.
// Tests the basic mat4 transformations, such as scaling, rotation, and translation.

#define MAX_PLANETS 20 // Does not affect test, just for allocating space in uniform block. Must match with shader.

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
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

/// Demo structures
struct PlanetInfoStruct
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
};

struct UniformBlock
{
    CameraMatrix mProjectView;
    mat4         mToWorldMat[MAX_PLANETS];
    vec4         mColor[MAX_PLANETS];
    float        mGeometryWeight[MAX_PLANETS][4];

    // Point Light Information
    vec3 mLightPosition;
    vec3 mLightColor;
};

struct UniformBlockSky
{
    CameraMatrix mProjectView;
};

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
RootSignature* pRootSignature = NULL;
Sampler*       pSamplerSkyBox = NULL;
Texture*       pSkyBoxTextures[6];
DescriptorSet* pDescriptorSetTexture = { NULL };
DescriptorSet* pDescriptorSetUniforms = { NULL };

Buffer* pProjViewUniformBuffer[gDataBufferCount] = { NULL };
Buffer* pSkyboxUniformBuffer[gDataBufferCount] = { NULL };

uint32_t     gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

int              gNumberOfSpherePoints;
UniformBlock     gUniformData;
UniformBlockSky  gUniformDataSky;
PlanetInfoStruct gPlanetInfoData[gNumPlanets];

ICameraController* pCameraController = NULL;

UIComponent* pGuiWindow = NULL;

uint32_t gFontID = 0;

QueryPool* pPipelineStatsQueryPool[gDataBufferCount] = {};

const char* pSkyBoxImageFileNames[] = { "Skybox_right1.tex",  "Skybox_left2.tex",  "Skybox_top3.tex",
                                        "Skybox_bottom4.tex", "Skybox_front5.tex", "Skybox_back6.tex" };

FontDrawDesc gFrameTimeDraw;

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

const char* gWindowTestScripts[] = { "TestFullScreen.lua", "TestCenteredWindow.lua", "TestNonCenteredWindow.lua", "TestBorderless.lua" };

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
#define DETAIL_LEVEL 64

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

class Transformations: public IApp
{
public:
    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        // window and renderer setup
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.mD3D11Supported = true;
        settings.mGLESSupported = true;
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
        {
            QueryPoolDesc poolDesc = {};
            poolDesc.mQueryCount = 3; // The count is 3 due to quest & multi-view use otherwise 2 is enough as we use 2 queries.
            poolDesc.mType = QUERY_TYPE_PIPELINE_STATISTICS;
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                addQueryPool(pRenderer, &poolDesc, &pPipelineStatsQueryPool[i]);
            }
        }

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

        // Dynamic sampler that is bound at runtime
        SamplerDesc samplerDesc = { FILTER_LINEAR,
                                    FILTER_LINEAR,
                                    MIPMAP_MODE_NEAREST,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE,
                                    ADDRESS_MODE_CLAMP_TO_EDGE };
        addSampler(pRenderer, &samplerDesc, &pSamplerSkyBox);

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
            ubDesc.mDesc.pName = "ProjViewUniformBuffer";
            ubDesc.mDesc.mSize = sizeof(UniformBlock);
            ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
            addResource(&ubDesc, NULL);
            ubDesc.mDesc.pName = "SkyboxUniformBuffer";
            ubDesc.mDesc.mSize = sizeof(UniformBlockSky);
            ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
            addResource(&ubDesc, NULL);
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
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        // Gpu profiler can only be added after initProfile.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        /************************************************************************/
        // GUI
        /************************************************************************/
        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
        uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

        SliderUintWidget vertexLayoutWidget;
        vertexLayoutWidget.mMin = 0;
        vertexLayoutWidget.mMax = 1;
        vertexLayoutWidget.mStep = 1;
        vertexLayoutWidget.pData = &gSphereLayoutType;
        UIWidget* pVLw = uiCreateComponentWidget(pGuiWindow, "Vertex Layout", &vertexLayoutWidget, WIDGET_TYPE_SLIDER_UINT);
        uiSetWidgetOnEditedCallback(pVLw, nullptr, reloadRequest);

        if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
        {
            static float4     color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DynamicTextWidget statsWidget;
            statsWidget.pText = &gPipelineStats;
            statsWidget.pColor = &color;
            uiCreateComponentWidget(pGuiWindow, "Pipeline Stats", &statsWidget, WIDGET_TYPE_DYNAMIC_TEXT);
        }

        const uint32_t numScripts = TF_ARRAY_COUNT(gWindowTestScripts);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gWindowTestScripts[i];
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

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = "circlepad.tex";
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
        InputActionCallback onAnyInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }

            return true;
        };

        typedef bool (*CameraInputHandler)(InputActionContext * ctx, DefaultInputActions::DefaultInputAction action);
        static CameraInputHandler onCameraInput = [](InputActionContext* ctx, DefaultInputActions::DefaultInputAction action)
        {
            if (*(ctx->pCaptured))
            {
                float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
                switch (action)
                {
                case DefaultInputActions::ROTATE_CAMERA:
                    pCameraController->onRotate(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA:
                    pCameraController->onMove(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                    pCameraController->onMoveY(delta[0]);
                    break;
                default:
                    break;
                }
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
        actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::ROTATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA_VERTICAL); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           if (!uiWantTextInput())
                               pCameraController->resetView();
                           return true;
                       } };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onAnyInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameIndex = 0;

        return true;
    }

    void Exit()
    {
        exitInputSystem();

        exitCameraController(pCameraController);

        exitUserInterface();

        exitFontSystem();

        // Exit profile
        exitProfiler();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pProjViewUniformBuffer[i]);
            removeResource(pSkyboxUniformBuffer[i]);
            if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
            {
                removeQueryPool(pRenderer, pPipelineStatsQueryPool[i]);
            }
        }

        removeResource(pSkyBoxVertexBuffer);

        for (uint i = 0; i < 6; ++i)
            removeResource(pSkyBoxTextures[i]);

        removeSampler(pRenderer, pSamplerSkyBox);

        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        removeSemaphore(pRenderer, pImageAcquiredSemaphore);

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
            removeResource(pSphereVertexBuffer);
            removeResource(pSphereIndexBuffer);
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
            removeRenderTarget(pRenderer, pDepthBuffer);
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

        pCameraController->update(deltaTime);
        /************************************************************************/
        // Scene Update
        /************************************************************************/
        static float currentTime = 0.0f;
        currentTime += deltaTime * 1000.0f;

        // update camera with time
        mat4 viewMat = pCameraController->getViewMatrix();

        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
        gUniformData.mProjectView = projMat * viewMat;

        // point light parameters
        gUniformData.mLightPosition = vec3(0, 0, 0);
        gUniformData.mLightColor = vec3(0.9f, 0.9f, 0.7f); // Pale Yellow

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
            gUniformData.mToWorldMat[i] = parentMat * rotOrbitY * rotOrbitZ * trans * rotSelf * scale;
            gUniformData.mColor[i] = gPlanetInfoData[i].mColor;

            float step;
            float phase = modf(currentTime * gPlanetInfoData[i].mMorphingSpeed / 2000.f, &step);
            if (phase > 0.5f)
                phase = 2 - phase * 2;
            else
                phase = phase * 2;

            gUniformData.mGeometryWeight[i][0] = phase;
        }

        viewMat.setTranslation(vec3(0));
        gUniformDataSky = {};
        gUniformDataSky.mProjectView = projMat * viewMat;
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
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        // Update uniform buffers
        BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        memcpy(viewProjCbv.pMappedData, &gUniformData, sizeof(gUniformData));
        endUpdateResource(&viewProjCbv);

        BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex] };
        beginUpdateResource(&skyboxViewProjCbv);
        memcpy(skyboxViewProjCbv.pMappedData, &gUniformDataSky, sizeof(gUniformDataSky));
        endUpdateResource(&skyboxViewProjCbv);

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
        {
            QueryData data3D = {};
            QueryData data2D = {};
            getQueryData(pRenderer, pPipelineStatsQueryPool[gFrameIndex], 0, &data3D);
            getQueryData(pRenderer, pPipelineStatsQueryPool[gFrameIndex], 1, &data2D);
            bformat(&gPipelineStats,
                    "\n"
                    "Pipeline Stats 3D:\n"
                    "    VS invocations:      %u\n"
                    "    PS invocations:      %u\n"
                    "    Clipper invocations: %u\n"
                    "    IA primitives:       %u\n"
                    "    Clipper primitives:  %u\n"
                    "\n"
                    "Pipeline Stats 2D UI:\n"
                    "    VS invocations:      %u\n"
                    "    PS invocations:      %u\n"
                    "    Clipper invocations: %u\n"
                    "    IA primitives:       %u\n"
                    "    Clipper primitives:  %u\n",
                    data3D.mPipelineStats.mVSInvocations, data3D.mPipelineStats.mPSInvocations, data3D.mPipelineStats.mCInvocations,
                    data3D.mPipelineStats.mIAPrimitives, data3D.mPipelineStats.mCPrimitives, data2D.mPipelineStats.mVSInvocations,
                    data2D.mPipelineStats.mPSInvocations, data2D.mPipelineStats.mCInvocations, data2D.mPipelineStats.mIAPrimitives,
                    data2D.mPipelineStats.mCPrimitives);
        }

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);
        if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
        {
            cmdResetQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], 0, 2);
            QueryDesc queryDesc = { 0 };
            cmdBeginQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
        }

        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Skybox/Planets");

        // simply record the screen cleaning command
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        const uint32_t skyboxVbStride = sizeof(float) * 4;
        // draw skybox
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Skybox");
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
        cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
        cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 0, pDescriptorSetUniforms);
        cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, &skyboxVbStride, NULL);
        cmdDraw(cmd, 36, 0);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Planets");

        cmdBindPipeline(cmd, pSpherePipeline);
        cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 1, pDescriptorSetUniforms);
        cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer, &gSphereVertexLayout.mBindings[0].mStride, nullptr);
        cmdBindIndexBuffer(cmd, pSphereIndexBuffer, INDEX_TYPE_UINT16, 0);

        cmdDrawIndexedInstanced(cmd, gSphereIndexCount, 0, gNumPlanets, 0, 0);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken); // Draw Skybox/Planets
        cmdBindRenderTargets(cmd, NULL);

        if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
        {
            QueryDesc queryDesc = { 0 };
            cmdEndQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);

            queryDesc = { 1 };
            cmdBeginQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
        }

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_LOAD };
        bindRenderTargets.mDepthStencil = { NULL, LOAD_ACTION_DONTCARE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(8.f, 15.f), &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(8.f, txtSizePx.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

        cmdDrawUserInterface(cmd);

        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
        cmdBindRenderTargets(cmd, NULL);

        barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        cmdEndGpuFrameProfile(cmd, gGpuProfileToken);

        if (pRenderer->pGpu->mSettings.mPipelineStatsQueries)
        {
            QueryDesc queryDesc = { 1 };
            cmdEndQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], &queryDesc);
            cmdResolveQuery(cmd, pPipelineStatsQueryPool[gFrameIndex], 0, 2);
        }

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
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.mSubmitDone = true;

        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() { return "01_Transformations"; }

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
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
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
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        return pDepthBuffer != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetTexture);
        desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 2 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetUniforms);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
    }

    void addRootSignatures()
    {
        Shader*  shaders[2];
        uint32_t shadersCount = 0;
        shaders[shadersCount++] = pSphereShader;
        shaders[shadersCount++] = pSkyBoxDrawShader;

        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = shadersCount;
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);
    }

    void removeRootSignatures() { removeRootSignature(pRenderer, pRootSignature); }

    void addShaders()
    {
        ShaderLoadDesc skyShader = {};
        skyShader.mStages[0].pFileName = "skybox.vert";
        skyShader.mStages[1].pFileName = "skybox.frag";

        ShaderLoadDesc basicShader = {};
        basicShader.mStages[0].pFileName = "basic.vert";
        basicShader.mStages[1].pFileName = "basic.frag";

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
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pSphereShader;
        pipelineSettings.pVertexLayout = &gSphereVertexLayout;
        pipelineSettings.pRasterizerState = &sphereRasterizerStateDesc;
        pipelineSettings.mVRFoveatedRendering = true;
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
        params[6].pName = "uSampler0";
        params[6].ppSamplers = &pSamplerSkyBox;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 7, params);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData uParams[1] = {};
            uParams[0].pName = "uniformBlock";
            uParams[0].ppBuffers = &pSkyboxUniformBuffer[i];
            updateDescriptorSet(pRenderer, i * 2 + 0, pDescriptorSetUniforms, 1, uParams);

            uParams[0].pName = "uniformBlock";
            uParams[0].ppBuffers = &pProjViewUniformBuffer[i];
            updateDescriptorSet(pRenderer, i * 2 + 1, pDescriptorSetUniforms, 1, uParams);
        }
    }
};
DEFINE_APPLICATION_MAIN(Transformations)
