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

#include "../../Middleware_3/Text/Fontstash.h"

#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui.h"
#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui_internal.h"

#include "AppUI.h"

#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"

#include "../../Common_3/OS/Core/RingBuffer.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"    //NOTE: this should be the last include in a .cpp

#define LABELID(prop) tinystl::string::format("##%llu", (uint64_t)(prop.pData))
#define LABELID1(prop) tinystl::string::format("##%llu", (uint64_t)(prop))

namespace ImGui {
bool SliderFloatWithSteps(const char* label, float* v, float v_min, float v_max, float v_step, const char* display_format)
{
	if (!display_format)
		display_format = "%.1f";

	tinystl::string text_buf = tinystl::string::format(display_format, *v);
	bool            value_changed = false;

	if (ImGui::GetIO().WantTextInput)
	{
		value_changed = ImGui::SliderFloat(label, v, v_min, v_max, text_buf.c_str());

		int v_i = int(((*v - v_min) / v_step) + 0.5f);
		*v = v_min + float(v_i) * v_step;
	}
	else
	{
		// Map from [v_min,v_max] to [0,N]
		const int countValues = int((v_max - v_min) / v_step);
		int       v_i = int(((*v - v_min) / v_step) + 0.5f);
		value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf.c_str());

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
	if (!display_format)
		display_format = "%d";

	tinystl::string text_buf = tinystl::string::format(display_format, *v);
	bool            value_changed = false;

	if (ImGui::GetIO().WantTextInput)
	{
		value_changed = ImGui::SliderInt(label, v, v_min, v_max, text_buf.c_str());

		int32_t v_i = int((*v - v_min) / v_step);
		*v = v_min + int32_t(v_i) * v_step;
	}
	else
	{
		// Map from [v_min,v_max] to [0,N]
		const int countValues = int((v_max - v_min) / v_step);
		int32_t   v_i = int((*v - v_min) / v_step);
		value_changed = ImGui::SliderInt(label, &v_i, 0, countValues, text_buf.c_str());

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

class ImguiGUIDriver: public GUIDriver
{
	public:
	// Declare virtual destructor
	virtual ~ImguiGUIDriver() {}
	bool init(Renderer* pRenderer);
	void exit();

	bool load(Fontstash* fontID, float fontSize, Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400);
	void unload();

	void* getContext();

	bool update(float deltaTime, GuiComponent** pGuiComponents, uint32_t componentCount, bool showDemoWindow);
	void draw(Cmd* q);

	void onInput(const ButtonData* data);
	bool isHovering(const float4& windowRect);
	int  needsTextInput() const;

	static void* alloc_func(size_t size, void* user_data) { return conf_malloc(size); }

	static void dealloc_func(void* ptr, void* user_data) { conf_free(ptr); }

	protected:
	static const uint32_t MAX_FRAMES = 3;
	ImGuiContext*         context;
	Texture*              pFontTexture;
	float2                dpiScale;
	bool                  loaded;
	uint32_t              frameIdx;

	using PipelineMap = tinystl::unordered_map<uint64_t, Pipeline*>;

	Renderer*          pRenderer;
	Shader*            pShaderTextured;
	RootSignature*     pRootSignatureTextured;
	PipelineMap        mPipelinesTextured;
	MeshRingBuffer*    pPlainMeshRingBuffer[MAX_FRAMES];
	UniformRingBuffer* pRingBuffer[MAX_FRAMES];
	/// Default states
	BlendState*      pBlendAlpha;
	DepthState*      pDepthState;
	RasterizerState* pRasterizerState;
	Sampler*         pDefaultSampler;
	VertexLayout     mVertexLayoutTextured = {};
};

void initGUIDriver(Renderer* pRenderer, GUIDriver** ppDriver)
{
	ImguiGUIDriver* pDriver = conf_placement_new<ImguiGUIDriver>(conf_calloc(1, sizeof(ImguiGUIDriver)));
	pDriver->init(pRenderer);
	*ppDriver = pDriver;
}

void removeGUIDriver(GUIDriver* pDriver)
{
	pDriver->exit();
	(reinterpret_cast<ImguiGUIDriver*>(pDriver))->~ImguiGUIDriver();
	conf_free(pDriver);
}

static float4 ToFloat4Color(uint color)
{
	float4 col;    // Translate colours back by bit shifting
	col.x = (float)((color & 0xFF000000) >> 24);
	col.y = (float)((color & 0x00FF0000) >> 16);
	col.z = (float)((color & 0x0000FF00) >> 8);
	col.w = (float)(color & 0x000000FF);
	return col;
}
static uint ToUintColor(float4 color)
{
	uint c = (((uint)color.x << 24) & 0xFF000000) | (((uint)color.y << 16) & 0x00FF0000) | (((uint)color.z << 8) & 0x0000FF00) |
			 (((uint)color.w) & 0x000000FF);
	return c;
}

void IWidget::ProcessCallbacks()
{
	if (pOnHover && ImGui::IsItemHovered())
		pOnHover();

	if (pOnActive && ImGui::IsItemActive())
		pOnActive();

	if (pOnFocus && ImGui::IsItemFocused())
		pOnFocus();

	if (pOnEdited && ImGui::IsItemEdited())
		pOnEdited();

	if (pOnDeactivated && ImGui::IsItemDeactivated())
		pOnDeactivated();

	if (pOnDeactivatedAfterEdit && ImGui::IsItemDeactivatedAfterEdit())
		pOnDeactivatedAfterEdit();
}

void CollapsingHeaderWidget::Draw()
{
	if (mPreviousCollapsed != mCollapsed)
	{
		ImGui::SetNextTreeNodeOpen(!mCollapsed);
		mPreviousCollapsed = mCollapsed;
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
	if (mDefaultOpen)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	if (ImGui::CollapsingHeader(mLabel.c_str(), flags))
	{
		for (IWidget* widget : mGroupedWidgets)
			widget->Draw();
	}

	ProcessCallbacks();
}

void LabelWidget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	ProcessCallbacks();
}

void SeparatorWidget::Draw()
{
	ImGui::Separator();
	ProcessCallbacks();
}

void ButtonWidget::Draw()
{
	ImGui::Button(mLabel.c_str());
	ProcessCallbacks();
}

void SliderFloatWidget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	ImGui::SliderFloatWithSteps(LABELID1(pData), pData, mMin, mMax, mStep, mFormat);
	ProcessCallbacks();
}

void SliderFloat2Widget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	for (uint32_t i = 0; i < 2; ++i)
	{
		ImGui::SliderFloatWithSteps(LABELID1(&pData->operator[](i)), &pData->operator[](i), mMin[i], mMax[i], mStep[i], mFormat);
		ProcessCallbacks();
	}
}

void SliderFloat3Widget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	for (uint32_t i = 0; i < 3; ++i)
	{
		ImGui::SliderFloatWithSteps(LABELID1(&pData->operator[](i)), &pData->operator[](i), mMin[i], mMax[i], mStep[i], mFormat);
		ProcessCallbacks();
	}
}

