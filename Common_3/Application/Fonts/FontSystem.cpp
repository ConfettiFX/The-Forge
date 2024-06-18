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

#include "../../Application/Interfaces/IFont.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Graphics/GraphicsConfig.h"

// include Fontstash (should be after MemoryTracking so that it also detects memory free/remove in fontstash)
#define FONTSTASH_IMPLEMENTATION
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../ThirdParty/OpenSource/Fontstash/src/fontstash.h"

#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../Utilities/RingBuffer.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifdef ENABLE_FORGE_FONTS

struct Fontstash
{
    // FONS
    FONScontext* pContext;
    // stb_ds dynamic arrays
    void**       mFontBuffers;
    uint32_t*    mFontBufferSizes;
    float        mFontMaxSize;
    uint32_t     mWidth;
    uint32_t     mHeight;

    // Renderer
    Renderer*      pRenderer;
    Texture*       pAtlasTexture;
    Shader*        pShaders[2];
    RootSignature* pRootSignature;
    DescriptorSet* pDescriptorSets;
    Pipeline*      pPipelines[2];
    Sampler*       pDefaultSampler;
    GPURingBuffer  mUniformRingBuffer;
    GPURingBuffer  mMeshRingBuffer;
    uint32_t       mRootConstantIndex;

    // Fontstash generation
    const uint8_t* pPixels;
    bool           mUpdateTexture;

    // Render size
    float2 mScaleBias;
    float2 mDpiScale;
    float  mDpiScaleMin;

    bool mRenderInitialized;

#if defined(TARGET_IOS) || defined(ANDROID)
    static const int TextureAtlasDimension = 512;
#elif defined(XBOX)
    static const int TextureAtlasDimension = 1024;
#else // PC / LINUX / MAC
    static const int TextureAtlasDimension = 2048;
#endif
};

struct FontstashDrawData
{
    CameraMatrix mProjView;
    mat4         mWorldMat;
    Cmd*         pCmd;
    bool         mText3D;
};

static Fontstash gFontstash = {};

// --  FONS renderer implementation --
static int fonsImplementationGenerateTexture(void* userPtr, int width, int height)
{
    UNREF_PARAM(userPtr);
    gFontstash.mWidth = width;
    gFontstash.mHeight = height;
    gFontstash.mUpdateTexture = true;
    return 1;
}

static void fonsImplementationModifyTexture(void* userPtr, int* rect, const unsigned char* data)
{
    UNREF_PARAM(userPtr);
    UNREF_PARAM(rect);
    gFontstash.pPixels = data;
    gFontstash.mUpdateTexture = true;
}

