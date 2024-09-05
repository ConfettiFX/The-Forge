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

    Sampler* pNearestClampSampler = NULL;
    Sampler* pLinearClampSampler = NULL;

    Texture* pColorBuffer = NULL;
    Texture* pDepthBuffer = NULL;
    Texture* ppParticleTextures[MAX_PARTICLE_SET_COUNT] = { 0 };

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
    Buffer*  pTransparencyListBuffer = NULL;
    Buffer*  pTransparencyListHeadsBuffer = NULL;

    // Managed by the PS
    Buffer*  pParticlesSectionsIndicesBuffer = NULL;
    Buffer*  pParticleSetVisibilityFlagBuffer = NULL;
    Buffer*  pParticleCountsBuffer = NULL;
    Buffer*  pParticlesToRasterize = NULL;
    Buffer** pBufferParticleRenderIndirectData = NULL;

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

    gPSSettings.pColorBuffer = pDesc->pColorBuffer;
    gPSSettings.pDepthBuffer = pDesc->pDepthBuffer;
    memcpy(gPSSettings.ppParticleTextures, pDesc->ppParticleTextures, sizeof(Texture*) * MAX_PARTICLE_SET_COUNT);

    gPSSettings.pParticlesData = pDesc->pParticlesBuffer;
    gPSSettings.pBitfieldData = pDesc->pBitfieldBuffer;
    gPSSettings.pTransparencyListBuffer = pDesc->pTransparencyListBuffer;
    gPSSettings.pTransparencyListHeadsBuffer = pDesc->pTransparencyListHeadsBuffer;

    gPSSettings.ppParticleConstantBuffer = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mFramesInFlight);
    gPSSettings.pBufferParticleRenderIndirectData = (Buffer**)tf_malloc(sizeof(Buffer*) * pDesc->mFramesInFlight);
    for (uint i = 0; i < pDesc->mFramesInFlight; i++)
        gPSSettings.ppParticleConstantBuffer[i] = pDesc->ppParticleConstantBuffer[i];

    BufferLoadDesc bufferLoadDesc = {};
    bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
    bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    bufferLoadDesc.mDesc.mStructStride = sizeof(uint4);
    bufferLoadDesc.mDesc.mElementCount = 1;
    bufferLoadDesc.mDesc.mSize = sizeof(uint4);
    bufferLoadDesc.mDesc.pName = "ParticleCountsBuffer";
    bufferLoadDesc.pData = NULL;
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticleCountsBuffer;
    addResource(&bufferLoadDesc, NULL);

    bufferLoadDesc.mDesc.mStructStride = sizeof(uint);
    // We have a value for the current frame and another for the previous one
    bufferLoadDesc.mDesc.mElementCount = PARTICLE_BUFFER_SECTION_COUNT * 2;
    bufferLoadDesc.mDesc.mSize = sizeof(uint) * bufferLoadDesc.mDesc.mElementCount;
    bufferLoadDesc.mDesc.pName = "ParticleSectionsIndices";
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticlesSectionsIndicesBuffer;
    addResource(&bufferLoadDesc, NULL);

    bufferLoadDesc.mDesc.mStructStride = sizeof(uint);
    // One element for the previous state, one for the current
    bufferLoadDesc.mDesc.mElementCount = MAX_PARTICLE_SET_COUNT * 2;
    bufferLoadDesc.mDesc.mSize = bufferLoadDesc.mDesc.mStructStride * bufferLoadDesc.mDesc.mElementCount;
    bufferLoadDesc.mDesc.pName = "ParticleSetVisibility";
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticleSetVisibilityFlagBuffer;
    addResource(&bufferLoadDesc, NULL);

    bufferLoadDesc.mDesc.mStructStride = sizeof(uint);
    bufferLoadDesc.mDesc.mElementCount = PARTICLE_COUNT / 10;
    bufferLoadDesc.mDesc.mSize = bufferLoadDesc.mDesc.mStructStride * bufferLoadDesc.mDesc.mElementCount;
    bufferLoadDesc.mDesc.pName = "ParticlesToRasterize";
    bufferLoadDesc.ppBuffer = &gPSSettings.pParticlesToRasterize;
    addResource(&bufferLoadDesc, NULL);

    for (uint32_t i = 0; i < gPSSettings.mFramesInFlight; i++)
    {
        bufferLoadDesc.mDesc.mStructStride = sizeof(uint32_t);
        bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
        bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        bufferLoadDesc.mDesc.mElementCount = 4;
        bufferLoadDesc.mForceReset = true;
        bufferLoadDesc.mDesc.mSize = sizeof(uint32_t) * bufferLoadDesc.mDesc.mElementCount;
        bufferLoadDesc.mDesc.pName = "ParticleRenderIndirectData";
        bufferLoadDesc.ppBuffer = &gPSSettings.pBufferParticleRenderIndirectData[i];
        addResource(&bufferLoadDesc, NULL);
    }

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
            DescriptorData params[6] = {};
            params[0].pName = "ParticleTextures";
            params[0].ppTextures = gPSSettings.ppParticleTextures;
            params[0].mCount = MAX_PARTICLE_SET_COUNT;
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
            updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pParticleRenderDescriptorSet_None, 6, params);
        }

        descriptorSetDesc = { gPSSettings.pParticleSimulateRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gPSSettings.mFramesInFlight };
        addDescriptorSet(gPSSettings.pRenderer, &descriptorSetDesc, &gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
        for (uint32_t i = 0; i < gPSSettings.mFramesInFlight; ++i)
        {
            DescriptorData params[2] = {};
            params[0].pName = "ParticleConstantBuffer";
            params[0].ppBuffers = &gPSSettings.ppParticleConstantBuffer[i];
            params[1].pName = "ParticleRenderIndirectData";
            params[1].ppBuffers = &gPSSettings.pBufferParticleRenderIndirectData[i];
            updateDescriptorSet(gPSSettings.pRenderer, i, gPSSettings.pParticleSimulateDescriptorSet_PerFrame, 2, params);
        }

        descriptorSetDesc = { gPSSettings.pParticleSimulateRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(gPSSettings.pRenderer, &descriptorSetDesc, &gPSSettings.pParticleSimulateDescriptorSet_None);

        {
            DescriptorData params[11] = {};
            params[0].pName = "ParticleTextures";
            params[0].ppTextures = gPSSettings.ppParticleTextures;
            params[0].mCount = MAX_PARTICLE_SET_COUNT;
            params[1].pName = "DepthBuffer";
            params[1].ppTextures = &gPSSettings.pDepthBuffer;
            params[2].pName = "ParticlesDataBuffer";
            params[2].ppBuffers = &gPSSettings.pParticlesData;
            params[3].pName = "ParticleCountsBuffer";
            params[3].ppBuffers = &gPSSettings.pParticleCountsBuffer;
            params[4].pName = "TransparencyList";
            params[4].ppBuffers = &gPSSettings.pTransparencyListBuffer;
            params[5].pName = "BitfieldBuffer";
            params[5].ppBuffers = &gPSSettings.pBitfieldData;
            params[6].pName = "ParticleSectionsIndices";
            params[6].ppBuffers = &gPSSettings.pParticlesSectionsIndicesBuffer;
            params[7].pName = "ParticleSetVisibility";
            params[7].ppBuffers = &gPSSettings.pParticleSetVisibilityFlagBuffer;
            params[8].pName = "ParticlesToRasterize";
            params[8].ppBuffers = &gPSSettings.pParticlesToRasterize;
            params[9].pName = "TransparencyListHeads";
            params[9].ppBuffers = &gPSSettings.pTransparencyListHeadsBuffer;
            updateDescriptorSet(gPSSettings.pRenderer, 0, gPSSettings.pParticleSimulateDescriptorSet_None, 10, params);
        }
    }

    return true;
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

    removeResource(gPSSettings.pParticleCountsBuffer);
    removeResource(gPSSettings.pParticleSetVisibilityFlagBuffer);
    removeResource(gPSSettings.pParticlesSectionsIndicesBuffer);
    for (uint32_t i = 0; i < gPSSettings.mFramesInFlight; i++)
    {
        removeResource(gPSSettings.pBufferParticleRenderIndirectData[i]);
    }
    removeResource(gPSSettings.pParticlesToRasterize);

    tf_free(gPSSettings.pBufferParticleRenderIndirectData);
    tf_free(gPSSettings.ppParticleConstantBuffer);
}

