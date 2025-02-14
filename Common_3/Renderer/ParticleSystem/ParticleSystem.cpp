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

#include "../../../Common_3/Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../Interfaces/IParticleSystem.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/particle_shared.h.fsl"
#undef NO_FSL_DEFINITIONS

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);

struct ParticleSystemSettings
{
    Renderer* pRenderer = NULL;

    TinyImageFormat mSwapColorFormat = TinyImageFormat::TinyImageFormat_UNDEFINED;
    TinyImageFormat mDepthFormat = TinyImageFormat::TinyImageFormat_UNDEFINED;
    uint32_t        mColorSampleQuality = 0;

    uint32_t mResolutionWidth = 0;
    uint32_t mResolutionHeight = 0;
    uint32_t mFramesInFlight = 0;
    uint32_t mParticleTextureCount = 0;

    Texture*  pColorBuffer = NULL;
    Texture*  pDepthBuffer = NULL;
    Texture** ppParticleTextures = { 0 };

    Pipeline* pParticleRenderPipeline = NULL;
    Pipeline* pParticleSimulatePipeline = NULL;

    // Passed by the user
    Buffer** ppParticleConstantBuffer = NULL;
    Buffer*  pParticlesData = NULL;
    Buffer*  pBitfieldData = NULL;
    Buffer*  pParticleSetsBuffer = NULL;
    Buffer*  pTransparencyListBuffer = NULL;
    Buffer*  pTransparencyListHeadsBuffer = NULL;
#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    Buffer* pStatsBuffer = NULL;
#endif

    // Managed by the PS
    Buffer* pParticlesToRasterize = NULL;
    Buffer* pBufferParticleRenderIndirectData = NULL;

    DescriptorSet* pDescriptorSetPersistent = NULL;
    DescriptorSet* pDescriptorSetPerFrame = NULL;
    DescriptorSet* pDescriptorSetPerBatch = NULL;
} gPSSettings;

bool initParticleSystem(const ParticleSystemInitDesc* pDesc)
{
    gPSSettings.pRenderer = pDesc->pRenderer;

    gPSSettings.mSwapColorFormat = (TinyImageFormat)pDesc->mSwapColorFormat;
    gPSSettings.mDepthFormat = (TinyImageFormat)pDesc->mDepthFormat;
    gPSSettings.mColorSampleQuality = pDesc->mColorSampleQuality;

    gPSSettings.mResolutionWidth = (uint32_t)pDesc->mSwapWidth;
    gPSSettings.mResolutionHeight = (uint32_t)pDesc->mSwapHeight;
    gPSSettings.mFramesInFlight = pDesc->mFramesInFlight;
    gPSSettings.mParticleTextureCount = pDesc->mParticleTextureCount;

    gPSSettings.pColorBuffer = pDesc->pColorBuffer;
    gPSSettings.pDepthBuffer = pDesc->pDepthBuffer;
    gPSSettings.ppParticleTextures = pDesc->ppParticleTextures;

    gPSSettings.pParticleRenderPipeline = pDesc->pParticleRenderPipeline;
    gPSSettings.pParticleSimulatePipeline = pDesc->pParticleSimulatePipeline;

    gPSSettings.pParticlesData = pDesc->pParticlesBuffer;
    gPSSettings.pBitfieldData = pDesc->pBitfieldBuffer;
    gPSSettings.pParticleSetsBuffer = pDesc->pParticleSetsBuffer;
    gPSSettings.pTransparencyListBuffer = pDesc->pTransparencyListBuffer;
    gPSSettings.pTransparencyListHeadsBuffer = pDesc->pTransparencyListHeadsBuffer;

    gPSSettings.ppParticleConstantBuffer = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mFramesInFlight);
    for (uint i = 0; i < pDesc->mFramesInFlight; i++)
        gPSSettings.ppParticleConstantBuffer[i] = pDesc->ppParticleConstantBuffer[i];

    BufferLoadDesc bufferLoadDesc = {};
    bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
    bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    bufferLoadDesc.pData = NULL;
    bufferLoadDesc.mDesc.mStructStride = sizeof(uint);
    bufferLoadDesc.mDesc.mElementCount = MAX_PARTICLES_COUNT / 10;
    bufferLoadDesc.mDesc.mSize = bufferLoadDesc.mDesc.mStructStride * bufferLoadDesc.mDesc.mElementCount;
    bufferLoadDesc.mDesc.pName = "ParticlesToRasterize";
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticlesToRasterize;
    addResource(&bufferLoadDesc, NULL);

    bufferLoadDesc.mDesc.mStructStride = sizeof(uint32_t);
    bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
    bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    bufferLoadDesc.mDesc.mElementCount = 4;
    bufferLoadDesc.mForceReset = true;
    bufferLoadDesc.mDesc.mSize = sizeof(uint32_t) * bufferLoadDesc.mDesc.mElementCount;
    bufferLoadDesc.mDesc.pName = "ParticleRenderIndirectData";
    bufferLoadDesc.ppBuffer = &gPSSettings.pBufferParticleRenderIndirectData;
    addResource(&bufferLoadDesc, NULL);