void SliderFloat4Widget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	for (uint32_t i = 0; i < 4; ++i)
	{
		ImGui::SliderFloatWithSteps(LABELID1(&pData->operator[](i)), &pData->operator[](i), mMin[i], mMax[i], mStep[i], mFormat);
		ProcessCallbacks();
	}
}

void SliderIntWidget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	ImGui::SliderIntWithSteps(LABELID1(pData), pData, mMin, mMax, mStep, mFormat);
	ProcessCallbacks();
}

void SliderUintWidget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	ImGui::SliderIntWithSteps(LABELID1(pData), (int32_t*)pData, (int32_t)mMin, (int32_t)mMax, (int32_t)mStep, mFormat);
	ProcessCallbacks();
}

void RadioButtonWidget::Draw()
{
	ImGui::RadioButton(mLabel.c_str(), pData, mRadioId);
	ProcessCallbacks();
}

void CheckboxWidget::Draw()
{
	ImGui::Text("%s", mLabel.c_str());
	ImGui::Checkbox(LABELID1(pData), pData);
	ProcessCallbacks();
}

void DropdownWidget::Draw()
{
	uint32_t& current = *pData;
	ImGui::Text("%s", mLabel.c_str());
	if (ImGui::BeginCombo(LABELID1(pData), mNames[current]))
	{
		for (uint32_t i = 0; i < (uint32_t)mNames.size(); ++i)
		{
			if (ImGui::Selectable(mNames[i]))
			{
				uint32_t prevVal = current;
				current = i;

				// Note that callbacks are sketchy with BeginCombo/EndCombo, so we manually process them here
				if (pOnEdited)
					pOnEdited();

				if (current != prevVal)
				{
					if (pOnDeactivatedAfterEdit)
						pOnDeactivatedAfterEdit();
				}
			}
		}
		ImGui::EndCombo();
	}
}

