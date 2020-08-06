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

#include "IRenderer.h"
#include "../OS/Interfaces/ILog.h"

#include "../OS/Interfaces/IMemory.h"

//This file contains shader reflection code that is the same for all platforms.
//We know it's the same for all platforms since it only interacts with the
// platform abstractions we created.

#define RESOURCE_NAME_CHECK
static bool ShaderResourceCmp(ShaderResource* a, ShaderResource* b)
{
	bool isSame = true;

	isSame = isSame && (a->type == b->type);
	isSame = isSame && (a->set == b->set);
	isSame = isSame && (a->reg == b->reg);

#ifdef METAL
    isSame = isSame && (a->mtlArgumentDescriptors.mArgumentIndex == b->mtlArgumentDescriptors.mArgumentIndex);
#endif
    
#ifdef RESOURCE_NAME_CHECK
	// we may not need this, the rest is enough but if we want to be super sure we can do this check
	isSame = isSame && (a->name_size == b->name_size);
	// early exit before string cmp
	if (isSame == false)
		return isSame;

	isSame = (strcmp(a->name, b->name) == 0);
#endif
	return isSame;
}

static bool ShaderVariableCmp(ShaderVariable* a, ShaderVariable* b)
{
	bool isSame = true;

	isSame = isSame && (a->offset == b->offset);
	isSame = isSame && (a->size == b->size);
	isSame = isSame && (a->name_size == b->name_size);

	// early exit before string cmp
	if (isSame == false)
		return isSame;

	isSame = (strcmp(a->name, b->name) == 0);

	return isSame;
}

void destroyShaderReflection(ShaderReflection* pReflection)
{
	if (pReflection == NULL)
		return;

	tf_free(pReflection->pNamePool);
	tf_free(pReflection->pVertexInputs);
	tf_free(pReflection->pShaderResources);
	tf_free(pReflection->pVariables);
}

void createPipelineReflection(ShaderReflection* pReflection, uint32_t stageCount, PipelineReflection* pOutReflection)
{
	//Parameter checks
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
	uint32_t        vertexStageIndex = ~0u;
	uint32_t        hullStageIndex = ~0u;
	uint32_t        domainStageIndex = ~0u;
	uint32_t        geometryStageIndex = ~0u;
	uint32_t        pixelStageIndex = ~0u;
	ShaderResource* pResources = NULL;
	uint32_t        resourceCount = 0;
	ShaderVariable* pVariables = NULL;
	uint32_t        variableCount = 0;

	//Should we be using dynamic arrays for these? Perhaps we can add std::vector
	// like functionality?
	ShaderResource* uniqueResources[512];
	ShaderStage     shaderUsage[512];
	ShaderVariable* uniqueVariable[512];
	ShaderResource* uniqueVariableParent[512];
	for (uint32_t i = 0; i < stageCount; ++i)
	{
		ShaderReflection* pSrcRef = pReflection + i;
		pOutReflection->mStageReflections[i] = *pSrcRef;

		if (pSrcRef->mShaderStage == SHADER_STAGE_VERT)
		{
			vertexStageIndex = i;
		}
#if !defined(METAL)
		else if (pSrcRef->mShaderStage == SHADER_STAGE_HULL)
		{
			hullStageIndex = i;
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
		}

		//Loop through all shader resources
		for (uint32_t j = 0; j < pSrcRef->mShaderResourceCount; ++j)
		{
			bool unique = true;

			//Go through all already added shader resources to see if this shader
			// resource was already added from a different shader stage. If we find a
			// duplicate shader resource, we add the shader stage to the shader stage
			// mask of that resource instead.
			for (uint32_t k = 0; k < resourceCount; ++k)
			{
				unique = !ShaderResourceCmp(&pSrcRef->pShaderResources[j], uniqueResources[k]);
				if (unique == false)
				{
					// update shader usage
					// NOT SURE
					//shaderUsage[k] = (ShaderStage)(shaderUsage[k] & pSrcRef->pShaderResources[j].used_stages);
					shaderUsage[k] |= pSrcRef->pShaderResources[j].used_stages;
					break;
				}
			}

			//If it's unique, we add it to the list of shader resourceas
			if (unique == true)
			{
				shaderUsage[resourceCount] = pSrcRef->pShaderResources[j].used_stages;
				uniqueResources[resourceCount] = &pSrcRef->pShaderResources[j];
				resourceCount++;
			}
		}

		//Loop through all shader variables (constant/uniform buffer members)
		for (uint32_t j = 0; j < pSrcRef->mVariableCount; ++j)
		{
			bool unique = true;
			//Go through all already added shader variables to see if this shader
			// variable was already added from a different shader stage. If we find a
			// duplicate shader variables, we don't add it.
			for (uint32_t k = 0; k < variableCount; ++k)
			{
				unique = !ShaderVariableCmp(&pSrcRef->pVariables[j], uniqueVariable[k]);
				if (unique == false)
					break;
			}

			//If it's unique we add it to the list of shader variables
			if (unique)
			{
				uniqueVariableParent[variableCount] = &pSrcRef->pShaderResources[pSrcRef->pVariables[j].parent_index];
				uniqueVariable[variableCount] = &pSrcRef->pVariables[j];
				variableCount++;
			}
		}
	}

	//Copy over the shader resources in a dynamic array of the correct size
	if (resourceCount)
	{
		pResources = (ShaderResource*)tf_calloc(resourceCount, sizeof(ShaderResource));

		for (uint32_t i = 0; i < resourceCount; ++i)
		{
			pResources[i] = *uniqueResources[i];
			pResources[i].used_stages = shaderUsage[i];
		}
	}

	//Copy over the shader variables in a dynamic array of the correct size
	if (variableCount)
	{
		pVariables = (ShaderVariable*)tf_malloc(sizeof(ShaderVariable) * variableCount);

		for (uint32_t i = 0; i < variableCount; ++i)
		{
			pVariables[i] = *uniqueVariable[i];
			ShaderResource* parentResource = uniqueVariableParent[i];
			// look for parent
			for (uint32_t j = 0; j < resourceCount; ++j)
			{
				if (ShaderResourceCmp(&pResources[j], parentResource))
				{
					pVariables[i].parent_index = j;
					break;
				}
			}
		}
	}

	// all refection structs should be built now
	pOutReflection->mShaderStages = combinedShaderStages;

	pOutReflection->mStageReflectionCount = stageCount;

	pOutReflection->mVertexStageIndex = vertexStageIndex;
	pOutReflection->mHullStageIndex = hullStageIndex;
	pOutReflection->mDomainStageIndex = domainStageIndex;
	pOutReflection->mGeometryStageIndex = geometryStageIndex;
	pOutReflection->mPixelStageIndex = pixelStageIndex;

	pOutReflection->pShaderResources = pResources;
	pOutReflection->mShaderResourceCount = resourceCount;

	pOutReflection->pVariables = pVariables;
	pOutReflection->mVariableCount = variableCount;
}

void destroyPipelineReflection(PipelineReflection* pReflection)
{
	if (pReflection == NULL)
		return;

	for (uint32_t i = 0; i < pReflection->mStageReflectionCount; ++i)
		destroyShaderReflection(&pReflection->mStageReflections[i]);

	tf_free(pReflection->pShaderResources);
	tf_free(pReflection->pVariables);
}
