/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../Interfaces/IUI.h"

#include "../Interfaces/ILog.h"
#include "../Interfaces/IFont.h"
#include "../Interfaces/IInput.h"
#include "../Interfaces/IFileSystem.h"
#include "../../Renderer/IResourceLoader.h"

// TODO(Alex): Delete this? Seems these files no longer exist.
#ifdef ENABLE_UI_PRECOMPILED_SHADERS
#include "Shaders/Compiled/imgui.vert.h"
#include "Shaders/Compiled/imgui.frag.h"
#endif

#include "../../ThirdParty/OpenSource/imgui/imgui.h"
#include "../../ThirdParty/OpenSource/imgui/imgui_internal.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../Interfaces/IMemory.h"

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#define TOUCH_INPUT 1
#endif

#define LABELID(prop, buffer) sprintf(buffer, "##%llu", (unsigned long long)(prop.pData))
#define LABELID1(prop, buffer) sprintf(buffer, "##%llu", (unsigned long long)(prop))

#define MAX_LABEL_LENGTH 128

#define MAX_LUA_STR_LEN 256
static const uint32_t MAX_FRAMES = 3;

struct GUIDriverUpdate
{
	UIComponent** pUIComponents = NULL;
	uint32_t		componentCount = 0;
	float			deltaTime = 0.f;
	float			width = 0.f;
	float			height = 0.f;
	bool			showDemoWindow = false;
};

struct UIAppImpl
{
	Renderer*						pRenderer = NULL;
	eastl::vector<UIComponent*>    mComponents;
	bool                            mUpdated = false;
};

typedef struct UserInterface
{
	float						mWidth = 0.f;
	float						mHeight = 0.f;
	uint32_t					mMaxDynamicUIUpdatesPerBatch = 20;

	// Following var is useful for seeing UI capabilities and tweaking style settings.
	// Will only take effect if at least one GUI Component is active.
	bool mShowDemoUiWindow = false;

	Renderer*						pRenderer = NULL;
	eastl::vector<UIComponent*>    mComponents;
	bool                            mUpdated = false;

	PipelineCache*		pPipelineCache = NULL;
	ImGuiContext*           context = NULL;
	eastl::vector<Texture*>  mFontTextures;
	float                   dpiScale[2] = { 0.0f };
	uint32_t                frameIdx = 0;

	Shader*            pShaderTextured = NULL;
	RootSignature*     pRootSignatureTextured = NULL;
	DescriptorSet*     pDescriptorSetUniforms = NULL;
	DescriptorSet*     pDescriptorSetTexture = NULL;
	Pipeline*          pPipelineTextured = NULL;
	Buffer*            pVertexBuffer = NULL;
	Buffer*            pIndexBuffer = NULL;
	Buffer*            pUniformBuffer[MAX_FRAMES] = { NULL };
	/// Default states
	Sampler*         pDefaultSampler = NULL;
	VertexLayout     mVertexLayoutTextured = {};
	uint32_t         mDynamicUIUpdates = 0;
	float            mNavInputs[ImGuiNavInput_COUNT] = { 0.0f };
	const float2*    pMovePosition = NULL;
	uint32_t         mLastUpdateCount = 0;
	float2           mLastUpdateMin[64] = {};
	float2           mLastUpdateMax[64] = {};
	bool             mActive = false;
	bool             mPostUpdateKeyDownStates[512] = { false };

	// Since gestures events always come first, we want to dismiss any other inputs after that
	bool mHandledGestures = false;

} UserInterface;

#ifdef ENABLE_FORGE_UI
static UserInterface* pUserInterface = NULL;

namespace ImGui {
	bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format)
	{
		char text_buf[30];
		bool          value_changed = false;

		if (!display_format)
			display_format = "%.1f";
		sprintf(text_buf, display_format, *v);

		if (ImGui::GetIO().WantTextInput)
		{
			value_changed = ImGui::SliderFloat(label, v, v_min, v_max, text_buf);

			int v_i = int(((*v - v_min) / v_step) + 0.5f);
			*v = v_min + float(v_i) * v_step;
		}
		else
		{
			// Map from [v_min,v_max] to [0,N]
			const int countValues = int((v_max - v_min) / v_step);
			int       v_i = int(((*v - v_min) / v_step) + 0.5f);
			value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

			// Remap from [0,N] to [v_min,v_max]
			*v = v_min + float(v_i) * v_step;
		}

		if (*v < v_min)
			*v = v_min;
		if (*v > v_max)
			*v = v_max;

		return value_changed;
	}

	bool SliderIntWithSteps(const char* label, int32_t* v, int32_t v_min, int32_t v_max, int32_t v_step, const char* display_format)
	{
		char text_buf[30];
		bool          value_changed = false;

		if (!display_format)
			display_format = "%d";
		sprintf(text_buf, display_format, *v);

		if (ImGui::GetIO().WantTextInput)
		{
			value_changed = ImGui::SliderInt(label, v, v_min, v_max, text_buf);

			int32_t v_i = int((*v - v_min) / v_step);
			*v = v_min + int32_t(v_i) * v_step;
		}
		else
		{
			// Map from [v_min,v_max] to [0,N]
			const int countValues = int((v_max - v_min) / v_step);
			int32_t   v_i = int((*v - v_min) / v_step);
			value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf);

			// Remap from [0,N] to [v_min,v_max]
			*v = v_min + int32_t(v_i) * v_step;
		}

		if (*v < v_min)
			*v = v_min;
		if (*v > v_max)
			*v = v_max;

		return value_changed;
	}
}    // namespace ImGui

/****************************************************************************/
// MARK: - Static Function Declarations
/****************************************************************************/

static void* alloc_func(size_t size, void* user_data) 
{ 
	return tf_malloc(size); 
}

static void dealloc_func(void* ptr, void* user_data) 
{ 
	tf_free(ptr); 
}

