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

#include "../Interfaces/IScreenshot.h"

#include "../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IUI.h"
#include "../Interfaces/IProfiler.h"

#include "../../Graphics/FSL/fsl_srt.h"
#include "../../Graphics/FSL/defaults.h"
#include "./Shaders/FSL/Copy.comp.srt.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC  tf_malloc
#define STBIW_REALLOC tf_realloc
#define STBIW_FREE    tf_free
#define STBIW_ASSERT  ASSERT
#define STBI_WRITE_NO_STDIO
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_image_write.h"

typedef struct ScreenshotCapturer
{
    Renderer*      pRenderer;
    Shader*        pCopyShader;
    Pipeline*      pCopyPipeline;
    DescriptorSet* pCopyDescriptorSet;
    CmdPool*       pCmdPool;
    Cmd*           pCmd;
    Queue*         pQueue;
    Fence*         pFence;
    Buffer*        pScreenshotScratchBuffer;
    Buffer*        pScreenShotUniformsBuffer;
    Sampler*       pSampler;
    size_t         mAppNameLen;
    bool           mScreenshotCaptureRequested;
    char           mScreenshotName[FS_MAX_PATH];
} ScreenshotCapturer;

static ScreenshotCapturer* pScreenshotCapturer = NULL;

#ifdef ENABLE_SCREENSHOT
typedef struct stbiw_ctx
{
    uint8_t* pBuffer;
    int      mOffset;
} stbiw_ctx;

static void stbiw_func(void* context, void* data, int size)
{
    stbiw_ctx* ctx = (stbiw_ctx*)context;
    memcpy(ctx->pBuffer + ctx->mOffset, data, size);
    ctx->mOffset += size;
}

// stbi ignores Alpha channel for HDR. DiscardAlpha is only meaningful when saveAsHDR == false
static void saveScreenshotToDisk(const char* name, Buffer* pBuffer, bool saveAsHDR, bool discardAlpha, uint32_t width, uint32_t height)
{
    char        screenshotFileName[FS_MAX_PATH] = {};
    const char* ext = saveAsHDR ? ".hdr" : ".png";
    size_t      nameLen = strlen(name);
    memcpy(screenshotFileName, name, nameLen);
    memcpy(screenshotFileName + nameLen, ext, 4);
    screenshotFileName[nameLen + 4] = '\0';

    void* pEncodedImage;
    int   encodedSize = 0;

    if (!saveAsHDR)
    {
        // Using a cpu buffer speeds things up greatly here even with extra memcpy.
        int            numComponents = discardAlpha ? 3 : 4;
        unsigned char* cpuBuffer = (unsigned char*)tf_malloc(width * height * numComponents);
        unsigned char* gpuBuffer = (unsigned char*)pBuffer->pCpuMappedAddress;
        for (uint32_t i = 0; i < width * height; i++)
        {
            memcpy(cpuBuffer + i * numComponents, gpuBuffer + i * 4, numComponents);
        }
        stbi_write_png_compression_level = 4;
        pEncodedImage = stbi_write_png_to_mem(cpuBuffer, width * numComponents, width, height, numComponents, &encodedSize);
        tf_free(cpuBuffer);
    }
    else
    {
        // stbi does not offer a write_hdr_to_mem, we need to allocate memory ourselves.
        pEncodedImage = tf_malloc(width * height * sizeof(float4));
        stbiw_ctx ctx = { (uint8_t*)pEncodedImage };
        stbi_write_hdr_to_func(stbiw_func, &ctx, width, height, 4, (float*)pBuffer->pCpuMappedAddress);
        encodedSize = ctx.mOffset;
    }
    FileStream fs = {};
    if (fsOpenStreamFromPath(RD_SCREENSHOTS, screenshotFileName, FM_WRITE, &fs))
    {
        LOGF(eINFO, "Writing screenshot to %s", screenshotFileName);
        VERIFY(fsWriteToStream(&fs, pEncodedImage, (size_t)encodedSize) == (size_t)encodedSize);
        VERIFY(fsCloseStream(&fs));
    }
    else
    {
        LOGF(eERROR, "Failed to open file for a screenshot: %s.Function %s failed with error: %s", screenshotFileName, FS_ERR_CTX.func,
             getFSErrCodeString(FS_ERR_CTX.code));
    }

    tf_free(pEncodedImage);
}
#endif

