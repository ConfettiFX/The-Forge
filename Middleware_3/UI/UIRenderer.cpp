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

#include "AppUI.h"
#include "Fontstash.h"
#include "NuklearGUIDriver.h"
#include "UIRenderer.h"
#include "UIShaders.h"

#include "../../Common_3/OS/Image/Image.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"
#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Core/RingBuffer.h"

#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

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
	pBuiltin3DTextShader(NULL),
	pRootSignatureTextureMesh(NULL),
	/// Default states
	pBlendAlpha(NULL),
	pDepthNone(NULL),
	pRasterizerNoCull(NULL),
	pDefaultSampler(NULL),
	/// Ring buffer for dynamic constant buffers (same buffer bound at different locations)
	pUniformRingBuffer(NULL),
	pPlainMeshRingBuffer(NULL),
	pTextureMeshRingBuffer(NULL)
{
#if defined(METAL)
	String vsPlainFile = "builtin_plain";
	String psPlainFile = "builtin_plain";
	String vsTexturedFile = "builtin_textured";
	String psTexturedRedAlphaFile = "builtin_textured_red_alpha";
	String psTexturedFile = "builtin_textured";

	String vsPlain = mtl_builtin_plain;
	String psPlain = mtl_builtin_plain;
	String vsTextured = mtl_builtin_textured;
	String psTextured = mtl_builtin_textured;
	String psTexturedRedAlpha = mtl_builtin_textured_red_alpha;
	String vsText = mtl_builtin_text;

	ShaderDesc plainShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsPlainFile, vsPlain, "VSMain" }, { psPlainFile, psPlain, "PSMain" } };
	ShaderDesc texShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTexturedFile, vsTextured, "VSMain" }, { psTexturedRedAlphaFile, psTexturedRedAlpha, "PSMain" } };
	ShaderDesc textureShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTexturedFile, vsTextured, "VSMain" }, { psTexturedFile, psTextured, "PSMain" } };
	ShaderDesc text3dShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { vsTexturedFile, vsText, "VSMain" }, { psTexturedRedAlphaFile, psTexturedRedAlpha, "PSMain" } };
	addShader(pRenderer, &plainShader, &pBuiltinPlainShader);
	addShader(pRenderer, &texShader, &pBuiltinTextShader);
	addShader(pRenderer, &textureShader, &pBuiltinTextureShader);
	addShader(pRenderer, &text3dShader, &pBuiltin3DTextShader);
#elif defined(DIRECT3D12) || defined(VULKAN)
	char* pPlainVert = NULL; uint32_t plainVertSize = 0;
	char* pPlainFrag = NULL; uint32_t plainFragSize = 0;
	char* pTextureVert = NULL; uint32_t textureVertSize = 0;
	char* pTextureFrag = NULL; uint32_t textureFragSize = 0;
	char* pTextureRedAlphaFrag = NULL; uint32_t textureRedAlphaFragSize = 0;
	char* pText3dVert = NULL; uint32_t text3dVertSize = 0;

	if (pRenderer->mSettings.mApi == RENDERER_API_D3D12 || pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
	{
		pPlainVert = (char*)d3d12_builtin_plain_vert; plainVertSize = sizeof(d3d12_builtin_plain_vert);
		pPlainFrag = (char*)d3d12_builtin_plain_frag; plainFragSize = sizeof(d3d12_builtin_plain_frag);
		pTextureVert = (char*)d3d12_builtin_textured_vert; textureVertSize = sizeof(d3d12_builtin_textured_vert);
		pTextureFrag = (char*)d3d12_builtin_textured_frag; textureFragSize = sizeof(d3d12_builtin_textured_frag);
		pTextureRedAlphaFrag = (char*)d3d12_builtin_textured_red_alpha_frag; textureRedAlphaFragSize = sizeof(d3d12_builtin_textured_red_alpha_frag);
		pText3dVert = (char*)d3d12_builtin_3dtext_vert; text3dVertSize = sizeof(d3d12_builtin_3dtext_vert);
	}
	else if (pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
	{
		pPlainVert = (char*)vk_builtin_plain_vert; plainVertSize = sizeof(vk_builtin_plain_vert);
		pPlainFrag = (char*)vk_builtin_plain_frag; plainFragSize = sizeof(vk_builtin_plain_frag);
		pTextureVert = (char*)vk_builtin_textured_vert; textureVertSize = sizeof(vk_builtin_textured_vert);
		pTextureFrag = (char*)vk_builtin_textured_frag; textureFragSize = sizeof(vk_builtin_textured_frag);
		pTextureRedAlphaFrag = (char*)vk_builtin_textured_red_alpha_frag; textureRedAlphaFragSize = sizeof(vk_builtin_textured_red_alpha_frag);
		pText3dVert = (char*)vk_builtin_3Dtext_vert; text3dVertSize = sizeof(vk_builtin_3Dtext_vert);
	}

	BinaryShaderDesc plainShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ (char*)pPlainVert, plainVertSize },{ (char*)pPlainFrag, plainFragSize } };
	BinaryShaderDesc texShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ (char*)pTextureVert, textureVertSize },{ (char*)pTextureRedAlphaFrag, textureRedAlphaFragSize } };
	BinaryShaderDesc textureShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ (char*)pTextureVert, textureVertSize },{ (char*)pTextureFrag, textureFragSize } };
	BinaryShaderDesc textShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ (char*)pText3dVert, text3dVertSize },{ (char*)pTextureRedAlphaFrag, textureRedAlphaFragSize } };
	addShaderBinary(pRenderer, &plainShader, &pBuiltinPlainShader);
	addShaderBinary(pRenderer, &texShader, &pBuiltinTextShader);
	addShaderBinary(pRenderer, &textureShader, &pBuiltinTextureShader);
	addShaderBinary(pRenderer, &textShaderDesc, &pBuiltin3DTextShader);
	
