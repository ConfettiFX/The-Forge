/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#ifdef USE_UI_PRECOMPILED_SHADERS
#include "Shaders/Compiled/imgui.vert.h"
#include "Shaders/Compiled/imgui.frag.h"
#endif

#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui.h"
#include "../../Common_3/ThirdParty/OpenSource/imgui/imgui_internal.h"
#include "../../Common_3/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "AppUI.h"

#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/IInput.h"
#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/IResourceLoader.h"

#include "../../Common_3/OS/Interfaces/IMemory.h"    //NOTE: this should be the last include in a .cpp

#define LABELID(prop, buffer) sprintf(buffer, "##%llu", (unsigned long long)(prop.pData))
#define LABELID1(prop, buffer) sprintf(buffer, "##%llu", (unsigned long long)(prop))

#define MAX_LABEL_LENGTH 128

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

class ImguiGUIDriver
{
	public:

	~ImguiGUIDriver() {}
	bool init(Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch);
	void exit();

	bool load(RenderTarget** ppRts, uint32_t count, PipelineCache* pCache);
	void unload();

	bool addFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont);

	void* getContext();

	bool update(GUIDriverUpdate* pGuiUpdate);
	void draw(Cmd* q);
	
	bool onButton(uint32_t button, bool press, const float2* vec)
	{
		ImGui::SetCurrentContext(context);
		ImGuiIO& io = ImGui::GetIO();
		pMovePosition = vec;
		
		switch (button)
		{
		case InputBindings::BUTTON_DPAD_LEFT: mNavInputs[ImGuiNavInput_DpadLeft] = (float)press; break;
		case InputBindings::BUTTON_DPAD_RIGHT: mNavInputs[ImGuiNavInput_DpadRight] = (float)press; break;
		case InputBindings::BUTTON_DPAD_UP: mNavInputs[ImGuiNavInput_DpadUp] = (float)press; break;
		case InputBindings::BUTTON_DPAD_DOWN: mNavInputs[ImGuiNavInput_DpadDown] = (float)press; break;
		case InputBindings::BUTTON_EAST: mNavInputs[ImGuiNavInput_Cancel] = (float)press; break;
		case InputBindings::BUTTON_WEST: mNavInputs[ImGuiNavInput_Menu] = (float)press; break;
		case InputBindings::BUTTON_NORTH: mNavInputs[ImGuiNavInput_Input] = (float)press; break;
		case InputBindings::BUTTON_L1: mNavInputs[ImGuiNavInput_FocusPrev] = (float)press; break;
		case InputBindings::BUTTON_R1: mNavInputs[ImGuiNavInput_FocusNext] = (float)press; break;
		case InputBindings::BUTTON_L2: mNavInputs[ImGuiNavInput_TweakSlow] = (float)press; break;
		case InputBindings::BUTTON_R2: mNavInputs[ImGuiNavInput_TweakFast] = (float)press; break;
		case InputBindings::BUTTON_R3: if (!press) { mActive = !mActive; } break;
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
			mNavInputs[ImGuiNavInput_Activate] = (float)press;
			if (pMovePosition)
			{
				if (InputBindings::BUTTON_SOUTH == button)
					io.MouseDown[0] = press;
				else if (InputBindings::BUTTON_MOUSE_RIGHT == button)
					io.MouseDown[1] = press;
				else if (InputBindings::BUTTON_MOUSE_MIDDLE == button)
					io.MouseDown[2] = press;
				else if (InputBindings::BUTTON_MOUSE_SCROLL_UP == button)
					io.MouseWheel = 1.f * scrollScale;
				else if(InputBindings::BUTTON_MOUSE_SCROLL_DOWN == button)
					io.MouseWheel = -1.f * scrollScale;

			}
			if (!mActive)
				return true;
			if (io.MousePos.x != -FLT_MAX && io.MousePos.y != -FLT_MAX)
			{
				return !(io.WantCaptureMouse);
			}
			else if (pMovePosition)
			{
				io.MousePos = *pMovePosition;
				for (uint32_t i = 0; i < mLastUpdateCount; ++i)
				{
					if (ImGui::IsMouseHoveringRect(mLastUpdateMin[i], mLastUpdateMax[i], false))
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
			mPostUpdateKeyDownStates[InputBindings::BUTTON_BACK] = press;
			break;
		case InputBindings::BUTTON_KEYLEFT: 
			if (press)
				io.KeysDown[InputBindings::BUTTON_KEYLEFT] = true; 
			mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYLEFT] = press;
			break;
		case InputBindings::BUTTON_KEYRIGHT: 
			if (press)
				io.KeysDown[InputBindings::BUTTON_KEYRIGHT] = true;
			mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYRIGHT] = press;
			break;
		case InputBindings::BUTTON_KEYHOME:
			if (press)
				io.KeysDown[InputBindings::BUTTON_KEYHOME] = true;
			mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYHOME] = press;
			break;
		case InputBindings::BUTTON_KEYEND: 
			if (press)
				io.KeysDown[InputBindings::BUTTON_KEYEND] = true;
			mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYEND] = press;
			break;
		case InputBindings::BUTTON_KEYDELETE: 
			if (press)
				io.KeysDown[InputBindings::BUTTON_KEYDELETE] = true;
			mPostUpdateKeyDownStates[InputBindings::BUTTON_KEYDELETE] = press;
			break;

		default:
			break;
		}

		return false;
	}

	bool onText(const wchar_t* pText)
	{
		ImGui::SetCurrentContext(context);
		ImGuiIO& io = ImGui::GetIO();
		uint32_t len = (uint32_t)wcslen(pText);
		for (uint32_t i = 0; i < len; ++i)
			io.AddInputCharacter(pText[i]);

		return !ImGui::GetIO().WantCaptureMouse;
	}

	uint8_t wantTextInput() const
	{
		ImGui::SetCurrentContext(context);
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
	
	bool isFocused()
	{
		ImGui::SetCurrentContext(context);
		return ImGui::GetIO().WantCaptureMouse;
	}

	void setCustomShader(Shader* pShader)
	{
		pShaderTextured = pShader;
		mCustomShader = true;
	}

	static void* alloc_func(size_t size, void* user_data) { return tf_malloc(size); }

	static void dealloc_func(void* ptr, void* user_data) { tf_free(ptr); }

	protected:
	static const uint32_t MAX_FRAMES = 3;
	ImGuiContext*           context = NULL;
	eastl::vector<Texture*>  mFontTextures;
	float2                  dpiScale;
	uint32_t                frameIdx = 0;

	Renderer*          pRenderer = NULL;
	Shader*            pShaderTextured = NULL;
	RootSignature*     pRootSignatureTextured = NULL;
	DescriptorSet*     pDescriptorSetUniforms = NULL;
	DescriptorSet*     pDescriptorSetTexture = NULL;
	Pipeline*          pPipelineTextured = NULL;
	Buffer*            pVertexBuffer = NULL;
	Buffer*            pIndexBuffer = NULL;
	Buffer*            pUniformBuffer[MAX_FRAMES] = {};
	/// Default states
	Sampler*         pDefaultSampler = NULL;
	VertexLayout     mVertexLayoutTextured = {};
	uint32_t         mMaxDynamicUIUpdatesPerBatch = 0;
	uint32_t         mDynamicUIUpdates = 0;
	float            mNavInputs[ImGuiNavInput_COUNT] = {};
	const float2*    pMovePosition = NULL;
	uint32_t         mLastUpdateCount = 0;
	float2           mLastUpdateMin[64] = {};
	float2           mLastUpdateMax[64] = {};
	bool             mActive = false;
	bool             mCustomShader = false;
	bool             mPostUpdateKeyDownStates[512] = {};

	// Since gestures events always come first, we want to dismiss any other inputs after that
	bool mHandledGestures = false;
};