static void fonsImplementationRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
    if (!gFontstash.pAtlasTexture)
    {
        return;
    }

    FontstashDrawData* draw = (FontstashDrawData*)userPtr;
    Cmd*               pCmd = draw->pCmd;

    if (gFontstash.mUpdateTexture)
    {
        // #TODO: Investigate - Causes hang on low-mid end Android phones (tested on Samsung Galaxy A50s)
#ifndef __ANDROID__
        waitQueueIdle(pCmd->pQueue);
#endif
        TextureUpdateDesc updateDesc = { gFontstash.pAtlasTexture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        beginUpdateResource(&updateDesc);
        TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
        for (uint32_t r = 0; r < subresource.mRowCount; ++r)
        {
            memcpy(subresource.pMappedData + r * subresource.mDstRowStride, gFontstash.pPixels + r * subresource.mSrcRowStride,
                   subresource.mSrcRowStride);
        }
        endUpdateResource(&updateDesc);

        gFontstash.mUpdateTexture = false;
    }

    GPURingBufferOffset buffer = getGPURingBufferOffset(&gFontstash.mMeshRingBuffer, nverts * sizeof(float4));
    BufferUpdateDesc    update = { buffer.pBuffer, buffer.mOffset };
    beginUpdateResource(&update);
    float4* vtx = (float4*)update.pMappedData;
    // build vertices
    for (int impl = 0; impl < nverts; impl++)
    {
        float4 vert = { verts[impl * 2 + 0], verts[impl * 2 + 1], tcoords[impl * 2 + 0], tcoords[impl * 2 + 1] };
        memcpy((void*)&vtx[impl], &vert, sizeof(vert));
    }
    endUpdateResource(&update);

    // extract color
    float4 color = unpackA8B8G8R8_SRGB(*colors);

    uint32_t  pipelineIndex = draw->mText3D ? 1 : 0;
    Pipeline* pPipeline = gFontstash.pPipelines[pipelineIndex];
    ASSERT(pPipeline);

    cmdBindPipeline(pCmd, pPipeline);

    struct UniformData
    {
        float4 color;
        float2 scaleBias;
#ifdef METAL
        float _pad0;
        float _pad1;
#endif
    } data;

    data.color = color;
    data.scaleBias = gFontstash.mScaleBias;

    if (draw->mText3D)
    {
        CameraMatrix mvp = (draw->mProjView * draw->mWorldMat);
        data.color = color;
        data.scaleBias.x = -data.scaleBias.x;

        GPURingBufferOffset uniformBlock = getGPURingBufferOffset(&gFontstash.mUniformRingBuffer, sizeof(mvp));
        BufferUpdateDesc    updateDesc = { uniformBlock.pBuffer, uniformBlock.mOffset };
        beginUpdateResource(&updateDesc);
        memcpy(updateDesc.pMappedData, &mvp, sizeof(mvp));
        endUpdateResource(&updateDesc);

        const uint32_t size = sizeof(mvp);
        const uint32_t stride = sizeof(float4);

        DescriptorDataRange range = { (uint32_t)uniformBlock.mOffset, size };
        DescriptorData      params[1] = {};
        params[0].pName = "uniformBlock_rootcbv";
        params[0].ppBuffers = &uniformBlock.pBuffer;
        params[0].pRanges = &range;
        cmdBindDescriptorSetWithRootCbvs(pCmd, 0, gFontstash.pDescriptorSets, 1, params);
        cmdBindPushConstants(pCmd, gFontstash.pRootSignature, gFontstash.mRootConstantIndex, &data);
        cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.mOffset);
        cmdDraw(pCmd, nverts, 0);
    }
    else
    {
        const uint32_t stride = sizeof(float4);
        cmdBindDescriptorSet(pCmd, 0, gFontstash.pDescriptorSets);
        cmdBindPushConstants(pCmd, gFontstash.pRootSignature, gFontstash.mRootConstantIndex, &data);
        cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.mOffset);
        cmdDraw(pCmd, nverts, 0);
    }
}

void fonsImplementationRemoveTexture(void*) {}
#endif

bool platformInitFontSystem()
{
#ifdef ENABLE_FORGE_FONTS
    float          dpiScale[2] = {};
    const uint32_t monitorIdx = getActiveMonitorIdx();
    getMonitorDpiScale(monitorIdx, dpiScale);
    gFontstash.mDpiScale.x = dpiScale[0];
    gFontstash.mDpiScale.y = dpiScale[1];

    gFontstash.mDpiScaleMin = min(gFontstash.mDpiScale.x, gFontstash.mDpiScale.y);

    gFontstash.mWidth = gFontstash.TextureAtlasDimension * (int)ceilf(gFontstash.mDpiScale.x);
    gFontstash.mHeight = gFontstash.TextureAtlasDimension * (int)ceilf(gFontstash.mDpiScale.y);
    gFontstash.mFontMaxSize = min(gFontstash.mWidth, gFontstash.mHeight) / 10.0f; // see fontstash.h, line 1271, for fontSize calculation

    // create FONS context
    FONSparams params = {};
    params.width = gFontstash.mWidth;
    params.height = gFontstash.mHeight;
    params.flags = (unsigned char)FONS_ZERO_TOPLEFT;
    params.renderCreate = fonsImplementationGenerateTexture;
    params.renderUpdate = fonsImplementationModifyTexture;
    params.renderDelete = fonsImplementationRemoveTexture;
    params.renderDraw = fonsImplementationRenderText;
    gFontstash.pContext = fonsCreateInternal(&params);

    return gFontstash.pContext != NULL;
#else
    return true;
#endif
}

