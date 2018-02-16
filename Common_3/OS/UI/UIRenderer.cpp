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

#include "../Interfaces/IUIManager.h"

#include "Fontstash.h"
#include "UI.h"
#include "NuklearGUIDriver.h"
#include "UIRenderer.h"
#include "UIShaders.h"

#include "../Image/Image.h"
#include "../../Renderer/IRenderer.h"
#include "../../Renderer/GpuProfiler.h"
#include "../../Renderer/ResourceLoader.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../Core/RingBuffer.h"

#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../Interfaces/IMemoryManager.h"

#define MAX_UNIFORM_BUFFER_SIZE 65536U

static const uint32_t gMaxDrawCallsPerFrame = 1024;
static const uint32_t gMaxVerticesPerMesh = 1024;

static uint32_t gWindowWidth = 0;
static uint32_t gWindowHeight = 0;


/************************************************************************
** UI RENDERER
************************************************************************/
UIRenderer::UIRenderer(Renderer* renderer) :
	pRenderer(renderer),
	/// Plain mesh pipeline data
	pBuiltinPlainShader(NULL),
	pRootSignaturePlainMesh(NULL),
	/// Texture mesh pipeline data
	pBuiltinTextShader(NULL),
	pBuiltinTextureShader(NULL),
	pRootSignatureTextureMesh(NULL),
	/// Default states
	pBlendAlpha(NULL),
	pDepthNone(NULL),
	pRasterizerNoCull(NULL),
	pDefaultSampler(NULL),
	pPointSampler(NULL),
	/// Ring buffer for dynamic constant buffers (same buffer bound at different locations)
	pUniformRingBuffer(NULL),
	pPlainMeshRingBuffer(NULL),
	pTextureMeshRingBuffer(NULL),
	pCurrentRootSignature(NULL),
	pCurrentCmd(NULL)
{
#if defined(METAL)
	String vsPlainFile = "builtin_plain";
	String psPlainFile = "builtin_plain";
	String vsTexturedFile = "builtin_textured";
	String psTexturedRedAlphaFile = "builtin_textured_red_alpha";
	String psTexturedFile = "builtin_textured";

	String vsPlain = builtin_plain;
	String psPlain = builtin_plain;
	String vsTextured = builtin_textured;
	String psTextured = builtin_textured;
	String psTexturedRedAlpha = builtin_textured_red_alpha;

	ShaderDesc plainShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsPlainFile, vsPlain, "VSMain" }, { psPlainFile, psPlain, "PSMain" } };
	ShaderDesc texShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTexturedFile, vsTextured, "VSMain" }, { psTexturedRedAlphaFile, psTexturedRedAlpha, "PSMain" } };
	ShaderDesc textureShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTexturedFile, vsTextured, "VSMain" }, { psTexturedFile, psTextured, "PSMain" } };
#elif defined(DIRECT3D12) || defined(VULKAN)
	tinystl::vector<char> vsPlain(sizeof(builtin_plain_vert)); memcpy(vsPlain.data(), builtin_plain_vert, sizeof(builtin_plain_vert));
	tinystl::vector<char> psPlain(sizeof(builtin_plain_frag)); memcpy(psPlain.data(), builtin_plain_frag, sizeof(builtin_plain_frag));

	tinystl::vector<char> vsTextured(sizeof(builtin_textured_vert)); memcpy(vsTextured.data(), builtin_textured_vert, sizeof(builtin_textured_vert));
	tinystl::vector<char> psTextured(sizeof(builtin_textured_frag)); memcpy(psTextured.data(), builtin_textured_frag, sizeof(builtin_textured_frag));
	tinystl::vector<char> psTexturedRedAlpha(sizeof(builtin_textured_red_alpha_frag)); memcpy(psTexturedRedAlpha.data(), builtin_textured_red_alpha_frag, sizeof(builtin_textured_red_alpha_frag));

	BinaryShaderDesc plainShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsPlain }, { psPlain } };
	BinaryShaderDesc texShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTextured }, { psTexturedRedAlpha } };
	BinaryShaderDesc textureShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTextured }, { psTextured } };
