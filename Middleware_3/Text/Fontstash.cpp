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

#ifdef USE_TEXT_PRECOMPILED_SHADERS
#include "Shaders/Compiled/fontstash2D.vert.h"
#include "Shaders/Compiled/fontstash3D.vert.h"
#include "Shaders/Compiled/fontstash.frag.h"
#endif

#include "Fontstash.h"

// include Fontstash (should be after MemoryTracking so that it also detects memory free/remove in fontstash)
#define FONTSTASH_IMPLEMENTATION
#include "../../Common_3/ThirdParty/OpenSource/Fontstash/src/fontstash.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Core/RingBuffer.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/IResourceLoader.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../Common_3/OS/Interfaces/IMemory.h"

class _Impl_FontStash
{
public:
	_Impl_FontStash()
	{
		pCurrentTexture = {};
		mWidth = 0;
		mHeight = 0;
		pContext = NULL;

		mText3D = false;
	}

	bool init(Renderer* renderer, int width_, int height_, uint32_t ringSizeBytes)
	{
		pRenderer = renderer;

		// create image
		TextureDesc desc = {};
		desc.mArraySize = 1;
		desc.mDepth = 1;
		desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		desc.mFormat = TinyImageFormat_R8_UNORM;
		desc.mHeight = height_;
		desc.mMipLevels = 1;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mStartState = RESOURCE_STATE_COMMON;
		desc.mWidth = width_;
		desc.pName = "Fontstash Texture";
		TextureLoadDesc loadDesc = {};
		loadDesc.ppTexture = &pCurrentTexture;
		loadDesc.pDesc = &desc;
		addResource(&loadDesc, NULL);

		// create FONS context
		FONSparams params;
		memset(&params, 0, sizeof(params));
		params.width = width_;
		params.height = height_;
		params.flags = (unsigned char)FONS_ZERO_TOPLEFT;
		params.renderCreate = fonsImplementationGenerateTexture;
		params.renderUpdate = fonsImplementationModifyTexture;
		params.renderDelete = fonsImplementationRemoveTexture;
		params.renderDraw = fonsImplementationRenderText;
		params.userPtr = this;

		pContext = fonsCreateInternal(&params);
		/************************************************************************/
		// Rendering resources
		/************************************************************************/
		SamplerDesc samplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

#ifdef USE_TEXT_PRECOMPILED_SHADERS
		BinaryShaderDesc binaryShaderDesc = {};
		binaryShaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
		binaryShaderDesc.mVert.mByteCodeSize = sizeof(gShaderFontstash2DVert);
		binaryShaderDesc.mVert.pByteCode = (char*)gShaderFontstash2DVert;
		binaryShaderDesc.mVert.pEntryPoint = "main";
		binaryShaderDesc.mFrag.mByteCodeSize = sizeof(gShaderFontstashFrag);
		binaryShaderDesc.mFrag.pByteCode = (char*)gShaderFontstashFrag;
		binaryShaderDesc.mFrag.pEntryPoint = "main";
		addShaderBinary(pRenderer, &binaryShaderDesc, &pShaders[0]);
		binaryShaderDesc.mVert.mByteCodeSize = sizeof(gShaderFontstash3DVert);
		binaryShaderDesc.mVert.pByteCode = (char*)gShaderFontstash3DVert;
		binaryShaderDesc.mVert.pEntryPoint = "main";
		addShaderBinary(pRenderer, &binaryShaderDesc, &pShaders[1]);
#else
		ShaderLoadDesc text2DShaderDesc = {};
		text2DShaderDesc.mStages[0] = { "fontstash2D.vert", NULL, 0, NULL };
		text2DShaderDesc.mStages[1] = { "fontstash.frag", NULL, 0, NULL};
		ShaderLoadDesc text3DShaderDesc = {};
		text3DShaderDesc.mStages[0] = { "fontstash3D.vert", NULL, 0, NULL };
		text3DShaderDesc.mStages[1] = { "fontstash.frag", NULL, 0, NULL };

		addShader(pRenderer, &text2DShaderDesc, &pShaders[0]);
		addShader(pRenderer, &text3DShaderDesc, &pShaders[1]);
#endif

		RootSignatureDesc textureRootDesc = { pShaders, 2 };
		const char* pStaticSamplers[] = { "uSampler0" };
		textureRootDesc.mStaticSamplerCount = 1;
		textureRootDesc.ppStaticSamplerNames = pStaticSamplers;
		textureRootDesc.ppStaticSamplers = &pDefaultSampler;
		addRootSignature(pRenderer, &textureRootDesc, &pRootSignature);

		addUniformGPURingBuffer(pRenderer, 65536, &pUniformRingBuffer, true);

		uint64_t size = sizeof(mat4);
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSets);
		DescriptorData setParams[2] = {};
		setParams[0].pName = "uniformBlock_rootcbv";
		setParams[0].ppBuffers = &pUniformRingBuffer->pBuffer;
		setParams[0].pSizes = &size;
		setParams[1].pName = "uTex0";
		setParams[1].ppTextures = &pCurrentTexture;
		updateDescriptorSet(pRenderer, 0, pDescriptorSets, 2, setParams);
		updateDescriptorSet(pRenderer, 1, pDescriptorSets, 2, setParams);

