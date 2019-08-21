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

#include "Fontstash.h"

// include Fontstash (should be after MemoryTracking so that it also detects memory free/remove in fontstash)
#define FONTSTASH_IMPLEMENTATION
#include "../../Common_3/ThirdParty/OpenSource/Fontstash/src/fontstash.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Core/RingBuffer.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../Common_3/OS/Interfaces/IMemory.h"

FSRoot FSR_MIDDLEWARE_TEXT = FSR_Middleware0;

class _Impl_FontStash
{
public:
	_Impl_FontStash()
	{
		pCurrentTexture = NULL;
		mWidth = 0;
		mHeight = 0;
		pContext = NULL;

		mText3D = false;
	}

	bool init(Renderer* renderer, int width_, int height_)
	{
		pRenderer = renderer;

		// create image
		TextureDesc desc = {};
		desc.mArraySize = 1;
		desc.mDepth = 1;
		desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		desc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		desc.mFormat = ImageFormat::R8;
		desc.mHeight = height_;
		desc.mMipLevels = 1;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mStartState = RESOURCE_STATE_COMMON;
		desc.mWidth = width_;
		desc.pDebugName = L"Fontstash Texture";
		TextureLoadDesc loadDesc = {};
		loadDesc.ppTexture = &pCurrentTexture;
		loadDesc.pDesc = &desc;
		addResource(&loadDesc);

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

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateDesc, &pBlendAlpha);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = false;
		depthStateDesc.mDepthWrite = false;
		addDepthState(pRenderer, &depthStateDesc, &pDepthStates[0]);

		DepthStateDesc depthStateEnableDesc = {};
		depthStateEnableDesc.mDepthTest = true;
		depthStateEnableDesc.mDepthWrite = true;
		depthStateEnableDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateEnableDesc, &pDepthStates[1]);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		rasterizerStateDesc.mScissor = true;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStates[0]);

		RasterizerStateDesc rasterizerStateFrontDesc = {};
		rasterizerStateFrontDesc.mCullMode = CULL_MODE_BACK;
		rasterizerStateFrontDesc.mScissor = true;
		addRasterizerState(pRenderer, &rasterizerStateFrontDesc, &pRasterizerStates[1]);

		ShaderLoadDesc text2DShaderDesc = {};
		text2DShaderDesc.mStages[0] = { "fontstash2D.vert", NULL, 0, FSR_MIDDLEWARE_TEXT };
		text2DShaderDesc.mStages[1] = { "fontstash.frag", NULL, 0, FSR_MIDDLEWARE_TEXT };
		ShaderLoadDesc text3DShaderDesc = {};
		text3DShaderDesc.mStages[0] = { "fontstash3D.vert", NULL, 0, FSR_MIDDLEWARE_TEXT };
		text3DShaderDesc.mStages[1] = { "fontstash.frag", NULL, 0, FSR_MIDDLEWARE_TEXT };

		addShader(pRenderer, &text2DShaderDesc, &pShaders[0]);
		addShader(pRenderer, &text3DShaderDesc, &pShaders[1]);

		RootSignatureDesc textureRootDesc = { pShaders, 2 };
#if defined(VULKAN)
		const char* pDynamicUniformBuffers[] = { "uniformBlock" };
		textureRootDesc.mDynamicUniformBufferCount = 1;
		textureRootDesc.ppDynamicUniformBufferNames = pDynamicUniformBuffers;
