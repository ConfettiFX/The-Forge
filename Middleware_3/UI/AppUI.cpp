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

#include "AppUI.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/Renderer/IResourceLoader.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../Middleware_3/Text/Fontstash.h"
#include "../../Common_3/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

//LUA
#include "../../Middleware_3/LUA/LuaManager.h"
static LuaManager*		  pLuaManager = NULL;
static bool				  localLuaManager = false;
int32_t					  luaCounter = 0;

#include "../../Common_3/OS/Interfaces/IMemory.h"

extern void allocGUIDriver(Renderer* pRenderer, void** ppDriver);
extern void freeGUIDriver(void* pDriver);
extern bool initGUIDriver(void* pDriver, Renderer* pRenderer, uint32_t const maxDynamicUIUpdatesPerBatch);
extern void exitGUIDriver(void* pDriver);
extern bool loadGUIDriver(void* pDriver, RenderTarget** ppRts, uint32_t count, PipelineCache* pCache);
extern void unloadGUIDriver(void* pDriver);
extern void setGUIDriverCustomShader(void* pDriver, Shader* pShader);
extern void updateGUIDriver(void* pDriver, GUIDriverUpdate* pGuiUpdate);
extern void drawGUIDriver(void* pDriver, Cmd* pCmd);
extern void addGUIDriverFont(void* pDriver, void* pFontBuffer, uint32_t fontBufferSize, void* pFontGlyphRanges, float fontSize, uintptr_t* pFont);
extern bool GUIDriverOnText(void* pDriver, const wchar_t* pText);
extern bool GUIDriverOnButton(void* pDriver, uint32_t button, bool press, const float2* pVec);
extern uint8_t GUIDriverWantTextInput(void* pDriver);
extern bool GUIDriverIsFocused(void* pDriver);

static TextDrawDesc       gDefaultTextDrawDesc = TextDrawDesc(0, 0xffffffff, 16);

#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
#define TOUCH_INPUT 1
#endif

#define MAX_LUA_STR_LEN 256

// CollapsingHeaderWidget public functions
IWidget* addCollapsingHeaderSubWidget(CollapsingHeaderWidget* pWidget, const char* pLabel, const void* pSubWidget, WidgetType type)
{
	IWidget widget{};
	widget.mType = type;
	widget.pWidget = (void*)pSubWidget;
	strcpy(widget.mLabel, pLabel);

	pWidget->mGroupedWidgets.emplace_back(cloneWidget(&widget));
	return pWidget->mGroupedWidgets.back();
}

void removeCollapsingHeaderSubWidget(CollapsingHeaderWidget* pWidget, IWidget* pSubWidget)
{
	decltype(pWidget->mGroupedWidgets)::iterator it = eastl::find_if(pWidget->mGroupedWidgets.begin(), pWidget->mGroupedWidgets.end(),
		[pSubWidget](const IWidget* pIterWidget) -> bool { return pSubWidget->pWidget == pIterWidget->pWidget; });
	if (it != pWidget->mGroupedWidgets.end())
	{
		destroyWidget(*it, true);
		pWidget->mGroupedWidgets.erase(it);
	}
}

void removeCollapsingHeaderAllSubWidgets(CollapsingHeaderWidget* pWidget)
{
	for (size_t i = 0; i < pWidget->mGroupedWidgets.size(); ++i)
	{
		destroyWidget(pWidget->mGroupedWidgets[i], true);
	}
}

void setCollapsingHeaderWidgetCollapsed(CollapsingHeaderWidget* pWidget, bool collapsed)
{
	pWidget->mCollapsed = collapsed;
	pWidget->mPreviousCollapsed = !collapsed;
}

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

void registerCollapsingHeaderWidgetLua(const IWidget* pWidget)
{
	const CollapsingHeaderWidget* pOriginalWidget = (const CollapsingHeaderWidget*)(pWidget->pWidget);
	for (IWidget* widget : pOriginalWidget->mGroupedWidgets)
	{
		addWidgetLua(widget);
	}
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

// Utilities
static void strErase(char* str, size_t& strSize, size_t pos)
{
	ASSERT(str);
	ASSERT(strSize);
	ASSERT(pos < strSize);

	if (pos == strSize - 1)
		str[pos] = 0;
	else
		memmove(str + pos, str + pos + 1, strSize - pos);

	--strSize;
}

static void TrimString(char* str)
{
	size_t size = strlen(str);

	if (isdigit(str[0]))
		strErase(str, size, 0);
	for (uint32_t i = 0; i < size; ++i)
	{
		if (isspace(str[i]) || (!isalnum(str[i]) && str[i] != '_'))
			strErase(str, size, i--);
	}
}

void registerSliderFloatWidgetLua(const IWidget* pWidget)
{
	const SliderFloatWidget* pOriginalWidget = (const SliderFloatWidget*)(pWidget->pWidget);

	float* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (float)state->GetNumberArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)*data);
		return 1;
	});
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

void registerSliderFloat2WidgetLua(const IWidget* pWidget)
{
	const SliderFloat2Widget* pOriginalWidget = (const SliderFloat2Widget*)(pWidget->pWidget);

	float2* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		data->x = (float)state->GetNumberArg(1);
		data->y = (float)state->GetNumberArg(2);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)data->x);
		state->PushResultNumber((double)data->y);
		return 2;
	});
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

void registerSliderFloat3WidgetLua(const IWidget* pWidget)
{
	const SliderFloat3Widget* pOriginalWidget = (const SliderFloat3Widget*)(pWidget->pWidget);

	float3* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		data->x = (float)state->GetNumberArg(1);
		data->y = (float)state->GetNumberArg(2);
		data->z = (float)state->GetNumberArg(3);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)data->x);
		state->PushResultNumber((double)data->y);
		state->PushResultNumber((double)data->z);
		return 3;
	});
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

