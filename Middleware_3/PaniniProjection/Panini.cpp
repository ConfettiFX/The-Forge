/*
* Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"
#include "../../Common_3/Renderer/GpuProfiler.h"

#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

FSRoot FSR_MIDDLEWARE_PANINI = FSR_Middleware2;
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

	tinystl::vector<vec4> vertices(numVertices);
	const unsigned        m = tessellationX + 1;
	const unsigned        n = tessellationY + 1;
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
	vbDesc.mDesc.mMemoryUsage =
		pRenderer->mSettings.mGpuMode == GPU_MODE_SINGLE ? RESOURCE_MEMORY_USAGE_GPU_ONLY : RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mFlags =
		pRenderer->mSettings.mGpuMode == GPU_MODE_SINGLE ? BUFFER_CREATION_FLAG_NONE : BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mDesc.mSize = vertices.size() * sizeof(vec4);
	vbDesc.mDesc.mVertexStride = sizeof(vec4);
	vbDesc.pData = vertices.data();
	vbDesc.ppBuffer = ppVertexBuffer;
	addResource(&vbDesc);

	// Tessellate the quad
	tinystl::vector<uint16_t> indices(numQuads * 6);
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
	ibDesc.mDesc.mMemoryUsage =
		pRenderer->mSettings.mGpuMode == GPU_MODE_SINGLE ? RESOURCE_MEMORY_USAGE_GPU_ONLY : RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ibDesc.mDesc.mFlags =
		pRenderer->mSettings.mGpuMode == GPU_MODE_SINGLE ? BUFFER_CREATION_FLAG_NONE : BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ibDesc.mDesc.mSize = indices.size() * sizeof(uint16_t);
	ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
	ibDesc.pData = indices.data();
	ibDesc.ppBuffer = ppIndexBuffer;
	addResource(&ibDesc);
}
/************************************************************************/
/* INTERFACE FUNCTIONS
************************************************************************/
bool Panini::Init(Renderer* renderer)
{
	pRenderer = renderer;
	// SHADER
	//----------------------------------------------------------------------------------------------------------------
	ShaderLoadDesc paniniPass = {};
	paniniPass.mStages[0] = { "panini_projection.vert", NULL, 0, FSR_MIDDLEWARE_PANINI };
	paniniPass.mStages[1] = { "panini_projection.frag", NULL, 0, FSR_MIDDLEWARE_PANINI };
	addShader(pRenderer, &paniniPass, &pShaderPanini);

	// SAMPLERS & STATES
	//----------------------------------------------------------------------------------------------------------------
	SamplerDesc samplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
								ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
	addSampler(pRenderer, &samplerDesc, &pSamplerPointWrap);

	RasterizerStateDesc rasterizerStateDesc = {};
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullNone);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	addDepthState(pRenderer, &depthStateDesc, &pDepthStateDisable);

	// ROOT SIGNATURE
	//----------------------------------------------------------------------------------------------------------------
	const char*       pStaticSamplerName = "uSampler";
	RootSignatureDesc paninniRootDesc = { &pShaderPanini, 1 };
	paninniRootDesc.mStaticSamplerCount = 1;
	paninniRootDesc.ppStaticSamplerNames = &pStaticSamplerName;
	paninniRootDesc.ppStaticSamplers = &pSamplerPointWrap;
	addRootSignature(pRenderer, &paninniRootDesc, &pRootSignaturePaniniPostProcess);

	createTessellatedQuadBuffers(
		pRenderer, &pVertexBufferTessellatedQuad, &pIndexBufferTessellatedQuad, mPaniniDistortionTessellation[0],
		mPaniniDistortionTessellation[1]);

	return true;
}

void Panini::Exit()
{
	removeShader(pRenderer, pShaderPanini);

	removeSampler(pRenderer, pSamplerPointWrap);
	removeRasterizerState(pRasterizerStateCullNone);
	removeDepthState(pDepthStateDisable);

	removeRootSignature(pRenderer, pRootSignaturePaniniPostProcess);

	removeResource(pVertexBufferTessellatedQuad);
	removeResource(pIndexBufferTessellatedQuad);
}

bool Panini::Load(RenderTarget** rts)
{
	// Vertexlayout
	VertexLayout vertexLayoutPanini = {};
	vertexLayoutPanini.mAttribCount = 1;
	vertexLayoutPanini.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutPanini.mAttribs[0].mFormat = ImageFormat::RGBA32F;
	vertexLayoutPanini.mAttribs[0].mBinding = 0;
	vertexLayoutPanini.mAttribs[0].mLocation = 0;
	vertexLayoutPanini.mAttribs[0].mOffset = 0;

	GraphicsPipelineDesc pipelineSettings = { 0 };
	pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	pipelineSettings.mRenderTargetCount = 1;
	pipelineSettings.pDepthState = pDepthStateDisable;
	pipelineSettings.pColorFormats = &rts[0]->mDesc.mFormat;
	pipelineSettings.pSrgbValues = &rts[0]->mDesc.mSrgb;
	pipelineSettings.mSampleCount = rts[0]->mDesc.mSampleCount;
	pipelineSettings.mSampleQuality = rts[0]->mDesc.mSampleQuality;
	pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	pipelineSettings.pRootSignature = pRootSignaturePaniniPostProcess;
	pipelineSettings.pShaderProgram = pShaderPanini;
	pipelineSettings.pVertexLayout = &vertexLayoutPanini;
	addPipeline(pRenderer, &pipelineSettings, &pPipelinePaniniPostProcess);

	return true;
}

void Panini::Unload()
{
	ASSERT(pPipelinePaniniPostProcess);
	removePipeline(pRenderer, pPipelinePaniniPostProcess);
}

void Panini::Draw(Cmd* cmd)
{
	ASSERT(cmd);
	ASSERT(pSourceTexture);

	//beginCmd(cmd);	// beginCmd() and endCmd() should be handled by the caller App

	// set pipeline state
	DescriptorData params[2] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pSourceTexture;
	params[1].pName = "PaniniRootConstants";
	params[1].pRootConstant = &mParams;
	cmdBindDescriptors(cmd, pRootSignaturePaniniPostProcess, 2, params);
	cmdBindPipeline(cmd, pPipelinePaniniPostProcess);

	// draw
	const uint32_t numIndices = mPaniniDistortionTessellation[0] * mPaniniDistortionTessellation[1] * 6;
	cmdBindIndexBuffer(cmd, pIndexBufferTessellatedQuad, 0);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferTessellatedQuad, NULL);
	cmdDrawIndexed(cmd, numIndices, 0, 0);
}

void Panini::SetSourceTexture(Texture* pTex)
{
	ASSERT(pTex);
	ASSERT(pTex->mDesc.mSampleCount == SAMPLE_COUNT_1 && "Panini Projection does not support MSAA Input Textures");
	pSourceTexture = pTex;
}
