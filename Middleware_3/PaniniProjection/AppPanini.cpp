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
/* DATA
/************************************************************************/
Renderer*			pRendererPanini = nullptr;
Queue*				pQGfxPanini = nullptr;
Queue*				pQCompPanini = nullptr;
Gui*				pGuiPanini = nullptr;
GpuProfiler*		pProfilerPanini = nullptr;

Texture*		pSourceTexture = nullptr;

float*				pFov = nullptr;

Shader*				pShaderPanini = nullptr;
RootSignature*		pRootSignaturePaniniPostProcess = nullptr;
Sampler*			pSamplerTrilinearAniso = nullptr;
DepthState*			pDepthStateDisable = nullptr;
RasterizerState*	pRasterizerStateCullNone = nullptr;
Pipeline*			pPipelinePaniniPostProcess = nullptr;
VertexLayout		vertexLayoutPanini = {};

bool				hasInitializedMode = false; // Check if any of the initialization modes has been called

Buffer*				pVertexBufferTessellatedQuad = nullptr;
Buffer*				pIndexBufferTessellatedQuad = nullptr;



// Panini projection renders into a tessellated rectangle which imitates a curved cylinder surface
const unsigned gPaniniDistortionTessellation[2] = { 64, 32 };


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

/************************************************************************/
/* INTERFACE FUNCTIONS
/************************************************************************/

bool AppPanini::SetCallbackToggle(void (*pfnPaniniToggleCallback)(bool))
{
	ASSERT(pfnPaniniToggleCallback);


	// UI SETTINGS
	//----------------------------------------------------------------------------------------------------------------
	PaniniParameters& params                  = gPaniniSettingsDynamic.mParams;	// shorthand
	tinystl::vector<UIProperty>& dynamicProps = gPaniniSettingsDynamic.mDynamicUIControls.mDynamicProperties;	// shorthand

	UIProperty fov("Camera Horizontal FoV", params.FoVH, 30.0f, 179.0f, 1.0f);
	addProperty(pGuiPanini, &fov);
	
	UIProperty toggle("Enable Panini Projection", gPaniniSettingsDynamic.mEnablePaniniProjection);
	addProperty(pGuiPanini, &toggle);
	
	dynamicProps.push_back(UIProperty("Panini D Parameter", params.D, 0.0f, 1.0f, 0.001f));
	dynamicProps.push_back(UIProperty("Panini S Parameter", params.S, 0.0f, 1.0f, 0.001f));
	dynamicProps.push_back(UIProperty("Screen Scale"      , params.scale, 1.0f, 10.0f, 0.01f));
	
	if (gPaniniSettingsDynamic.mEnablePaniniProjection)
	{
		gPaniniSettingsDynamic.mDynamicUIControls.ShowDynamicProperties(pGuiPanini);
	}

	gPaniniSettingsDynamic.pGui = pGuiPanini;
	g_pfnPaniniToggleCallback = pfnPaniniToggleCallback;

	hasInitializedMode = true;
	
return true;
}


bool AppPanini::Init(Renderer* renderer, Queue* gfxQ, Queue* cmpQ, Gui* gui, GpuProfiler* profiler)
{
	pRendererPanini = renderer;
	pQGfxPanini = gfxQ;
	pQCompPanini = cmpQ;
	pGuiPanini = gui;
	pProfilerPanini = profiler;


	// SHADER
	//----------------------------------------------------------------------------------------------------------------
	ShaderLoadDesc paniniPass = {};
	paniniPass.mStages[0] = { "panini_projection.vert", NULL, 0, FSR_SrcShaders_Common };
	paniniPass.mStages[1] = { "panini_projection.frag", NULL, 0, FSR_SrcShaders_Common };
	addShader(pRendererPanini, &paniniPass, &pShaderPanini);

	// SAMPLERS & STATES
	//----------------------------------------------------------------------------------------------------------------
	addSampler(pRendererPanini, &pSamplerTrilinearAniso, FILTER_TRILINEAR_ANISO, FILTER_BILINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0.0f, 8.0f);
	addRasterizerState(&pRasterizerStateCullNone, CULL_MODE_NONE);
	addDepthState(pRendererPanini, &pDepthStateDisable, false, false);


	// ROOT SIGNATURE
	//----------------------------------------------------------------------------------------------------------------
	RootSignatureDesc paninniRootDesc = {};

	paninniRootDesc.mStaticSamplers["uSampler"] = pSamplerTrilinearAniso;
	addRootSignature(pRendererPanini, 1, &pShaderPanini, &pRootSignaturePaniniPostProcess, &paninniRootDesc);

	// Vertexlayout panini
	//----------------------------------------------------------------------------------------------------------------
	vertexLayoutPanini.mAttribCount = 1;
	vertexLayoutPanini.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutPanini.mAttribs[0].mFormat = ImageFormat::RGBA32F;
	vertexLayoutPanini.mAttribs[0].mBinding = 0;
	vertexLayoutPanini.mAttribs[0].mLocation = 0;
	vertexLayoutPanini.mAttribs[0].mOffset = 0;

	createTessellatedQuadBuffers(&pVertexBufferTessellatedQuad, &pIndexBufferTessellatedQuad, gPaniniDistortionTessellation[0], gPaniniDistortionTessellation[1]);

	return true;
}