void ProgressBarWidget::Draw()
{
	size_t currProgress = *pData;
	ImGui::Text("%s", mLabel.c_str());
	ImGui::ProgressBar((float)currProgress / mMaxProgress);
	ProcessCallbacks();
}

void ColorSliderWidget::Draw()
{
	uint&  colorPick = *(uint*)pData;
	float4 combo_color = ToFloat4Color(colorPick) / 255.0f;

	float col[4] = { combo_color.x, combo_color.y, combo_color.z, combo_color.w };
	ImGui::Text("%s", mLabel.c_str());
	if (ImGui::ColorEdit4(LABELID1(pData), col, ImGuiColorEditFlags_AlphaPreview))
	{
		if (col[0] != combo_color.x || col[1] != combo_color.y || col[2] != combo_color.z || col[3] != combo_color.w)
		{
			combo_color = col;
			colorPick = ToUintColor(combo_color * 255.0f);
		}
	}
	ProcessCallbacks();
}

void ColorPickerWidget::Draw()
{
	uint&  colorPick = *(uint*)pData;
	float4 combo_color = ToFloat4Color(colorPick) / 255.0f;

	float col[4] = { combo_color.x, combo_color.y, combo_color.z, combo_color.w };
	ImGui::Text("%s", mLabel.c_str());
	if (ImGui::ColorPicker4(LABELID1(pData), col, ImGuiColorEditFlags_AlphaPreview))
	{
		if (col[0] != combo_color.x || col[1] != combo_color.y || col[2] != combo_color.z || col[3] != combo_color.w)
		{
			combo_color = col;
			colorPick = ToUintColor(combo_color * 255.0f);
		}
	}
	ProcessCallbacks();
}

void TextboxWidget::Draw()
{
	ImGui::InputText(LABELID1(pData), (char*)pData, mLength, mAutoSelectAll ? ImGuiInputTextFlags_AutoSelectAll : 0);
	ProcessCallbacks();
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
}

