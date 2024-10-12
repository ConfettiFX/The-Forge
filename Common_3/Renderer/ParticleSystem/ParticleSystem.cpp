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

    Sampler* pNearestClampSampler = NULL;
    Sampler* pLinearClampSampler = NULL;

    Texture*  pColorBuffer = NULL;
    Texture*  pDepthBuffer = NULL;
    Texture** ppParticleTextures = { 0 };

    Shader* pParticleRenderShader = NULL;
    Shader* pParticleSimulateShader = NULL;
    Shader* pResetSortingShader = NULL;

    RootSignature* pParticleRenderRootSignature = NULL;
    RootSignature* pParticleSimulateRootSignature = NULL;

    Pipeline* pParticleRenderPipeline = NULL;
    Pipeline* pParticleSimulatePipeline = NULL;
    Pipeline* pResetSortingPipeline = NULL;

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
    Buffer* pParticleSetVisibilityFlagBuffer = NULL;
    Buffer* pParticleStateBuffer = NULL;
    Buffer* pParticlesToRasterize = NULL;
    Buffer* pBufferParticleRenderIndirectData = NULL;

    DescriptorSet* pParticleRenderDescriptorSet_PerFrame = NULL;
    DescriptorSet* pParticleRenderDescriptorSet_None = NULL;
    DescriptorSet* pParticleSimulateDescriptorSet_PerFrame = NULL;
    DescriptorSet* pParticleSimulateDescriptorSet_None = NULL;
} gPSSettings;