		BufferDesc vbDesc = {};
		vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		vbDesc.mSize = ringSizeBytes;
		vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		addGPURingBuffer(pRenderer, &vbDesc, &pMeshRingBuffer);
		/************************************************************************/
		/************************************************************************/

		return true;
	}

	void exit()
	{
		// unload fontstash context
		fonsDeleteInternal(pContext);

		removeResource(pCurrentTexture);

		// unload font buffers
		for (unsigned int i = 0; i < (uint32_t)mFontBuffers.size(); i++)
			tf_free(mFontBuffers[i]);

		removeDescriptorSet(pRenderer, pDescriptorSets);
		removeRootSignature(pRenderer, pRootSignature);

		for (uint32_t i = 0; i < 2; ++i)
		{
			removeShader(pRenderer, pShaders[i]);
		}

		removeGPURingBuffer(pMeshRingBuffer);
		removeGPURingBuffer(pUniformRingBuffer);
		removeSampler(pRenderer, pDefaultSampler);
	}

	bool load(RenderTarget** pRts, uint32_t count, PipelineCache* pCache)
	{
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(vertexLayout.mAttribs[0].mFormat) / 8;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
		blendStateDesc.mIndependentBlend = false;

		DepthStateDesc depthStateDesc[2] = {};
		depthStateDesc[0].mDepthTest = false;
		depthStateDesc[0].mDepthWrite = false;

		depthStateDesc[1].mDepthTest = true;
		depthStateDesc[1].mDepthWrite = true;
		depthStateDesc[1].mDepthFunc = CMP_LEQUAL;

		RasterizerStateDesc rasterizerStateDesc[2] = {};
		rasterizerStateDesc[0].mCullMode = CULL_MODE_NONE;
		rasterizerStateDesc[0].mScissor = true;

		rasterizerStateDesc[1].mCullMode = CULL_MODE_BACK;
		rasterizerStateDesc[1].mScissor = true;

		PipelineDesc pipelineDesc = {};
		pipelineDesc.pCache = pCache;
		pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
		pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
		pipelineDesc.mGraphicsDesc.pBlendState = &blendStateDesc;
		pipelineDesc.mGraphicsDesc.pRootSignature = pRootSignature;
		pipelineDesc.mGraphicsDesc.pVertexLayout = &vertexLayout;
		pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
		pipelineDesc.mGraphicsDesc.mSampleCount = pRts[0]->mSampleCount;
		pipelineDesc.mGraphicsDesc.mSampleQuality = pRts[0]->mSampleQuality;
		pipelineDesc.mGraphicsDesc.pColorFormats = &pRts[0]->mFormat;
		for (uint32_t i = 0; i < min(count, 2U); ++i)
		{
			pipelineDesc.mGraphicsDesc.mDepthStencilFormat = (i > 0) ? pRts[1]->mFormat : TinyImageFormat_UNDEFINED;
			pipelineDesc.mGraphicsDesc.pShaderProgram = pShaders[i];
			pipelineDesc.mGraphicsDesc.pDepthState = &depthStateDesc[i];
			pipelineDesc.mGraphicsDesc.pRasterizerState = &rasterizerStateDesc[i];
			addPipeline(pRenderer, &pipelineDesc, &pPipelines[i]);
		}

		mScaleBias = { 2.0f / (float)pRts[0]->mWidth, -2.0f / (float)pRts[0]->mHeight };

		return true;
	}

	void unload()
	{
		for (uint32_t i = 0; i < 2; ++i)
		{
			if (pPipelines[i])
				removePipeline(pRenderer, pPipelines[i]);

			pPipelines[i] = {};
		}
	}

	static int  fonsImplementationGenerateTexture(void* userPtr, int width, int height);
	static void fonsImplementationModifyTexture(void* userPtr, int* rect, const unsigned char* data);
	static void fonsImplementationRenderText(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
	static void fonsImplementationRemoveTexture(void* userPtr);

	Renderer*    pRenderer;
	FONScontext* pContext;

	const uint8_t* pPixels;
	Texture*       pCurrentTexture;
	bool           mUpdateTexture;

	uint32_t mWidth;
	uint32_t mHeight;
	float2   mScaleBias;

	eastl::vector<void*>           mFontBuffers;
	eastl::vector<uint32_t>        mFontBufferSizes;
	eastl::vector<eastl::string>   mFontNames;

	mat4 mProjView;
	mat4 mWorldMat;
	Cmd* pCmd;

	Shader*            pShaders[2];
	RootSignature*     pRootSignature;
	DescriptorSet*     pDescriptorSets;
	Pipeline*          pPipelines[2];
	/// Default states
	Sampler*             pDefaultSampler;
	GPURingBuffer*       pUniformRingBuffer;
	GPURingBuffer*       pMeshRingBuffer;
	float2               mDpiScale;
	float                mDpiScaleMin;
	bool                 mText3D;
};