void platformExitFontSystem()
{
#ifdef ENABLE_FORGE_FONTS
    // unload font buffers
    for (ptrdiff_t i = 0; i < arrlen(gFontstash.mFontBuffers); ++i)
    {
        tf_free(gFontstash.mFontBuffers[i]);
    }
    arrfree(gFontstash.mFontBuffers);
    // unload font buffer sizes
    arrfree(gFontstash.mFontBufferSizes);

    // unload fontstash context
    fonsDeleteInternal(gFontstash.pContext);
    gFontstash = {};
#endif
}

bool initFontSystem(FontSystemDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(!gFontstash.mRenderInitialized);

    gFontstash.pRenderer = pDesc->pRenderer;

    // create image
    TextureDesc desc = {};
    desc.mArraySize = 1;
    desc.mDepth = 1;
    desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    desc.mFormat = TinyImageFormat_R8_UNORM;
    desc.mHeight = gFontstash.mHeight;
    desc.mMipLevels = 1;
    desc.mSampleCount = SAMPLE_COUNT_1;
    desc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    desc.mWidth = gFontstash.mWidth;
    desc.pName = "Fontstash Texture";
    TextureLoadDesc loadDesc = {};
    loadDesc.ppTexture = &gFontstash.pAtlasTexture;
    loadDesc.pDesc = &desc;
    addResource(&loadDesc, NULL);

    /************************************************************************/
    // Rendering resources
    /************************************************************************/
    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(gFontstash.pRenderer, &samplerDesc, &gFontstash.pDefaultSampler);

    addUniformGPURingBuffer(gFontstash.pRenderer, 65536, &gFontstash.mUniformRingBuffer, true);

    BufferDesc vbDesc = {};
    vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mSize = pDesc->mFontstashRingSizeBytes;
    vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    addGPURingBuffer(gFontstash.pRenderer, &vbDesc, &gFontstash.mMeshRingBuffer);
    /************************************************************************/
    /************************************************************************/
    gFontstash.mRenderInitialized = true;
#endif
    return true;
}

void exitFontSystem()
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.mRenderInitialized);

    removeResource(gFontstash.pAtlasTexture);

    removeGPURingBuffer(&gFontstash.mMeshRingBuffer);
    removeGPURingBuffer(&gFontstash.mUniformRingBuffer);
    removeSampler(gFontstash.pRenderer, gFontstash.pDefaultSampler);

    gFontstash.mRenderInitialized = false;
#endif
}

