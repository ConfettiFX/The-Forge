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

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/OS/Math/MathTypes.h"

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

struct uniformBlockVS {
	
	vec4 scaleBias;
	vec2 TextureSize;
	mat4 mProjView;
	mat4 mWorldMat;
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

	void		drawTexturedR8AsAlpha(Cmd* pCmd, PrimitiveTopology primitives, TexVertex *vertices, const uint32_t nVertices, Texture* texture, const float4* color);
	
	//for rendering text in world space
	void		drawTexturedR8AsAlpha(Cmd* pCmd, PrimitiveTopology primitives, TexVertex *vertices, const uint32_t nVertices, Texture* texture, const float4* color,const mat4& projView,const mat4& worldMat);
	void		drawTextured(Cmd* pCmd, PrimitiveTopology primitives, TexVertex* vertices, const uint32_t nVertices, Texture* texture, const float4* color);
	void		drawPlain(Cmd* pCmd, PrimitiveTopology primitives, float2* vertices, const uint32_t nVertices, const float4* color);

	void		setScissor(Cmd* pCmd, const RectDesc* rect);

	void		beginRender(uint32_t w, uint32_t h, uint32_t outputFormatCount, ImageFormat::Enum* outputFormats, bool* srgbValues, ImageFormat::Enum depthStencilFormat, SampleCount sampleCount, uint32_t sampleQuality);
	void		reset();

	Texture*	addTexture(Image* image, uint32_t flags);
	void		removeTexture(Texture* tex);
	
	uint32_t	addFontstash(uint32_t width, uint32_t height);
	Fontstash*	getFontstash(uint32_t fontID);

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
	Shader*							pBuiltin3DTextShader;
	RootSignature*					pRootSignatureTextureMesh;
	PipelineMap						mPipelineTextMesh;
	PipelineMap						mPipeline3DTextMesh;
	PipelineMap						mPipelineTextureMesh;

	/// Default states
	BlendState*						pBlendAlpha;
	DepthState*						pDepthNone;
	DepthState*						pDepthEnable;
	RasterizerState*				pRasterizerNoCull;
	Sampler*						pDefaultSampler;

	/// Resource tracking
	tinystl::vector <Fontstash*>	mFontStashes;
	tinystl::vector <Texture*>		mTextureRemoveQueue;

	/// Ring buffer for dynamic constant buffers (same buffer bound at different locations)
	struct UniformRingBuffer*		pUniformRingBuffer;
	/// Ring buffers for dynamic vertex buffers / index buffers 
	struct MeshRingBuffer*			pPlainMeshRingBuffer;
	struct MeshRingBuffer*			pTextureMeshRingBuffer;
	

	/// Mutable data
	PipelineVector*					pCurrentPipelinePlainMesh;
	PipelineVector*					pCurrentPipelineTextMesh;
	PipelineVector*					pCurrentPipeline3DTextMesh;
	PipelineVector*					pCurrentPipelineTextureMesh;
	
	mat4 							mProjectionView;
	mat4							mWorldMat;
};
