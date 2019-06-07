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

#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../Middleware_3/Text/Fontstash.h"

#include "../../Common_3/OS/Input/InputSystem.h"
#include "../../Common_3/OS/Input/InputMappings.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

namespace PlatformEvents {
extern bool skipMouseCapture;
}

FSRoot                         FSR_MIDDLEWARE_UI = FSR_Middleware1;
static eastl::vector<UIApp*> gInstances;
static Mutex                   gMutex;

extern void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver, uint32_t const maxDynamicUIUpdatesPerBatch);
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

UIApp::UIApp(int32_t const fontAtlasSize, uint32_t const maxDynamicUIUpdatesPerBatch)
{
	mFontAtlasSize = fontAtlasSize;
	mMaxDynamicUIUpdatesPerBatch = maxDynamicUIUpdatesPerBatch;
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
	initGUIDriver(pImpl->pRenderer, &pDriver, mMaxDynamicUIUpdatesPerBatch);

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
	UIApp** it = eastl::find(gInstances.begin(), gInstances.end(), this);
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
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pImpl->pFontStash->drawText(
		cmd, pText, screenCoordsInPx.getX(), screenCoordsInPx.getY(), pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize,
		pDesc->mFontSpacing, pDesc->mFontBlur);
}

void UIApp::DrawTextInWorldSpace(Cmd* pCmd, const char* pText, const mat4& matWorld, const mat4& matProjView, const TextDrawDesc* pDrawDesc)
{
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
	const GpuProfileDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultGpuProfileDrawDesc;
	float2                    pos = screenCoordsInPx;
	pImpl->pFontStash->drawText(
		pCmd, "-----GPU Times-----", pos.x, pos.y, pDesc->mDrawDesc.mFontID, pDesc->mDrawDesc.mFontColor, pDesc->mDrawDesc.mFontSize,
		pDesc->mDrawDesc.mFontSpacing, pDesc->mDrawDesc.mFontBlur);
	pos.y += pDesc->mHeightOffset;

	draw_gpu_profile_recurse(pCmd, pImpl->pFontStash, pos, pDesc, pGpuProfiler, &pGpuProfiler->mRoot);
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
	GuiComponent** it = eastl::find(pImpl->mComponents.begin(), pImpl->mComponents.end(), pComponent);
	if (it != pImpl->mComponents.end())
	{
		(*it)->RemoveAllWidgets();
		pImpl->mComponents.erase(it);
		GuiComponent** active_it = eastl::find(pImpl->mComponentsToUpdate.begin(), pImpl->mComponentsToUpdate.end(), pComponent);
		if (active_it != pImpl->mComponentsToUpdate.end())
			pImpl->mComponentsToUpdate.erase(active_it);
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

	eastl::vector<GuiComponent*> activeComponents(pImpl->mComponentsToUpdate.size());
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
	decltype(mWidgets)::iterator it = eastl::find(mWidgets.begin(), mWidgets.end(), pWidget);
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
		LOGF(LogLevel::eERROR, "Error loading texture file: %s", pJoystickTexture);
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
	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	vbDesc.mDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.mDesc.mVertexStride = sizeof(float4);
	vbDesc.ppBuffer = &pMeshBuffer;
	addResource(&vbDesc);
	/************************************************************************/
	/************************************************************************/

	mInitialized = true;
	return true;
}

void VirtualJoystickUI::Exit()
{
	if (!mInitialized)
		return;

	removeSampler(pRenderer, pSampler);
	removeResource(pMeshBuffer);
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

	uint64_t bufferOffset = 0;
	for (uint i = 0; i < 2; i++)
	{
		if (mSticks[i].mIsPressed)
		{
			float2 joystickSize = float2(extSide);
			vec2   joystickCenter = mSticks[i].mStartPos;
			float2 joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			// the last variable can be used to create a border
			TexVertex        vertices[4] = { MAKETEXQUAD(
				joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
			BufferUpdateDesc updateDesc = { pMeshBuffer, vertices, 0, bufferOffset, sizeof(vertices) };
			updateResource(&updateDesc);
			cmdBindVertexBuffer(pCmd, 1, &pMeshBuffer, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(vertices);

			joystickSize = float2(intSide);
			joystickCenter = mSticks[i].mCurrPos;
			joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			// the last variable can be used to create a border
			TexVertex verticesInner[4] = { MAKETEXQUAD(
				joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0) };
			updateDesc = { pMeshBuffer, verticesInner, 0, bufferOffset, sizeof(verticesInner) };
			updateResource(&updateDesc);
			cmdBindVertexBuffer(pCmd, 1, &pMeshBuffer, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(verticesInner);
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