static const uint64_t VERTEX_BUFFER_SIZE = 1024 * 64 * sizeof(ImDrawVert);
static const uint64_t INDEX_BUFFER_SIZE = 128 * 1024 * sizeof(ImDrawIdx);

void allocGUIDriver(Renderer* pRenderer, void** ppDriver)
{
	ImguiGUIDriver* pDriver = tf_new(ImguiGUIDriver);
	*ppDriver = pDriver;
}

void freeGUIDriver(void* pDriver)
{
	tf_delete((ImguiGUIDriver*)pDriver);
}

bool initGUIDriver(void* pDriver, Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch)
{
	return ((ImguiGUIDriver*)pDriver)->init(pRenderer, maxDynamicUIUpdatesPerBatch);
}

void exitGUIDriver(void* pDriver)
{
	((ImguiGUIDriver*)pDriver)->exit();
}

bool loadGUIDriver(void* pDriver, RenderTarget** ppRts, uint32_t count, PipelineCache* pCache)
{
	return ((ImguiGUIDriver*)pDriver)->load(ppRts, count, pCache);
}

void unloadGUIDriver(void* pDriver)
{
	((ImguiGUIDriver*)pDriver)->unload();
}

void setGUIDriverCustomShader(void* pDriver, Shader* pShader)
{
	((ImguiGUIDriver*)pDriver)->setCustomShader(pShader);
}