bool Fontstash::init(Renderer* renderer, uint32_t width, uint32_t height, uint32_t ringSizeBytes)
{
	impl = tf_placement_new<_Impl_FontStash>(tf_calloc(1, sizeof(_Impl_FontStash)));
	impl->mDpiScale = getDpiScale();
	impl->mDpiScaleMin = min(impl->mDpiScale.x, impl->mDpiScale.y);

	width = width * (int)ceilf(impl->mDpiScale.x);
	height = height * (int)ceilf(impl->mDpiScale.y);

	bool success = impl->init(renderer, width, height, ringSizeBytes);
	m_fFontMaxSize = min(width, height) / 10.0f;    // see fontstash.h, line 1271, for fontSize calculation

	return success;
}

void Fontstash::exit()
{
	impl->exit();
	impl->~_Impl_FontStash();
	tf_free(impl);
}

bool Fontstash::load(RenderTarget** pRts, uint32_t count, PipelineCache* pCache)
{
	return impl->load(pRts, count, pCache);
}

void Fontstash::unload()
{
	impl->unload();
}

int Fontstash::defineFont(const char* identification, const char* pFontPath)
{
	FONScontext* fs = impl->pContext;
	
	FileStream fh = {};
	if (fsOpenStreamFromPath(RD_FONTS, pFontPath, FM_READ_BINARY, &fh))
	{
		ssize_t bytes = fsGetStreamFileSize(&fh);
		void*    buffer = tf_malloc(bytes);
		fsReadFromStream(&fh, buffer, bytes);

		// add buffer to font buffers for cleanup
		impl->mFontBuffers.emplace_back(buffer);
		impl->mFontBufferSizes.emplace_back((uint32_t)bytes);
		impl->mFontNames.emplace_back(pFontPath);

		fsCloseStream(&fh);

		return fonsAddFontMem(fs, identification, (unsigned char*)buffer, (int)bytes, 0);
	}

	return INT32_MAX;
}

void* Fontstash::getFontBuffer(uint32_t index)
{
	if (index < impl->mFontBuffers.size())
		return impl->mFontBuffers[index];
	return NULL;
}