bool ImguiGUIDriver::init(Renderer* renderer)
{
	mHandledGestures = false;
	pRenderer = renderer;
	loaded = false;
	/************************************************************************/
	// Rendering resources
	/************************************************************************/
	SamplerDesc samplerDesc = { FILTER_LINEAR,
								FILTER_LINEAR,
								MIPMAP_MODE_NEAREST,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE,
								ADDRESS_MODE_CLAMP_TO_EDGE };
	addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

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

	ShaderLoadDesc texturedShaderDesc = {};
	texturedShaderDesc.mStages[0] = { "imgui.vert", NULL, 0, FSR_MIDDLEWARE_UI };
	texturedShaderDesc.mStages[1] = { "imgui.frag", NULL, 0, FSR_MIDDLEWARE_UI };
	addShader(pRenderer, &texturedShaderDesc, &pShaderTextured);

	const char*       pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pShaderTextured, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pDefaultSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignatureTextured);

	BufferDesc vbDesc = {};
	vbDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mVertexStride = sizeof(ImDrawVert);
	vbDesc.mSize = 1024 * 64 * vbDesc.mVertexStride;
	vbDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;

	BufferDesc ibDesc = vbDesc;
	ibDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	ibDesc.mIndexType = INDEX_TYPE_UINT16;
	ibDesc.mSize = 128 * 1024 * sizeof(ImDrawIdx);
	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
	{
		addMeshRingBuffer(pRenderer, &vbDesc, &ibDesc, &pPlainMeshRingBuffer[i]);
		addUniformRingBuffer(pRenderer, 256, &pRingBuffer[i]);
	}

	mVertexLayoutTextured.mAttribCount = 3;
	mVertexLayoutTextured.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	mVertexLayoutTextured.mAttribs[0].mFormat = ImageFormat::RG32F;
	mVertexLayoutTextured.mAttribs[0].mBinding = 0;
	mVertexLayoutTextured.mAttribs[0].mLocation = 0;
	mVertexLayoutTextured.mAttribs[0].mOffset = 0;
	mVertexLayoutTextured.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
	mVertexLayoutTextured.mAttribs[1].mFormat = ImageFormat::RG32F;
	mVertexLayoutTextured.mAttribs[1].mBinding = 0;
	mVertexLayoutTextured.mAttribs[1].mLocation = 1;
	mVertexLayoutTextured.mAttribs[1].mOffset = calculateImageFormatStride(mVertexLayoutTextured.mAttribs[0].mFormat);
	mVertexLayoutTextured.mAttribs[2].mSemantic = SEMANTIC_COLOR;
	mVertexLayoutTextured.mAttribs[2].mFormat = ImageFormat::RGBA8;
	mVertexLayoutTextured.mAttribs[2].mBinding = 0;
	mVertexLayoutTextured.mAttribs[2].mLocation = 2;
	mVertexLayoutTextured.mAttribs[2].mOffset =
		mVertexLayoutTextured.mAttribs[1].mOffset + calculateImageFormatStride(mVertexLayoutTextured.mAttribs[1].mFormat);
	/************************************************************************/
	/************************************************************************/
	dpiScale = getDpiScale();

	//// init UI (input)
	ImGui::SetAllocatorFunctions(alloc_func, dealloc_func);
	context = ImGui::CreateContext();
	ImGui::SetCurrentContext(context);

	return true;
}

void ImguiGUIDriver::exit()
{
	for (PipelineMap::iterator it = mPipelinesTextured.begin(); it != mPipelinesTextured.end(); ++it)
	{
		removePipeline(pRenderer, it.node->second);
	}

	mPipelinesTextured.clear();

	removeSampler(pRenderer, pDefaultSampler);
	removeBlendState(pBlendAlpha);
	removeDepthState(pDepthState);
	removeRasterizerState(pRasterizerState);
	removeShader(pRenderer, pShaderTextured);
	removeRootSignature(pRenderer, pRootSignatureTextured);
	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
	{
		removeMeshRingBuffer(pPlainMeshRingBuffer[i]);
		removeUniformRingBuffer(pRingBuffer[i]);
	}
}

bool ImguiGUIDriver::load(Fontstash* fontstash, float fontSize, Texture* cursorTexture, float uiwidth, float uiheight)
{
	if (!loaded)
	{
		// Build and load the texture atlas into a texture
		// (In the examples/ app this is usually done within the ImGui_ImplXXX_Init() function from one of the demo Renderer)
		int            width, height;
		unsigned char* pixels = NULL;
		ImFontConfig   config = {};
		config.FontDataOwnedByAtlas = false;
		ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
			fontstash->getFontBuffer("default"), fontstash->getFontBufferSize("default"), fontSize * min(dpiScale.x, dpiScale.y), &config);
		if (font != NULL)
		{
			ImGui::GetIO().FontDefault = font;
		}
		else
		{
			ImGui::GetIO().Fonts->AddFontDefault();
		}
		ImGui::GetIO().Fonts->Build();
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		// At this point you've got the texture data and you need to upload that your your graphic system:
		// After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
		// This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
		Image image = {};
		image.RedefineDimensions(ImageFormat::RGBA8, width, height, 1, 1, 1);
		image.SetPixels(pixels);
		TextureLoadDesc loadDesc = {};
		loadDesc.pImage = &image;
		loadDesc.ppTexture = &pFontTexture;
		addResource(&loadDesc);
		ImGui::GetIO().Fonts->TexID = (void*)pFontTexture;

		SetDefaultStyle();

		ImGuiIO& io = ImGui::GetIO();
		//io.KeyMap[ImGuiKey_Tab] = VK_TAB;
		//io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
		//io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
		//io.KeyMap[ImGuiKey_Home] = VK_HOME;
		//io.KeyMap[ImGuiKey_End] = VK_END;
		//io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
		//io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
		io.KeyMap[ImGuiKey_LeftArrow] = KEY_PAD_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = KEY_PAD_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = KEY_PAD_UP;
		io.KeyMap[ImGuiKey_DownArrow] = KEY_PAD_DOWN;
		io.KeyMap[ImGuiKey_Backspace] = KEY_RIGHT_STICK_BUTTON;
		io.KeyMap[ImGuiKey_Delete] = KEY_DELETE;
		io.KeyMap[ImGuiKey_Space] = KEY_LEFT_TRIGGER;
		io.KeyMap[ImGuiKey_Enter] = KEY_MENU;
		io.KeyMap[ImGuiKey_Escape] = KEY_CANCEL;
		io.KeyMap[ImGuiKey_A] = 'A';
		io.KeyMap[ImGuiKey_C] = 'C';
		io.KeyMap[ImGuiKey_V] = 'V';
		io.KeyMap[ImGuiKey_X] = 'X';
		io.KeyMap[ImGuiKey_Y] = 'Y';
		io.KeyMap[ImGuiKey_Z] = 'Z';

		loaded = true;
	}
	return true;
}

