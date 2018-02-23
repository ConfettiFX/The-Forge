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

/************************************************************************/
/* SETTINGS
/************************************************************************/
// we won't be using a dedicated command list for Panini projection post process.
// instead, the app that uses Panini will provide the command list.
#define USE_DEDICATED_COMMAND_LIST 0

/************************************************************************/
/* DATA
/************************************************************************/
Shader*				pShaderPanini = nullptr;
RootSignature*		pRootSignaturePaniniPostProcess = nullptr;
Sampler*			pSamplerTrilinearAniso = nullptr;
DepthState*			pDepthStateDisable = nullptr;
RasterizerState*	pRasterizerStateCullNone = nullptr;
Pipeline*			pPipielinePaniniPostProcess = nullptr;

Buffer*				pVertexBufferTessellatedQuad = nullptr;
Buffer*				pIndexBufferTessellatedQuad = nullptr;

#if USE_DEDICATED_COMMAND_LIST
CmdPool*			pPaniniCmdPool = nullptr;
Cmd**				ppPaniniCmds = nullptr;
#endif

// Panini projection renders into a tessellated rectangle which imitates a curved cylinder surface
const unsigned gPaniniDistortionTessellation[2] = { 64, 32 };

// either the static Panini parameters will be used
PaniniParameters	gPaniniParametersStatic;
bool				bUsingStaticPaniniParams = false;
// or the dynamic gui controls will be used depending on initPanini() call.
struct PaniniGUI
{
	Gui*				pGui = nullptr;
	bool				mEnablePaniniProjection = false;
	PaniniParameters	mParams;
	DynamicUIControls	mDynamicUIControls;
};
PaniniGUI			gPaniniSettingsDynamic;

void (*g_pfnPaniniToggleCallback)(bool bEnabled) = nullptr;

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

bool initializeRenderingResources(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
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


	// PIPELINE
	//----------------------------------------------------------------------------------------------------------------
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
	pipelineSettings.pColorFormats = &pRenderTarget->mDesc.mFormat;
	pipelineSettings.pSrgbValues = &pRenderTarget->mDesc.mSrgb;
	pipelineSettings.mSampleCount = pRenderTarget->mDesc.mSampleCount;
	pipelineSettings.mSampleQuality = pRenderTarget->mDesc.mSampleQuality;
	pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	pipelineSettings.pRootSignature = pRootSignaturePaniniPostProcess;
	pipelineSettings.pShaderProgram = pShaderPanini;
	pipelineSettings.pVertexLayout = &vertexLayoutPanini;
	addPipeline(pRenderer, &pipelineSettings, &pPipielinePaniniPostProcess);

	createTessellatedQuadBuffers(&pVertexBufferTessellatedQuad, &pIndexBufferTessellatedQuad, gPaniniDistortionTessellation[0], gPaniniDistortionTessellation[1]);


#if USE_DEDICATED_COMMAND_LIST
	// COMMAND LIST
	//----------------------------------------------------------------------------------------------------------------
	addCmdPool(pRenderer, pGraphicsQueue, false, &pPaniniCmdPool);
	addCmd_n(pPaniniCmdPool, false, imageCount, &ppPaniniCmds);
#endif

	return true;
}


/************************************************************************/
/* INTERFACE FUNCTIONS
/************************************************************************/
#if USE_DEDICATED_COMMAND_LIST
bool initPanini(Renderer* pRenderer, RenderTarget* pRenderTarget, Queue* pGraphicsQueue, uint32_t imageCount)
#else
bool AppPanini::Init(Renderer* pRenderer, RenderTarget* pRenderTarget, Gui* pGuiWindow, void (*pfnPaniniToggleCallback)(bool))
#endif
{
	ASSERT(pRenderer);
	ASSERT(pRenderTarget);
	ASSERT(pGuiWindow);
	ASSERT(pfnPaniniToggleCallback);

	bool bSuccess = initializeRenderingResources(pRenderer, pRenderTarget);
	if (!bSuccess)
	{
		LOGERRORF("Error initializing Panini Projection Post Process rendering resources.");
		return false;
	}

	// UI SETTINGS
	//----------------------------------------------------------------------------------------------------------------
	PaniniParameters& params                  = gPaniniSettingsDynamic.mParams;	// shorthand
	tinystl::vector<UIProperty>& dynamicProps = gPaniniSettingsDynamic.mDynamicUIControls.mDynamicProperties;	// shorthand

	UIProperty fov("Camera Horizontal FoV", params.FoVH, 30.0f, 179.0f, 1.0f);
	addProperty(pGuiWindow, &fov);
	
	UIProperty toggle("Enable Panini Projection", gPaniniSettingsDynamic.mEnablePaniniProjection);
	addProperty(pGuiWindow, &toggle);
	
	dynamicProps.push_back(UIProperty("Panini D Parameter", params.D, 0.0f, 1.0f, 0.001f));
	dynamicProps.push_back(UIProperty("Panini S Parameter", params.S, 0.0f, 1.0f, 0.001f));
	dynamicProps.push_back(UIProperty("Screen Scale"      , params.scale, 1.0f, 10.0f, 0.01f));
	
	if (gPaniniSettingsDynamic.mEnablePaniniProjection)
	{
		gPaniniSettingsDynamic.mDynamicUIControls.ShowDynamicProperties(pGuiWindow);
	}

	gPaniniSettingsDynamic.pGui = pGuiWindow;
	g_pfnPaniniToggleCallback = pfnPaniniToggleCallback;

	return bSuccess;
}