void updateGUIDriver(void* pDriver, GUIDriverUpdate* pGuiUpdate)
{
	((ImguiGUIDriver*)pDriver)->update(pGuiUpdate);
}

void drawGUIDriver(void* pDriver, Cmd* pCmd)
{
	((ImguiGUIDriver*)pDriver)->draw(pCmd);
}

void addGUIDriverFont(void* pDriver, void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont)
{
	((ImguiGUIDriver*)pDriver)->addFont(pFontBuffer, fontBufferSize, pFontGlyphRanges, fontSize, pFont);
}

bool GUIDriverOnText(void* pDriver, const wchar_t* pText)
{
	return ((ImguiGUIDriver*)pDriver)->onText(pText);
}

bool GUIDriverOnButton(void* pDriver, uint32_t button, bool press, const float2* pVec)
{
	return ((ImguiGUIDriver*)pDriver)->onButton(button, press, pVec);
}

uint8_t GUIDriverWantTextInput(void* pDriver)
{
	return ((ImguiGUIDriver*)pDriver)->wantTextInput();
}

bool GUIDriverIsFocused(void* pDriver)
{
	return ((ImguiGUIDriver*)pDriver)->isFocused();
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

// IWidget public functions
void processWidgetCallbacks(IWidget* pWidget, bool deferred)
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
void drawCollapsingHeaderWidget(IWidget* pWidget)
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
		for (IWidget* widget : pOriginalWidget->mGroupedWidgets)
			drawWidget(widget);
	}

	processWidgetCallbacks(pWidget);
}

// DebugTexturesWidget private functions
void drawDebugTexturesWidget(IWidget* pWidget)
{
	DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);

	for (Texture* tex : pOriginalWidget->mTextures)
	{
		ImGui::Image(tex, pOriginalWidget->mTextureDisplaySize);
		ImGui::SameLine();
	}

	processWidgetCallbacks(pWidget);
}