#endif

	addShader(pRenderer, &plainShader, &pBuiltinPlainShader);
	addShader(pRenderer, &texShader, &pBuiltinTextShader);
	addShader(pRenderer, &textureShader, &pBuiltinTextureShader);

	addSampler(pRenderer, &pDefaultSampler);
	addSampler(pRenderer, &pPointSampler, FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST);
	addBlendState(&pBlendAlpha, BlendConstant::BC_SRC_ALPHA, BC_ONE_MINUS_SRC_ALPHA, BC_SRC_ALPHA, BC_ONE_MINUS_SRC_ALPHA);
	addDepthState(pRenderer, &pDepthNone, false, false);
	addRasterizerState(&pRasterizerNoCull, CullMode::CULL_MODE_NONE, 0, 0.0f, FillMode::FILL_MODE_SOLID, false, true);

	BufferDesc vbDesc = {};
	vbDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mSize = gMaxVerticesPerMesh * sizeof(float2);
	vbDesc.mVertexStride = sizeof(float2);
	vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	addMeshRingBuffer(gMaxDrawCallsPerFrame, &vbDesc, NULL, &pPlainMeshRingBuffer);

	vbDesc.mSize = gMaxVerticesPerMesh * sizeof(TexVertex);
	vbDesc.mVertexStride = sizeof(TexVertex);
	addMeshRingBuffer(gMaxDrawCallsPerFrame, &vbDesc, NULL, &pTextureMeshRingBuffer);

	addUniformRingBuffer(pRenderer, gMaxDrawCallsPerFrame * 2 * (uint32_t)pRenderer->pActiveGpuSettings->mUniformBufferAlignment, &pUniformRingBuffer);

	RootSignatureDesc plainRootDesc = {};
	RootSignatureDesc textureRootDesc = {};
#if defined(VULKAN)
	plainRootDesc.mDynamicUniformBuffers.push_back("uniformBlockVS");
	textureRootDesc.mDynamicUniformBuffers.push_back("uniformBlockVS");
	plainRootDesc.mDynamicUniformBuffers.push_back("uniformBlockPS");
	textureRootDesc.mDynamicUniformBuffers.push_back("uniformBlockPS");
#endif
	textureRootDesc.mStaticSamplers["uSampler0"] = pDefaultSampler;

	addRootSignature(pRenderer, 1, &pBuiltinPlainShader, &pRootSignaturePlainMesh, &plainRootDesc);
	addRootSignature(pRenderer, 1, &pBuiltinTextShader, &pRootSignatureTextureMesh, &textureRootDesc);

	registerWindowResizeEvent(onWindowResize);
}

UIRenderer::~UIRenderer()
{
	// cleanup fonts
	for (int i = 0; i < mFontStashes.size(); ++i)
	{
		mFontStashes[i]->~Fontstash();
		conf_free(mFontStashes[i]);
		mFontStashes[i] = nullptr;
	}

	removeShader(pRenderer, pBuiltinPlainShader);
	removeShader(pRenderer, pBuiltinTextShader);
	removeShader(pRenderer, pBuiltinTextureShader);

	for (PipelineMapNode& node : mPipelinePlainMesh)
	{
		uint64_t hash = node.first;
		for (uint32_t i = 0; i < PrimitiveTopology::PRIMITIVE_TOPO_COUNT; ++i)
		{
			if (i == PRIMITIVE_TOPO_PATCH_LIST)
				continue;

			removePipeline(pRenderer, mPipelinePlainMesh[hash][i]);
			removePipeline(pRenderer, mPipelineTextMesh[hash][i]);
			removePipeline(pRenderer, mPipelineTextureMesh[hash][i]);
		}
	}

	removeUniformRingBuffer(pUniformRingBuffer);
	removeMeshRingBuffer(pPlainMeshRingBuffer);
	removeMeshRingBuffer(pTextureMeshRingBuffer);

	for (Texture* tex : mTextureRemoveQueue)
	{
		removeResource (tex);
	}

	removeSampler(pRenderer, pDefaultSampler);
	removeSampler(pRenderer, pPointSampler);
	removeBlendState(pBlendAlpha);
	removeDepthState(pDepthNone);
	removeRasterizerState(pRasterizerNoCull);

	removeRootSignature(pRenderer, pRootSignaturePlainMesh);
	removeRootSignature(pRenderer, pRootSignatureTextureMesh);
}

Texture* UIRenderer::addTexture(Image* image, uint32_t flags)
{
  UNREF_PARAM(flags);
	Texture* pTexture = NULL;

	TextureLoadDesc textureDesc = {};
	textureDesc.pImage = image;
	textureDesc.ppTexture = &pTexture;
	addResource (&textureDesc);

	return pTexture;
}

void UIRenderer::removeTexture(Texture* tex)
{
	mTextureRemoveQueue.emplace_back(tex);
}