bool particleSystemInit(const ParticleSystemInitDesc* pDesc)
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
    bufferLoadDesc.mDesc.mStructStride = sizeof(ParticleBufferStateData);
    bufferLoadDesc.mDesc.mElementCount = 1;
    bufferLoadDesc.mDesc.mSize = sizeof(ParticleBufferStateData);
    bufferLoadDesc.mDesc.pName = "ParticleBufferState";
    bufferLoadDesc.pData = NULL;
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticleStateBuffer;
    addResource(&bufferLoadDesc, NULL);

    bufferLoadDesc.mDesc.mStructStride = sizeof(uint);
    // 2 elements for visibility, one for the previous frame, one for the current frame
    // 2 elements for allocation, one for the previous frame, one for the current frame
    bufferLoadDesc.mDesc.mElementCount = MAX_PARTICLE_SET_COUNT * 4;
    bufferLoadDesc.mDesc.mSize = bufferLoadDesc.mDesc.mStructStride * bufferLoadDesc.mDesc.mElementCount;
    bufferLoadDesc.mDesc.pName = "ParticleSetVisibility";
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticleSetVisibilityFlagBuffer;
    addResource(&bufferLoadDesc, NULL);

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

    // Samplers
    {
        SamplerDesc desc{};
        desc.mAddressU = desc.mAddressV = desc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
        desc.mMagFilter = desc.mMinFilter = FILTER_NEAREST;
        addSampler(gPSSettings.pRenderer, &desc, &gPSSettings.pNearestClampSampler);

        desc.mMagFilter = desc.mMinFilter = FILTER_LINEAR;
        addSampler(gPSSettings.pRenderer, &desc, &gPSSettings.pLinearClampSampler);
    }

    // shader
    {
        ShaderLoadDesc shaderDesc = {};
        shaderDesc.mVert = { "particle.vert" };
#if defined(AUTOMATED_TESTING)
        shaderDesc.mFrag = { "particle_hq.frag" };
#else
        shaderDesc.mFrag = { "particle.frag" };
#endif
        addShader(gPSSettings.pRenderer, &shaderDesc, &gPSSettings.pParticleRenderShader);

        shaderDesc = {};
#if defined(AUTOMATED_TESTING)
        shaderDesc.mComp = { "particle_simulate_hq.comp" };
#else
        shaderDesc.mComp = { "particle_simulate.comp" };
#endif

        addShader(gPSSettings.pRenderer, &shaderDesc, &gPSSettings.pParticleSimulateShader);
        shaderDesc.mComp = { "particle_clear_sorting_structs.comp" };
        addShader(gPSSettings.pRenderer, &shaderDesc, &gPSSettings.pResetSortingShader);
    }

    // root signature and cmd signature
    {
        const char* pStaticSamplerNames[] = { "NearestClampSampler", "LinearClampSampler" };
        Sampler*    pStaticSamplers[] = { gPSSettings.pNearestClampSampler, gPSSettings.pLinearClampSampler };

        Shader* renderShaders[] = { gPSSettings.pParticleRenderShader };

        RootSignatureDesc rootDesc = { renderShaders, sizeof(renderShaders) / sizeof(*renderShaders) };
        rootDesc.ppStaticSamplerNames = pStaticSamplerNames;
        rootDesc.ppStaticSamplers = pStaticSamplers;
        rootDesc.mStaticSamplerCount = 2;
        addRootSignature(gPSSettings.pRenderer, &rootDesc, &gPSSettings.pParticleRenderRootSignature);

        Shader* simulateShaders[] = { gPSSettings.pParticleSimulateShader, gPSSettings.pResetSortingShader };
        rootDesc = { simulateShaders, sizeof(simulateShaders) / sizeof(*simulateShaders) };
        rootDesc.ppStaticSamplerNames = pStaticSamplerNames;
        rootDesc.ppStaticSamplers = pStaticSamplers;
        rootDesc.mStaticSamplerCount = 2;
        addRootSignature(gPSSettings.pRenderer, &rootDesc, &gPSSettings.pParticleSimulateRootSignature);
    }

    // pipeline
    RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
    BlendStateDesc      nullBlending = {};
    DepthStateDesc      depthStateDesc = {};
    depthStateDesc.mDepthTest = true;
    depthStateDesc.mDepthWrite = false;
    depthStateDesc.mDepthFunc = CMP_GEQUAL;

    PipelineDesc pipelineDesc = {};
    pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
    pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
    pipelineDesc.mGraphicsDesc.pDepthState = &depthStateDesc;
    pipelineDesc.mGraphicsDesc.pBlendState = &nullBlending;
    pipelineDesc.mGraphicsDesc.pRasterizerState = &rasterizerStateCullNoneDesc;
    pipelineDesc.mGraphicsDesc.pRootSignature = gPSSettings.pParticleRenderRootSignature;
    pipelineDesc.mGraphicsDesc.pShaderProgram = gPSSettings.pParticleRenderShader;
    pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
    pipelineDesc.mGraphicsDesc.pColorFormats = &gPSSettings.mSwapColorFormat;
    pipelineDesc.mGraphicsDesc.mDepthStencilFormat = (TinyImageFormat)pDesc->mDepthFormat;
    pipelineDesc.mGraphicsDesc.mSampleQuality = gPSSettings.mColorSampleQuality;
    addPipeline(gPSSettings.pRenderer, &pipelineDesc, &gPSSettings.pParticleRenderPipeline);

    pipelineDesc = {};
    pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
    pipelineDesc.mComputeDesc.pRootSignature = gPSSettings.pParticleSimulateRootSignature;
    pipelineDesc.mComputeDesc.pShaderProgram = gPSSettings.pParticleSimulateShader;
    addPipeline(gPSSettings.pRenderer, &pipelineDesc, &gPSSettings.pParticleSimulatePipeline);

    pipelineDesc.mComputeDesc.pShaderProgram = gPSSettings.pResetSortingShader;
    addPipeline(gPSSettings.pRenderer, &pipelineDesc, &gPSSettings.pResetSortingPipeline);

    // descriptor sets
    {
        DescriptorSetDesc descriptorSetDesc = { gPSSettings.pParticleRenderRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                                gPSSettings.mFramesInFlight };
        addDescriptorSet(gPSSettings.pRenderer, &descriptorSetDesc, &gPSSettings.pParticleRenderDescriptorSet_PerFrame);

        for (uint32_t i = 0; i < gPSSettings.mFramesInFlight; ++i)
        {
            DescriptorData params = {};
            params.pName = "ParticleConstantBuffer";
            params.ppBuffers = &gPSSettings.ppParticleConstantBuffer[i];
            updateDescriptorSet(gPSSettings.pRenderer, i, gPSSettings.pParticleRenderDescriptorSet_PerFrame, 1, &params);
        }

        descriptorSetDesc = { gPSSettings.pParticleRenderRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(gPSSettings.pRenderer, &descriptorSetDesc, &gPSSettings.pParticleRenderDescriptorSet_None);
        {
            DescriptorData params[7] = {};
            params[0].pName = "ParticleTextures";
            params[0].ppTextures = gPSSettings.ppParticleTextures;
            params[0].mCount = gPSSettings.mParticleTextureCount;
            params[1].pName = "ParticlesDataBuffer";
            params[1].ppBuffers = &gPSSettings.pParticlesData;
            params[2].pName = "BitfieldBuffer";
            params[2].ppBuffers = &gPSSettings.pBitfieldData;
            params[3].pName = "TransparencyList";
            params[3].ppBuffers = &gPSSettings.pTransparencyListBuffer;
            params[4].pName = "TransparencyListHeads";
            params[4].ppBuffers = &gPSSettings.pTransparencyListHeadsBuffer;
            params[5].pName = "ParticlesToRasterize";
            params[5].ppBuffers = &gPSSettings.pParticlesToRasterize;
            params[6].pName = "ParticleSetsBuffer";
            params[6].ppBuffers = &gPSSettings.pParticleSetsBuffer;
            updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pParticleRenderDescriptorSet_None, 7, params);
        }

        descriptorSetDesc = { gPSSettings.pParticleSimulateRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gPSSettings.mFramesInFlight };
        addDescriptorSet(gPSSettings.pRenderer, &descriptorSetDesc, &gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
        for (uint32_t i = 0; i < gPSSettings.mFramesInFlight; ++i)
        {
            DescriptorData params[2] = {};
            params[0].pName = "ParticleConstantBuffer";
            params[0].ppBuffers = &gPSSettings.ppParticleConstantBuffer[i];
            updateDescriptorSet(gPSSettings.pRenderer, i, gPSSettings.pParticleSimulateDescriptorSet_PerFrame, 1, params);
        }

        descriptorSetDesc = { gPSSettings.pParticleSimulateRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(gPSSettings.pRenderer, &descriptorSetDesc, &gPSSettings.pParticleSimulateDescriptorSet_None);

        {
            uint32_t       descriptorCount = 0;
            DescriptorData params[12] = {};
            params[descriptorCount].pName = "ParticleTextures";
            params[descriptorCount].ppTextures = gPSSettings.ppParticleTextures;
            params[descriptorCount++].mCount = gPSSettings.mParticleTextureCount;
            params[descriptorCount].pName = "DepthBuffer";
            params[descriptorCount++].ppTextures = &gPSSettings.pDepthBuffer;
            params[descriptorCount].pName = "ParticlesDataBuffer";
            params[descriptorCount++].ppBuffers = &gPSSettings.pParticlesData;
            params[descriptorCount].pName = "ParticleBufferState";
            params[descriptorCount++].ppBuffers = &gPSSettings.pParticleStateBuffer;
            params[descriptorCount].pName = "TransparencyList";
            params[descriptorCount++].ppBuffers = &gPSSettings.pTransparencyListBuffer;
            params[descriptorCount].pName = "BitfieldBuffer";
            params[descriptorCount++].ppBuffers = &gPSSettings.pBitfieldData;
            params[descriptorCount].pName = "ParticleSetsBuffer";
            params[descriptorCount++].ppBuffers = &gPSSettings.pParticleSetsBuffer;
            params[descriptorCount].pName = "ParticleSetVisibility";
            params[descriptorCount++].ppBuffers = &gPSSettings.pParticleSetVisibilityFlagBuffer;
            params[descriptorCount].pName = "ParticlesToRasterize";
            params[descriptorCount++].ppBuffers = &gPSSettings.pParticlesToRasterize;
            params[descriptorCount].pName = "TransparencyListHeads";
            params[descriptorCount++].ppBuffers = &gPSSettings.pTransparencyListHeadsBuffer;
            params[descriptorCount].pName = "ParticleRenderIndirectData";
            params[descriptorCount++].ppBuffers = &gPSSettings.pBufferParticleRenderIndirectData;
#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
            params[descriptorCount].pName = "StatsBuffer";
            params[descriptorCount++].ppBuffers = &gPSSettings.pStatsBuffer;
#endif
            updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pParticleSimulateDescriptorSet_None, descriptorCount, params);
        }
    }

    return true;
}

