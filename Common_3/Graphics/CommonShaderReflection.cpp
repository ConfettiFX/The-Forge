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

#include "../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../Utilities/Interfaces/ILog.h"
#include "Interfaces/IGraphics.h"
#include "../Utilities/Math/MathTypes.h"
#include "../Utilities/Interfaces/IMemory.h"

#define DESCRIPTOR_UPDATE_FREQ_PADDING 0
// This file contains shader reflection code that is the same for all platforms.
// We know it's the same for all platforms since it only interacts with the
//  platform abstractions we created.
void removeShaderReflection(ShaderReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    tf_free(pReflection->pNamePool);
    tf_free(pReflection->pVertexInputs);
}

void addPipelineReflection(ShaderReflection* pReflection, uint32_t stageCount, PipelineReflection* pOutReflection)
{
    // Parameter checks
    if (pReflection == NULL)
    {
        LOGF(LogLevel::eERROR, "Parameter 'pReflection' is NULL.");
        return;
    }
    if (stageCount == 0)
    {
        LOGF(LogLevel::eERROR, "Parameter 'stageCount' is 0.");
        return;
    }
    if (pOutReflection == NULL)
    {
        LOGF(LogLevel::eERROR, "Parameter 'pOutShaderReflection' is NULL.");
        return;
    }

    // Sanity check to make sure we don't have repeated stages.
    ShaderStage combinedShaderStages = (ShaderStage)0;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        if ((combinedShaderStages & pReflection[i].mShaderStage) != 0)
        {
            LOGF(LogLevel::eERROR, "Duplicate shader stage was detected in shader reflection array.");
            return;
        }
        combinedShaderStages = (ShaderStage)(combinedShaderStages | pReflection[i].mShaderStage);
    }

    // Combine all shaders
    // this will have a large amount of looping
    // 1. count number of resources
    uint32_t vertexStageIndex = UINT32_MAX;
    uint32_t hullStageIndex = UINT32_MAX;
    uint32_t domainStageIndex = UINT32_MAX;
    uint32_t geometryStageIndex = UINT32_MAX;
    uint32_t pixelStageIndex = UINT32_MAX;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        ShaderReflection* pSrcRef = pReflection + i;
#if defined(DIRECT3D12)
        pOutReflection->mResourceHeapIndexing |= pSrcRef->mResourceHeapIndexing;
        pOutReflection->mSamplerHeapIndexing |= pSrcRef->mSamplerHeapIndexing;
#endif

        if (pSrcRef->mShaderStage == SHADER_STAGE_VERT)
        {
            vertexStageIndex = i;
            pOutReflection->mVertexInputsCount = pSrcRef->mVertexInputsCount;
        }
#if !defined(METAL)
        else if (pSrcRef->mShaderStage == SHADER_STAGE_HULL)
        {
            hullStageIndex = i;
            pOutReflection->mNumControlPoint = pSrcRef->mNumControlPoint;
        }
        else if (pSrcRef->mShaderStage == SHADER_STAGE_DOMN)
        {
            domainStageIndex = i;
        }
        else if (pSrcRef->mShaderStage == SHADER_STAGE_GEOM)
        {
            geometryStageIndex = i;
        }
#endif
        else if (pSrcRef->mShaderStage == SHADER_STAGE_FRAG)
        {
            pixelStageIndex = i;
            pOutReflection->mOutputRenderTargetTypesMask = pSrcRef->mOutputRenderTargetTypesMask;
        }
        else if (pSrcRef->mShaderStage == SHADER_STAGE_COMP)
        {
            memcpy(pOutReflection->mNumThreadsPerGroup, pSrcRef->mNumThreadsPerGroup, sizeof(pSrcRef->mNumThreadsPerGroup));
        }
    }

    if (pOutReflection->mNamePoolSize)
    {
        pOutReflection->pNamePool = (char*)tf_calloc(pOutReflection->mNamePoolSize, 1);
    }

    // all refection structs should be built now
    pOutReflection->mShaderStages = combinedShaderStages;

    pOutReflection->mStageReflectionCount = stageCount;

    pOutReflection->mVertexStageIndex = vertexStageIndex;
    pOutReflection->mHullStageIndex = hullStageIndex;
    pOutReflection->mDomainStageIndex = domainStageIndex;
    pOutReflection->mGeometryStageIndex = geometryStageIndex;
    pOutReflection->mPixelStageIndex = pixelStageIndex;
}

void removePipelineReflection(PipelineReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    tf_free(pReflection->pNamePool);
}