uint32_t Fontstash::getFontBufferSize(uint32_t index)
{
	if (index < impl->mFontBufferSizes.size())
		return impl->mFontBufferSizes[index];
	return UINT_MAX;
}

void Fontstash::drawText(
	Cmd* pCmd, const char* message, float x, float y, int fontID, unsigned int color /*=0xffffffff*/, float size /*=16.0f*/,
	float spacing /*=3.0f*/, float blur /*=0.0f*/)
{
	impl->mText3D = false;
	impl->pCmd = pCmd;
	// clamp the font size to max size.
	// Precomputed font texture puts limitation to the maximum size.
	size = min(size, m_fFontMaxSize);

	FONScontext* fs = impl->pContext;
	fonsSetSize(fs, size * impl->mDpiScaleMin);
	fonsSetFont(fs, fontID);
	fonsSetColor(fs, color);
	fonsSetSpacing(fs, spacing * impl->mDpiScaleMin);
	fonsSetBlur(fs, blur);
	fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

	// considering the retina scaling:
	// the render target is already scaled up (w/ retina) and the (x,y) position given to this function
	// is expected to be in the render target's area. Hence, we don't scale up the position again.
	fonsDrawText(fs, x /** impl->mDpiScale.x*/, y /** impl->mDpiScale.y*/, message, NULL);
}

void Fontstash::drawText(
	Cmd* pCmd, const char* message, const mat4& projView, const mat4& worldMat, int fontID, unsigned int color /*=0xffffffff*/,
	float size /*=16.0f*/, float spacing /*=3.0f*/, float blur /*=0.0f*/)
{
	impl->mText3D = true;
	impl->mProjView = projView;
	impl->mWorldMat = worldMat;
	impl->pCmd = pCmd;
	// clamp the font size to max size.
	// Precomputed font texture puts limitation to the maximum size.
	size = min(size, m_fFontMaxSize);

	FONScontext* fs = impl->pContext;
	fonsSetSize(fs, size * impl->mDpiScaleMin);
	fonsSetFont(fs, fontID);
	fonsSetColor(fs, color);
	fonsSetSpacing(fs, spacing * impl->mDpiScaleMin);
	fonsSetBlur(fs, blur);
	fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
	fonsDrawText(fs, 0.0f, 0.0f, message, NULL);
}

float Fontstash::measureText(
	float* out_bounds, const char* message, float x, float y, int fontID, unsigned int color /*=0xffffffff*/
	,
	float size /*=16.0f*/
	,
	float spacing /*=0.0f*/
	,
	float blur /*=0.0f*/
)
{
	if (out_bounds == NULL)
		return 0;

	const int    messageLength = (int)strlen(message);
	FONScontext* fs = impl->pContext;
	fonsSetSize(fs, size * impl->mDpiScaleMin);
	fonsSetFont(fs, fontID);
	fonsSetColor(fs, color);
	fonsSetSpacing(fs, spacing * impl->mDpiScaleMin);
	fonsSetBlur(fs, blur);
	fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

	// considering the retina scaling:
	// the render target is already scaled up (w/ retina) and the (x,y) position given to this function
	// is expected to be in the render target's area. Hence, we don't scale up the position again.
	return fonsTextBounds(fs, x /** impl->mDpiScale.x*/, y /** impl->mDpiScale.y*/, message, message + messageLength, out_bounds);
}

// --  FONS renderer implementation --
int _Impl_FontStash::fonsImplementationGenerateTexture(void* userPtr, int width, int height)
{
	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;
	ctx->mWidth = width;
	ctx->mHeight = height;

	ctx->mUpdateTexture = true;

	return 1;
}

void _Impl_FontStash::fonsImplementationModifyTexture(void* userPtr, int* rect, const unsigned char* data)
{
	UNREF_PARAM(rect);

	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;

	ctx->pPixels = data;
	ctx->mUpdateTexture = true;
}

