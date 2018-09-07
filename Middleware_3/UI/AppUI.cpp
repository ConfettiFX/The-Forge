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

#include "UIShaders.h"

#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Image/Image.h"
#include "../../Common_3/OS/Interfaces/ICameraController.h"

#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"


#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../Middleware_3/Text/Fontstash.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../../Common_3/OS/Core/RingBuffer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

namespace PlatformEvents
{
	extern bool skipMouseCapture;
}

static tinystl::vector<GuiComponentImpl*> gInstances;
static Mutex gMutex;

extern void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver);
extern void removeGUIDriver(GUIDriver* pDriver);

static bool uiInputEvent(const ButtonData* pData);
/************************************************************************/
// UI Implementation
/************************************************************************/
struct UIAppImpl
{
	Renderer*									pRenderer;
	Fontstash*									pFontStash;
	uint32_t									mWidth;
	uint32_t									mHeight;

	tinystl::vector<GuiComponent*>				mComponents;

	tinystl::vector<struct GuiComponentImpl*>	mComponentsToUpdate;
	float										mDeltaTime;
};
UIAppImpl* pInst;

struct GuiComponentImpl
{
	bool Init(class UIApp* pApp, const char* pTitle, const GuiDesc* pDesc)
	{
		initGUIDriver(pApp->pImpl->pRenderer, &pDriver);

		pDriver->load(pApp->pImpl->pFontStash, pDesc->mDefaultTextDrawDesc.mFontSize, NULL);

		mInitialWindowRect =
		{
			pDesc->mStartPosition.getX(),
			pDesc->mStartPosition.getY(),
			pDesc->mStartSize.getX(),
			pDesc->mStartSize.getY()
		};

		SetActive(true);
		SetTitle(pTitle);

		MutexLock lock(gMutex);
		gInstances.emplace_back(this);

		if (gInstances.size() == 1)
		{
			InputSystem::RegisterInputEvent(uiInputEvent);
		}

		return true;
	}

	void Exit()
	{
		pDriver->unload();

		removeGUIDriver(pDriver);

		MutexLock lock(gMutex);
		gInstances.erase(gInstances.find(this));
	}

	void Draw(Cmd* pCmd, float deltaTime)
	{
		if (!IsActive())
			return;

		pDriver->draw(pCmd, deltaTime,
			mTitle,
			mInitialWindowRect.x, mInitialWindowRect.y, mInitialWindowRect.z, mInitialWindowRect.w,
			&mProperties[0], (uint32_t)mProperties.size());
	}

	void SetTitle(const char* title_)
	{
		this->mTitle = title_;
	}

	void SetActive(bool active)
	{
		mActive = active;
	}

	bool IsActive()
	{
		return mActive;
	}

	unsigned int AddControl(const UIProperty& prop)
	{
		// Try first to fill empty property slot
		for (unsigned int i = 0; i < (uint32_t)mProperties.size(); i++)
		{
			UIProperty& prop_slot = mProperties[i];
			if (prop_slot.pData != NULL)
				continue;

			prop_slot = prop;
			return i;
		}

		mProperties.emplace_back(prop);
		return (uint32_t)mProperties.size() - 1;
	}

	UIProperty& GetControl(unsigned int idx)
	{
		return mProperties[idx];
	}

	void ClearControls()
	{
		mProperties.clear();
	}

	void RemoveControl(unsigned int idx)
	{
		UIProperty& prop = mProperties[idx];
		prop.pData = NULL;
		prop.pCallback = NULL;
	}

	void SetControlFlag(unsigned int propertyId, UIProperty::FLAG flag, bool state)
	{
		ASSERT(propertyId < (uint32_t)mProperties.size());
		unsigned int& flags = mProperties[propertyId].mFlags;
		flags = state ? (flags | flag) : (flags & ~flag);
	}