void loadFontSystem(const FontSystemLoadDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    if (pDesc->mLoadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        if (pDesc->mLoadType & RELOAD_TYPE_SHADER)
        {
            ShaderLoadDesc text2DShaderDesc = {};
            text2DShaderDesc.mStages[0] = { "fontstash2D.vert" };
            text2DShaderDesc.mStages[1] = { "fontstash.frag" };
            ShaderLoadDesc text3DShaderDesc = {};
            text3DShaderDesc.mStages[0] = { "fontstash3D.vert" };
            text3DShaderDesc.mStages[1] = { "fontstash.frag" };

            addShader(gFontstash.pRenderer, &text2DShaderDesc, &gFontstash.pShaders[0]);
            addShader(gFontstash.pRenderer, &text3DShaderDesc, &gFontstash.pShaders[1]);

            RootSignatureDesc textureRootDesc = { gFontstash.pShaders, 2 };
            const char*       pStaticSamplers[] = { "uSampler0" };
            textureRootDesc.mStaticSamplerCount = 1;
            textureRootDesc.ppStaticSamplerNames = pStaticSamplers;
            textureRootDesc.ppStaticSamplers = &gFontstash.pDefaultSampler;
            addRootSignature(gFontstash.pRenderer, &textureRootDesc, &gFontstash.pRootSignature);
            gFontstash.mRootConstantIndex = getDescriptorIndexFromName(gFontstash.pRootSignature, "uRootConstants");

            DescriptorSetDesc setDesc = { gFontstash.pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(gFontstash.pRenderer, &setDesc, &gFontstash.pDescriptorSets);
            DescriptorData setParams[1] = {};
            setParams[0].pName = "uTex0";
            setParams[0].ppTextures = &gFontstash.pAtlasTexture;
            updateDescriptorSet(gFontstash.pRenderer, 0, gFontstash.pDescriptorSets, 1, setParams);
        }

        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;

        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(vertexLayout.mAttribs[0].mFormat) / 8;

        BlendStateDesc blendStateDesc = {};
        blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
        blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
        blendStateDesc.mIndependentBlend = false;

        DepthStateDesc depthStateDesc[2] = {};
        depthStateDesc[0].mDepthTest = false;
        depthStateDesc[0].mDepthWrite = false;

        depthStateDesc[1].mDepthTest = true;
        depthStateDesc[1].mDepthWrite = true;
        depthStateDesc[1].mDepthFunc = (CompareMode)pDesc->mDepthCompareMode;

        RasterizerStateDesc rasterizerStateDesc[2] = {};
        rasterizerStateDesc[0].mCullMode = CULL_MODE_NONE;
        rasterizerStateDesc[0].mScissor = true;

        rasterizerStateDesc[1].mCullMode = (CullMode)pDesc->mCullMode;
        rasterizerStateDesc[1].mScissor = true;

        PipelineDesc pipelineDesc = {};
        pipelineDesc.pCache = pDesc->pCache;
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        pipelineDesc.mGraphicsDesc.mVRFoveatedRendering = true;
        pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
        pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
        pipelineDesc.mGraphicsDesc.pBlendState = &blendStateDesc;
        pipelineDesc.mGraphicsDesc.pRootSignature = gFontstash.pRootSignature;
        pipelineDesc.mGraphicsDesc.pVertexLayout = &vertexLayout;
        pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
        pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
        pipelineDesc.mGraphicsDesc.mSampleQuality = 0;
        pipelineDesc.mGraphicsDesc.pColorFormats = (TinyImageFormat*)&pDesc->mColorFormat;

        uint32_t count = pDesc->mDepthFormat == TinyImageFormat_UNDEFINED ? 1 : 2;
        for (uint32_t i = 0; i < count; ++i)
        {
            pipelineDesc.mGraphicsDesc.mDepthStencilFormat = (i > 0) ? (TinyImageFormat)pDesc->mDepthFormat : TinyImageFormat_UNDEFINED;
            pipelineDesc.mGraphicsDesc.pShaderProgram = gFontstash.pShaders[i];
            pipelineDesc.mGraphicsDesc.pDepthState = &depthStateDesc[i];
            pipelineDesc.mGraphicsDesc.pRasterizerState = &rasterizerStateDesc[i];
            addPipeline(gFontstash.pRenderer, &pipelineDesc, &gFontstash.pPipelines[i]);
        }
    }

    if (pDesc->mLoadType & RELOAD_TYPE_RESIZE)
    {
        gFontstash.mScaleBias = { 2.0f / (float)pDesc->mWidth, -2.0f / (float)pDesc->mHeight };
    }

#endif
}

void unloadFontSystem(ReloadType unloadType)
{
#ifdef ENABLE_FORGE_FONTS
    if (unloadType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
    {
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(gFontstash.pPipelines); ++i)
        {
            if (gFontstash.pPipelines[i])
            {
                removePipeline(gFontstash.pRenderer, gFontstash.pPipelines[i]);
                gFontstash.pPipelines[i] = NULL;
            }
        }

        if (unloadType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSet(gFontstash.pRenderer, gFontstash.pDescriptorSets);
            removeRootSignature(gFontstash.pRenderer, gFontstash.pRootSignature);

            for (uint32_t i = 0; i < 2; ++i)
            {
                removeShader(gFontstash.pRenderer, gFontstash.pShaders[i]);
            }
        }
    }
#endif
}

void cmdDrawTextWithFont(Cmd* pCmd, float2 screenCoordsInPx, const FontDrawDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.mRenderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    ASSERT(pDesc);
    ASSERT(pDesc->pText);

    const char* message = pDesc->pText;
    float       x = screenCoordsInPx.getX();
    float       y = screenCoordsInPx.getY();
    int         fontID = pDesc->mFontID;
    unsigned    color = pDesc->mFontColor;
    float       size = pDesc->mFontSize;
    float       spacing = pDesc->mFontSpacing;
    float       blur = pDesc->mFontBlur;

    FontstashDrawData draw = {};
    draw.mText3D = false;
    draw.pCmd = pCmd;
    // clamp the font size to max size.
    // Precomputed font texture puts limitation to the maximum size.
    size = min(size, gFontstash.mFontMaxSize);

    FONScontext* fs = gFontstash.pContext;
    fs->params.userPtr = &draw; // -V506 (draw only used inside this function)
    fonsSetSize(fs, size * gFontstash.mDpiScaleMin);
    fonsSetFont(fs, fontID);
    fonsSetColor(fs, color);
    fonsSetSpacing(fs, spacing * gFontstash.mDpiScaleMin);
    fonsSetBlur(fs, blur);
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    // considering the retina scaling:
    // the render target is already scaled up (w/ retina) and the (x,y) position given to this function
    // is expected to be in the render target's area. Hence, we don't scale up the position again.
    fonsDrawText(fs, x /** gFontstash.mDpiScale.x*/, y /** gFontstash.mDpiScale.y*/, message, NULL);
#endif
}

void cmdDrawWorldSpaceTextWithFont(Cmd* pCmd, const mat4* pMatWorld, const CameraMatrix* pMatProjView, const FontDrawDesc* pDesc)
{
#ifdef ENABLE_FORGE_FONTS
    // ASSERT(pFontStash);
    ASSERT(gFontstash.mRenderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    ASSERT(pDesc);
    ASSERT(pDesc->pText);
    ASSERT(pMatWorld);
    ASSERT(pMatProjView);

    const char*         message = pDesc->pText;
    const mat4&         worldMat = *pMatWorld;
    const CameraMatrix& projView = *pMatProjView;
    int                 fontID = pDesc->mFontID;
    unsigned            color = pDesc->mFontColor;
    float               size = pDesc->mFontSize;
    float               spacing = pDesc->mFontSpacing;
    float               blur = pDesc->mFontBlur;

    FontstashDrawData draw = {};
    draw.mText3D = true;
    draw.mProjView = projView;
    draw.mWorldMat = worldMat;
    draw.pCmd = pCmd;
    // clamp the font size to max size.
    // Precomputed font texture puts limitation to the maximum size.
    size = min(size, gFontstash.mFontMaxSize);

    FONScontext* fs = gFontstash.pContext;
    fs->params.userPtr = &draw; // -V506 (draw only used inside this function)
    fonsSetSize(fs, size * gFontstash.mDpiScaleMin);
    fonsSetFont(fs, fontID);
    fonsSetColor(fs, color);
    fonsSetSpacing(fs, spacing * gFontstash.mDpiScaleMin);
    fonsSetBlur(fs, blur);
    fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
    fonsDrawText(fs, 0.0f, 0.0f, message, NULL);
#endif
}

void cmdDrawDebugFontAtlas(Cmd* pCmd, float2 screenCoordInPx)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.mRenderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    FontstashDrawData draw = {};
    draw.mText3D = false;
    draw.pCmd = pCmd;

    FONScontext* fs = gFontstash.pContext;
    fs->params.userPtr = &draw; // -V506 (draw only used inside this function)
    fonsDrawDebug(fs, screenCoordInPx.x, screenCoordInPx.y);
#endif
}

void fntDefineFonts(const FontDesc* pDescs, uint32_t count, uint32_t* pOutIDs)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(pDescs);
    ASSERT(pOutIDs);
    ASSERT(count > 0);

    arrsetcap(gFontstash.mFontBuffers, arrcap(gFontstash.mFontBuffers) + count);
    arrsetcap(gFontstash.mFontBufferSizes, arrcap(gFontstash.mFontBufferSizes) + count);

    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t     id;
        FONScontext* fs = gFontstash.pContext;

        FileStream fh = {};
        if (fsOpenStreamFromPath(RD_FONTS, pDescs[i].pFontPath, FM_READ, &fh))
        {
            ssize_t bytes = fsGetStreamFileSize(&fh);
            void*   buffer = tf_malloc(bytes);
            fsReadFromStream(&fh, buffer, bytes);

            // add buffer to font buffers for cleanup
            arrpush(gFontstash.mFontBuffers, buffer);
            ASSERT(bytes < UINT32_MAX);
            arrpush(gFontstash.mFontBufferSizes, (uint32_t)bytes);

            fsCloseStream(&fh);

            id = fonsAddFontMem(fs, pDescs[i].pFontName, (unsigned char*)buffer, (int)bytes, 0);
        }
        else
        {
            id = UINT32_MAX;
        }

        ASSERT(id != UINT32_MAX);

        pOutIDs[i] = id;
    }