#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    bufferLoadDesc.mDesc.mStructStride = sizeof(ParticleSystemStats);
    bufferLoadDesc.mDesc.mElementCount = 1;
    bufferLoadDesc.mDesc.mSize = sizeof(ParticleSystemStats);
    bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_GENERIC_READ;
    bufferLoadDesc.mForceReset = true;
    bufferLoadDesc.mDesc.pName = "ParticleSystemStats";
    bufferLoadDesc.ppBuffer = &gPSSettings.pStatsBuffer;
    addResource(&bufferLoadDesc, NULL);
#endif

    // descriptor sets
    {
        gPSSettings.pDescriptorSetPersistent = pDesc->pDescriptorSetPersistent;
        gPSSettings.pDescriptorSetPerFrame = pDesc->pDescriptorSetPerFrame;
        gPSSettings.pDescriptorSetPerBatch = pDesc->pDescriptorSetPerBatch;

        uint32_t       descriptorCount = 0;
        DescriptorData params[12] = {};
        params[descriptorCount].mIndex = pDesc->mParticleTexturesIndex;
        params[descriptorCount].ppTextures = gPSSettings.ppParticleTextures;
        params[descriptorCount++].mCount = gPSSettings.mParticleTextureCount;
        params[descriptorCount].mIndex = pDesc->mDepthBufferIndex;
        params[descriptorCount++].ppTextures = &gPSSettings.pDepthBuffer;
        params[descriptorCount].mIndex = pDesc->mParticleSetBufferIndex;
        params[descriptorCount++].ppBuffers = &gPSSettings.pParticleSetsBuffer;
        updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pDescriptorSetPersistent, descriptorCount, params);

        DescriptorData perBatchParams[9] = {};
        perBatchParams[0].mIndex = pDesc->mTransparencyListIndex;
        perBatchParams[0].ppBuffers = &gPSSettings.pTransparencyListBuffer;
        perBatchParams[1].mIndex = pDesc->mParticlesDataBufferIndex;
        perBatchParams[1].ppBuffers = &gPSSettings.pParticlesData;
        perBatchParams[2].mIndex = pDesc->mParticleRenderIndirectDataIndex;
        perBatchParams[2].ppBuffers = &gPSSettings.pBufferParticleRenderIndirectData;
        perBatchParams[3].mIndex = pDesc->mParticlesToRasterizeIndex;
        perBatchParams[3].ppBuffers = &gPSSettings.pParticlesToRasterize;
        perBatchParams[4].mIndex = pDesc->mBitfieldBufferIndex;
        perBatchParams[4].ppBuffers = &gPSSettings.pBitfieldData;
        perBatchParams[5].mIndex = pDesc->mTransparencyListHeadsIndex;
        perBatchParams[5].ppBuffers = &gPSSettings.pTransparencyListHeadsBuffer;