	// returns: 0: no input handled, 1: input handled
	bool OnInput(const struct ButtonData* pData)
	{
		// Handle the mouse click events:
		//   We want to send ButtonData with click position to the UI system
		//   
		if (   pData->mUserId == KEY_CONFIRM      // left  click
			|| pData->mUserId == KEY_RIGHT_BUMPER // right click
			|| pData->mUserId == KEY_MOUSE_WHEEL
		)
		{
			// Query the latest UI_MOVE event since the current event 
			// which is a click event, doesn't contain the mouse position. 
			// Here we construct the 'toSend' data to contain both the 
			// position (from the latest Move event) and click info from the
			// current event.
			ButtonData latestUIMoveEventData = InputSystem::GetButtonData((uint32_t)KEY_UI_MOVE);
			ButtonData toSend = *pData;
			toSend.mValue[0] = latestUIMoveEventData.mValue[0];
			toSend.mValue[1] = latestUIMoveEventData.mValue[1];

			PlatformEvents::skipMouseCapture = pDriver->onInput(&toSend);
			return PlatformEvents::skipMouseCapture;
		}
		
		// just relay the rest of the events to the UI and let the UI system process the events
		return pDriver->onInput(pData);
	}

private:
	GUIDriver*					pDriver;
	float4						mInitialWindowRect;
	tinystl::string				mTitle;
	tinystl::vector<UIProperty>	mProperties;
	bool						mActive;
};

bool UIApp::Init(Renderer* renderer)
{
	pImpl = (struct UIAppImpl*)conf_calloc(1, sizeof(*pImpl));
	pInst = pImpl;
	pImpl->pRenderer = renderer;
	// Figure out the max font size for the current configuration
	uint32 uiMaxFrontSize = uint32(UIMaxFontSize::UI_MAX_FONT_SIZE_512);

	// Add and initialize the fontstash 
	pImpl->pFontStash = conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), renderer, (int)uiMaxFrontSize, (int)uiMaxFrontSize);

	return true;
}

void UIApp::Exit()
{
	// Make copy of vector since RemoveGuiComponent will modify the original vector
	tinystl::vector<GuiComponent*> components = pImpl->mComponents;
	for (uint32_t i = 0; i < (uint32_t)components.size(); ++i)
		RemoveGuiComponent(components[i]);

	pImpl->pFontStash->destroy();
	conf_free(pImpl->pFontStash);

	pImpl->~UIAppImpl();
	conf_free(pImpl);
}

bool UIApp::Load(RenderTarget** rts)
{
	pImpl->mWidth = rts[0]->mDesc.mWidth;
	pImpl->mHeight = rts[0]->mDesc.mHeight;

	return true;
}

void UIApp::Unload()
{
}

uint32_t UIApp::LoadFont(const char* pFontPath, uint root)
{
	uint32_t fontID = (uint32_t)pImpl->pFontStash->defineFont("default", pFontPath, root);
	ASSERT(fontID != -1);

	return fontID;
}

GuiComponent* UIApp::AddGuiComponent(const char* pTitle, const GuiDesc* pDesc)
{
	GuiComponent* pComponent = conf_placement_new<GuiComponent>(conf_calloc(1, sizeof(GuiComponent)));
	pComponent->pImpl = (struct GuiComponentImpl*)conf_calloc(1, sizeof(GuiComponentImpl));
	pComponent->pImpl->Init(this, pTitle, pDesc);

	pImpl->mComponents.emplace_back(pComponent);

	return pComponent;
}

void UIApp::RemoveGuiComponent(GuiComponent* pComponent)
{
	ASSERT(pComponent);

	pImpl->mComponents.erase(pImpl->mComponents.find(pComponent));

	pComponent->pImpl->Exit();
	pComponent->pImpl->~GuiComponentImpl();
	conf_free(pComponent->pImpl);
	pComponent->~GuiComponent();
	conf_free(pComponent);
}

void UIApp::Update(float deltaTime)
{
	pImpl->mComponentsToUpdate.clear();
	pImpl->mDeltaTime = deltaTime;
}

