/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#ifdef VULKAN

#include "../IRenderer.h"

#include "../../Tools/SpirvTools/SpirvTools.h"
#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Interfaces/IMemory.h"

static DescriptorType sSPIRV_TO_DESCRIPTOR[SPIRV_TYPE_COUNT] = {
	DESCRIPTOR_TYPE_UNDEFINED,        DESCRIPTOR_TYPE_UNDEFINED,    DESCRIPTOR_TYPE_UNIFORM_BUFFER,  DESCRIPTOR_TYPE_RW_BUFFER,
	DESCRIPTOR_TYPE_TEXTURE,          DESCRIPTOR_TYPE_RW_TEXTURE,   DESCRIPTOR_TYPE_SAMPLER,         DESCRIPTOR_TYPE_ROOT_CONSTANT,
	DESCRIPTOR_TYPE_INPUT_ATTACHMENT, DESCRIPTOR_TYPE_TEXEL_BUFFER, DESCRIPTOR_TYPE_RW_TEXEL_BUFFER, DESCRIPTOR_TYPE_RAY_TRACING,
};

static TextureDimension sSPIRV_TO_RESOURCE_DIM[SPIRV_DIM_COUNT] = {
	TEXTURE_DIM_UNDEFINED,
	TEXTURE_DIM_UNDEFINED,
	TEXTURE_DIM_1D,
	TEXTURE_DIM_1D_ARRAY,
	TEXTURE_DIM_2D,
	TEXTURE_DIM_2D_ARRAY,
	TEXTURE_DIM_2DMS,
	TEXTURE_DIM_2DMS_ARRAY,
	TEXTURE_DIM_3D,
	TEXTURE_DIM_CUBE,
	TEXTURE_DIM_CUBE_ARRAY,
};

bool filterResouce(SPIRV_Resource* resource, ShaderStage currentStage)
{
	bool filter = false;

	// remove used resouces
	// TODO: log warning
	filter = filter || (resource->is_used == false);

	// remove stage outputs
	filter = filter || (resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_OUTPUTS);

	// remove stage inputs that are not on the vertex shader
	filter = filter || (resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS && currentStage != SHADER_STAGE_VERT);

	return filter;
}