void ImguiGUIDriver::unload()
{
	if (pFontTexture)
	{
		removeResource(pFontTexture);
	}
	ImGui::DestroyContext(context);
}

void* ImguiGUIDriver::getContext() { return context; }

void ImguiGUIDriver::onInput(const ButtonData* data)
{
	ImGui::SetCurrentContext(context);
	ImGuiIO& io = ImGui::GetIO();

	if (GAINPUT_GAMEPAD & data->mActiveDevicesMask)
	{
		io.NavActive = true;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
		return;
	}

	io.NavActive = false;

	// Always handle gestures first
	if (GESTURE_SWIPE_2 == data->mUserId)
	{
		// Divide by extra multiple that ImGUI applies when using the MouseWheel values
		// We do that because swiping is different than mousewheel and matches exactly pixels
		// Removing this division will cause the scroll to not match the fingers movement.
		float scroll_amount = 1.0f;
		if (context->HoveredWindow)
			scroll_amount = 5.f * context->HoveredWindow->CalcFontSize();
		io.MouseWheel += data->mValue[INPUT_Y_AXIS] * getDpiScale().y / scroll_amount;

		mHandledGestures = true;

		for (int i = 0; i < 5; i++)
		{
			io.MouseDown[0] = false;
		}
	}

	if (mHandledGestures)
		return;

	if (data->mUserId == KEY_UI_MOVE)
	{
		io.MousePos = float2((float)data->mValue[0], (float)data->mValue[1]);
	}
	else if (data->mUserId == KEY_CONFIRM || data->mUserId == KEY_RIGHT_BUMPER || data->mUserId == KEY_MOUSE_WHEEL_BUTTON)
	{
		uint32_t index = data->mUserId == KEY_CONFIRM ? 0 : (data->mUserId == KEY_RIGHT_BUMPER ? 1 : 2);
		io.MouseDown[index] = data->mIsPressed;
		io.MousePos = float2((float)data->mValue[0], (float)data->mValue[1]);
	}
	else if (KEY_MOUSE_WHEEL == data->mUserId)
	{
		io.MouseWheel += data->mValue[0];
	}
	else if (KEY_CHAR == data->mUserId)
	{
		if (data->mIsPressed && needsTextInput())
			io.AddInputCharacter(data->mCharacter);
	}
	else if (KEY_LEFT_STICK == data->mUserId)
	{
		io.KeysDown[(int)'A'] = data->mIsPressed;
	}
	else if (KEY_RIGHT_STICK_BUTTON == data->mUserId || KEY_DELETE == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
	}
	else if (KEY_RIGHT_CTRL == data->mUserId || KEY_LEFT_CTRL == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
		io.KeyCtrl = data->mIsPressed;
	}
	else if (KEY_RIGHT_SHIFT == data->mUserId || KEY_LEFT_BUMPER == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
		io.KeyShift = data->mIsPressed;
	}
	else if (KEY_RIGHT_SUPER == data->mUserId || KEY_LEFT_SUPER == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
		io.KeySuper = data->mIsPressed;
	}
	else if (
		KEY_PAD_LEFT == data->mUserId || KEY_PAD_RIGHT == data->mUserId || KEY_PAD_UP == data->mUserId || KEY_PAD_DOWN == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
	}
	else if (KEY_MENU == data->mUserId)
	{
		io.KeysDown[data->mUserId] = data->mIsPressed;
	}
}