void _Impl_FontStash::fonsImplementationRenderText(
	void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	_Impl_FontStash* ctx = (_Impl_FontStash*)userPtr;
	if (!ctx->pCurrentTexture)
		return;

	Cmd* pCmd = ctx->pCmd;

	if (ctx->mUpdateTexture)
	{
		// #TODO: Investigate - Causes hang on low-mid end Android phones (tested on Samsung Galaxy A50s)
#ifndef __ANDROID__
		waitQueueIdle(pCmd->pQueue);
#endif

		SyncToken token = {};
		TextureUpdateDesc updateDesc = {};
		updateDesc.pTexture = ctx->pCurrentTexture;
		beginUpdateResource(&updateDesc);
		for (uint32_t r = 0; r < updateDesc.mRowCount; ++r)
		{
			memcpy(updateDesc.pMappedData + r * updateDesc.mDstRowStride,
				ctx->pPixels + r * updateDesc.mSrcRowStride, updateDesc.mSrcRowStride);
		}
		endUpdateResource(&updateDesc, &token);
		waitForToken(&token);

		ctx->mUpdateTexture = false;
	}

	GPURingBufferOffset buffer = getGPURingBufferOffset(ctx->pMeshRingBuffer, nverts * sizeof(float4));
	BufferUpdateDesc update = { buffer.pBuffer, buffer.mOffset };
	beginUpdateResource(&update);
	float4* vtx = (float4*)update.pMappedData;
	// build vertices
	for (int impl = 0; impl < nverts; impl++)
	{
		vtx[impl].setX(verts[impl * 2 + 0]);
		vtx[impl].setY(verts[impl * 2 + 1]);
		vtx[impl].setZ(tcoords[impl * 2 + 0]);
		vtx[impl].setW(tcoords[impl * 2 + 1]);
	}
	endUpdateResource(&update, NULL);

	// extract color
	uint8_t* colorByte = (uint8_t*)colors;
	float4 color;
	for (int i = 0; i < 4; i++)
		color[i] = ((float)colorByte[i]) / 255.0f;

	uint32_t                               pipelineIndex = ctx->mText3D ? 1 : 0;
	Pipeline*                              pPipeline = ctx->pPipelines[pipelineIndex];
	ASSERT(pPipeline);

	cmdBindPipeline(pCmd, pPipeline);

	struct UniformData
	{
		float4 color;
		float2 scaleBias;
	} data;

	data.color = color;
	data.scaleBias = ctx->mScaleBias;

	if (ctx->mText3D)
	{
		mat4 mvp = ctx->mProjView * ctx->mWorldMat;
		data.color = color;
		data.scaleBias.x = -data.scaleBias.x;

		GPURingBufferOffset uniformBlock = {};
		uniformBlock = getGPURingBufferOffset(ctx->pUniformRingBuffer, sizeof(mvp));
		BufferUpdateDesc updateDesc = { uniformBlock.pBuffer, uniformBlock.mOffset };
		beginUpdateResource(&updateDesc);
		*((mat4*)updateDesc.pMappedData) = mvp;
		endUpdateResource(&updateDesc, NULL);

		const uint64_t size = sizeof(mvp);
		const uint32_t stride = sizeof(float4);

		DescriptorData params[1] = {};
		params[0].pName = "uniformBlock_rootcbv";
		params[0].ppBuffers = &uniformBlock.pBuffer;
		params[0].pOffsets = &uniformBlock.mOffset;
		params[0].pSizes = &size;
		updateDescriptorSet(ctx->pRenderer, pipelineIndex, ctx->pDescriptorSets, 1, params);
		cmdBindDescriptorSet(pCmd, pipelineIndex, ctx->pDescriptorSets);
		cmdBindPushConstants(pCmd, ctx->pRootSignature, "uRootConstants", &data);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.mOffset);
		cmdDraw(pCmd, nverts, 0);
	}
	else
	{
		const uint32_t stride = sizeof(float4);
		cmdBindDescriptorSet(pCmd, pipelineIndex, ctx->pDescriptorSets);
		cmdBindPushConstants(pCmd, ctx->pRootSignature, "uRootConstants", &data);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &stride, &buffer.mOffset);
		cmdDraw(pCmd, nverts, 0);
	}
}

void _Impl_FontStash::fonsImplementationRemoveTexture(void* userPtr)
{
	UNREF_PARAM(userPtr);
}