#endif
		pRootSignature = NULL;
		const char* pStaticSamplers[] = { "uSampler0" };
		textureRootDesc.mStaticSamplerCount = 1;
		textureRootDesc.ppStaticSamplerNames = pStaticSamplers;
		textureRootDesc.ppStaticSamplers = &pDefaultSampler;
		addRootSignature(pRenderer, &textureRootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDescs[] =
		{
			{ pRootSignature }, // 2D
			{ pRootSignature }  // 3D
		};
		addDescriptorBinder(pRenderer, 0, 2, descriptorBinderDescs, &pDescriptorBinder);

		addUniformGPURingBuffer(pRenderer, 65536, &pUniformRingBuffer, true);

		BufferDesc vbDesc = {};
		vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		vbDesc.mSize = 1024 * 1024 * sizeof(float4);
		vbDesc.mVertexStride = sizeof(float4);
		vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
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
			conf_free(mFontBuffers[i]);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);
		removeRootSignature(pRenderer, pRootSignature);

		for (uint32_t i = 0; i < 2; ++i)
		{
			removeShader(pRenderer, pShaders[i]);
		}

		removeGPURingBuffer(pMeshRingBuffer);
		removeGPURingBuffer(pUniformRingBuffer);
		for (uint32_t i = 0; i < 2; ++i)
		{
			removeDepthState(pDepthStates[i]);
			removeRasterizerState(pRasterizerStates[i]);
		}
		removeBlendState(pBlendAlpha);
		removeSampler(pRenderer, pDefaultSampler);
	}

	bool load(RenderTarget** ppRts, uint32_t count)
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
		vertexLayout.mAttribs[1].mOffset = ImageFormat::GetImageFormatStride(ImageFormat::RG32F);

		PipelineDesc pipelineDesc = {};
		pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
		pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
		pipelineDesc.mGraphicsDesc.pBlendState = pBlendAlpha;
		pipelineDesc.mGraphicsDesc.pRootSignature = pRootSignature;
		pipelineDesc.mGraphicsDesc.pVertexLayout = &vertexLayout;
		pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
		pipelineDesc.mGraphicsDesc.mSampleCount = ppRts[0]->mDesc.mSampleCount;
		pipelineDesc.mGraphicsDesc.mSampleQuality = ppRts[0]->mDesc.mSampleQuality;
		pipelineDesc.mGraphicsDesc.pColorFormats = &ppRts[0]->mDesc.mFormat;
		pipelineDesc.mGraphicsDesc.pSrgbValues = &ppRts[0]->mDesc.mSrgb;
		for (uint32_t i = 0; i < min(count, 2U); ++i)
		{
			pipelineDesc.mGraphicsDesc.mDepthStencilFormat = (i > 0) ? ppRts[1]->mDesc.mFormat : ImageFormat::NONE;
			pipelineDesc.mGraphicsDesc.pShaderProgram = pShaders[i];
			pipelineDesc.mGraphicsDesc.pDepthState = pDepthStates[i];
			pipelineDesc.mGraphicsDesc.pRasterizerState = pRasterizerStates[i];
			addPipeline(pRenderer, &pipelineDesc, &pPipelines[i]);
		}

		mScaleBias = { 2.0f / (float)ppRts[0]->mDesc.mWidth, -2.0f / (float)ppRts[0]->mDesc.mHeight };

		return true;
	}

	void unload()
	{
		for (uint32_t i = 0; i < 2; ++i)
		{
			if (pPipelines[i])
				removePipeline(pRenderer, pPipelines[i]);

			pPipelines[i] = NULL;
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

	Shader*           pShaders[2];
	RootSignature*    pRootSignature;
	DescriptorBinder* pDescriptorBinder;
	Pipeline*         pPipelines[2];
	/// Default states
	BlendState*          pBlendAlpha;
	DepthState*          pDepthStates[2];
	RasterizerState*     pRasterizerStates[2];
	Sampler*             pDefaultSampler;
	GPURingBuffer*       pUniformRingBuffer;
	GPURingBuffer*       pMeshRingBuffer;
	float2               mDpiScale;
	float                mDpiScaleMin;
	bool                 mText3D;
};

bool Fontstash::init(Renderer* renderer, uint32_t width, uint32_t height)
{
	impl = conf_placement_new<_Impl_FontStash>(conf_calloc(1, sizeof(_Impl_FontStash)));
	impl->mDpiScale = getDpiScale();
	impl->mDpiScaleMin = min(impl->mDpiScale.x, impl->mDpiScale.y);

	width = width * (int)ceilf(impl->mDpiScale.x);
	height = height * (int)ceilf(impl->mDpiScale.y);

	bool success = impl->init(renderer, width, height);
	m_fFontMaxSize = min(width, height) / 10.0f;    // see fontstash.h, line 1271, for fontSize calculation

	return success;
}

void Fontstash::exit()
{
	impl->exit();
	impl->~_Impl_FontStash();
	conf_free(impl);
}

bool Fontstash::load(RenderTarget** ppRts, uint32_t count)
{
	return impl->load(ppRts, count);
}

void Fontstash::unload()
{
	impl->unload();
}

int Fontstash::defineFont(const char* identification, const char* filename, uint32_t root)
{
	FONScontext* fs = impl->pContext;

	File file = File();
	file.Open(filename, FileMode::FM_ReadBinary, (FSRoot)root);
	unsigned bytes = file.GetSize();
	void*    buffer = conf_malloc(bytes);
	file.Read(buffer, bytes);

	// add buffer to font buffers for cleanup
	impl->mFontBuffers.emplace_back(buffer);
	impl->mFontBufferSizes.emplace_back(bytes);
	impl->mFontNames.emplace_back(file.GetName());

	file.Close();

	return fonsAddFontMem(fs, identification, (unsigned char*)buffer, (int)bytes, 0);
}

int Fontstash::getFontID(const char* identification)
{
	FONScontext* fs = impl->pContext;
	return fonsGetFontByName(fs, identification);
}

const char* Fontstash::getFontName(const char* identification)
{
	FONScontext* fs = impl->pContext;
	return impl->mFontNames[fonsGetFontByName(fs, identification)].c_str();
}

void* Fontstash::getFontBuffer(const char* identification)
{
	FONScontext* fs = impl->pContext;
	return impl->mFontBuffers[fonsGetFontByName(fs, identification)];
}

uint32_t Fontstash::getFontBufferSize(const char* identification)
{
	FONScontext* fs = impl->pContext;
	return impl->mFontBufferSizes[fonsGetFontByName(fs, identification)];
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
	if (ctx->pCurrentTexture == NULL)
		return;

	Cmd* pCmd = ctx->pCmd;

	if (ctx->mUpdateTexture)
	{
		waitQueueIdle(pCmd->pCmdPool->pQueue);

		RawImageData rawData;
		rawData.pRawData = (uint8_t*)ctx->pPixels;
		rawData.mFormat = ImageFormat::R8;
		rawData.mWidth = ctx->mWidth;
		rawData.mHeight = ctx->mHeight;
		rawData.mDepth = 1;
		rawData.mArraySize = 1;
		rawData.mMipLevels = 1;

		TextureUpdateDesc updateDesc = {};
		updateDesc.pTexture = ctx->pCurrentTexture;
		updateDesc.pRawImageData = &rawData;
		updateResource(&updateDesc);

		ctx->mUpdateTexture = false;
	}

	float4* vtx = (float4*)alloca(nverts * sizeof(float4));

	// build vertices
	for (int impl = 0; impl < nverts; impl++)
	{
		vtx[impl].setX(verts[impl * 2 + 0]);
		vtx[impl].setY(verts[impl * 2 + 1]);
		vtx[impl].setZ(tcoords[impl * 2 + 0]);
		vtx[impl].setW(tcoords[impl * 2 + 1]);
	}

	GPURingBufferOffset buffer = getGPURingBufferOffset(ctx->pMeshRingBuffer, nverts * sizeof(float4));
	BufferUpdateDesc update = { buffer.pBuffer, vtx, 0, buffer.mOffset, nverts * sizeof(float4) };
	updateResource(&update);

	// extract color
	ubyte* colorByte = (ubyte*)colors;
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
		BufferUpdateDesc updateDesc = { uniformBlock.pBuffer, &mvp, 0, uniformBlock.mOffset, sizeof(mvp) };
		updateResource(&updateDesc);

		uint64_t size = sizeof(mvp);

		DescriptorData params[3] = {};
		params[0].pName = "uRootConstants";
		params[0].pRootConstant = &data;
		params[1].pName = "uniformBlock";
		params[1].ppBuffers = &uniformBlock.pBuffer;
		params[1].pOffsets = &uniformBlock.mOffset;
		params[1].pSizes = &size;
		params[2].pName = "uTex0";
		params[2].ppTextures = &ctx->pCurrentTexture;
		cmdBindDescriptors(pCmd, ctx->pDescriptorBinder, ctx->pRootSignature, 3, params);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, nverts, 0);
	}
	else
	{
		DescriptorData params[2] = {};
		params[0].pName = "uRootConstants";
		params[0].pRootConstant = &data;
		params[1].pName = "uTex0";
		params[1].ppTextures = &ctx->pCurrentTexture;
		cmdBindDescriptors(pCmd, ctx->pDescriptorBinder, ctx->pRootSignature, 2, params);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, nverts, 0);
	}
}

void _Impl_FontStash::fonsImplementationRemoveTexture(void* userPtr)
{
	UNREF_PARAM(userPtr);
}