void UIRenderer::beginRender(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil)
{
	uint64_t hash = 0;
	for (uint32_t i = 0; i < renderTargetCount; ++i)
	{
		gWindowWidth = ppRenderTargets[i]->mDesc.mWidth;
		gWindowHeight = ppRenderTargets[i]->mDesc.mHeight;
		hash = tinystl::hash_state(&ppRenderTargets[i]->pTexture->mTextureId, 1, hash);
	}
	if (pDepthStencil)
	{
		gWindowWidth = pDepthStencil->mDesc.mWidth;
		gWindowHeight = pDepthStencil->mDesc.mHeight;
		hash = tinystl::hash_state(&pDepthStencil->pTexture->mTextureId, 1, hash);
	}

	PipelineMapNode* pNode = mPipelineTextureMesh.find(hash).node;
	if (!pNode)
	{
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = calculateImageFormatStride(ImageFormat::RG32F);

		GraphicsPipelineDesc pipelineDesc = { 0 };
		pipelineDesc.pBlendState = pBlendAlpha;
		pipelineDesc.pDepthState = pDepthNone;
		pipelineDesc.pRasterizerState = pRasterizerNoCull;
		pipelineDesc.pVertexLayout = &vertexLayout;
		pipelineDesc.mRenderTargetCount = renderTargetCount;
		pipelineDesc.ppRenderTargets = ppRenderTargets;
		pipelineDesc.pDepthStencil = pDepthStencil;

		PipelineVector pipelinePlainMesh = PipelineVector(PrimitiveTopology::PRIMITIVE_TOPO_COUNT);
		PipelineVector pipelineTextMesh = PipelineVector(PrimitiveTopology::PRIMITIVE_TOPO_COUNT);
		PipelineVector pipelineTextureMesh = PipelineVector(PrimitiveTopology::PRIMITIVE_TOPO_COUNT);

		for (uint32_t i = 0; i < PrimitiveTopology::PRIMITIVE_TOPO_COUNT; ++i)
		{
			if (i == PRIMITIVE_TOPO_PATCH_LIST)
				continue;

			pipelineDesc.mPrimitiveTopo = (PrimitiveTopology)i;

			pipelineDesc.pShaderProgram = pBuiltinPlainShader;
			pipelineDesc.pRootSignature = pRootSignaturePlainMesh;
			vertexLayout.mAttribCount = 1;
			addPipeline(pRenderer, &pipelineDesc, &pipelinePlainMesh[i]);

			pipelineDesc.pShaderProgram = pBuiltinTextShader;
			pipelineDesc.pRootSignature = pRootSignatureTextureMesh;
			vertexLayout.mAttribCount = 2;
			addPipeline(pRenderer, &pipelineDesc, &pipelineTextMesh[i]);

			pipelineDesc.pShaderProgram = pBuiltinTextureShader;
			addPipeline(pRenderer, &pipelineDesc, &pipelineTextureMesh[i]);
		}

		pCurrentPipelinePlainMesh = &mPipelinePlainMesh.insert({ hash, pipelinePlainMesh }).first->second;
		pCurrentPipelineTextMesh = &mPipelineTextMesh.insert({ hash, pipelineTextMesh }).first->second;
		pCurrentPipelineTextureMesh = &mPipelineTextureMesh.insert({ hash, pipelineTextureMesh }).first->second;
	}
	else
	{
		pCurrentPipelinePlainMesh = &mPipelinePlainMesh[hash];
		pCurrentPipelineTextMesh = &mPipelineTextMesh[hash];
		pCurrentPipelineTextureMesh = &mPipelineTextureMesh[hash];
	}

	pCurrentCmd = pCmd;
}

void UIRenderer::reset()
{
	pCurrentRootSignature = NULL;
}

void UIRenderer::onWindowResize(const struct WindowResizeEventData* pData)
{
	gWindowWidth = getRectWidth(pData->rect);
	gWindowHeight = getRectHeight(pData->rect);
}

uint32_t UIRenderer::addFontstash(uint32_t width, uint32_t height)
{
	mFontStashes.push_back(conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), this, (int)width, (int)height));
	return mFontStashes.getCount() - 1;
}

Fontstash* UIRenderer::getFontstash(uint32_t fontID)
{
	if (mFontStashes.getCount() > fontID)
	{
		return mFontStashes[fontID];
	}
	return NULL;
}

int UIRenderer::addFont(const char * filename, const char * fontName, FSRoot root)
{
	Fontstash* pFontStash = this->getFontstash(0);
	if (!pFontStash)
	{
		LOGERRORF("Cannot add font: Fontstash haven't been initialized. Did you call addUIManagerInterface()?");
		return -1;
	}

	const int success = pFontStash->defineFont(fontName, filename, root);
	if (success < 0) 
	{
		LOGERRORF("Error loading font \"name=%s | file=%s\"", fontName, filename);
	}
	return success;
}

#if 0
void UIRenderer::removeFont(const char * fontName)
{
	Fontstash* pFontStash = this->getFontstash(0);
	if (!pFontStash)
	{
		LOGERRORF("Cannot add font: Fontstash haven't been initialized. Did you call addUIManagerInterface()?");
		return;
	}


}

