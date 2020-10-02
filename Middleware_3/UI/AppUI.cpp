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

#include "AppUI.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"

#include "../../Common_3/Renderer/IResourceLoader.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../Middleware_3/Text/Fontstash.h"
#include "../../Common_3/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../Common_3/OS/Interfaces/IMemory.h"

extern void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver);
extern void removeGUIDriver(GUIDriver* pDriver);

static TextDrawDesc       gDefaultTextDrawDesc = TextDrawDesc(0, 0xffffffff, 16);

IWidget* CollapsingHeaderWidget::Clone() const
{
	CollapsingHeaderWidget* pWidget =
		tf_placement_new<CollapsingHeaderWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mDefaultOpen, this->mCollapsed, this->mHeaderIsVisible);

	// Need to read the subwidgets as the destructor will remove them all
	for (size_t i = 0; i < mGroupedWidgets.size(); ++i)
		pWidget->AddSubWidget(*mGroupedWidgets[i]);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DebugTexturesWidget::Clone() const
{
	DebugTexturesWidget* pWidget = tf_placement_new<DebugTexturesWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel);
	pWidget->SetTextures(this->mTextures, mTextureDisplaySize);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* LabelWidget::Clone() const
{
	LabelWidget* pWidget = tf_placement_new<LabelWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* ColorLabelWidget::Clone() const
{
	ColorLabelWidget* pWidget = tf_placement_new<ColorLabelWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mColor);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* HorizontalSpaceWidget::Clone() const
{
	HorizontalSpaceWidget* pWidget = tf_placement_new<HorizontalSpaceWidget>(tf_calloc(1, sizeof(*pWidget)));

	CloneBase(pWidget);

	return pWidget;
}

IWidget* ButtonWidget::Clone() const
{
	ButtonWidget* pWidget = tf_placement_new<ButtonWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SeparatorWidget::Clone() const
{
	SeparatorWidget* pWidget = tf_placement_new<SeparatorWidget>(tf_calloc(1, sizeof(*pWidget)));

	CloneBase(pWidget);

	return pWidget;
}

IWidget* VerticalSeparatorWidget::Clone() const
{
	VerticalSeparatorWidget* pWidget = tf_placement_new<VerticalSeparatorWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLineCount);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SliderFloatWidget::Clone() const
{
	SliderFloatWidget* pWidget = tf_placement_new<SliderFloatWidget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SliderFloat2Widget::Clone() const
{
	SliderFloat2Widget* pWidget = tf_placement_new<SliderFloat2Widget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SliderFloat3Widget::Clone() const
{
	SliderFloat3Widget* pWidget = tf_placement_new<SliderFloat3Widget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SliderFloat4Widget::Clone() const
{
	SliderFloat4Widget* pWidget = tf_placement_new<SliderFloat4Widget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SliderIntWidget::Clone() const
{
	SliderIntWidget* pWidget = tf_placement_new<SliderIntWidget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* SliderUintWidget::Clone() const
{
	SliderUintWidget* pWidget = tf_placement_new<SliderUintWidget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mMin, this->mMax, this->mStep, this->mFormat);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* RadioButtonWidget::Clone() const
{
	RadioButtonWidget* pWidget =
		tf_placement_new<RadioButtonWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mRadioId);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DropdownWidget::Clone() const
{
	const char** ppNames = (const char**)alloca(mValues.size() * sizeof(const char*));
	for (uint32_t i = 0; i < (uint32_t)mValues.size(); ++i)
		ppNames[i] = mNames[i].c_str();
	DropdownWidget* pWidget = tf_placement_new<DropdownWidget>(
		tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, ppNames, this->mValues.data(), (uint32_t)this->mValues.size());

	CloneBase(pWidget);

	return pWidget;
}

IWidget* ProgressBarWidget::Clone() const
{
	ProgressBarWidget* pWidget =
		tf_placement_new<ProgressBarWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, mMaxProgress);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* ColorSliderWidget::Clone() const
{
	ColorSliderWidget* pWidget = tf_placement_new<ColorSliderWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* HistogramWidget::Clone() const
{
	HistogramWidget* pWidget = tf_placement_new<HistogramWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pValues, this->mCount, this->mMinScale, this->mMaxScale, this->mHistogramSize, this->mHistogramTitle);

	CloneBase(pWidget);

	return pWidget;
}


IWidget* PlotLinesWidget::Clone() const
{
	PlotLinesWidget* pWidget = tf_placement_new<PlotLinesWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mValues, this->mNumValues, this->mScaleMin, this->mScaleMax, this->mPlotScale, this->mTitle);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* ColorPickerWidget::Clone() const
{
	ColorPickerWidget* pWidget = tf_placement_new<ColorPickerWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* TextboxWidget::Clone() const
{
	TextboxWidget* pWidget =
		tf_placement_new<TextboxWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mLength, this->mAutoSelectAll);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DynamicTextWidget::Clone() const
{
	DynamicTextWidget* pWidget =
		tf_placement_new<DynamicTextWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mLength, this->pColor);

	CloneBase(pWidget);

	return pWidget;
}


IWidget* FilledRectWidget::Clone() const
{
	FilledRectWidget* pWidget =
		tf_placement_new<FilledRectWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos, this->mScale, this->mColor);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DrawTextWidget::Clone() const
{
	DrawTextWidget* pWidget =
		tf_placement_new<DrawTextWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos, this->mColor);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DrawTooltipWidget::Clone() const
{
	DrawTooltipWidget* pWidget =
		tf_placement_new<DrawTooltipWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mShowTooltip, this->mText);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DrawLineWidget::Clone() const
{
	DrawLineWidget* pWidget =
		tf_placement_new<DrawLineWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos1, this->mPos2, this->mColor, this->mAddItem);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* DrawCurveWidget::Clone() const
{
	DrawCurveWidget* pWidget =
		tf_placement_new<DrawCurveWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPos, this->mNumPoints, this->mThickness, this->mColor);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* CheckboxWidget::Clone() const
{
	CheckboxWidget* pWidget = tf_placement_new<CheckboxWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* OneLineCheckboxWidget::Clone() const
{
	OneLineCheckboxWidget* pWidget = tf_placement_new<OneLineCheckboxWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->pData, this->mColor);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* CursorLocationWidget::Clone() const
{
	CursorLocationWidget* pWidget = tf_placement_new<CursorLocationWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mLocation);

	CloneBase(pWidget);

	return pWidget;
}

IWidget* ColumnWidget::Clone() const
{
	ColumnWidget* pWidget = tf_placement_new<ColumnWidget>(tf_calloc(1, sizeof(*pWidget)), this->mLabel, this->mPerColumnWidgets);

	CloneBase(pWidget);

	return pWidget;
}
/************************************************************************/
// UI Implementation
/************************************************************************/

UIApp::UIApp(int32_t const fontAtlasSize, uint32_t const maxDynamicUIUpdatesPerBatch, uint32_t fontstashRingSizeBytes)
{
	mFontAtlasSize = fontAtlasSize;
	mMaxDynamicUIUpdatesPerBatch = maxDynamicUIUpdatesPerBatch;
	mFontstashRingSizeBytes = fontstashRingSizeBytes;
}

bool UIApp::Init(Renderer* renderer, PipelineCache* pCache)
{
	mShowDemoUiWindow = false;

	pImpl = tf_new(UIAppImpl);
	pImpl->pRenderer = renderer;

	pDriver = NULL;
	pPipelineCache = pCache;

	// Initialize the fontstash
	//
	// To support more characters and different font configurations
	// the app will need more memory for the fontstash atlas.
	//
	if (mFontAtlasSize <= 0) // then we assume we'll only draw debug text in the UI, in which case the atlas size can be kept small
		mFontAtlasSize = 256;

	pImpl->pFontStash = tf_new(Fontstash);
	bool success = pImpl->pFontStash->init(renderer, mFontAtlasSize, mFontAtlasSize, mFontstashRingSizeBytes);

	initGUIDriver(pImpl->pRenderer, &pDriver);
	if (pCustomShader)
		pDriver->setCustomShader(pCustomShader);
	success &= pDriver->init(pImpl->pRenderer, mMaxDynamicUIUpdatesPerBatch);

	return success;
}

void UIApp::Exit()
{
	RemoveAllGuiComponents();

	pImpl->pFontStash->exit();
	tf_delete(pImpl->pFontStash);

	pDriver->exit();
	removeGUIDriver(pDriver);
	pDriver = NULL;

	tf_delete(pImpl);
}

bool UIApp::Load(RenderTarget** rts, uint32_t count)
{
	ASSERT(rts && rts[0]);
	mWidth = (float)rts[0]->mWidth;
	mHeight = (float)rts[0]->mHeight;

	bool success = pDriver->load(rts, count, pPipelineCache);
	success &= pImpl->pFontStash->load(rts, count, pPipelineCache);

	return success;
}

void UIApp::Unload()
{
	pDriver->unload();
	pImpl->pFontStash->unload();
}

uint32_t UIApp::LoadFont(const char* pFontPath)
{
	uint32_t fontID = (uint32_t)pImpl->pFontStash->defineFont("default", pFontPath);
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

#if defined(__linux__) || defined(NX64)
#define sprintf_s sprintf    // On linux and NX, we should use sprintf as sprintf_s is not part of the standard c library
#endif

GuiComponent* UIApp::AddGuiComponent(const char* pTitle, const GuiDesc* pDesc)
{
	GuiComponent* pComponent = tf_placement_new<GuiComponent>(tf_calloc(1, sizeof(GuiComponent)));
	pComponent->mHasCloseButton = false;
	pComponent->mFlags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;
#if defined(TARGET_IOS) || defined(__ANDROID__)
	pComponent->mFlags |= GUI_COMPONENT_FLAGS_START_COLLAPSED;
#endif

	void* pFontBuffer = pImpl->pFontStash->getFontBuffer(pDesc->mDefaultTextDrawDesc.mFontID);
	uint32_t fontBufferSize = pImpl->pFontStash->getFontBufferSize(pDesc->mDefaultTextDrawDesc.mFontID);
	if (pFontBuffer)
		pDriver->addFont(pFontBuffer, fontBufferSize, NULL, pDesc->mDefaultTextDrawDesc.mFontSize, &pComponent->pFont);

	pComponent->mInitialWindowRect = { pDesc->mStartPosition.getX(), pDesc->mStartPosition.getY(), pDesc->mStartSize.getX(),
										 pDesc->mStartSize.getY() };

	pComponent->mActive = true;
	pComponent->mTitle = pTitle;
	pComponent->mAlpha = 1.0f;
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
	tf_free(pComponent);
}

void UIApp::RemoveAllGuiComponents()
{
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponents.size(); ++i)
	{
		pImpl->mComponents[i]->RemoveAllWidgets();
		pImpl->mComponents[i]->~GuiComponent();
		tf_free(pImpl->mComponents[i]);
	}

	pImpl->mComponents.clear();
	pImpl->mComponentsToUpdate.clear();
}

void UIApp::Update(float deltaTime)
{
	if (pImpl->mUpdated || !pImpl->mComponentsToUpdate.size())
		return;

	pImpl->mUpdated = true;

	eastl::vector<GuiComponent*> activeComponents(pImpl->mComponentsToUpdate.size());
	uint32_t                       activeComponentCount = 0;
	for (uint32_t i = 0; i < (uint32_t)pImpl->mComponentsToUpdate.size(); ++i)
		if (pImpl->mComponentsToUpdate[i]->mActive)
			activeComponents[activeComponentCount++] = pImpl->mComponentsToUpdate[i];

	GUIDriver::GUIUpdate guiUpdate{ activeComponents.data(), activeComponentCount, deltaTime, mWidth, mHeight, mShowDemoUiWindow };
	pDriver->update(&guiUpdate);

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
			tf_free(pWidget);
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
			tf_free(mWidgets[i]);
		}
	}

	mWidgets.clear();
	mWidgetsClone.clear();
}

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#define TOUCH_INPUT 1
#endif

/************************************************************************/
/************************************************************************/
bool VirtualJoystickUI::Init(Renderer* renderer, const char* pJoystickTexture)
{
#if TOUCH_INPUT
	pRenderer = renderer;

	TextureLoadDesc loadDesc = {};
	SyncToken token = {};
	loadDesc.pFileName = pJoystickTexture;
	loadDesc.ppTexture = &pTexture;
	addResource(&loadDesc, &token);
	waitForToken(&token);

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
	/************************************************************************/
	// Shader
	/************************************************************************/
	ShaderLoadDesc texturedShaderDesc = {};
	texturedShaderDesc.mStages[0] = { "textured_mesh.vert", NULL, 0 };
	texturedShaderDesc.mStages[1] = { "textured_mesh.frag", NULL, 0 };
	addShader(pRenderer, &texturedShaderDesc, &pShader);

	const char*       pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pShader, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignature);

	DescriptorSetDesc descriptorSetDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
	addDescriptorSet(pRenderer, &descriptorSetDesc, &pDescriptorSet);
	/************************************************************************/
	// Resources
	/************************************************************************/
	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.ppBuffer = &pMeshBuffer;
	addResource(&vbDesc, NULL);
	/************************************************************************/
	// Prepare descriptor sets
	/************************************************************************/
	DescriptorData params[1] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pTexture;
	updateDescriptorSet(pRenderer, 0, pDescriptorSet, 1, params);
#endif
	return true;
}

void VirtualJoystickUI::Exit()
{
#if TOUCH_INPUT
	removeSampler(pRenderer, pSampler);
	removeResource(pMeshBuffer);
	removeDescriptorSet(pRenderer, pDescriptorSet);
	removeRootSignature(pRenderer, pRootSignature);
	removeShader(pRenderer, pShader);
	removeResource(pTexture);
#endif
}

bool VirtualJoystickUI::Load(RenderTarget* pScreenRT)
{
#if TOUCH_INPUT
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
	vertexLayout.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(TinyImageFormat_R32G32_SFLOAT) / 8;

	BlendStateDesc blendStateDesc = {};
	blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
	blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
	blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
	blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
	blendStateDesc.mMasks[0] = ALL;
	blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
	blendStateDesc.mIndependentBlend = false;

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;

	RasterizerStateDesc rasterizerStateDesc = {};
	rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
	rasterizerStateDesc.mScissor = true;

	PipelineDesc desc = {};
	desc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
	pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
	pipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
	pipelineDesc.mRenderTargetCount = 1;
	pipelineDesc.mSampleCount = pScreenRT->mSampleCount;
	pipelineDesc.mSampleQuality = pScreenRT->mSampleQuality;
	pipelineDesc.pBlendState = &blendStateDesc;
	pipelineDesc.pColorFormats = &pScreenRT->mFormat;
	pipelineDesc.pDepthState = &depthStateDesc;
	pipelineDesc.pRasterizerState = &rasterizerStateDesc;
	pipelineDesc.pRootSignature = pRootSignature;
	pipelineDesc.pShaderProgram = pShader;
	pipelineDesc.pVertexLayout = &vertexLayout;
	addPipeline(pRenderer, &desc, &pPipeline);

	mRenderSize[0] = (float)pScreenRT->mWidth;
	mRenderSize[1] = (float)pScreenRT->mHeight;
#endif
	return true;
}

void VirtualJoystickUI::Unload()
{
#if TOUCH_INPUT
	removePipeline(pRenderer, pPipeline);
#endif
}

void VirtualJoystickUI::Update(float dt)
{
}

bool VirtualJoystickUI::OnMove(uint32_t id, bool press, const float2* vec)
{
#if TOUCH_INPUT
	if (!vec) return false;


	if (!mSticks[id].mPressed)
	{
		mSticks[id].mStartPos = *vec;
		mSticks[id].mCurrPos = *vec;
	}
	else
	{
		mSticks[id].mCurrPos = *vec;
	}
	mSticks[id].mPressed = press;
	return true;
#else
	return false;
#endif
}

void VirtualJoystickUI::Draw(Cmd* pCmd, const float4& color)
{
#if TOUCH_INPUT
	if (!(mSticks[0].mPressed || mSticks[1].mPressed))
		return;

	struct RootConstants
	{
		float4 color;
		float2 scaleBias;
	} data = {};

	cmdBindPipeline(pCmd, pPipeline);
	cmdBindDescriptorSet(pCmd, 0, pDescriptorSet);
	data.color = color;
	data.scaleBias = { 2.0f / (float)mRenderSize[0], -2.0f / (float)mRenderSize[1] };
	cmdBindPushConstants(pCmd, pRootSignature, "uRootConstants", &data);

	// Draw the camera controller's virtual joysticks.
	float extSide = mOutsideRadius;
	float intSide = mInsideRadius;

	uint64_t bufferOffset = 0;
	for (uint i = 0; i < 2; i++)
	{
		if (mSticks[i].mPressed)
		{
			float2 joystickSize = float2(extSide);
			float2 joystickCenter = mSticks[i].mStartPos - float2(0.0f, mRenderSize.y * 0.1f);
			float2 joystickPos = joystickCenter - joystickSize * 0.5f;

			const uint32_t vertexStride = sizeof(float4);
			BufferUpdateDesc updateDesc = { pMeshBuffer, bufferOffset };
			beginUpdateResource(&updateDesc);
			TexVertex* vertices = (TexVertex*)updateDesc.pMappedData;
			// the last variable can be used to create a border
			MAKETEXQUAD(vertices, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
			endUpdateResource(&updateDesc, NULL);
			cmdBindVertexBuffer(pCmd, 1, &pMeshBuffer, &vertexStride, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(TexVertex) * 4;

			joystickSize = float2(intSide);
			joystickCenter = mSticks[i].mCurrPos - float2(0.0f, mRenderSize.y * 0.1f);
			joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			updateDesc = { pMeshBuffer, bufferOffset };
			beginUpdateResource(&updateDesc);
			TexVertex* verticesInner = (TexVertex*)updateDesc.pMappedData;
			// the last variable can be used to create a border
			MAKETEXQUAD(verticesInner, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
			endUpdateResource(&updateDesc, NULL);
			cmdBindVertexBuffer(pCmd, 1, &pMeshBuffer, &vertexStride, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(TexVertex) * 4;
		}
	}
#endif
}
