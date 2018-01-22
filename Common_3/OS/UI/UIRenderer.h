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

#pragma once

#include "../../Renderer/IRenderer.h"
#include "../Math/MathTypes.h"

struct TexVertex
{
	TexVertex() {};
	TexVertex(const float2 p, const float2 t)
	{
		position = p;
		texCoord = t;
	}
	float2 position = float2(0.0f, 0.0f);
	float2 texCoord;
};

class Fontstash;
class Image;
struct TextDrawDesc;
class ICameraController;

class UIRenderer
{
public: 
	static void onWindowResize(const struct WindowResizeEventData* pData);

public:
	UIRenderer(Renderer* renderer);
	~UIRenderer();

	void		drawTexturedR8AsAlpha(PrimitiveTopology primitives, TexVertex *vertices, const uint32_t nVertices, Texture* texture, const float4* color);
	void		drawTextured(PrimitiveTopology primitives, TexVertex* vertices, const uint32_t nVertices, Texture* texture, const float4* color);
	void		drawPlain(PrimitiveTopology primitives, float2* vertices, const uint32_t nVertices, const float4* color);

	void		setScissor(const RectDesc* rect);

	void		beginRender(Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil);
	void		reset();

	Texture*	addTexture(Image* image, uint32_t flags);
	void		removeTexture(Texture* tex);
	
	uint32_t	addFontstash(uint32_t width, uint32_t height);
	Fontstash*	getFontstash(uint32_t fontID);

	int			addFont(const char* filename, const char* fontName = "", FSRoot root = FSRoot::FSR_Builtin_Fonts);
	
private:
	using PipelineVector = tinystl::vector <Pipeline*>;
	using PipelineMap = tinystl::unordered_map<uint64_t, PipelineVector>;
	using PipelineMapNode = tinystl::unordered_hash_node<uint64_t, PipelineVector>;

	Renderer*					pRenderer;

	/// Plain mesh pipeline data
	Shader*							pBuiltinPlainShader;
	RootSignature*					pRootSignaturePlainMesh;
	PipelineMap						mPipelinePlainMesh;

	/// Texture mesh pipeline data
	Shader*							pBuiltinTextShader;
	Shader*							pBuiltinTextureShader;
	RootSignature*					pRootSignatureTextureMesh;
	PipelineMap						mPipelineTextMesh;
	PipelineMap						mPipelineTextureMesh;

	/// Default states
	BlendState*						pBlendAlpha;
	DepthState*						pDepthNone;
	RasterizerState*				pRasterizerNoCull;
	Sampler*						pDefaultSampler;
	Sampler*						pPointSampler;

	/// Resource tracking
	tinystl::vector <Fontstash*>	mFontStashes;
	tinystl::vector <Texture*>		mTextureRemoveQueue;

	/// Ring buffer for dynamic constant buffers (same buffer bound at different locations)
	struct UniformRingBuffer*		pUniformRingBuffer;
	/// Ring buffers for dynamic vertex buffers / index buffers 
	struct MeshRingBuffer*			pPlainMeshRingBuffer;
	struct MeshRingBuffer*			pTextureMeshRingBuffer;

	/// Mutable data
	RootSignature*					pCurrentRootSignature;
	Cmd*							pCurrentCmd;
	PipelineVector*					pCurrentPipelinePlainMesh;
	PipelineVector*					pCurrentPipelineTextMesh;
	PipelineVector*					pCurrentPipelineTextureMesh;
};
