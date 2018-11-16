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

static tinystl::vector<GuiComponent*> gInstances;
static Mutex gMutex;

extern void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver);
extern void removeGUIDriver(GUIDriver* pDriver);

static bool uiInputEvent(const ButtonData* pData);

static void CloneCallbacks(IWidget* pSrc, IWidget* pDst)
{
	// Clone the callbacks
	pDst->pOnActive = pSrc->pOnActive;
	pDst->pOnHover = pSrc->pOnHover;
	pDst->pOnFocus = pSrc->pOnFocus;
	pDst->pOnEdited = pSrc->pOnEdited;
	pDst->pOnDeactivated = pSrc->pOnDeactivated;
	pDst->pOnDeactivatedAfterEdit = pSrc->pOnDeactivatedAfterEdit;
}

IWidget* CollapsingHeaderWidget::Clone() const
{
	CollapsingHeaderWidget* pWidget = conf_placement_new<CollapsingHeaderWidget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel);

	// Need to read the subwidgets as the destructor will remove them all
	for (size_t i = 0; i < mGroupedWidgets.size(); ++i)
		pWidget->AddSubWidget(*mGroupedWidgets[i]);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* LabelWidget::Clone() const
{
	LabelWidget* pWidget = conf_placement_new<LabelWidget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ButtonWidget::Clone() const
{
	ButtonWidget* pWidget = conf_placement_new<ButtonWidget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SeparatorWidget::Clone() const
{
	SeparatorWidget* pWidget = conf_placement_new<SeparatorWidget>(
		conf_calloc(1, sizeof(*pWidget)));

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloatWidget::Clone() const
{
	SliderFloatWidget* pWidget = conf_placement_new<SliderFloatWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloat2Widget::Clone() const
{
	SliderFloat2Widget* pWidget = conf_placement_new<SliderFloat2Widget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloat3Widget::Clone() const
{
	SliderFloat3Widget* pWidget = conf_placement_new<SliderFloat3Widget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloat4Widget::Clone() const
{
	SliderFloat4Widget* pWidget = conf_placement_new<SliderFloat4Widget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderIntWidget::Clone() const
{
	SliderIntWidget* pWidget = conf_placement_new<SliderIntWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);


	return pWidget;
}

IWidget* SliderUintWidget::Clone() const
{
	SliderUintWidget* pWidget = conf_placement_new<SliderUintWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* RadioButtonWidget::Clone() const
{
	RadioButtonWidget* pWidget = conf_placement_new<RadioButtonWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mRadioId);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* DropdownWidget::Clone() const
{
	const char** ppNames = (const char**)alloca(mValues.size() * sizeof(const char*));
	for (uint32_t i = 0; i < (uint32_t)mValues.size(); ++i)
		ppNames[i] = mNames[i].c_str();
	DropdownWidget* pWidget = conf_placement_new<DropdownWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, ppNames, this->mValues.data(), (uint32_t)this->mValues.size());

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ProgressBarWidget::Clone() const
{
	ProgressBarWidget* pWidget = conf_placement_new<ProgressBarWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, mMaxProgress);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ColorSliderWidget::Clone() const
{
	ColorSliderWidget* pWidget = conf_placement_new<ColorSliderWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ColorPickerWidget::Clone() const
{
	ColorPickerWidget* pWidget = conf_placement_new<ColorPickerWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* TextboxWidget::Clone() const
{
	TextboxWidget* pWidget = conf_placement_new<TextboxWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData, this->mLength, this->mAutoSelectAll);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* CheckboxWidget::Clone() const
{
	CheckboxWidget* pWidget = conf_placement_new<CheckboxWidget>(
		conf_calloc(1, sizeof(*pWidget)),
		this->mLabel, this->pData);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}
/************************************************************************/
// UI Implementation
/************************************************************************/
struct UIAppImpl
{
	Renderer*								   pRenderer;
	Fontstash*								  pFontStash;
	uint32_t									mWidth;
	uint32_t									mHeight;

	tinystl::vector<GuiComponent*>			  mComponents;

	tinystl::vector<GuiComponent*>  mComponentsToUpdate;
	float									   mDeltaTime;
};
UIAppImpl* pInst;

bool UIApp::Init(Renderer* renderer)
{
	mShowDemoUiWindow = false;

	pImpl = (struct UIAppImpl*)conf_calloc(1, sizeof(*pImpl));
	pInst = pImpl;
	pImpl->pRenderer = renderer;


	// Initialize the fontstash
	//
	// To support more characters and different font configurations
	// the app will need more memory for the fontstash atlas.
	//
#if defined(TARGET_IOS) || defined(ANDROID)
	const int TextureAtlasDimension = 512;
#elif defined(DURANGO)
	const int TextureAtlasDimension = 1024;
#else // PC / LINUX / MAC
	const int TextureAtlasDimension = 2048;
#endif
	pImpl->pFontStash = conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), renderer, TextureAtlasDimension, TextureAtlasDimension);

	return pImpl->pFontStash != NULL;
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

float2 UIApp::MeasureText(const char* pText, const TextDrawDesc& drawDesc ) const
{
	float textBounds[4] = {};
	pImpl->pFontStash->measureText(textBounds, pText, 0, 0, drawDesc.mFontID, drawDesc.mFontColor, drawDesc.mFontSize, drawDesc.mFontSpacing, drawDesc.mFontBlur);
	return float2(textBounds[2] - textBounds[0], textBounds[3] - textBounds[1]);
}

void UIApp::DrawText(Cmd* cmd, const float2& screenCoordsInPx, const char* pText, const TextDrawDesc& drawDesc) const
{
	const TextDrawDesc* pDesc = &drawDesc;
	pImpl->pFontStash->drawText(cmd, pText, screenCoordsInPx.getX(), screenCoordsInPx.getY(),
		pDesc->mFontID, pDesc->mFontColor,
		pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

void UIApp::DrawTextInWorldSpace(Cmd* pCmd, const char* pText, const TextDrawDesc& drawDesc, const mat4& matWorld, const mat4& matProjView)
{
	const TextDrawDesc* pDesc = &drawDesc;
	pImpl->pFontStash->drawText(pCmd, pText, matProjView, matWorld,
		pDesc->mFontID, pDesc->mFontColor,
		pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

GuiComponent* UIApp::AddGuiComponent(const char* pTitle, const GuiDesc* pDesc)
{
	GuiComponent* pComponent = conf_placement_new<GuiComponent>(conf_calloc(1, sizeof(GuiComponent)));
	pComponent->mHasCloseButton = false;
	pComponent->mFlags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;

	initGUIDriver(pImpl->pRenderer, &pComponent->pDriver);
	pComponent->pDriver->load(pImpl->pFontStash, pDesc->mDefaultTextDrawDesc.mFontSize, NULL);

	pComponent->mInitialWindowRect =
	{
		pDesc->mStartPosition.getX(),
		pDesc->mStartPosition.getY(),
		pDesc->mStartSize.getX(),
		pDesc->mStartSize.getY()
	};

	pComponent->mActive = true;
	pComponent->mTitle = pTitle;

	MutexLock lock(gMutex);
	gInstances.emplace_back(pComponent);

	if (gInstances.size() == 1)
	{
		InputSystem::RegisterInputEvent(uiInputEvent);
	}

	pImpl->mComponents.emplace_back(pComponent);

	return pComponent;
}

void UIApp::RemoveGuiComponent(GuiComponent* pComponent)
{
	ASSERT(pComponent);

	for (uint32_t i = 0; i < (uint32_t)pComponent->mWidgets.size(); ++i)
	{
		IWidget* pWidget = pComponent->mWidgets[i];
		pWidget->~IWidget();
		conf_free(pWidget);
	}

	pImpl->mComponents.erase(pImpl->mComponents.find(pComponent));

	pComponent->pDriver->unload();
	pComponent->mWidgets.clear();

	removeGUIDriver(pComponent->pDriver);

	MutexLock lock(gMutex);
	gInstances.erase(gInstances.find(pComponent));

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
	bool hasYetToShowDemoUi = true;

	// Only on iOS as this only applies to virtual keyboard.
	// TODO: add Durango at a later stage
#ifdef TARGET_IOS
	//stores whether or not we need text input for
	//any gui component
	//if any component requires textInput then this is true.
	int wantsTextInput = 0;
#endif
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponentsToUpdate.size(); ++i)
	{
		GuiComponent* pGui = pImpl->mComponentsToUpdate[i];
		if (!pGui->mActive)
			continue;

		pGui->pDriver->draw(pCmd, pImpl->mDeltaTime,
			pGui->mTitle,
			pGui->mHasCloseButton ? &pGui->mActive : NULL,
			pGui->mFlags,
			pGui->mInitialWindowRect.x, pGui->mInitialWindowRect.y, pGui->mInitialWindowRect.z, pGui->mInitialWindowRect.w,
			pGui->mWidgets.data(), (uint32_t)pGui->mWidgets.size(),
			pGui->mContextualMenuLabels, pGui->mContextualMenuCallbacks,
			(hasYetToShowDemoUi && mShowDemoUiWindow) ? true : false);

		if (mShowDemoUiWindow)
			hasYetToShowDemoUi = false;

#ifdef TARGET_IOS
		//check if current component requires textInput
		//only support one type of text
		//check for bigger that way we enable keyboard with all characters
		//if there's one widget that requires digits only and one that requires all text
		if(pGui->pDriver->needsTextInput() > wantsTextInput)
			wantsTextInput = pGui->pDriver->needsTextInput();
#endif
	}

#ifdef TARGET_IOS
	//if current Virtual keyboard state is not equal to
	//text input status then toggle the appropriate behavior (hide, show)
	if(InputSystem::IsVirtualKeyboardActive() != (wantsTextInput > 0))
	{
		InputSystem::ToggleVirtualTouchKeyboard(wantsTextInput);
	}
#endif
}

void UIApp::Gui(GuiComponent* pGui)
{
	pImpl->mComponentsToUpdate.emplace_back(pGui);
}

IWidget* GuiComponent::AddWidget(const IWidget& widget)
{
	mWidgets.emplace_back(widget.Clone());
	return mWidgets.back();
}

void GuiComponent::RemoveWidget(IWidget* pWidget)
{
	decltype(mWidgets)::iterator it = mWidgets.find(pWidget);
	if (it != mWidgets.end())
	{
		IWidget* pWidget = *it;
		mWidgets.erase(it);
		pWidget->~IWidget();
		conf_free(pWidget);
	}
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
	{
		LOGERRORF("Error loading texture file: %s", pJoystickTexture);
		return false;
	}
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
#if defined(TARGET_IOS) || defined(__ANDROID__)
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
	// returns: 0: no input handled, 1: input handled
bool OnInput(const struct ButtonData* pData, GUIDriver* pDriver)
{
	// Handle the mouse click events:
	// We want to send ButtonData with click position to the UI system
	//
	if (pData->mUserId == KEY_CONFIRM	 // left  click
		|| pData->mUserId == KEY_RIGHT_BUMPER // right click
		|| pData->mUserId == KEY_MOUSE_WHEEL)
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

static bool uiInputEvent(const ButtonData * pData)
{
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
	{
		if (gInstances[i]->mActive && OnInput(pData, gInstances[i]->pDriver))
		{
			return true;
		}
	}

	// KEY_LEFT_STICK_BUTTON <-> F1 Key : See InputMapphings.h for details
	// F1: Toggle Displaying UI
	if (pData->mUserId == KEY_LEFT_STICK_BUTTON && pData->mIsTriggered)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->mActive = (!gInstances[i]->mActive);

		PlatformEvents::skipMouseCapture = false;
	}

	return false;
}
/************************************************************************/
/************************************************************************/