bool AppPanini::Init(Renderer* pRenderer, RenderTarget* pRenderTarget, PaniniParameters params)
{
	ASSERT(pRenderer);
	ASSERT(pRenderTarget);

	bool bSuccess = initializeRenderingResources(pRenderer, pRenderTarget);
	if (!bSuccess)
	{
		LOGERRORF("Error initializing Panini Projection Post Process rendering resources.");
		return false;
	}

	gPaniniParametersStatic = params;
	bUsingStaticPaniniParams = true;
	
	return bSuccess;
}

void AppPanini::Exit(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	removeShader(pRenderer, pShaderPanini);

	removeSampler(pRenderer, pSamplerTrilinearAniso);
	removeRasterizerState(pRasterizerStateCullNone);
	removeDepthState(pDepthStateDisable);

	removeRootSignature(pRenderer, pRootSignaturePaniniPostProcess);
	removePipeline(pRenderer, pPipielinePaniniPostProcess);

	removeResource(pVertexBufferTessellatedQuad);
	removeResource(pIndexBufferTessellatedQuad);

#if USE_DEDICATED_COMMAND_LIST
	removeCmd_n(pPaniniCmdPool, imageCount, ppPaniniCmds);
	removeCmdPool(pRenderer, pPaniniCmdPool);
#endif
}


void AppPanini::Update(float* pFieldOfView)
{
	ASSERT(g_pfnPaniniToggleCallback);

	// if we're not using dynamic GUI, don't do any updates
	if (!gPaniniSettingsDynamic.pGui) return;

	// handle toggle event
	static bool gbWasPaniniEnabled = gPaniniSettingsDynamic.mEnablePaniniProjection;	// state cache
	if (gPaniniSettingsDynamic.mEnablePaniniProjection != gbWasPaniniEnabled)
	{
		gbWasPaniniEnabled = gPaniniSettingsDynamic.mEnablePaniniProjection;
		if (gbWasPaniniEnabled)
		{
			gPaniniSettingsDynamic.mDynamicUIControls.ShowDynamicProperties(gPaniniSettingsDynamic.pGui);
		}
		else
		{
			gPaniniSettingsDynamic.mDynamicUIControls.HideDynamicProperties(gPaniniSettingsDynamic.pGui);
		}
		g_pfnPaniniToggleCallback(gbWasPaniniEnabled);
	}

	// update projection matrix parameters of the app of it has a dynamic GUI
	if (pFieldOfView)
	{
		*pFieldOfView = gPaniniSettingsDynamic.mParams.FoVH * (PI / 180.0f);
	}
	else
	{
		LOGWARNINGF("Panini Projection post process has been initialized with dynamic GUI but the FoV" \
					" parameter passed to updatePanini() is null. Changing FoV through the GUI will have no effect");
	}
}


void AppPanini::UpdateParameters(PaniniParameters paniniParams)
{
	if (gPaniniSettingsDynamic.pGui)
	{
		gPaniniSettingsDynamic.mParams = paniniParams;
	}
	else
	{
		gPaniniParametersStatic = paniniParams;
	}
}

// Draws the Panini Projection distortion to the @pPaniniOutputRenderTarget. 
void AppPanini::Draw(Cmd* cmd, Texture* pInputTexture, GpuProfiler* pGraphicsGpuProfiler /*= nullptr*/)
{
	ASSERT(cmd);
	ASSERT(pInputTexture);

	// panini parameters will either be controlled by GUI OR will be set during initPanini() function (bUsingStaticPaniniParams=true)
	ASSERT((gPaniniSettingsDynamic.pGui || bUsingStaticPaniniParams));

	//beginCmd(cmd);	// beginCmd() and endCmd() should be handled by the caller App

	if (pGraphicsGpuProfiler)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Panini Projection Pass");
	}

	// set pipeline state 
	DescriptorData params[2] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pInputTexture;
	params[1].pName = "PaniniRootConstants";
	params[1].pRootConstant = gPaniniSettingsDynamic.pGui ? &gPaniniSettingsDynamic.mParams : &gPaniniParametersStatic;
	cmdBindDescriptors(cmd, pRootSignaturePaniniPostProcess, 2, params);
	cmdBindPipeline(cmd, pPipielinePaniniPostProcess);

	// draw
	const uint32_t numIndices = gPaniniDistortionTessellation[0] * gPaniniDistortionTessellation[1] * 6;
	cmdBindIndexBuffer(cmd, pIndexBufferTessellatedQuad);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferTessellatedQuad);
	cmdDrawIndexed(cmd, numIndices, 0);

	if (pGraphicsGpuProfiler)
	{
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}

	//endCmd(cmd);	// beginCmd() and endCmd() should be handled by the caller App
}