void UIApp::Draw(Cmd* pCmd)
{
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponentsToUpdate.size(); ++i)
	{
		pImpl->mComponentsToUpdate[i]->Draw(pCmd, pImpl->mDeltaTime);
	}
}

void UIApp::Gui(GuiComponent* pGui)
{
	pImpl->mComponentsToUpdate.emplace_back(pGui->pImpl);
}

uint32_t GuiComponent::AddControl(const UIProperty& control)
{
	return pImpl->AddControl(control);
}

void GuiComponent::RemoveControl(unsigned int controlID)
{
	pImpl->RemoveControl(controlID);
}

/************************************************************************/
/************************************************************************/
bool VirtualJoystickUI::Init(Renderer* renderer, const char* pJoystickTexture, uint root)
{
	pRenderer = renderer;

	TextureLoadDesc loadDesc = {};
	loadDesc.pFilename = pJoystickTexture;
	loadDesc.mRoot = (FSRoot)root;
	loadDesc.ppTexture = &pTexture;
	addResource(&loadDesc);

	if (!pTexture)
		return false;
	/************************************************************************/
	// States
	/************************************************************************/
	SamplerDesc samplerDesc =
	{
		FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_NEAREST,
		ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
	};
	addSampler(pRenderer, &samplerDesc, &pSampler);

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
	addDepthState(pRenderer, &depthStateDesc, &pDepthState);

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
	rasterizerStateDesc.mScissor = true;
	addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerState);
	/************************************************************************/
	// Shader
	/************************************************************************/
#if defined(METAL)
	tinystl::string texturedShaderFile = "builtin_plain";
	tinystl::string texturedShader = mtl_builtin_textured;
	ShaderDesc texturedShaderDesc = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG, { texturedShaderFile, texturedShader, "VSMain" }, { texturedShaderFile, texturedShader, "PSMain" } };
	addShader(pRenderer, &texturedShaderDesc, &pShader);
#elif defined(DIRECT3D12) || defined(VULKAN)
	char* pTexturedVert = NULL; uint texturedVertSize = 0;
	char* pTexturedFrag = NULL; uint texturedFragSize = 0;

	if (pRenderer->mSettings.mApi == RENDERER_API_D3D12 || pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
	{
		pTexturedVert = (char*)d3d12_builtin_textured_vert; texturedVertSize = sizeof(d3d12_builtin_textured_vert);
		pTexturedFrag = (char*)d3d12_builtin_textured_frag; texturedFragSize = sizeof(d3d12_builtin_textured_frag);
	}
	else if (pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
	{
		pTexturedVert = (char*)vk_builtin_textured_vert; texturedVertSize = sizeof(vk_builtin_textured_vert);
		pTexturedFrag = (char*)vk_builtin_textured_frag; texturedFragSize = sizeof(vk_builtin_textured_frag);
	}
	
	BinaryShaderDesc texturedShader = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
		{ (char*)pTexturedVert, texturedVertSize },{ (char*)pTexturedFrag, texturedFragSize } };
	addShaderBinary(pRenderer, &texturedShader, &pShader);
#endif
	
	
	const char* pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pShader, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignature);
	
	/************************************************************************/
	// Resources
	/************************************************************************/
	BufferDesc vbDesc = {};
	vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.mVertexStride = sizeof(float4);
	addMeshRingBuffer(pRenderer, &vbDesc, NULL, &pMeshRingBuffer);
	/************************************************************************/
	/************************************************************************/

	return true;
}

void VirtualJoystickUI::Exit()
{
	removeMeshRingBuffer(pMeshRingBuffer);
	removeRasterizerState(pRasterizerState);
	removeBlendState(pBlendAlpha);
	removeDepthState(pDepthState);
	removeRootSignature(pRenderer, pRootSignature);
	removeShader(pRenderer, pShader);
	removeResource(pTexture);
}