#endif

	SamplerDesc samplerDesc = {
		FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST,
		ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
	};
	addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mSrcFactor = BC_SRC_ALPHA;
	blendStateDesc.mDstFactor = BC_ONE_MINUS_SRC_ALPHA;
	blendStateDesc.mSrcAlphaFactor = BC_SRC_ALPHA;
	blendStateDesc.mDstAlphaFactor = BC_ONE_MINUS_SRC_ALPHA;
	blendStateDesc.mMask = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	addBlendState(pRenderer, &blendStateDesc, &pBlendAlpha);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	addDepthState(pRenderer, &depthStateDesc, &pDepthNone);
	
	DepthStateDesc depthStateEnableDesc = {};
	depthStateEnableDesc.mDepthTest = true;
	depthStateEnableDesc.mDepthWrite = true;
	depthStateEnableDesc.mDepthFunc = CMP_LEQUAL;
	addDepthState(pRenderer, &depthStateEnableDesc, &pDepthEnable);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
	rasterizerStateDesc.mScissor = true;
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerNoCull);

	BufferDesc vbDesc = {};
	vbDesc.mUsage = BUFFER_USAGE_VERTEX;
	vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mSize = gMaxVerticesPerMesh * sizeof(float2);
	vbDesc.mVertexStride = sizeof(float2);
	vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	addMeshRingBuffer(pRenderer, gMaxDrawCallsPerFrame, &vbDesc, NULL, &pPlainMeshRingBuffer);

	vbDesc.mSize = gMaxVerticesPerMesh * sizeof(TexVertex);
	vbDesc.mVertexStride = sizeof(TexVertex);
	addMeshRingBuffer(pRenderer, gMaxDrawCallsPerFrame, &vbDesc, NULL, &pTextureMeshRingBuffer);

	addUniformRingBuffer(pRenderer, gMaxDrawCallsPerFrame * 2 * (uint32_t)pRenderer->pActiveGpuSettings->mUniformBufferAlignment, &pUniformRingBuffer);

	RootSignatureDesc plainRootDesc = { &pBuiltinPlainShader, 1 };
	RootSignatureDesc textureRootDesc = { &pBuiltinTextShader,1 };
#if defined(VULKAN)
	const char* pDynamicUniformBuffers[] = { "uniformBlockVS", "uniformBlockPS" };
	plainRootDesc.mDynamicUniformBufferCount = 2;
	plainRootDesc.ppDynamicUniformBufferNames = pDynamicUniformBuffers;
	textureRootDesc.mDynamicUniformBufferCount = 2;
	textureRootDesc.ppDynamicUniformBufferNames = pDynamicUniformBuffers;