void vk_createShaderReflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection)
{
	if (pOutReflection == NULL)
	{
		LOGF(LogLevel::eERROR, "Create Shader Refection failed. Invalid reflection output!");
		return;    // TODO: error msg
	}

	CrossCompiler cc;

	CreateCrossCompiler((const uint32_t*)shaderCode, shaderSize / sizeof(uint32_t), &cc);

	ReflectEntryPoint(&cc);
	ReflectShaderResources(&cc);
	ReflectShaderVariables(&cc);

	if (shaderStage == SHADER_STAGE_COMP)
	{
		ReflectComputeShaderWorkGroupSize(
			&cc, &pOutReflection->mNumThreadsPerGroup[0], &pOutReflection->mNumThreadsPerGroup[1], &pOutReflection->mNumThreadsPerGroup[2]);
	}
	else if (shaderStage == SHADER_STAGE_TESC)
	{
		ReflectHullShaderControlPoint(&cc, &pOutReflection->mNumControlPoint);
	}

	// lets find out the size of the name pool we need
	// also get number of resources while we are at it
	uint32_t namePoolSize = 0;
	uint32_t vertexInputCount = 0;
	uint32_t resouceCount = 0;
	uint32_t variablesCount = 0;

	namePoolSize += cc.EntryPointSize + 1;

	for (uint32_t i = 0; i < cc.ShaderResourceCount; ++i)
	{
		SPIRV_Resource* resource = cc.pShaderResouces + i;

		// filter out what we don't use
		if (!filterResouce(resource, shaderStage))
		{
			namePoolSize += resource->name_size + 1;

			if (resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS && shaderStage == SHADER_STAGE_VERT)
			{
				++vertexInputCount;
			}
			else
			{
				++resouceCount;
			}
		}
	}

	for (uint32_t i = 0; i < cc.UniformVariablesCount; ++i)
	{
		SPIRV_Variable* variable = cc.pUniformVariables + i;

		// check if parent buffer was filtered out
		bool parentFiltered = filterResouce(cc.pShaderResouces + variable->parent_index, shaderStage);

		// filter out what we don't use
		// TODO: log warning
		if (variable->is_used && !parentFiltered)
		{
			namePoolSize += variable->name_size + 1;
			++variablesCount;
		}
	}

	// we now have the size of the memory pool and number of resources
	char* namePool = NULL;
	if (namePoolSize)
		namePool = (char*)tf_calloc(namePoolSize, 1);
	char* pCurrentName = namePool;

	pOutReflection->pEntryPoint = pCurrentName;
	memcpy(pCurrentName, cc.pEntryPoint, cc.EntryPointSize);
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
			if (!filterResouce(resource, shaderStage) && resource->type == SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS)
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

	uint32_t*       indexRemap = NULL;
	ShaderResource* pResources = NULL;
	// continue with resources
	if (resouceCount)
	{
		indexRemap = (uint32_t*)tf_malloc(sizeof(uint32_t) * cc.ShaderResourceCount);
		pResources = (ShaderResource*)tf_malloc(sizeof(ShaderResource) * resouceCount);

		uint32_t j = 0;
		for (uint32_t i = 0; i < cc.ShaderResourceCount; ++i)
		{
			// set index remap
			indexRemap[i] = (uint32_t)-1;

			SPIRV_Resource* resource = cc.pShaderResouces + i;

			// filter out what we don't use
			if (!filterResouce(resource, shaderStage) && resource->type != SPIRV_Resource_Type::SPIRV_TYPE_STAGE_INPUTS)
			{
				// set new index
				indexRemap[i] = j;

				pResources[j].type = sSPIRV_TO_DESCRIPTOR[resource->type];
				pResources[j].set = resource->set;
				pResources[j].reg = resource->binding;
				pResources[j].size = resource->size;
				pResources[j].used_stages = shaderStage;

				pResources[j].name = pCurrentName;
				pResources[j].name_size = resource->name_size;
				pResources[j].dim = sSPIRV_TO_RESOURCE_DIM[resource->dim];
				// we dont own the names memory we need to copy it to the name pool
				memcpy(pCurrentName, resource->name, resource->name_size);
				pCurrentName += resource->name_size + 1;
				++j;
			}
		}
	}

	ShaderVariable* pVariables = NULL;
	// now do variables
	if (variablesCount)
	{
		pVariables = (ShaderVariable*)tf_malloc(sizeof(ShaderVariable) * variablesCount);

		uint32_t j = 0;
		for (uint32_t i = 0; i < cc.UniformVariablesCount; ++i)
		{
			SPIRV_Variable* variable = cc.pUniformVariables + i;

			// check if parent buffer was filtered out
			bool parentFiltered = filterResouce(cc.pShaderResouces + variable->parent_index, shaderStage);

			// filter out what we don't use
			if (variable->is_used && !parentFiltered)
			{
				pVariables[j].offset = variable->offset;
				pVariables[j].size = variable->size;
				pVariables[j].parent_index = indexRemap[variable->parent_index];

				pVariables[j].name = pCurrentName;
				pVariables[j].name_size = variable->name_size;
				// we dont own the names memory we need to copy it to the name pool
				memcpy(pCurrentName, variable->name, variable->name_size);
				pCurrentName += variable->name_size + 1;
				++j;
			}
		}
	}

	tf_free(indexRemap);
	DestroyCrossCompiler(&cc);

	// all refection structs should be built now
	pOutReflection->mShaderStage = shaderStage;

	pOutReflection->pNamePool = namePool;
	pOutReflection->mNamePoolSize = namePoolSize;

	pOutReflection->pVertexInputs = pVertexInputs;
	pOutReflection->mVertexInputsCount = vertexInputCount;

	pOutReflection->pShaderResources = pResources;
	pOutReflection->mShaderResourceCount = resouceCount;

	pOutReflection->pVariables = pVariables;
	pOutReflection->mVariableCount = variablesCount;
}
#endif    // #ifdef VULKAN