#endif
}

int2 fntGetFontAtlasSize()
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.mRenderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    int2         size = {};
    FONScontext* fs = gFontstash.pContext;
    fonsGetAtlasSize(fs, &size.x, &size.y);
    return size;
#endif
}

void fntResetFontAtlas(int2 newAtlasSize)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.mRenderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    int2 currentSize = fntGetFontAtlasSize();

    if (newAtlasSize.x == 0)
    {
        newAtlasSize.x = currentSize.x;
    }
    if (newAtlasSize.y == 0)
    {
        newAtlasSize.y = currentSize.y;
    }
    FONScontext* fs = gFontstash.pContext;
    fonsResetAtlas(fs, newAtlasSize.x, newAtlasSize.y);
#endif
}

void fntExpandAtlas(int2 additionalSize)
{
#ifdef ENABLE_FORGE_FONTS
    ASSERT(gFontstash.mRenderInitialized && "Font Rendering not initialized! Make sure to call initFontRendering!");

    FONScontext* fs = gFontstash.pContext;
    fonsExpandAtlas(fs, additionalSize.x, additionalSize.y);
#endif
}

void* fntGetRawFontData(uint32_t fontID)
{
#ifdef ENABLE_FORGE_FONTS
    if (fontID < arrlen(gFontstash.mFontBuffers))
        return gFontstash.mFontBuffers[fontID];
    else
        return NULL;
#else
    return NULL;
#endif
}