bool VirtualJoystickUI::Load(RenderTarget* pScreenRT, uint depthFormat )
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

	GraphicsPipelineDesc pipelineDesc = {};
	pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
	pipelineDesc.mDepthStencilFormat = (ImageFormat::Enum)depthFormat;
	pipelineDesc.mRenderTargetCount = 1;
	pipelineDesc.mSampleCount = pScreenRT->mDesc.mSampleCount;
	pipelineDesc.mSampleQuality = pScreenRT->mDesc.mSampleQuality;
	pipelineDesc.pBlendState = pBlendAlpha;
	pipelineDesc.pColorFormats = &pScreenRT->mDesc.mFormat;
	pipelineDesc.pDepthState = pDepthState;
	pipelineDesc.pRasterizerState = pRasterizerState;
	pipelineDesc.pSrgbValues = &pScreenRT->mDesc.mSrgb;
	pipelineDesc.pRootSignature = pRootSignature;
	pipelineDesc.pShaderProgram = pShader;
	pipelineDesc.pVertexLayout = &vertexLayout;
	addPipeline(pRenderer, &pipelineDesc, &pPipeline);

	return true;
}

void VirtualJoystickUI::Unload()
{
	removePipeline(pRenderer, pPipeline);
}

void VirtualJoystickUI::Draw(Cmd* pCmd, class ICameraController* pCameraController, const float4& color)
{
#ifdef TARGET_IOS
	struct RootConstants
	{
		float4 color;
		float2 scaleBias;
	} data = {};

	cmdBindPipeline(pCmd, pPipeline);
	data.color = color;
	data.scaleBias = { 2.0f / (float)pCmd->mBoundWidth, -2.0f / (float)pCmd->mBoundHeight };
	DescriptorData params[2] = {};
	params[0].pName = "uRootConstants";
	params[0].pRootConstant = &data;
	params[1].pName = "uTex";
	params[1].ppTextures = &pTexture;
	cmdBindDescriptors(pCmd, pRootSignature, 2, params);

	// Draw the camera controller's virtual joysticks.
	float extSide = min(pCmd->mBoundHeight, pCmd->mBoundWidth) * pCameraController->getVirtualJoystickExternalRadius();
	float intSide = min(pCmd->mBoundHeight, pCmd->mBoundWidth) * pCameraController->getVirtualJoystickInternalRadius();

	{
		float2 joystickSize = float2(extSide);
		vec2 joystickCenter = pCameraController->getVirtualLeftJoystickCenter();
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
	{
		vec2 joystickCenter = pCameraController->getVirtualRightJoystickCenter();
		float2 joystickSize = float2(extSide);
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
	{
		float2 joystickSize = float2(intSide);
		vec2 joystickCenter = pCameraController->getVirtualLeftJoystickPos();
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
	{
		float2 joystickSize = float2(intSide);
		vec2 joystickCenter = pCameraController->getVirtualRightJoystickPos();
		float2 joystickPos = float2(joystickCenter.getX() * pCmd->mBoundWidth, joystickCenter.getY() * pCmd->mBoundHeight) - 0.5f * joystickSize;

		// the last variable can be used to create a border
		TexVertex vertices[] = { MAKETEXQUAD(joystickPos.x, joystickPos.y,
			joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
		RingBufferOffset buffer = getVertexBufferOffset(pMeshRingBuffer, sizeof(vertices));
		BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
		updateResource(&updateDesc);
		cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
		cmdDraw(pCmd, 4, 0);
	}
#endif
}
/************************************************************************/
// Event Handlers
/************************************************************************/
static bool uiInputEvent(const ButtonData * pData)
{
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->IsActive() && gInstances[i]->OnInput(pData))
			return true;

	// KEY_LEFT_STICK_BUTTON <-> F1 Key : See InputMapphings.h for details
	// F1: Toggle Displaying UI
	if (pData->mUserId == KEY_LEFT_STICK_BUTTON && pData->mIsTriggered)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->SetActive(!gInstances[i]->IsActive());
	}

	return false;
}
/************************************************************************/
/************************************************************************/
