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

#include "../GraphicsConfig.h"

#ifdef VULKAN

#include "../ThirdParty/OpenSource/SPIRV_Cross/SpirvTools.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IGraphics.h"
#include "../../Utilities/Interfaces/IMemory.h"

bool filterResource(SPIRV_Resource* resource, ShaderStage currentStage)
{
    bool filter = false;

    // remove used resources
    // TODO: log warning
    filter = filter || (resource->is_used == false);

    // remove stage outputs
    filter = filter || (resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_OUTPUTS);

    // remove stage inputs that are not on the vertex shader
    filter = filter || (resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS && currentStage != SHADER_STAGE_VERT);

    return filter;
}

void vk_addShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection)
{
    if (pOutReflection == NULL)
    {
        LOGF(LogLevel::eERROR, "Create Shader Refection failed. Invalid reflection output!");
        return; // TODO: error msg
    }

    CrossCompiler cc;

    CreateCrossCompiler((const uint32_t*)shaderCode, shaderSize / sizeof(uint32_t), &cc);

    ReflectEntryPoint(&cc);
    ReflectShaderResources(&cc);
    ReflectShaderVariables(&cc);

    if (shaderStage == SHADER_STAGE_COMP)
    {
        ReflectComputeShaderWorkGroupSize(&cc, &pOutReflection->mNumThreadsPerGroup[0], &pOutReflection->mNumThreadsPerGroup[1],
                                          &pOutReflection->mNumThreadsPerGroup[2]);
    }
    else if (shaderStage == SHADER_STAGE_TESC)
    {
        ReflectHullShaderControlPoint(&cc, &pOutReflection->mNumControlPoint);
    }

    // lets find out the size of the name pool we need
    // also get number of resources while we are at it
    uint32_t namePoolSize = 0;
    uint32_t vertexInputCount = 0;

    namePoolSize += cc.EntryPointSize + 1;

    for (uint32_t i = 0; i < cc.ShaderResourceCount; ++i)
    {
        SPIRV_Resource* resource = cc.pShaderResouces + i;

        // filter out what we don't use
        if (!filterResource(resource, shaderStage))
        {
            namePoolSize += resource->name_size + 1;

            if (resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS && shaderStage == SHADER_STAGE_VERT)
            {
                ++vertexInputCount;
            }
        }
    }

    for (uint32_t i = 0; i < cc.UniformVariablesCount; ++i)
    {
        SPIRV_Variable* variable = cc.pUniformVariables + i;

        // check if parent buffer was filtered out
        bool parentFiltered = filterResource(cc.pShaderResouces + variable->parent_index, shaderStage);

        // filter out what we don't use
        // TODO: log warning
        if (variable->is_used && !parentFiltered)
        {
            namePoolSize += variable->name_size + 1;
        }
    }

    // we now have the size of the memory pool and number of resources
    char* namePool = NULL;
    if (namePoolSize)
        namePool = (char*)tf_calloc(namePoolSize, 1);
    char* pCurrentName = namePool;

    pOutReflection->pEntryPoint = pCurrentName;
    ASSERT(pCurrentName);
    memcpy(pCurrentName, cc.pEntryPoint, cc.EntryPointSize); //-V575
    pCurrentName += cc.EntryPointSize + 1;

    VertexInput* pVertexInputs = NULL;
    // start with the vertex input
    if (shaderStage == SHADER_STAGE_VERT && vertexInputCount > 0)
    {
        pVertexInputs = (VertexInput*)tf_malloc(sizeof(VertexInput) * vertexInputCount);

        uint32_t j = 0;
        for (uint32_t i = 0; i < cc.ShaderResourceCount; ++i)
        {
            SPIRV_Resource* resource = cc.pShaderResouces + i;

            // filter out what we don't use
            if (!filterResource(resource, shaderStage) && resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS)
            {
                pVertexInputs[j].size = resource->size;
                pVertexInputs[j].name = pCurrentName;
                pVertexInputs[j].name_size = resource->name_size;
                // we dont own the names memory we need to copy it to the name pool
                memcpy(pCurrentName, resource->name, resource->name_size);
                pCurrentName += resource->name_size + 1;
                ++j;
            }
        }
    }

    DestroyCrossCompiler(&cc);

    // all refection structs should be built now
    pOutReflection->mShaderStage = shaderStage;

    pOutReflection->pNamePool = namePool;
    pOutReflection->mNamePoolSize = namePoolSize;

    pOutReflection->pVertexInputs = pVertexInputs;
    pOutReflection->mVertexInputsCount = vertexInputCount;
}
#endif // #ifdef VULKAN