uint32_t fntGetRawFontDataSize(uint32_t fontID)
{
#ifdef ENABLE_FORGE_FONTS
    if (fontID < arrlen(gFontstash.mFontBufferSizes))
        return gFontstash.mFontBufferSizes[fontID];
    else
        return UINT_MAX;
#else
    return 0;
#endif
}

float2 fntMeasureFontText(const char* pText, const FontDrawDesc* pDrawDesc)
{
#ifdef ENABLE_FORGE_FONTS

    float textBounds[4] = {};

    const int    messageLength = (int)strlen(pText);
    FONScontext* fs = gFontstash.pContext;
    fonsSetSize(fs, pDrawDesc->mFontSize * gFontstash.mDpiScaleMin);
    fonsSetFont(fs, pDrawDesc->mFontID);
    fonsSetColor(fs, pDrawDesc->mFontColor);
    fonsSetSpacing(fs, pDrawDesc->mFontSpacing * gFontstash.mDpiScaleMin);
    fonsSetBlur(fs, pDrawDesc->mFontBlur);
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    // considering the retina scaling:
    // the render target is already scaled up (w/ retina) and the (x,y) position given to this function
    // is expected to be in the render target's area. Hence, we don't scale up the position again.
    fonsTextBounds(fs, 0.0f /** gFontstash.mDpiScale.x*/, 0.0f /** gFontstash.mDpiScale.y*/, pText, pText + messageLength, textBounds);

    return float2(textBounds[2] - textBounds[0], textBounds[3] - textBounds[1]);
#else
    return float2(0, 0);
#endif
}