#endif
	const char* pStaticSamplers[] = { "uSampler0" };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplers;
	textureRootDesc.ppStaticSamplers = &pDefaultSampler;

	addRootSignature(pRenderer, &plainRootDesc, &pRootSignaturePlainMesh);
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignatureTextureMesh);

	registerWindowResizeEvent(onWindowResize);
}

UIRenderer::~UIRenderer()
{
	// cleanup fonts
	for (int i = 0; i < mFontStashes.size(); ++i)
	{
		mFontStashes[i]->~Fontstash();
		conf_free(mFontStashes[i]);
		mFontStashes[i] = NULL;
	}

	removeShader(pRenderer, pBuiltinPlainShader);
	removeShader(pRenderer, pBuiltinTextShader);
	removeShader(pRenderer, pBuiltinTextureShader);
	removeShader(pRenderer, pBuiltin3DTextShader);

	for (PipelineMapNode& node : mPipelinePlainMesh)
	{
		uint64_t hash = node.first;
		for (uint32_t i = 0; i < PrimitiveTopology::PRIMITIVE_TOPO_COUNT; ++i)
		{
			if (i == PRIMITIVE_TOPO_PATCH_LIST)
				continue;

			removePipeline(pRenderer, mPipelinePlainMesh[hash][i]);
			removePipeline(pRenderer, mPipelineTextMesh[hash][i]);
			removePipeline(pRenderer, mPipeline3DTextMesh[hash][i]);
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
	removeBlendState(pBlendAlpha);
	removeDepthState(pDepthEnable);
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

void UIRenderer::beginRender(uint32_t w, uint32_t h,
	uint32_t outputFormatCount, ImageFormat::Enum* outputFormats, bool* srgbValues,
	ImageFormat::Enum depthStencilFormat, SampleCount sampleCount, uint32_t sampleQuality)
{
	
	gWindowWidth = w;
	gWindowHeight = h;
	uint64_t hash = 0;
	const uint32_t numHashValues = (outputFormatCount * 2) + 3;
	uint32_t* values = (uint32_t*)alloca(numHashValues * sizeof(uint32_t));
	for (uint32_t i = 0; i < outputFormatCount; ++i)
	{
		values[i * 2] = outputFormats[i];
		values[i * 2 + 1] = srgbValues[i];
	}
	values[outputFormatCount * 2] = depthStencilFormat;
	values[outputFormatCount * 2 + 1] = sampleCount;
	values[outputFormatCount * 2 + 2] = sampleQuality;
	hash = tinystl::hash_state(values, numHashValues, hash);

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
		pipelineDesc.mDepthStencilFormat = depthStencilFormat;
		pipelineDesc.pRasterizerState = pRasterizerNoCull;
		pipelineDesc.pVertexLayout = &vertexLayout;
		pipelineDesc.mRenderTargetCount = outputFormatCount;
		pipelineDesc.pColorFormats = outputFormats;
		pipelineDesc.pSrgbValues = srgbValues;
		pipelineDesc.mSampleCount = sampleCount;
		pipelineDesc.mSampleCount = sampleCount;
		pipelineDesc.mSampleQuality = sampleQuality;

		PipelineVector pipelinePlainMesh = PipelineVector(PrimitiveTopology::PRIMITIVE_TOPO_COUNT);
		PipelineVector pipelineTextMesh = PipelineVector(PrimitiveTopology::PRIMITIVE_TOPO_COUNT);
		PipelineVector pipeline3DTextMesh = PipelineVector(PrimitiveTopology::PRIMITIVE_TOPO_COUNT);
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
			
			pipelineDesc.pDepthState = pDepthEnable;
			pipelineDesc.pShaderProgram = pBuiltin3DTextShader;
			pipelineDesc.pRootSignature = pRootSignatureTextureMesh;
			vertexLayout.mAttribCount = 2;
			addPipeline(pRenderer, &pipelineDesc, &pipeline3DTextMesh[i]);
			
			pipelineDesc.pDepthState = pDepthNone;
			pipelineDesc.pShaderProgram = pBuiltinTextureShader;
			addPipeline(pRenderer, &pipelineDesc, &pipelineTextureMesh[i]);
		}

		pCurrentPipelinePlainMesh = &mPipelinePlainMesh.insert({ hash, pipelinePlainMesh }).first->second;
		pCurrentPipelineTextMesh = &mPipelineTextMesh.insert({ hash, pipelineTextMesh }).first->second;
		pCurrentPipeline3DTextMesh = &mPipeline3DTextMesh.insert({ hash, pipeline3DTextMesh }).first->second;
		pCurrentPipelineTextureMesh = &mPipelineTextureMesh.insert({ hash, pipelineTextureMesh }).first->second;
	}
	else
	{
		pCurrentPipelinePlainMesh = &mPipelinePlainMesh[hash];
		pCurrentPipelineTextMesh = &mPipelineTextMesh[hash];
		pCurrentPipeline3DTextMesh = &mPipeline3DTextMesh[hash];
		pCurrentPipelineTextureMesh = &mPipelineTextureMesh[hash];
	}
}

void UIRenderer::reset()
{
}

void UIRenderer::onWindowResize(const struct WindowResizeEventData* pData)
{
	gWindowWidth = getRectWidth(pData->rect);
	gWindowHeight = getRectHeight(pData->rect);
}

uint32_t UIRenderer::addFontstash(uint32_t width, uint32_t height)
{
	mFontStashes.push_back(conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), this, (int)width, (int)height));
	return (uint32_t)mFontStashes.size() - 1;
}

