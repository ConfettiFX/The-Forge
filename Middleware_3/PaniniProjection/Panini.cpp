/*
* Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include "Panini.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/IResourceLoader.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IMemory.h"

namespace eastl {
template <>
struct has_equality<vec4>: eastl::false_type
{
};
}    // namespace eastl

ResourceDirectory RD_MIDDLEWARE_PANINI = RD_MIDDLEWARE_2;
/************************************************************************/
/* HELPER FUNCTIONS
************************************************************************/
void createTessellatedQuadBuffers(
	Renderer* pRenderer, Buffer** ppVertexBuffer, Buffer** ppIndexBuffer, unsigned tessellationX, unsigned tessellationY)
{
	ASSERT(tessellationX >= 1);
	ASSERT(tessellationY >= 1);

	// full screen quad coordinates [-1, -1] to [1, 1] -> width & height = 2
	const float width = 2.0f;
	const float height = 2.0f;
	const float dx = width / tessellationX;
	const float dy = height / tessellationY;

	const int numQuads = tessellationX * tessellationY;
	const int numVertices = (tessellationX + 1) * (tessellationY + 1);

	eastl::vector<vec4> vertices(numVertices);
	const unsigned      m = tessellationX + 1;
	const unsigned      n = tessellationY + 1;
	for (unsigned i = 0; i < n; ++i)
	{
		const float y = i * dy - 1.0f;    // offset w/ -1.0f :  [0,2]->[-1,1]
		for (unsigned j = 0; j < m; ++j)
		{
			const float x = j * dx - 1.0f;    // offset w/ -1.0f :  [0,2]->[-1,1]
			vertices[i * m + j] = vec4(x, y, 0, 1);
		}
	}

	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = pRenderer->mGpuMode == GPU_MODE_SINGLE ? RESOURCE_MEMORY_USAGE_GPU_ONLY : RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mFlags = pRenderer->mGpuMode == GPU_MODE_SINGLE ? BUFFER_CREATION_FLAG_NONE : BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mDesc.mSize = vertices.size() * sizeof(vec4);
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	vbDesc.pData = vertices.data();
	vbDesc.ppBuffer = ppVertexBuffer;
	addResource(&vbDesc, NULL);

	// Tessellate the quad
	eastl::vector<uint16_t> indices(numQuads * 6);
	//	A +------+ B
	//	  |    / |
	//	  |   /  |
	//	  |  /   |
	//	  | /	 |
	//	  |/	 |
	//	C +------+ D
	//
	//  A   : V(i  , j  )
	//  B   : V(i  , j+1)
	//  C   : V(i+1, j  )
	//  D   : V(i+1, j+1)
	//
	//  ABC : (i*n +j   , i*n + j+1, (i+1)*n + j  )
	//  CBD : ((i+1)*n +j, i*n + j+1, (i+1)*n + j+1)
	unsigned quad = 0;
	for (unsigned i = 0; i < tessellationY; ++i)
	{
		for (unsigned j = 0; j < tessellationX; ++j)
		{
			indices[quad * 6 + 0] = (uint16_t)(i * m + j);
			indices[quad * 6 + 1] = (uint16_t)(i * m + j + 1);
			indices[quad * 6 + 2] = (uint16_t)((i + 1) * m + j);
			indices[quad * 6 + 3] = (uint16_t)((i + 1) * m + j);
			indices[quad * 6 + 4] = (uint16_t)(i * m + j + 1);
			indices[quad * 6 + 5] = (uint16_t)((i + 1) * m + j + 1);
			quad++;
		}
	}

	BufferLoadDesc ibDesc = {};
	ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	ibDesc.mDesc.mMemoryUsage = pRenderer->mGpuMode == GPU_MODE_SINGLE ? RESOURCE_MEMORY_USAGE_GPU_ONLY : RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ibDesc.mDesc.mFlags = pRenderer->mGpuMode == GPU_MODE_SINGLE ? BUFFER_CREATION_FLAG_NONE : BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ibDesc.mDesc.mSize = indices.size() * sizeof(uint16_t);
	ibDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	ibDesc.pData = indices.data();
	ibDesc.ppBuffer = ppIndexBuffer;
	addResource(&ibDesc, NULL);
}
/************************************************************************/
/* INTERFACE FUNCTIONS
************************************************************************/
bool Panini::Init(Renderer* renderer, PipelineCache* pCache)
{
	pRenderer = renderer;
	pPipelineCache = pCache;
	mIndex = -1;

	// SHADER
	//----------------------------------------------------------------------------------------------------------------
	ShaderLoadDesc paniniPass = {};
	paniniPass.mStages[0] = { "panini_projection.vert", NULL, 0 };
	paniniPass.mStages[1] = { "panini_projection.frag", NULL, 0 };
	addShader(pRenderer, &paniniPass, &pShader);

	// SAMPLERS & STATES
	//----------------------------------------------------------------------------------------------------------------
	SamplerDesc samplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
								ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
	addSampler(pRenderer, &samplerDesc, &pSamplerPointWrap);
	// ROOT SIGNATURE
	//----------------------------------------------------------------------------------------------------------------
	const char*       pStaticSamplerName = "uSampler";
	RootSignatureDesc paninniRootDesc = { &pShader, 1 };
	paninniRootDesc.mStaticSamplerCount = 1;
	paninniRootDesc.ppStaticSamplerNames = &pStaticSamplerName;
	paninniRootDesc.ppStaticSamplers = &pSamplerPointWrap;
	addRootSignature(pRenderer, &paninniRootDesc, &pRootSignature);

	SetMaxDraws(1);    // Create descriptor binder space that allows for 1 texture per frame by default

	createTessellatedQuadBuffers(
		pRenderer, &pVertexBufferTessellatedQuad, &pIndexBufferTessellatedQuad, mPaniniDistortionTessellation[0],
		mPaniniDistortionTessellation[1]);

	return true;
}