static void SetDefaultStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.4f;
	float4* colors = style.Colors;
	colors[ImGuiCol_Text] = float4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = float4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = float4(0.06f, 0.06f, 0.06f, 1.0f);
	colors[ImGuiCol_ChildBg] = float4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = float4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = float4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = float4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = float4(0.20f, 0.21f, 0.22f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = float4(0.40f, 0.40f, 0.40f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = float4(0.18f, 0.18f, 0.18f, 0.67f);
	colors[ImGuiCol_TitleBg] = float4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = float4(0.29f, 0.29f, 0.29f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = float4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = float4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = float4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = float4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = float4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = float4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = float4(0.94f, 0.94f, 0.94f, 1.00f);
	colors[ImGuiCol_SliderGrab] = float4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = float4(0.86f, 0.86f, 0.86f, 1.00f);
	colors[ImGuiCol_Button] = float4(0.44f, 0.44f, 0.44f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = float4(0.46f, 0.47f, 0.48f, 1.00f);
	colors[ImGuiCol_ButtonActive] = float4(0.42f, 0.42f, 0.42f, 1.00f);
	colors[ImGuiCol_Header] = float4(0.70f, 0.70f, 0.70f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = float4(0.70f, 0.70f, 0.70f, 0.80f);
	colors[ImGuiCol_HeaderActive] = float4(0.48f, 0.50f, 0.52f, 1.00f);
	colors[ImGuiCol_Separator] = float4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = float4(0.72f, 0.72f, 0.72f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = float4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = float4(0.91f, 0.91f, 0.91f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = float4(0.81f, 0.81f, 0.81f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = float4(0.46f, 0.46f, 0.46f, 0.95f);
	colors[ImGuiCol_PlotLines] = float4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = float4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = float4(0.73f, 0.60f, 0.15f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = float4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = float4(0.87f, 0.87f, 0.87f, 0.35f);
	colors[ImGuiCol_ModalWindowDarkening] = float4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = float4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = float4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = float4(1.00f, 1.00f, 1.00f, 0.70f);

	float dpiScale[2];
	getDpiScale(dpiScale);
	style.ScaleAllSizes(min(dpiScale[0], dpiScale[1]));
}

/****************************************************************************/
// MARK: - Non-static Function Forward Declarations
/****************************************************************************/

// UIWidget functions
UIWidget* cloneWidget(const UIWidget* pWidget);
void processWidgetCallbacks(UIWidget* pWidget, bool deferred = false);
void drawWidget(UIWidget* pWidget);
void destroyWidget(UIWidget* pWidget, bool freeUnderlying);

/****************************************************************************/
// MARK: - Non-static Function Definitions
/****************************************************************************/

bool addImguiFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont)
{
	ImGuiIO& io = ImGui::GetIO();
	// Build and load the texture atlas into a texture
	int            width, height, bytesPerPixel;
	unsigned char* pixels = NULL;

	if (pFontBuffer == NULL)
	{
		*pFont = (uintptr_t)io.Fonts->AddFontDefault();
	}
	else
	{
		ImFontConfig   config = {};
		config.FontDataOwnedByAtlas = false;
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize,
			fontSize * min(pUserInterface->dpiScale[0], pUserInterface->dpiScale[1]), &config,
			(const ImWchar*)pFontGlyphRanges);
		if (font != NULL)
		{
			io.FontDefault = font;
			*pFont = (uintptr_t)font;
		}
		else
		{
			*pFont = (uintptr_t)io.Fonts->AddFontDefault();
		}
	}

	io.Fonts->Build();
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

	// At this point you've got the texture data and you need to upload that your your graphic system:
	// After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
	// This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
	Texture* pTexture = NULL;
	SyncToken token = {};
	TextureLoadDesc loadDesc = {};
	TextureDesc textureDesc = {};
	textureDesc.mArraySize = 1;
	textureDesc.mDepth = 1;
	textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	textureDesc.mHeight = height;
	textureDesc.mMipLevels = 1;
	textureDesc.mSampleCount = SAMPLE_COUNT_1;
	textureDesc.mStartState = RESOURCE_STATE_COMMON;
	textureDesc.mWidth = width;
	textureDesc.pName = "ImGui Font Texture";
	loadDesc.pDesc = &textureDesc;
	loadDesc.ppTexture = &pTexture;
	addResource(&loadDesc, &token);
	waitForToken(&token);

	TextureUpdateDesc updateDesc = { pTexture };
	beginUpdateResource(&updateDesc);
	for (uint32_t r = 0; r < updateDesc.mRowCount; ++r)
	{
		memcpy(updateDesc.pMappedData + r * updateDesc.mDstRowStride,
			pixels + r * updateDesc.mSrcRowStride, updateDesc.mSrcRowStride);
	}
	endUpdateResource(&updateDesc, &token);

	pUserInterface->mFontTextures.emplace_back(pTexture);
	io.Fonts->TexID = (void*)(pUserInterface->mFontTextures.size() - 1);

	DescriptorData params[1] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pTexture;
	updateDescriptorSet(pUserInterface->pRenderer, (uint32_t)pUserInterface->mFontTextures.size() - 1, pUserInterface->pDescriptorSetTexture, 1, params);

	return true;
}

/****************************************************************************/
// MARK: - Static Value Definitions
/****************************************************************************/

static const uint64_t VERTEX_BUFFER_SIZE = 1024 * 64 * sizeof(ImDrawVert);
static const uint64_t INDEX_BUFFER_SIZE = 128 * 1024 * sizeof(ImDrawIdx);

/****************************************************************************/
// MARK: - Base UIWidget Helper Functions
/****************************************************************************/

// CollapsingHeaderWidget private functions
CollapsingHeaderWidget* cloneCollapsingHeaderWidget(const void* pWidget)
{
	const CollapsingHeaderWidget* pOriginalWidget = (const CollapsingHeaderWidget*)pWidget;
	CollapsingHeaderWidget* pClonedWidget = (CollapsingHeaderWidget*)tf_calloc(1, sizeof(CollapsingHeaderWidget));

	pClonedWidget->mCollapsed = pOriginalWidget->mCollapsed;
	pClonedWidget->mDefaultOpen = pOriginalWidget->mDefaultOpen;
	pClonedWidget->mHeaderIsVisible = pOriginalWidget->mHeaderIsVisible;
	pClonedWidget->mGroupedWidgets = pOriginalWidget->mGroupedWidgets;

	return pClonedWidget;
}

// DebugTexturesWidget private functions
DebugTexturesWidget* cloneDebugTexturesWidget(const void* pWidget)
{
	const DebugTexturesWidget* pOriginalWidget = (const DebugTexturesWidget*)pWidget;
	DebugTexturesWidget* pClonedWidget = (DebugTexturesWidget*)tf_calloc(1, sizeof(DebugTexturesWidget));

	pClonedWidget->mTextureDisplaySize = pOriginalWidget->mTextureDisplaySize;
	pClonedWidget->mTextures = pOriginalWidget->mTextures;

	return pClonedWidget;
}

// LabelWidget private functions
LabelWidget* cloneLabelWidget(const void* pWidget)
{
	LabelWidget* pClonedWidget = (LabelWidget*)tf_calloc(1, sizeof(LabelWidget));

	return pClonedWidget;
}

// ColorLabelWidget private functions
ColorLabelWidget* cloneColorLabelWidget(const void* pWidget)
{
	const ColorLabelWidget* pOriginalWidget = (const ColorLabelWidget*)pWidget;
	ColorLabelWidget* pClonedWidget = (ColorLabelWidget*)tf_calloc(1, sizeof(ColorLabelWidget));

	pClonedWidget->mColor = pOriginalWidget->mColor;

	return pClonedWidget;
}

// HorizontalSpaceWidget private functions
HorizontalSpaceWidget* cloneHorizontalSpaceWidget(const void* pWidget)
{
	HorizontalSpaceWidget* pClonedWidget = (HorizontalSpaceWidget*)tf_calloc(1, sizeof(HorizontalSpaceWidget));

	return pClonedWidget;
}

// SeparatorWidget private functions
SeparatorWidget* cloneSeparatorWidget(const void* pWidget)
{
	SeparatorWidget* pClonedWidget = (SeparatorWidget*)tf_calloc(1, sizeof(SeparatorWidget));

	return pClonedWidget;
}

// VerticalSeparatorWidget private functions
VerticalSeparatorWidget* cloneVerticalSeparatorWidget(const void* pWidget)
{
	const VerticalSeparatorWidget* pOriginalWidget = (const VerticalSeparatorWidget*)pWidget;
	VerticalSeparatorWidget* pClonedWidget = (VerticalSeparatorWidget*)tf_calloc(1, sizeof(VerticalSeparatorWidget));

	pClonedWidget->mLineCount = pOriginalWidget->mLineCount;

	return pClonedWidget;
}

// ButtonWidget private functions
ButtonWidget* cloneButtonWidget(const void* pWidget)
{
	ButtonWidget* pClonedWidget = (ButtonWidget*)tf_calloc(1, sizeof(ButtonWidget));

	return pClonedWidget;
}

// SliderFloatWidget private functions
SliderFloatWidget* cloneSliderFloatWidget(const void* pWidget)
{
	const SliderFloatWidget* pOriginalWidget = (const SliderFloatWidget*)pWidget;
	SliderFloatWidget* pClonedWidget = (SliderFloatWidget*)tf_calloc(1, sizeof(SliderFloatWidget));

	memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMin = pOriginalWidget->mMin;
	pClonedWidget->mMax = pOriginalWidget->mMax;
	pClonedWidget->mStep = pOriginalWidget->mStep;

	return pClonedWidget;
}

// SliderFloat2Widget private functions
SliderFloat2Widget* cloneSliderFloat2Widget(const void* pWidget)
{
	const SliderFloat2Widget* pOriginalWidget = (const SliderFloat2Widget*)pWidget;
	SliderFloat2Widget* pClonedWidget = (SliderFloat2Widget*)tf_calloc(1, sizeof(SliderFloat2Widget));

	memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMin = pOriginalWidget->mMin;
	pClonedWidget->mMax = pOriginalWidget->mMax;
	pClonedWidget->mStep = pOriginalWidget->mStep;

	return pClonedWidget;
}

// SliderFloat3Widget private functions
SliderFloat3Widget* cloneSliderFloat3Widget(const void* pWidget)
{
	const SliderFloat3Widget* pOriginalWidget = (const SliderFloat3Widget*)pWidget;
	SliderFloat3Widget* pClonedWidget = (SliderFloat3Widget*)tf_calloc(1, sizeof(SliderFloat3Widget));

	memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMin = pOriginalWidget->mMin;
	pClonedWidget->mMax = pOriginalWidget->mMax;
	pClonedWidget->mStep = pOriginalWidget->mStep;

	return pClonedWidget;
}

// SliderFloat4Widget private functions
SliderFloat4Widget* cloneSliderFloat4Widget(const void* pWidget)
{
	const SliderFloat4Widget* pOriginalWidget = (const SliderFloat4Widget*)pWidget;
	SliderFloat4Widget* pClonedWidget = (SliderFloat4Widget*)tf_calloc(1, sizeof(SliderFloat4Widget));

	memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMin = pOriginalWidget->mMin;
	pClonedWidget->mMax = pOriginalWidget->mMax;
	pClonedWidget->mStep = pOriginalWidget->mStep;

	return pClonedWidget;
}

// SliderIntWidget private functions
SliderIntWidget* cloneSliderIntWidget(const void* pWidget)
{
	const SliderIntWidget* pOriginalWidget = (const SliderIntWidget*)pWidget;
	SliderIntWidget* pClonedWidget = (SliderIntWidget*)tf_calloc(1, sizeof(SliderIntWidget));

	memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMin = pOriginalWidget->mMin;
	pClonedWidget->mMax = pOriginalWidget->mMax;
	pClonedWidget->mStep = pOriginalWidget->mStep;

	return pClonedWidget;
}

// SliderUintWidget private functions
SliderUintWidget* cloneSliderUintWidget(const void* pWidget)
{
	const SliderUintWidget* pOriginalWidget = (const SliderUintWidget*)pWidget;
	SliderUintWidget* pClonedWidget = (SliderUintWidget*)tf_calloc(1, sizeof(SliderUintWidget));

	memset(pClonedWidget->mFormat, 0, MAX_FORMAT_STR_LENGTH);
	strcpy(pClonedWidget->mFormat, pOriginalWidget->mFormat);

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMin = pOriginalWidget->mMin;
	pClonedWidget->mMax = pOriginalWidget->mMax;
	pClonedWidget->mStep = pOriginalWidget->mStep;

	return pClonedWidget;
}

// RadioButtonWidget private functions
RadioButtonWidget* cloneRadioButtonWidget(const void* pWidget)
{
	const RadioButtonWidget* pOriginalWidget = (const RadioButtonWidget*)pWidget;
	RadioButtonWidget* pClonedWidget = (RadioButtonWidget*)tf_calloc(1, sizeof(RadioButtonWidget));

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mRadioId = pOriginalWidget->mRadioId;

	return pClonedWidget;
}

// CheckboxWidget private functions
CheckboxWidget* cloneCheckboxWidget(const void* pWidget)
{
	const CheckboxWidget* pOriginalWidget = (const CheckboxWidget*)pWidget;
	CheckboxWidget* pClonedWidget = (CheckboxWidget*)tf_calloc(1, sizeof(CheckboxWidget));

	pClonedWidget->pData = pOriginalWidget->pData;

	return pClonedWidget;
}

// OneLineCheckboxWidget private functions
OneLineCheckboxWidget* cloneOneLineCheckboxWidget(const void* pWidget)
{
	const OneLineCheckboxWidget* pOriginalWidget = (const OneLineCheckboxWidget*)pWidget;
	OneLineCheckboxWidget* pClonedWidget = (OneLineCheckboxWidget*)tf_calloc(1, sizeof(OneLineCheckboxWidget));

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mColor = pOriginalWidget->mColor;

	return pClonedWidget;
}

// CursorLocationWidget private functions
CursorLocationWidget* cloneCursorLocationWidget(const void* pWidget)
{
	const CursorLocationWidget* pOriginalWidget = (const CursorLocationWidget*)pWidget;
	CursorLocationWidget* pClonedWidget = (CursorLocationWidget*)tf_calloc(1, sizeof(CursorLocationWidget));

	pClonedWidget->mLocation = pOriginalWidget->mLocation;

	return pClonedWidget;
}

// DropdownWidget private functions
DropdownWidget* cloneDropdownWidget(const void* pWidget)
{
	const DropdownWidget* pOriginalWidget = (const DropdownWidget*)pWidget;
	DropdownWidget* pClonedWidget = (DropdownWidget*)tf_calloc(1, sizeof(DropdownWidget));

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mValues = pOriginalWidget->mValues;
	pClonedWidget->mNames.resize(pOriginalWidget->mNames.size());

	for (uint32_t i = 0; i < pOriginalWidget->mNames.size(); ++i)
	{
		pClonedWidget->mNames[i] = (char*)tf_calloc(strlen(pOriginalWidget->mNames[i]) + 1, sizeof(char));
		strcpy(pClonedWidget->mNames[i], pOriginalWidget->mNames[i]);
	}

	return pClonedWidget;
}

// ColumnWidget private functions
ColumnWidget* cloneColumnWidget(const void* pWidget)
{
	const ColumnWidget* pOriginalWidget = (const ColumnWidget*)pWidget;
	ColumnWidget* pClonedWidget = (ColumnWidget*)tf_calloc(1, sizeof(ColumnWidget));

	pClonedWidget->mPerColumnWidgets = pOriginalWidget->mPerColumnWidgets;
	pClonedWidget->mNumColumns = pOriginalWidget->mNumColumns;

	return pClonedWidget;
}

// ProgressBarWidget private functions
ProgressBarWidget* cloneProgressBarWidget(const void* pWidget)
{
	const ProgressBarWidget* pOriginalWidget = (const ProgressBarWidget*)pWidget;
	ProgressBarWidget* pClonedWidget = (ProgressBarWidget*)tf_calloc(1, sizeof(ProgressBarWidget));

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mMaxProgress = pOriginalWidget->mMaxProgress;

	return pClonedWidget;
}

// ColorSliderWidget private functions
ColorSliderWidget* cloneColorSliderWidget(const void* pWidget)
{
	const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)pWidget;
	ColorSliderWidget* pClonedWidget = (ColorSliderWidget*)tf_calloc(1, sizeof(ColorSliderWidget));

	pClonedWidget->pData = pOriginalWidget->pData;

	return pClonedWidget;
}

// HistogramWidget private functions
HistogramWidget* cloneHistogramWidget(const void* pWidget)
{
	const HistogramWidget* pOriginalWidget = (const HistogramWidget*)pWidget;
	HistogramWidget* pClonedWidget = (HistogramWidget*)tf_calloc(1, sizeof(HistogramWidget));

	pClonedWidget->pValues = pOriginalWidget->pValues;
	pClonedWidget->mCount = pOriginalWidget->mCount;
	pClonedWidget->mMinScale = pOriginalWidget->mMinScale;
	pClonedWidget->mMaxScale = pOriginalWidget->mMaxScale;
	pClonedWidget->mHistogramSize = pOriginalWidget->mHistogramSize;
	pClonedWidget->mHistogramTitle = pOriginalWidget->mHistogramTitle;

	return pClonedWidget;
}

// PlotLinesWidget private functions
PlotLinesWidget* clonePlotLinesWidget(const void* pWidget)
{
	const PlotLinesWidget* pOriginalWidget = (const PlotLinesWidget*)pWidget;
	PlotLinesWidget* pClonedWidget = (PlotLinesWidget*)tf_calloc(1, sizeof(PlotLinesWidget));

	pClonedWidget->mValues = pOriginalWidget->mValues;
	pClonedWidget->mNumValues = pOriginalWidget->mNumValues;
	pClonedWidget->mScaleMin = pOriginalWidget->mScaleMin;
	pClonedWidget->mScaleMax = pOriginalWidget->mScaleMax;
	pClonedWidget->mPlotScale = pOriginalWidget->mPlotScale;
	pClonedWidget->mTitle = pOriginalWidget->mTitle;

	return pClonedWidget;
}

// ColorPickerWidget private functions
ColorPickerWidget* cloneColorPickerWidget(const void* pWidget)
{
	const ColorPickerWidget* pOriginalWidget = (const ColorPickerWidget*)pWidget;
	ColorPickerWidget* pClonedWidget = (ColorPickerWidget*)tf_calloc(1, sizeof(ColorPickerWidget));

	pClonedWidget->pData = pOriginalWidget->pData;

	return pClonedWidget;
}

// TextboxWidget private functions
TextboxWidget* cloneTextboxWidget(const void* pWidget)
{
	const TextboxWidget* pOriginalWidget = (const TextboxWidget*)pWidget;
	TextboxWidget* pClonedWidget = (TextboxWidget*)tf_calloc(1, sizeof(TextboxWidget));

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mLength = pOriginalWidget->mLength;
	pClonedWidget->mAutoSelectAll = pOriginalWidget->mAutoSelectAll;

	return pClonedWidget;
}

// DynamicTextWidget private functions
DynamicTextWidget* cloneDynamicTextWidget(const void* pWidget)
{
	const DynamicTextWidget* pOriginalWidget = (const DynamicTextWidget*)pWidget;
	DynamicTextWidget* pClonedWidget = (DynamicTextWidget*)tf_calloc(1, sizeof(DynamicTextWidget));

	pClonedWidget->pData = pOriginalWidget->pData;
	pClonedWidget->mLength = pOriginalWidget->mLength;
	pClonedWidget->pColor = pOriginalWidget->pColor;

	return pClonedWidget;
}

// FilledRectWidget private functions
FilledRectWidget* cloneFilledRectWidget(const void* pWidget)
{
	const FilledRectWidget* pOriginalWidget = (const FilledRectWidget*)pWidget;
	FilledRectWidget* pClonedWidget = (FilledRectWidget*)tf_calloc(1, sizeof(FilledRectWidget));

	pClonedWidget->mPos = pOriginalWidget->mPos;
	pClonedWidget->mScale = pOriginalWidget->mScale;
	pClonedWidget->mColor = pOriginalWidget->mColor;

	return pClonedWidget;
}

// DrawTextWidget private functions
DrawTextWidget* cloneDrawTextWidget(const void* pWidget)
{
	const DrawTextWidget* pOriginalWidget = (const DrawTextWidget*)pWidget;
	DrawTextWidget* pClonedWidget = (DrawTextWidget*)tf_calloc(1, sizeof(DrawTextWidget));

	pClonedWidget->mPos = pOriginalWidget->mPos;
	pClonedWidget->mColor = pOriginalWidget->mColor;

	return pClonedWidget;
}

// DrawTooltipWidget private functions
DrawTooltipWidget* cloneDrawTooltipWidget(const void* pWidget)
{
	const DrawTooltipWidget* pOriginalWidget = (const DrawTooltipWidget*)pWidget;
	DrawTooltipWidget* pClonedWidget = (DrawTooltipWidget*)tf_calloc(1, sizeof(DrawTooltipWidget));

	pClonedWidget->mShowTooltip = pOriginalWidget->mShowTooltip;
	pClonedWidget->mText = pOriginalWidget->mText;

	return pClonedWidget;
}

// DrawLineWidget private functions
DrawLineWidget* cloneDrawLineWidget(const void* pWidget)
{
	const DrawLineWidget* pOriginalWidget = (const DrawLineWidget*)pWidget;
	DrawLineWidget* pClonedWidget = (DrawLineWidget*)tf_calloc(1, sizeof(DrawLineWidget));

	pClonedWidget->mPos1 = pOriginalWidget->mPos1;
	pClonedWidget->mPos2 = pOriginalWidget->mPos2;
	pClonedWidget->mColor = pOriginalWidget->mColor;
	pClonedWidget->mAddItem = pOriginalWidget->mAddItem;

	return pClonedWidget;
}

// DrawCurveWidget private functions
DrawCurveWidget* cloneDrawCurveWidget(const void* pWidget)
{
	const DrawCurveWidget* pOriginalWidget = (const DrawCurveWidget*)pWidget;
	DrawCurveWidget* pClonedWidget = (DrawCurveWidget*)tf_calloc(1, sizeof(DrawCurveWidget));

	pClonedWidget->mPos = pOriginalWidget->mPos;
	pClonedWidget->mNumPoints = pOriginalWidget->mNumPoints;
	pClonedWidget->mThickness = pOriginalWidget->mThickness;
	pClonedWidget->mColor = pOriginalWidget->mColor;

	return pClonedWidget;
}

// UIWidget private functions
void cloneWidgetBase(UIWidget* pDstWidget, const UIWidget* pSrcWidget)
{
	pDstWidget->mType = pSrcWidget->mType;
	strcpy(pDstWidget->mLabel, pSrcWidget->mLabel);

	pDstWidget->pOnHover = pSrcWidget->pOnHover;
	pDstWidget->pOnActive = pSrcWidget->pOnActive;
	pDstWidget->pOnFocus = pSrcWidget->pOnFocus;
	pDstWidget->pOnEdited = pSrcWidget->pOnEdited;
	pDstWidget->pOnDeactivated = pSrcWidget->pOnDeactivated;
	pDstWidget->pOnDeactivatedAfterEdit = pSrcWidget->pOnDeactivatedAfterEdit;

	pDstWidget->mDeferred = pSrcWidget->mDeferred;
}



// UIWidget public functions
UIWidget* cloneWidget(const UIWidget* pOtherWidget)
{
	UIWidget* pWidget = (UIWidget*)tf_calloc(1, sizeof(UIWidget));
	cloneWidgetBase(pWidget, pOtherWidget);

	switch (pOtherWidget->mType)
	{
	case WIDGET_TYPE_COLLAPSING_HEADER:
	{
		pWidget->pWidget = cloneCollapsingHeaderWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DEBUG_TEXTURES:
	{
		pWidget->pWidget = cloneDebugTexturesWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_LABEL:
	{
		pWidget->pWidget = cloneLabelWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_LABEL:
	{
		pWidget->pWidget = cloneColorLabelWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_HORIZONTAL_SPACE:
	{
		pWidget->pWidget = cloneHorizontalSpaceWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SEPARATOR:
	{
		pWidget->pWidget = cloneSeparatorWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_VERTICAL_SEPARATOR:
	{
		pWidget->pWidget = cloneVerticalSeparatorWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_BUTTON:
	{
		pWidget->pWidget = cloneButtonWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT:
	{
		pWidget->pWidget = cloneSliderFloatWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT2:
	{
		pWidget->pWidget = cloneSliderFloat2Widget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT3:
	{
		pWidget->pWidget = cloneSliderFloat3Widget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT4:
	{
		pWidget->pWidget = cloneSliderFloat4Widget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_INT:
	{
		pWidget->pWidget = cloneSliderIntWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_UINT:
	{
		pWidget->pWidget = cloneSliderUintWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_RADIO_BUTTON:
	{
		pWidget->pWidget = cloneRadioButtonWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_CHECKBOX:
	{
		pWidget->pWidget = cloneCheckboxWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_ONE_LINE_CHECKBOX:
	{
		pWidget->pWidget = cloneOneLineCheckboxWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_CURSOR_LOCATION:
	{
		pWidget->pWidget = cloneCursorLocationWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DROPDOWN:
	{
		pWidget->pWidget = cloneDropdownWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_COLUMN:
	{
		pWidget->pWidget = cloneColumnWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_PROGRESS_BAR:
	{
		pWidget->pWidget = cloneProgressBarWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_SLIDER:
	{
		pWidget->pWidget = cloneColorSliderWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_HISTOGRAM:
	{
		pWidget->pWidget = cloneHistogramWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_PLOT_LINES:
	{
		pWidget->pWidget = clonePlotLinesWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_PICKER:
	{
		pWidget->pWidget = cloneColorPickerWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_TEXTBOX:
	{
		pWidget->pWidget = cloneTextboxWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DYNAMIC_TEXT:
	{
		pWidget->pWidget = cloneDynamicTextWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_FILLED_RECT:
	{
		pWidget->pWidget = cloneFilledRectWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_TEXT:
	{
		pWidget->pWidget = cloneDrawTextWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_TOOLTIP:
	{
		pWidget->pWidget = cloneDrawTooltipWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_LINE:
	{
		pWidget->pWidget = cloneDrawLineWidget(pOtherWidget->pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_CURVE:
	{
		pWidget->pWidget = cloneDrawCurveWidget(pOtherWidget->pWidget);
		break;
	}

	default:
		ASSERT(0);
	}

	return pWidget;
}

// UIWidget public functions
void processWidgetCallbacks(UIWidget* pWidget, bool deferred)
{
	if (!deferred)
	{
		pWidget->mHovered = ImGui::IsItemHovered();
		pWidget->mActive = ImGui::IsItemActive();
		pWidget->mFocused = ImGui::IsItemFocused();
		pWidget->mEdited = ImGui::IsItemEdited();
		pWidget->mDeactivated = ImGui::IsItemDeactivated();
		pWidget->mDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
	}

	if (pWidget->mDeferred != deferred)
	{
		return;
	}

	if (pWidget->pOnHover && pWidget->mHovered)
		pWidget->pOnHover();

	if (pWidget->pOnActive && pWidget->mActive)
		pWidget->pOnActive();

	if (pWidget->pOnFocus && pWidget->mFocused)
		pWidget->pOnFocus();

	if (pWidget->pOnEdited && pWidget->mEdited)
		pWidget->pOnEdited();

	if (pWidget->pOnDeactivated && pWidget->mDeactivated)
		pWidget->pOnDeactivated();

	if (pWidget->pOnDeactivatedAfterEdit && pWidget->mDeactivatedAfterEdit)
		pWidget->pOnDeactivatedAfterEdit();

}

// CollapsingHeaderWidget private functions
void drawCollapsingHeaderWidget(UIWidget* pWidget)
{
	CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);

	if (pOriginalWidget->mPreviousCollapsed != pOriginalWidget->mCollapsed)
	{
		ImGui::SetNextTreeNodeOpen(!pOriginalWidget->mCollapsed);
		pOriginalWidget->mPreviousCollapsed = pOriginalWidget->mCollapsed;
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
	if (pOriginalWidget->mDefaultOpen)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	if (!pOriginalWidget->mHeaderIsVisible || ImGui::CollapsingHeader(pWidget->mLabel, flags))
	{
		for (UIWidget* widget : pOriginalWidget->mGroupedWidgets)
			drawWidget(widget);
	}

	processWidgetCallbacks(pWidget);
}

// DebugTexturesWidget private functions
void drawDebugTexturesWidget(UIWidget* pWidget)
{
	DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);

	for (uint32_t i = 0; i < pOriginalWidget->mTextures.size(); ++i)
	{
		Texture* tex = (Texture*)pOriginalWidget->mTextures[i];
		ImGui::Image(tex, pOriginalWidget->mTextureDisplaySize);
		ImGui::SameLine();
	}

	processWidgetCallbacks(pWidget);
}

// LabelWidget private functions
void drawLabelWidget(UIWidget* pWidget)
{
	ImGui::Text("%s", pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// ColorLabelWidget private functions
void drawColorLabelWidget(UIWidget* pWidget)
{
	ColorLabelWidget* pOriginalWidget = (ColorLabelWidget*)(pWidget->pWidget);

	ImGui::TextColored(pOriginalWidget->mColor, "%s", pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// HorizontalSpaceWidget private functions
void drawHorizontalSpaceWidget(UIWidget* pWidget)
{
	ImGui::SameLine();
	processWidgetCallbacks(pWidget);
}

// SeparatorWidget private functions
void drawSeparatorWidget(UIWidget* pWidget)
{
	ImGui::Separator();
	processWidgetCallbacks(pWidget);
}

// VerticalSeparatorWidget private functions
void drawVerticalSeparatorWidget(UIWidget* pWidget)
{
	VerticalSeparatorWidget* pOriginalWidget = (VerticalSeparatorWidget*)(pWidget->pWidget);

	for (uint32_t i = 0; i < pOriginalWidget->mLineCount; ++i)
	{
		ImGui::VerticalSeparator();
	}

	processWidgetCallbacks(pWidget);
}

// ButtonWidget private functions
void drawButtonWidget(UIWidget* pWidget)
{
	ImGui::Button(pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// SliderFloatWidget private functions
void drawSliderFloatWidget(UIWidget* pWidget)
{
	SliderFloatWidget* pOriginalWidget = (SliderFloatWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::SliderFloatWithSteps(label, pOriginalWidget->pData, pOriginalWidget->mMin, pOriginalWidget->mMax, pOriginalWidget->mStep, pOriginalWidget->mFormat);
	processWidgetCallbacks(pWidget);
}

// SliderFloat2Widget private functions
void drawSliderFloat2Widget(UIWidget* pWidget)
{
	SliderFloat2Widget* pOriginalWidget = (SliderFloat2Widget*)(pWidget->pWidget);

	ImGui::Text("%s", pWidget->mLabel);
	for (uint32_t i = 0; i < 2; ++i)
	{
		char label[MAX_LABEL_LENGTH];
		LABELID1(&pOriginalWidget->pData->operator[](i), label);

		ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->mMin[i], pOriginalWidget->mMax[i], pOriginalWidget->mStep[i], pOriginalWidget->mFormat);
		processWidgetCallbacks(pWidget);
	}
}

// SliderFloat3Widget private functions
void drawSliderFloat3Widget(UIWidget* pWidget)
{
	SliderFloat3Widget* pOriginalWidget = (SliderFloat3Widget*)(pWidget->pWidget);

	ImGui::Text("%s", pWidget->mLabel);
	for (uint32_t i = 0; i < 3; ++i)
	{
		char label[MAX_LABEL_LENGTH];
		LABELID1(&pOriginalWidget->pData->operator[](i), label);
		ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->mMin[i], pOriginalWidget->mMax[i], pOriginalWidget->mStep[i], pOriginalWidget->mFormat);
		processWidgetCallbacks(pWidget);
	}
}

// SliderFloat4Widget private functions
void drawSliderFloat4Widget(UIWidget* pWidget)
{
	SliderFloat4Widget* pOriginalWidget = (SliderFloat4Widget*)(pWidget->pWidget);

	ImGui::Text("%s", pWidget->mLabel);
	for (uint32_t i = 0; i < 4; ++i)
	{
		char label[MAX_LABEL_LENGTH];
		LABELID1(&pOriginalWidget->pData->operator[](i), label);
		ImGui::SliderFloatWithSteps(label, &pOriginalWidget->pData->operator[](i), pOriginalWidget->mMin[i], pOriginalWidget->mMax[i], pOriginalWidget->mStep[i], pOriginalWidget->mFormat);
		processWidgetCallbacks(pWidget);
	}
}

// SliderIntWidget private functions
void drawSliderIntWidget(UIWidget* pWidget)
{
	SliderIntWidget* pOriginalWidget = (SliderIntWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::SliderIntWithSteps(label, pOriginalWidget->pData, pOriginalWidget->mMin, pOriginalWidget->mMax, pOriginalWidget->mStep, pOriginalWidget->mFormat);
	processWidgetCallbacks(pWidget);
}

// SliderUintWidget private functions
void drawSliderUintWidget(UIWidget* pWidget)
{
	SliderUintWidget* pOriginalWidget = (SliderUintWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::SliderIntWithSteps(label, (int32_t*)pOriginalWidget->pData, (int32_t)pOriginalWidget->mMin, (int32_t)pOriginalWidget->mMax, (int32_t)pOriginalWidget->mStep,
		pOriginalWidget->mFormat);
	processWidgetCallbacks(pWidget);
}

// RadioButtonWidget private functions
void drawRadioButtonWidget(UIWidget* pWidget)
{
	RadioButtonWidget* pOriginalWidget = (RadioButtonWidget*)(pWidget->pWidget);

	ImGui::RadioButton(pWidget->mLabel, pOriginalWidget->pData, pOriginalWidget->mRadioId);
	processWidgetCallbacks(pWidget);
}

// CheckboxWidget private functions
void drawCheckboxWidget(UIWidget* pWidget)
{
	CheckboxWidget* pOriginalWidget = (CheckboxWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::Checkbox(label, pOriginalWidget->pData);
	processWidgetCallbacks(pWidget);
}

// OneLineCheckboxWidget private functions
void drawOneLineCheckboxWidget(UIWidget* pWidget)
{
	OneLineCheckboxWidget* pOriginalWidget = (OneLineCheckboxWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Checkbox(label, pOriginalWidget->pData);
	ImGui::SameLine();
	ImGui::TextColored(pOriginalWidget->mColor, "%s", pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// CursorLocationWidget private functions
void drawCursorLocationWidget(UIWidget* pWidget)
{
	CursorLocationWidget* pOriginalWidget = (CursorLocationWidget*)(pWidget->pWidget);

	ImGui::SetCursorPos(pOriginalWidget->mLocation);
	processWidgetCallbacks(pWidget);
}

// DropdownWidget private functions
void drawDropdownWidget(UIWidget* pWidget)
{
	DropdownWidget* pOriginalWidget = (DropdownWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	uint32_t& current = *(pOriginalWidget->pData);
	ImGui::Text("%s", pWidget->mLabel);
	if (ImGui::BeginCombo(label, pOriginalWidget->mNames[current]))
	{
		for (uint32_t i = 0; i < (uint32_t)pOriginalWidget->mNames.size(); ++i)
		{
			if (ImGui::Selectable(pOriginalWidget->mNames[i]))
			{
				uint32_t prevVal = current;
				current = i;

				// Note that callbacks are sketchy with BeginCombo/EndCombo, so we manually process them here
				if (pWidget->pOnEdited)
					pWidget->pOnEdited();

				if (current != prevVal)
				{
					if (pWidget->pOnDeactivatedAfterEdit)
						pWidget->pOnDeactivatedAfterEdit();
				}
			}
		}
		ImGui::EndCombo();
	}
}

// ColumnWidget private functions
void drawColumnWidget(UIWidget* pWidget)
{
	ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);

	// Test a simple 4 col table.
	ImGui::BeginColumns(pWidget->mLabel, pOriginalWidget->mNumColumns, ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoForceWithinWindow);

	for (uint32_t i = 0; i < pOriginalWidget->mNumColumns; ++i)
	{
		drawWidget(pOriginalWidget->mPerColumnWidgets[i]);
		ImGui::NextColumn();
	}

	ImGui::EndColumns();

	processWidgetCallbacks(pWidget);
}

// ProgressBarWidget private functions
void drawProgressBarWidget(UIWidget* pWidget)
{
	ProgressBarWidget* pOriginalWidget = (ProgressBarWidget*)(pWidget->pWidget);

	size_t currProgress = *(pOriginalWidget->pData);
	ImGui::Text("%s", pWidget->mLabel);
	ImGui::ProgressBar((float)currProgress / pOriginalWidget->mMaxProgress);
	processWidgetCallbacks(pWidget);
}

// ColorSliderWidget private functions
void drawColorSliderWidget(UIWidget* pWidget)
{
	ColorSliderWidget* pOriginalWidget = (ColorSliderWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	float4* combo_color = pOriginalWidget->pData;

	float col[4] = { combo_color->x, combo_color->y, combo_color->z, combo_color->w };
	ImGui::Text("%s", pWidget->mLabel);
	if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaPreview))
	{
		if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z || col[3] != combo_color->w)
		{
			*combo_color = col;
		}
	}
	processWidgetCallbacks(pWidget);
}

// HistogramWidget private functions
void drawHistogramWidget(UIWidget* pWidget)
{
	HistogramWidget* pOriginalWidget = (HistogramWidget*)(pWidget->pWidget);

	ImGui::PlotHistogram(pWidget->mLabel, pOriginalWidget->pValues, pOriginalWidget->mCount, 0, pOriginalWidget->mHistogramTitle, *(pOriginalWidget->mMinScale),
		*(pOriginalWidget->mMaxScale), pOriginalWidget->mHistogramSize);

	processWidgetCallbacks(pWidget);
}

// PlotLinesWidget private functions
void drawPlotLinesWidget(UIWidget* pWidget)
{
	PlotLinesWidget* pOriginalWidget = (PlotLinesWidget*)(pWidget->pWidget);

	ImGui::PlotLines(pWidget->mLabel, pOriginalWidget->mValues, pOriginalWidget->mNumValues, 0, pOriginalWidget->mTitle, *(pOriginalWidget->mScaleMin), *(pOriginalWidget->mScaleMax),
		*(pOriginalWidget->mPlotScale));

	processWidgetCallbacks(pWidget);
}

// ColorPickerWidget private functions
void drawColorPickerWidget(UIWidget* pWidget)
{
	ColorPickerWidget* pOriginalWidget = (ColorPickerWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	float4* combo_color = pOriginalWidget->pData;

	float col[4] = { combo_color->x, combo_color->y, combo_color->z, combo_color->w };
	ImGui::Text("%s", pWidget->mLabel);
	if (ImGui::ColorPicker4(label, col, ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_Float))
	{
		if (col[0] != combo_color->x || col[1] != combo_color->y || col[2] != combo_color->z || col[3] != combo_color->w)
		{
			*combo_color = col;
		}
	}
	processWidgetCallbacks(pWidget);
}

// TextboxWidget private functions
void drawTextboxWidget(UIWidget* pWidget)
{
	TextboxWidget* pOriginalWidget = (TextboxWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::InputText(label, (char*)pOriginalWidget->pData, pOriginalWidget->mLength, pOriginalWidget->mAutoSelectAll ? ImGuiInputTextFlags_AutoSelectAll : 0);
	processWidgetCallbacks(pWidget);
}

// DynamicTextWidget private functions
void drawDynamicTextWidget(UIWidget* pWidget)
{
	DynamicTextWidget* pOriginalWidget = (DynamicTextWidget*)(pWidget->pWidget);

	ImGui::TextColored(*(pOriginalWidget->pColor), "%s", pOriginalWidget->pData);
	processWidgetCallbacks(pWidget);
}

// FilledRectWidget private functions
void drawFilledRectWidget(UIWidget* pWidget)
{
	FilledRectWidget* pOriginalWidget = (FilledRectWidget*)(pWidget->pWidget);

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	float2 pos = window->Pos - window->Scroll + pOriginalWidget->mPos;
	float2 pos2 = float2(pos.x + pOriginalWidget->mScale.x, pos.y + pOriginalWidget->mScale.y);

	ImGui::GetWindowDrawList()->AddRectFilled(pos, pos2, ImGui::GetColorU32(pOriginalWidget->mColor));

	processWidgetCallbacks(pWidget);
}

// DrawTextWidget private functions
void drawDrawTextWidget(UIWidget* pWidget)
{
	DrawTextWidget* pOriginalWidget = (DrawTextWidget*)(pWidget->pWidget);

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	float2 pos = window->Pos - window->Scroll + pOriginalWidget->mPos;
	const float2 line_size = ImGui::CalcTextSize(pWidget->mLabel);

	ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(pOriginalWidget->mColor), pWidget->mLabel);

	ImRect bounding_box(pos, pos + line_size);
	ImGui::ItemSize(bounding_box);
	ImGui::ItemAdd(bounding_box, 0);

	processWidgetCallbacks(pWidget);
}

// DrawTooltipWidget private functions
void drawDrawTooltipWidget(UIWidget* pWidget)
{
	DrawTooltipWidget* pOriginalWidget = (DrawTooltipWidget*)(pWidget->pWidget);

	if ((*(pOriginalWidget->mShowTooltip)) == true)
	{
		ImGui::BeginTooltip();

		ImGui::TextUnformatted(pOriginalWidget->mText);

		ImGui::EndTooltip();
	}

	processWidgetCallbacks(pWidget);
}

// DrawLineWidget private functions
void drawDrawLineWidget(UIWidget* pWidget)
{
	DrawLineWidget* pOriginalWidget = (DrawLineWidget*)(pWidget->pWidget);

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	float2 pos1 = window->Pos - window->Scroll + pOriginalWidget->mPos1;
	float2 pos2 = window->Pos - window->Scroll + pOriginalWidget->mPos2;

	ImGui::GetWindowDrawList()->AddLine(pos1, pos2, ImGui::GetColorU32(pOriginalWidget->mColor));

	if (pOriginalWidget->mAddItem)
	{
		ImRect bounding_box(pos1, pos2);
		ImGui::ItemSize(bounding_box);
		ImGui::ItemAdd(bounding_box, 0);
	}

	processWidgetCallbacks(pWidget);
}

// DrawCurveWidget private functions
void drawDrawCurveWidget(UIWidget* pWidget)
{
	DrawCurveWidget* pOriginalWidget = (DrawCurveWidget*)(pWidget->pWidget);

	ImGuiWindow* window = ImGui::GetCurrentWindow();

	for (uint32_t i = 0; i < pOriginalWidget->mNumPoints - 1; i++)
	{
		float2 pos1 = window->Pos - window->Scroll + pOriginalWidget->mPos[i];
		float2 pos2 = window->Pos - window->Scroll + pOriginalWidget->mPos[i + 1];
		ImGui::GetWindowDrawList()->AddLine(pos1, pos2, ImGui::GetColorU32(pOriginalWidget->mColor), pOriginalWidget->mThickness);
	}

	processWidgetCallbacks(pWidget);
}

void drawWidget(UIWidget* pWidget)
{
	switch (pWidget->mType)
	{
	case WIDGET_TYPE_COLLAPSING_HEADER:
	{
		drawCollapsingHeaderWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DEBUG_TEXTURES:
	{
		drawDebugTexturesWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_LABEL:
	{
		drawLabelWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_LABEL:
	{
		drawColorLabelWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_HORIZONTAL_SPACE:
	{
		drawHorizontalSpaceWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_SEPARATOR:
	{
		drawSeparatorWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_VERTICAL_SEPARATOR:
	{
		drawVerticalSeparatorWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_BUTTON:
	{
		drawButtonWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT:
	{
		drawSliderFloatWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT2:
	{
		drawSliderFloat2Widget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT3:
	{
		drawSliderFloat3Widget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT4:
	{
		drawSliderFloat4Widget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_INT:
	{
		drawSliderIntWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_UINT:
	{
		drawSliderUintWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_RADIO_BUTTON:
	{
		drawRadioButtonWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_CHECKBOX:
	{
		drawCheckboxWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_ONE_LINE_CHECKBOX:
	{
		drawOneLineCheckboxWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_CURSOR_LOCATION:
	{
		drawCursorLocationWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DROPDOWN:
	{
		drawDropdownWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_COLUMN:
	{
		drawColumnWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_PROGRESS_BAR:
	{
		drawProgressBarWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_SLIDER:
	{
		drawColorSliderWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_HISTOGRAM:
	{
		drawHistogramWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_PLOT_LINES:
	{
		drawPlotLinesWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_PICKER:
	{
		drawColorPickerWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_TEXTBOX:
	{
		drawTextboxWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DYNAMIC_TEXT:
	{
		drawDynamicTextWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_FILLED_RECT:
	{
		drawFilledRectWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_TEXT:
	{
		drawDrawTextWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_TOOLTIP:
	{
		drawDrawTooltipWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_LINE:
	{
		drawDrawLineWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_DRAW_CURVE:
	{
		drawDrawCurveWidget(pWidget);
		break;
	}

	default:
		ASSERT(0);
	}
}

void destroyWidget(UIWidget* pWidget, bool freeUnderlying)
{
	if (freeUnderlying)
	{
		switch (pWidget->mType)
		{
		case WIDGET_TYPE_COLLAPSING_HEADER:
		{
			CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);
			uiDestroyAllCollapsingHeaderSubWidgets(pOriginalWidget);
			pOriginalWidget->mGroupedWidgets.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_DEBUG_TEXTURES:
		{
			DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);
			pOriginalWidget->mTextures.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_DROPDOWN:
		{
			DropdownWidget* pOriginalWidget = (DropdownWidget*)(pWidget->pWidget);
			for (uint32_t i = 0; i < pOriginalWidget->mNames.size(); ++i)
			{
				tf_free(pOriginalWidget->mNames[i]);
			}
			pOriginalWidget->mNames.set_capacity(0);
			pOriginalWidget->mValues.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_COLUMN:
		{
			ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);
			pOriginalWidget->mPerColumnWidgets.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_LABEL:
		case WIDGET_TYPE_COLOR_LABEL:
		case WIDGET_TYPE_HORIZONTAL_SPACE:
		case WIDGET_TYPE_SEPARATOR:
		case WIDGET_TYPE_VERTICAL_SEPARATOR:
		case WIDGET_TYPE_BUTTON:
		case WIDGET_TYPE_SLIDER_FLOAT:
		case WIDGET_TYPE_SLIDER_FLOAT2:
		case WIDGET_TYPE_SLIDER_FLOAT3:
		case WIDGET_TYPE_SLIDER_FLOAT4:
		case WIDGET_TYPE_SLIDER_INT:
		case WIDGET_TYPE_SLIDER_UINT:
		case WIDGET_TYPE_RADIO_BUTTON:
		case WIDGET_TYPE_CHECKBOX:
		case WIDGET_TYPE_ONE_LINE_CHECKBOX:
		case WIDGET_TYPE_CURSOR_LOCATION:
		case WIDGET_TYPE_PROGRESS_BAR:
		case WIDGET_TYPE_COLOR_SLIDER:
		case WIDGET_TYPE_HISTOGRAM:
		case WIDGET_TYPE_PLOT_LINES:
		case WIDGET_TYPE_COLOR_PICKER:
		case WIDGET_TYPE_TEXTBOX:
		case WIDGET_TYPE_DYNAMIC_TEXT:
		case WIDGET_TYPE_FILLED_RECT:
		case WIDGET_TYPE_DRAW_TEXT:
		case WIDGET_TYPE_DRAW_TOOLTIP:
		case WIDGET_TYPE_DRAW_LINE:
		case WIDGET_TYPE_DRAW_CURVE:
		{
			break;
		}

		default:
			ASSERT(0);
		}
		
		switch (pWidget->mType)
		{
		case WIDGET_TYPE_COLLAPSING_HEADER:
		{
			CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);
			uiDestroyAllCollapsingHeaderSubWidgets(pOriginalWidget);
			pOriginalWidget->mGroupedWidgets.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_DEBUG_TEXTURES:
		{
			DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);
			pOriginalWidget->mTextures.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_DROPDOWN:
		{
			DropdownWidget* pOriginalWidget = (DropdownWidget*)(pWidget->pWidget);
			for (uint32_t i = 0; i < pOriginalWidget->mNames.size(); ++i)
			{
				tf_free(pOriginalWidget->mNames[i]);
			}
			pOriginalWidget->mNames.set_capacity(0);
			pOriginalWidget->mValues.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_COLUMN:
		{
			ColumnWidget* pOriginalWidget = (ColumnWidget*)(pWidget->pWidget);
			pOriginalWidget->mPerColumnWidgets.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_LABEL:
		case WIDGET_TYPE_COLOR_LABEL:
		case WIDGET_TYPE_HORIZONTAL_SPACE:
		case WIDGET_TYPE_SEPARATOR:
		case WIDGET_TYPE_VERTICAL_SEPARATOR:
		case WIDGET_TYPE_BUTTON:
		case WIDGET_TYPE_SLIDER_FLOAT:
		case WIDGET_TYPE_SLIDER_FLOAT2:
		case WIDGET_TYPE_SLIDER_FLOAT3:
		case WIDGET_TYPE_SLIDER_FLOAT4:
		case WIDGET_TYPE_SLIDER_INT:
		case WIDGET_TYPE_SLIDER_UINT:
		case WIDGET_TYPE_RADIO_BUTTON:
		case WIDGET_TYPE_CHECKBOX:
		case WIDGET_TYPE_ONE_LINE_CHECKBOX:
		case WIDGET_TYPE_CURSOR_LOCATION:
		case WIDGET_TYPE_PROGRESS_BAR:
		case WIDGET_TYPE_COLOR_SLIDER:
		case WIDGET_TYPE_HISTOGRAM:
		case WIDGET_TYPE_PLOT_LINES:
		case WIDGET_TYPE_COLOR_PICKER:
		case WIDGET_TYPE_TEXTBOX:
		case WIDGET_TYPE_DYNAMIC_TEXT:
		case WIDGET_TYPE_FILLED_RECT:
		case WIDGET_TYPE_DRAW_TEXT:
		case WIDGET_TYPE_DRAW_TOOLTIP:
		case WIDGET_TYPE_DRAW_LINE:
		case WIDGET_TYPE_DRAW_CURVE:
		{
			break;
		}

		default:
			ASSERT(0);
		}

		tf_free(pWidget->pWidget);
	}

	tf_free(pWidget);
}

#endif // ENABLE_FORGE_UI

/****************************************************************************/
// MARK: - Collapsing Header Widget Public Functions
/****************************************************************************/

// CollapsingHeaderWidget public functions
UIWidget* uiCreateCollapsingHeaderSubWidget(CollapsingHeaderWidget* pWidget, const char* pLabel, const void* pSubWidget, WidgetType type)
{
#ifdef ENABLE_FORGE_UI
	UIWidget widget{};
	widget.mType = type;
	widget.pWidget = (void*)pSubWidget;
	strcpy(widget.mLabel, pLabel);

	pWidget->mGroupedWidgets.emplace_back(cloneWidget(&widget));
	return pWidget->mGroupedWidgets.back();
#else
	return NULL; 
#endif
}

void uiDestroyCollapsingHeaderSubWidget(CollapsingHeaderWidget* pWidget, UIWidget* pSubWidget)
{
#ifdef ENABLE_FORGE_UI
	decltype(pWidget->mGroupedWidgets)::iterator it = eastl::find_if(pWidget->mGroupedWidgets.begin(), pWidget->mGroupedWidgets.end(),
		[pSubWidget](const UIWidget* pIterWidget) -> bool { return pSubWidget->pWidget == pIterWidget->pWidget; });
	if (it != pWidget->mGroupedWidgets.end())
	{
		destroyWidget(*it, true);
		pWidget->mGroupedWidgets.erase(it);
	}
#endif
}

void uiDestroyAllCollapsingHeaderSubWidgets(CollapsingHeaderWidget* pWidget)
{
#ifdef ENABLE_FORGE_UI
	for (size_t i = 0; i < pWidget->mGroupedWidgets.size(); ++i)
	{
		destroyWidget(pWidget->mGroupedWidgets[i], true);
	}
#endif
}

void uiSetCollapsingHeaderWidgetCollapsed(CollapsingHeaderWidget* pWidget, bool collapsed)
{
#ifdef ENABLE_FORGE_UI
	pWidget->mCollapsed = collapsed;
	pWidget->mPreviousCollapsed = !collapsed;
#endif
}

/****************************************************************************/
// MARK: - Dynamic UI Public Functions
/****************************************************************************/

// DynamicUIWidgets public functions
UIWidget* uiCreateDynamicWidgets(DynamicUIWidgets* pDynamicUI, const char* pLabel, const void* pWidget, WidgetType type)
{
#ifdef ENABLE_FORGE_UI
	UIWidget widget{};
	widget.mType = type;
	widget.pWidget = (void*)pWidget;
	strcpy(widget.mLabel, pLabel);

	pDynamicUI->mDynamicProperties.emplace_back(cloneWidget(&widget));

	return pDynamicUI->mDynamicProperties.back();
#else
	return NULL;
#endif
}

void uiShowDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
	for (size_t i = 0; i < pDynamicUI->mDynamicProperties.size(); ++i)
	{
		UIWidget* pWidget = pDynamicUI->mDynamicProperties[i];
		UIWidget* pNewWidget = uiCreateComponentWidget(pGui, pWidget->mLabel, pWidget->pWidget, pWidget->mType, false);
		cloneWidgetBase(pNewWidget, pWidget);
	}
#endif
}

void uiHideDynamicWidgets(const DynamicUIWidgets* pDynamicUI, UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
	for (size_t i = 0; i < pDynamicUI->mDynamicProperties.size(); i++)
	{
		// We should not erase the widgets in this for-loop, otherwise the IDs
		// in mDynamicPropHandles will not match once  UIComponent::mWidgets changes size.
		uiDestroyComponentWidget(pGui, pDynamicUI->mDynamicProperties[i]);
	}
#endif
}

void uiDestroyDynamicWidgets(DynamicUIWidgets* pDynamicUI)
{
#ifdef ENABLE_FORGE_UI
	for (size_t i = 0; i < pDynamicUI->mDynamicProperties.size(); ++i)
	{
		destroyWidget(pDynamicUI->mDynamicProperties[i], true);
	}

	pDynamicUI->mDynamicProperties.set_capacity(0);
#endif
}

/****************************************************************************/
// MARK: - UI Component Public Functions
/****************************************************************************/

void uiCreateComponent(const char* pTitle, const UIComponentDesc* pDesc, UIComponent** ppUIComponent)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(ppUIComponent);
	UIComponent* pComponent = (UIComponent*)(tf_calloc(1, sizeof(UIComponent)));
	pComponent->mHasCloseButton = false;
	pComponent->mFlags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;
#if defined(TARGET_IOS) || defined(__ANDROID__)
	pComponent->mFlags |= GUI_COMPONENT_FLAGS_START_COLLAPSED;
#endif

#ifdef ENABLE_FORGE_FONTS
	// Functions not accessible via normal interface header
	extern void* fntGetRawFontData(uint32_t fontID);
	extern uint32_t fntGetRawFontDataSize(uint32_t fontID);

	// Use Requested Forge Font
	void* pFontBuffer = fntGetRawFontData(pDesc->mFontID);
	uint32_t fontBufferSize = fntGetRawFontDataSize(pDesc->mFontID);
	if (pFontBuffer)
		addImguiFont(pFontBuffer, fontBufferSize, NULL, pDesc->mFontSize, &pComponent->pFont);
#else
	// Use ImGui Default Font
	addImguiFont(NULL, NULL, NULL, NULL, &pComponent->pFont);
#endif

	pComponent->mInitialWindowRect = { pDesc->mStartPosition.getX(), pDesc->mStartPosition.getY(), pDesc->mStartSize.getX(),
										 pDesc->mStartSize.getY() };

	pComponent->mActive = true;
	strcpy(pComponent->mTitle, pTitle);
	pComponent->mAlpha = 1.0f;
	pUserInterface->mComponents.emplace_back(pComponent);

	*ppUIComponent = pComponent;
#endif
}

void uiDestroyComponent(UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pGui);

	uiDestroyAllComponentWidgets(pGui);
	UIComponent** it = eastl::find(pUserInterface->mComponents.begin(), pUserInterface->mComponents.end(), pGui);
	if (it != pUserInterface->mComponents.end())
	{
		uiDestroyAllComponentWidgets(*it);
		pUserInterface->mComponents.erase(it);
		pGui->mWidgets.clear();
	}

	tf_free(pGui);
#endif
}

void uiSetComponentActive(UIComponent* pUIComponent, bool active)
{
#ifdef ENABLE_FORGE_UI
	pUIComponent->mActive = active;
#endif
}

/****************************************************************************/
// MARK: - Public UIWidget Add/Remove Functions
/****************************************************************************/

// UIComponent public functions
UIWidget* uiCreateComponentWidget(UIComponent* pGui, const char* pLabel, const void* pWidget, WidgetType type, bool clone /* = true*/)
{
#ifdef ENABLE_FORGE_UI
	UIWidget* pBaseWidget = (UIWidget*)tf_calloc(1, sizeof(UIWidget));
	pBaseWidget->mType = type;
	pBaseWidget->pWidget = (void*)pWidget;
	strcpy(pBaseWidget->mLabel, pLabel);

	pGui->mWidgets.emplace_back(clone ? cloneWidget(pBaseWidget) : pBaseWidget);
	pGui->mWidgetsClone.emplace_back(clone);

	if (clone)
		tf_free(pBaseWidget);

	return pGui->mWidgets.back();
#else
	return NULL; 
#endif
}

void uiDestroyComponentWidget(UIComponent* pGui, UIWidget* pWidget)
{
#ifdef ENABLE_FORGE_UI
	decltype(pGui->mWidgets)::iterator it = eastl::find_if(pGui->mWidgets.begin(), pGui->mWidgets.end(),
		[pWidget](const UIWidget* pIterWidget) -> bool { return pWidget->pWidget == pIterWidget->pWidget; });
	if (it != pGui->mWidgets.end())
	{
		destroyWidget(*it, pGui->mWidgetsClone[it - pGui->mWidgets.begin()]);
		pGui->mWidgetsClone.erase(pGui->mWidgetsClone.begin() + (it - pGui->mWidgets.begin()));
		pGui->mWidgets.erase(it);
	}
#endif
}

void uiDestroyAllComponentWidgets(UIComponent* pGui)
{
#ifdef ENABLE_FORGE_UI
	for (uint32_t i = 0; i < (uint32_t)(pGui->mWidgets.size()); ++i)
	{
		destroyWidget(pGui->mWidgets[i], pGui->mWidgetsClone[i]);
	}

	pGui->mWidgets.clear();
	pGui->mWidgetsClone.clear();
	pGui->mWidgets.set_capacity(0);
	pGui->mWidgetsClone.set_capacity(0);
	pGui->mContextualMenuCallbacks.clear();
	pGui->mContextualMenuCallbacks.set_capacity(0);
	pGui->mContextualMenuLabels.clear();
	pGui->mContextualMenuLabels.set_capacity(0);
#endif
}

/****************************************************************************/
// MARK: - Safe Public Setter Functions
/****************************************************************************/

void uiSetComponentFlags(UIComponent* pGui, int32_t flags)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pGui);

	pGui->mFlags = flags;
#endif
}

void uiSetWidgetDeferred(UIWidget* pWidget, bool deferred)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget);

	pWidget->mDeferred = deferred; 
#endif
}

void uiSetWidgetOnHoverCallback(UIWidget* pWidget, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget);
	ASSERT(callback);

	pWidget->pOnHover = callback;
#endif
}

void uiSetWidgetOnActiveCallback(UIWidget* pWidget, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget);
	ASSERT(callback);

	pWidget->pOnActive = callback;
#endif
}

void uiSetWidgetOnFocusCallback(UIWidget* pWidget, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget);
	ASSERT(callback);

	pWidget->pOnFocus = callback;
#endif
}

void uiSetWidgetOnEditedCallback(UIWidget* pWidget, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget); 
	ASSERT(callback); 

	pWidget->pOnEdited = callback; 
#endif
}

void uiSetWidgetOnDeactivatedCallback(UIWidget* pWidget, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget);
	ASSERT(callback);

	pWidget->pOnDeactivated = callback;
#endif
}

void uiSetWidgetOnDeactivatedAfterEditCallback(UIWidget* pWidget, WidgetCallback callback)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pWidget);
	ASSERT(callback);

	pWidget->pOnDeactivatedAfterEdit = callback;
#endif
}

/****************************************************************************/
// MARK: - Private Platform Layer Life Cycle Functions
/****************************************************************************/

// UIApp public functions
bool platformInitUserInterface()
{
#ifdef ENABLE_FORGE_UI
	UserInterface* pAppUI = tf_new(UserInterface);

	pAppUI->mShowDemoUiWindow = false;

	pAppUI->mHandledGestures = false;
	pAppUI->mActive = true;
	memset(pAppUI->mPostUpdateKeyDownStates, 0, sizeof(pAppUI->mPostUpdateKeyDownStates));

	getDpiScale(pAppUI->dpiScale);

	//// init UI (input)
	ImGui::SetAllocatorFunctions(alloc_func, dealloc_func);
	pAppUI->context = ImGui::CreateContext();
	ImGui::SetCurrentContext(pAppUI->context);

	SetDefaultStyle();

	ImGuiIO& io = ImGui::GetIO();

	// TODO: Might be a good idea to considder adding these flags in some platforms:
	//         - ImGuiConfigFlags_IsTouchScreen
	//         - ImGuiConfigFlags_NavEnableKeyboard
	io.ConfigFlags = ImGuiConfigFlags_NavEnableGamepad; 

	io.KeyMap[ImGuiKey_Backspace] = InputBindings::BUTTON_BACK;
	io.KeyMap[ImGuiKey_LeftArrow] = InputBindings::BUTTON_KEYLEFT;
	io.KeyMap[ImGuiKey_RightArrow] = InputBindings::BUTTON_KEYRIGHT;
	io.KeyMap[ImGuiKey_Home] = InputBindings::BUTTON_KEYHOME;
	io.KeyMap[ImGuiKey_End] = InputBindings::BUTTON_KEYEND;
	io.KeyMap[ImGuiKey_Delete] = InputBindings::BUTTON_KEYDELETE;

	pUserInterface = pAppUI;
#endif

	return true; 
}

void platformExitUserInterface()
{
#ifdef ENABLE_FORGE_UI
	for (uint32_t i = 0; i < (uint32_t)pUserInterface->mComponents.size(); ++i)
	{
		uiDestroyAllComponentWidgets(pUserInterface->mComponents[i]);
		tf_free(pUserInterface->mComponents[i]);
	}
	pUserInterface->mComponents.clear();

	ImGui::DestroyDemoWindow();
	ImGui::DestroyContext(pUserInterface->context);

	tf_delete(pUserInterface);
#endif
}

void platformUpdateUserInterface(float deltaTime)
{
#ifdef ENABLE_FORGE_UI
	if (pUserInterface->mUpdated)
		return;

	eastl::vector<UIComponent*> activeComponents(pUserInterface->mComponents.size());
	uint32_t                       activeComponentCount = 0;
	for (uint32_t i = 0; i < (uint32_t)pUserInterface->mComponents.size(); ++i)
		if (pUserInterface->mComponents[i]->mActive)
			activeComponents[activeComponentCount++] = pUserInterface->mComponents[i];

	if (activeComponentCount == 0)
		return;

	pUserInterface->mUpdated = true;

	GUIDriverUpdate guiUpdate = {};
	guiUpdate.pUIComponents = activeComponents.data();
	guiUpdate.componentCount = activeComponentCount;
	guiUpdate.deltaTime = deltaTime;
	guiUpdate.width = pUserInterface->mWidth;
	guiUpdate.height = pUserInterface->mHeight;
	guiUpdate.showDemoWindow = pUserInterface->mShowDemoUiWindow;

	ImGui::SetCurrentContext(pUserInterface->context);
	// #TODO: Use window size as render-target size cannot be trusted to be the same as window size
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = guiUpdate.width;
	io.DisplaySize.y = guiUpdate.height;
	io.DeltaTime = guiUpdate.deltaTime;
	if (pUserInterface->pMovePosition)
		io.MousePos = *pUserInterface->pMovePosition;

	memcpy(io.NavInputs, pUserInterface->mNavInputs, sizeof(pUserInterface->mNavInputs));

	ImGui::NewFrame();

	bool ret = false;

	if (pUserInterface->mActive)
	{
		if (guiUpdate.showDemoWindow)
			ImGui::ShowDemoWindow();


		pUserInterface->mLastUpdateCount = guiUpdate.componentCount;

		for (uint32_t compIndex = 0; compIndex < guiUpdate.componentCount; ++compIndex)
		{
			UIComponent*                           pComponent = guiUpdate.pUIComponents[compIndex];
			char									title[MAX_TITLE_STR_LENGTH]{};
			int32_t                                 UIComponentFlags = pComponent->mFlags;
			bool*                                   pCloseButtonActiveValue = pComponent->mHasCloseButton ? &pComponent->mHasCloseButton : NULL;
			const eastl::vector<char*>& contextualMenuLabels = pComponent->mContextualMenuLabels;
			const eastl::vector<WidgetCallback>&  contextualMenuCallbacks = pComponent->mContextualMenuCallbacks;
			const float4&                           windowRect = pComponent->mInitialWindowRect;
			float4&                                 currentWindowRect = pComponent->mCurrentWindowRect;
			UIWidget**                               pProps = pComponent->mWidgets.data();
			uint32_t                                propCount = (uint32_t)pComponent->mWidgets.size();

			strcpy(title, pComponent->mTitle);

			if (title[0] == '\0')
				sprintf(title, "##%llu", (unsigned long long)pComponent);
			// Setup the ImGuiWindowFlags
			ImGuiWindowFlags guiWinFlags = GUI_COMPONENT_FLAGS_NONE;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_TITLE_BAR)
				guiWinFlags |= ImGuiWindowFlags_NoTitleBar;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE)
				guiWinFlags |= ImGuiWindowFlags_NoResize;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
				guiWinFlags |= ImGuiWindowFlags_NoMove;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_SCROLLBAR)
				guiWinFlags |= ImGuiWindowFlags_NoScrollbar;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_COLLAPSE)
				guiWinFlags |= ImGuiWindowFlags_NoCollapse;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE)
				guiWinFlags |= ImGuiWindowFlags_AlwaysAutoResize;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_INPUTS)
				guiWinFlags |= ImGuiWindowFlags_NoInputs;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_MEMU_BAR)
				guiWinFlags |= ImGuiWindowFlags_MenuBar;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_HORIZONTAL_SCROLLBAR)
				guiWinFlags |= ImGuiWindowFlags_HorizontalScrollbar;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_FOCUS_ON_APPEARING)
				guiWinFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS)
				guiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR)
				guiWinFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR)
				guiWinFlags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_USE_WINDOW_PADDING)
				guiWinFlags |= ImGuiWindowFlags_AlwaysUseWindowPadding;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_INPUT)
				guiWinFlags |= ImGuiWindowFlags_NoNavInputs;
			if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_FOCUS)
				guiWinFlags |= ImGuiWindowFlags_NoNavFocus;

			ImGui::PushFont((ImFont*)pComponent->pFont);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pComponent->mAlpha);
			bool result = ImGui::Begin(title, pCloseButtonActiveValue, guiWinFlags);
			if (result)
			{
				// Setup the contextual menus
				if (!contextualMenuLabels.empty() && ImGui::BeginPopupContextItem())    // <-- This is using IsItemHovered()
				{
					for (size_t i = 0; i < contextualMenuLabels.size(); i++)
					{
						if (ImGui::MenuItem(contextualMenuLabels[i]))
						{
							if (i < contextualMenuCallbacks.size())
								contextualMenuCallbacks[i]();
						}
					}
					ImGui::EndPopup();
				}

				bool overrideSize = false;
				bool overridePos = false;

				if ((UIComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE) && !(UIComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE))
					overrideSize = true;

				if (UIComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
					overridePos = true;

				ImGui::SetWindowSize(
					float2(windowRect.z, windowRect.w), overrideSize ? ImGuiCond_Always : ImGuiCond_Once);
				ImGui::SetWindowPos(
					float2(windowRect.x, windowRect.y), overridePos ? ImGuiCond_Always : ImGuiCond_Once);

				if (UIComponentFlags & GUI_COMPONENT_FLAGS_START_COLLAPSED)
					ImGui::SetWindowCollapsed(true, ImGuiCond_Once);

				for (uint32_t i = 0; i < propCount; ++i)
				{
					if (pProps[i] != nullptr)
					{
						drawWidget(pProps[i]);
					}
				}

				ret = ret || ImGui::GetIO().WantCaptureMouse;
			}

			float2 pos = ImGui::GetWindowPos();
			float2 size = ImGui::GetWindowSize();
			currentWindowRect.x = pos.x;
			currentWindowRect.y = pos.y;
			currentWindowRect.z = size.x;
			currentWindowRect.w = size.y;
			pUserInterface->mLastUpdateMin[compIndex] = pos;
			pUserInterface->mLastUpdateMax[compIndex] = pos + size;

			// Need to call ImGui::End event if result is false since we called ImGui::Begin
			ImGui::End();
			ImGui::PopStyleVar();
			ImGui::PopFont();
		}
	}
	ImGui::EndFrame();

	if (pUserInterface->mActive)
	{
		for (uint32_t compIndex = 0; compIndex < guiUpdate.componentCount; ++compIndex)
		{
			UIComponent*                           pComponent = guiUpdate.pUIComponents[compIndex];
			UIWidget**                               pProps = pComponent->mWidgets.data();
			uint32_t                                propCount = (uint32_t)pComponent->mWidgets.size();

			for (uint32_t i = 0; i < propCount; ++i)
			{
				if (pProps[i] != nullptr)
				{
					processWidgetCallbacks(pProps[i], true);
				}
			}
		}
	}

	if (!io.MouseDown[0])
	{
		io.MousePos = float2(-FLT_MAX);
	}

	pUserInterface->mHandledGestures = false;

	// Apply post update keydown states
	memcpy(io.KeysDown, pUserInterface->mPostUpdateKeyDownStates, sizeof(io.KeysDown));
#endif
}

/****************************************************************************/
// MARK: - Public App Layer Life Cycle Functions
/****************************************************************************/

void initUserInterface(UserInterfaceDesc* pDesc)
{
#ifdef ENABLE_FORGE_UI
	pUserInterface->pRenderer = (Renderer*)pDesc->pRenderer;
	pUserInterface->pPipelineCache = (PipelineCache*)pDesc->pCache;
	pUserInterface->mMaxDynamicUIUpdatesPerBatch = pDesc->maxDynamicUIUpdatesPerBatch; 

	/************************************************************************/
	// Rendering resources
	/************************************************************************/
	SamplerDesc samplerDesc = { FILTER_LINEAR,
								FILTER_LINEAR,
								MIPMAP_MODE_NEAREST,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE };
	addSampler(pUserInterface->pRenderer, &samplerDesc, &pUserInterface->pDefaultSampler);

#ifdef ENABLE_UI_PRECOMPILED_SHADERS
	BinaryShaderDesc binaryShaderDesc = {};
	binaryShaderDesc.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
	binaryShaderDesc.mVert.mByteCodeSize = sizeof(gShaderImguiVert);
	binaryShaderDesc.mVert.pByteCode = (char*)gShaderImguiVert;
	binaryShaderDesc.mVert.pEntryPoint = "main";
	binaryShaderDesc.mFrag.mByteCodeSize = sizeof(gShaderImguiFrag);
	binaryShaderDesc.mFrag.pByteCode = (char*)gShaderImguiFrag;
	binaryShaderDesc.mFrag.pEntryPoint = "main";
	addShaderBinary(pRenderer, &binaryShaderDesc, &pShaderTextured);
#else
	ShaderLoadDesc texturedShaderDesc = {};
	texturedShaderDesc.mStages[0] = { "imgui.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
	texturedShaderDesc.mStages[1] = { "imgui.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
	addShader(pUserInterface->pRenderer, &texturedShaderDesc, &pUserInterface->pShaderTextured);
#endif

	const char*       pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pUserInterface->pShaderTextured, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pUserInterface->pDefaultSampler;
	addRootSignature(pUserInterface->pRenderer, &textureRootDesc, &pUserInterface->pRootSignatureTextured);

	DescriptorSetDesc setDesc = { pUserInterface->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, 1 + (pDesc->maxDynamicUIUpdatesPerBatch * MAX_FRAMES) };
	addDescriptorSet(pUserInterface->pRenderer, &setDesc, &pUserInterface->pDescriptorSetTexture);
	setDesc = { pUserInterface->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_NONE, MAX_FRAMES };
	addDescriptorSet(pUserInterface->pRenderer, &setDesc, &pUserInterface->pDescriptorSetUniforms);

	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mSize = VERTEX_BUFFER_SIZE * MAX_FRAMES;
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.ppBuffer = &pUserInterface->pVertexBuffer;
	addResource(&vbDesc, NULL);

	BufferLoadDesc ibDesc = vbDesc;
	ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	ibDesc.mDesc.mSize = INDEX_BUFFER_SIZE * MAX_FRAMES;
	ibDesc.ppBuffer = &pUserInterface->pIndexBuffer;
	addResource(&ibDesc, NULL);

	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.mDesc.mSize = sizeof(mat4);
	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
	{
		ubDesc.ppBuffer = &pUserInterface->pUniformBuffer[i];
		addResource(&ubDesc, NULL);
	}

	pUserInterface->mVertexLayoutTextured.mAttribCount = 3;
	pUserInterface->mVertexLayoutTextured.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	pUserInterface->mVertexLayoutTextured.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
	pUserInterface->mVertexLayoutTextured.mAttribs[0].mBinding = 0;
	pUserInterface->mVertexLayoutTextured.mAttribs[0].mLocation = 0;
	pUserInterface->mVertexLayoutTextured.mAttribs[0].mOffset = 0;
	pUserInterface->mVertexLayoutTextured.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
	pUserInterface->mVertexLayoutTextured.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
	pUserInterface->mVertexLayoutTextured.mAttribs[1].mBinding = 0;
	pUserInterface->mVertexLayoutTextured.mAttribs[1].mLocation = 1;
	pUserInterface->mVertexLayoutTextured.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(pUserInterface->mVertexLayoutTextured.mAttribs[0].mFormat) / 8;
	pUserInterface->mVertexLayoutTextured.mAttribs[2].mSemantic = SEMANTIC_COLOR;
	pUserInterface->mVertexLayoutTextured.mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	pUserInterface->mVertexLayoutTextured.mAttribs[2].mBinding = 0;
	pUserInterface->mVertexLayoutTextured.mAttribs[2].mLocation = 2;
	pUserInterface->mVertexLayoutTextured.mAttribs[2].mOffset =
	pUserInterface->mVertexLayoutTextured.mAttribs[1].mOffset + TinyImageFormat_BitSizeOfBlock(pUserInterface->mVertexLayoutTextured.mAttribs[1].mFormat) / 8;

	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
	{
		DescriptorData params[1] = {};
		params[0].pName = "uniformBlockVS";
		params[0].ppBuffers = &pUserInterface->pUniformBuffer[i];
		updateDescriptorSet(pUserInterface->pRenderer, i, pUserInterface->pDescriptorSetUniforms, 1, params);
	}
#endif
}

void exitUserInterface()
{
#ifdef ENABLE_FORGE_UI
	removeSampler(pUserInterface->pRenderer, pUserInterface->pDefaultSampler);
	removeShader(pUserInterface->pRenderer, pUserInterface->pShaderTextured);	
	removeDescriptorSet(pUserInterface->pRenderer, pUserInterface->pDescriptorSetTexture);
	removeDescriptorSet(pUserInterface->pRenderer, pUserInterface->pDescriptorSetUniforms);
	removeRootSignature(pUserInterface->pRenderer, pUserInterface->pRootSignatureTextured);
	removeResource(pUserInterface->pVertexBuffer);
	removeResource(pUserInterface->pIndexBuffer);
	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
		removeResource(pUserInterface->pUniformBuffer[i]);

	for (Texture*& pFontTexture : pUserInterface->mFontTextures)
		removeResource(pFontTexture);

	pUserInterface->mFontTextures.set_capacity(0);
#endif
}

bool addUserInterfacePipelines(void* pRenderTarget)
{
#ifdef ENABLE_FORGE_UI
	ASSERT(pRenderTarget);
	RenderTarget* pRT = (RenderTarget*)pRenderTarget;
	pUserInterface->mWidth = (float)pRT->mWidth;
	pUserInterface->mHeight = (float)pRT->mHeight;

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
	desc.pCache = pUserInterface->pPipelineCache;
	desc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
	pipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
	pipelineDesc.mRenderTargetCount = 1;
	pipelineDesc.mSampleCount = pRT->mSampleCount;
	pipelineDesc.pBlendState = &blendStateDesc;
	pipelineDesc.mSampleQuality = pRT->mSampleQuality;
	pipelineDesc.pColorFormats = &pRT->mFormat;
	pipelineDesc.pDepthState = &depthStateDesc;
	pipelineDesc.pRasterizerState = &rasterizerStateDesc;
	pipelineDesc.pRootSignature = pUserInterface->pRootSignatureTextured;
	pipelineDesc.pShaderProgram = pUserInterface->pShaderTextured;
	pipelineDesc.pVertexLayout = &pUserInterface->mVertexLayoutTextured;
	pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    pipelineDesc.mVRFoveatedRendering = true;
	addPipeline(pUserInterface->pRenderer, &desc, &pUserInterface->pPipelineTextured);

#if TOUCH_INPUT
	extern bool addVirtualJoystickPipeline(void* pScreenRenderTarget);
	addVirtualJoystickPipeline(pRenderTarget);
#endif
#endif

	return true;
}

void removeUserInterfacePipelines()
{
#ifdef ENABLE_FORGE_UI
#if TOUCH_INPUT
	extern bool removeVirtualJoystickPipeline(); 
	removeVirtualJoystickPipeline();
#endif

	removePipeline(pUserInterface->pRenderer, pUserInterface->pPipelineTextured);
#endif
}

void cmdDrawUserInterface(void* pCmd)
{
#ifdef ENABLE_FORGE_UI
	Cmd* cmd = (Cmd*)pCmd; 

	extern void drawProfilerUI();
	drawProfilerUI(); 

	if (pUserInterface->mUpdated)
	{
		pUserInterface->mUpdated = false;

		/************************************************************************/
		/************************************************************************/
		ImGui::SetCurrentContext(pUserInterface->context);
		ImGui::Render();
		pUserInterface->mDynamicUIUpdates = 0;

		ImDrawData* draw_data = ImGui::GetDrawData();

		Pipeline*            pPipeline = pUserInterface->pPipelineTextured;

		uint32_t vSize = 0;
		uint32_t iSize = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			vSize += (int)(cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
			iSize += (int)(cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
		}

		vSize = min<uint32_t>(vSize, VERTEX_BUFFER_SIZE);
		iSize = min<uint32_t>(iSize, INDEX_BUFFER_SIZE);

		// Copy and convert all vertices into a single contiguous buffer
		uint64_t vOffset = pUserInterface->frameIdx * VERTEX_BUFFER_SIZE;
		uint64_t iOffset = pUserInterface->frameIdx * INDEX_BUFFER_SIZE;
		uint64_t vtx_dst = vOffset;
		uint64_t idx_dst = iOffset;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			BufferUpdateDesc  update = { pUserInterface->pVertexBuffer, vtx_dst };
			beginUpdateResource(&update);
			memcpy(update.pMappedData, cmd_list->VtxBuffer.data(), cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
			endUpdateResource(&update, NULL);

			update = { pUserInterface->pIndexBuffer, idx_dst };
			beginUpdateResource(&update);
			memcpy(update.pMappedData, cmd_list->IdxBuffer.data(), cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
			endUpdateResource(&update, NULL);

			vtx_dst += (cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
			idx_dst += (cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
		}

		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float mvp[4][4] = {
			{ 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
			{ 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.5f, 0.0f },
			{ (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
		};
		BufferUpdateDesc update = { pUserInterface->pUniformBuffer[pUserInterface->frameIdx] };
		beginUpdateResource(&update);
		*((mat4*)update.pMappedData) = *(mat4*)mvp;
		endUpdateResource(&update, NULL);

		const uint32_t vertexStride = sizeof(ImDrawVert);

		cmdSetViewport(cmd, 0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
		cmdSetScissor(
			cmd, (uint32_t)draw_data->DisplayPos.x, (uint32_t)draw_data->DisplayPos.y, (uint32_t)draw_data->DisplaySize.x,
			(uint32_t)draw_data->DisplaySize.y);
		cmdBindPipeline(cmd, pPipeline);
		cmdBindIndexBuffer(cmd, pUserInterface->pIndexBuffer, INDEX_TYPE_UINT16, iOffset);
		cmdBindVertexBuffer(cmd, 1, &pUserInterface->pVertexBuffer, &vertexStride, &vOffset);

		cmdBindDescriptorSet(cmd, pUserInterface->frameIdx, pUserInterface->pDescriptorSetUniforms);

		// Render command lists
		int    vtx_offset = 0;
		int    idx_offset = 0;
		float2 pos = draw_data->DisplayPos;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			for (uint32_t cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback)
				{
					// User callback (registered via ImDrawList::AddCallback)
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					// Apply scissor/clipping rectangle
					//const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - pos.x), (LONG)(pcmd->ClipRect.y - pos.y), (LONG)(pcmd->ClipRect.z - pos.x), (LONG)(pcmd->ClipRect.w - pos.y) };
					//pCmd->pDxCmdList->RSSetScissorRects(1, &r);
					cmdSetScissor(
						cmd, (uint32_t)(pcmd->ClipRect.x - pos.x), (uint32_t)(pcmd->ClipRect.y - pos.y),
						(uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x), (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y));

					size_t id = (size_t)pcmd->TextureId;
					if (id >= pUserInterface->mFontTextures.size())
					{
						uint32_t setIndex = (uint32_t)pUserInterface->mFontTextures.size() + (pUserInterface->frameIdx * pUserInterface->mMaxDynamicUIUpdatesPerBatch + pUserInterface->mDynamicUIUpdates);
						DescriptorData params[1] = {};
						params[0].pName = "uTex";
						params[0].ppTextures = (Texture**)&pcmd->TextureId;
						updateDescriptorSet(pUserInterface->pRenderer, setIndex, pUserInterface->pDescriptorSetTexture, 1, params);
						cmdBindDescriptorSet(cmd, setIndex, pUserInterface->pDescriptorSetTexture);
						++pUserInterface->mDynamicUIUpdates;
					}
					else
					{
						cmdBindDescriptorSet(cmd, (uint32_t)id, pUserInterface->pDescriptorSetTexture);
					}

					cmdDrawIndexed(cmd, pcmd->ElemCount, idx_offset, vtx_offset);
				}
				idx_offset += pcmd->ElemCount;
			}
			vtx_offset += (int)cmd_list->VtxBuffer.size();
		}

		pUserInterface->frameIdx = (pUserInterface->frameIdx + 1) % MAX_FRAMES;
	}

#if TOUCH_INPUT
	extern void drawVirtualJoystick(Cmd* pCmd, const float4* color);
	float4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	drawVirtualJoystick((Cmd*)pCmd, &color);
#endif
#endif
}

bool uiOnText(const wchar_t* pText)
{
#ifdef ENABLE_FORGE_UI
	ImGui::SetCurrentContext(pUserInterface->context);
	ImGuiIO& io = ImGui::GetIO();
	uint32_t len = (uint32_t)wcslen(pText);
	for (uint32_t i = 0; i < len; ++i)
		io.AddInputCharacter(pText[i]);

	return !ImGui::GetIO().WantCaptureMouse;
#else
	return false; 
#endif
}

bool uiOnButton(uint32_t button, bool press, const float2* pVec)
{
#ifdef ENABLE_FORGE_UI
	ImGui::SetCurrentContext(pUserInterface->context);
	ImGuiIO& io = ImGui::GetIO();
	pUserInterface->pMovePosition = pVec;

	switch (button)
	{
	case InputBindings::BUTTON_DPAD_LEFT: pUserInterface->mNavInputs[ImGuiNavInput_DpadLeft] = (float)press; break;
	case InputBindings::BUTTON_DPAD_RIGHT: pUserInterface->mNavInputs[ImGuiNavInput_DpadRight] = (float)press; break;
	case InputBindings::BUTTON_DPAD_UP: pUserInterface->mNavInputs[ImGuiNavInput_DpadUp] = (float)press; break;
	case InputBindings::BUTTON_DPAD_DOWN: pUserInterface->mNavInputs[ImGuiNavInput_DpadDown] = (float)press; break;
	case InputBindings::BUTTON_EAST: pUserInterface->mNavInputs[ImGuiNavInput_Cancel] = (float)press; break;
	case InputBindings::BUTTON_WEST: pUserInterface->mNavInputs[ImGuiNavInput_Menu] = (float)press; break;
	case InputBindings::BUTTON_NORTH: pUserInterface->mNavInputs[ImGuiNavInput_Input] = (float)press; break;
	case InputBindings::BUTTON_L1: pUserInterface->mNavInputs[ImGuiNavInput_FocusPrev] = (float)press; break;
	case InputBindings::BUTTON_R1: pUserInterface->mNavInputs[ImGuiNavInput_FocusNext] = (float)press; break;
	case InputBindings::BUTTON_L2: pUserInterface->mNavInputs[ImGuiNavInput_TweakSlow] = (float)press; break;
	case InputBindings::BUTTON_R2: pUserInterface->mNavInputs[ImGuiNavInput_TweakFast] = (float)press; break;
	case InputBindings::BUTTON_R3: if (!press) { pUserInterface->mActive = !pUserInterface->mActive; } break;
	case InputBindings::BUTTON_KEYSHIFTL:
	case InputBindings::BUTTON_KEYSHIFTR:
		io.KeyShift = press;
		break;
	case InputBindings::BUTTON_SOUTH:
	case InputBindings::BUTTON_MOUSE_RIGHT:
	case InputBindings::BUTTON_MOUSE_MIDDLE:
	case InputBindings::BUTTON_MOUSE_SCROLL_UP:
	case InputBindings::BUTTON_MOUSE_SCROLL_DOWN:
	{
		const float scrollScale = 0.25f; // This should maybe be customized by client?  1.f would scroll ~5 lines of txt according to ImGui doc.
		pUserInterface->mNavInputs[ImGuiNavInput_Activate] = (float)press;
		if (pUserInterface->pMovePosition)
		{
			if (InputBindings::BUTTON_SOUTH == button)
				io.MouseDown[0] = press;
			else if (InputBindings::BUTTON_MOUSE_RIGHT == button)
				io.MouseDown[1] = press;
			else if (InputBindings::BUTTON_MOUSE_MIDDLE == button)
				io.MouseDown[2] = press;
			else if (InputBindings::BUTTON_MOUSE_SCROLL_UP == button)
				io.MouseWheel = 1.f * scrollScale;
			else if (InputBindings::BUTTON_MOUSE_SCROLL_DOWN == button)
				io.MouseWheel = -1.f * scrollScale;

		}
		if (!pUserInterface->mActive)
			return true;
		if (io.MousePos.x != -FLT_MAX && io.MousePos.y != -FLT_MAX)
		{
			return !(io.WantCaptureMouse);
		}
		else if (pUserInterface->pMovePosition)
		{
			io.MousePos = *pUserInterface->pMovePosition;
			for (uint32_t i = 0; i < pUserInterface->mLastUpdateCount; ++i)
			{
				if (ImGui::IsMouseHoveringRect(pUserInterface->mLastUpdateMin[i], pUserInterface->mLastUpdateMax[i], false))
				{
					io.WantCaptureMouse = true;
					return false;
				}
			}
			return true;
		}
		break; 
	}

	// Note that for keyboard keys, we only set them to true here if they are pressed because we may have a press/release
	// happening in one frame and it would never get registered.  Instead, unpressed are deferred at the end of update().
	// This scenario occurs with mobile soft (on-screen) keyboards.
	case InputBindings::BUTTON_BACK:
		if (press)
			io.KeysDown[InputBindings::BUTTON_BACK] = true;
		pUserInterface->mPostUpdateKeyDownStates[InputBindings::BUTTON_BACK] = press;
		break;
	case InputBindings::BUTTON_KEYLEFT:
		if (press)
			io.KeysDown[InputBindings::BUTTON_KEYLEFT] = true;
		pUserInterface->mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYLEFT] = press;
		break;
	case InputBindings::BUTTON_KEYRIGHT:
		if (press)
			io.KeysDown[InputBindings::BUTTON_KEYRIGHT] = true;
		pUserInterface->mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYRIGHT] = press;
		break;
	case InputBindings::BUTTON_KEYHOME:
		if (press)
			io.KeysDown[InputBindings::BUTTON_KEYHOME] = true;
		pUserInterface->mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYHOME] = press;
		break;
	case InputBindings::BUTTON_KEYEND:
		if (press)
			io.KeysDown[InputBindings::BUTTON_KEYEND] = true;
		pUserInterface->mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYEND] = press;
		break;
	case InputBindings::BUTTON_KEYDELETE:
		if (press)
			io.KeysDown[InputBindings::BUTTON_KEYDELETE] = true;
		pUserInterface->mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYDELETE] = press;
		break;

	default:
		break;
	}

	return false;
#else
	return true;
#endif
}

bool uiOnStick(uint32_t stick, const float2* pStick)
{
#ifdef ENABLE_FORGE_UI
	ImGui::SetCurrentContext(pUserInterface->context);
	
	switch (stick)
	{
	case InputBindings::FLOAT_LEFTSTICK:
	{
		ASSERT(pStick);
		const float2 vec = *pStick;
		if (vec.x < 0.f)
			pUserInterface->mNavInputs[ImGuiNavInput_LStickLeft] = abs(vec.x);
		else if (vec.x > 0.f)
			pUserInterface->mNavInputs[ImGuiNavInput_LStickRight] = vec.x;
		else
		{
			pUserInterface->mNavInputs[ImGuiNavInput_LStickLeft] = 0.f;
			pUserInterface->mNavInputs[ImGuiNavInput_LStickRight] = 0.f;
		}

		if (vec.y < 0.f)
			pUserInterface->mNavInputs[ImGuiNavInput_LStickDown] = abs(vec.y);
		else if (vec.y > 0.f)
			pUserInterface->mNavInputs[ImGuiNavInput_LStickUp] = vec.y;
		else
		{
			pUserInterface->mNavInputs[ImGuiNavInput_LStickDown] = 0.f;
			pUserInterface->mNavInputs[ImGuiNavInput_LStickUp] = 0.f;
		}

		break;
	}
	}

	return false;
#else
	return true;
#endif
}

bool uiOnInput(uint32_t binding, bool buttonPress, const float2* pMousePos, const float2* pStick)
{
#ifdef ENABLE_FORGE_UI
	if (binding <= InputBindings::FLOAT_BINDINGS_END)
		return uiOnStick(binding, pStick);
	
	return uiOnButton(binding, buttonPress, pMousePos);
#else
return true;
#endif
}

uint8_t uiWantTextInput()
{
#ifdef ENABLE_FORGE_UI
	ImGui::SetCurrentContext(pUserInterface->context);
	//The User flags are not what I expect them to be.
	//We need access to Per-Component InputFlags
	ImGuiContext*       guiContext = (ImGuiContext*)pUserInterface->context;
	ImGuiInputTextFlags currentInputFlags = guiContext->InputTextState.UserFlags;

	//0 -> Not pressed
	//1 -> Digits Only keyboard
	//2 -> Full Keyboard (Chars + Digits)
	int inputState = ImGui::GetIO().WantTextInput ? 2 : 0;
	//keyboard only Numbers
	if (inputState > 0 && (currentInputFlags & ImGuiInputTextFlags_CharsDecimal))
	{
		inputState = 1;
	}

	return inputState;
#else
	return 0; 
#endif
}

bool uiIsFocused()
{
#ifdef ENABLE_FORGE_UI
	ImGui::SetCurrentContext(pUserInterface->context);
	ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureMouse || io.WantCaptureKeyboard || io.NavActive || io.NavVisible;
#else
	return false; 
#endif
}