ParticleSystemStats particleSystemGetStats()
{
    ParticleSystemStats ret = {};

#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    mapBuffer(gPSSettings.pRenderer, gPSSettings.pStatsBuffer, 0);
    memcpy(&ret, gPSSettings.pStatsBuffer->pCpuMappedAddress, sizeof(ParticleSystemStats));
    unmapBuffer(gPSSettings.pRenderer, gPSSettings.pStatsBuffer);
#endif

    return ret;
}

void particleSystemExit()
{
    removeShader(gPSSettings.pRenderer, gPSSettings.pParticleRenderShader);
    removeShader(gPSSettings.pRenderer, gPSSettings.pParticleSimulateShader);
    removeShader(gPSSettings.pRenderer, gPSSettings.pResetSortingShader);

    removeRootSignature(gPSSettings.pRenderer, gPSSettings.pParticleRenderRootSignature);
    removeRootSignature(gPSSettings.pRenderer, gPSSettings.pParticleSimulateRootSignature);

    removePipeline(gPSSettings.pRenderer, gPSSettings.pParticleRenderPipeline);
    removePipeline(gPSSettings.pRenderer, gPSSettings.pParticleSimulatePipeline);
    removePipeline(gPSSettings.pRenderer, gPSSettings.pResetSortingPipeline);

    removeDescriptorSet(gPSSettings.pRenderer, gPSSettings.pParticleRenderDescriptorSet_PerFrame);
    removeDescriptorSet(gPSSettings.pRenderer, gPSSettings.pParticleRenderDescriptorSet_None);
    removeDescriptorSet(gPSSettings.pRenderer, gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
    removeDescriptorSet(gPSSettings.pRenderer, gPSSettings.pParticleSimulateDescriptorSet_None);

    removeSampler(gPSSettings.pRenderer, gPSSettings.pNearestClampSampler);
    removeSampler(gPSSettings.pRenderer, gPSSettings.pLinearClampSampler);

    removeResource(gPSSettings.pParticleStateBuffer);
    removeResource(gPSSettings.pParticleSetVisibilityFlagBuffer);
    removeResource(gPSSettings.pBufferParticleRenderIndirectData);
    removeResource(gPSSettings.pParticlesToRasterize);
#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    removeResource(gPSSettings.pStatsBuffer);
#endif

    tf_free(gPSSettings.ppParticleConstantBuffer);
}

void particleSystemUpdateConstantBuffers(uint32_t frameIndex, ParticleConstantBufferData* cameraData)
{
    ASSERT(cameraData);

    BufferUpdateDesc bufferUpdateDesc = { gPSSettings.ppParticleConstantBuffer[frameIndex] };
    beginUpdateResource(&bufferUpdateDesc);
    memcpy(bufferUpdateDesc.pMappedData, cameraData, sizeof(*cameraData));
    endUpdateResource(&bufferUpdateDesc);
}

void particleSystemCmdBegin(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);
    ASSERT(frameIndex < gPSSettings.mFramesInFlight);

#if defined(FORGE_DEBUG) && !defined(AUTOMATED_TESTING)
    BufferBarrier bb = { gPSSettings.pStatsBuffer, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, 1, &bb, 0, NULL, 0, NULL);
#endif
    cmdBindPipeline(pCmd, gPSSettings.pResetSortingPipeline);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pParticleSimulateDescriptorSet_None);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
    cmdDispatch(pCmd, 1, 1, 1);
}

void particleSystemCmdSimulate(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);

    BufferBarrier resetBarriers[] = { { gPSSettings.pParticlesToRasterize, RESOURCE_STATE_UNORDERED_ACCESS,
                                        RESOURCE_STATE_UNORDERED_ACCESS } };
    cmdResourceBarrier(pCmd, 1, resetBarriers, 0, NULL, 0, NULL);

    uint32_t threadCount = (uint32_t)ceil(sqrt((float)MAX_PARTICLES_COUNT / (PARTICLES_BATCH_X * PARTICLES_BATCH_Y)));

    cmdBindPipeline(pCmd, gPSSettings.pParticleSimulatePipeline);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pParticleSimulateDescriptorSet_None);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
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

void particleSystemCmdRender(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);

    cmdBindPipeline(pCmd, gPSSettings.pParticleRenderPipeline);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pParticleRenderDescriptorSet_PerFrame);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pParticleRenderDescriptorSet_None);
    cmdExecuteIndirect(pCmd, INDIRECT_DRAW, 1, gPSSettings.pBufferParticleRenderIndirectData, 0, NULL, 0);
    cmdBindRenderTargets(pCmd, NULL);

    BufferBarrier barrier = { gPSSettings.pBufferParticleRenderIndirectData,
                              RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
}