void particleSystemUpdateConstantBuffers(uint32_t frameIndex, ParticleConstantBufferData* cameraData)
{
    ASSERT(cameraData);

    uint32_t dispatchCount = (uint32_t)ceil(sqrt((float)PARTICLE_COUNT / (PARTICLES_BATCH_X * PARTICLES_BATCH_Y)));
    cameraData->SimulationDispatchSize = dispatchCount;
    BufferUpdateDesc bufferUpdateDesc = { gPSSettings.ppParticleConstantBuffer[frameIndex] };
    beginUpdateResource(&bufferUpdateDesc);
    memcpy(bufferUpdateDesc.pMappedData, cameraData, sizeof(*cameraData));
    endUpdateResource(&bufferUpdateDesc);
}

void particleSystemCmdBegin(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);
    ASSERT(frameIndex < gPSSettings.mFramesInFlight);

    cmdBindPipeline(pCmd, gPSSettings.pResetSortingPipeline);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pParticleSimulateDescriptorSet_None);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
    cmdDispatch(pCmd, 1, 1, 1);
}

void particleSystemCmdSimulate(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);

    BufferBarrier resetBarriers[] = {
        { gPSSettings.pParticlesSectionsIndicesBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS },
        { gPSSettings.pParticlesToRasterize, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS },
    };
    cmdResourceBarrier(pCmd, 2, resetBarriers, 0, NULL, 0, NULL);

    uint32_t dispatchCount = (uint32_t)ceil(sqrt((float)PARTICLE_COUNT / (PARTICLES_BATCH_X * PARTICLES_BATCH_Y)));

    cmdBindPipeline(pCmd, gPSSettings.pParticleSimulatePipeline);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pParticleSimulateDescriptorSet_None);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pParticleSimulateDescriptorSet_PerFrame);
    cmdDispatch(pCmd, dispatchCount, dispatchCount, 1);

    BufferBarrier bufferBarriers[] = {
        { gPSSettings.pParticlesData, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS },
        { gPSSettings.pBufferParticleRenderIndirectData[frameIndex], RESOURCE_STATE_UNORDERED_ACCESS,
          RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE },
    };
    cmdResourceBarrier(pCmd, 2, bufferBarriers, 0, NULL, 0, NULL);
}

void particleSystemCmdRender(Cmd* pCmd, uint32_t frameIndex)
{
    ASSERT(pCmd);

    cmdBindPipeline(pCmd, gPSSettings.pParticleRenderPipeline);
    cmdBindDescriptorSet(pCmd, frameIndex, gPSSettings.pParticleRenderDescriptorSet_PerFrame);
    cmdBindDescriptorSet(pCmd, 0, gPSSettings.pParticleRenderDescriptorSet_None);
    cmdExecuteIndirect(pCmd, INDIRECT_DRAW, 1, gPSSettings.pBufferParticleRenderIndirectData[frameIndex], 0, NULL, 0);
    cmdBindRenderTargets(pCmd, NULL);

    BufferBarrier barrier = { gPSSettings.pBufferParticleRenderIndirectData[frameIndex],
                              RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
    cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
}