void initScreenshotCapturer(Renderer* pRenderer, Queue* pGraphicsQueue, const char* appName)
{
#ifdef ENABLE_SCREENSHOT
    ASSERT(!pScreenshotCapturer);
    ASSERT(pRenderer);
    ASSERT(pGraphicsQueue);
    ASSERT(appName);

    pScreenshotCapturer = (ScreenshotCapturer*)tf_malloc(sizeof(ScreenshotCapturer));
    *pScreenshotCapturer = {};
    pScreenshotCapturer->pQueue = pGraphicsQueue;
    pScreenshotCapturer->pRenderer = pRenderer;
    pScreenshotCapturer->mAppNameLen = strlen(appName);
    if (VERIFYMSG(pScreenshotCapturer->mAppNameLen < FS_MAX_PATH, "App name exceeds FS_MAX_PATH. It is ignored by screenshot capturer."))
    {
        memcpy(pScreenshotCapturer->mScreenshotName, appName, pScreenshotCapturer->mAppNameLen);
        pScreenshotCapturer->mScreenshotName[pScreenshotCapturer->mAppNameLen] = '\0';
    }
    else
    {
        pScreenshotCapturer->mAppNameLen = 0;
        pScreenshotCapturer->mScreenshotName[0] = '\0';
    }

    SamplerDesc pointSamplerDesc = {};
    pointSamplerDesc.mMinFilter = FILTER_NEAREST;
    pointSamplerDesc.mMagFilter = FILTER_NEAREST;
    pointSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
    pointSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
    pointSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
    pointSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
    addSampler(pRenderer, &pointSamplerDesc, &pScreenshotCapturer->pSampler);

    BufferLoadDesc bufferDesc = {};
    bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    bufferDesc.mDesc.mSize = sizeof(ScreenShotParams);
    bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    bufferDesc.mDesc.pName = "ScreenShot params uniforms";
    bufferDesc.pData = NULL;
    bufferDesc.ppBuffer = &pScreenshotCapturer->pScreenShotUniformsBuffer;
    addResource(&bufferDesc, NULL);

    CmdPoolDesc cmdPoolDesc = {};
    cmdPoolDesc.pQueue = pGraphicsQueue;
    cmdPoolDesc.mTransient = true;
    initCmdPool(pRenderer, &cmdPoolDesc, &pScreenshotCapturer->pCmdPool);

    CmdDesc cmdDesc = {};
    cmdDesc.pPool = pScreenshotCapturer->pCmdPool;
#ifdef ENABLE_GRAPHICS_DEBUG_ANNOTATION
    cmdDesc.pName = "Screenshot Cmd";
#endif
    initCmd(pRenderer, &cmdDesc, &pScreenshotCapturer->pCmd);

    initFence(pRenderer, &pScreenshotCapturer->pFence);

    ShaderLoadDesc copyShaderDesc = {};
    copyShaderDesc.mComp.pFileName = "copy.comp";
    addShader(pRenderer, &copyShaderDesc, &pScreenshotCapturer->pCopyShader);

    // All work happen sequentially, we won't have two inflight captures. So one descriptor is enough.
    DescriptorSetDesc descriptorSetDesc = SRT_SET_DESC(SrtCopyCompData, PerDraw, 1, 0);

    descriptorSetDesc.mMaxSets = 1;
    addDescriptorSet(pRenderer, &descriptorSetDesc, &pScreenshotCapturer->pCopyDescriptorSet);

    PipelineDesc pipelineDesc = {};
    PIPELINE_LAYOUT_DESC(pipelineDesc, NULL, NULL, NULL, SRT_LAYOUT_DESC(SrtCopyCompData, PerDraw));
    pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
    pipelineDesc.mComputeDesc = {};

    ComputePipelineDesc* copyPipelineDesc = &pipelineDesc.mComputeDesc;
    copyPipelineDesc->pShaderProgram = pScreenshotCapturer->pCopyShader;
    addPipeline(pRenderer, &pipelineDesc, &pScreenshotCapturer->pCopyPipeline);
#else
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pGraphicsQueue);
    UNREF_PARAM(appName);
#endif
}

void exitScreenshotCapturer()
{
#ifdef ENABLE_SCREENSHOT
    ASSERT(pScreenshotCapturer);
    Renderer* pRenderer = pScreenshotCapturer->pRenderer;

    // Clean up gpu resources
    removeResource(pScreenshotCapturer->pScreenShotUniformsBuffer);
    removeSampler(pRenderer, pScreenshotCapturer->pSampler);
    removePipeline(pRenderer, pScreenshotCapturer->pCopyPipeline);
    removeDescriptorSet(pRenderer, pScreenshotCapturer->pCopyDescriptorSet);
    removeShader(pRenderer, pScreenshotCapturer->pCopyShader);
    if (pScreenshotCapturer->pScreenshotScratchBuffer)
    {
        removeResource(pScreenshotCapturer->pScreenshotScratchBuffer);
    }
    exitCmd(pRenderer, pScreenshotCapturer->pCmd);
    exitCmdPool(pRenderer, pScreenshotCapturer->pCmdPool);
    exitFence(pRenderer, pScreenshotCapturer->pFence);

    tf_free(pScreenshotCapturer);
    pScreenshotCapturer = NULL;
#endif
}

