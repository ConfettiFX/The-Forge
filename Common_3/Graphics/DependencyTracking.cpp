/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "Interfaces/IGraphics.h"

#ifdef ENABLE_DEPENDENCY_TRACKER

#include "../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

void removeShaderDependencies(Shader* pShaderProgram)
{
	for (int rootSignatureIndex = 0; rootSignatureIndex < hmlen(pShaderProgram->pRootSignatureDependencies); ++rootSignatureIndex)
	{
		RootSignature* pRootSignature = pShaderProgram->pRootSignatureDependencies[rootSignatureIndex].key;
		hmdel(pRootSignature->pShaderDependencies, pShaderProgram); // -V568
	}
	hmfree(pShaderProgram->pRootSignatureDependencies);
}

void removeRootSignatureDependencies(RootSignature* pRootSignature)
{
	for (int shaderIndex = 0; shaderIndex < hmlen(pRootSignature->pShaderDependencies); ++shaderIndex)
	{
		Shader* pShader = pRootSignature->pShaderDependencies[shaderIndex].key;
		hmdel(pShader->pRootSignatureDependencies, pRootSignature); // -V568
	}
	for (int pipelineIndex = 0; pipelineIndex < hmlen(pRootSignature->pPipelineDependencies); ++pipelineIndex)
	{
		Pipeline* pPipeline = pRootSignature->pPipelineDependencies[pipelineIndex].key;
		hmdel(pPipeline->pRootSignatureDependencies, pRootSignature); // -V568
	}
	hmfree(pRootSignature->pShaderDependencies);
	hmfree(pRootSignature->pPipelineDependencies);
}

void removePipelineDependencies(Pipeline* pPipeline)
{
	for (int rootSignatureIndex = 0; rootSignatureIndex < hmlen(pPipeline->pRootSignatureDependencies); ++rootSignatureIndex)
	{
		RootSignature* pRootSignature = pPipeline->pRootSignatureDependencies[rootSignatureIndex].key;
		hmdel(pRootSignature->pPipelineDependencies, pPipeline); // -V568
	}
	hmfree(pPipeline->pRootSignatureDependencies);
}

void addShaderDependencies(Shader* pShaderProgram, const BinaryShaderDesc*)
{
	pShaderProgram->pRootSignatureDependencies = NULL;
}

void addRootSignatureDependencies(RootSignature* pRootSignature, const RootSignatureDesc* pRootSignatureDesc)
{
	pRootSignature->pShaderDependencies = NULL;
	pRootSignature->pPipelineDependencies = NULL;

	for (uint32_t shaderIndex = 0; shaderIndex < pRootSignatureDesc->mShaderCount; ++shaderIndex)
	{
		Shader* pShader = pRootSignatureDesc->ppShaders[shaderIndex];
		hmput(pShader->pRootSignatureDependencies, pRootSignature, 0);
		hmput(pRootSignature->pShaderDependencies, pShader, 0);
	}
}

void addPipelineDependencies(Pipeline* pPipeline, const PipelineDesc* pPipelineDesc)
{
	pPipeline->pRootSignatureDependencies = NULL;

	switch (pPipelineDesc->mType)
	{
	case PIPELINE_TYPE_COMPUTE:
		hmput(pPipelineDesc->mGraphicsDesc.pRootSignature->pPipelineDependencies, pPipeline, 0);
		hmput(pPipeline->pRootSignatureDependencies, pPipelineDesc->mGraphicsDesc.pRootSignature, 0);
		break;

	case PIPELINE_TYPE_GRAPHICS:
		hmput(pPipelineDesc->mComputeDesc.pRootSignature->pPipelineDependencies, pPipeline, 0);
		hmput(pPipeline->pRootSignatureDependencies, pPipelineDesc->mComputeDesc.pRootSignature, 0);
		break;

	case PIPELINE_TYPE_RAYTRACING:
		{
			hmput(pPipelineDesc->mRaytracingDesc.pGlobalRootSignature->pPipelineDependencies, pPipeline, 0);
			hmput(pPipeline->pRootSignatureDependencies, pPipelineDesc->mRaytracingDesc.pGlobalRootSignature, 0);

			if (pPipelineDesc->mRaytracingDesc.pRayGenRootSignature != NULL)
			{
				hmput(pPipelineDesc->mRaytracingDesc.pRayGenRootSignature->pPipelineDependencies, pPipeline, 0);
				hmput(pPipeline->pRootSignatureDependencies, pPipelineDesc->mRaytracingDesc.pRayGenRootSignature, 0);
			}

			if (pPipelineDesc->mRaytracingDesc.ppMissRootSignatures != NULL)
			{
				for (uint32_t missRootSignatureIndex = 0; missRootSignatureIndex < pPipelineDesc->mRaytracingDesc.mMissShaderCount; ++missRootSignatureIndex)
				{
					RootSignature* pMissRootSignature = pPipelineDesc->mRaytracingDesc.ppMissRootSignatures[missRootSignatureIndex];
					hmput(pMissRootSignature->pPipelineDependencies, pPipeline, 0);
					hmput(pPipeline->pRootSignatureDependencies, pMissRootSignature, 0);
				}
			}

			for (uint32_t hitGroupIndex = 0; hitGroupIndex < pPipelineDesc->mRaytracingDesc.mHitGroupCount; ++hitGroupIndex)
			{
				RootSignature* pHitGroupRootSignature = pPipelineDesc->mRaytracingDesc.pHitGroups[hitGroupIndex].pRootSignature;
				if (pHitGroupRootSignature != NULL)
				{
					hmput(pHitGroupRootSignature->pPipelineDependencies, pPipeline, 0);
					hmput(pPipeline->pRootSignatureDependencies, pHitGroupRootSignature, 0);
				}
			}

			if (pPipelineDesc->mRaytracingDesc.pEmptyRootSignature != NULL)
			{
				hmput(pPipelineDesc->mRaytracingDesc.pEmptyRootSignature->pPipelineDependencies, pPipeline, 0);
				hmput(pPipeline->pRootSignatureDependencies, pPipelineDesc->mRaytracingDesc.pEmptyRootSignature, 0);
			}
		} break;

		default: ASSERT(false && "Unknown pipeline type"); break;
	}
}

#endif