bool ImguiGUIDriver::isHovering(const float4& windowRect)
{
	return ImGui::IsMouseHoveringRect(
		float2(windowRect.x, windowRect.y), float2(windowRect.x + windowRect.z, windowRect.y + windowRect.w), false);
}

bool ImguiGUIDriver::update(float deltaTime, GuiComponent** pGuiComponents, uint32_t componentCount, bool showDemoWindow)
{
	ImGui::SetCurrentContext(context);
	// #TODO: Use window size as render-target size cannot be trusted to be the same as window size
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = (float)InputSystem::GetDisplayWidth();
	io.DisplaySize.y = (float)InputSystem::GetDisplayHeight();
	io.DeltaTime = deltaTime;

	if (io.NavActive)
	{
		io.NavInputs[ImGuiNavInput_Activate] = InputSystem::GetButtonData(KEY_CONFIRM).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_Cancel] = InputSystem::GetButtonData(KEY_CANCEL).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_Menu] = InputSystem::GetButtonData(KEY_BUTTON_X).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_Input] = InputSystem::GetButtonData(KEY_BUTTON_Y).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_DpadLeft] = InputSystem::GetButtonData(KEY_PAD_LEFT).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_DpadRight] = InputSystem::GetButtonData(KEY_PAD_RIGHT).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_DpadUp] = InputSystem::GetButtonData(KEY_PAD_UP).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_DpadDown] = InputSystem::GetButtonData(KEY_PAD_DOWN).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_FocusNext] = InputSystem::GetButtonData(KEY_RIGHT_BUMPER).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_FocusPrev] = InputSystem::GetButtonData(KEY_LEFT_BUMPER).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_TweakFast] = InputSystem::GetButtonData(KEY_RIGHT_BUMPER).mIsPressed ? 1.0f : 0.0f;
		io.NavInputs[ImGuiNavInput_TweakSlow] = InputSystem::GetButtonData(KEY_LEFT_BUMPER).mIsPressed ? 1.0f : 0.0f;
	}

	ImGui::NewFrame();

	if (showDemoWindow)
		ImGui::ShowDemoWindow();

	bool ret = false;

	for (uint32_t compIndex = 0; compIndex < componentCount; ++compIndex)
	{
		GuiComponent*                           pComponent = pGuiComponents[compIndex];
		tinystl::string                         title = pComponent->mTitle;
		int32_t                                 guiComponentFlags = pComponent->mFlags;
		bool*                                   pCloseButtonActiveValue = pComponent->mHasCloseButton ? &pComponent->mHasCloseButton : NULL;
		const tinystl::vector<tinystl::string>& contextualMenuLabels = pComponent->mContextualMenuLabels;
		const tinystl::vector<WidgetCallback>&  contextualMenuCallbacks = pComponent->mContextualMenuCallbacks;
		const float4&                           windowRect = pComponent->mInitialWindowRect;
		float4&                                 currentWindowRect = pComponent->mCurrentWindowRect;
		IWidget**                               pProps = pComponent->mWidgets.data();
		uint32_t                                propCount = (uint32_t)pComponent->mWidgets.size();

		if (title == "")
			title = tinystl::string::format("##%llu", (uint64_t)pComponent);
		// Setup the ImGuiWindowFlags
		ImGuiWindowFlags guiWinFlags = GUI_COMPONENT_FLAGS_NONE;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_TITLE_BAR)
			guiWinFlags |= ImGuiWindowFlags_NoTitleBar;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE)
			guiWinFlags |= ImGuiWindowFlags_NoResize;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
			guiWinFlags |= ImGuiWindowFlags_NoMove;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_SCROLLBAR)
			guiWinFlags |= ImGuiWindowFlags_NoScrollbar;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_COLLAPSE)
			guiWinFlags |= ImGuiWindowFlags_NoCollapse;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE)
			guiWinFlags |= ImGuiWindowFlags_AlwaysAutoResize;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_INPUTS)
			guiWinFlags |= ImGuiWindowFlags_NoInputs;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_MEMU_BAR)
			guiWinFlags |= ImGuiWindowFlags_MenuBar;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_HORIZONTAL_SCROLLBAR)
			guiWinFlags |= ImGuiWindowFlags_HorizontalScrollbar;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_FOCUS_ON_APPEARING)
			guiWinFlags |= ImGuiWindowFlags_NoFocusOnAppearing;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_BRING_TO_FRONT_ON_FOCUS)
			guiWinFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_VERTICAL_SCROLLBAR)
			guiWinFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_HORIZONTAL_SCROLLBAR)
			guiWinFlags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_USE_WINDOW_PADDING)
			guiWinFlags |= ImGuiWindowFlags_AlwaysUseWindowPadding;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_INPUT)
			guiWinFlags |= ImGuiWindowFlags_NoNavInputs;
		if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_NAV_FOCUS)
			guiWinFlags |= ImGuiWindowFlags_NoNavFocus;

		bool result = ImGui::Begin(title, pCloseButtonActiveValue, guiWinFlags);
		if (result)
		{
			// Setup the contextual menus
			if (!contextualMenuLabels.empty() && ImGui::BeginPopupContextItem())    // <-- This is using IsItemHovered()
			{
				for (size_t i = 0; i < contextualMenuLabels.size(); i++)
				{
					if (ImGui::MenuItem(contextualMenuLabels[i].c_str()))
					{
						if (i < contextualMenuCallbacks.size())
							contextualMenuCallbacks[i]();
					}
				}
				ImGui::EndPopup();
			}

			bool overrideSize = false;
			bool overridePos = false;

			if ((guiComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE) && !(guiComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE))
				overrideSize = true;

			if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
				overridePos = true;

			ImGui::SetWindowSize(
				float2(windowRect.z * dpiScale.x, windowRect.w * dpiScale.y), overrideSize ? ImGuiCond_Always : ImGuiCond_Once);
			ImGui::SetWindowPos(
				float2(windowRect.x * dpiScale.x, windowRect.y * dpiScale.y), overridePos ? ImGuiCond_Always : ImGuiCond_Once);

			float2 min = ImGui::GetWindowPos();
			float2 max = ImGui::GetWindowSize();
			currentWindowRect.x = min.x;
			currentWindowRect.y = min.y;
			currentWindowRect.z = max.x;
			currentWindowRect.w = max.y;

			for (uint32_t i = 0; i < propCount; ++i)
				if (pProps[i])
					pProps[i]->Draw();

			if (!ret)
				ret = ImGui::GetIO().WantCaptureMouse;
		}
		// Need to call ImGui::End event if result is false since we called ImGui::Begin
		ImGui::End();
	}

	ImGui::EndFrame();

	// Flush key down array since onInput will apply the right states again next frame
	for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++)
		io.KeysDown[i] = 0;

	mHandledGestures = false;

	return ret;
}

