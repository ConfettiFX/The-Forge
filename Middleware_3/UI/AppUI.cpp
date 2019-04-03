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

#include "AppUI.h"

#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Image/Image.h"
#include "../../Common_3/OS/Interfaces/ICameraController.h"

#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../Middleware_3/Text/Fontstash.h"

#include "../../Common_3/OS/Input/InputSystem.h"
#include "../../Common_3/OS/Input/InputMappings.h"

#include "../../Common_3/OS/Core/RingBuffer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

namespace PlatformEvents {
extern bool skipMouseCapture;
}

FSRoot                         FSR_MIDDLEWARE_UI = FSR_Middleware1;
static tinystl::vector<UIApp*> gInstances;
static Mutex                   gMutex;

extern void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver);
extern void removeGUIDriver(GUIDriver* pDriver);

static bool uiInputEvent(const ButtonData* pData);

typedef struct GpuProfileDrawDesc
{
	float        mChildIndent = 25.0f;
	float        mHeightOffset = 20.0f;
	TextDrawDesc mDrawDesc = TextDrawDesc(0, 0xFF00CCAA, 15);
} GpuProfileDrawDesc;

static GpuProfileDrawDesc gDefaultGpuProfileDrawDesc = {};
static TextDrawDesc       gDefaultTextDrawDesc = TextDrawDesc(0, 0xffffffff, 16);

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
	CollapsingHeaderWidget* pWidget =
		conf_placement_new<CollapsingHeaderWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mDefaultOpen, this->mCollapsed, this->mHeaderIsVisible);

	// Need to read the subwidgets as the destructor will remove them all
	for (size_t i = 0; i < mGroupedWidgets.size(); ++i)
		pWidget->AddSubWidget(*mGroupedWidgets[i]);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* DebugTexturesWidget::Clone() const
{
	DebugTexturesWidget* pWidget = conf_placement_new<DebugTexturesWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel);
	pWidget->SetTextures(this->mTextures, mTextureDisplaySize);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* LabelWidget::Clone() const
{
	LabelWidget* pWidget = conf_placement_new<LabelWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ButtonWidget::Clone() const
{
	ButtonWidget* pWidget = conf_placement_new<ButtonWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SeparatorWidget::Clone() const
{
	SeparatorWidget* pWidget = conf_placement_new<SeparatorWidget>(conf_calloc(1, sizeof(*pWidget)));

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloatWidget::Clone() const
{
	SliderFloatWidget* pWidget = conf_placement_new<SliderFloatWidget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloat2Widget::Clone() const
{
	SliderFloat2Widget* pWidget = conf_placement_new<SliderFloat2Widget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloat3Widget::Clone() const
{
	SliderFloat3Widget* pWidget = conf_placement_new<SliderFloat3Widget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderFloat4Widget::Clone() const
{
	SliderFloat4Widget* pWidget = conf_placement_new<SliderFloat4Widget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderIntWidget::Clone() const
{
	SliderIntWidget* pWidget = conf_placement_new<SliderIntWidget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* SliderUintWidget::Clone() const
{
	SliderUintWidget* pWidget = conf_placement_new<SliderUintWidget>(
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* RadioButtonWidget::Clone() const
{
	RadioButtonWidget* pWidget =
		conf_placement_new<RadioButtonWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mRadioId);

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
		conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, ppNames, this->mValues.data(), (uint32_t)this->mValues.size());

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ProgressBarWidget::Clone() const
{
	ProgressBarWidget* pWidget =
		conf_placement_new<ProgressBarWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, mMaxProgress);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ColorSliderWidget::Clone() const
{
	ColorSliderWidget* pWidget = conf_placement_new<ColorSliderWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* ColorPickerWidget::Clone() const
{
	ColorPickerWidget* pWidget = conf_placement_new<ColorPickerWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* TextboxWidget::Clone() const
{
	TextboxWidget* pWidget =
		conf_placement_new<TextboxWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mLength, this->mAutoSelectAll);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}

IWidget* CheckboxWidget::Clone() const
{
	CheckboxWidget* pWidget = conf_placement_new<CheckboxWidget>(conf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);

	// Clone the callbacks
	CloneCallbacks((IWidget*)this, pWidget);

	return pWidget;
}
/************************************************************************/
// UI Implementation
/************************************************************************/

UIApp::UIApp(int32_t const fontAtlasSize)
{
	mFontAtlasSize = fontAtlasSize;
}

bool UIApp::Init(Renderer* renderer)
{
	mShowDemoUiWindow = false;

	pImpl = (struct UIAppImpl*)conf_calloc(1, sizeof(*pImpl));
	pImpl->pRenderer = renderer;

	pDriver = NULL;

	// Initialize the fontstash
	//
	// To support more characters and different font configurations
	// the app will need more memory for the fontstash atlas.
	//
	if (mFontAtlasSize <= 0) // then we assume we'll only draw debug text in the UI, in which case the atlas size can be kept small
		mFontAtlasSize = 256;

	pImpl->pFontStash =
		conf_placement_new<Fontstash>(conf_calloc(1, sizeof(Fontstash)), renderer, mFontAtlasSize, mFontAtlasSize);
	initGUIDriver(pImpl->pRenderer, &pDriver);

	MutexLock lock(gMutex);
	gInstances.emplace_back(this);

	if (gInstances.size() == 1)
	{
		InputSystem::RegisterInputEvent(uiInputEvent, UINT_MAX);
	}

	return pImpl->pFontStash != NULL;
}

void UIApp::Exit()
{
	UIApp** it = gInstances.find(this);
	ASSERT(it != gInstances.end());
	if (it != gInstances.end())
	{
		gInstances.erase(it);
	}

	RemoveAllGuiComponents();

	pImpl->pFontStash->destroy();
	conf_free(pImpl->pFontStash);

	pDriver->unload();
	removeGUIDriver(pDriver);
	pDriver = NULL;

	pImpl->~UIAppImpl();
	conf_free(pImpl);
}

bool UIApp::Load(RenderTarget** rts)
{ 
	ASSERT(rts && rts[0]);
	mWidth = (float)rts[0]->mDesc.mWidth;
	mHeight = (float)rts[0]->mDesc.mHeight;
	return true;
}

void UIApp::Unload() {}

uint32_t UIApp::LoadFont(const char* pFontPath, uint root)
{
	uint32_t fontID = (uint32_t)pImpl->pFontStash->defineFont("default", pFontPath, root);
	ASSERT(fontID != -1);

	return fontID;
}

float2 UIApp::MeasureText(const char* pText, const TextDrawDesc& drawDesc) const
{
	float textBounds[4] = {};
	pImpl->pFontStash->measureText(
		textBounds, pText, 0, 0, drawDesc.mFontID, drawDesc.mFontColor, drawDesc.mFontSize, drawDesc.mFontSpacing, drawDesc.mFontBlur);
	return float2(textBounds[2] - textBounds[0], textBounds[3] - textBounds[1]);
}

void UIApp::DrawText(Cmd* cmd, const float2& screenCoordsInPx, const char* pText, const TextDrawDesc* pDrawDesc) const
{
#if (ENABLE_MICRO_PROFILER)
	if (mMicroProfileEnabled)
	{
		return;
	}
#endif
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pImpl->pFontStash->drawText(
		cmd, pText, screenCoordsInPx.getX(), screenCoordsInPx.getY(), pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize,
		pDesc->mFontSpacing, pDesc->mFontBlur);
}

void UIApp::DrawTextInWorldSpace(Cmd* pCmd, const char* pText, const mat4& matWorld, const mat4& matProjView, const TextDrawDesc* pDrawDesc)
{
#if (ENABLE_MICRO_PROFILER)
	if (mMicroProfileEnabled)
	{
		return;
	}
#endif
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pImpl->pFontStash->drawText(
		pCmd, pText, matProjView, matWorld, pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

#if defined(__linux__)
#define sprintf_s sprintf    // On linux, we should use sprintf as sprintf_s is not part of the standard c library
#endif


static void draw_gpu_profile_recurse(
	Cmd* pCmd, Fontstash* pFontStash, float2& startPos, const GpuProfileDrawDesc* pDrawDesc, struct GpuProfiler* pGpuProfiler,
	GpuTimerTree* pRoot)
{
#if defined(DIRECT3D12) || defined(VULKAN) || defined(DIRECT3D11)
	if (!pRoot)
		return;

	float originalX = startPos.getX();

	if (pRoot->mGpuTimer.mIndex > 0 && pRoot != &pGpuProfiler->mRoot)
	{
		char   buffer[128];
		double time = getAverageGpuTime(pGpuProfiler, &pRoot->mGpuTimer);
		sprintf_s(buffer, "%s -  %f ms", pRoot->mGpuTimer.mName.c_str(), time * 1000.0);

		pFontStash->drawText(
			pCmd, buffer, startPos.x, startPos.y, pDrawDesc->mDrawDesc.mFontID, pDrawDesc->mDrawDesc.mFontColor,
			pDrawDesc->mDrawDesc.mFontSize, pDrawDesc->mDrawDesc.mFontSpacing, pDrawDesc->mDrawDesc.mFontBlur);
		startPos.y += pDrawDesc->mHeightOffset;

		if ((uint32_t)pRoot->mChildren.size())
			startPos.setX(startPos.getX() + pDrawDesc->mChildIndent);
	}

	for (uint32_t i = 0; i < (uint32_t)pRoot->mChildren.size(); ++i)
	{
		draw_gpu_profile_recurse(pCmd, pFontStash, startPos, pDrawDesc, pGpuProfiler, pRoot->mChildren[i]);
	}

	startPos.x = originalX;
#endif
}

void UIApp::DrawDebugGpuProfile(Cmd * pCmd, const float2& screenCoordsInPx, GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc)
{
#if (ENABLE_MICRO_PROFILER)
	if (mMicroProfileEnabled)
	{
		ProfileDraw(pCmd, MICROPROFILE_UIWINDOW_WIDTH, MICROPROFILE_UIWINDOW_HEIGHT);
		return;
	}
#endif
	const GpuProfileDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultGpuProfileDrawDesc;
	float2                    pos = screenCoordsInPx;
	pImpl->pFontStash->drawText(
		pCmd, "-----GPU Times-----", pos.x, pos.y, pDesc->mDrawDesc.mFontID, pDesc->mDrawDesc.mFontColor, pDesc->mDrawDesc.mFontSize,
		pDesc->mDrawDesc.mFontSpacing, pDesc->mDrawDesc.mFontBlur);
	pos.y += pDesc->mHeightOffset;

	draw_gpu_profile_recurse(pCmd, pImpl->pFontStash, pos, pDesc, pGpuProfiler, &pGpuProfiler->mRoot);
	return;
}

GuiComponent* UIApp::AddGuiComponent(const char* pTitle, const GuiDesc* pDesc)
{
	GuiComponent* pComponent = conf_placement_new<GuiComponent>(conf_calloc(1, sizeof(GuiComponent)));
	pComponent->mHasCloseButton = false;
	pComponent->mFlags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;

	pDriver->load(pImpl->pFontStash, pDesc->mDefaultTextDrawDesc.mFontSize, NULL);

	pComponent->mInitialWindowRect = { pDesc->mStartPosition.getX(), pDesc->mStartPosition.getY(), pDesc->mStartSize.getX(),
									   pDesc->mStartSize.getY() };

	pComponent->mActive = true;
	pComponent->mTitle = pTitle;
	pComponent->pDriver = pDriver;

	pImpl->mComponents.emplace_back(pComponent);

	return pComponent;
}

void UIApp::RemoveGuiComponent(GuiComponent* pComponent)
{
	ASSERT(pComponent);

	pComponent->RemoveAllWidgets();
	GuiComponent** it = pImpl->mComponents.find(pComponent);
	if (it != pImpl->mComponents.end())
	{
		(*it)->RemoveAllWidgets();
		pImpl->mComponents.erase(it);
		pComponent->mWidgets.clear();
	}

	pComponent->~GuiComponent();
	conf_free(pComponent);
}

void UIApp::RemoveAllGuiComponents()
{
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponents.size(); ++i)
	{
		pImpl->mComponents[i]->RemoveAllWidgets();
		pImpl->mComponents[i]->~GuiComponent();
		conf_free(pImpl->mComponents[i]);
	}

	pImpl->mComponents.clear();
	pImpl->mComponentsToUpdate.clear();
}

void UIApp::Update(float deltaTime)
{
	pImpl->mUpdated = true;

	tinystl::vector<GuiComponent*> activeComponents(pImpl->mComponentsToUpdate.size());
	uint32_t                       activeComponentCount = 0;
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponentsToUpdate.size(); ++i)
		if (pImpl->mComponentsToUpdate[i]->mActive)
			activeComponents[activeComponentCount++] = pImpl->mComponentsToUpdate[i];

	GUIDriver::GUIUpdate guiUpdate{ activeComponents.data(), activeComponentCount, deltaTime, mWidth, mHeight, mShowDemoUiWindow };
	mHovering = pDriver->update(&guiUpdate);

	// Only on iOS as this only applies to virtual keyboard.
	// TODO: add Durango at a later stage
#ifdef TARGET_IOS
	//stores whether or not we need text input for
	//any gui component
	//if any component requires textInput then this is true.
	int wantsTextInput = 0;

	//check if current component requires textInput
	//only support one type of text
	//check for bigger that way we enable keyboard with all characters
	//if there's one widget that requires digits only and one that requires all text
	if (pDriver->needsTextInput() > wantsTextInput)
		wantsTextInput = pDriver->needsTextInput();

	//if current Virtual keyboard state is not equal to
	//text input status then toggle the appropriate behavior (hide, show)
	if (InputSystem::IsVirtualKeyboardActive() != (wantsTextInput > 0))
	{
		InputSystem::ToggleVirtualKeyboard(wantsTextInput);
	}
#endif

	pImpl->mComponentsToUpdate.clear();
}

void UIApp::Draw(Cmd* pCmd)
{
	if (pImpl->mUpdated)
	{
		pImpl->mUpdated = false;
		pDriver->draw(pCmd);
	}
}

void UIApp::Gui(GuiComponent* pGui) { pImpl->mComponentsToUpdate.emplace_back(pGui); }

IWidget* GuiComponent::AddWidget(const IWidget& widget, bool clone /* = true*/)
{
	mWidgets.emplace_back((clone ? widget.Clone() : (IWidget*)&widget));
	mWidgetsClone.emplace_back(clone);
	return mWidgets.back();
}

void GuiComponent::RemoveWidget(IWidget* pWidget)
{
	decltype(mWidgets)::iterator it = mWidgets.find(pWidget);
	if (it != mWidgets.end())
	{
		IWidget* pWidget = *it;
		if (mWidgetsClone[it - mWidgets.begin()])
		{
			pWidget->~IWidget();
			conf_free(pWidget);
		}
		mWidgetsClone.erase(mWidgetsClone.begin() + (it - mWidgets.begin()));
		mWidgets.erase(it);
	}
}

void GuiComponent::RemoveAllWidgets()
{
	for (uint32_t i = 0; i < (uint32_t)mWidgets.size(); ++i)
	{
		if (mWidgetsClone[i])
		{
			mWidgets[i]->~IWidget();
			conf_free(mWidgets[i]);
		}
	}

	mWidgets.clear();
	mWidgetsClone.clear();
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
	loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
	addResource(&loadDesc);

	if (!pTexture)
	{
		LOGERRORF("Error loading texture file: %s", pJoystickTexture);
		return false;
	}
	/************************************************************************/
	// States
	/************************************************************************/
	SamplerDesc samplerDesc = { FILTER_LINEAR,
								FILTER_LINEAR,
								MIPMAP_MODE_NEAREST,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE };
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
	ShaderLoadDesc texturedShaderDesc = {};
	texturedShaderDesc.mStages[0] = { "textured_mesh.vert", NULL, 0, FSR_MIDDLEWARE_UI };
	texturedShaderDesc.mStages[1] = { "textured_mesh.frag", NULL, 0, FSR_MIDDLEWARE_UI };
	addShader(pRenderer, &texturedShaderDesc, &pShader);

	const char*       pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pShader, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignature);

	DescriptorBinderDesc descriptorBinderDesc = { pRootSignature };
	addDescriptorBinder(pRenderer, 0, 1, &descriptorBinderDesc, &pDescriptorBinder);

	/************************************************************************/
	// Resources
	/************************************************************************/
	BufferDesc vbDesc = {};
	vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	vbDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.mVertexStride = sizeof(float4);
	addGPURingBuffer(pRenderer, &vbDesc, &pMeshRingBuffer);
	/************************************************************************/
	/************************************************************************/

	mInitialized = true;
	return true;
}

void VirtualJoystickUI::Exit()
{
	if (!mInitialized)
		return;

	removeGPURingBuffer(pMeshRingBuffer);
	removeRasterizerState(pRasterizerState);
	removeBlendState(pBlendAlpha);
	removeDepthState(pDepthState);
	removeDescriptorBinder(pRenderer, pDescriptorBinder);
	removeRootSignature(pRenderer, pRootSignature);
	removeShader(pRenderer, pShader);
	removeResource(pTexture);
}

bool VirtualJoystickUI::Load(RenderTarget* pScreenRT, uint depthFormat)
{
	if (!mInitialized)
		return false;

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

	PipelineDesc desc = {};
	desc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
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
	addPipeline(pRenderer, &desc, &pPipeline);

	mRenderSize[0] = (float)pScreenRT->mDesc.mWidth;
	mRenderSize[1] = (float)pScreenRT->mDesc.mHeight;
	return true;
}

void VirtualJoystickUI::Unload()
{
	if (!mInitialized)
		return;
	removePipeline(pRenderer, pPipeline);
}

void VirtualJoystickUI::InitLRSticks(float insideRad, float outsideRad, float deadzone)
{
	float contentScaleFactor = getDpiScale().getX();
#ifdef TARGET_IOS
	contentScaleFactor /= [UIScreen.mainScreen nativeScale];
#endif
	mInsideRadius = insideRad * contentScaleFactor;
	mOutsideRadius = outsideRad * contentScaleFactor;
	mDeadzone = deadzone * contentScaleFactor;
	mSticks[0].mTouchIndex = mSticks[1].mTouchIndex = -1;
	mActive = mInitialized;
}

vec2 VirtualJoystickUI::GetLeftStickDir() { return mSticks[0].mIsPressed ? mSticks[0].mDir : vec2(0.f, 0.f); }

vec2 VirtualJoystickUI::GetRightStickDir() { return mSticks[1].mIsPressed ? mSticks[1].mDir : vec2(0.f, 0.f); }

vec2 VirtualJoystickUI::GetStickRadius() { return vec2(mOutsideRadius, mInsideRadius); }

void VirtualJoystickUI::Update(float dt)
{
	if (!mActive)
		return;

	const float halfRad = mOutsideRadius * 0.5f;
	for (uint i = 0; i < 2; i++)
	{
		if (mSticks[i].mIsPressed)
		{
			vec2  joystickDir = (mSticks[i].mCurrPos - mSticks[i].mStartPos);
			float dirLength = length(joystickDir);
			// Update velocity vector
			if (dirLength > mDeadzone)
			{
				vec2 normalizedJoystickDir = (joystickDir) / halfRad;
				if (dirLength > halfRad)
					normalizedJoystickDir = normalize(joystickDir) * (halfRad);

				mSticks[i].mDir = normalizedJoystickDir;
			}
			else
			{
				mSticks[i].mDir = vec2(0, 0);
				mSticks[i].mCurrPos = mSticks[i].mStartPos;
			}
		}
	}
}

bool VirtualJoystickUI::IsActive(bool left) { return mActive && (left ? mSticks[0].mIsPressed : mSticks[1].mIsPressed); }

bool VirtualJoystickUI::IsAnyActive() { return mActive && (mSticks[0].mIsPressed || mSticks[1].mIsPressed); }

void VirtualJoystickUI::SetActive(bool state)
{
	if (mInitialized)
		mActive = state;
}

bool VirtualJoystickUI::OnInputEvent(const ButtonData* pData)
{
	if (pData->mEventConsumed || !mActive)
		return false;

	if (pData->mActiveDevicesMask & GAINPUT_TOUCH && (pData->mUserId == VIRTUAL_JOYSTICK_TOUCH0 || pData->mUserId == VIRTUAL_JOYSTICK_TOUCH1))
	{
		// Get normalized touch pos
		vec2 touchPos = vec2(pData->mValue[0], pData->mValue[1]);

		// if true then finger is at left half of screen
		// otherwise right half
		int stickIndex = touchPos.getX() > mRenderSize[0] / 2.f ? 1 : 0;

		if (mSticks[0].mTouchIndex != -1 || mSticks[1].mTouchIndex != -1)
		{
			if (mSticks[0].mTouchIndex == pData->mTouchIndex && mSticks[0].mIsPressed)
			{
				stickIndex = 0;
			}
			else if (mSticks[1].mTouchIndex == pData->mTouchIndex && mSticks[1].mIsPressed)
				stickIndex = 1;
		}

		if (mSticks[stickIndex].mTouchIndex != pData->mTouchIndex && mSticks[stickIndex].mIsPressed)
			return false;

		bool firstTouch = false;

		// If jostick is being triggered for first first
		// we need to place it there.
		if (pData->mIsReleased || !pData->mIsPressed)
		{
			mSticks[stickIndex].mIsPressed = false;
			mSticks[stickIndex].mTouchIndex = -1;
			return false;
		}
		else if (pData->mIsPressed)
		{
			if (!mSticks[stickIndex].mIsPressed)
				firstTouch = true;
		}

		// Spawn joystick at desired position
		if (firstTouch)
		{
			mSticks[stickIndex].mIsPressed = true;
			mSticks[stickIndex].mStartPos = touchPos;
			mSticks[stickIndex].mCurrPos = touchPos;
			mSticks[stickIndex].mTouchIndex = pData->mTouchIndex;
		}

		// Calculate the new joystick positions.
		vec2 normalizedDelta(
			pData->mValue[0] - mSticks[stickIndex].mStartPos.getX(), pData->mValue[1] - mSticks[stickIndex].mStartPos.getY());

		vec2  newPos(pData->mValue[0], pData->mValue[1]);
		float halfRad = mOutsideRadius / 2.f - mDeadzone;
		if (length(normalizedDelta) > halfRad)
			newPos = mSticks[stickIndex].mStartPos + normalize(normalizedDelta) * halfRad;

		mSticks[stickIndex].mCurrPos = newPos;
	}

	return true;
}

void VirtualJoystickUI::Draw(Cmd* pCmd, const float4& color)
{
	if (!mActive)
		return;

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
	cmdBindDescriptors(pCmd, pDescriptorBinder, pRootSignature, 2, params);

	if (mRenderSize[0] != (float)pCmd->mBoundWidth)
		mRenderSize[0] = (float)pCmd->mBoundWidth;
	if (mRenderSize[1] != (float)pCmd->mBoundHeight)
		mRenderSize[1] = (float)pCmd->mBoundHeight;

	// Draw the camera controller's virtual joysticks.
	float extSide = mOutsideRadius;
	float intSide = mInsideRadius;

	for (uint i = 0; i < 2; i++)
	{
		if (mSticks[i].mIsPressed)
		{
			float2 joystickSize = float2(extSide);
			vec2   joystickCenter = mSticks[i].mStartPos;
			float2 joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			// the last variable can be used to create a border
			TexVertex        vertices[] = { MAKETEXQUAD(
                joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
			GPURingBufferOffset buffer = getGPURingBufferOffset(pMeshRingBuffer, sizeof(vertices));
			BufferUpdateDesc updateDesc = { buffer.pBuffer, vertices, 0, buffer.mOffset, sizeof(vertices) };
			updateResource(&updateDesc);
			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);

			joystickSize = float2(intSide);
			joystickCenter = mSticks[i].mCurrPos;
			joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			// the last variable can be used to create a border
			TexVertex verticesInner[] = { MAKETEXQUAD(
				joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
			buffer = getGPURingBufferOffset(pMeshRingBuffer, sizeof(verticesInner));
			updateDesc = { buffer.pBuffer, verticesInner, 0, buffer.mOffset, sizeof(verticesInner) };
			updateResource(&updateDesc);
			cmdBindVertexBuffer(pCmd, 1, &buffer.pBuffer, &buffer.mOffset);
			cmdDraw(pCmd, 4, 0);
		}
	}
}
/************************************************************************/
// Event Handlers
/************************************************************************/
// returns: 0: no input handled, 1: input handled
void OnInput(const struct ButtonData* pData, GUIDriver* pDriver)
{
	ButtonData toSend = *pData;
	// Handle the mouse click events:
	// We want to send ButtonData with click position to the UI system
	//
	if (pData->mUserId == KEY_CONFIRM    // left  click
		|| pData->mUserId == KEY_RIGHT_BUMPER)
	{
		// Query the latest UI_MOVE event since the current event
		// which is a click event, doesn't contain the mouse position.
		// Here we construct the 'toSend' data to contain both the
		// position (from the latest Move event) and click info from the
		// current event.
		toSend.mValue[0] = InputSystem::GetFloatInput(KEY_UI_MOVE, 0);
		toSend.mValue[1] = InputSystem::GetFloatInput(KEY_UI_MOVE, 1);
	}

	// just relay the rest of the events to the UI and let the UI system process the events
	pDriver->onInput(&toSend);
}

static bool uiInputEvent(const ButtonData* pData)
{
	// KEY_LEFT_STICK_BUTTON <-> F1 Key : See InputMapphings.h for details
	// F1: Toggle Displaying UI
	if (pData->mUserId == KEY_LEFT_STICK_BUTTON && pData->mIsTriggered)
	{
		for (uint32_t app = 0; app < (uint32_t)gInstances.size(); ++app)
		{
			UIAppImpl* pImpl = gInstances[app]->pImpl;
			for (uint32_t i = 0; i < (uint32_t)pImpl->mComponents.size(); ++i)
			{
				pImpl->mComponents[i]->mActive = !pImpl->mComponents[i]->mActive;
			}
		}

		PlatformEvents::skipMouseCapture = false;
		return true;
	}

	// if cursor is hidden on capture, and mosue is captured then we can't use the UI. so we shouldn't parse input events.
	// Otherwise UI receives input events when fps camera is active.
	// another approach would be to change input events priorities when fps camera is active.
	if (InputSystem::GetHideMouseCursorWhileCaptured() && InputSystem::IsMouseCaptured() && !pData->mIsReleased)
		return false;

	// if input event was consumed and it's a press/triggered event
	// then we ignore it.
	// We want to use the input event if it was a release so we can correctly
	// release internally set values.
	if (pData->mEventConsumed && !pData->mIsReleased)
		return false;

	if (gInstances.size())
	{
		for (uint32_t app = 0; app < (uint32_t)gInstances.size(); ++app)
		{
			UIApp* pApp = gInstances[app];
			OnInput(pData, pApp->pDriver);
			for (uint32_t i = 0; i < (uint32_t)pApp->pImpl->mComponents.size(); ++i)
			{
				GuiComponent* pGui = pApp->pImpl->mComponents[i];
				// consume the input event
				// if UI requires text input
				// Or if any element is hovered and active.
				if ((pGui->mActive && pApp->pDriver->isHovering(pGui->mCurrentWindowRect)) || pApp->pDriver->needsTextInput())
				{
					PlatformEvents::skipMouseCapture = true;
					return true;
				}
			}
		}
	}

	PlatformEvents::skipMouseCapture = false;
	return false;
}
/************************************************************************/
/************************************************************************/

void UIApp::ActivateMicroProfile(bool isActive)
{
#if (ENABLE_MICRO_PROFILER)
	gUIApp_MP = this;
	mMicroProfileEnabled = isActive;
	ProfileSetDisplayMode(isActive);
#endif
}
#if (ENABLE_MICRO_PROFILER)
void UIApp::ProfileInitUI()
{
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		memset(&g_MicroProfileUI, 0, sizeof(g_MicroProfileUI));
		UI.nActiveMenu = (uint32_t)-1;
		UI.fDetailedOffsetTarget = UI.fDetailedOffset = 0.f;
		UI.fDetailedRangeTarget = UI.fDetailedRange = 50.f;

		UI.nOpacityBackground = 0xffu << 24;
		UI.nOpacityForeground = 0xffu << 24;

		UI.bShowSpikes = false;

		UI.nWidth = 100;
		UI.nHeight = 100;

		UI.nCustomActive = (uint32_t)-1;
		UI.nCustomTimerCount = 0;
		UI.nCustomCount = 0;

		int nIndex = 0;
		UI.Options[nIndex++] = SOptionDesc(0xff, 0, "%s", "Reference");
		for (int i = 0; i < MICROPROFILE_NUM_REFERENCE_PRESETS; ++i)
		{
			UI.Options[nIndex++] = SOptionDesc(0, i, "  %6.2fms", g_MicroProfileReferenceTimePresets[i]);
		}
		UI.Options[nIndex++] = SOptionDesc(0xff, 0, "%s", "BG Opacity");
		for (int i = 0; i < MICROPROFILE_NUM_OPACITY_PRESETS; ++i)
		{
			UI.Options[nIndex++] = SOptionDesc(1, i, "  %7d%%", (i + 1) * 25);
		}
		UI.Options[nIndex++] = SOptionDesc(0xff, 0, "%s", "FG Opacity");
		for (int i = 0; i < MICROPROFILE_NUM_OPACITY_PRESETS; ++i)
		{
			UI.Options[nIndex++] = SOptionDesc(2, i, "  %7d%%", (i + 1) * 25);
		}
		UI.Options[nIndex++] = SOptionDesc(0xff, 0, "%s", "Spike Display");
		UI.Options[nIndex++] = SOptionDesc(3, 0, "%s", "  Enable");

#if MICROPROFILE_CONTEXT_SWITCH_TRACE
		UI.Options[nIndex++] = SOptionDesc(0xff, 0, "%s", "CSwitch Trace");
		UI.Options[nIndex++] = SOptionDesc(4, 0, "%s", "  All Threads");
		UI.Options[nIndex++] = SOptionDesc(4, 1, "%s", "  No Bars");
#endif
		ASSERT(nIndex == MICROPROFILE_OPTION_SIZE);

		UI.nCounterWidth = 100;
		UI.nLimitWidth = 100;
		UI.nCounterWidthTemp = 100;
		UI.nLimitWidthTemp = 100;

	}
}

void UIApp::ProfileSetDisplayMode(int nValue)
{
	MicroProfile& S = *MicroProfileGet();
	nValue = nValue >= 0 && nValue < MP_DRAW_SIZE ? nValue : S.nDisplay;
	S.nDisplay = nValue;
	UI.nOffsetY[S.nDisplay] = 0;
}

void UIApp::ProfileToggleDisplayMode()
{
	MicroProfile& S = *MicroProfileGet();
	S.nDisplay = (S.nDisplay + 1) % MP_DRAW_SIZE;
	UI.nOffsetY[S.nDisplay] = 0;
}


void UIApp::ProfileStringArrayClear(MicroProfileStringArray* pArray)
{
	pArray->nNumStrings = 0;
	pArray->pBufferPos = &pArray->Buffer[0];
}

void UIApp::ProfileStringArrayAddLiteral(MicroProfileStringArray* pArray, const char* pLiteral)
{
	ASSERT(pArray->nNumStrings < MICROPROFILE_TOOLTIP_MAX_STRINGS);
	pArray->ppStrings[pArray->nNumStrings++] = pLiteral;
}

MICROPROFILE_FORMAT(3, 4) void UIApp::ProfileStringArrayFormat(MicroProfileStringArray* pArray, const char* fmt, ...)
{
	ASSERT(pArray->nNumStrings < MICROPROFILE_TOOLTIP_MAX_STRINGS);
	pArray->ppStrings[pArray->nNumStrings++] = pArray->pBufferPos;
	va_list args;
	va_start(args, fmt);
	pArray->pBufferPos += 1 + vsprintf_s(pArray->pBufferPos, MICROPROFILE_TOOLTIP_STRING_BUFFER_SIZE, fmt, args);
	va_end(args);
	ASSERT(pArray->pBufferPos < pArray->Buffer + MICROPROFILE_TOOLTIP_STRING_BUFFER_SIZE);
}
void UIApp::ProfileStringArrayCopy(MicroProfileStringArray* pDest, MicroProfileStringArray* pSrc)
{
	memcpy(&pDest->ppStrings[0], &pSrc->ppStrings[0], sizeof(pDest->ppStrings));
	memcpy(&pDest->Buffer[0], &pSrc->Buffer[0], sizeof(pDest->Buffer));
	for (uint32_t i = 0; i < MICROPROFILE_TOOLTIP_MAX_STRINGS; ++i)
	{
		if (i < pSrc->nNumStrings)
		{
			if (pSrc->ppStrings[i] >= &pSrc->Buffer[0] && pSrc->ppStrings[i] < &pSrc->Buffer[0] + MICROPROFILE_TOOLTIP_STRING_BUFFER_SIZE)
			{
				pDest->ppStrings[i] += &pDest->Buffer[0] - &pSrc->Buffer[0];
			}
		}
	}
	pDest->nNumStrings = pSrc->nNumStrings;
}

void UIApp::ProfileFloatWindowSize(const char** ppStrings, uint32_t nNumStrings, uint32_t* pColors, uint32_t& nWidth, uint32_t& nHeight, uint32_t* pStringLengths)
{
	uint32_t* nStringLengths = pStringLengths ? pStringLengths : (uint32_t*)alloca(nNumStrings * sizeof(uint32_t));
	uint32_t nTextCount = nNumStrings / 2;
	for (uint32_t i = 0; i < nTextCount; ++i)
	{
		uint32_t i0 = i * 2;
		uint32_t s0, s1;
		nStringLengths[i0] = s0 = (uint32_t)strlen(ppStrings[i0]);
		nStringLengths[i0 + 1] = s1 = (uint32_t)strlen(ppStrings[i0 + 1]);
		nWidth = MicroProfileMax(s0 + s1, nWidth);
	}
	nWidth = (MICROPROFILE_TEXT_WIDTH + 1) * (2 + nWidth) + 2 * MICROPROFILE_BORDER_SIZE;
	if (pColors)
		nWidth += MICROPROFILE_TEXT_WIDTH + 1;
	nHeight = (MICROPROFILE_TEXT_HEIGHT + 1) * nTextCount + 2 * MICROPROFILE_BORDER_SIZE;
}

void UIApp::ProfileDrawFloatWindow(Cmd* pCmd, uint32_t nX, uint32_t nY, const char** ppStrings, uint32_t nNumStrings, uint32_t nColor, uint32_t* pColors)
{
	uint32_t nWidth = 0, nHeight = 0;
	uint32_t* nStringLengths = (uint32_t*)alloca(nNumStrings * sizeof(uint32_t));
	ProfileFloatWindowSize(ppStrings, nNumStrings, pColors, nWidth, nHeight, nStringLengths);
	uint32_t nTextCount = nNumStrings / 2;
	if (nX + nWidth > UI.nWidth)
		nX = UI.nWidth - nWidth;
	if (nY + nHeight > UI.nHeight)
		nY = UI.nHeight - nHeight;
	ProfileDrawBox(pCmd, nX - 1, nY - 1, nX + nWidth + 1, nY + nHeight + 1, 0xff000000 | nColor, MicroProfileBoxTypeFlat);
	ProfileDrawBox(pCmd, nX, nY, nX + nWidth, nY + nHeight, 0xff000000, MicroProfileBoxTypeFlat);
	if (pColors)
	{
		nX += MICROPROFILE_TEXT_WIDTH + 1;
		nWidth -= MICROPROFILE_TEXT_WIDTH + 1;
	}
	for (uint32_t i = 0; i < nTextCount; ++i)
	{
		int i0 = i * 2;
		if (pColors)
		{
			ProfileDrawBox(pCmd, nX - MICROPROFILE_TEXT_WIDTH, nY, nX, nY + MICROPROFILE_TEXT_WIDTH, pColors[i] | 0xff000000, MicroProfileBoxTypeFlat);
		}
		ProfileDrawText(pCmd, nX + 1, nY + 1, (uint32_t)-1, ppStrings[i0], (uint32_t)strlen(ppStrings[i0]));
		ProfileDrawText(pCmd, nX + nWidth - nStringLengths[i0 + 1] * (MICROPROFILE_TEXT_WIDTH + 1), nY + 1, (uint32_t)-1, ppStrings[i0 + 1], (uint32_t)strlen(ppStrings[i0 + 1]));
		nY += (MICROPROFILE_TEXT_HEIGHT + 1);
	}
}

void UIApp::ProfileDrawTextBackground(Cmd* pCmd, uint32_t nX, uint32_t nY, uint32_t nColor, uint32_t nBgColor, const char* pString, uint32_t nStrLen)
{
	uint32_t nWidth = (MICROPROFILE_TEXT_WIDTH + 1) * (nStrLen)+2 * MICROPROFILE_BORDER_SIZE;
	uint32_t nHeight = (MICROPROFILE_TEXT_HEIGHT + 1);
	ProfileDrawBox(pCmd, nX, nY, nX + nWidth, nY + nHeight, nBgColor, MicroProfileBoxTypeFlat);
	ProfileDrawText(pCmd, nX, nY, nColor, pString, nStrLen);
}

void UIApp::ProfileToolTipMeta(MicroProfileStringArray* pToolTip)
{
	MicroProfile& S = *MicroProfileGet();
	if (UI.nRangeBeginIndex != UI.nRangeEndIndex && UI.pRangeLog)
	{
		uint64_t nMetaSum[MICROPROFILE_META_MAX] = { 0 };
		uint64_t nMetaSumInclusive[MICROPROFILE_META_MAX] = { 0 };
		int nStackDepth = 0;
		uint32_t nRange[2][2];
		MicroProfileThreadLog* pLog = UI.pRangeLog;


		MicroProfileGetRange(UI.nRangeEndIndex, UI.nRangeBeginIndex, nRange);
		for (uint32_t i = 0; i < 2; ++i)
		{
			uint32_t nStart = nRange[i][0];
			uint32_t nEnd = nRange[i][1];
			for (uint32_t j = nStart; j < nEnd; ++j)
			{
				MicroProfileLogEntry LE = pLog->Log[j];
				uint64_t nType = MicroProfileLogType(LE);
				switch (nType)
				{
				case MP_LOG_META:
				{
					int64_t nMetaIndex = MicroProfileLogTimerIndex(LE);
					int64_t nMetaCount = MicroProfileLogGetTick(LE);
					ASSERT(nMetaIndex < MICROPROFILE_META_MAX);
					if (nStackDepth > 1)
					{
						nMetaSumInclusive[nMetaIndex] += nMetaCount;
					}
					else
					{
						nMetaSum[nMetaIndex] += nMetaCount;
					}
				}
				break;
				case MP_LOG_LEAVE:
					if (nStackDepth)
					{
						nStackDepth--;
					}
					else
					{
						for (int i = 0; i < MICROPROFILE_META_MAX; ++i)
						{
							nMetaSumInclusive[i] += nMetaSum[i];
							nMetaSum[i] = 0;
						}
					}
					break;
				case MP_LOG_ENTER:
					nStackDepth++;
					break;
				}

			}
		}
		bool bSpaced = false;
		for (int i = 0; i < MICROPROFILE_META_MAX; ++i)
		{
			if (S.MetaCounters[i].pName && (nMetaSum[i] || nMetaSumInclusive[i]))
			{
				if (!bSpaced)
				{
					bSpaced = true;
					ProfileStringArrayAddLiteral(pToolTip, "");
					ProfileStringArrayAddLiteral(pToolTip, "");
				}
				ProfileStringArrayFormat(pToolTip, "%s excl", S.MetaCounters[i].pName);
				ProfileStringArrayFormat(pToolTip, "%5lld", (long long)nMetaSum[i]);
				ProfileStringArrayFormat(pToolTip, "%s incl", S.MetaCounters[i].pName);
				ProfileStringArrayFormat(pToolTip, "%5lld", (long long)(nMetaSum[i] + nMetaSumInclusive[i]));
			}
		}
	}
}

void UIApp::ProfileToolTipLabel(MicroProfileStringArray* pToolTip)
{
	if (UI.nRangeBeginIndex != UI.nRangeEndIndex && UI.pRangeLog)
	{
		bool bSpaced = false;
		int nStackDepth = 0;
		uint32_t nRange[2][2];
		MicroProfileThreadLog* pLog = UI.pRangeLog;

		MicroProfileGetRange(UI.nRangeEndIndex, UI.nRangeBeginIndex, nRange);
		for (uint32_t i = 0; i < 2; ++i)
		{
			uint32_t nStart = nRange[i][0];
			uint32_t nEnd = nRange[i][1];
			for (uint32_t j = nStart; j < nEnd; ++j)
			{
				MicroProfileLogEntry LE = pLog->Log[j];
				uint32_t nType = (uint32_t)MicroProfileLogType(LE);
				switch (nType)
				{
				case MP_LOG_LABEL:
				case MP_LOG_LABEL_LITERAL:
				{
					if (nStackDepth == 1)
					{
						uint64_t nLabel = MicroProfileLogGetTick(LE);
						const char* pLabelName = MicroProfileGetLabel(nType, nLabel);

						if (!bSpaced)
						{
							bSpaced = true;
							ProfileStringArrayAddLiteral(pToolTip, "");
							ProfileStringArrayAddLiteral(pToolTip, "");
						}

						if (pToolTip->nNumStrings + 2 <= MICROPROFILE_TOOLTIP_MAX_STRINGS)
						{
							ProfileStringArrayAddLiteral(pToolTip, "Label:");
							ProfileStringArrayAddLiteral(pToolTip, pLabelName ? pLabelName : "??");
						}
					}
				}
				break;
				case MP_LOG_LEAVE:
					if (nStackDepth)
					{
						nStackDepth--;
					}
					break;
				case MP_LOG_ENTER:
					nStackDepth++;
					break;
				}

			}
		}
	}
}

void UIApp::ProfileDrawFloatTooltip(Cmd* pCmd, uint32_t nX, uint32_t nY, uint32_t nToken, uint64_t nTime)
{
	MicroProfile& S = *MicroProfileGet();

	uint32_t nIndex = MicroProfileGetTimerIndex(nToken);
	uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
	uint32_t nAggregateCount = S.Aggregate[nIndex].nCount ? S.Aggregate[nIndex].nCount : 1;

	uint32_t nGroupId = MicroProfileGetGroupIndex(nToken);
	uint32_t nTimerId = MicroProfileGetTimerIndex(nToken);
	bool bGpu = S.GroupInfo[nGroupId].Type == MicroProfileTokenTypeGpu;

	float fToMs = MicroProfileTickToMsMultiplier(bGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu());

	float fMs = fToMs * (nTime);
	float fFrameMs = fToMs * (S.Frame[nIndex].nTicks);
	float fAverage = fToMs * (S.Aggregate[nIndex].nTicks / nAggregateFrames);
	float fCallAverage = fToMs * (S.Aggregate[nIndex].nTicks / nAggregateCount);
	float fMax = fToMs * (S.AggregateMax[nIndex]);
	float fMin = fToMs * (S.AggregateMin[nIndex]);

	float fFrameMsExclusive = fToMs * (S.FrameExclusive[nIndex]);
	float fAverageExclusive = fToMs * (S.AggregateExclusive[nIndex] / nAggregateFrames);
	float fMaxExclusive = fToMs * (S.AggregateMaxExclusive[nIndex]);

	float fGroupAverage = fToMs * (S.AggregateGroup[nGroupId] / nAggregateFrames);
	float fGroupMax = fToMs * (S.AggregateGroupMax[nGroupId]);
	float fGroup = fToMs * (S.FrameGroup[nGroupId]);


	MicroProfileStringArray ToolTip;
	ProfileStringArrayClear(&ToolTip);
	const char* pGroupName = S.GroupInfo[nGroupId].pName;
	const char* pTimerName = S.TimerInfo[nTimerId].pName;
	ProfileStringArrayAddLiteral(&ToolTip, "Timer:");
	ProfileStringArrayFormat(&ToolTip, "%s", pTimerName);

#if MICROPROFILE_DEBUG
	ProfileStringArrayFormat(&ToolTip, "0x%p", UI.nHoverAddressEnter);
	ProfileStringArrayFormat(&ToolTip, "0x%p", UI.nHoverAddressLeave);
#endif

	if (nTime != (uint64_t)0)
	{
		ProfileStringArrayAddLiteral(&ToolTip, "Time:");
		ProfileStringArrayFormat(&ToolTip, "%6.3fms", fMs);
		ProfileStringArrayAddLiteral(&ToolTip, "");
		ProfileStringArrayAddLiteral(&ToolTip, "");
	}

	ProfileStringArrayAddLiteral(&ToolTip, "Frame Time:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fFrameMs);

	ProfileStringArrayAddLiteral(&ToolTip, "Average:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fAverage);

	ProfileStringArrayAddLiteral(&ToolTip, "Max:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fMax);

	ProfileStringArrayAddLiteral(&ToolTip, "Min:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fMin);

	ProfileStringArrayAddLiteral(&ToolTip, "");
	ProfileStringArrayAddLiteral(&ToolTip, "");

	ProfileStringArrayAddLiteral(&ToolTip, "Call Average:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fCallAverage);

	ProfileStringArrayAddLiteral(&ToolTip, "Call Count:");
	ProfileStringArrayFormat(&ToolTip, "%6.2f", double(nAggregateCount) / nAggregateFrames);

	ProfileStringArrayAddLiteral(&ToolTip, "");
	ProfileStringArrayAddLiteral(&ToolTip, "");

	ProfileStringArrayAddLiteral(&ToolTip, "Exclusive Frame Time:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fFrameMsExclusive);

	ProfileStringArrayAddLiteral(&ToolTip, "Exclusive Average:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fAverageExclusive);

	ProfileStringArrayAddLiteral(&ToolTip, "Exclusive Max:");
	ProfileStringArrayFormat(&ToolTip, "%6.3fms", fMaxExclusive);

	ProfileStringArrayAddLiteral(&ToolTip, "");
	ProfileStringArrayAddLiteral(&ToolTip, "");

	ProfileStringArrayAddLiteral(&ToolTip, "Group:");
	ProfileStringArrayFormat(&ToolTip, "%s", pGroupName);
	ProfileStringArrayAddLiteral(&ToolTip, "Frame Time:");
	ProfileStringArrayFormat(&ToolTip, "%6.3f", fGroup);
	ProfileStringArrayAddLiteral(&ToolTip, "Frame Average:");
	ProfileStringArrayFormat(&ToolTip, "%6.3f", fGroupAverage);
	ProfileStringArrayAddLiteral(&ToolTip, "Frame Max:");
	ProfileStringArrayFormat(&ToolTip, "%6.3f", fGroupMax);

	ProfileToolTipMeta(&ToolTip);
	ProfileToolTipLabel(&ToolTip);

	ProfileDrawFloatWindow(pCmd, nX, nY + 20, &ToolTip.ppStrings[0], ToolTip.nNumStrings, S.TimerInfo[nTimerId].nColor);

	if (UI.nMouseLeftMod)
	{
		int nIndex = (g_MicroProfileUI.LockedToolTipFront + MICROPROFILE_TOOLTIP_MAX_LOCKED - 1) % MICROPROFILE_TOOLTIP_MAX_LOCKED;
		g_MicroProfileUI.nLockedToolTipColor[nIndex] = S.TimerInfo[nTimerId].nColor;
		ProfileStringArrayCopy(&g_MicroProfileUI.LockedToolTips[nIndex], &ToolTip);
		g_MicroProfileUI.LockedToolTipFront = nIndex;

	}
}

int64_t UIApp::ProfileGetGpuTickSync(int64_t nTickCpu, int64_t nTickGpu)
{
	if (UI.nTickReferenceCpu && UI.nTickReferenceGpu)
	{
		int64_t nTicksPerSecondCpu = MicroProfileTicksPerSecondCpu();
		int64_t nTicksPerSecondGpu = MicroProfileTicksPerSecondGpu();

		return (int64_t)((nTickCpu - UI.nTickReferenceCpu) * (double(nTicksPerSecondGpu) / double(nTicksPerSecondCpu)) + UI.nTickReferenceGpu);
	}
	else
	{
		return (int64_t)nTickGpu;
	}
}

void UIApp::ProfileZoomTo(int64_t nTickStart, int64_t nTickEnd, MicroProfileTokenType eToken)
{
	MicroProfile& S = *MicroProfileGet();

	bool bGpu = eToken == MicroProfileTokenTypeGpu;
	int64_t nStartCpu = S.Frames[S.nFrameCurrent].nFrameStartCpu;
	int64_t nStart = bGpu ? ProfileGetGpuTickSync(nStartCpu, S.Frames[S.nFrameCurrent].nFrameStartGpu) : nStartCpu;
	uint64_t nFrequency = bGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu();

	float fToMs = MicroProfileTickToMsMultiplier(nFrequency);
	UI.fDetailedOffsetTarget = MicroProfileLogTickDifference(nStart, nTickStart) * fToMs;
	UI.fDetailedRangeTarget = MicroProfileMax(MicroProfileLogTickDifference(nTickStart, nTickEnd) * fToMs, 0.01f); // clamp to 10us
}

void UIApp::ProfileCenter(int64_t nTickCenter)
{
	MicroProfile& S = *MicroProfileGet();
	int64_t nStart = S.Frames[S.nFrameCurrent].nFrameStartCpu;
	float fToMs = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondCpu());
	float fCenter = MicroProfileLogTickDifference(nStart, nTickCenter) * fToMs;
	UI.fDetailedOffsetTarget = UI.fDetailedOffset = fCenter - 0.5f * UI.fDetailedRange;
}

#if MICROPROFILE_DEBUG
uint64_t* g_pMicroProfileDumpStart = 0;
uint64_t* g_pMicroProfileDumpEnd = 0;
void MicroProfileDebugDumpRange()
{
	MicroProfile& S = *MicroProfileGet();
	if (g_pMicroProfileDumpStart != g_pMicroProfileDumpEnd)
	{
		uint64_t* pStart = g_pMicroProfileDumpStart;
		uint64_t* pEnd = g_pMicroProfileDumpEnd;
		while (pStart != pEnd)
		{
			uint64_t nTick = MicroProfileLogGetTick(*pStart);
			uint64_t nToken = MicroProfileLogTimerIndex(*pStart);
			uint32_t nTimerId = MicroProfileGetTimerIndex(nToken);

			const char* pTimerName = S.TimerInfo[nTimerId].pName;
			char buffer[256];
			uint64_t type = MicroProfileLogType(*pStart);

			const char* pBegin = type == MP_LOG_LEAVE ? "END" :
				(type == MP_LOG_ENTER ? "BEGIN" : "META");
			snprintf(buffer, 255, "DUMP 0x%p: %s :: %" PRIx64 ": %s\n", pStart, pBegin, nTick, pTimerName);
#ifdef _WIN32
			OutputDebugString(buffer);
#else
			printf("%s", buffer);
#endif
			pStart++;
		}

		g_pMicroProfileDumpStart = g_pMicroProfileDumpEnd;
	}
}
#define MP_DEBUG_DUMP_RANGE() MicroProfileDebugDumpRange()
#else
#define MP_DEBUG_DUMP_RANGE() do{} while(0)
#endif

#define MICROPROFILE_HOVER_DIST 0.5f

void UIApp::ProfileDrawDetailedContextSwitchBars(Cmd* pCmd, uint32_t nY, uint32_t nThreadId, uint32_t nContextSwitchStart, uint32_t nContextSwitchEnd, int64_t nBaseTicks, uint32_t nBaseY)
{
	MicroProfile& S = *MicroProfileGet();
	int64_t nTickIn = -1;
	uint32_t nThreadBefore = -1;
	float fToMs = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondCpu());
	float fMsToScreen = UI.nWidth / UI.fDetailedRange;
	float fMouseX = (float)UI.nMouseX;
	float fMouseY = (float)UI.nMouseY;

	int nLineDrawn = -1;

	for (uint32_t j = nContextSwitchStart; j != nContextSwitchEnd; j = (j + 1) % MICROPROFILE_CONTEXT_SWITCH_BUFFER_SIZE)
	{
		ASSERT(j < MICROPROFILE_CONTEXT_SWITCH_BUFFER_SIZE);
		MicroProfileContextSwitch CS = S.ContextSwitch[j];

		if (nTickIn == -1)
		{
			if (CS.nThreadIn == nThreadId)
			{
				nTickIn = CS.nTicks;
				nThreadBefore = CS.nThreadOut;
			}
		}
		else
		{
			if (CS.nThreadOut == nThreadId)
			{
				int64_t nTickOut = CS.nTicks;
				float fMsStart = fToMs * MicroProfileLogTickDifference(nBaseTicks, nTickIn);
				float fMsEnd = fToMs * MicroProfileLogTickDifference(nBaseTicks, nTickOut);
				if (fMsStart <= fMsEnd)
				{
					float fXStart = fMsStart * fMsToScreen;
					float fXEnd = fMsEnd * fMsToScreen;
					float fYStart = (float)nY;
					float fYEnd = fYStart + (MICROPROFILE_DETAILED_CONTEXT_SWITCH_HEIGHT);
					uint32_t nColor = g_nMicroProfileContextSwitchThreadColors[CS.nCpu%MICROPROFILE_NUM_CONTEXT_SWITCH_COLORS];
					float fXDist = MicroProfileMax(fXStart - fMouseX, fMouseX - fXEnd);
					bool bHover = fXDist < MICROPROFILE_HOVER_DIST && fYStart <= fMouseY && fMouseY <= fYEnd && nBaseY < fMouseY;
					if (bHover)
					{
						UI.nRangeBegin = nTickIn;
						UI.nRangeEnd = nTickOut;
						S.nContextSwitchHoverTickIn = nTickIn;
						S.nContextSwitchHoverTickOut = nTickOut;
						S.nContextSwitchHoverThread = CS.nThreadOut;
						S.nContextSwitchHoverThreadBefore = nThreadBefore;
						S.nContextSwitchHoverThreadAfter = CS.nThreadIn;
						S.nContextSwitchHoverCpuNext = CS.nCpu;
						nColor = UI.nHoverColor;
					}
					if (CS.nCpu == S.nContextSwitchHoverCpu)
					{
						nColor = UI.nHoverColorShared;
					}

					uint32_t nIntegerWidth = (uint32_t)(fXEnd - fXStart);
					if (nIntegerWidth)
					{
						ProfileDrawBox(pCmd, (int)fXStart, (int)fYStart, (int)fXEnd, (int)fYEnd, nColor | UI.nOpacityForeground, MicroProfileBoxTypeFlat);
					}
					else
					{
						float fXAvg = 0.5f * (fXStart + fXEnd);
						int nLineX = (int)floor(fXAvg + 0.5f);

						if (nLineDrawn != nLineX)
						{
							nLineDrawn = nLineX;
							ProfileDrawLineVertical(pCmd, nLineX, (int)(fYStart + 0.5f), int(fYEnd + 0.5f), nColor | UI.nOpacityForeground);
						}
					}
				}
				nTickIn = -1;
			}
		}
	}
}

void UIApp::ProfileWriteThreadHeader(Cmd* pCmd, uint32_t nY, MicroProfileThreadIdType ThreadId, const char* pNamedThread, const char* pThreadModule)
{
	char Buffer[512];
	if (pThreadModule)
	{
		snprintf(Buffer, sizeof(Buffer) - 1, "%04x: %s [%s]", (uint32_t)ThreadId, pNamedThread ? pNamedThread : "", pThreadModule);
	}
	else
	{
		snprintf(Buffer, sizeof(Buffer) - 1, "%04x: %s", (uint32_t)ThreadId, pNamedThread ? pNamedThread : "");
	}
	uint32_t nStrLen = (uint32_t)strlen(Buffer);
	ProfileDrawTextBackground(pCmd, 10, nY, 0xffffff, 0x88777777, Buffer, nStrLen);
}

uint32_t UIApp::ProfileWriteProcessHeader(Cmd* pCmd, uint32_t nY, uint32_t nProcessId)
{
	char Name[256];
	const char* pProcessName = MicroProfileGetProcessName(nProcessId, Name, sizeof(Name));

	char Buffer[512];
	nY += MICROPROFILE_TEXT_HEIGHT + 1;
	if (pProcessName)
	{
		snprintf(Buffer, sizeof(Buffer) - 1, "* %04x: %s", nProcessId, pProcessName);
	}
	else
	{
		snprintf(Buffer, sizeof(Buffer) - 1, "* %04x", nProcessId);
	}
	uint32_t nStrLen = (uint32_t)strlen(Buffer);
	ProfileDrawTextBackground(pCmd, 0, nY, 0xffffff, 0x88777777, Buffer, nStrLen);
	nY += MICROPROFILE_TEXT_HEIGHT + 1;
	return nY;
}

void UIApp::ProfileGetFrameRange(int64_t nTicks, int64_t nTicksEnd, int32_t nLogIndex, uint32_t* nFrameBegin, uint32_t* nFrameEnd)
{
	MicroProfile& S = *MicroProfileGet();
	ASSERT(nLogIndex < 0 || S.Pool[nLogIndex]);

	bool bGpu = (nLogIndex >= 0) ? S.Pool[nLogIndex]->nGpu != 0 : false;
	uint32_t nPut = (nLogIndex >= 0) ? S.Pool[nLogIndex]->nPut.load(std::memory_order_relaxed) : 0;

	uint32_t nBegin = S.nFrameCurrent;

	for (uint32_t i = 0; i < MICROPROFILE_MAX_FRAME_HISTORY - MICROPROFILE_GPU_FRAME_DELAY; ++i)
	{
		uint32_t nFrame = (S.nFrameCurrent + MICROPROFILE_MAX_FRAME_HISTORY - i) % MICROPROFILE_MAX_FRAME_HISTORY;

		if (nLogIndex >= 0)
		{
			uint32_t nCurrStart = S.Frames[nBegin].nLogStart[nLogIndex];
			uint32_t nPrevStart = S.Frames[nFrame].nLogStart[nLogIndex];
			bool bOverflow = (nPrevStart <= nCurrStart) ? (nPut >= nPrevStart && nPut < nCurrStart) : (nPut < nCurrStart || nPut >= nPrevStart);
			if (bOverflow)
				break;
		}

		nBegin = nFrame;
		if ((bGpu ? S.Frames[nBegin].nFrameStartGpu : S.Frames[nBegin].nFrameStartCpu) <= nTicks)
			break;
	}

	uint32_t nEnd = nBegin;

	while (nEnd != S.nFrameCurrent)
	{
		nEnd = (nEnd + 1) % MICROPROFILE_MAX_FRAME_HISTORY;
		if ((bGpu ? S.Frames[nEnd].nFrameStartGpu : S.Frames[nEnd].nFrameStartCpu) >= nTicksEnd)
			break;
	}

	*nFrameBegin = nBegin;
	*nFrameEnd = nEnd;
}

void UIApp::ProfileDrawDetailedBars(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight, int nBaseY, int nSelectedFrame)
{
	MicroProfile& S = *MicroProfileGet();
	MP_DEBUG_DUMP_RANGE();
	int nY = nBaseY - UI.nOffsetY[MP_DRAW_DETAILED];
	int64_t nNumBoxes = 0;
	int64_t nNumLines = 0;

	UI.nRangeBegin = 0;
	UI.nRangeEnd = 0;
	UI.nRangeBeginGpu = 0;
	UI.nRangeEndGpu = 0;
	UI.nRangeBeginIndex = UI.nRangeEndIndex = 0;
	UI.pRangeLog = 0;

	int64_t nFrameStartCpu = S.Frames[S.nFrameCurrent].nFrameStartCpu;
	int64_t nFrameStartGpu = S.Frames[S.nFrameCurrent].nFrameStartGpu;
	int64_t nTicksPerSecondCpu = MicroProfileTicksPerSecondCpu();
	int64_t nTicksPerSecondGpu = MicroProfileTicksPerSecondGpu();
	float fToMsCpu = MicroProfileTickToMsMultiplier(nTicksPerSecondCpu);
	float fToMsGpu = MicroProfileTickToMsMultiplier(nTicksPerSecondGpu);

	if (!S.nRunning && UI.nTickReferenceCpu < nFrameStartCpu)
	{
		int64_t nRefCpu = 0, nRefGpu = 0;
		if (MicroProfileGetGpuTickReference(&nRefCpu, &nRefGpu))
		{
			UI.nTickReferenceCpu = nRefCpu;
			UI.nTickReferenceGpu = nRefGpu;
		}
	}

	float fDetailedOffset = UI.fDetailedOffset;
	float fDetailedRange = UI.fDetailedRange;

	int64_t nDetailedOffsetTicksCpu = MicroProfileMsToTick(fDetailedOffset, MicroProfileTicksPerSecondCpu());
	int64_t nDetailedOffsetTicksGpu = MicroProfileMsToTick(fDetailedOffset, MicroProfileTicksPerSecondGpu());
	int64_t nBaseTicksCpu = nDetailedOffsetTicksCpu + nFrameStartCpu;
	int64_t nBaseTicksGpu = ProfileGetGpuTickSync(nBaseTicksCpu, nDetailedOffsetTicksGpu + nFrameStartGpu);
	int64_t nBaseTicksEndCpu = nBaseTicksCpu + MicroProfileMsToTick(fDetailedRange, MicroProfileTicksPerSecondCpu());
	int64_t nBaseTicksEndGpu = nBaseTicksGpu + MicroProfileMsToTick(fDetailedRange, MicroProfileTicksPerSecondGpu());

	uint32_t nFrameBegin, nFrameEnd;
	ProfileGetFrameRange(nBaseTicksCpu, nBaseTicksEndCpu, -1, &nFrameBegin, &nFrameEnd);

	float fMsBase = fToMsCpu * nDetailedOffsetTicksCpu;
	float fMs = fDetailedRange;
	float fMsEnd = fMs + fMsBase;
	float fWidth = (float)nWidth;
	float fMsToScreen = fWidth / fMs;

	for (uint32_t i = nFrameBegin; i != nFrameEnd; i = (i + 1) % MICROPROFILE_MAX_FRAME_HISTORY)
	{
		uint64_t nTickStart = S.Frames[i].nFrameStartCpu;
		float fMsStart = fToMsCpu * MicroProfileLogTickDifference(nBaseTicksCpu, nTickStart);
		float fXStart = fMsStart * fMsToScreen;

		ProfileDrawLineVertical(pCmd, (int)(fXStart), nBaseY, nBaseY + nHeight, UI.nOpacityForeground | 0xbbbbbb);
	}

	{
		float fRate = floor(2 * (log10(fMs) - 1)) / 2;
		float fStep = powf(10.f, fRate);
		float fRcpStep = 1.f / fStep;
		int nColorIndex = (int)(floor(fMsBase*fRcpStep));
		float fStart = floor(fMsBase*fRcpStep) * fStep;

		char StepLabel[64] = "";
		if (fStep >= 0.005 && fStep <= 1000)
		{
			if (fStep >= 1)
				sprintf_s(StepLabel, "%.3gms", fStep);
			else
				sprintf_s(StepLabel, "%.2fms", fStep);
		}

		uint32_t nStepLabelLength = (uint32_t)strlen(StepLabel);
		float fStepLabelOffset = (fStep*fMsToScreen - nStepLabelLength * (MICROPROFILE_TEXT_WIDTH + 1)) / 2;

		for (float f = fStart; f < fMsEnd; )
		{
			float fStart = f;
			float fNext = f + fStep;
			ProfileDrawBox(pCmd, (int)((fStart - fMsBase) * fMsToScreen), nBaseY, (int)((fNext - fMsBase) * fMsToScreen + 1), nBaseY + nHeight, UI.nOpacityBackground | g_nMicroProfileBackColors[nColorIndex++ & 1], MicroProfileBoxTypeFlat);

			if (nStepLabelLength)
				ProfileDrawText(pCmd, (int)((fStart - fMsBase) * fMsToScreen + fStepLabelOffset), nBaseY, UI.nOpacityForeground | 0x808080, StepLabel, nStepLabelLength);

			f = fNext;
		}
	}

	nY += MICROPROFILE_TEXT_HEIGHT + 1;

	MicroProfileLogEntry* pMouseOver = UI.pDisplayMouseOver;
	MicroProfileLogEntry* pMouseOverNext = 0;
	uint64_t nMouseOverToken = pMouseOver ? UI.nDisplayMouseOverTimerIndex : MICROPROFILE_INVALID_TOKEN;

	float fMouseX = (float)UI.nMouseX;
	float fMouseY = (float)UI.nMouseY;
	uint64_t nHoverToken = MICROPROFILE_INVALID_TOKEN;
	int64_t nHoverTime = 0;

	static int nHoverCounter = 155;
	static int nHoverCounterDelta = 10;
	nHoverCounter += nHoverCounterDelta;
	if (nHoverCounter >= 245)
		nHoverCounterDelta = -10;
	else if (nHoverCounter < 100)
		nHoverCounterDelta = 10;
	UI.nHoverColor = (nHoverCounter << 24) | (nHoverCounter << 16) | (nHoverCounter << 8) | nHoverCounter;
	uint32_t nHoverCounterShared = nHoverCounter >> 2;
	UI.nHoverColorShared = (nHoverCounterShared << 24) | (nHoverCounterShared << 16) | (nHoverCounterShared << 8) | nHoverCounterShared;

	uint32_t nLinesDrawn[MICROPROFILE_STACK_MAX] = { 0 };

	S.nContextSwitchHoverThread = S.nContextSwitchHoverThreadAfter = S.nContextSwitchHoverThreadBefore = -1;

	uint32_t nContextSwitchStart = -1;
	uint32_t nContextSwitchEnd = -1;
	S.nContextSwitchHoverCpuNext = 0xff;
	S.nContextSwitchHoverTickIn = -1;
	S.nContextSwitchHoverTickOut = -1;
	if (S.bContextSwitchRunning)
	{
		MicroProfileContextSwitchSearch(&nContextSwitchStart, &nContextSwitchEnd, nBaseTicksCpu, nBaseTicksEndCpu);
	}

	uint64_t nActiveGroup = S.nAllGroupsWanted ? S.nGroupMask : S.nActiveGroupWanted;

	bool bSkipBarView = S.bContextSwitchRunning && S.bContextSwitchNoBars;

	if (!bSkipBarView)
	{
		for (uint32_t i = 0; i < MICROPROFILE_MAX_THREADS; ++i)
		{
			MicroProfileThreadLog* pLog = S.Pool[i];
			if (!pLog)
				continue;

			bool bGpu = pLog->nGpu != 0;
			float fToMs = bGpu ? fToMsGpu : fToMsCpu;
			int64_t nBaseTicks = bGpu ? nBaseTicksGpu : nBaseTicksCpu;
			int64_t nBaseTicksEnd = bGpu ? nBaseTicksEndGpu : nBaseTicksEndCpu;
			MicroProfileThreadIdType nThreadId = pLog->nThreadId;

			int64_t nGapTime = (bGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu()) * MICROPROFILE_GAP_TIME / 1000;

			uint32_t nLogFrameBegin, nLogFrameEnd;
			ProfileGetFrameRange(nBaseTicks - nGapTime, nBaseTicksEnd + nGapTime, i, &nLogFrameBegin, &nLogFrameEnd);

			uint32_t nGet = S.Frames[nLogFrameBegin].nLogStart[i];
			uint32_t nPut = nLogFrameEnd == S.nFrameCurrent ? pLog->nPut.load(std::memory_order_relaxed) : S.Frames[nLogFrameEnd].nLogStart[i];
			if (nPut == nGet)
				continue;

			uint32_t nRange[2][2] = { {0, 0}, {0, 0}, };
			MicroProfileGetRange(nPut, nGet, nRange);

			uint32_t nMaxStackDepth = 0;

			nY += 3;
			ProfileWriteThreadHeader(pCmd, nY, nThreadId, &pLog->ThreadName[0], NULL);
			nY += 3;
			nY += MICROPROFILE_TEXT_HEIGHT + 1;

			if (S.bContextSwitchRunning)
			{
				ProfileDrawDetailedContextSwitchBars(pCmd, nY, pLog->nThreadId, nContextSwitchStart, nContextSwitchEnd, nBaseTicks, nBaseY);
				nY -= MICROPROFILE_DETAILED_BAR_HEIGHT;
				nY += MICROPROFILE_DETAILED_CONTEXT_SWITCH_HEIGHT + 1;
			}

			uint32_t nYDelta = MICROPROFILE_DETAILED_BAR_HEIGHT;
			uint32_t nStack[MICROPROFILE_STACK_MAX];
			uint32_t nStackPos = 0;
			for (uint32_t j = 0; j < 2; ++j)
			{
				uint32_t nStart = nRange[j][0];
				uint32_t nEnd = nRange[j][1];
				for (uint32_t k = nStart; k < nEnd; ++k)
				{
					MicroProfileLogEntry* pEntry = pLog->Log + k;
					uint64_t nType = MicroProfileLogType(*pEntry);
					if (MP_LOG_ENTER == nType)
					{
						ASSERT(nStackPos < MICROPROFILE_STACK_MAX);
						nStack[nStackPos++] = k;
					}
					else if (MP_LOG_META == nType)
					{

					}
					else if (MP_LOG_LEAVE == nType)
					{
						if (0 == nStackPos)
						{
							continue;
						}

						MicroProfileLogEntry* pEntryEnter = pLog->Log + nStack[nStackPos - 1];
						if (MicroProfileLogTimerIndex(*pEntryEnter) != MicroProfileLogTimerIndex(*pEntry))
						{
							//uprintf("mismatch %llx %llx\n", pEntryEnter->nToken, pEntry->nToken);
							continue;
						}
						int64_t nTickStart = MicroProfileLogGetTick(*pEntryEnter);
						int64_t nTickEnd = MicroProfileLogGetTick(*pEntry);
						uint64_t nTimerIndex = MicroProfileLogTimerIndex(*pEntry);
						uint32_t nColor = S.TimerInfo[nTimerIndex].nColor;
						if (!(nActiveGroup & (1ull << S.TimerInfo[nTimerIndex].nGroupIndex)))
						{
							nStackPos--;
							continue;
						}
						if (nMouseOverToken == nTimerIndex)
						{
							if (pEntry == pMouseOver)
							{
								nColor = UI.nHoverColor;
								if (bGpu)
								{
									UI.nRangeBeginGpu = *pEntryEnter;
									UI.nRangeEndGpu = *pEntry;
									uint32_t nCpuBegin = (nStack[nStackPos - 1] + 1) % MICROPROFILE_BUFFER_SIZE;
									uint32_t nCpuEnd = (k + 1) % MICROPROFILE_BUFFER_SIZE;
									MicroProfileLogEntry LogCpuBegin = pLog->Log[nCpuBegin];
									MicroProfileLogEntry LogCpuEnd = pLog->Log[nCpuEnd];
									if (MicroProfileLogType(LogCpuBegin) == MP_LOG_GPU_EXTRA && MicroProfileLogType(LogCpuEnd) == MP_LOG_GPU_EXTRA)
									{
										UI.nRangeBegin = LogCpuBegin;
										UI.nRangeEnd = LogCpuEnd;
									}
									UI.nRangeBeginIndex = nStack[nStackPos - 1];
									UI.nRangeEndIndex = k;
									UI.pRangeLog = pLog;
								}
								else
								{
									UI.nRangeBegin = *pEntryEnter;
									UI.nRangeEnd = *pEntry;
									UI.nRangeBeginIndex = nStack[nStackPos - 1];
									UI.nRangeEndIndex = k;
									UI.pRangeLog = pLog;

								}
							}
							else
							{
								nColor = UI.nHoverColorShared;
							}
						}

						const char* pName = S.TimerInfo[nTimerIndex].pName;
						uint32_t nNameLen = S.TimerInfo[nTimerIndex].nNameLen;

						if (pName[0] == '$' && pEntryEnter < pEntry)
						{
							MicroProfileLogEntry e = pEntryEnter[1 + bGpu];
							uint64_t nLogType = MicroProfileLogType(e);
							if (nLogType == MP_LOG_LABEL || nLogType == MP_LOG_LABEL_LITERAL)
							{
								const char* pLabel = MicroProfileGetLabel((uint32_t)nLogType, (uint32_t)MicroProfileLogGetTick(e));

								if (pLabel)
								{
									pName = pLabel;
									nNameLen = (uint32_t)strlen(pLabel);
								}
							}
						}

						nMaxStackDepth = MicroProfileMax(nMaxStackDepth, nStackPos);
						float fMsStart = fToMs * MicroProfileLogTickDifference(nBaseTicks, nTickStart);
						float fMsEnd = fToMs * MicroProfileLogTickDifference(nBaseTicks, nTickEnd);
						float fXStart = fMsStart * fMsToScreen;
						float fXEnd = fMsEnd * fMsToScreen;
						float fYStart = (float)(nY + nStackPos * nYDelta);
						float fYEnd = fYStart + (MICROPROFILE_DETAILED_BAR_HEIGHT);
						float fXDist = MicroProfileMax(fXStart - fMouseX, fMouseX - fXEnd);
						bool bHover = fXDist < MICROPROFILE_HOVER_DIST && fYStart <= fMouseY && fMouseY <= fYEnd && nBaseY < fMouseY;
						uint32_t nIntegerWidth = (uint32_t)(fXEnd - fXStart);
						if (nIntegerWidth)
						{
							if (bHover && UI.nActiveMenu == (uint32_t)-1)
							{
								nHoverToken = MicroProfileLogTimerIndex(*pEntry);
#if MICROPROFILE_DEBUG
								UI.nHoverAddressEnter = (uint64_t)pEntryEnter;
								UI.nHoverAddressLeave = (uint64_t)pEntry;
#endif
								nHoverTime = MicroProfileLogTickDifference(nTickStart, nTickEnd);
								pMouseOverNext = pEntry;
							}

							ProfileDrawBox(pCmd, (int)fXStart, (int)fYStart, (int)fXEnd, (int)fYEnd, nColor | UI.nOpacityForeground, MicroProfileBoxTypeBar);
#if MICROPROFILE_DETAILED_BAR_NAMES
							if (nIntegerWidth > 3 * MICROPROFILE_TEXT_WIDTH)
							{
								float fXStartText = MicroProfileMax(fXStart, 0.f);
								int nTextWidth = (int)(fXEnd - fXStartText);
								int nCharacters = (nTextWidth - MICROPROFILE_TEXT_WIDTH) / (MICROPROFILE_TEXT_WIDTH + 1);
								if (nCharacters > 0)
								{
									ProfileDrawText(pCmd, (int)(fXStartText + 1), (int)(fYStart + 1), -1, pName, MicroProfileMin<uint32_t>(nNameLen, nCharacters));
								}
							}
#endif
							++nNumBoxes;
						}
						else
						{
							float fXAvg = 0.5f * (fXStart + fXEnd);
							int nLineX = (int)floor(fXAvg + 0.5f);
							if (nLineX != (int)nLinesDrawn[nStackPos])
							{
								if (bHover && UI.nActiveMenu == (uint32_t)-1)
								{
									nHoverToken = (uint32_t)MicroProfileLogTimerIndex(*pEntry);
									nHoverTime = MicroProfileLogTickDifference(nTickStart, nTickEnd);
									pMouseOverNext = pEntry;
								}
								nLinesDrawn[nStackPos] = nLineX;
								ProfileDrawLineVertical(pCmd, nLineX, (int)(fYStart + 0.5f), (int)(fYEnd + 0.5f), nColor | UI.nOpacityForeground);
								++nNumLines;
							}
						}
						nStackPos--;

						if (0 == nStackPos && MicroProfileLogTickDifference(nTickEnd, nBaseTicksEnd) < 0)
						{
							break;
						}
					}
				}
			}
			nY += nMaxStackDepth * nYDelta + MICROPROFILE_DETAILED_BAR_HEIGHT + 1;
		}
	}
	if (S.bContextSwitchRunning && (S.bContextSwitchAllThreads || S.bContextSwitchNoBars))
	{
		uint32_t nContextSwitchSearchEnd = S.bContextSwitchAllThreads ? nContextSwitchEnd : nContextSwitchStart;

		MicroProfileThreadInfo Threads[MICROPROFILE_MAX_CONTEXT_SWITCH_THREADS];
		uint32_t nNumThreadsBase = 0;
		uint32_t nNumThreads = MicroProfileContextSwitchGatherThreads(nContextSwitchStart, nContextSwitchSearchEnd, Threads, &nNumThreadsBase);

		//std::sort(&Threads[nNumThreadsBase], &Threads[nNumThreads],
		//			[](const MicroProfileThreadInfo& l, const MicroProfileThreadInfo& r)
		//{
		//	return l.nProcessId == r.nProcessId ? l.nThreadId < r.nThreadId : l.nProcessId > r.nProcessId;
		//});

		uint32_t nStart = nNumThreadsBase;
		if (S.bContextSwitchNoBars)
			nStart = 0;
		MicroProfileProcessIdType nLastProcessId = MP_GETCURRENTPROCESSID();
		for (uint32_t i = nStart; i < nNumThreads; ++i)
		{
			MicroProfileThreadInfo tt = Threads[i];
			if (tt.nThreadId)
			{
				if (nLastProcessId != tt.nProcessId)
				{
					nY = ProfileWriteProcessHeader(pCmd, nY, (uint32_t)tt.nProcessId);
					nLastProcessId = tt.nProcessId;
				}

				ProfileDrawDetailedContextSwitchBars(pCmd, nY + 2, tt.nThreadId, nContextSwitchStart, nContextSwitchEnd, nBaseTicksCpu, nBaseY);

				ProfileWriteThreadHeader(pCmd, nY, tt.nThreadId, i < nNumThreadsBase && S.Pool[i] ? &S.Pool[i]->ThreadName[0] : NULL, NULL);
				nY += MICROPROFILE_TEXT_HEIGHT + 1;
			}
		}
	}

	S.nContextSwitchHoverCpu = S.nContextSwitchHoverCpuNext;




	UI.pDisplayMouseOver = pMouseOverNext;
	UI.nDisplayMouseOverTimerIndex = pMouseOverNext ? MicroProfileLogTimerIndex(*pMouseOverNext) : MICROPROFILE_INVALID_TOKEN;

	if (!S.nRunning)
	{
		if (nHoverToken != MICROPROFILE_INVALID_TOKEN && nHoverTime)
		{
			UI.nHoverToken = nHoverToken;
			UI.nHoverTime = nHoverTime;
		}

		if (nSelectedFrame != -1)
		{
			UI.nRangeBegin = S.Frames[nSelectedFrame].nFrameStartCpu;
			UI.nRangeEnd = S.Frames[(nSelectedFrame + 1) % MICROPROFILE_MAX_FRAME_HISTORY].nFrameStartCpu;
			UI.nRangeBeginGpu = S.Frames[nSelectedFrame].nFrameStartGpu;
			UI.nRangeEndGpu = S.Frames[(nSelectedFrame + 1) % MICROPROFILE_MAX_FRAME_HISTORY].nFrameStartGpu;
		}
		if (UI.nRangeBegin != UI.nRangeEnd)
		{
			float fMsStart = fToMsCpu * MicroProfileLogTickDifference(nBaseTicksCpu, UI.nRangeBegin);
			float fMsEnd = fToMsCpu * MicroProfileLogTickDifference(nBaseTicksCpu, UI.nRangeEnd);
			float fXStart = fMsStart * fMsToScreen;
			float fXEnd = fMsEnd * fMsToScreen;
			ProfileDrawBox(pCmd, (int)fXStart, nBaseY, (int)fXEnd, nHeight, MICROPROFILE_FRAME_COLOR_HIGHTLIGHT, MicroProfileBoxTypeFlat);
			ProfileDrawLineVertical(pCmd, (int)fXStart, nBaseY, nHeight, MICROPROFILE_FRAME_COLOR_HIGHTLIGHT | 0x44000000);
			ProfileDrawLineVertical(pCmd, (int)fXEnd, nBaseY, nHeight, MICROPROFILE_FRAME_COLOR_HIGHTLIGHT | 0x44000000);

			fMsStart += fDetailedOffset;
			fMsEnd += fDetailedOffset;
			char sBuffer[32];
			snprintf(sBuffer, sizeof(sBuffer) - 1, "%.2fms", fMsStart);
			uint32_t nLenStart = (uint32_t)strlen(sBuffer);
			float fStartTextWidth = (float)((1 + MICROPROFILE_TEXT_WIDTH) * nLenStart);
			float fStartTextX = fXStart - fStartTextWidth - 2;
			ProfileDrawBox(pCmd, (int)fStartTextX, nBaseY, (int)(fStartTextX + fStartTextWidth + 2), MICROPROFILE_TEXT_HEIGHT + 2 + nBaseY, 0x33000000, MicroProfileBoxTypeFlat);
			ProfileDrawText(pCmd, (int)fStartTextX + 1, nBaseY, (uint32_t)-1, sBuffer, nLenStart);
			snprintf(sBuffer, sizeof(sBuffer) - 1, "%.2fms", fMsEnd);
			uint32_t nLenEnd = (uint32_t)strlen(sBuffer);
			ProfileDrawBox(pCmd, (int)(fXEnd + 1), nBaseY, (int)(fXEnd + 1 + (1 + MICROPROFILE_TEXT_WIDTH) * nLenEnd + 3), MICROPROFILE_TEXT_HEIGHT + 2 + nBaseY, 0x33000000, MicroProfileBoxTypeFlat);
			ProfileDrawText(pCmd, (int)(fXEnd + 2), nBaseY + 1, (uint32_t)-1, sBuffer, nLenEnd);

			if (UI.nMouseRight)
			{
				ProfileZoomTo(UI.nRangeBegin, UI.nRangeEnd, MicroProfileTokenTypeCpu);
			}
		}

		if (UI.nRangeBeginGpu != UI.nRangeEndGpu)
		{
			float fMsStart = fToMsGpu * MicroProfileLogTickDifference(nBaseTicksGpu, UI.nRangeBeginGpu);
			float fMsEnd = fToMsGpu * MicroProfileLogTickDifference(nBaseTicksGpu, UI.nRangeEndGpu);
			float fXStart = fMsStart * fMsToScreen;
			float fXEnd = fMsEnd * fMsToScreen;
			ProfileDrawBox(pCmd, (int)fXStart, nBaseY, (int)fXEnd, nHeight, MICROPROFILE_FRAME_COLOR_HIGHTLIGHT_GPU, MicroProfileBoxTypeFlat);
			ProfileDrawLineVertical(pCmd, (int)fXStart, nBaseY, nHeight, MICROPROFILE_FRAME_COLOR_HIGHTLIGHT_GPU | 0x44000000);
			ProfileDrawLineVertical(pCmd, (int)fXEnd, nBaseY, nHeight, MICROPROFILE_FRAME_COLOR_HIGHTLIGHT_GPU | 0x44000000);

			nBaseY += MICROPROFILE_TEXT_HEIGHT + 1;

			fMsStart += fDetailedOffset;
			fMsEnd += fDetailedOffset;
			char sBuffer[32];
			snprintf(sBuffer, sizeof(sBuffer) - 1, "%.2fms", fMsStart);
			uint32_t nLenStart = (uint32_t)strlen(sBuffer);
			float fStartTextWidth = (float)((1 + MICROPROFILE_TEXT_WIDTH) * nLenStart);
			float fStartTextX = fXStart - fStartTextWidth - 2;
			ProfileDrawBox(pCmd, (int)fStartTextX, nBaseY, (int)(fStartTextX + fStartTextWidth + 2), MICROPROFILE_TEXT_HEIGHT + 2 + nBaseY, 0x33000000, MicroProfileBoxTypeFlat);
			ProfileDrawText(pCmd, (int)(fStartTextX + 1), nBaseY, (uint32_t)-1, sBuffer, nLenStart);
			snprintf(sBuffer, sizeof(sBuffer) - 1, "%.2fms", fMsEnd);
			uint32_t nLenEnd = (uint32_t)strlen(sBuffer);
			ProfileDrawBox(pCmd, (int)(fXEnd + 1), nBaseY, (int)(fXEnd + 1 + (1 + MICROPROFILE_TEXT_WIDTH) * nLenEnd + 3), MICROPROFILE_TEXT_HEIGHT + 2 + nBaseY, 0x33000000, MicroProfileBoxTypeFlat);
			ProfileDrawText(pCmd, (int)(fXEnd + 2), nBaseY + 1, (uint32_t)-1, sBuffer, nLenEnd);

			if (UI.nMouseRight)
			{
				ProfileZoomTo(UI.nRangeBeginGpu, UI.nRangeEndGpu, MicroProfileTokenTypeGpu);
			}
		}
	}
}


void UIApp::ProfileDrawDetailedFrameHistory(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight, uint32_t nBaseY, uint32_t nSelectedFrame)
{
	(void)nHeight;

	MicroProfile& S = *MicroProfileGet();

	const uint32_t nBarHeight = MICROPROFILE_FRAME_HISTORY_HEIGHT;
	float fBaseX = (float)nWidth;
	float fDx = fBaseX / MICROPROFILE_NUM_FRAMES;

	uint32_t nLastIndex = (S.nFrameCurrent + 1) % MICROPROFILE_MAX_FRAME_HISTORY;
	ProfileDrawBox(0, nBaseY, nWidth, nBaseY + MICROPROFILE_FRAME_HISTORY_HEIGHT, 0xff000000 | g_nMicroProfileBackColors[0], MicroProfileBoxTypeFlat);
	float fToMs = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondCpu()) * S.fRcpReferenceTime;
	float fToMsGpu = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondGpu()) * S.fRcpReferenceTime;


	MicroProfileFrameState* pFrameCurrent = &S.Frames[S.nFrameCurrent];
	uint64_t nFrameStartCpu = pFrameCurrent->nFrameStartCpu;
	int64_t nDetailedOffsetTicksCpu = MicroProfileMsToTick(UI.fDetailedOffset, MicroProfileTicksPerSecondCpu());
	int64_t nCpuStart = nDetailedOffsetTicksCpu + nFrameStartCpu;
	int64_t nCpuEnd = nCpuStart + MicroProfileMsToTick(UI.fDetailedRange, MicroProfileTicksPerSecondCpu());;


	float fSelectionStart = (float)nWidth;
	float fSelectionEnd = 0.f;
	for (uint32_t i = 0; i < MICROPROFILE_NUM_FRAMES; ++i)
	{
		uint32_t nIndex = (S.nFrameCurrent + MICROPROFILE_MAX_FRAME_HISTORY - i) % MICROPROFILE_MAX_FRAME_HISTORY;
		MicroProfileFrameState* pCurrent = &S.Frames[nIndex];
		MicroProfileFrameState* pNext = &S.Frames[nLastIndex];

		int64_t nTicks = pNext->nFrameStartCpu - pCurrent->nFrameStartCpu;
		int64_t nTicksGpu = pNext->nFrameStartGpu - pCurrent->nFrameStartGpu;
		float fScale = fToMs * nTicks;
		float fScaleGpu = fToMsGpu * nTicksGpu;
		fScale = fScale > 1.f ? 0.f : 1.f - fScale;
		fScaleGpu = fScaleGpu > 1.f ? 0.f : 1.f - fScaleGpu;
		float fXEnd = fBaseX;
		float fXStart = fBaseX - fDx;
		fBaseX = fXStart;
		uint32_t nColor = MICROPROFILE_FRAME_HISTORY_COLOR_CPU;
		if (nIndex == nSelectedFrame)
			nColor = (uint32_t)-1;
		ProfileDrawBox(pCmd, (int)fXStart, (int)(nBaseY + fScale * nBarHeight), (int)fXEnd, nBaseY + MICROPROFILE_FRAME_HISTORY_HEIGHT, nColor, MicroProfileBoxTypeBar);
		if (pNext->nFrameStartCpu > nCpuStart)
		{
			fSelectionStart = fXStart;
		}
		if (pCurrent->nFrameStartCpu < nCpuEnd && fSelectionEnd == 0.f)
		{
			fSelectionEnd = fXEnd;
		}
		nLastIndex = nIndex;
	}
	ProfileDrawBox(pCmd, (int)fSelectionStart, nBaseY, (int)fSelectionEnd, nBaseY + MICROPROFILE_FRAME_HISTORY_HEIGHT, MICROPROFILE_FRAME_HISTORY_COLOR_HIGHTLIGHT, MicroProfileBoxTypeFlat);
}
void UIApp::ProfileDrawDetailedView(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight, bool bDrawBars)
{
	MicroProfile& S = *MicroProfileGet();

	MICROPROFILE_SCOPE(g_MicroProfileDetailed);
	uint32_t nBaseY = MICROPROFILE_TEXT_HEIGHT + 1;

	int nSelectedFrame = -1;
	if (UI.nMouseY > nBaseY && UI.nMouseY <= nBaseY + MICROPROFILE_FRAME_HISTORY_HEIGHT && UI.nActiveMenu == (uint32_t)-1)
	{

		nSelectedFrame = ((MICROPROFILE_NUM_FRAMES) * (UI.nWidth - UI.nMouseX) / UI.nWidth);
		nSelectedFrame = (S.nFrameCurrent + MICROPROFILE_MAX_FRAME_HISTORY - nSelectedFrame) % MICROPROFILE_MAX_FRAME_HISTORY;
		UI.nHoverFrame = nSelectedFrame;
		if (UI.nMouseRight)
		{
			int64_t nRangeBegin = S.Frames[nSelectedFrame].nFrameStartCpu;
			int64_t nRangeEnd = S.Frames[(nSelectedFrame + 1) % MICROPROFILE_MAX_FRAME_HISTORY].nFrameStartCpu;
			ProfileZoomTo(nRangeBegin, nRangeEnd, MicroProfileTokenTypeCpu);
		}
		if (UI.nMouseDownLeft)
		{
			uint64_t nFrac = (1024 * (MICROPROFILE_NUM_FRAMES) * (UI.nMouseX) / UI.nWidth) % 1024;
			int64_t nRangeBegin = S.Frames[nSelectedFrame].nFrameStartCpu;
			int64_t nRangeEnd = S.Frames[(nSelectedFrame + 1) % MICROPROFILE_MAX_FRAME_HISTORY].nFrameStartCpu;
			ProfileCenter(nRangeBegin + (nRangeEnd - nRangeBegin) * nFrac / 1024);
		}
	}
	else
	{
		UI.nHoverFrame = -1;
	}

	if (bDrawBars)
	{
		ProfileDrawDetailedBars(pCmd, nWidth, nHeight, nBaseY + MICROPROFILE_FRAME_HISTORY_HEIGHT, nSelectedFrame);
	}

	ProfileDrawDetailedFrameHistory(pCmd, nWidth, nHeight, nBaseY, nSelectedFrame);
}

void UIApp::ProfileDrawHeader(Cmd* pCmd, int32_t nX, uint32_t nWidth, const char* pName)
{
	if (pName)
	{
		ProfileDrawBox(pCmd, nX - 8, MICROPROFILE_TEXT_HEIGHT + 2, nX + nWidth + 5, MICROPROFILE_TEXT_HEIGHT + 2 + (MICROPROFILE_TEXT_HEIGHT + 1), 0xff000000 | g_nMicroProfileBackColors[1], MicroProfileBoxTypeFlat);
		ProfileDrawText(pCmd, nX, MICROPROFILE_TEXT_HEIGHT + 2, (uint32_t)-1, pName, (uint32_t)strlen(pName));
	}
}

void UIApp::ProfileLoopActiveGroupsDraw(Cmd* pCmd, int32_t nX, int32_t nY, MicroProfileLoopGroupCallback CB, void* pData)
{
	MicroProfile& S = *MicroProfileGet();
	nY += MICROPROFILE_TEXT_HEIGHT + 2;
	uint64_t nGroup = S.nAllGroupsWanted ? S.nGroupMask : S.nActiveGroupWanted;
	uint32_t nCount = 0;
	for (uint32_t j = 0; j < MICROPROFILE_MAX_GROUPS; ++j)
	{
		uint64_t nMask = 1ll << j;
		if (nMask & nGroup)
		{
			nY += MICROPROFILE_TEXT_HEIGHT + 1;
			for (uint32_t i = 0; i < S.nTotalTimers; ++i)
			{
				uint64_t nTokenMask = MicroProfileGetGroupMask(S.TimerInfo[i].nToken);
				if (nTokenMask & nMask)
				{
					if (nY >= 0)
						CB(pCmd, i, nCount, nX, nY, pData);

					nCount += 2;
					nY += MICROPROFILE_TEXT_HEIGHT + 1;

					if (nY > (int)UI.nHeight)
						return;
				}
			}

		}
	}
}


void UIApp::ProfileCalcTimers(float* pTimers, float* pAverage, float* pMax, float* pMin, float* pCallAverage, float* pExclusive, float* pAverageExclusive, float* pMaxExclusive, uint64_t nGroup, uint32_t nSize)
{
	MicroProfile& S = *MicroProfileGet();

	uint32_t nCount = 0;
	uint64_t nMask = 1;

	for (uint32_t j = 0; j < MICROPROFILE_MAX_GROUPS; ++j)
	{
		if (nMask & nGroup)
		{
			const float fToMs = MicroProfileTickToMsMultiplier(S.GroupInfo[j].Type == MicroProfileTokenTypeGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu());
			for (uint32_t i = 0; i < S.nTotalTimers; ++i)
			{
				uint64_t nTokenMask = MicroProfileGetGroupMask(S.TimerInfo[i].nToken);
				if (nTokenMask & nMask)
				{
					ASSERT(nCount + 2 <= nSize);
					{
						uint32_t nTimer = i;
						uint32_t nIdx = nCount;
						uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
						uint32_t nAggregateCount = S.Aggregate[nTimer].nCount ? S.Aggregate[nTimer].nCount : 1;
						float fToPrc = S.fRcpReferenceTime;
						float fMs = fToMs * (S.Frame[nTimer].nTicks);
						float fPrc = MicroProfileMin(fMs * fToPrc, 1.f);
						float fAverageMs = fToMs * (S.Aggregate[nTimer].nTicks / nAggregateFrames);
						float fAveragePrc = MicroProfileMin(fAverageMs * fToPrc, 1.f);
						float fMaxMs = fToMs * (S.AggregateMax[nTimer]);
						float fMaxPrc = MicroProfileMin(fMaxMs * fToPrc, 1.f);
						float fMinMs = fToMs * (S.AggregateMin[nTimer] != uint64_t(-1) ? S.AggregateMin[nTimer] : 0);
						float fMinPrc = MicroProfileMin(fMinMs * fToPrc, 1.f);
						float fCallAverageMs = fToMs * (S.Aggregate[nTimer].nTicks / nAggregateCount);
						float fCallAveragePrc = MicroProfileMin(fCallAverageMs * fToPrc, 1.f);
						float fMsExclusive = fToMs * (S.FrameExclusive[nTimer]);
						float fPrcExclusive = MicroProfileMin(fMsExclusive * fToPrc, 1.f);
						float fAverageMsExclusive = fToMs * (S.AggregateExclusive[nTimer] / nAggregateFrames);
						float fAveragePrcExclusive = MicroProfileMin(fAverageMsExclusive * fToPrc, 1.f);
						float fMaxMsExclusive = fToMs * (S.AggregateMaxExclusive[nTimer]);
						float fMaxPrcExclusive = MicroProfileMin(fMaxMsExclusive * fToPrc, 1.f);
						pTimers[nIdx] = fMs;
						pTimers[nIdx + 1] = fPrc;
						pAverage[nIdx] = fAverageMs;
						pAverage[nIdx + 1] = fAveragePrc;
						pMax[nIdx] = fMaxMs;
						pMax[nIdx + 1] = fMaxPrc;
						pMin[nIdx] = fMinMs;
						pMin[nIdx + 1] = fMinPrc;
						pCallAverage[nIdx] = fCallAverageMs;
						pCallAverage[nIdx + 1] = fCallAveragePrc;
						pExclusive[nIdx] = fMsExclusive;
						pExclusive[nIdx + 1] = fPrcExclusive;
						pAverageExclusive[nIdx] = fAverageMsExclusive;
						pAverageExclusive[nIdx + 1] = fAveragePrcExclusive;
						pMaxExclusive[nIdx] = fMaxMsExclusive;
						pMaxExclusive[nIdx + 1] = fMaxPrcExclusive;
					}
					nCount += 2;
				}
			}
		}
		nMask <<= 1ll;
	}
}

uint32_t UIApp::ProfileDrawBarArray(Cmd* pCmd, int32_t nX, int32_t nY, float* pTimers, const char* pName, uint32_t nTotalHeight, float* pTimers2)
{
	const uint32_t nTextWidth = 6 * (1 + MICROPROFILE_TEXT_WIDTH);
	const uint32_t nWidth = MICROPROFILE_BAR_WIDTH;

	ProfileDrawLineVertical(pCmd, nX - 5, 0, nTotalHeight + nY, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
	float* pTimersArray[2] = { pTimers, pTimers2 };
	ProfileLoopActiveGroupsDraw(pCmd, nX, nY, &this->ProfileDrawBarArrayCallback, pTimersArray);
	ProfileDrawHeader(pCmd, nX, nTextWidth + nWidth, pName);
	return nWidth + 5 + nTextWidth;

}

uint32_t UIApp::ProfileDrawBarCallCount(Cmd* pCmd, int32_t nX, int32_t nY, const char* pName)
{
	ProfileLoopActiveGroupsDraw(pCmd, nX, nY, &this->ProfileDrawBarCallCountCallback, 0);
	const uint32_t nTextWidth = 6 * MICROPROFILE_TEXT_WIDTH;
	ProfileDrawHeader(pCmd, nX, 5 + nTextWidth, pName);
	return 5 + nTextWidth;
}



uint32_t UIApp::ProfileDrawBarMetaAverage(Cmd* pCmd, int32_t nX, int32_t nY, uint64_t* pCounters, const char* pName, uint32_t nTotalHeight)
{
	if (!pName)
		return 0;
	ProfileDrawLineVertical(pCmd, nX - 5, 0, nTotalHeight + nY, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
	uint32_t nTextWidth = (1 + MICROPROFILE_TEXT_WIDTH) * MicroProfileMax<uint32_t>(6, (uint32_t)strlen(pName));
	float fRcpFrames = 1.f / (MicroProfileGet()->nAggregateFrames ? MicroProfileGet()->nAggregateFrames : 1);
	MicroProfileMetaAverageArgs Args = { pCounters, fRcpFrames };
	ProfileLoopActiveGroupsDraw(pCmd, nX + nTextWidth, nY, &this->ProfileDrawBarMetaAverageCallback, &Args);
	ProfileDrawHeader(pCmd, nX, 5 + nTextWidth, pName);
	return 5 + nTextWidth;
}


uint32_t UIApp::ProfileDrawBarMetaCount(Cmd* pCmd, int32_t nX, int32_t nY, uint64_t* pCounters, const char* pName, uint32_t nTotalHeight)
{
	if (!pName)
		return 0;

	ProfileDrawLineVertical(pCmd, nX - 5, 0, nTotalHeight + nY, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
	uint32_t nTextWidth = (1 + MICROPROFILE_TEXT_WIDTH) * MicroProfileMax<uint32_t>(6, (uint32_t)strlen(pName));
	ProfileLoopActiveGroupsDraw(pCmd, nX + nTextWidth, nY, &this->ProfileDrawBarMetaCountCallback, pCounters);
	ProfileDrawHeader(pCmd, nX, 5 + nTextWidth, pName);
	return 5 + nTextWidth;
}

uint32_t UIApp::ProfileDrawBarLegend(Cmd* pCmd, int32_t nX, int32_t nY, uint32_t nTotalHeight, uint32_t nMaxWidth)
{
	ProfileDrawLineVertical(pCmd, nX - 5, nY, nTotalHeight, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
	ProfileLoopActiveGroupsDraw(pCmd, nMaxWidth, nY, &this->ProfileDrawBarLegendCallback, 0);
	return nX;
}

bool UIApp::ProfileDrawGraph(Cmd* pCmd, uint32_t nScreenWidth, uint32_t nScreenHeight)
{
	MicroProfile& S = *MicroProfileGet();

	MICROPROFILE_SCOPE(g_MicroProfileDrawGraph);
	bool bEnabled = false;
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
		if (S.Graph[i].nToken != MICROPROFILE_INVALID_TOKEN)
			bEnabled = true;
	if (!bEnabled)
		return false;

	uint32_t nX = nScreenWidth - MICROPROFILE_GRAPH_WIDTH;
	uint32_t nY = nScreenHeight - MICROPROFILE_GRAPH_HEIGHT;
	ProfileDrawBox(pCmd, nX, nY, nX + MICROPROFILE_GRAPH_WIDTH, nY + MICROPROFILE_GRAPH_HEIGHT, 0x88000000 | g_nMicroProfileBackColors[0], MicroProfileBoxTypeFlat);
	bool bMouseOver = UI.nMouseX >= nX && UI.nMouseY >= nY;
	float fMouseXPrc = (float(UI.nMouseX - nX)) / MICROPROFILE_GRAPH_WIDTH;
	if (bMouseOver)
	{
		float fXAvg = fMouseXPrc * MICROPROFILE_GRAPH_WIDTH + nX;
		ProfileDrawLineVertical(pCmd, (int)fXAvg, nY, nY + MICROPROFILE_GRAPH_HEIGHT, (uint32_t)-1);
	}


	float fY = (float)nScreenHeight;
	float fDX = MICROPROFILE_GRAPH_WIDTH * 1.f / MICROPROFILE_GRAPH_HISTORY;
	float fDY = MICROPROFILE_GRAPH_HEIGHT;
	uint32_t nPut = S.nGraphPut;
	float* pGraphData = (float*)alloca(sizeof(float)* MICROPROFILE_GRAPH_HISTORY * 2);
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
	{
		if (S.Graph[i].nToken != MICROPROFILE_INVALID_TOKEN)
		{
			uint32_t nGroupId = MicroProfileGetGroupIndex(S.Graph[i].nToken);
			bool bGpu = S.GroupInfo[nGroupId].Type == MicroProfileTokenTypeGpu;
			float fToMs = MicroProfileTickToMsMultiplier(bGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu());
			float fToPrc = fToMs * S.fRcpReferenceTime * 3 / 4;

			float fX = (float)nX;
			for (uint32_t j = 0; j < MICROPROFILE_GRAPH_HISTORY; ++j)
			{
				float fWeigth = MicroProfileMin(fToPrc * (S.Graph[i].nHistory[(j + nPut) % MICROPROFILE_GRAPH_HISTORY]), 1.f);
				pGraphData[(j * 2)] = fX;
				pGraphData[(j * 2) + 1] = fY - fDY * fWeigth;
				fX += fDX;
			}
			ProfileDrawLine2D(pCmd, MICROPROFILE_GRAPH_HISTORY, pGraphData, UI.nOpacityForeground | S.TimerInfo[MicroProfileGetTimerIndex(S.Graph[i].nToken)].nColor);
		}
	}
	{
		float fY1 = 0.25f * MICROPROFILE_GRAPH_HEIGHT + nY;
		float fY2 = 0.50f * MICROPROFILE_GRAPH_HEIGHT + nY;
		float fY3 = 0.75f * MICROPROFILE_GRAPH_HEIGHT + nY;
		ProfileDrawLineHorizontal(pCmd, nX, nX + MICROPROFILE_GRAPH_WIDTH, (int)fY1, 0xffdd4444);
		ProfileDrawLineHorizontal(pCmd, nX, nX + MICROPROFILE_GRAPH_WIDTH, (int)fY2, 0xff000000 | g_nMicroProfileBackColors[0]);
		ProfileDrawLineHorizontal(pCmd, nX, nX + MICROPROFILE_GRAPH_WIDTH, (int)fY3, 0xff000000 | g_nMicroProfileBackColors[0]);

		char buf[32];
		snprintf(buf, sizeof(buf) - 1, "%5.2fms", S.fReferenceTime);
		uint32_t nLen = (uint32_t)strlen(buf);
		ProfileDrawText(pCmd, nX + 1, (int)(fY1 - (2 + MICROPROFILE_TEXT_HEIGHT)), (uint32_t)-1, buf, nLen);
	}



	if (bMouseOver)
	{
		uint32_t pColors[MICROPROFILE_MAX_GRAPHS];
		MicroProfileStringArray Strings;
		ProfileStringArrayClear(&Strings);
		uint32_t nTextCount = 0;
		uint32_t nGraphIndex = (S.nGraphPut + MICROPROFILE_GRAPH_HISTORY - int(MICROPROFILE_GRAPH_HISTORY*(1.f - fMouseXPrc))) % MICROPROFILE_GRAPH_HISTORY;

		uint32_t nX = UI.nMouseX;
		uint32_t nY = UI.nMouseY + 20;

		for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
		{
			if (S.Graph[i].nToken != MICROPROFILE_INVALID_TOKEN)
			{
				uint32_t nGroupId = MicroProfileGetGroupIndex(S.Graph[i].nToken);
				bool bGpu = S.GroupInfo[nGroupId].Type == MicroProfileTokenTypeGpu;
				float fToMs = MicroProfileTickToMsMultiplier(bGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu());
				uint32_t nIndex = MicroProfileGetTimerIndex(S.Graph[i].nToken);
				uint32_t nColor = S.TimerInfo[nIndex].nColor;
				const char* pName = S.TimerInfo[nIndex].pName;
				pColors[nTextCount++] = nColor;
				ProfileStringArrayAddLiteral(&Strings, pName);
				ProfileStringArrayFormat(&Strings, "%5.2fms", fToMs * (S.Graph[i].nHistory[nGraphIndex]));
			}
		}
		if (nTextCount)
		{
			ProfileDrawFloatWindow(pCmd, nX, nY, Strings.ppStrings, Strings.nNumStrings, 0, pColors);
		}

		if (UI.nMouseRight)
		{
			for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
			{
				S.Graph[i].nToken = MICROPROFILE_INVALID_TOKEN;
			}
		}
	}

	return bMouseOver;
}

void UIApp::ProfileDumpTimers()
{
	MicroProfile& S = *MicroProfileGet();

	uint64_t nActiveGroup = S.nGroupMask;

	uint32_t nNumTimers = S.nTotalTimers;
	uint32_t nBlockSize = 2 * nNumTimers;
	float* pTimers = (float*)alloca(nBlockSize * 8 * sizeof(float));
	float* pAverage = pTimers + nBlockSize;
	float* pMax = pTimers + 2 * nBlockSize;
	float* pMin = pTimers + 3 * nBlockSize;
	float* pCallAverage = pTimers + 4 * nBlockSize;
	float* pTimersExclusive = pTimers + 5 * nBlockSize;
	float* pAverageExclusive = pTimers + 6 * nBlockSize;
	float* pMaxExclusive = pTimers + 7 * nBlockSize;
	ProfileCalcTimers(pTimers, pAverage, pMax, pMin, pCallAverage, pTimersExclusive, pAverageExclusive, pMaxExclusive, nActiveGroup, nBlockSize);

	MICROPROFILE_PRINTF("%11s, ", "Time");
	MICROPROFILE_PRINTF("%11s, ", "Average");
	MICROPROFILE_PRINTF("%11s, ", "Max");
	MICROPROFILE_PRINTF("%11s, ", "Min");
	MICROPROFILE_PRINTF("%11s, ", "Call Avg");
	MICROPROFILE_PRINTF("%9s, ", "Count");
	MICROPROFILE_PRINTF("%11s, ", "Excl");
	MICROPROFILE_PRINTF("%11s, ", "Avg Excl");
	MICROPROFILE_PRINTF("%11s, \n", "Max Excl");

	for (uint32_t j = 0; j < MICROPROFILE_MAX_GROUPS; ++j)
	{
		uint64_t nMask = 1ll << j;
		if (nMask & nActiveGroup)
		{
			MICROPROFILE_PRINTF("%s\n", S.GroupInfo[j].pName);
			for (uint32_t i = 0; i < S.nTotalTimers; ++i)
			{
				uint64_t nTokenMask = MicroProfileGetGroupMask(S.TimerInfo[i].nToken);
				if (nTokenMask & nMask)
				{
					uint32_t nIdx = i * 2;
					MICROPROFILE_PRINTF("%9.2fms, ", pTimers[nIdx]);
					MICROPROFILE_PRINTF("%9.2fms, ", pAverage[nIdx]);
					MICROPROFILE_PRINTF("%9.2fms, ", pMax[nIdx]);
					MICROPROFILE_PRINTF("%9.2fms, ", pMin[nIdx]);
					MICROPROFILE_PRINTF("%9.2fms, ", pCallAverage[nIdx]);
					MICROPROFILE_PRINTF("%9d, ", S.Frame[i].nCount);
					MICROPROFILE_PRINTF("%9.2fms, ", pTimersExclusive[nIdx]);
					MICROPROFILE_PRINTF("%9.2fms, ", pAverageExclusive[nIdx]);
					MICROPROFILE_PRINTF("%9.2fms, ", pMaxExclusive[nIdx]);
					MICROPROFILE_PRINTF("%s\n", S.TimerInfo[i].pName);
				}
			}
		}
	}
}



uint32_t UIApp::ProfileDrawCounterRecursive(Cmd* pCmd, uint32_t nIndex, uint32_t nY, uint32_t nOffset, uint32_t nTimerWidth)
{
	MicroProfile& S = *MicroProfileGet();
	uint8_t bGraphDetailed = 0 != (S.CounterInfo[nIndex].nFlags & MICROPROFILE_COUNTER_FLAG_DETAILED_GRAPH);
	uint32_t nRows = bGraphDetailed ? 5 : 1;
	const uint32_t nHeight = MICROPROFILE_TEXT_HEIGHT;
	const uint32_t nCounterWidth = UI.nCounterWidth;
	const uint32_t nLimitWidth = UI.nLimitWidth;

	uint32_t nY0 = nY + nOffset * (nHeight + 1);
	uint32_t nBackHeight = (nHeight + 1) * nRows;

	MicroProfileCounterInfo& CI = S.CounterInfo[nIndex];
	bool bInside = (UI.nActiveMenu == (uint32_t)-1) && ((UI.nMouseY >= nY0) && (UI.nMouseY < (nY0 + nBackHeight)));
	uint32_t nTotalWidth = nTimerWidth + nCounterWidth * 3 + MICROPROFILE_COUNTER_WIDTH + nLimitWidth + 4 * (MICROPROFILE_TEXT_WIDTH + 1)
		+ 4 + MICROPROFILE_GRAPH_HISTORY;
	uint32_t nBackColor = 0xff000000 | (g_nMicroProfileBackColors[nOffset & 1] + ((bInside) ? 0x002c2c2c : 0));
	ProfileDrawBox(pCmd, 0, nY0, nTotalWidth, nY0 + nBackHeight + 1, nBackColor, MicroProfileBoxTypeFlat);
	uint32_t nIndent = MICROPROFILE_COUNTER_INDENT * CI.nLevel * (MICROPROFILE_TEXT_WIDTH + 1);
	if (CI.nFirstChild != -1 && 0 != (CI.nFlags & MICROPROFILE_COUNTER_FLAG_CLOSED))
	{
		ProfileDrawText(pCmd, nIndent, nY0, 0xffffffff, "*", 1);
	}

	ProfileDrawText(pCmd, nIndent + MICROPROFILE_TEXT_WIDTH + 1, nY0, 0xffffffff, CI.pName, CI.nNameLen);
	char buffer[64];
	int64_t nCounterValue = S.Counters[nIndex].load();
	uint32_t nX = nTimerWidth + nCounterWidth;
	int nLen = MicroProfileFormatCounter(S.CounterInfo[nIndex].eFormat, nCounterValue, buffer, sizeof(buffer));
	UI.nCounterWidthTemp = MicroProfileMax((uint32_t)nLen, UI.nCounterWidthTemp);
	if (0 != nCounterValue || 0 != (CI.nFlags & MICROPROFILE_COUNTER_FLAG_LEAF))
	{
		ProfileDrawTextRight(pCmd, nX, nY0, 0xffffffff, buffer, nLen);
	}
	int64_t nLimit = S.CounterInfo[nIndex].nLimit;
	if (nLimit)
	{
		nX += MICROPROFILE_TEXT_WIDTH + 1;
		ProfileDrawText(pCmd, nX, nY0, 0xffffffff, "/", 1);
		nX += 2 * (MICROPROFILE_TEXT_WIDTH + 1);
		int nLen = MicroProfileFormatCounter(S.CounterInfo[nIndex].eFormat, nLimit, buffer, sizeof(buffer));
		UI.nLimitWidthTemp = MicroProfileMax(UI.nLimitWidthTemp, (uint32_t)nLen);
		ProfileDrawText(pCmd, nX, nY0, 0xffffffff, buffer, nLen);
		nX += nLimitWidth;
		nY0 += 1;

		float fCounterPrc = (float)nCounterValue / nLimit;
		fCounterPrc = MicroProfileMax(fCounterPrc, 0.f);
		float fBoxPrc = 1.f;
		if (fCounterPrc > 1.f)
		{
			fBoxPrc = 1.f / fCounterPrc;
			fCounterPrc = 1.f;
		}

		ProfileDrawBox(pCmd, nX, nY0, nX + (int)(fBoxPrc * MICROPROFILE_COUNTER_WIDTH), nY0 + nHeight, 0xffffffff, MicroProfileBoxTypeFlat);
		ProfileDrawBox(pCmd, nX + 1, nY0 + 1, nX + MICROPROFILE_COUNTER_WIDTH - 1, nY0 + nHeight - 1, nBackColor, MicroProfileBoxTypeFlat);
		ProfileDrawBox(pCmd, nX + 1, nY0 + 1, nX + (int)(fCounterPrc * (MICROPROFILE_COUNTER_WIDTH - 1)), nY0 + nHeight - 1, 0xff0088ff, MicroProfileBoxTypeFlat);
		nX += MICROPROFILE_COUNTER_WIDTH + 5;
	}
	else
	{
		nX += MICROPROFILE_TEXT_WIDTH + 1;
		nX += 2 * (MICROPROFILE_TEXT_WIDTH + 1);
		nX += nLimitWidth;
		nX += MICROPROFILE_COUNTER_WIDTH + 5;
	}

	if (bInside && (UI.nMouseLeft || UI.nMouseRight))
	{
		if (UI.nMouseX > nX)
		{
			if (UI.nMouseRight)
			{
				CI.nFlags &= ~MICROPROFILE_COUNTER_FLAG_DETAILED;
			}
			else
			{
				// toggle through detailed & detailed graph
				if (CI.nFlags & MICROPROFILE_COUNTER_FLAG_DETAILED)
				{
					CI.nFlags ^= MICROPROFILE_COUNTER_FLAG_DETAILED_GRAPH;
				}
				else
				{
					CI.nFlags |= MICROPROFILE_COUNTER_FLAG_DETAILED;
				}
			}
			if (0 == (CI.nFlags & MICROPROFILE_COUNTER_FLAG_DETAILED))
			{
				CI.nFlags &= ~MICROPROFILE_COUNTER_FLAG_DETAILED_GRAPH;
			}
		}
		else if (UI.nMouseLeft)
		{
			CI.nFlags ^= MICROPROFILE_COUNTER_FLAG_CLOSED;
		}
	}

#if MICROPROFILE_COUNTER_HISTORY
	if (0 != (CI.nFlags & MICROPROFILE_COUNTER_FLAG_DETAILED))
	{
		static float pGraphData[MICROPROFILE_GRAPH_HISTORY * 2];
		static float pGraphFillData[MICROPROFILE_GRAPH_HISTORY * 4];

		int32_t nMouseGraph = UI.nMouseX - nX;


		int64_t nCounterMax = S.nCounterMax[nIndex];
		int64_t nCounterMin = S.nCounterMin[nIndex];
		uint32_t nBaseIndex = S.nCounterHistoryPut;
		float fX = (float)nX;

		int64_t nCounterHeightBase = nCounterMax;
		int64_t nCounterOffset = 0;
		if (nCounterMin < 0)
		{
			nCounterHeightBase = nCounterMax - nCounterMin;
			nCounterOffset = -nCounterMin;
		}
		const int32_t nGraphHeight = nRows * nHeight;
		double fRcpMax = nGraphHeight * 1.0 / nCounterHeightBase;
		const int32_t nYOffset = nY0 + (bGraphDetailed ? 3 : 1);
		const int32_t nYBottom = nGraphHeight + nYOffset;
		for (uint32_t i = 0; i < MICROPROFILE_GRAPH_HISTORY; ++i)
		{
			uint32_t nHistoryIndex = (nBaseIndex + i) % MICROPROFILE_GRAPH_HISTORY;
			int64_t nValue = MicroProfileClamp(S.nCounterHistory[nHistoryIndex][nIndex], nCounterMin, nCounterMax);
			float fPrc = nGraphHeight - (float)((double)(nValue + nCounterOffset) * fRcpMax);
			pGraphData[(i * 2)] = fX;
			pGraphData[(i * 2) + 1] = nYOffset + fPrc;

			pGraphFillData[(i * 4) + 0] = fX;
			pGraphFillData[(i * 4) + 1] = nYOffset + fPrc;
			pGraphFillData[(i * 4) + 2] = fX;
			pGraphFillData[(i * 4) + 3] = (float)nYBottom;

			fX += 1;
		}
		ProfileDrawLine2D(pCmd, MICROPROFILE_GRAPH_HISTORY * 2, pGraphFillData, 0x330088ff);
		ProfileDrawLine2D(pCmd, MICROPROFILE_GRAPH_HISTORY, pGraphData, 0xff0088ff);

		if (nMouseGraph < MICROPROFILE_GRAPH_HISTORY && bInside && nCounterMin <= nCounterMax)
		{
			uint32_t nMouseX = nX + nMouseGraph;
			float fMouseX = (float)nMouseX;
			uint32_t nHistoryIndex = (nBaseIndex + nMouseGraph) % MICROPROFILE_GRAPH_HISTORY;
			int64_t nValue = MicroProfileClamp(S.nCounterHistory[nHistoryIndex][nIndex], nCounterMin, nCounterMax);
			float fPrc = nGraphHeight - (float)((double)(nValue + nCounterOffset) * fRcpMax);
			float fCursor[4];
			fCursor[0] = fMouseX - 2.f;
			fCursor[1] = nYOffset + fPrc + 2.f;
			fCursor[2] = fMouseX + 2.f;
			fCursor[3] = nYOffset + fPrc - 2.f;
			ProfileDrawLine2D(pCmd, 2, fCursor, 0xddff8800);
			fCursor[0] = fMouseX + 2.f;
			fCursor[1] = nYOffset + fPrc + 2.f;
			fCursor[2] = fMouseX - 2.f;
			fCursor[3] = nYOffset + fPrc - 2.f;
			ProfileDrawLine2D(pCmd, 2, fCursor, 0xddff8800);
			int nLen = MicroProfileFormatCounter(S.CounterInfo[nIndex].eFormat, nValue, buffer, sizeof(buffer));
			ProfileDrawText(pCmd, nX, nY0, 0xffffffff, buffer, nLen);
		}


		nX += MICROPROFILE_GRAPH_HISTORY + 5;
		if (nCounterMin <= nCounterMax)
		{
			int nLen = MicroProfileFormatCounter(S.CounterInfo[nIndex].eFormat, nCounterMin, buffer, sizeof(buffer));
			ProfileDrawText(pCmd, nX, nY0, 0xffffffff, buffer, nLen);
			nX += nCounterWidth;
			nLen = MicroProfileFormatCounter(S.CounterInfo[nIndex].eFormat, nCounterMax, buffer, sizeof(buffer));
			ProfileDrawText(pCmd, nX, nY0, 0xffffffff, buffer, nLen);
		}

	}
#endif

	nOffset += nRows;
	if (0 == (CI.nFlags & MICROPROFILE_COUNTER_FLAG_CLOSED))
	{
		int nChild = CI.nFirstChild;
		while (nChild != -1)
		{
			nOffset = ProfileDrawCounterRecursive(pCmd, nChild, nY, nOffset, nTimerWidth);
			nChild = S.CounterInfo[nChild].nSibling;
		}
	}


	return nOffset;
}

void UIApp::ProfileDrawCounterView(Cmd* pCmd, uint32_t nScreenWidth, uint32_t nScreenHeight)
{
	(void)nScreenWidth;
	(void)nScreenHeight;

	MicroProfile& S = *MicroProfileGet();
	MICROPROFILE_SCOPE(g_MicroProfileDrawBarView);

	UI.nCounterWidthTemp = 7;
	UI.nLimitWidthTemp = 7;
	const uint32_t nHeight = MICROPROFILE_TEXT_HEIGHT;
	uint32_t nTimerWidth = 7 * (MICROPROFILE_TEXT_WIDTH + 1);
	for (uint32_t i = 0; i < S.nNumCounters; ++i)
	{
		uint32_t nWidth = (2 + S.CounterInfo[i].nNameLen + MICROPROFILE_COUNTER_INDENT * S.CounterInfo[i].nLevel) * (MICROPROFILE_TEXT_WIDTH + 1);
		nTimerWidth = MicroProfileMax(nTimerWidth, nWidth);
	}
	uint32_t nX = nTimerWidth + UI.nOffsetX[MP_DRAW_COUNTERS];
	uint32_t nY = nHeight + 3 - UI.nOffsetY[MP_DRAW_COUNTERS];
	uint32_t nNumCounters = S.nNumCounters;
	nX = 0;
	nY = (2 * nHeight) + 3 - UI.nOffsetY[MP_DRAW_COUNTERS];
	uint32_t nOffset = 0;
	for (uint32_t i = 0; i < nNumCounters; ++i)
	{
		if (S.CounterInfo[i].nParent == -1)
		{
			nOffset = ProfileDrawCounterRecursive(pCmd, i, nY, nOffset, nTimerWidth);
		}
	}
	nX = 0;
	ProfileDrawHeader(pCmd, nX, nTimerWidth, "Name");
	nX += nTimerWidth;
	ProfileDrawHeader(pCmd, nX, UI.nCounterWidth + 1 * (MICROPROFILE_TEXT_WIDTH * 3), "Value");
	nX += UI.nCounterWidth;
	nX += 1 * (MICROPROFILE_TEXT_WIDTH * 3);
	ProfileDrawHeader(pCmd, nX, UI.nLimitWidth + MICROPROFILE_COUNTER_WIDTH, "Limit");
	nX += UI.nLimitWidth + MICROPROFILE_COUNTER_WIDTH + 4;
	ProfileDrawHeader(pCmd, nX, MICROPROFILE_GRAPH_HISTORY, "Graph");
	nX += MICROPROFILE_GRAPH_HISTORY + 4;
	ProfileDrawHeader(pCmd, nX, UI.nCounterWidth, "Min");
	nX += UI.nCounterWidth;
	ProfileDrawHeader(pCmd, nX, UI.nCounterWidth, "Max");
	nX += UI.nCounterWidth;
	uint32_t nTotalWidth = nX;//nTimerWidth + UI.nCounterWidth + MICROPROFILE_COUNTER_WIDTH + UI.nLimitWidth + 3 * (MICROPROFILE_TEXT_WIDTH+1);



	ProfileDrawLineVertical(pCmd, nTimerWidth - 2, 0, nOffset*(nHeight + 1) + nY, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
	ProfileDrawLineHorizontal(pCmd, 0, nTotalWidth, 2 * MICROPROFILE_TEXT_HEIGHT + 3, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);

	UI.nCounterWidth = (1 + UI.nCounterWidthTemp) * (MICROPROFILE_TEXT_WIDTH + 1);
	UI.nLimitWidth = (1 + UI.nLimitWidthTemp) * (MICROPROFILE_TEXT_WIDTH + 1);

}



void UIApp::ProfileDrawBarView(Cmd* pCmd, uint32_t nScreenWidth, uint32_t nScreenHeight)
{
	(void)nScreenWidth;
	(void)nScreenHeight;

	MicroProfile& S = *MicroProfileGet();

	uint64_t nActiveGroup = S.nAllGroupsWanted ? S.nGroupMask : S.nActiveGroupWanted;
	if (!nActiveGroup)
		return;
	MICROPROFILE_SCOPE(g_MicroProfileDrawBarView);

	const uint32_t nHeight = MICROPROFILE_TEXT_HEIGHT;
	int nColorIndex = 0;
	uint32_t nMaxTimerNameLen = 1;
	uint32_t nNumTimers = 0;
	uint32_t nNumGroups = 0;
	for (uint32_t j = 0; j < MICROPROFILE_MAX_GROUPS; ++j)
	{
		if (nActiveGroup & (1ll << j))
		{
			nNumTimers += S.GroupInfo[j].nNumTimers;
			nNumGroups += 1;
			nMaxTimerNameLen = MicroProfileMax(nMaxTimerNameLen, S.GroupInfo[j].nMaxTimerNameLen);
		}
	}
	uint32_t nTimerWidth = 2 + (4 + nMaxTimerNameLen) * (MICROPROFILE_TEXT_WIDTH + 1);
	uint32_t nX = nTimerWidth + UI.nOffsetX[MP_DRAW_BARS];
	uint32_t nY = nHeight + 3 - UI.nOffsetY[MP_DRAW_BARS];
	uint32_t nBlockSize = 2 * nNumTimers;
	float* pTimers = (float*)alloca(nBlockSize * 8 * sizeof(float));
	float* pAverage = pTimers + nBlockSize;
	float* pMax = pTimers + 2 * nBlockSize;
	float* pMin = pTimers + 3 * nBlockSize;
	float* pCallAverage = pTimers + 4 * nBlockSize;
	float* pTimersExclusive = pTimers + 5 * nBlockSize;
	float* pAverageExclusive = pTimers + 6 * nBlockSize;
	float* pMaxExclusive = pTimers + 7 * nBlockSize;
	ProfileCalcTimers(pTimers, pAverage, pMax, pMin, pCallAverage, pTimersExclusive, pAverageExclusive, pMaxExclusive, nActiveGroup, nBlockSize);
	uint32_t nWidth = 0;
	{
		uint32_t nMetaIndex = 0;
		for (uint32_t i = 1; i; i <<= 1)
		{
			if (S.nBars & i)
			{
				if (i >= MP_DRAW_META_FIRST)
				{
					if (nMetaIndex < MICROPROFILE_META_MAX && S.MetaCounters[nMetaIndex].pName)
					{
						uint32_t nStrWidth = (uint32_t)strlen(S.MetaCounters[nMetaIndex].pName);
						if (S.nBars & MP_DRAW_TIMERS)
							nWidth += 6 + (1 + MICROPROFILE_TEXT_WIDTH) * (nStrWidth);
						if (S.nBars & MP_DRAW_AVERAGE)
							nWidth += 6 + (1 + MICROPROFILE_TEXT_WIDTH) * (nStrWidth + 4);
						if (S.nBars & MP_DRAW_MAX)
							nWidth += 6 + (1 + MICROPROFILE_TEXT_WIDTH) * (nStrWidth + 4);
						if (S.nBars & MP_DRAW_MIN)
							nWidth += 6 + (1 + MICROPROFILE_TEXT_WIDTH) * (nStrWidth + 4);
					}
				}
				else
				{
					nWidth += MICROPROFILE_BAR_WIDTH + 6 + 6 * (1 + MICROPROFILE_TEXT_WIDTH);
					if (i & MP_DRAW_CALL_COUNT)
						nWidth += 6 + 6 * MICROPROFILE_TEXT_WIDTH;
				}
			}
			if (i >= MP_DRAW_META_FIRST)
			{
				++nMetaIndex;
			}
		}
		nWidth += (1 + nMaxTimerNameLen) * (MICROPROFILE_TEXT_WIDTH + 1);
		for (uint32_t i = 0; i < nNumTimers + nNumGroups + 1; ++i)
		{
			uint32_t nY0 = nY + i * (nHeight + 1);
			bool bInside = (UI.nActiveMenu == (uint32_t)-1) && ((UI.nMouseY >= nY0) && (UI.nMouseY < (nY0 + nHeight + 1)));
			ProfileDrawBox(pCmd, nX, nY0, nWidth + nX, nY0 + (nHeight + 1) + 1, UI.nOpacityBackground | (g_nMicroProfileBackColors[nColorIndex++ & 1] + ((bInside) ? 0x002c2c2c : 0)), MicroProfileBoxTypeFlat);
		}
		nX += 10;
	}
	int nTotalHeight = (nNumTimers + nNumGroups + 1) * (nHeight + 1);
	uint32_t nLegendOffset = 1;
	if (S.nBars & MP_DRAW_TIMERS)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pTimers, "Time", nTotalHeight) + 1;
	if (S.nBars & MP_DRAW_AVERAGE)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pAverage, "Average", nTotalHeight) + 1;
	if (S.nBars & MP_DRAW_MAX)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pMax, (!UI.bShowSpikes) ? "Max Time" : "Max Time, Spike", nTotalHeight, UI.bShowSpikes ? pAverage : NULL) + 1;
	if (S.nBars & MP_DRAW_MIN)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pMin, (!UI.bShowSpikes) ? "Min Time" : "Min Time, Spike", nTotalHeight, UI.bShowSpikes ? pAverage : NULL) + 1;
	if (S.nBars & MP_DRAW_CALL_COUNT)
	{
		nX += ProfileDrawBarArray(pCmd, nX, nY, pCallAverage, "Call Average", nTotalHeight) + 1;
		nX += ProfileDrawBarCallCount(pCmd, nX, nY, "Count") + 1;
	}
	if (S.nBars & MP_DRAW_TIMERS_EXCLUSIVE)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pTimersExclusive, "Exclusive Time", nTotalHeight) + 1;
	if (S.nBars & MP_DRAW_AVERAGE_EXCLUSIVE)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pAverageExclusive, "Exclusive Average", nTotalHeight) + 1;
	if (S.nBars & MP_DRAW_MAX_EXCLUSIVE)
		nX += ProfileDrawBarArray(pCmd, nX, nY, pMaxExclusive, (!UI.bShowSpikes) ? "Exclusive Max Time" : "Excl Max Time, Spike", nTotalHeight, UI.bShowSpikes ? pAverageExclusive : NULL) + 1;

	for (int i = 0; i < MICROPROFILE_META_MAX; ++i)
	{
		if (0 != (S.nBars & (MP_DRAW_META_FIRST << i)) && S.MetaCounters[i].pName)
		{
			uint32_t nBufferSize = (uint32_t)strlen(S.MetaCounters[i].pName) + 32;
			char* buffer = (char*)alloca(nBufferSize);
			if (S.nBars & MP_DRAW_TIMERS)
				nX += ProfileDrawBarMetaCount(pCmd, nX, nY, &S.MetaCounters[i].nCounters[0], S.MetaCounters[i].pName, nTotalHeight) + 1;
			if (S.nBars & MP_DRAW_AVERAGE)
			{
				snprintf(buffer, nBufferSize - 1, "%s Avg", S.MetaCounters[i].pName);
				nX += ProfileDrawBarMetaAverage(pCmd, nX, nY, &S.MetaCounters[i].nAggregate[0], buffer, nTotalHeight) + 1;
			}
			if (S.nBars & MP_DRAW_MAX)
			{
				snprintf(buffer, nBufferSize - 1, "%s Max", S.MetaCounters[i].pName);
				nX += ProfileDrawBarMetaCount(pCmd, nX, nY, &S.MetaCounters[i].nAggregateMax[0], buffer, nTotalHeight) + 1;
			}
		}
	}
	nX = 0;
	nY = nHeight + 3 - UI.nOffsetY[MP_DRAW_BARS];
	for (uint32_t i = 0; i < nNumTimers + nNumGroups + 1; ++i)
	{
		uint32_t nY0 = nY + i * (nHeight + 1);
		bool bInside = (UI.nActiveMenu == (uint32_t)-1) && ((UI.nMouseY >= nY0) && (UI.nMouseY < (nY0 + nHeight + 1)));
		ProfileDrawBox(pCmd, nX, nY0, nTimerWidth, nY0 + (nHeight + 1) + 1, 0xff000000 | (g_nMicroProfileBackColors[nColorIndex++ & 1] + ((bInside) ? 0x002c2c2c : 0)), MicroProfileBoxTypeFlat);
	}
	nX += ProfileDrawBarLegend(pCmd, nX, nY, nTotalHeight, nTimerWidth - 5) + 1;

	for (uint32_t j = 0; j < MICROPROFILE_MAX_GROUPS; ++j)
	{
		if (nActiveGroup & (1ll << j))
		{
			ProfileDrawText(pCmd, nX, nY + (1 + nHeight) * nLegendOffset, (uint32_t)-1, S.GroupInfo[j].pName, S.GroupInfo[j].nNameLen);
			nLegendOffset += S.GroupInfo[j].nNumTimers + 1;
		}
	}
	ProfileDrawHeader(pCmd, nX, nTimerWidth - 5, "Group");
	ProfileDrawTextRight(pCmd, nTimerWidth - 3, MICROPROFILE_TEXT_HEIGHT + 2, (uint32_t)-1, "Timer", 5);
	ProfileDrawLineVertical(pCmd, nTimerWidth, 0, nTotalHeight + nY, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
	ProfileDrawLineHorizontal(pCmd, 0, nWidth, 2 * MICROPROFILE_TEXT_HEIGHT + 3, UI.nOpacityBackground | g_nMicroProfileBackColors[0] | g_nMicroProfileBackColors[1]);
}

void UIApp::ProfileDrawMenu(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight)
{
	(void)nWidth;
	(void)nHeight;

	MicroProfile& S = *MicroProfileGet();

	uint32_t nX = 0;
	uint32_t nY = 0;
#define SBUF_SIZE 256
	char buffer[256];
	ProfileDrawBox(pCmd, nX, nY, nX + nWidth, nY + (MICROPROFILE_TEXT_HEIGHT + 1) + 1, 0xff000000 | g_nMicroProfileBackColors[1], MicroProfileBoxTypeFlat);

#define MICROPROFILE_MENU_MAX 16
	const char* pMenuText[MICROPROFILE_MENU_MAX] = { 0 };
	uint32_t 	nMenuX[MICROPROFILE_MENU_MAX] = { 0 };
	uint32_t nNumMenuItems = 0;

	snprintf(buffer, 127, "MicroProfile");
	uint32_t nLen = (uint32_t)strlen(buffer);
	ProfileDrawText(pCmd, nX, nY, (uint32_t)-1, buffer, nLen);
	nX += (sizeof("MicroProfile") + 2) * (MICROPROFILE_TEXT_WIDTH + 1);
	// Comment out menus
	/*pMenuText[nNumMenuItems++] = "Mode";
	pMenuText[nNumMenuItems++] = "Groups";
	char AggregateText[64];
	snprintf(AggregateText, sizeof(AggregateText)-1, "Aggregate[%d]", S.nAggregateFlip ? S.nAggregateFlip : S.nAggregateFlipCount);
	pMenuText[nNumMenuItems++] = &AggregateText[0];
	pMenuText[nNumMenuItems++] = "Timers";
	pMenuText[nNumMenuItems++] = "Options";
	pMenuText[nNumMenuItems++] = "Preset";
	pMenuText[nNumMenuItems++] = "Custom";
	pMenuText[nNumMenuItems++] = "Dump";*/
	const int nPauseIndex = nNumMenuItems;
	/*pMenuText[nNumMenuItems++] = S.nRunning ? "Pause" : "Unpause";
	pMenuText[nNumMenuItems++] = "Help";*/

	if (S.nOverflow)
	{
		pMenuText[nNumMenuItems++] = "!BUFFERSFULL!";
	}


	if (UI.GroupMenuCount != S.nGroupCount + S.nCategoryCount)
	{
		UI.GroupMenuCount = S.nGroupCount + S.nCategoryCount;
		for (uint32_t i = 0; i < S.nCategoryCount; ++i)
		{
			UI.GroupMenu[i].nIsCategory = 1;
			UI.GroupMenu[i].nCategoryIndex = i;
			UI.GroupMenu[i].nIndex = i;
			UI.GroupMenu[i].pName = S.CategoryInfo[i].pName;
		}
		for (uint32_t i = 0; i < S.nGroupCount; ++i)
		{
			uint32_t idx = i + S.nCategoryCount;
			UI.GroupMenu[idx].nIsCategory = 0;
			UI.GroupMenu[idx].nCategoryIndex = S.GroupInfo[i].nCategory;
			UI.GroupMenu[idx].nIndex = i;
			UI.GroupMenu[idx].pName = S.GroupInfo[i].pName;
		}
		/*std::sort(&UI.GroupMenu[0], &UI.GroupMenu[UI.GroupMenuCount],
			[] (const MicroProfileGroupMenuItem& l, const MicroProfileGroupMenuItem& r) -> bool
			{
				if(l.nCategoryIndex < r.nCategoryIndex)
				{
					return true;
				}
				else if(r.nCategoryIndex < l.nCategoryIndex)
				{
					return false;
				}
				if(r.nIsCategory || l.nIsCategory)
				{
					return l.nIsCategory > r.nIsCategory;
				}
				return MP_STRCASECMP(l.pName, r.pName)<0;
			}
		);*/
	}

	MicroProfileSubmenuCallback GroupCallback[MICROPROFILE_MENU_MAX] =
	{
		&this->ProfileUIMenuMode,
		&this->ProfileUIMenuGroups,
		&this->ProfileUIMenuAggregate,
		&this->ProfileUIMenuTimers,
		&this->ProfileUIMenuOptions,
		&this->ProfileUIMenuPreset,
		&this->ProfileUIMenuCustom,
		&this->ProfileUIMenuDump,
	};

	MicroProfileClickCallback CBClick[MICROPROFILE_MENU_MAX] =
	{
		&this->ProfileUIClickMode,
		&this->ProfileUIClickGroups,
		&this->ProfileUIClickAggregate,
		&this->ProfileUIClickTimers,
		&this->ProfileUIClickOptions,
		&this->ProfileUIClickPreset,
		&this->ProfileUIClickCustom,
		&this->ProfileUIClickDump,
	};


	uint32_t nSelectMenu = (uint32_t)-1;
	for (uint32_t i = 0; i < nNumMenuItems; ++i)
	{
		nMenuX[i] = nX;
		uint32_t nLen = (uint32_t)strlen(pMenuText[i]);
		uint32_t nEnd = nX + nLen * (MICROPROFILE_TEXT_WIDTH + 1);
		if (UI.nMouseY <= MICROPROFILE_TEXT_HEIGHT && UI.nMouseX <= nEnd && UI.nMouseX >= nX)
		{
			ProfileDrawBox(pCmd, nX - 1, nY, nX + nLen * (MICROPROFILE_TEXT_WIDTH + 1), nY + (MICROPROFILE_TEXT_HEIGHT + 1) + 1, 0xff888888, MicroProfileBoxTypeFlat);
			nSelectMenu = i;
			if ((UI.nMouseLeft || UI.nMouseRight) && (int)i == nPauseIndex)
			{
				S.nToggleRunning = 1;
			}
		}
		ProfileDrawText(pCmd, nX, nY, (uint32_t)-1, pMenuText[i], (uint32_t)strlen(pMenuText[i]));
		nX += (nLen + 1) * (MICROPROFILE_TEXT_WIDTH + 1);
	}
	uint32_t nMenu = nSelectMenu != (uint32_t)-1 ? nSelectMenu : UI.nActiveMenu;
	UI.nActiveMenu = nSelectMenu;
	if ((uint32_t)-1 != nMenu && GroupCallback[nMenu])
	{
		nX = nMenuX[nMenu];
		nY += MICROPROFILE_TEXT_HEIGHT + 1;
		MicroProfileSubmenuCallback CB = GroupCallback[nMenu];
		int nNumLines = 0;
		bool bSelected = false;
		const char* pString = CB(nNumLines, &bSelected);
		uint32_t nWidth = 0, nHeight = 0;
		while (pString)
		{
			nWidth = MicroProfileMax<int>(nWidth, (int)strlen(pString));
			nNumLines++;
			pString = CB(nNumLines, &bSelected);
		}
		nWidth = (2 + nWidth) * (MICROPROFILE_TEXT_WIDTH + 1);
		nHeight = nNumLines * (MICROPROFILE_TEXT_HEIGHT + 1);
		if (UI.nMouseY <= nY + nHeight + 0 && UI.nMouseY >= nY - 0 && UI.nMouseX <= nX + nWidth + 0 && UI.nMouseX >= nX - 0)
		{
			UI.nActiveMenu = nMenu;
		}
		ProfileDrawBox(pCmd, nX, nY, nX + nWidth, nY + nHeight, 0xff000000 | g_nMicroProfileBackColors[1], MicroProfileBoxTypeFlat);
		for (int i = 0; i < nNumLines; ++i)
		{
			bool bSelected = false;
			const char* pString = CB(i, &bSelected);
			if (UI.nMouseY >= nY && UI.nMouseY < nY + MICROPROFILE_TEXT_HEIGHT + 1)
			{
				if ((UI.nMouseLeft || UI.nMouseRight) && CBClick[nMenu])
				{
					CBClick[nMenu](i);
				}
				ProfileDrawBox(pCmd, nX, nY, nX + nWidth, nY + MICROPROFILE_TEXT_HEIGHT + 1, 0xff888888, MicroProfileBoxTypeFlat);
			}
			snprintf(buffer, SBUF_SIZE - 1, "%c %s", bSelected ? '*' : ' ', pString);
			uint32_t nLen = (uint32_t)strlen(buffer);
			ProfileDrawText(pCmd, nX, nY, (uint32_t)-1, buffer, nLen);
			nY += MICROPROFILE_TEXT_HEIGHT + 1;
		}
	}

	{
		static char FrameTimeMessage[64];
		float fToMs = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondCpu());
		uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
		float fMs = fToMs * (S.nFlipTicks);
		float fAverageMs = fToMs * (S.nFlipAggregateDisplay / nAggregateFrames);
		float fMaxMs = fToMs * S.nFlipMaxDisplay;
		snprintf(FrameTimeMessage, sizeof(FrameTimeMessage) - 1, "Time[%6.2f] Avg[%6.2f] Max[%6.2f]", fMs, fAverageMs, fMaxMs);
		uint32_t nLen = (uint32_t)strlen(FrameTimeMessage);
		pMenuText[nNumMenuItems++] = &FrameTimeMessage[0];
		ProfileDrawText(pCmd, nWidth - nLen * (MICROPROFILE_TEXT_WIDTH + 1), 0, -1, FrameTimeMessage, nLen);
	}
}


void UIApp::ProfileMoveGraph()
{

	int nZoom = UI.nMouseWheelDelta;
	int nPanX = 0;
	int nPanY = 0;
	static int X = 0, Y = 0;
	if (UI.nMouseDownLeft && !UI.nModDown)
	{
		nPanX = UI.nMouseX - X;
		nPanY = UI.nMouseY - Y;
	}
	X = UI.nMouseX;
	Y = UI.nMouseY;

	if (nZoom)
	{
		float fOldRange = UI.fDetailedRange;
		if (nZoom > 0)
		{
			UI.fDetailedRangeTarget = UI.fDetailedRange *= UI.nModDown ? 1.40f : 1.05f;
		}
		else
		{
			float fNewDetailedRange = UI.fDetailedRange / (UI.nModDown ? 1.40f : 1.05f);
			if (fNewDetailedRange < 1e-4f) //100ns
				fNewDetailedRange = 1e-4f;
			UI.fDetailedRangeTarget = UI.fDetailedRange = fNewDetailedRange;
		}

		float fDiff = fOldRange - UI.fDetailedRange;
		float fMousePrc = MicroProfileMax((float)UI.nMouseX / UI.nWidth, 0.f);
		UI.fDetailedOffsetTarget = UI.fDetailedOffset += fDiff * fMousePrc;

	}
	if (nPanX)
	{
		UI.fDetailedOffsetTarget = UI.fDetailedOffset += -nPanX * UI.fDetailedRange / UI.nWidth;
	}
	int nMode = MicroProfileGet()->nDisplay;
	if (nMode < MP_DRAW_SIZE)
	{
		UI.nOffsetY[nMode] -= nPanY;
		UI.nOffsetX[nMode] += nPanX;
		if (UI.nOffsetX[nMode] > 0)
			UI.nOffsetX[nMode] = 0;
		if (UI.nOffsetY[nMode] < 0)
			UI.nOffsetY[nMode] = 0;
	}
}

void UIApp::ProfileDrawCustom(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight)
{
	(void)nWidth;

	if ((uint32_t)-1 != UI.nCustomActive)
	{
		MicroProfile& S = *MicroProfileGet();
		ASSERT(UI.nCustomActive < MICROPROFILE_CUSTOM_MAX);
		MicroProfileCustom* pCustom = &UI.Custom[UI.nCustomActive];
		uint32_t nCount = pCustom->nNumTimers;
		uint32_t nAggregateFrames = S.nAggregateFrames ? S.nAggregateFrames : 1;
		uint32_t nExtraOffset = 1 + ((pCustom->nFlags & MICROPROFILE_CUSTOM_STACK) != 0 ? 3 : 0);
		uint32_t nOffsetYBase = nHeight - (nExtraOffset + nCount)* (1 + MICROPROFILE_TEXT_HEIGHT) - MICROPROFILE_CUSTOM_PADDING;
		uint32_t nOffsetY = nOffsetYBase;
		float fReference = pCustom->fReference;
		float fRcpReference = 1.f / fReference;
		uint32_t nReducedWidth = UI.nWidth - 2 * MICROPROFILE_CUSTOM_PADDING - MICROPROFILE_GRAPH_WIDTH;

		char Buffer[MICROPROFILE_NAME_MAX_LEN * 2 + 1];
		float* pTime = (float*)alloca(sizeof(float)*nCount);
		float* pTimeAvg = (float*)alloca(sizeof(float)*nCount);
		float* pTimeMax = (float*)alloca(sizeof(float)*nCount);
		uint32_t* pColors = (uint32_t*)alloca(sizeof(uint32_t)*nCount);
		uint32_t nMaxOffsetX = 0;
		ProfileDrawBox(pCmd, MICROPROFILE_CUSTOM_PADDING - 1, nOffsetY - 1, MICROPROFILE_CUSTOM_PADDING + nReducedWidth + 1, UI.nHeight - MICROPROFILE_CUSTOM_PADDING + 1, 0x88000000 | g_nMicroProfileBackColors[0], MicroProfileBoxTypeFlat);

		for (uint32_t i = 0; i < nCount; ++i)
		{
			uint16_t nTimerIndex = MicroProfileGetTimerIndex(pCustom->pTimers[i]);
			uint16_t nGroupIndex = MicroProfileGetGroupIndex(pCustom->pTimers[i]);
			float fToMs = MicroProfileTickToMsMultiplier(S.GroupInfo[nGroupIndex].Type == MicroProfileTokenTypeGpu ? MicroProfileTicksPerSecondGpu() : MicroProfileTicksPerSecondCpu());
			pTime[i] = S.Frame[nTimerIndex].nTicks * fToMs;
			pTimeAvg[i] = fToMs * (S.Aggregate[nTimerIndex].nTicks / nAggregateFrames);
			pTimeMax[i] = fToMs * (S.AggregateMax[nTimerIndex]);
			pColors[i] = S.TimerInfo[nTimerIndex].nColor;
		}

		ProfileDrawText(pCmd, MICROPROFILE_CUSTOM_PADDING + 3 * MICROPROFILE_TEXT_WIDTH, nOffsetY, (uint32_t)-1, "Avg", sizeof("Avg") - 1);
		ProfileDrawText(pCmd, MICROPROFILE_CUSTOM_PADDING + 13 * MICROPROFILE_TEXT_WIDTH, nOffsetY, (uint32_t)-1, "Max", sizeof("Max") - 1);
		for (uint32_t i = 0; i < nCount; ++i)
		{
			nOffsetY += (1 + MICROPROFILE_TEXT_HEIGHT);
			uint16_t nTimerIndex = MicroProfileGetTimerIndex(pCustom->pTimers[i]);
			uint16_t nGroupIndex = MicroProfileGetGroupIndex(pCustom->pTimers[i]);
			MicroProfileTimerInfo* pTimerInfo = &S.TimerInfo[nTimerIndex];
			uint32_t nOffsetX = MICROPROFILE_CUSTOM_PADDING;
			snprintf(Buffer, sizeof(Buffer) - 1, "%6.2f", pTimeAvg[i]);
			uint32_t nSize = (uint32_t)strlen(Buffer);
			ProfileDrawText(pCmd, nOffsetX, nOffsetY, (uint32_t)-1, Buffer, nSize);
			nOffsetX += (nSize + 2) * (MICROPROFILE_TEXT_WIDTH + 1);
			snprintf(Buffer, sizeof(Buffer) - 1, "%6.2f", pTimeMax[i]);
			nSize = (uint32_t)strlen(Buffer);
			ProfileDrawText(pCmd, nOffsetX, nOffsetY, (uint32_t)-1, Buffer, nSize);
			nOffsetX += (nSize + 2) * (MICROPROFILE_TEXT_WIDTH + 1);
			snprintf(Buffer, sizeof(Buffer) - 1, "%s:%s", S.GroupInfo[nGroupIndex].pName, pTimerInfo->pName);
			nSize = (uint32_t)strlen(Buffer);
			ProfileDrawText(pCmd, nOffsetX, nOffsetY, pTimerInfo->nColor, Buffer, nSize);
			nOffsetX += (nSize + 2) * (MICROPROFILE_TEXT_WIDTH + 1);
			nMaxOffsetX = MicroProfileMax(nMaxOffsetX, nOffsetX);
		}
		uint32_t nMaxWidth = nReducedWidth - nMaxOffsetX;

		if (pCustom->nFlags & MICROPROFILE_CUSTOM_BARS)
		{
			nOffsetY = nOffsetYBase;
			float* pMs = pCustom->nFlags & MICROPROFILE_CUSTOM_BAR_SOURCE_MAX ? pTimeMax : pTimeAvg;
			const char* pString = pCustom->nFlags & MICROPROFILE_CUSTOM_BAR_SOURCE_MAX ? "Max" : "Avg";
			ProfileDrawText(pCmd, nMaxOffsetX, nOffsetY, (uint32_t)-1, pString, (uint32_t)strlen(pString));
			snprintf(Buffer, sizeof(Buffer) - 1, "%6.2fms", fReference);
			uint32_t nSize = (uint32_t)strlen(Buffer);
			ProfileDrawText(pCmd, nReducedWidth - (1 + nSize) * (MICROPROFILE_TEXT_WIDTH + 1), nOffsetY, (uint32_t)-1, Buffer, nSize);
			for (uint32_t i = 0; i < nCount; ++i)
			{
				nOffsetY += (1 + MICROPROFILE_TEXT_HEIGHT);
				uint32_t nWidth = MicroProfileMin(nMaxWidth, (uint32_t)(nMaxWidth * pMs[i] * fRcpReference));
				ProfileDrawBox(pCmd, nMaxOffsetX, nOffsetY, nMaxOffsetX + nWidth, nOffsetY + MICROPROFILE_TEXT_HEIGHT, pColors[i] | 0xff000000, MicroProfileBoxTypeFlat);
			}
		}
		if (pCustom->nFlags & MICROPROFILE_CUSTOM_STACK)
		{
			nOffsetY += 2 * (1 + MICROPROFILE_TEXT_HEIGHT);
			const char* pString = pCustom->nFlags & MICROPROFILE_CUSTOM_STACK_SOURCE_MAX ? "Max" : "Avg";
			ProfileDrawText(pCmd, MICROPROFILE_CUSTOM_PADDING, nOffsetY, (uint32_t)-1, pString, (uint32_t)strlen(pString));
			snprintf(Buffer, sizeof(Buffer) - 1, "%6.2fms", fReference);
			uint32_t nSize = (uint32_t)strlen(Buffer);
			ProfileDrawText(pCmd, nReducedWidth - (1 + nSize) * (MICROPROFILE_TEXT_WIDTH + 1), nOffsetY, (uint32_t)-1, Buffer, nSize);
			nOffsetY += (1 + MICROPROFILE_TEXT_HEIGHT);
			float fPosX = MICROPROFILE_CUSTOM_PADDING;
			float* pMs = pCustom->nFlags & MICROPROFILE_CUSTOM_STACK_SOURCE_MAX ? pTimeMax : pTimeAvg;
			for (uint32_t i = 0; i < nCount; ++i)
			{
				float fWidth = pMs[i] * fRcpReference * nReducedWidth;
				uint32_t nX = (uint32_t)fPosX;
				fPosX += fWidth;
				uint32_t nXEnd = (uint32_t)fPosX;
				if (nX < nXEnd)
				{
					ProfileDrawBox(pCmd, nX, nOffsetY, nXEnd, nOffsetY + MICROPROFILE_TEXT_HEIGHT, pColors[i] | 0xff000000, MicroProfileBoxTypeFlat);
				}
			}
		}
	}
}

void UIApp::ProfileDraw(Cmd* pCmd, uint32_t nWidth, uint32_t nHeight)
{
	MICROPROFILE_SCOPE(g_MicroProfileDraw);
	MicroProfile& S = *MicroProfileGet();

	{
		static int once = 0;
		if (0 == once)
		{
			std::recursive_mutex& m = MicroProfileGetMutex();
			m.lock();
			ProfileInitUI();
			uint32_t nDisplay = S.nDisplay;
			ProfileLoadPreset(MICROPROFILE_DEFAULT_PRESET);
			once++;
			S.nDisplay = nDisplay;// dont load display, just state
			m.unlock();

		}
	}


	if (S.nDisplay)
	{
		std::recursive_mutex& m = MicroProfileGetMutex();
		m.lock();
		UI.nWidth = nWidth;
		UI.nHeight = nHeight;
		UI.nHoverToken = MICROPROFILE_INVALID_TOKEN;
		UI.nHoverTime = 0;
		UI.nHoverFrame = -1;
		if (S.nDisplay != MP_DRAW_DETAILED)
			S.nContextSwitchHoverThread = S.nContextSwitchHoverThreadAfter = S.nContextSwitchHoverThreadBefore = -1;
		ProfileMoveGraph();


		if (S.nDisplay == MP_DRAW_DETAILED || S.nDisplay == MP_DRAW_FRAME)
		{
			ProfileDrawDetailedView(pCmd, nWidth, nHeight, /* bDrawBars= */ S.nDisplay == MP_DRAW_DETAILED);
		}
		else if (S.nDisplay == MP_DRAW_BARS && S.nBars)
		{
			ProfileDrawBarView(pCmd, nWidth, nHeight);
		}
		else if (S.nDisplay == MP_DRAW_COUNTERS)
		{
			ProfileDrawCounterView(pCmd, nWidth, nHeight);
		}

		ProfileDrawMenu(pCmd, nWidth, nHeight);
		bool bMouseOverGraph = ProfileDrawGraph(pCmd, nWidth, nHeight);
		ProfileDrawCustom(pCmd, nWidth, nHeight);
		bool bHidden = S.nDisplay == MP_DRAW_HIDDEN;
		if (!bHidden)
		{
			uint32_t nLockedToolTipX = 3;
			bool bDeleted = false;
			for (int i = 0; i < MICROPROFILE_TOOLTIP_MAX_LOCKED; ++i)
			{
				int nIndex = (g_MicroProfileUI.LockedToolTipFront + i) % MICROPROFILE_TOOLTIP_MAX_LOCKED;
				if (g_MicroProfileUI.LockedToolTips[nIndex].ppStrings[0])
				{
					uint32_t nToolTipWidth = 0, nToolTipHeight = 0;
					ProfileFloatWindowSize(g_MicroProfileUI.LockedToolTips[nIndex].ppStrings, g_MicroProfileUI.LockedToolTips[nIndex].nNumStrings, 0, nToolTipWidth, nToolTipHeight, 0);
					uint32_t nStartY = nHeight - nToolTipHeight - 2;
					if (!bDeleted && UI.nMouseY > nStartY && UI.nMouseX > nLockedToolTipX && UI.nMouseX <= nLockedToolTipX + nToolTipWidth && (UI.nMouseLeft || UI.nMouseRight))
					{
						bDeleted = true;
						int j = i;
						for (; j < MICROPROFILE_TOOLTIP_MAX_LOCKED - 1; ++j)
						{
							int nIndex0 = (g_MicroProfileUI.LockedToolTipFront + j) % MICROPROFILE_TOOLTIP_MAX_LOCKED;
							int nIndex1 = (g_MicroProfileUI.LockedToolTipFront + j + 1) % MICROPROFILE_TOOLTIP_MAX_LOCKED;
							ProfileStringArrayCopy(&g_MicroProfileUI.LockedToolTips[nIndex0], &g_MicroProfileUI.LockedToolTips[nIndex1]);
						}
						ProfileStringArrayClear(&g_MicroProfileUI.LockedToolTips[(g_MicroProfileUI.LockedToolTipFront + j) % MICROPROFILE_TOOLTIP_MAX_LOCKED]);
					}
					else
					{
						ProfileDrawFloatWindow(pCmd, nLockedToolTipX, nHeight - nToolTipHeight - 2, &g_MicroProfileUI.LockedToolTips[nIndex].ppStrings[0], g_MicroProfileUI.LockedToolTips[nIndex].nNumStrings, g_MicroProfileUI.nLockedToolTipColor[nIndex]);
						nLockedToolTipX += nToolTipWidth + 4;
					}
				}
			}

			if (UI.nActiveMenu == 9)
			{
				if (S.nDisplay & MP_DRAW_DETAILED)
				{
					MicroProfileStringArray DetailedHelp;
					ProfileStringArrayClear(&DetailedHelp);
					ProfileStringArrayFormat(&DetailedHelp, "%s", MICROPROFILE_HELP_LEFT);
					ProfileStringArrayAddLiteral(&DetailedHelp, "Toggle Graph");
					ProfileStringArrayFormat(&DetailedHelp, "%s", MICROPROFILE_HELP_RIGHT);
					ProfileStringArrayAddLiteral(&DetailedHelp, "Zoom");
					ProfileStringArrayFormat(&DetailedHelp, "%s + %s", MICROPROFILE_HELP_MOD, MICROPROFILE_HELP_LEFT);
					ProfileStringArrayAddLiteral(&DetailedHelp, "Lock Tooltip");
					ProfileStringArrayAddLiteral(&DetailedHelp, "Drag");
					ProfileStringArrayAddLiteral(&DetailedHelp, "Pan View");
					ProfileStringArrayAddLiteral(&DetailedHelp, "Mouse Wheel");
					ProfileStringArrayAddLiteral(&DetailedHelp, "Zoom");
					ProfileDrawFloatWindow(pCmd, nWidth, MICROPROFILE_FRAME_HISTORY_HEIGHT + 20, DetailedHelp.ppStrings, DetailedHelp.nNumStrings, 0xff777777);

					MicroProfileStringArray DetailedHistoryHelp;
					ProfileStringArrayClear(&DetailedHistoryHelp);
					ProfileStringArrayFormat(&DetailedHistoryHelp, "%s", MICROPROFILE_HELP_LEFT);
					ProfileStringArrayAddLiteral(&DetailedHistoryHelp, "Center View");
					ProfileStringArrayFormat(&DetailedHistoryHelp, "%s", MICROPROFILE_HELP_RIGHT);
					ProfileStringArrayAddLiteral(&DetailedHistoryHelp, "Zoom to frame");
					ProfileDrawFloatWindow(pCmd, nWidth, 20, DetailedHistoryHelp.ppStrings, DetailedHistoryHelp.nNumStrings, 0xff777777);
				}
				else if (0 != (S.nDisplay & MP_DRAW_BARS) && S.nBars)
				{
					MicroProfileStringArray BarHelp;
					ProfileStringArrayClear(&BarHelp);
					ProfileStringArrayFormat(&BarHelp, "%s", MICROPROFILE_HELP_LEFT);
					ProfileStringArrayAddLiteral(&BarHelp, "Toggle Graph");
					ProfileStringArrayFormat(&BarHelp, "%s + %s", MICROPROFILE_HELP_MOD, MICROPROFILE_HELP_LEFT);
					ProfileStringArrayAddLiteral(&BarHelp, "Lock Tooltip");
					ProfileStringArrayAddLiteral(&BarHelp, "Drag");
					ProfileStringArrayAddLiteral(&BarHelp, "Pan View");
					ProfileDrawFloatWindow(pCmd, nWidth, MICROPROFILE_FRAME_HISTORY_HEIGHT + 20, BarHelp.ppStrings, BarHelp.nNumStrings, 0xff777777);

				}
				MicroProfileStringArray Debug;
				ProfileStringArrayClear(&Debug);
				ProfileStringArrayAddLiteral(&Debug, "Memory Usage");
				ProfileStringArrayFormat(&Debug, "%4.2fmb", S.nMemUsage / (1024.f * 1024.f));
#if MICROPROFILE_WEBSERVER
				ProfileStringArrayAddLiteral(&Debug, "Web Server Port");
				ProfileStringArrayFormat(&Debug, "%d", MicroProfileWebServerPort());
#endif
				uint32_t nFrameNext = (S.nFrameCurrent + 1) % MICROPROFILE_MAX_FRAME_HISTORY;
				MicroProfileFrameState* pFrameCurrent = &S.Frames[S.nFrameCurrent];
				MicroProfileFrameState* pFrameNext = &S.Frames[nFrameNext];

				ProfileStringArrayAddLiteral(&Debug, "");
				ProfileStringArrayAddLiteral(&Debug, "");
				ProfileStringArrayAddLiteral(&Debug, "Usage");
				ProfileStringArrayAddLiteral(&Debug, "markers [frames] ");

#if MICROPROFILE_CONTEXT_SWITCH_TRACE
				ProfileStringArrayAddLiteral(&Debug, "Context Switch");
				ProfileStringArrayFormat(&Debug, "%9d [%7d]", S.nContextSwitchUsage, MICROPROFILE_CONTEXT_SWITCH_BUFFER_SIZE / S.nContextSwitchUsage);
#endif

				for (uint32_t i = 0; i < MICROPROFILE_MAX_THREADS; ++i)
				{
					if (pFrameCurrent->nLogStart[i] && S.Pool[i])
					{
						uint32_t nEnd = pFrameNext->nLogStart[i];
						uint32_t nStart = pFrameCurrent->nLogStart[i];
						// volatile is a workaround for MSVC 2015 bug - compiler inserts cmov for the condition below despite the fact that RHS traps when nUsage==0
						volatile uint32_t nUsage = nStart <= nEnd ? (nEnd - nStart) : (nEnd + MICROPROFILE_BUFFER_SIZE - nStart);
						uint32_t nFrameSupport = (nUsage == 0) ? MICROPROFILE_BUFFER_SIZE : MICROPROFILE_BUFFER_SIZE / nUsage;
						ProfileStringArrayFormat(&Debug, "%s", &S.Pool[i]->ThreadName[0]);
						ProfileStringArrayFormat(&Debug, "%9d [%7d]", nUsage, nFrameSupport);
					}
				}

				ProfileDrawFloatWindow(pCmd, 0, nHeight - 10, Debug.ppStrings, Debug.nNumStrings, 0xff777777);
			}



			if (UI.nActiveMenu == (uint32_t)-1 && !bMouseOverGraph)
			{
				if (UI.nHoverToken != MICROPROFILE_INVALID_TOKEN)
				{
					ProfileDrawFloatTooltip(pCmd, UI.nMouseX, UI.nMouseY, (uint32_t)UI.nHoverToken, UI.nHoverTime);
				}
				else if (S.nContextSwitchHoverThreadAfter != (uint32_t)-1 && S.nContextSwitchHoverThreadBefore != (uint32_t)-1)
				{
					float fToMs = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondCpu());
					MicroProfileStringArray ToolTip;
					ProfileStringArrayClear(&ToolTip);
					ProfileStringArrayAddLiteral(&ToolTip, "Context Switch");
					ProfileStringArrayFormat(&ToolTip, "%04x", S.nContextSwitchHoverThread);
					ProfileStringArrayAddLiteral(&ToolTip, "Before");
					ProfileStringArrayFormat(&ToolTip, "%04x", S.nContextSwitchHoverThreadBefore);
					ProfileStringArrayAddLiteral(&ToolTip, "After");
					ProfileStringArrayFormat(&ToolTip, "%04x", S.nContextSwitchHoverThreadAfter);
					ProfileStringArrayAddLiteral(&ToolTip, "Duration");
					int64_t nDifference = MicroProfileLogTickDifference(S.nContextSwitchHoverTickIn, S.nContextSwitchHoverTickOut);
					ProfileStringArrayFormat(&ToolTip, "%6.2fms", fToMs * nDifference);
					ProfileStringArrayAddLiteral(&ToolTip, "CPU");
					ProfileStringArrayFormat(&ToolTip, "%d", S.nContextSwitchHoverCpu);
					ProfileDrawFloatWindow(pCmd, UI.nMouseX, UI.nMouseY + 20, &ToolTip.ppStrings[0], ToolTip.nNumStrings, -1);
				}
				else if (UI.nHoverFrame != -1)
				{
					uint32_t nNextFrame = (UI.nHoverFrame + 1) % MICROPROFILE_MAX_FRAME_HISTORY;
					int64_t nTick = S.Frames[UI.nHoverFrame].nFrameStartCpu;
					int64_t nTickNext = S.Frames[nNextFrame].nFrameStartCpu;
					int64_t nTickGpu = S.Frames[UI.nHoverFrame].nFrameStartGpu;
					int64_t nTickNextGpu = S.Frames[nNextFrame].nFrameStartGpu;

					float fToMs = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondCpu());
					float fToMsGpu = MicroProfileTickToMsMultiplier(MicroProfileTicksPerSecondGpu());
					float fMs = fToMs * (nTickNext - nTick);
					float fMsGpu = fToMsGpu * (nTickNextGpu - nTickGpu);
					MicroProfileStringArray ToolTip;
					ProfileStringArrayClear(&ToolTip);
					ProfileStringArrayFormat(&ToolTip, "Frame %d", UI.nHoverFrame);
#if MICROPROFILE_DEBUG
					ProfileStringArrayFormat(&ToolTip, "%p", &S.Frames[UI.nHoverFrame]);
#else
					ProfileStringArrayAddLiteral(&ToolTip, "");
#endif
					ProfileStringArrayAddLiteral(&ToolTip, "CPU Time");
					ProfileStringArrayFormat(&ToolTip, "%6.2fms", fMs);
					ProfileStringArrayAddLiteral(&ToolTip, "GPU Time");
					ProfileStringArrayFormat(&ToolTip, "%6.2fms", fMsGpu);
#if MICROPROFILE_DEBUG
					for (int i = 0; i < MICROPROFILE_MAX_THREADS; ++i)
					{
						if (S.Frames[UI.nHoverFrame].nLogStart[i])
						{
							ProfileStringArrayFormat(&ToolTip, "%d", i);
							ProfileStringArrayFormat(&ToolTip, "%d", S.Frames[UI.nHoverFrame].nLogStart[i]);
						}
					}
#endif
					ProfileDrawFloatWindow(pCmd, UI.nMouseX, UI.nMouseY + 20, &ToolTip.ppStrings[0], ToolTip.nNumStrings, -1);
				}
				if (UI.nMouseLeft)
				{
					if (UI.nHoverToken != MICROPROFILE_INVALID_TOKEN)
						ProfileToggleGraph(UI.nHoverToken);
				}
			}
		}

#if MICROPROFILE_DRAWCURSOR
		{
			float fCursor[8] =
			{
				float(MicroProfileMax(0, (int)UI.nMouseX - 3)), float(UI.nMouseY),
				float(MicroProfileMin(nWidth, UI.nMouseX + 3)), float(UI.nMouseY),
				float(UI.nMouseX), float(MicroProfileMax((int)UI.nMouseY - 3, 0)),
				float(UI.nMouseX), float(MicroProfileMin(nHeight, UI.nMouseY + 3)),
			};
			MicroProfileDrawLine2D(2, &fCursor[0], 0xff00ff00);
			MicroProfileDrawLine2D(2, &fCursor[4], 0xff00ff00);
		}
#endif
		m.unlock();
	}
	else if (UI.nCustomActive != (uint32_t)-1)
	{
		std::recursive_mutex& m = MicroProfileGetMutex();
		m.lock();
		ProfileDrawGraph(pCmd, nWidth, nHeight);
		ProfileDrawCustom(pCmd, nWidth, nHeight);
		m.unlock();

	}
	UI.nMouseLeft = UI.nMouseRight = 0;
	UI.nMouseLeftMod = UI.nMouseRightMod = 0;
	UI.nMouseWheelDelta = 0;
	if (S.nOverflow)
		S.nOverflow--;

	UI.fDetailedOffset = UI.fDetailedOffset + (UI.fDetailedOffsetTarget - UI.fDetailedOffset) * MICROPROFILE_ANIM_DELAY_PRC;
	UI.fDetailedRange = UI.fDetailedRange + (UI.fDetailedRangeTarget - UI.fDetailedRange) * MICROPROFILE_ANIM_DELAY_PRC;


}

bool UIApp::ProfileIsDrawing()
{
	MicroProfile& S = *MicroProfileGet();
	return S.nDisplay != 0;
}

void UIApp::ProfileToggleGraph(MicroProfileToken nToken)
{
	MicroProfile& S = *MicroProfileGet();
	uint32_t nTimerId = MicroProfileGetTimerIndex(nToken);
	nToken &= 0xffff;
	int32_t nMinSort = 0x7fffffff;
	int32_t nFreeIndex = -1;
	int32_t nMinIndex = 0;
	int32_t nMaxSort = 0x80000000;
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
	{
		if (S.Graph[i].nToken == MICROPROFILE_INVALID_TOKEN)
			nFreeIndex = i;
		if (S.Graph[i].nToken == nToken)
		{
			S.Graph[i].nToken = MICROPROFILE_INVALID_TOKEN;
			S.TimerInfo[nTimerId].bGraph = false;
			return;
		}
		if (S.Graph[i].nKey < nMinSort)
		{
			nMinSort = S.Graph[i].nKey;
			nMinIndex = i;
		}
		if (S.Graph[i].nKey > nMaxSort)
		{
			nMaxSort = S.Graph[i].nKey;
		}
	}
	int nIndex = nFreeIndex > -1 ? nFreeIndex : nMinIndex;
	if (nFreeIndex == -1)
	{
		uint32_t idx = MicroProfileGetTimerIndex(S.Graph[nIndex].nToken);
		S.TimerInfo[idx].bGraph = false;
	}
	S.Graph[nIndex].nToken = nToken;
	S.Graph[nIndex].nKey = nMaxSort + 1;
	memset(&S.Graph[nIndex].nHistory[0], 0, sizeof(S.Graph[nIndex].nHistory));
	S.TimerInfo[nTimerId].bGraph = true;
}

void UIApp::ProfileModKey(uint32_t nKeyState)
{
	UI.nModDown = nKeyState ? 1 : 0;
}

void UIApp::ProfileClearGraph()
{
	MicroProfile& S = *MicroProfileGet();
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
	{
		if (S.Graph[i].nToken != 0)
		{
			S.Graph[i].nToken = MICROPROFILE_INVALID_TOKEN;
		}
	}
}

void UIApp::updateProfileMousePosition(uint32_t nX, uint32_t nY, int nWheelDelta)
{
	UI.nMouseX = nX;
	UI.nMouseY = nY;
	UI.nMouseWheelDelta = nWheelDelta;
}

void UIApp::updateProfileMouseButton(uint32_t nLeft, uint32_t nRight)
{
	bool bCanRelease = abs((int)(UI.nMouseDownX - UI.nMouseX)) + abs((int)(UI.nMouseDownY - UI.nMouseY)) < 3;

	if (0 == nLeft && UI.nMouseDownLeft && bCanRelease)
	{
		if (UI.nModDown)
			UI.nMouseLeftMod = 1;
		else
			UI.nMouseLeft = 1;
	}

	if (0 == nRight && UI.nMouseDownRight && bCanRelease)
	{
		if (UI.nModDown)
			UI.nMouseRightMod = 1;
		else
			UI.nMouseRight = 1;
	}
	if ((nLeft || nRight) && !(UI.nMouseDownLeft || UI.nMouseDownRight))
	{
		UI.nMouseDownX = UI.nMouseX;
		UI.nMouseDownY = UI.nMouseY;
	}

	UI.nMouseDownLeft = nLeft;
	UI.nMouseDownRight = nRight;

}

void UIApp::ProfileDrawLineVertical(Cmd* pCmd, int nX, int nTop, int nBottom, uint32_t nColor)
{
	ProfileDrawBox(pCmd, nX, nTop, nX + 1, nBottom, nColor, MicroProfileBoxTypeFlat);
}

void UIApp::ProfileDrawLineHorizontal(Cmd* pCmd, int nLeft, int nRight, int nY, uint32_t nColor)
{
	ProfileDrawBox(pCmd, nLeft, nY, nRight, nY + 1, nColor, MicroProfileBoxTypeFlat);
}



#include <stdio.h>

#define MICROPROFILE_PRESET_HEADER_MAGIC 0x28586813
#define MICROPROFILE_PRESET_HEADER_VERSION 0x00000102
struct MicroProfilePresetHeader
{
	uint32_t nMagic;
	uint32_t nVersion;
	//groups, threads, aggregate, reference frame, graphs timers
	uint32_t nGroups[MICROPROFILE_MAX_GROUPS];
	uint32_t nThreads[MICROPROFILE_MAX_THREADS];
	uint32_t nGraphName[MICROPROFILE_MAX_GRAPHS];
	uint32_t nGraphGroupName[MICROPROFILE_MAX_GRAPHS];
	uint32_t nAllGroupsWanted;
	uint32_t nAllThreadsWanted;
	uint32_t nAggregateFlip;
	float fReferenceTime;
	uint32_t nBars;
	uint32_t nDisplay;
	uint32_t nOpacityBackground;
	uint32_t nOpacityForeground;
	uint32_t nShowSpikes;
};

#ifndef MICROPROFILE_PRESET_FILENAME_FUNC
#define MICROPROFILE_PRESET_FILENAME_FUNC ProfilePresetFilename
static const char* ProfilePresetFilename(const char* pSuffix)
{
	static char filename[512];
	snprintf(filename, sizeof(filename) - 1, ".microprofilepreset.%s", pSuffix);
	return filename;
}
#endif

void UIApp::ProfileSavePreset(const char* pPresetName)
{
	std::lock_guard<std::recursive_mutex> Lock(MicroProfileGetMutex());
	FILE* F;
	fopen_s(&F, MICROPROFILE_PRESET_FILENAME_FUNC(pPresetName), "wb");

	if (!F) return;

	MicroProfile& S = *MicroProfileGet();

	MicroProfilePresetHeader Header;
	memset(&Header, 0, sizeof(Header));
	Header.nAggregateFlip = S.nAggregateFlip;
	Header.nBars = S.nBars;
	Header.fReferenceTime = S.fReferenceTime;
	Header.nAllGroupsWanted = S.nAllGroupsWanted;
	Header.nAllThreadsWanted = S.nAllThreadsWanted;
	Header.nMagic = MICROPROFILE_PRESET_HEADER_MAGIC;
	Header.nVersion = MICROPROFILE_PRESET_HEADER_VERSION;
	Header.nDisplay = S.nDisplay;
	Header.nOpacityBackground = UI.nOpacityBackground;
	Header.nOpacityForeground = UI.nOpacityForeground;
	Header.nShowSpikes = UI.bShowSpikes ? 1 : 0;
	fwrite(&Header, sizeof(Header), 1, F);
	uint64_t nMask = 1;
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GROUPS; ++i)
	{
		if (S.nActiveGroupWanted & nMask)
		{
			uint32_t offset = ftell(F);
			const char* pName = S.GroupInfo[i].pName;
			int nLen = (int)strlen(pName) + 1;
			fwrite(pName, nLen, 1, F);
			Header.nGroups[i] = offset;
		}
		nMask <<= 1;
	}
	for (uint32_t i = 0; i < MICROPROFILE_MAX_THREADS; ++i)
	{
		MicroProfileThreadLog* pLog = S.Pool[i];
		if (pLog && S.nThreadActive[i])
		{
			uint32_t nOffset = ftell(F);
			const char* pName = &pLog->ThreadName[0];
			int nLen = (int)strlen(pName) + 1;
			fwrite(pName, nLen, 1, F);
			Header.nThreads[i] = nOffset;
		}
	}
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
	{
		MicroProfileToken nToken = S.Graph[i].nToken;
		if (nToken != MICROPROFILE_INVALID_TOKEN)
		{
			uint32_t nGroupIndex = MicroProfileGetGroupIndex(nToken);
			uint32_t nTimerIndex = MicroProfileGetTimerIndex(nToken);
			const char* pGroupName = S.GroupInfo[nGroupIndex].pName;
			const char* pTimerName = S.TimerInfo[nTimerIndex].pName;
			ASSERT(pGroupName);
			ASSERT(pTimerName);
			int nGroupLen = (int)strlen(pGroupName) + 1;
			int nTimerLen = (int)strlen(pTimerName) + 1;

			uint32_t nOffsetGroup = ftell(F);
			fwrite(pGroupName, nGroupLen, 1, F);
			uint32_t nOffsetTimer = ftell(F);
			fwrite(pTimerName, nTimerLen, 1, F);
			Header.nGraphName[i] = nOffsetTimer;
			Header.nGraphGroupName[i] = nOffsetGroup;
		}
	}
	fseek(F, 0, SEEK_SET);
	fwrite(&Header, sizeof(Header), 1, F);

	fclose(F);

}



void UIApp::ProfileLoadPreset(const char* pSuffix)
{
	std::lock_guard<std::recursive_mutex> Lock(MicroProfileGetMutex());
	FILE* F;
	fopen_s(&F, MICROPROFILE_PRESET_FILENAME_FUNC(pSuffix), "rb");
	if (!F)
	{
		return;
	}
	fseek(F, 0, SEEK_END);
	int nSize = ftell(F);
	char* const pBuffer = (char*)alloca(nSize);
	fseek(F, 0, SEEK_SET);
	int nRead = (int)fread(pBuffer, nSize, 1, F);
	fclose(F);
	if (1 != nRead)
		return;

	MicroProfile& S = *MicroProfileGet();

	MicroProfilePresetHeader& Header = *(MicroProfilePresetHeader*)pBuffer;

	if (Header.nMagic != MICROPROFILE_PRESET_HEADER_MAGIC || Header.nVersion != MICROPROFILE_PRESET_HEADER_VERSION)
	{
		return;
	}

	S.nAggregateFlip = Header.nAggregateFlip;
	S.nBars = Header.nBars;
	S.fReferenceTime = Header.fReferenceTime;
	S.fRcpReferenceTime = 1.f / Header.fReferenceTime;
	S.nAllGroupsWanted = Header.nAllGroupsWanted;
	S.nAllThreadsWanted = Header.nAllThreadsWanted;
	S.nDisplay = Header.nDisplay;
	S.nActiveGroupWanted = 0;
	UI.nOpacityBackground = Header.nOpacityBackground;
	UI.nOpacityForeground = Header.nOpacityForeground;
	UI.bShowSpikes = Header.nShowSpikes == 1;

	memset(&S.nThreadActive[0], 0, sizeof(S.nThreadActive));

	for (uint32_t i = 0; i < MICROPROFILE_MAX_GROUPS; ++i)
	{
		if (Header.nGroups[i])
		{
			const char* pGroupName = pBuffer + Header.nGroups[i];
			for (uint32_t j = 0; j < MICROPROFILE_MAX_GROUPS; ++j)
			{
				if (0 == MP_STRCASECMP(pGroupName, S.GroupInfo[j].pName))
				{
					S.nActiveGroupWanted |= (1ll << j);
				}
			}
		}
	}
	for (uint32_t i = 0; i < MICROPROFILE_MAX_THREADS; ++i)
	{
		if (Header.nThreads[i])
		{
			const char* pThreadName = pBuffer + Header.nThreads[i];
			for (uint32_t j = 0; j < MICROPROFILE_MAX_THREADS; ++j)
			{
				MicroProfileThreadLog* pLog = S.Pool[j];
				if (pLog && 0 == MP_STRCASECMP(pThreadName, &pLog->ThreadName[0]))
				{
					S.nThreadActive[j] = 1;
				}
			}
		}
	}
	for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
	{
		MicroProfileToken nPrevToken = S.Graph[i].nToken;
		S.Graph[i].nToken = MICROPROFILE_INVALID_TOKEN;
		if (Header.nGraphName[i] && Header.nGraphGroupName[i])
		{
			const char* pGraphName = pBuffer + Header.nGraphName[i];
			const char* pGraphGroupName = pBuffer + Header.nGraphGroupName[i];
			for (uint32_t j = 0; j < S.nTotalTimers; ++j)
			{
				uint64_t nGroupIndex = S.TimerInfo[j].nGroupIndex;
				if (0 == MP_STRCASECMP(pGraphName, S.TimerInfo[j].pName) && 0 == MP_STRCASECMP(pGraphGroupName, S.GroupInfo[nGroupIndex].pName))
				{
					MicroProfileToken nToken = MicroProfileMakeToken(1ll << nGroupIndex, (uint16_t)j);
					S.Graph[i].nToken = nToken;			// note: group index is stored here but is checked without in MicroProfileToggleGraph()!
					S.TimerInfo[j].bGraph = true;
					if (nToken != nPrevToken)
					{
						memset(&S.Graph[i].nHistory, 0, sizeof(S.Graph[i].nHistory));
					}
					break;
				}
			}
		}
	}
}

uint32_t UIApp::ProfileCustomGroupFind(const char* pCustomName)
{
	for (uint32_t i = 0; i < UI.nCustomCount; ++i)
	{
		if (!MP_STRCASECMP(pCustomName, UI.Custom[i].pName))
		{
			return i;
		}
	}
	return (uint32_t)-1;
}

uint32_t UIApp::ProfileCustomGroup(const char* pCustomName)
{
	for (uint32_t i = 0; i < UI.nCustomCount; ++i)
	{
		if (!MP_STRCASECMP(pCustomName, UI.Custom[i].pName))
		{
			return i;
		}
	}
	ASSERT(UI.nCustomCount < MICROPROFILE_CUSTOM_MAX);
	uint32_t nIndex = UI.nCustomCount;
	UI.nCustomCount++;
	memset(&UI.Custom[nIndex], 0, sizeof(UI.Custom[nIndex]));
	uint32_t nLen = (uint32_t)strlen(pCustomName);
	if (nLen > MICROPROFILE_NAME_MAX_LEN - 1)
		nLen = MICROPROFILE_NAME_MAX_LEN - 1;
	memcpy(&UI.Custom[nIndex].pName[0], pCustomName, nLen);
	UI.Custom[nIndex].pName[nLen] = '\0';
	return nIndex;
}
void UIApp::ProfileCustomGroup(const char* pCustomName, uint32_t nMaxTimers, uint32_t nAggregateFlip, float fReferenceTime, uint32_t nFlags)
{
	uint32_t nIndex = ProfileCustomGroup(pCustomName);
	ASSERT(UI.Custom[nIndex].pTimers == 0);//only call once!
	UI.Custom[nIndex].pTimers = &UI.CustomTimer[UI.nCustomTimerCount];
	UI.Custom[nIndex].nMaxTimers = nMaxTimers;
	UI.Custom[nIndex].fReference = fReferenceTime;
	UI.nCustomTimerCount += nMaxTimers;
	ASSERT(UI.nCustomTimerCount <= MICROPROFILE_CUSTOM_MAX_TIMERS); //bump MICROPROFILE_CUSTOM_MAX_TIMERS
	UI.Custom[nIndex].nFlags = nFlags;
	UI.Custom[nIndex].nAggregateFlip = nAggregateFlip;
}

void UIApp::ProfileCustomGroupEnable(uint32_t nIndex)
{
	if (nIndex < UI.nCustomCount)
	{
		MicroProfile& S = *MicroProfileGet();
		S.nForceGroupUI = UI.Custom[nIndex].nGroupMask;
		MicroProfileSetAggregateFrames(UI.Custom[nIndex].nAggregateFlip);
		S.fReferenceTime = UI.Custom[nIndex].fReference;
		S.fRcpReferenceTime = 1.f / UI.Custom[nIndex].fReference;
		UI.nCustomActive = nIndex;

		for (uint32_t i = 0; i < MICROPROFILE_MAX_GRAPHS; ++i)
		{
			if (S.Graph[i].nToken != MICROPROFILE_INVALID_TOKEN)
			{
				uint32_t nTimerId = MicroProfileGetTimerIndex(S.Graph[i].nToken);
				S.TimerInfo[nTimerId].bGraph = false;
				S.Graph[i].nToken = MICROPROFILE_INVALID_TOKEN;
			}
		}

		for (uint32_t i = 0; i < UI.Custom[nIndex].nNumTimers; ++i)
		{
			if (i == MICROPROFILE_MAX_GRAPHS)
			{
				break;
			}
			S.Graph[i].nToken = UI.Custom[nIndex].pTimers[i];
			S.Graph[i].nKey = i;
			uint32_t nTimerId = MicroProfileGetTimerIndex(S.Graph[i].nToken);
			S.TimerInfo[nTimerId].bGraph = true;
		}
	}
}

void UIApp::ProfileCustomGroupToggle(const char* pCustomName)
{
	uint32_t nIndex = ProfileCustomGroupFind(pCustomName);
	if (nIndex == (uint32_t)-1 || nIndex == UI.nCustomActive)
	{
		ProfileCustomGroupDisable();
	}
	else
	{
		ProfileCustomGroupEnable(nIndex);
	}
}

void UIApp::ProfileCustomGroupEnable(const char* pCustomName)
{
	uint32_t nIndex = ProfileCustomGroupFind(pCustomName);
	ProfileCustomGroupEnable(nIndex);
}
void UIApp::ProfileCustomGroupDisable()
{
	MicroProfile& S = *MicroProfileGet();
	S.nForceGroupUI = 0;
	UI.nCustomActive = (uint32_t)-1;
}

void UIApp::ProfileCustomGroupAddTimer(const char* pCustomName, const char* pGroup, const char* pTimer)
{
	uint32_t nIndex = ProfileCustomGroupFind(pCustomName);
	if ((uint32_t)-1 == nIndex)
	{
		return;
	}
	uint32_t nTimerIndex = UI.Custom[nIndex].nNumTimers;
	ASSERT(nTimerIndex < UI.Custom[nIndex].nMaxTimers);
	uint64_t nToken = MicroProfileFindToken(pGroup, pTimer);
	ASSERT(nToken != MICROPROFILE_INVALID_TOKEN); //Timer must be registered first.
	UI.Custom[nIndex].pTimers[nTimerIndex] = nToken;
	uint16_t nGroup = MicroProfileGetGroupIndex(nToken);
	UI.Custom[nIndex].nGroupMask |= (1ll << nGroup);
	UI.Custom[nIndex].nNumTimers++;
}
#endif