void UIRenderer::removeFont(int fontID)
{
	Fontstash* pFontStash = this->getFontstash(0);
	if (!pFontStash)
	{
		LOGERRORF("Cannot add font: Fontstash haven't been initialized. Did you call addUIManagerInterface()?");
		return;
	}


}
#endif


void UIRenderer::drawTexturedR8AsAlpha(PrimitiveTopology primitives, TexVertex* pVertices, const uint32_t nVertices, Texture* pTexture, const float4* pColor)
{
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");

	uint32_t vertexDataSize = sizeof(TexVertex) * nVertices;
	float4 scaleBias2D(2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f);
	float uniBuffer[6] = { scaleBias2D.getX(), scaleBias2D.getY(), scaleBias2D.getZ(), scaleBias2D.getW(), (float)pTexture->mDesc.mWidth, (float)pTexture->mDesc.mHeight };

	Buffer* buffer = getVertexBuffer(pTextureMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(uniBuffer));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*pColor));

	BufferUpdateDesc vbUpdate = { buffer, pVertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, uniBuffer, 0, vs.mOffset, sizeof(uniBuffer) };
	updateResource(&updateDesc);
	updateDesc = { ps.pUniformBuffer, pColor, 0, ps.mOffset, sizeof(*pColor) };
	updateResource(&updateDesc);

	DescriptorData params[3] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].mOffset = vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].mOffset = ps.mOffset;
	params[2].pName = "uTex0";
	params[2].ppTextures = &pTexture;
	cmdBindPipeline(pCurrentCmd, pCurrentPipelineTextMesh->operator[](primitives));
	cmdBindDescriptors(pCurrentCmd, pRootSignatureTextureMesh, 3, params);
	cmdBindVertexBuffer(pCurrentCmd, 1, &buffer);
	cmdDraw(pCurrentCmd, nVertices, 0);
}

void UIRenderer::drawPlain(PrimitiveTopology primitives, float2* pVertices, const uint32_t nVertices, const float4* pColor)
{
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");

	uint32_t vertexDataSize = sizeof(float2) * nVertices;
	float data[4] = { 2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f };

	Buffer* buffer = getVertexBuffer(pPlainMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(data));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*pColor));

	BufferUpdateDesc vbUpdate = { buffer, pVertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, data, 0, vs.mOffset, sizeof(data) };
	updateResource(&updateDesc);
	updateDesc = { ps.pUniformBuffer, pColor, 0, ps.mOffset, sizeof(*pColor) };
	updateResource(&updateDesc);

	DescriptorData params[2] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].mOffset = vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].mOffset = ps.mOffset;

	cmdBindPipeline(pCurrentCmd, pCurrentPipelinePlainMesh->operator[](primitives));
	cmdBindDescriptors(pCurrentCmd, pRootSignaturePlainMesh, 2, params);
	cmdBindVertexBuffer(pCurrentCmd, 1, &buffer);
	cmdDraw(pCurrentCmd, nVertices, 0);
}

void UIRenderer::drawTextured(PrimitiveTopology primitives, TexVertex* pVertices, const uint32_t nVertices, Texture* pTexture, const float4* pColor)
{
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");

	uint32_t vertexDataSize = sizeof(TexVertex) * nVertices;
	float4 scaleBias2D(2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f);
	float uniBuffer[6] = { scaleBias2D.getX(), scaleBias2D.getY(), scaleBias2D.getZ(), scaleBias2D.getW(), (float)pTexture->mDesc.mWidth, (float)pTexture->mDesc.mHeight };

	Buffer* buffer = getVertexBuffer(pTextureMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(uniBuffer));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*pColor));

	BufferUpdateDesc vbUpdate = { buffer, pVertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, uniBuffer, 0, vs.mOffset, sizeof(uniBuffer) };
	updateResource(&updateDesc);
	updateDesc = { ps.pUniformBuffer, pColor, 0, ps.mOffset, sizeof(*pColor) };
	updateResource(&updateDesc);

	DescriptorData params[3] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].mOffset = vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].mOffset = ps.mOffset;
	params[2].pName = "uTex0";
	params[2].ppTextures = &pTexture;
	cmdBindPipeline(pCurrentCmd, pCurrentPipelineTextureMesh->operator[](primitives));
	cmdBindDescriptors(pCurrentCmd, pRootSignatureTextureMesh, 3, params);
	cmdBindVertexBuffer(pCurrentCmd, 1, &buffer);
	cmdDraw(pCurrentCmd, nVertices, 0);
}

void UIRenderer::setScissor(const RectDesc* pRect)
{
	cmdSetScissor(pCurrentCmd, max(0, pRect->left), max(0, pRect->top), getRectWidth(*pRect), getRectHeight(*pRect));
}