#ifdef ENABLE_SCREENSHOT
static void updateUIVisibility()
{
    uiToggleRendering(!pScreenshotCapturer->mScreenshotCaptureRequested);
    toggleProfilerDrawing(!pScreenshotCapturer->mScreenshotCaptureRequested);
}
#endif

void requestScreenshotCapture(const char* name)
{
#ifdef ENABLE_SCREENSHOT
    ASSERT(pScreenshotCapturer);

    size_t nameLen = strlen(name);
    // appname + '_' + name + (.png / .hdr)
    size_t appNameLen = pScreenshotCapturer->mAppNameLen;
    if (VERIFYMSG(nameLen + appNameLen + 4 < FS_MAX_PATH,
                  "App name + screenshot name's total size exceeds FS_MAX_PATH. Only using app name."))
    {
        pScreenshotCapturer->mScreenshotName[appNameLen] = '_';
        memcpy(pScreenshotCapturer->mScreenshotName + appNameLen + 1, name, nameLen);
        pScreenshotCapturer->mScreenshotName[nameLen + appNameLen + 1] = '\0';
    }
    pScreenshotCapturer->mScreenshotCaptureRequested = true;
    updateUIVisibility();
#else
    UNREF_PARAM(name);
#endif
}

bool isScreenshotCaptureRequested()
{
#ifdef ENABLE_SCREENSHOT
    ASSERT(pScreenshotCapturer);
    return pScreenshotCapturer->mScreenshotCaptureRequested;
#else
    return false;
#endif
}