void Panini::Exit()
{
	removeShader(pRenderer, pShader);

	removeSampler(pRenderer, pSamplerPointWrap);

	removeDescriptorSet(pRenderer, pDescriptorSet);
	removeRootSignature(pRenderer, pRootSignature);

	removeResource(pVertexBufferTessellatedQuad);
	removeResource(pIndexBufferTessellatedQuad);

	pRenderer = 0;
	pDescriptorSet = NULL;
}

bool Panini::Load(RenderTarget** rts, uint32_t count)
{
	// Vertexlayout
	VertexLayout vertexLayoutPanini = {};
	vertexLayoutPanini.mAttribCount = 1;
	vertexLayoutPanini.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutPanini.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
	vertexLayoutPanini.mAttribs[0].mBinding = 0;
	vertexLayoutPanini.mAttribs[0].mLocation = 0;
	vertexLayoutPanini.mAttribs[0].mOffset = 0;

	RasterizerStateDesc rasterizerStateDesc = {};

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;

	PipelineDesc graphicsPipelineDesc = {};
	graphicsPipelineDesc.pCache = pPipelineCache;
	graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
	pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	pipelineSettings.mRenderTargetCount = 1;
	pipelineSettings.pDepthState = &depthStateDesc;
	pipelineSettings.pColorFormats = &rts[0]->mFormat;
	pipelineSettings.mSampleCount = rts[0]->mSampleCount;
	pipelineSettings.mSampleQuality = rts[0]->mSampleQuality;
	pipelineSettings.pRasterizerState = &rasterizerStateDesc;
	pipelineSettings.pRootSignature = pRootSignature;
	pipelineSettings.pShaderProgram = pShader;
	pipelineSettings.pVertexLayout = &vertexLayoutPanini;
	addPipeline(pRenderer, &graphicsPipelineDesc, &pPipeline);

	return true;
}

void Panini::Unload()
{
	ASSERT(pPipeline);
	removePipeline(pRenderer, pPipeline);
}

void Panini::Update(float deltaTime)
{
	if (mIndex >= mMaxDraws)
		mIndex = 0;
}

void Panini::Draw(Cmd* cmd)
{
	ASSERT(cmd);
	ASSERT(mIndex != UINT32_MAX);
	ASSERT(pDescriptorSet);

	//beginCmd(cmd);	// beginCmd() and endCmd() should be handled by the caller App

	// set pipeline state
	cmdBindPipeline(cmd, pPipeline);
	cmdBindPushConstants(cmd, pRootSignature, "PaniniRootConstants", &mParams);
	cmdBindDescriptorSet(cmd, mIndex++, pDescriptorSet);

	// draw
	const uint32_t stride = sizeof(vec4);
	const uint32_t numIndices = mPaniniDistortionTessellation[0] * mPaniniDistortionTessellation[1] * 6;
	cmdBindIndexBuffer(cmd, pIndexBufferTessellatedQuad, INDEX_TYPE_UINT16, 0);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferTessellatedQuad, &stride, NULL);
	cmdDrawIndexed(cmd, numIndices, 0, 0);
}

void Panini::SetMaxDraws(uint32_t maxDraws)
{
	if (pDescriptorSet)
	{
		removeDescriptorSet(pRenderer, pDescriptorSet);
	}

	DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, maxDraws };
	addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
	mMaxDraws = maxDraws;
}

void Panini::SetSourceTexture(Texture* pTex, uint32_t index)
{
	ASSERT(pTex);

	DescriptorData params[2] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pTex;
	updateDescriptorSet(pRenderer, index, pDescriptorSet, 1, params);
}
