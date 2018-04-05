/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

#include "AppPanini.h"

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"
#include "../../Common_3/Renderer/GpuProfiler.h"

#include "../../Common_3/OS/Interfaces/IUIManager.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"
/************************************************************************/
/* HELPER FUNCTIONS
/************************************************************************/
void createTessellatedQuadBuffers(Buffer** ppVertexBuffer, Buffer** ppIndexBuffer, unsigned tessellationX, unsigned tessellationY)
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
	const unsigned m = tessellationX + 1;
	const unsigned n = tessellationY + 1;
	for (unsigned i = 0; i < n; ++i)
	{
		const float y = i * dy - 1.0f;		// offset w/ -1.0f :  [0,2]->[-1,1]
		for (unsigned j = 0; j < m; ++j)
		{
			const float x = j * dx - 1.0f;	// offset w/ -1.0f :  [0,2]->[-1,1]
			vertices[i*m + j] = vec4(x, y, 0, 1);
		}
	}

	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbDesc.mDesc.mSize = vertices.size() * sizeof(vec4);
	vbDesc.mDesc.mVertexStride = sizeof(vec4);
	vbDesc.pData = vertices.data();
	vbDesc.ppBuffer = ppVertexBuffer;
	addResource(&vbDesc);

	// Tessellate the quad
	tinystl::vector<uint16_t> indices(numQuads * 6);
	//	  A	+------+ B
	//		|	 / |
	//		|	/  |
	//		|  /   |
	//		| /	   |
	//		|/	   |
	//	  C	+------+ D
	//
	//	A	: V(i  , j  )
	//	B	: V(i  , j+1)
	//	C	: V(i+1, j  )
	//	D	: V(i+1, j+1)
	//
	//	ABC	: (i*n +j    , i*n + j+1, (i+1)*n + j  )
	//	CBD : ((i+1)*n +j, i*n + j+1, (i+1)*n + j+1)
	unsigned quad = 0;
	for (unsigned i = 0; i < tessellationY; ++i)
	{
		for (unsigned j = 0; j < tessellationX; ++j)
		{
			indices[quad * 6 + 0] = (uint16_t)(i*m + j);
			indices[quad * 6 + 1] = (uint16_t)(i*m + j + 1);
			indices[quad * 6 + 2] = (uint16_t)((i + 1)*m + j);
			indices[quad * 6 + 3] = (uint16_t)((i + 1)*m + j);
			indices[quad * 6 + 4] = (uint16_t)(i*m + j + 1);
			indices[quad * 6 + 5] = (uint16_t)((i + 1)*m + j + 1);
			quad++;
		}
	}

	BufferLoadDesc ibDesc = {};
	ibDesc.mDesc.mUsage = BUFFER_USAGE_INDEX;
	ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	ibDesc.mDesc.mSize = indices.size() * sizeof(uint16_t);
	ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
	ibDesc.pData = indices.data();
	ibDesc.ppBuffer = ppIndexBuffer;
	addResource(&ibDesc);
}
/************************************************************************/
/* INTERFACE FUNCTIONS
/************************************************************************/
bool AppPanini::Init(Renderer* renderer)
{
	pRenderer = renderer;
	// SHADER
	//----------------------------------------------------------------------------------------------------------------
	ShaderLoadDesc paniniPass = {};
	paniniPass.mStages[0] = { "panini_projection.vert", NULL, 0, FSR_SrcShaders_Common };
	paniniPass.mStages[1] = { "panini_projection.frag", NULL, 0, FSR_SrcShaders_Common };
	addShader(pRenderer, &paniniPass, &pShaderPanini);

	// SAMPLERS & STATES
	//----------------------------------------------------------------------------------------------------------------
	addSampler(pRenderer, &pSamplerTrilinearAniso, FILTER_TRILINEAR_ANISO, FILTER_BILINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0.0f, 8.0f);
	addRasterizerState(&pRasterizerStateCullNone, CULL_MODE_NONE);
	addDepthState(pRenderer, &pDepthStateDisable, false, false);


	// ROOT SIGNATURE
	//----------------------------------------------------------------------------------------------------------------
	RootSignatureDesc paninniRootDesc = {};

	paninniRootDesc.mStaticSamplers["uSampler"] = pSamplerTrilinearAniso;
	addRootSignature(pRenderer, 1, &pShaderPanini, &pRootSignaturePaniniPostProcess, &paninniRootDesc);

	createTessellatedQuadBuffers(&pVertexBufferTessellatedQuad, &pIndexBufferTessellatedQuad, gPaniniDistortionTessellation[0], gPaniniDistortionTessellation[1]);

	return true;
}

void AppPanini::Exit()
{
	removeShader(pRenderer, pShaderPanini);

	removeSampler(pRenderer, pSamplerTrilinearAniso);
	removeRasterizerState(pRasterizerStateCullNone);
	removeDepthState(pDepthStateDisable);

	removeRootSignature(pRenderer, pRootSignaturePaniniPostProcess);

	removeResource(pVertexBufferTessellatedQuad);
	removeResource(pIndexBufferTessellatedQuad);
}

bool AppPanini::Load(RenderTarget** rts)
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

void AppPanini::Unload()
{
	ASSERT(pPipelinePaniniPostProcess);
	removePipeline(pRenderer, pPipelinePaniniPostProcess);
}

void AppPanini::Draw(Cmd* cmd)
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
	const uint32_t numIndices = gPaniniDistortionTessellation[0] * gPaniniDistortionTessellation[1] * 6;
	cmdBindIndexBuffer(cmd, pIndexBufferTessellatedQuad);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferTessellatedQuad);
	cmdDrawIndexed(cmd, numIndices, 0);
}

void AppPanini::SetSourceTexture(Texture* pTex)
{
	ASSERT(pTex);
	ASSERT(pTex->mDesc.mSampleCount == SAMPLE_COUNT_1 && "Panini Projection does not support MSAA Input Textures");
	pSourceTexture = pTex;
}