// LabelWidget private functions
void drawLabelWidget(IWidget* pWidget)
{
	ImGui::Text("%s", pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// ColorLabelWidget private functions
void drawColorLabelWidget(IWidget* pWidget)
{
	ColorLabelWidget* pOriginalWidget = (ColorLabelWidget*)(pWidget->pWidget);

	ImGui::TextColored(pOriginalWidget->mColor, "%s", pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// HorizontalSpaceWidget private functions
void drawHorizontalSpaceWidget(IWidget* pWidget)
{
	ImGui::SameLine();
	processWidgetCallbacks(pWidget);
}

// SeparatorWidget private functions
void drawSeparatorWidget(IWidget* pWidget)
{
	ImGui::Separator();
	processWidgetCallbacks(pWidget);
}

// VerticalSeparatorWidget private functions
void drawVerticalSeparatorWidget(IWidget* pWidget)
{
	VerticalSeparatorWidget* pOriginalWidget = (VerticalSeparatorWidget*)(pWidget->pWidget);

	for (uint32_t i = 0; i < pOriginalWidget->mLineCount; ++i)
	{
		ImGui::VerticalSeparator();
	}

	processWidgetCallbacks(pWidget);
}

// ButtonWidget private functions
void drawButtonWidget(IWidget* pWidget)
{
	ImGui::Button(pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// SliderFloatWidget private functions
void drawSliderFloatWidget(IWidget* pWidget)
{
	SliderFloatWidget* pOriginalWidget = (SliderFloatWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::SliderFloatWithSteps(label, pOriginalWidget->pData, pOriginalWidget->mMin, pOriginalWidget->mMax, pOriginalWidget->mStep, pOriginalWidget->mFormat);
	processWidgetCallbacks(pWidget);
}

// SliderFloat2Widget private functions
void drawSliderFloat2Widget(IWidget* pWidget)
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
void drawSliderFloat3Widget(IWidget* pWidget)
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
void drawSliderFloat4Widget(IWidget* pWidget)
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
void drawSliderIntWidget(IWidget* pWidget)
{
	SliderIntWidget* pOriginalWidget = (SliderIntWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::SliderIntWithSteps(label, pOriginalWidget->pData, pOriginalWidget->mMin, pOriginalWidget->mMax, pOriginalWidget->mStep, pOriginalWidget->mFormat);
	processWidgetCallbacks(pWidget);
}

// SliderUintWidget private functions
void drawSliderUintWidget(IWidget* pWidget)
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
void drawRadioButtonWidget(IWidget* pWidget)
{
	RadioButtonWidget* pOriginalWidget = (RadioButtonWidget*)(pWidget->pWidget);

	ImGui::RadioButton(pWidget->mLabel, pOriginalWidget->pData, pOriginalWidget->mRadioId);
	processWidgetCallbacks(pWidget);
}

// CheckboxWidget private functions
void drawCheckboxWidget(IWidget* pWidget)
{
	CheckboxWidget* pOriginalWidget = (CheckboxWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Text("%s", pWidget->mLabel);
	ImGui::Checkbox(label, pOriginalWidget->pData);
	processWidgetCallbacks(pWidget);
}

// OneLineCheckboxWidget private functions
void drawOneLineCheckboxWidget(IWidget* pWidget)
{
	OneLineCheckboxWidget* pOriginalWidget = (OneLineCheckboxWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::Checkbox(label, pOriginalWidget->pData);
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(pOriginalWidget->mColor), "%s", pWidget->mLabel);
	processWidgetCallbacks(pWidget);
}

// CursorLocationWidget private functions
void drawCursorLocationWidget(IWidget* pWidget)
{
	CursorLocationWidget* pOriginalWidget = (CursorLocationWidget*)(pWidget->pWidget);

	ImGui::SetCursorPos(pOriginalWidget->mLocation);
	processWidgetCallbacks(pWidget);
}

// DropdownWidget private functions
void drawDropdownWidget(IWidget* pWidget)
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
void drawColumnWidget(IWidget* pWidget)
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
void drawProgressBarWidget(IWidget* pWidget)
{
	ProgressBarWidget* pOriginalWidget = (ProgressBarWidget*)(pWidget->pWidget);

	size_t currProgress = *(pOriginalWidget->pData);
	ImGui::Text("%s", pWidget->mLabel);
	ImGui::ProgressBar((float)currProgress / pOriginalWidget->mMaxProgress);
	processWidgetCallbacks(pWidget);
}

// ColorSliderWidget private functions
void drawColorSliderWidget(IWidget* pWidget)
{
	ColorSliderWidget* pOriginalWidget = (ColorSliderWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	uint& colorPick = *((uint*)pOriginalWidget->pData);
	float4 combo_color = ToFloat4Color(colorPick) / 255.0f;

	float col[4] = { combo_color.x, combo_color.y, combo_color.z, combo_color.w };
	ImGui::Text("%s", pWidget->mLabel);
	if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaPreview))
	{
		if (col[0] != combo_color.x || col[1] != combo_color.y || col[2] != combo_color.z || col[3] != combo_color.w)
		{
			combo_color = col;
			colorPick = ToUintColor(combo_color * 255.0f);
		}
	}
	processWidgetCallbacks(pWidget);
}

// HistogramWidget private functions
void drawHistogramWidget(IWidget* pWidget)
{
	HistogramWidget* pOriginalWidget = (HistogramWidget*)(pWidget->pWidget);

	ImGui::PlotHistogram(pWidget->mLabel, pOriginalWidget->pValues, pOriginalWidget->mCount, 0, pOriginalWidget->mHistogramTitle, *(pOriginalWidget->mMinScale),
		*(pOriginalWidget->mMaxScale), pOriginalWidget->mHistogramSize);

	processWidgetCallbacks(pWidget);
}

// PlotLinesWidget private functions
void drawPlotLinesWidget(IWidget* pWidget)
{
	PlotLinesWidget* pOriginalWidget = (PlotLinesWidget*)(pWidget->pWidget);

	ImGui::PlotLines(pWidget->mLabel, pOriginalWidget->mValues, pOriginalWidget->mNumValues, 0, pOriginalWidget->mTitle, *(pOriginalWidget->mScaleMin), *(pOriginalWidget->mScaleMax),
		*(pOriginalWidget->mPlotScale));

	processWidgetCallbacks(pWidget);
}

// ColorPickerWidget private functions
void drawColorPickerWidget(IWidget* pWidget)
{
	ColorPickerWidget* pOriginalWidget = (ColorPickerWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	uint& colorPick = *((uint*)pOriginalWidget->pData);
	float4 combo_color = ToFloat4Color(colorPick) / 255.0f;

	float col[4] = { combo_color.x, combo_color.y, combo_color.z, combo_color.w };
	ImGui::Text("%s", pWidget->mLabel);
	if (ImGui::ColorPicker4(label, col, ImGuiColorEditFlags_AlphaPreview))
	{
		if (col[0] != combo_color.x || col[1] != combo_color.y || col[2] != combo_color.z || col[3] != combo_color.w)
		{
			combo_color = col;
			colorPick = ToUintColor(combo_color * 255.0f);
		}
	}
	processWidgetCallbacks(pWidget);
}

// TextboxWidget private functions
void drawTextboxWidget(IWidget* pWidget)
{
	TextboxWidget* pOriginalWidget = (TextboxWidget*)(pWidget->pWidget);

	char label[MAX_LABEL_LENGTH];
	LABELID1(pOriginalWidget->pData, label);

	ImGui::InputText(label, (char*)pOriginalWidget->pData, pOriginalWidget->mLength, pOriginalWidget->mAutoSelectAll ? ImGuiInputTextFlags_AutoSelectAll : 0);
	processWidgetCallbacks(pWidget);
}

// DynamicTextWidget private functions
void drawDynamicTextWidget(IWidget* pWidget)
{
	DynamicTextWidget* pOriginalWidget = (DynamicTextWidget*)(pWidget->pWidget);

	ImGui::TextColored(*(pOriginalWidget->pColor), "%s", pOriginalWidget->pData);
	processWidgetCallbacks(pWidget);
}

// FilledRectWidget private functions
void drawFilledRectWidget(IWidget* pWidget)
{
	FilledRectWidget* pOriginalWidget = (FilledRectWidget*)(pWidget->pWidget);

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	float2 pos = window->Pos - window->Scroll + pOriginalWidget->mPos;
	float2 pos2 = float2(pos.x + pOriginalWidget->mScale.x, pos.y + pOriginalWidget->mScale.y);

	ImGui::GetWindowDrawList()->AddRectFilled(pos, pos2, ImGui::GetColorU32(pOriginalWidget->mColor));

	processWidgetCallbacks(pWidget);
}

// DrawTextWidget private functions
void drawDrawTextWidget(IWidget* pWidget)
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
void drawDrawTooltipWidget(IWidget* pWidget)
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
void drawDrawLineWidget(IWidget* pWidget)
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
void drawDrawCurveWidget(IWidget* pWidget)
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

void drawWidget(IWidget* pWidget)
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

	const float2 dpiScale = getDpiScale();
	style.ScaleAllSizes(min(dpiScale.x, dpiScale.y));
}

bool ImguiGUIDriver::init(Renderer* renderer, uint32_t const maxDynamicUIUpdatesPerBatch)
{
	mHandledGestures = false;
	pRenderer = renderer;
	mMaxDynamicUIUpdatesPerBatch = maxDynamicUIUpdatesPerBatch;
	mActive = true;
	memset(mPostUpdateKeyDownStates, false, sizeof(mPostUpdateKeyDownStates)); //-V601
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

	if (!mCustomShader)
	{
#ifdef USE_UI_PRECOMPILED_SHADERS
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
		texturedShaderDesc.mStages[0] = { "imgui.vert", NULL, 0, NULL };
		texturedShaderDesc.mStages[1] = { "imgui.frag", NULL, 0, NULL };
		addShader(pRenderer, &texturedShaderDesc, &pShaderTextured);
#endif
	}

	const char*       pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pShaderTextured, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pDefaultSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pRootSignatureTextured);

	DescriptorSetDesc setDesc = { pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, 1 + (maxDynamicUIUpdatesPerBatch * MAX_FRAMES) };
	addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
	setDesc = { pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_NONE, MAX_FRAMES };
	addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);

	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mSize = VERTEX_BUFFER_SIZE * MAX_FRAMES;
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.ppBuffer = &pVertexBuffer;
	addResource(&vbDesc, NULL);

	BufferLoadDesc ibDesc = vbDesc;
	ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
	ibDesc.mDesc.mSize = INDEX_BUFFER_SIZE * MAX_FRAMES;
	ibDesc.ppBuffer = &pIndexBuffer;
	addResource(&ibDesc, NULL);

	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.mDesc.mSize = sizeof(mat4);
	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
	{
		ubDesc.ppBuffer = &pUniformBuffer[i];
		addResource(&ubDesc, NULL);
	}

	mVertexLayoutTextured.mAttribCount = 3;
	mVertexLayoutTextured.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	mVertexLayoutTextured.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
	mVertexLayoutTextured.mAttribs[0].mBinding = 0;
	mVertexLayoutTextured.mAttribs[0].mLocation = 0;
	mVertexLayoutTextured.mAttribs[0].mOffset = 0;
	mVertexLayoutTextured.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
	mVertexLayoutTextured.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
	mVertexLayoutTextured.mAttribs[1].mBinding = 0;
	mVertexLayoutTextured.mAttribs[1].mLocation = 1;
	mVertexLayoutTextured.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(mVertexLayoutTextured.mAttribs[0].mFormat) / 8;
	mVertexLayoutTextured.mAttribs[2].mSemantic = SEMANTIC_COLOR;
	mVertexLayoutTextured.mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	mVertexLayoutTextured.mAttribs[2].mBinding = 0;
	mVertexLayoutTextured.mAttribs[2].mLocation = 2;
	mVertexLayoutTextured.mAttribs[2].mOffset =
		mVertexLayoutTextured.mAttribs[1].mOffset + TinyImageFormat_BitSizeOfBlock(mVertexLayoutTextured.mAttribs[1].mFormat) / 8;
	/************************************************************************/
	/************************************************************************/
	dpiScale = getDpiScale();

	//// init UI (input)
	ImGui::SetAllocatorFunctions(alloc_func, dealloc_func);
	context = ImGui::CreateContext();
	ImGui::SetCurrentContext(context);

	SetDefaultStyle();

	ImGuiIO& io = ImGui::GetIO();
	io.NavActive = true;
	io.WantCaptureMouse = false;
	io.KeyMap[ImGuiKey_Backspace] = InputBindings::BUTTON_BACK;
	io.KeyMap[ImGuiKey_LeftArrow] = InputBindings::BUTTON_KEYLEFT;
	io.KeyMap[ImGuiKey_RightArrow] = InputBindings::BUTTON_KEYRIGHT;
	io.KeyMap[ImGuiKey_Home] = InputBindings::BUTTON_KEYHOME;
	io.KeyMap[ImGuiKey_End] = InputBindings::BUTTON_KEYEND;
	io.KeyMap[ImGuiKey_Delete] = InputBindings::BUTTON_KEYDELETE;

	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
	{
		DescriptorData params[1] = {};
		params[0].pName = "uniformBlockVS";
		params[0].ppBuffers = &pUniformBuffer[i];
		updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
	}

	return true;
}

void ImguiGUIDriver::exit()
{
	removeSampler(pRenderer, pDefaultSampler);
	if (!mCustomShader)
		removeShader(pRenderer, pShaderTextured);
	removeDescriptorSet(pRenderer, pDescriptorSetTexture);
	removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
	removeRootSignature(pRenderer, pRootSignatureTextured);
	removeResource(pVertexBuffer);
	removeResource(pIndexBuffer);
	for (uint32_t i = 0; i < MAX_FRAMES; ++i)
		removeResource(pUniformBuffer[i]);

	for (Texture*& pFontTexture : mFontTextures)
		removeResource(pFontTexture);

	mFontTextures.set_capacity(0);
	ImGui::DestroyDemoWindow();
	ImGui::DestroyContext(context);
}

bool ImguiGUIDriver::addFont(void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont)
{
	// Build and load the texture atlas into a texture
	int            width, height, bytesPerPixel;
	unsigned char* pixels = NULL;
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig   config = {};
	config.FontDataOwnedByAtlas = false;
	ImFont* font = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize,
		fontSize * min(dpiScale.x, dpiScale.y), &config,
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

	mFontTextures.emplace_back(pTexture);
	io.Fonts->TexID = (void*)(mFontTextures.size() - 1);

	DescriptorData params[1] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pTexture;
	updateDescriptorSet(pRenderer, (uint32_t)mFontTextures.size() - 1, pDescriptorSetTexture, 1, params);

	return true;
}

bool ImguiGUIDriver::load(RenderTarget** ppRts, uint32_t count, PipelineCache* pCache)
{
	UNREF_PARAM(count);

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
	desc.pCache = pCache;
	desc.mType = PIPELINE_TYPE_GRAPHICS;
	GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
	pipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
	pipelineDesc.mRenderTargetCount = 1;
	pipelineDesc.mSampleCount = ppRts[0]->mSampleCount;
	pipelineDesc.pBlendState = &blendStateDesc;
	pipelineDesc.mSampleQuality = ppRts[0]->mSampleQuality;
	pipelineDesc.pColorFormats = &ppRts[0]->mFormat;
	pipelineDesc.pDepthState = &depthStateDesc;
	pipelineDesc.pRasterizerState = &rasterizerStateDesc;
	pipelineDesc.pRootSignature = pRootSignatureTextured;
	pipelineDesc.pShaderProgram = pShaderTextured;
	pipelineDesc.pVertexLayout = &mVertexLayoutTextured;
	pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	addPipeline(pRenderer, &desc, &pPipelineTextured);

	return true;
}

void ImguiGUIDriver::unload()
{
	removePipeline(pRenderer, pPipelineTextured);
}

void* ImguiGUIDriver::getContext()
{
	return context;
}

bool ImguiGUIDriver::update(GUIDriverUpdate* pGuiUpdate)
{
	ImGui::SetCurrentContext(context);
	// #TODO: Use window size as render-target size cannot be trusted to be the same as window size
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize.x = pGuiUpdate->width;
	io.DisplaySize.y = pGuiUpdate->height;
	io.DeltaTime = pGuiUpdate->deltaTime;
	if (pMovePosition)
		io.MousePos = *pMovePosition;
	
	memcpy(io.NavInputs, mNavInputs, sizeof(mNavInputs));
	
	ImGui::NewFrame();

	bool ret = false;

	if (mActive)
	{
		if (pGuiUpdate->showDemoWindow)
			ImGui::ShowDemoWindow();


		mLastUpdateCount = pGuiUpdate->componentCount;

		for (uint32_t compIndex = 0; compIndex < pGuiUpdate->componentCount; ++compIndex)
		{
			GuiComponent*                           pComponent = pGuiUpdate->pGuiComponents[compIndex];
			char									title[MAX_TITLE_STR_LENGTH]{};
			int32_t                                 guiComponentFlags = pComponent->mFlags;
			bool*                                   pCloseButtonActiveValue = pComponent->mHasCloseButton ? &pComponent->mHasCloseButton : NULL;
			const eastl::vector<char*>& contextualMenuLabels = pComponent->mContextualMenuLabels;
			const eastl::vector<WidgetCallback>&  contextualMenuCallbacks = pComponent->mContextualMenuCallbacks;
			const float4&                           windowRect = pComponent->mInitialWindowRect;
			float4&                                 currentWindowRect = pComponent->mCurrentWindowRect;
			IWidget**                               pProps = pComponent->mWidgets.data();
			uint32_t                                propCount = (uint32_t)pComponent->mWidgets.size();

			strcpy(title, pComponent->mTitle);

			if (title[0] == '\0')
				sprintf(title, "##%llu", (unsigned long long)pComponent);
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

				if ((guiComponentFlags & GUI_COMPONENT_FLAGS_NO_RESIZE) && !(guiComponentFlags & GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE))
					overrideSize = true;

				if (guiComponentFlags & GUI_COMPONENT_FLAGS_NO_MOVE)
					overridePos = true;

				ImGui::SetWindowSize(
					float2(windowRect.z, windowRect.w), overrideSize ? ImGuiCond_Always : ImGuiCond_Once);
				ImGui::SetWindowPos(
					float2(windowRect.x, windowRect.y), overridePos ? ImGuiCond_Always : ImGuiCond_Once);

				if (guiComponentFlags & GUI_COMPONENT_FLAGS_START_COLLAPSED)
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
			mLastUpdateMin[compIndex] = pos;
			mLastUpdateMax[compIndex] = pos + size;

			// Need to call ImGui::End event if result is false since we called ImGui::Begin
			ImGui::End();
            ImGui::PopStyleVar();
			ImGui::PopFont();
		}
	}
	ImGui::EndFrame();

	if (mActive)
	{
		for (uint32_t compIndex = 0; compIndex < pGuiUpdate->componentCount; ++compIndex)
		{
			GuiComponent*                           pComponent = pGuiUpdate->pGuiComponents[compIndex];
			IWidget**                               pProps = pComponent->mWidgets.data();
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

	mHandledGestures = false;

	// Apply post update keydown states
	memcpy(io.KeysDown, mPostUpdateKeyDownStates, sizeof(io.KeysDown));

	return ret;
}

void ImguiGUIDriver::draw(Cmd* pCmd)
{
	/************************************************************************/
	/************************************************************************/
	ImGui::SetCurrentContext(context);
	ImGui::Render();
	mDynamicUIUpdates = 0;

	ImDrawData* draw_data = ImGui::GetDrawData();

	Pipeline*            pPipeline = pPipelineTextured;

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
	uint64_t vOffset = frameIdx * VERTEX_BUFFER_SIZE;
	uint64_t iOffset = frameIdx * INDEX_BUFFER_SIZE;
	uint64_t vtx_dst = vOffset;
	uint64_t idx_dst = iOffset;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		BufferUpdateDesc  update = { pVertexBuffer, vtx_dst };
		beginUpdateResource(&update);
		memcpy(update.pMappedData, cmd_list->VtxBuffer.data(), cmd_list->VtxBuffer.size() * sizeof(ImDrawVert));
		endUpdateResource(&update, NULL);

		update = { pIndexBuffer, idx_dst };
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
	BufferUpdateDesc update = { pUniformBuffer[frameIdx] };
	beginUpdateResource(&update);
	*((mat4*)update.pMappedData) = *(mat4*)mvp;
	endUpdateResource(&update, NULL);

	const uint32_t vertexStride = sizeof(ImDrawVert);

	cmdSetViewport(pCmd, 0.0f, 0.0f, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f);
	cmdSetScissor(
		pCmd, (uint32_t)draw_data->DisplayPos.x, (uint32_t)draw_data->DisplayPos.y, (uint32_t)draw_data->DisplaySize.x,
		(uint32_t)draw_data->DisplaySize.y);
	cmdBindPipeline(pCmd, pPipeline);
	cmdBindIndexBuffer(pCmd, pIndexBuffer, INDEX_TYPE_UINT16, iOffset);
	cmdBindVertexBuffer(pCmd, 1, &pVertexBuffer, &vertexStride, &vOffset);

	cmdBindDescriptorSet(pCmd, frameIdx, pDescriptorSetUniforms);

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

				size_t id = (size_t)pcmd->TextureId;
				if (id >= mFontTextures.size())
				{
					uint32_t setIndex = (uint32_t)mFontTextures.size() + (frameIdx * mMaxDynamicUIUpdatesPerBatch + mDynamicUIUpdates);
					DescriptorData params[1] = {};
					params[0].pName = "uTex";
					params[0].ppTextures = (Texture**)&pcmd->TextureId;
					updateDescriptorSet(pRenderer, setIndex, pDescriptorSetTexture, 1, params);
					cmdBindDescriptorSet(pCmd, setIndex, pDescriptorSetTexture);
					++mDynamicUIUpdates;
				}
				else
				{
					cmdBindDescriptorSet(pCmd, (uint32_t)id, pDescriptorSetTexture);
				}

				cmdDrawIndexed(pCmd, pcmd->ElemCount, idx_offset, vtx_offset);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += (int)cmd_list->VtxBuffer.size();
	}

	frameIdx = (frameIdx + 1) % MAX_FRAMES;
}