#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
        perBatchParams[6].mIndex = pDesc->mStatsBufferIndex;
        perBatchParams[6].ppBuffers = &gPSSettings.pStatsBuffer;
        updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pDescriptorSetPerBatch, 7, perBatchParams);
#else
        updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pDescriptorSetPerBatch, 6, perBatchParams);
#endif
    }

    return true;
}

ParticleSystemStats getParticleSystemStats()
{
    ParticleSystemStats ret = {};

#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    mapBuffer(gPSSettings.pRenderer, gPSSettings.pStatsBuffer, 0);
    memcpy(&ret, gPSSettings.pStatsBuffer->pCpuMappedAddress, sizeof(ParticleSystemStats));
    unmapBuffer(gPSSettings.pRenderer, gPSSettings.pStatsBuffer);
#endif

    return ret;
}

void exitParticleSystem()
{
    removeResource(gPSSettings.pBufferParticleRenderIndirectData);
    removeResource(gPSSettings.pParticlesToRasterize);
#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    removeResource(gPSSettings.pStatsBuffer);
#endif

    tf_free(gPSSettings.ppParticleConstantBuffer);
}

void updateParticleSystemConstantBuffers(uint32_t frameIndex, ParticleConstantBufferData* cameraData)
{
    ASSERT(cameraData);

    BufferUpdateDesc bufferUpdateDesc = { gPSSettings.ppParticleConstantBuffer[frameIndex] };
    beginUpdateResource(&bufferUpdateDesc);
    memcpy(bufferUpdateDesc.pMappedData, cameraData, sizeof(*cameraData));
    endUpdateResource(&bufferUpdateDesc);
}

void cmdParticleSystemSimulate(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);

#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    BufferBarrier bb = { gPSSettings.pStatsBuffer, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, 1, &bb, 0, NULL, 0, NULL);
#endif
    BufferBarrier resetBarriers[] = { { gPSSettings.pParticlesToRasterize, RESOURCE_STATE_UNORDERED_ACCESS,
                                        RESOURCE_STATE_UNORDERED_ACCESS } };
    cmdResourceBarrier(pCmd, 1, resetBarriers, 0, NULL, 0, NULL);

    uint32_t threadCount = (uint32_t)ceil(sqrt((float)MAX_PARTICLES_COUNT / (PARTICLES_BATCH_X * PARTICLES_BATCH_Y)));

    cmdBindPipeline(pCmd, gPSSettings.pParticleSimulatePipeline);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pDescriptorSetPersistent);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pDescriptorSetPerBatch);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pDescriptorSetPerFrame);
    cmdDispatch(pCmd, threadCount, threadCount, 1);

    BufferBarrier bufferBarriers[] = {
        { gPSSettings.pParticlesData, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS },
        { gPSSettings.pBufferParticleRenderIndirectData, RESOURCE_STATE_UNORDERED_ACCESS,
          RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE },
#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
        { gPSSettings.pStatsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_GENERIC_READ}
#endif
    };

#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    cmdResourceBarrier(pCmd, 3, bufferBarriers, 0, NULL, 0, NULL);
#else
    cmdResourceBarrier(pCmd, 2, bufferBarriers, 0, NULL, 0, NULL);
#endif
}

void cmdParticleSystemRender(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);

    cmdBindPipeline(pCmd, gPSSettings.pParticleRenderPipeline);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pDescriptorSetPerFrame);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pDescriptorSetPersistent);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pDescriptorSetPerBatch);
    cmdExecuteIndirect(pCmd, INDIRECT_DRAW, 1, gPSSettings.pBufferParticleRenderIndirectData, 0, NULL, 0);
    cmdBindRenderTargets(pCmd, NULL);

    BufferBarrier barrier = { gPSSettings.pBufferParticleRenderIndirectData,
                              RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
}