void AppPanini::Exit()
{
	removeShader(pRendererPanini, pShaderPanini);

	removeSampler(pRendererPanini, pSamplerTrilinearAniso);
	removeRasterizerState(pRasterizerStateCullNone);
	removeDepthState(pDepthStateDisable);

	removeRootSignature(pRendererPanini, pRootSignaturePaniniPostProcess);

	removeResource(pVertexBufferTessellatedQuad);
	removeResource(pIndexBufferTessellatedQuad);
}

bool AppPanini::Load(RenderTarget** rts)
{

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
	addPipeline(pRendererPanini, &pipelineSettings, &pPipelinePaniniPostProcess);

	return true;
}

void AppPanini::Unload() {

	if (pPipelinePaniniPostProcess)
	{
		removePipeline(pRendererPanini, pPipelinePaniniPostProcess);
	}
}

void AppPanini::Draw(Cmd* cmd)
{
	ASSERT(cmd);
	ASSERT(pSourceTexture);

	// panini parameters will either be controlled by GUI OR will be set during initPanini() function (bUsingStaticPaniniParams=true)
	ASSERT((gPaniniSettingsDynamic.pGui));

	
	if (!hasInitializedMode)
	{
		LOGERRORF("Panini has not been initialized in dynamic or static mode, please call InitDynamic() or InitStatic() after Init()");
		return;
	}
	//beginCmd(cmd);	// beginCmd() and endCmd() should be handled by the caller App

	if (pProfilerPanini)
	{ 
		cmdBeginGpuTimestampQuery(cmd, pProfilerPanini, "Panini Projection Pass");
	}
	
	// set pipeline state 
	DescriptorData params[2] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pSourceTexture;
	params[1].pName = "PaniniRootConstants";
	params[1].pRootConstant = &gPaniniSettingsDynamic.mParams;
	cmdBindDescriptors(cmd, pRootSignaturePaniniPostProcess, 2, params);
	cmdBindPipeline(cmd, pPipelinePaniniPostProcess);

	// draw
	const uint32_t numIndices = gPaniniDistortionTessellation[0] * gPaniniDistortionTessellation[1] * 6;
	cmdBindIndexBuffer(cmd, pIndexBufferTessellatedQuad);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferTessellatedQuad);
	cmdDrawIndexed(cmd, numIndices, 0);

	if (pProfilerPanini)
	{
		cmdEndGpuTimestampQuery(cmd, pProfilerPanini);
	}
	// do some drawing
}

void AppPanini::SetFovPtr(float* pFieldOfView)
{
	pFov = pFieldOfView;
}

void AppPanini::Update(float deltaTime)
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
	if (pFov)
	{
		*pFov = gPaniniSettingsDynamic.mParams.FoVH * (PI / 180.0f);
	}
	else
	{
		LOGWARNINGF("Panini Projection post process has been initialized with dynamic GUI but the FoV" \
			" parameter passed to updatePanini() is null. Changing FoV through the GUI will have no effect");
	}
}

void AppPanini::SetSourceTexture(Texture* pTex)
{
	pSourceTexture = pTex;
}