Fontstash* UIRenderer::getFontstash(uint32_t fontID)
{
	if ((uint32_t)mFontStashes.size() > fontID)
	{
		return mFontStashes[fontID];
	}
	return NULL;
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


void UIRenderer::drawTexturedR8AsAlpha(Cmd* pCmd, PrimitiveTopology primitives, TexVertex *vertices, const uint32_t nVertices, Texture* texture, const float4* color)
{
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");

	uint32_t vertexDataSize = sizeof(TexVertex) * nVertices;
	float4 scaleBias2D(2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f);
	float uniBuffer[6] = { scaleBias2D.getX(), scaleBias2D.getY(), scaleBias2D.getZ(), scaleBias2D.getW(), (float)texture->mDesc.mWidth, (float)texture->mDesc.mHeight };

	Buffer* buffer = getVertexBuffer(pTextureMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(uniBuffer));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*color));

	BufferUpdateDesc vbUpdate = { buffer, vertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, uniBuffer, 0, vs.mOffset, sizeof(uniBuffer) };
	updateResource(&updateDesc);
	updateDesc = { ps.pUniformBuffer, color, 0, ps.mOffset, sizeof(*color) };
	updateResource(&updateDesc);

	DescriptorData params[3] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].pOffsets = &vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].pOffsets = &ps.mOffset;
	params[2].pName = "uTex0";
	params[2].ppTextures = &texture;
	cmdBindPipeline(pCmd, pCurrentPipelineTextMesh->operator[](primitives));
	cmdBindDescriptors(pCmd, pRootSignatureTextureMesh, 3, params);
	cmdBindVertexBuffer(pCmd, 1, &buffer);
	cmdDraw(pCmd, nVertices, 0);
}

void UIRenderer::drawPlain(Cmd* pCmd, PrimitiveTopology primitives, float2* vertices, const uint32_t nVertices, const float4* color)
{
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");

	uint32_t vertexDataSize = sizeof(float2) * nVertices;
	float data[4] = { 2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f };

	Buffer* buffer = getVertexBuffer(pPlainMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(data));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*color));

	BufferUpdateDesc vbUpdate = { buffer, vertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, data, 0, vs.mOffset, sizeof(data) };
	updateResource(&updateDesc);
	updateDesc = { ps.pUniformBuffer, color, 0, ps.mOffset, sizeof(*color) };
	updateResource(&updateDesc);

	DescriptorData params[2] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].pOffsets = &vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].pOffsets = &ps.mOffset;

	cmdBindPipeline(pCmd, pCurrentPipelinePlainMesh->operator[](primitives));
	cmdBindDescriptors(pCmd, pRootSignaturePlainMesh, 2, params);
	cmdBindVertexBuffer(pCmd, 1, &buffer);
	cmdDraw(pCmd, nVertices, 0);
}