void ImguiGUIDriver::draw(Cmd* pCmd)
{
	/************************************************************************/
	/************************************************************************/
	ImGui::Render();
	frameIdx = (frameIdx + 1) % MAX_FRAMES;
	resetMeshRingBuffer(pPlainMeshRingBuffer[frameIdx]);

	ImDrawData* draw_data = ImGui::GetDrawData();

	Pipeline*            pPipeline = NULL;
	GraphicsPipelineDesc pipelineDesc = {};
	pipelineDesc.mDepthStencilFormat = (ImageFormat::Enum)pCmd->mBoundDepthStencilFormat;
	pipelineDesc.mRenderTargetCount = pCmd->mBoundRenderTargetCount;
	pipelineDesc.mSampleCount = pCmd->mBoundSampleCount;
	pipelineDesc.pBlendState = pBlendAlpha;
	pipelineDesc.mSampleQuality = pCmd->mBoundSampleQuality;
	pipelineDesc.pColorFormats = (ImageFormat::Enum*)pCmd->pBoundColorFormats;
	pipelineDesc.pDepthState = pDepthState;
	pipelineDesc.pRasterizerState = pRasterizerState;
	pipelineDesc.pSrgbValues = pCmd->pBoundSrgbValues;
	PipelineMap::iterator it = mPipelinesTextured.find(pCmd->mRenderPassHash);
	if (it == mPipelinesTextured.end())
	{
		pipelineDesc.pRootSignature = pRootSignatureTextured;
		pipelineDesc.pShaderProgram = pShaderTextured;
		pipelineDesc.pVertexLayout = &mVertexLayoutTextured;
		pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		addPipeline(pCmd->pRenderer, &pipelineDesc, &pPipeline);
		mPipelinesTextured.insert({ pCmd->mRenderPassHash, pPipeline });
	}
	else
	{
		pPipeline = it.node->second;
	}
	uint32_t vSize = 0;
	uint32_t iSize = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		vSize += (int)(cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
		iSize += (int)(cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));
	}

	RingBufferOffset vOffset = getVertexBufferOffset(pPlainMeshRingBuffer[frameIdx], vSize);
	RingBufferOffset iOffset = getIndexBufferOffset(pPlainMeshRingBuffer[frameIdx], iSize);
	// Copy and convert all vertices into a single contiguous buffer
	uint64_t vtx_dst = vOffset.mOffset;
	uint64_t idx_dst = iOffset.mOffset;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		BufferUpdateDesc  update = { vOffset.pBuffer, cmd_list->VtxBuffer.data(), 0, vtx_dst,
                                    cmd_list->VtxBuffer.size() * sizeof(ImDrawVert) };
		updateResource(&update);
		update = { iOffset.pBuffer, cmd_list->IdxBuffer.data(), 0, idx_dst, cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx) };
		updateResource(&update);

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
	RingBufferOffset offset = getUniformBufferOffset(pRingBuffer[frameIdx], sizeof(mvp));
	BufferUpdateDesc update = { offset.pBuffer, mvp, 0, offset.mOffset, sizeof(mvp) };
	updateResource(&update);

	cmdSetViewport(pCmd, 0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
	cmdSetScissor(
		pCmd, (uint32_t)draw_data->DisplayPos.x, (uint32_t)draw_data->DisplayPos.y, (uint32_t)draw_data->DisplaySize.x,
		(uint32_t)draw_data->DisplaySize.y);
	cmdBindPipeline(pCmd, pPipeline);
	cmdBindIndexBuffer(pCmd, pPlainMeshRingBuffer[frameIdx]->pIndexBuffer, iOffset.mOffset);
	cmdBindVertexBuffer(pCmd, 1, &pPlainMeshRingBuffer[frameIdx]->pVertexBuffer, &vOffset.mOffset);

	DescriptorData params[1] = {};
	params[0].pName = "uniformBlockVS";
	params[0].pOffsets = &offset.mOffset;
	params[0].ppBuffers = &offset.pBuffer;
	cmdBindDescriptors(pCmd, pRootSignatureTextured, 1, params);

	// Render command lists
	int    vtx_offset = 0;
	int    idx_offset = 0;
	float2 pos = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
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
					pCmd, (uint32_t)(pcmd->ClipRect.x - pos.x), (uint32_t)(pcmd->ClipRect.y - pos.y),
					(uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x), (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y));

				DescriptorData params[1] = {};
				params[0].pName = "uTex";
				params[0].ppTextures = (Texture**)&pcmd->TextureId;
				cmdBindDescriptors(pCmd, pRootSignatureTextured, 1, params);
				cmdDrawIndexed(pCmd, pcmd->ElemCount, idx_offset, vtx_offset);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += (int)cmd_list->VtxBuffer.size();
	}
}

int ImguiGUIDriver::needsTextInput() const
{
	//The User flags are not what I expect them to be.
	//We need access to Per-Component InputFlags
	ImGuiContext*       guiContext = (ImGuiContext*)this->context;
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
}