void registerSliderFloat4WidgetLua(const IWidget* pWidget)
{
	const SliderFloat4Widget* pOriginalWidget = (const SliderFloat4Widget*)(pWidget->pWidget);

	float4* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		data->x = (float)state->GetNumberArg(1);
		data->y = (float)state->GetNumberArg(2);
		data->z = (float)state->GetNumberArg(3);
		data->w = (float)state->GetNumberArg(4);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultNumber((double)data->x);
		state->PushResultNumber((double)data->y);
		state->PushResultNumber((double)data->z);
		state->PushResultNumber((double)data->w);
		return 4;
	});
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

void registerSliderIntWidget(const IWidget* pWidget)
{
	const SliderIntWidget* pOriginalWidget = (const SliderIntWidget*)(pWidget->pWidget);

	int32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (int32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
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

void registerSliderUintWidgetLua(const IWidget* pWidget)
{
	const SliderUintWidget* pOriginalWidget = (const SliderUintWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
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

void registerCheckboxWidgetLua(const IWidget* pWidget)
{
	const CheckboxWidget* pOriginalWidget = (const CheckboxWidget*)(pWidget->pWidget);

	bool* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (bool)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
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

void registerOneLineCheckboxWidgetLua(const IWidget* pWidget)
{
	const OneLineCheckboxWidget* pOriginalWidget = (const OneLineCheckboxWidget*)(pWidget->pWidget);

	bool* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (bool)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
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

void registerDropdownWidgetLua(const IWidget* pWidget)
{
	const DropdownWidget* pOriginalWidget = (const DropdownWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});

	char functionSizeName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionSizeName, "Size");
	strcat(functionSizeName, pWidget->mLabel);

	TrimString(functionSizeName);

	uint32_t size = (uint32_t)pOriginalWidget->mValues.size();
	pLuaManager->SetFunction(functionSizeName, [size](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)size);
		return 1;
	});
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

void registerProgressBarWidgetLua(const IWidget* pWidget)
{
	const ProgressBarWidget* pOriginalWidget = (const ProgressBarWidget*)(pWidget->pWidget);

	size_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (size_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
}

// ColorSliderWidget private functions
ColorSliderWidget* cloneColorSliderWidget(const void* pWidget)
{
	const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)pWidget;
	ColorSliderWidget* pClonedWidget = (ColorSliderWidget*)tf_calloc(1, sizeof(ColorSliderWidget));

	pClonedWidget->pData = pOriginalWidget->pData;

	return pClonedWidget;
}

void registerColorSliderWidgetLua(const IWidget* pWidget)
{
	const ColorSliderWidget* pOriginalWidget = (const ColorSliderWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
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

void registerColorPickerWidget(const IWidget* pWidget)
{
	const ColorPickerWidget* pOriginalWidget = (const ColorPickerWidget*)(pWidget->pWidget);

	uint32_t* data = pOriginalWidget->pData;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		*data = (uint32_t)state->GetIntegerArg(1);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultInteger((int)*data);
		return 1;
	});
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

void registerTextboxWidgetLua(const IWidget* pWidget)
{
	const TextboxWidget* pOriginalWidget = (const TextboxWidget*)(pWidget->pWidget);

	char* data = pOriginalWidget->pData;
	uint32_t len = pOriginalWidget->mLength;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data, len](ILuaStateWrap* state) -> int {
		char strData[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, strData);

		size_t size = strlen(strData);
		ASSERT(len > size);
		memcpy(data, strData, size);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data](ILuaStateWrap* state) -> int {
		state->PushResultString(data);
		return 1;
	});
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

void registerDynamicTextWidgetLua(const IWidget* pWidget)
{
	const DynamicTextWidget* pOriginalWidget = (const DynamicTextWidget*)(pWidget->pWidget);

	char* data = pOriginalWidget->pData;
	uint32_t len = pOriginalWidget->mLength;
	float4* color = pOriginalWidget->pColor;
	char functionName[MAX_LABEL_STR_LENGTH + 3];
	sprintf(functionName, "Set");
	strcat(functionName, pWidget->mLabel);

	TrimString(functionName);

	pLuaManager->SetFunction(functionName, [data, len, color](ILuaStateWrap* state) -> int {
		char strData[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, strData);
		size_t size = strlen(strData);
		ASSERT(len > size);
		memcpy(data, strData, size);
		color->x = (float)state->GetNumberArg(2);
		color->y = (float)state->GetNumberArg(3);
		color->z = (float)state->GetNumberArg(4);
		color->w = (float)state->GetNumberArg(5);
		return 0;
	});

	functionName[0] = 'G';
	pLuaManager->SetFunction(functionName, [data, color](ILuaStateWrap* state) -> int {
		state->PushResultString(data);
		state->PushResultNumber(color->x);
		state->PushResultNumber(color->y);
		state->PushResultNumber(color->z);
		state->PushResultNumber(color->w);
		return 5;
	});
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

// IWidget private functions
void cloneWidgetBase(IWidget* pDstWidget, const IWidget* pSrcWidget)
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

void destroyWidget(IWidget* pWidget, bool freeUnderlying)
{
	if (freeUnderlying)
	{
		switch (pWidget->mType)
		{
		case WIDGET_TYPE_COLLAPSING_HEADER:
		{
			CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);
			removeCollapsingHeaderAllSubWidgets(pOriginalWidget);
			pOriginalWidget->mGroupedWidgets.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_DEBUG_TEXTURES:
		{
			DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);
			pOriginalWidget->mTextures.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_LABEL:
		{
			break;
		}

		case WIDGET_TYPE_COLOR_LABEL:
		{
			break;
		}

		case WIDGET_TYPE_HORIZONTAL_SPACE:
		{
			break;
		}

		case WIDGET_TYPE_SEPARATOR:
		{
			break;
		}

		case WIDGET_TYPE_VERTICAL_SEPARATOR:
		{
			break;
		}

		case WIDGET_TYPE_BUTTON:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT2:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT3:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT4:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_INT:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_UINT:
		{
			break;
		}

		case WIDGET_TYPE_RADIO_BUTTON:
		{
			break;
		}

		case WIDGET_TYPE_CHECKBOX:
		{
			break;
		}

		case WIDGET_TYPE_ONE_LINE_CHECKBOX:
		{
			break;
		}

		case WIDGET_TYPE_CURSOR_LOCATION:
		{
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

		case WIDGET_TYPE_PROGRESS_BAR:
		{
			break;
		}

		case WIDGET_TYPE_COLOR_SLIDER:
		{
			break;
		}

		case WIDGET_TYPE_HISTOGRAM:
		{
			break;
		}

		case WIDGET_TYPE_PLOT_LINES:
		{
			break;
		}

		case WIDGET_TYPE_COLOR_PICKER:
		{
			break;
		}

		case WIDGET_TYPE_TEXTBOX:
		{
			break;
		}

		case WIDGET_TYPE_DYNAMIC_TEXT:
		{
			break;
		}

		case WIDGET_TYPE_FILLED_RECT:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_TEXT:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_TOOLTIP:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_LINE:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_CURVE:
		{
			break;
		}

		default:
			ASSERT(0);
		}switch (pWidget->mType)
		{
		case WIDGET_TYPE_COLLAPSING_HEADER:
		{
			CollapsingHeaderWidget* pOriginalWidget = (CollapsingHeaderWidget*)(pWidget->pWidget);
			removeCollapsingHeaderAllSubWidgets(pOriginalWidget);
			pOriginalWidget->mGroupedWidgets.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_DEBUG_TEXTURES:
		{
			DebugTexturesWidget* pOriginalWidget = (DebugTexturesWidget*)(pWidget->pWidget);
			pOriginalWidget->mTextures.set_capacity(0);
			break;
		}

		case WIDGET_TYPE_LABEL:
		{
			break;
		}

		case WIDGET_TYPE_COLOR_LABEL:
		{
			break;
		}

		case WIDGET_TYPE_HORIZONTAL_SPACE:
		{
			break;
		}

		case WIDGET_TYPE_SEPARATOR:
		{
			break;
		}

		case WIDGET_TYPE_VERTICAL_SEPARATOR:
		{
			break;
		}

		case WIDGET_TYPE_BUTTON:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT2:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT3:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_FLOAT4:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_INT:
		{
			break;
		}

		case WIDGET_TYPE_SLIDER_UINT:
		{
			break;
		}

		case WIDGET_TYPE_RADIO_BUTTON:
		{
			break;
		}

		case WIDGET_TYPE_CHECKBOX:
		{
			break;
		}

		case WIDGET_TYPE_ONE_LINE_CHECKBOX:
		{
			break;
		}

		case WIDGET_TYPE_CURSOR_LOCATION:
		{
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

		case WIDGET_TYPE_PROGRESS_BAR:
		{
			break;
		}

		case WIDGET_TYPE_COLOR_SLIDER:
		{
			break;
		}

		case WIDGET_TYPE_HISTOGRAM:
		{
			break;
		}

		case WIDGET_TYPE_PLOT_LINES:
		{
			break;
		}

		case WIDGET_TYPE_COLOR_PICKER:
		{
			break;
		}

		case WIDGET_TYPE_TEXTBOX:
		{
			break;
		}

		case WIDGET_TYPE_DYNAMIC_TEXT:
		{
			break;
		}

		case WIDGET_TYPE_FILLED_RECT:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_TEXT:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_TOOLTIP:
		{
			break;
		}

		case WIDGET_TYPE_DRAW_LINE:
		{
			break;
		}

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

// IWidget public functions
IWidget* cloneWidget(const IWidget* pOtherWidget)
{
	IWidget* pWidget = (IWidget*)tf_calloc(1, sizeof(IWidget));
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

void addWidgetLua(const IWidget* pWidget)
{
	typedef eastl::pair<char*, WidgetCallback> NamePtrPair;
	eastl::vector<NamePtrPair> functionsList;
	char functionName[MAX_LABEL_STR_LENGTH]{};
	strcpy(functionName, pWidget->mLabel);

	TrimString(functionName);

	if (pWidget->pOnHover)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnHover");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnHover });
	}
	if (pWidget->pOnActive)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnActive");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnActive });
	}
	if (pWidget->pOnFocus)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnFocus");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnFocus });
	}
	if (pWidget->pOnEdited)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnEdited");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnEdited });
	}
	if (pWidget->pOnDeactivated)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnDeactivated");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnDeactivated });
	}
	if (pWidget->pOnDeactivatedAfterEdit)
	{
		char* fullName = (char*)tf_calloc(MAX_LABEL_STR_LENGTH + 32, sizeof(char));
		strcpy(fullName, functionName);
		strcat(fullName, "OnDeactivatedAfterEdit");
		functionsList.emplace_back(NamePtrPair{ fullName, pWidget->pOnDeactivatedAfterEdit });
	}

	for (NamePtrPair pair : functionsList)
	{
		pLuaManager->SetFunction(pair.first, [pair](ILuaStateWrap* state) -> int {
			pair.second();
			return 0;
		});

		tf_free(pair.first);
	}

	switch (pWidget->mType)
	{
	case WIDGET_TYPE_COLLAPSING_HEADER:
	{
		registerCollapsingHeaderWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_DEBUG_TEXTURES:
	{
		break;
	}

	case WIDGET_TYPE_LABEL:
	{
		break;
	}

	case WIDGET_TYPE_COLOR_LABEL:
	{
		break;
	}

	case WIDGET_TYPE_HORIZONTAL_SPACE:
	{
		break;
	}

	case WIDGET_TYPE_SEPARATOR:
	{
		break;
	}

	case WIDGET_TYPE_VERTICAL_SEPARATOR:
	{
		break;
	}

	case WIDGET_TYPE_BUTTON:
	{
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT:
	{
		registerSliderFloatWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT2:
	{
		registerSliderFloat2WidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT3:
	{
		registerSliderFloat3WidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_FLOAT4:
	{
		registerSliderFloat4WidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_INT:
	{
		registerSliderIntWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_SLIDER_UINT:
	{
		registerSliderUintWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_RADIO_BUTTON:
	{
		break;
	}

	case WIDGET_TYPE_CHECKBOX:
	{
		registerCheckboxWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_ONE_LINE_CHECKBOX:
	{
		registerOneLineCheckboxWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_CURSOR_LOCATION:
	{
		break;
	}

	case WIDGET_TYPE_DROPDOWN:
	{
		registerDropdownWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_COLUMN:
	{
		break;
	}

	case WIDGET_TYPE_PROGRESS_BAR:
	{
		registerProgressBarWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_COLOR_SLIDER:
	{
		registerColorSliderWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_HISTOGRAM:
	{
		break;
	}

	case WIDGET_TYPE_PLOT_LINES:
	{
		break;
	}

	case WIDGET_TYPE_COLOR_PICKER:
	{
		registerColorPickerWidget(pWidget);
		break;
	}

	case WIDGET_TYPE_TEXTBOX:
	{
		registerTextboxWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_DYNAMIC_TEXT:
	{
		registerDynamicTextWidgetLua(pWidget);
		break;
	}

	case WIDGET_TYPE_FILLED_RECT:
	{
		break;
	}

	case WIDGET_TYPE_DRAW_TEXT:
	{
		break;
	}

	case WIDGET_TYPE_DRAW_TOOLTIP:
	{
		break;
	}

	case WIDGET_TYPE_DRAW_LINE:
	{
		break;
	}

	case WIDGET_TYPE_DRAW_CURVE:
	{
		break;
	}

	default:
		ASSERT(0);
	}
}

// GuiComponent public functions
IWidget* addGuiWidget(GuiComponent* pGui, const char* pLabel, const void* pWidget, WidgetType type, bool clone /* = true*/)
{
	IWidget* pBaseWidget = (IWidget*)tf_calloc(1, sizeof(IWidget));
	pBaseWidget->mType = type;
	pBaseWidget->pWidget = (void*)pWidget;
	strcpy(pBaseWidget->mLabel, pLabel);

	pGui->mWidgets.emplace_back(clone ? cloneWidget(pBaseWidget) : pBaseWidget);
	pGui->mWidgetsClone.emplace_back(clone);

	if (clone)
		tf_free(pBaseWidget);

	return pGui->mWidgets.back();
}

void removeGuiWidget(GuiComponent* pGui, IWidget* pWidget)
{
	decltype(pGui->mWidgets)::iterator it = eastl::find_if(pGui->mWidgets.begin(), pGui->mWidgets.end(),
		[pWidget](const IWidget* pIterWidget) -> bool { return pWidget->pWidget == pIterWidget->pWidget; });
	if (it != pGui->mWidgets.end())
	{
		destroyWidget(*it, pGui->mWidgetsClone[it - pGui->mWidgets.begin()]);
		pGui->mWidgetsClone.erase(pGui->mWidgetsClone.begin() + (it - pGui->mWidgets.begin()));
		pGui->mWidgets.erase(it);
	}
}

void removeGuiAllWidgets(GuiComponent* pGui)
{
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
}

// DynamicUIWidgets public functions
IWidget* addDynamicUIWidget(DynamicUIWidgets* pDynamicUI, const char* pLabel, const void* pWidget, WidgetType type)
{
	IWidget widget{};
	widget.mType = type;
	widget.pWidget = (void*)pWidget;
	strcpy(widget.mLabel, pLabel);

	pDynamicUI->mDynamicProperties.emplace_back(cloneWidget(&widget));

	return pDynamicUI->mDynamicProperties.back();
}

void showDynamicUIWidgets(const DynamicUIWidgets* pDynamicUI, GuiComponent* pGui)
{
	for (size_t i = 0; i < pDynamicUI->mDynamicProperties.size(); ++i)
	{
		IWidget* pWidget = pDynamicUI->mDynamicProperties[i];
		IWidget* pNewWidget = addGuiWidget(pGui, pWidget->mLabel, pWidget->pWidget, pWidget->mType, false);
		cloneWidgetBase(pNewWidget, pWidget);
	}
}

void hideDynamicUIWidgets(const DynamicUIWidgets* pDynamicUI, GuiComponent* pGui)
{
	for (size_t i = 0; i < pDynamicUI->mDynamicProperties.size(); i++)
	{
		// We should not erase the widgets in this for-loop, otherwise the IDs
		// in mDynamicPropHandles will not match once  GuiComponent::mWidgets changes size.
		removeGuiWidget(pGui, pDynamicUI->mDynamicProperties[i]);
	}
}

void removeDynamicUI(DynamicUIWidgets* pDynamicUI)
{
	for (size_t i = 0; i < pDynamicUI->mDynamicProperties.size(); ++i)
	{
		destroyWidget(pDynamicUI->mDynamicProperties[i], true);
	}

	pDynamicUI->mDynamicProperties.set_capacity(0);
}

// UIApp public functions
void initAppUI(Renderer* pRenderer, UIAppDesc* pDesc, UIApp** ppUIApp)
{
	UIApp* pAppUI = tf_new(UIApp);
	pAppUI->mFontAtlasSize = pDesc->fontAtlasSize;
	pAppUI->mFontstashRingSizeBytes = pDesc->fontStashRingSizeBytes;
	pAppUI->mMaxDynamicUIUpdatesPerBatch = pDesc->maxDynamicUIUpdatesPerBatch;

	pAppUI->mShowDemoUiWindow = false;

	pAppUI->pImpl = tf_new(UIAppImpl);
	pAppUI->pImpl->pRenderer = pRenderer;

	pAppUI->pDriver = NULL;
	pAppUI->pPipelineCache = pDesc->pCache;

	// Initialize the fontstash
	//
	// To support more characters and different font configurations
	// the app will need more memory for the fontstash atlas.
	//
	if (pAppUI->mFontAtlasSize <= 0) // then we assume we'll only draw debug text in the UI, in which case the atlas size can be kept small
		pAppUI->mFontAtlasSize = 256;

	pAppUI->pImpl->pFontStash = tf_new(Fontstash);
	bool success = pAppUI->pImpl->pFontStash->init(pRenderer, pAppUI->mFontAtlasSize, pAppUI->mFontAtlasSize, pAppUI->mFontstashRingSizeBytes);

	allocGUIDriver(pAppUI->pImpl->pRenderer, &(pAppUI->pDriver));
	if (pAppUI->pCustomShader)
		setGUIDriverCustomShader(pAppUI->pDriver, pAppUI->pCustomShader);
	success &= initGUIDriver(pAppUI->pDriver, pAppUI->pImpl->pRenderer, pAppUI->mMaxDynamicUIUpdatesPerBatch);

	if (!pLuaManager)
	{
		pLuaManager = tf_new(LuaManager);
		pLuaManager->Init();
		localLuaManager = true;
	}

	pLuaManager->SetFunction("LOGINFO", [](ILuaStateWrap* state) -> int {
		char str[MAX_LUA_STR_LEN]{};
		state->GetStringArg(1, str);
		LOGF(LogLevel::eINFO, str);
		return 0;
	});

	pLuaManager->SetFunction("SetCounter", [](ILuaStateWrap* state) -> int {
		luaCounter = (int32_t)state->GetIntegerArg(1);
		return 0;
	});

	if (!success)
	{
		tf_delete(pAppUI);
		*ppUIApp = nullptr;
	}
	else
		*ppUIApp = pAppUI;
}

void exitAppUI(UIApp* pAppUI)
{
	if (localLuaManager)
	{
		pLuaManager->Exit();
		tf_delete(pLuaManager);
		pLuaManager = NULL;
	}

	for (char* pStr : pAppUI->mRuntimeScripts)
		tf_free(pStr);
	pAppUI->mRuntimeScripts.clear();

	for (char* pStr : pAppUI->mTestScripts)
		tf_free(pStr);
	pAppUI->mTestScripts.clear();

	removeAppUIAllGuiComponents(pAppUI);

	pAppUI->pImpl->pFontStash->exit();
	tf_delete(pAppUI->pImpl->pFontStash);
	pAppUI->pImpl->pFontStash = NULL;

	exitGUIDriver(pAppUI->pDriver);
	freeGUIDriver(pAppUI->pDriver);
	pAppUI->pDriver = NULL;

	tf_delete(pAppUI->pImpl);
	pAppUI->pImpl = NULL;

	tf_delete(pAppUI);
}

bool addAppGUIDriver(UIApp* pAppUI, RenderTarget** ppRTs, uint32_t count)
{
	ASSERT(ppRTs && ppRTs[0]);
	pAppUI->mWidth = (float)ppRTs[0]->mWidth;
	pAppUI->mHeight = (float)ppRTs[0]->mHeight;

	bool success = loadGUIDriver(pAppUI->pDriver, ppRTs, count, pAppUI->pPipelineCache);
	success &= pAppUI->pImpl->pFontStash->load(ppRTs, count, pAppUI->pPipelineCache);

	return success;
}

void removeAppGUIDriver(UIApp* pAppUI)
{
	unloadGUIDriver(pAppUI->pDriver);
	pAppUI->pImpl->pFontStash->unload();
}

void updateAppUI(UIApp* pAppUI, float deltaTime)
{
	if (pAppUI->pImpl->mUpdated || !pAppUI->pImpl->mComponentsToUpdate.size())
		return;

	if (luaCounter > 0)
		--luaCounter;

#if defined(AUTOMATED_TESTING)
	if (!pAppUI->mTestScripts.empty() && !luaCounter)
	{
		char* pLogMsg = (char*)tf_calloc(strlen(pAppUI->mTestScripts.front()) + 32, sizeof(char));
		sprintf(pLogMsg, "Script ");
		strcat(pLogMsg, pAppUI->mTestScripts.front());
		strcat(pLogMsg, " is running..");

		LOGF(LogLevel::eINFO, pLogMsg);
		pLuaManager->RunScript(pAppUI->mTestScripts.front());
		tf_free(pAppUI->mTestScripts.front());
		pAppUI->mTestScripts.pop_front();

		tf_free(pLogMsg);
	}
#endif

	pAppUI->pImpl->mUpdated = true;

	eastl::vector<GuiComponent*> activeComponents(pAppUI->pImpl->mComponentsToUpdate.size());
	uint32_t                       activeComponentCount = 0;
	for (uint32_t i = 0; i < (uint32_t)pAppUI->pImpl->mComponentsToUpdate.size(); ++i)
		if (pAppUI->pImpl->mComponentsToUpdate[i]->mActive)
			activeComponents[activeComponentCount++] = pAppUI->pImpl->mComponentsToUpdate[i];

	GUIDriverUpdate guiUpdate = {};
	guiUpdate.pGuiComponents = activeComponents.data();
	guiUpdate.componentCount = activeComponentCount;
	guiUpdate.deltaTime = deltaTime;
	guiUpdate.width = pAppUI->mWidth;
	guiUpdate.height = pAppUI->mHeight;
	guiUpdate.showDemoWindow = pAppUI->mShowDemoUiWindow;
	updateGUIDriver(pAppUI->pDriver, &guiUpdate);

	pAppUI->pImpl->mComponentsToUpdate.clear();

	if (!pAppUI->mRuntimeScripts.empty() && !luaCounter)
	{
		char* pLogMsg = (char*)tf_calloc(strlen(pAppUI->mRuntimeScripts.front()) + 32, sizeof(char));
		sprintf(pLogMsg, "Script ");
		strcat(pLogMsg, pAppUI->mRuntimeScripts.front());
		strcat(pLogMsg, " is running..");

		LOGF(LogLevel::eINFO, pLogMsg);
		pLuaManager->RunScript(pAppUI->mRuntimeScripts.front());
		tf_free(pAppUI->mRuntimeScripts.front());
		pAppUI->mRuntimeScripts.pop_front();

		tf_free(pLogMsg);
	}
}

void drawAppUI(UIApp* pAppUI, Cmd* pCmd)
{
	if (pAppUI->pImpl->mUpdated)
	{
		pAppUI->pImpl->mUpdated = false;
		drawGUIDriver(pAppUI->pDriver, pCmd);
	}
}

void addAppUILuaManager(UIApp* pAppUI, LuaManager* aLuaManager)
{
	ASSERT(!pLuaManager || aLuaManager);
	pLuaManager = aLuaManager;
}

void addAppUITestScripts(UIApp* pAppUI, const char** ppFilenames, uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		char* pFilename = (char*)tf_calloc(strlen(ppFilenames[i]) + 1, sizeof(char));
		strcpy(pFilename, ppFilenames[i]);
		pAppUI->mTestScripts.push_back(pFilename);
	}
}

void runAppUITestScript(UIApp* pAppUI, const char* pFilename)
{
	char* pFilenameTemp = (char*)tf_calloc(strlen(pFilename) + 1, sizeof(char));
	strcpy(pFilenameTemp, pFilename);

	pAppUI->mRuntimeScripts.push_back(pFilenameTemp);
}

uint32_t initAppUIFont(UIApp* pAppUI, const char* pFontPath)
{
	uint32_t fontID = (uint32_t)pAppUI->pImpl->pFontStash->defineFont("default", pFontPath);
	ASSERT(fontID != -1);

	return fontID;
}

GuiComponent* addAppUIGuiComponent(UIApp* pAppUI, const char* pTitle, const GuiDesc* pDesc)
{
	GuiComponent* pComponent = tf_placement_new<GuiComponent>(tf_calloc(1, sizeof(GuiComponent)));
	pComponent->mHasCloseButton = false;
	pComponent->mFlags = GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE;
#if defined(TARGET_IOS) || defined(__ANDROID__)
	pComponent->mFlags |= GUI_COMPONENT_FLAGS_START_COLLAPSED;
#endif

	void* pFontBuffer = pAppUI->pImpl->pFontStash->getFontBuffer(pDesc->mDefaultTextDrawDesc.mFontID);
	uint32_t fontBufferSize = pAppUI->pImpl->pFontStash->getFontBufferSize(pDesc->mDefaultTextDrawDesc.mFontID);
	if (pFontBuffer)
		addGUIDriverFont(pAppUI->pDriver, pFontBuffer, fontBufferSize, NULL, pDesc->mDefaultTextDrawDesc.mFontSize, &pComponent->pFont);

	pComponent->mInitialWindowRect = { pDesc->mStartPosition.getX(), pDesc->mStartPosition.getY(), pDesc->mStartSize.getX(),
										 pDesc->mStartSize.getY() };

	pComponent->mActive = true;
	strcpy(pComponent->mTitle, pTitle);
	pComponent->mAlpha = 1.0f;
	pAppUI->pImpl->mComponents.emplace_back(pComponent);

	return pComponent;
}

void removeAppUIGuiComponent(UIApp* pAppUI, GuiComponent* pGui)
{
	ASSERT(pGui);

	removeGuiAllWidgets(pGui);
	GuiComponent** it = eastl::find(pAppUI->pImpl->mComponents.begin(), pAppUI->pImpl->mComponents.end(), pGui);
	if (it != pAppUI->pImpl->mComponents.end())
	{
		removeGuiAllWidgets(*it);
		pAppUI->pImpl->mComponents.erase(it);
		GuiComponent** active_it = eastl::find(pAppUI->pImpl->mComponentsToUpdate.begin(), pAppUI->pImpl->mComponentsToUpdate.end(), pGui);
		if (active_it != pAppUI->pImpl->mComponentsToUpdate.end())
			pAppUI->pImpl->mComponentsToUpdate.erase(active_it);
		pGui->mWidgets.clear();
	}

	tf_free(pGui);
}

void removeAppUIAllGuiComponents(UIApp* pAppUI)
{
	for (uint32_t i = 0; i < (uint32_t)pAppUI->pImpl->mComponents.size(); ++i)
	{
		removeGuiAllWidgets(pAppUI->pImpl->mComponents[i]);
		tf_free(pAppUI->pImpl->mComponents[i]);
	}

	pAppUI->pImpl->mComponents.clear();
	pAppUI->pImpl->mComponentsToUpdate.clear();
}

void appUIGui(UIApp* pAppUI, GuiComponent* pGui)
{
	pAppUI->pImpl->mComponentsToUpdate.emplace_back(pGui);
}

float2 measureAppUIText(UIApp* pAppUI, const char* pText, const TextDrawDesc* pDrawDesc)
{
	float textBounds[4] = {};
	pAppUI->pImpl->pFontStash->measureText(
		textBounds, pText, 0, 0, pDrawDesc->mFontID, pDrawDesc->mFontColor, pDrawDesc->mFontSize, pDrawDesc->mFontSpacing, pDrawDesc->mFontBlur);
	return float2(textBounds[2] - textBounds[0], textBounds[3] - textBounds[1]);
}

void drawAppUIText(UIApp* pAppUI, Cmd* pCmd, const float2* pScreenCoordsInPx, const char* pText, const TextDrawDesc* pDrawDesc)
{
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pAppUI->pImpl->pFontStash->drawText(
		pCmd, pText, pScreenCoordsInPx->getX(), pScreenCoordsInPx->getY(), pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize,
		pDesc->mFontSpacing, pDesc->mFontBlur);
}

void drawAppUITextInWorldSpace(UIApp* pAppUI, Cmd* pCmd, const char* pText, const mat4* pMatWorld, const mat4* pMatProjView, const TextDrawDesc* pDrawDesc)
{
	const TextDrawDesc* pDesc = pDrawDesc ? pDrawDesc : &gDefaultTextDrawDesc;
	pAppUI->pImpl->pFontStash->drawText(
		pCmd, pText, *pMatProjView, *pMatWorld, pDesc->mFontID, pDesc->mFontColor, pDesc->mFontSize, pDesc->mFontSpacing, pDesc->mFontBlur);
}

bool appUIOnText(UIApp* pAppUI, const wchar_t* pText)
{
	return GUIDriverOnText(pAppUI->pDriver, pText);
}

bool appUIOnButton(UIApp* pAppUI, uint32_t button, bool press, const float2* pVec)
{
	return GUIDriverOnButton(pAppUI->pDriver, button, press, pVec);
}

uint8_t appUIWantTextInput(UIApp* pAppUI)
{
	return GUIDriverWantTextInput(pAppUI->pDriver);
}

bool appUIIsFocused(UIApp* pAppUI)
{
	return GUIDriverIsFocused(pAppUI->pDriver);
}

// VirtualJoystickUI public functions
VirtualJoystickUI* initVirtualJoystickUI(Renderer* pRenderer, const char* pJoystickTexture)
{
	VirtualJoystickUI* pVirtualJoystick = tf_new(VirtualJoystickUI);

#if TOUCH_INPUT
	pVirtualJoystick->pRenderer = pRenderer;

	TextureLoadDesc loadDesc = {};
	SyncToken token = {};
	loadDesc.pFileName = pJoystickTexture;
	loadDesc.ppTexture = &pVirtualJoystick->pTexture;
	addResource(&loadDesc, &token);
	waitForToken(&token);

	if (!pVirtualJoystick->pTexture)
	{
		LOGF(LogLevel::eERROR, "Error loading texture file: %s", pJoystickTexture);
		tf_delete(pVirtualJoystick);
		return nullptr;
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
	addSampler(pRenderer, &samplerDesc, &pVirtualJoystick->pSampler);
	/************************************************************************/
	// Shader
	/************************************************************************/
	ShaderLoadDesc texturedShaderDesc = {};
	texturedShaderDesc.mStages[0] = { "textured_mesh.vert", NULL, 0 };
	texturedShaderDesc.mStages[1] = { "textured_mesh.frag", NULL, 0 };
	addShader(pRenderer, &texturedShaderDesc, &pVirtualJoystick->pShader);

	const char* pStaticSamplerNames[] = { "uSampler" };
	RootSignatureDesc textureRootDesc = { &pVirtualJoystick->pShader, 1 };
	textureRootDesc.mStaticSamplerCount = 1;
	textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
	textureRootDesc.ppStaticSamplers = &pVirtualJoystick->pSampler;
	addRootSignature(pRenderer, &textureRootDesc, &pVirtualJoystick->pRootSignature);

	DescriptorSetDesc descriptorSetDesc = { pVirtualJoystick->pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
	addDescriptorSet(pRenderer, &descriptorSetDesc, &pVirtualJoystick->pDescriptorSet);
	/************************************************************************/
	// Resources
	/************************************************************************/
	BufferLoadDesc vbDesc = {};
	vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	vbDesc.mDesc.mSize = 128 * 4 * sizeof(float4);
	vbDesc.ppBuffer = &pVirtualJoystick->pMeshBuffer;
	addResource(&vbDesc, NULL);
	/************************************************************************/
	// Prepare descriptor sets
	/************************************************************************/
	DescriptorData params[1] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pVirtualJoystick->pTexture;
	updateDescriptorSet(pRenderer, 0, pVirtualJoystick->pDescriptorSet, 1, params);
#endif
	return pVirtualJoystick;
}

void exitVirtualJoystickUI(VirtualJoystickUI* pVirtualJoystick)
{
#if TOUCH_INPUT
	removeSampler(pVirtualJoystick->pRenderer, pVirtualJoystick->pSampler);
	removeResource(pVirtualJoystick->pMeshBuffer);
	removeDescriptorSet(pVirtualJoystick->pRenderer, pVirtualJoystick->pDescriptorSet);
	removeRootSignature(pVirtualJoystick->pRenderer, pVirtualJoystick->pRootSignature);
	removeShader(pVirtualJoystick->pRenderer, pVirtualJoystick->pShader);
	removeResource(pVirtualJoystick->pTexture);
#endif

	tf_delete(pVirtualJoystick);
}

bool addVirtualJoystickUIPipeline(VirtualJoystickUI* pVirtualJoystick, RenderTarget* pScreenRT)
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
	pipelineDesc.pRootSignature = pVirtualJoystick->pRootSignature;
	pipelineDesc.pShaderProgram = pVirtualJoystick->pShader;
	pipelineDesc.pVertexLayout = &vertexLayout;
	addPipeline(pVirtualJoystick->pRenderer, &desc, &pVirtualJoystick->pPipeline);

	pVirtualJoystick->mRenderSize[0] = (float)pScreenRT->mWidth;
	pVirtualJoystick->mRenderSize[1] = (float)pScreenRT->mHeight;
#endif
	return true;
}

void removeVirtualJoystickUIPipeline(VirtualJoystickUI* pVirtualJoystick)
{
#if TOUCH_INPUT
	removePipeline(pVirtualJoystick->pRenderer, pVirtualJoystick->pPipeline);
#endif
}

void updateVirtualJoystickUI(VirtualJoystickUI* pVirtualJoystick, float deltaTime)
{
}

void drawVirtualJoystickUI(VirtualJoystickUI* pVirtualJoystick, Cmd* pCmd, const float4* pColor)
{
#if TOUCH_INPUT
	if (!(pVirtualJoystick->mSticks[0].mPressed || pVirtualJoystick->mSticks[1].mPressed))
		return;

	struct RootConstants
	{
		float4 color;
		float2 scaleBias;
	} data = {};

	cmdBindPipeline(pCmd, pVirtualJoystick->pPipeline);
	cmdBindDescriptorSet(pCmd, 0, pVirtualJoystick->pDescriptorSet);
	data.color = *pColor;
	data.scaleBias = { 2.0f / (float)pVirtualJoystick->mRenderSize[0], -2.0f / (float)pVirtualJoystick->mRenderSize[1] };
	cmdBindPushConstants(pCmd, pVirtualJoystick->pRootSignature, "uRootConstants", &data);

	// Draw the camera controller's virtual joysticks.
	float extSide = pVirtualJoystick->mOutsideRadius;
	float intSide = pVirtualJoystick->mInsideRadius;

	uint64_t bufferOffset = 0;
	for (uint i = 0; i < 2; i++)
	{
		if (pVirtualJoystick->mSticks[i].mPressed)
		{
			float2 joystickSize = float2(extSide);
			float2 joystickCenter = pVirtualJoystick->mSticks[i].mStartPos - float2(0.0f, pVirtualJoystick->mRenderSize.y * 0.1f);
			float2 joystickPos = joystickCenter - joystickSize * 0.5f;

			const uint32_t vertexStride = sizeof(float4);
			BufferUpdateDesc updateDesc = { pVirtualJoystick->pMeshBuffer, bufferOffset };
			beginUpdateResource(&updateDesc);
			TexVertex* vertices = (TexVertex*)updateDesc.pMappedData;
			// the last variable can be used to create a border
			MAKETEXQUAD(vertices, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
			endUpdateResource(&updateDesc, NULL);
			cmdBindVertexBuffer(pCmd, 1, &pVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(TexVertex) * 4;

			joystickSize = float2(intSide);
			joystickCenter = pVirtualJoystick->mSticks[i].mCurrPos - float2(0.0f, pVirtualJoystick->mRenderSize.y * 0.1f);
			joystickPos = float2(joystickCenter.getX(), joystickCenter.getY()) - 0.5f * joystickSize;

			updateDesc = { pVirtualJoystick->pMeshBuffer, bufferOffset };
			beginUpdateResource(&updateDesc);
			TexVertex* verticesInner = (TexVertex*)updateDesc.pMappedData;
			// the last variable can be used to create a border
			MAKETEXQUAD(verticesInner, joystickPos.x, joystickPos.y, joystickPos.x + joystickSize.x, joystickPos.y + joystickSize.y, 0);
			endUpdateResource(&updateDesc, NULL);
			cmdBindVertexBuffer(pCmd, 1, &pVirtualJoystick->pMeshBuffer, &vertexStride, &bufferOffset);
			cmdDraw(pCmd, 4, 0);
			bufferOffset += sizeof(TexVertex) * 4;
		}
	}
#endif
}

bool virtualJoystickUIOnMove(VirtualJoystickUI* pVirtualJoystick, uint32_t id, bool press, const float2* pVec)
{
#if TOUCH_INPUT
	if (!pVec) return false;


	if (!pVirtualJoystick->mSticks[id].mPressed)
	{
		pVirtualJoystick->mSticks[id].mStartPos = *pVec;
		pVirtualJoystick->mSticks[id].mCurrPos = *pVec;
	}
	else
	{
		pVirtualJoystick->mSticks[id].mCurrPos = *pVec;
	}
	pVirtualJoystick->mSticks[id].mPressed = press;
	return true;
#else
	return false;
#endif
}