void captureScreenshot(ScreenshotDesc* pDesc)
{
#ifdef ENABLE_SCREENSHOT
    ASSERT(pScreenshotCapturer);

    Renderer*     pRenderer = pScreenshotCapturer->pRenderer;
    RenderTarget* pRenderTarget = pDesc->pRenderTarget;

    bool saveAsHDR = pDesc->mColorSpace > COLOR_SPACE_SDR_SRGB;
    bool convertToSrgb =
        (pRenderTarget->mFormat == TinyImageFormat_R8G8B8A8_SRGB || pRenderTarget->mFormat == TinyImageFormat_B8G8R8A8_SRGB);
    TinyImageFormat screenshotFormat = saveAsHDR ? TinyImageFormat_R32G32B32A32_UINT : TinyImageFormat_R8G8B8A8_UINT;
    uint32_t        formatByteStride = TinyImageFormat_BitSizeOfBlock(screenshotFormat) / 8;
    uint32_t        rtWidth = pRenderTarget->mWidth;
    uint32_t        rtHeight = pRenderTarget->mHeight;
    // If we are saving the screenshot of a VR image, the source would be a texture array
    // So we make the screenshot image having a width that's twice the eye buffer resolution width.
    rtWidth = rtWidth * pRenderTarget->mArraySize;
    uint32_t rtSize = rtWidth * rtHeight * formatByteStride;

    Cmd* cmd = pScreenshotCapturer->pCmd;
    resetCmdPool(pScreenshotCapturer->pRenderer, pScreenshotCapturer->pCmdPool);
    beginCmd(cmd);

    // Perform "lazy loading" here. This can be moved into loadScreenshotCapturer() and be hooked into app
    // life cycle.
    if (!pScreenshotCapturer->pScreenshotScratchBuffer || rtSize > pScreenshotCapturer->pScreenshotScratchBuffer->mSize)
    {
        if (pScreenshotCapturer->pScreenshotScratchBuffer)
        {
            removeResource(pScreenshotCapturer->pScreenshotScratchBuffer);
        }

        SyncToken      token = {};
        BufferLoadDesc scratchBufferLoadDesc = {};
        scratchBufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER_RAW;
        scratchBufferLoadDesc.mDesc.mSize = rtSize;
        scratchBufferLoadDesc.mDesc.mElementCount = (rtSize + 3) / sizeof(uint32_t);
        scratchBufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
        // dx12 defaults to RESOURCE_STATE_COMMON for RW buffer allocated in non-default heap
        scratchBufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
        scratchBufferLoadDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        scratchBufferLoadDesc.mDesc.pName = "ScratchBuffer";
        scratchBufferLoadDesc.ppBuffer = &pScreenshotCapturer->pScreenshotScratchBuffer;
        addResource(&scratchBufferLoadDesc, &token);
        waitForToken(&token);

        BufferBarrier bufBarrier = {};
        bufBarrier.pBuffer = pScreenshotCapturer->pScreenshotScratchBuffer;
        bufBarrier.mCurrentState = RESOURCE_STATE_COMMON;
        bufBarrier.mNewState = RESOURCE_STATE_COPY_SOURCE;
        cmdResourceBarrier(cmd, 1, &bufBarrier, 0, NULL, 0, NULL);
    }

    // update buffer which contains screenshot params
    ScreenShotParams rootConstant = {};
    rootConstant.convertToSrgb = convertToSrgb;
    rootConstant.saveAsHDR = saveAsHDR;
    rootConstant.flipRedBlue = pDesc->flipRedBlue;
    BufferUpdateDesc uniformsUpdateDesc = { pScreenshotCapturer->pScreenShotUniformsBuffer };
    beginUpdateResource(&uniformsUpdateDesc);
    memcpy(uniformsUpdateDesc.pMappedData, &rootConstant, sizeof(ScreenShotParams));
    endUpdateResource(&uniformsUpdateDesc);

    DescriptorData copyParam[3] = {};
    copyParam[0].mIndex = SRT_RES_IDX(SrtCopyCompData, PerDraw, gInputTexture);
    copyParam[0].ppTextures = &pRenderTarget->pTexture;
    copyParam[1].mIndex = SRT_RES_IDX(SrtCopyCompData, PerDraw, gOutputBuffer);
    copyParam[1].ppBuffers = &pScreenshotCapturer->pScreenshotScratchBuffer;
    copyParam[2].mIndex = SRT_RES_IDX(SrtCopyCompData, PerDraw, gScreenShotConstants);
    copyParam[2].ppBuffers = &pScreenshotCapturer->pScreenShotUniformsBuffer;
    updateDescriptorSet(pRenderer, 0, pScreenshotCapturer->pCopyDescriptorSet, 3, copyParam);

    // Perform copy
    cmdBindRenderTargets(cmd, NULL);
    BufferBarrier bufBarrier = {};
    bufBarrier.pBuffer = pScreenshotCapturer->pScreenshotScratchBuffer;
    bufBarrier.mCurrentState = RESOURCE_STATE_COPY_SOURCE;
    bufBarrier.mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
    RenderTargetBarrier rtBarrier = {};
    rtBarrier.pRenderTarget = pRenderTarget;
    rtBarrier.mCurrentState = RESOURCE_STATE_PRESENT;
    rtBarrier.mNewState = RESOURCE_STATE_SHADER_RESOURCE;
    cmdResourceBarrier(cmd, 1, &bufBarrier, 0, NULL, 1, &rtBarrier);

    uint3 threadGroupSize = pScreenshotCapturer->pCopyShader->mNumThreadsPerGroup;
    cmdBindPipeline(cmd, pScreenshotCapturer->pCopyPipeline);
    cmdBindDescriptorSet(cmd, 0, pScreenshotCapturer->pCopyDescriptorSet);
    cmdDispatch(cmd, (rtWidth + threadGroupSize.x - 1) / threadGroupSize.x, (rtHeight + threadGroupSize.y - 1) / threadGroupSize.y, 1);

    bufBarrier.mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
    bufBarrier.mNewState = RESOURCE_STATE_COPY_SOURCE;
    rtBarrier.mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
    rtBarrier.mNewState = RESOURCE_STATE_PRESENT;
    cmdResourceBarrier(cmd, 1, &bufBarrier, 0, NULL, 1, &rtBarrier);

    endCmd(cmd);

    QueueSubmitDesc submitDesc = {};
    submitDesc.mCmdCount = 1;
    submitDesc.ppCmds = &pScreenshotCapturer->pCmd;
    submitDesc.ppWaitSemaphores = pDesc->ppWaitSemaphores;
    submitDesc.mWaitSemaphoreCount = pDesc->mWaitSemaphoresCount;
    submitDesc.pSignalFence = pScreenshotCapturer->pFence;
    queueSubmit(pScreenshotCapturer->pQueue, &submitDesc);

    waitForFences(pRenderer, 1, &pScreenshotCapturer->pFence);

    saveScreenshotToDisk(pScreenshotCapturer->mScreenshotName, pScreenshotCapturer->pScreenshotScratchBuffer, saveAsHDR,
                         pDesc->discardAlpha, rtWidth, rtHeight);

    pScreenshotCapturer->mScreenshotCaptureRequested = false;
    updateUIVisibility();
#else
    UNREF_PARAM(pDesc);
#endif
}