void UIRenderer::drawTextured(Cmd* pCmd, PrimitiveTopology primitives, TexVertex* vertices, const uint32_t nVertices, Texture* texture, const float4* color)
{
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");

	uint32_t vertexDataSize = sizeof(TexVertex) * nVertices;
	float4 scaleBias2D(2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f);
	float uniBuffer[6] = { scaleBias2D.getX(), scaleBias2D.getY(), scaleBias2D.getZ(), scaleBias2D.getW(), (float)texture->mDesc.mWidth, (float)texture->mDesc.mHeight };

	Buffer* buffer = getVertexBuffer(pTextureMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(uniBuffer));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*color));

	BufferUpdateDesc vbUpdate = { buffer, vertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, uniBuffer, 0, vs.mOffset, sizeof(uniBuffer) };
	updateResource(&updateDesc);
	updateDesc = { ps.pUniformBuffer, color, 0, ps.mOffset, sizeof(*color) };
	updateResource(&updateDesc);

	DescriptorData params[3] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].pOffsets = &vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].pOffsets = &ps.mOffset;
	params[2].pName = "uTex0";
	params[2].ppTextures = &texture;
	cmdBindPipeline(pCmd, pCurrentPipelineTextureMesh->operator[](primitives));
	cmdBindDescriptors(pCmd, pRootSignatureTextureMesh, 3, params);
	cmdBindVertexBuffer(pCmd, 1, &buffer);
	cmdDraw(pCmd, nVertices, 0);
}

void UIRenderer::setScissor(Cmd* pCmd, const RectDesc* rect)
{
	cmdSetScissor(pCmd, max(0, rect->left), max(0, rect->top), getRectWidth(*rect), getRectHeight(*rect));
}

void UIRenderer::drawTexturedR8AsAlpha(Cmd* pCmd, PrimitiveTopology primitives, TexVertex *vertices, const uint32_t nVertices, Texture* texture, const float4* color,const mat4& projView , const mat4& worldMat) 
{
	
	ASSERT(primitives != PRIMITIVE_TOPO_PATCH_LIST && "Primitive type not supported for UI rendering");
	
	uint32_t vertexDataSize = sizeof(TexVertex) * nVertices;
	vec4 pos = vec4(-2.0f / (float)gWindowWidth, -2.0f / (float)gWindowHeight, -1.0f, 1.0f);

	uniformBlockVS uniBuffer;
	uniBuffer.scaleBias = pos;
	uniBuffer.TextureSize = vec2((float)texture->mDesc.mWidth, (float)texture->mDesc.mHeight);
	uniBuffer.mProjView = projView;
	uniBuffer.mWorldMat = worldMat;	

	Buffer* buffer = getVertexBuffer(pTextureMeshRingBuffer);
	UniformBufferOffset vs = getUniformBufferOffset(pUniformRingBuffer, sizeof(uniBuffer));
	UniformBufferOffset ps = getUniformBufferOffset(pUniformRingBuffer, sizeof(*color));

	BufferUpdateDesc vbUpdate = { buffer, vertices, 0, 0, vertexDataSize };
	updateResource(&vbUpdate);
	
	BufferUpdateDesc updateDesc = { vs.pUniformBuffer, &uniBuffer, 0, vs.mOffset, sizeof(uniBuffer) };
	updateResource(&updateDesc);
	
	updateDesc = { ps.pUniformBuffer, color, 0, ps.mOffset, sizeof(*color) };
	updateResource(&updateDesc);

	DescriptorData params[3] = {};
	params[0].pName = "uniformBlockVS";
	params[0].ppBuffers = &vs.pUniformBuffer;
	params[0].pOffsets = &vs.mOffset;
	params[1].pName = "uniformBlockPS";
	params[1].ppBuffers = &ps.pUniformBuffer;
	params[1].pOffsets = &ps.mOffset;
	params[2].pName = "uTex0";
	params[2].ppTextures = &texture;
	cmdBindDescriptors(pCmd, pRootSignatureTextureMesh, 3, params);
	cmdBindPipeline(pCmd, pCurrentPipeline3DTextMesh->operator[](primitives));
	cmdBindVertexBuffer(pCmd, 1, &buffer);
	cmdDraw(pCmd,nVertices, 0);

}