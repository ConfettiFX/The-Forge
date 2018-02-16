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

#pragma once

#include "../Math/MathTypes.h"
#include "IOperatingSystem.h"
#include "IFileSystem.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

enum UIPropertyType
{
	UI_PROPERTY_FLOAT,
	UI_PROPERTY_INT,
	UI_PROPERTY_UINT,
	UI_PROPERTY_BOOL,
	UI_PROPERTY_ENUM,
	UI_PROPERTY_BUTTON,
	UI_PROPERTY_TEXTINPUT
};

typedef void(*UIButtonFn)(void*);
typedef void(*PropertyChangedCallback)(const class UIProperty* pProp);

class UIProperty
{
public:
	enum FLAG
	{
		FLAG_NONE = 0,
		FLAG_VISIBLE = 1 << 0,
	};

	UIProperty(const char* description, int steps, float& value, float min = 0.0f, float max = 1.0f);
	UIProperty(const char* description, float& value, float min = 0.0f, float max = 1.0f, float increment = 0.02f, bool expScale = false);
	UIProperty(const char* description, int& value, int min = -100, int max = 100, int increment = 1);
	UIProperty(const char* description, unsigned int& value, unsigned int min = 0, unsigned int max = 100, unsigned int increment = 1);
	UIProperty(const char* description, bool& value);
	UIProperty(const char* description, UIButtonFn fn, void* userdata);
	UIProperty(const char* description, char* value, unsigned int length);

	template <class T>
	UIProperty(const char* description, T& value, const char** enumNames, const T* enumValues, PropertyChangedCallback callback = NULL) :
		description(description),
		type(UI_PROPERTY_ENUM),
		flags(FLAG_VISIBLE),
		source(&value),
		callback(callback)
	{
		settings.eByteSize = sizeof(T);
		settings.eNames = enumNames;
		settings.eValues = (const void*)enumValues;

		memset(uiState, 0, sizeof(uiState));
	}

	void setSettings(int steps, float min, float max);

	const char* description;
	UIPropertyType type;
	unsigned int flags;
	void* source;
	PropertyChangedCallback callback = NULL;
	union Settings
	{
		struct
		{
			float fMin;
			float fMax;
			float fIncrement;
			bool fExpScale;
		};
		struct
		{
			int iMin;
			int iMax;
			int iIncrement;
		};
		struct
		{
			unsigned int uiMin;
			unsigned int uiMax;
			unsigned int uiIncrement;
		};
		struct
		{
			const char** eNames;
			const void* eValues;
			int eByteSize;
		};
		struct
		{
			unsigned int sLen;
		};
		struct
		{
			void* pUserData;
		};
	} settings;

	char uiState[8];

	void* clbCustom;
	bool(*clbVisible)(void*);

	int enumComputeIndex() const;
	void modify(int steps);
};

typedef struct TextDrawDesc
{
	TextDrawDesc(uint32_t font = 0, uint32_t color = 0xffffffff, float size = 15.0f, float spacing = 0.0f, float blur = 0.0f) :
		mFontID(font), mFontColor(color), mFontSize(size), mFontSpacing(spacing), mFontBlur(blur) {}

	uint32_t mFontID;
	uint32_t mFontColor;
	float mFontSize;
	float mFontSpacing;
	float mFontBlur;
} TextDrawDesc;

typedef struct GpuProfileDrawDesc
{
	float mChildIndent = 25.0f;
	float mHeightOffset = 25.0f;
	TextDrawDesc mDefaultGpuTextDrawDesc = TextDrawDesc(0, 0xFF00CCAA, 15);
} GpuProfileDrawDesc;

typedef struct UISettings
{
	const char* pDefaultFontName;

	TextDrawDesc mDefaultFrameTimeTextDrawDesc	= TextDrawDesc(0, 0xff00ffff, 18);
	TextDrawDesc mDefaultTextDrawDesc			= TextDrawDesc(0, 0xffffffff, 16);
	GpuProfileDrawDesc mDefaultGpuProfileDrawDesc;
} UISettings;

typedef struct GuiDesc
{
	GuiDesc(const vec2& startPos = { 0.0f, 150.0f }, const vec2& startSize = { 600.0f, 550.0f }, const TextDrawDesc& textDrawDesc = { 0, 0xffffffff, 16 }) :
		mStartPosition(startPos),
		mStartSize(startSize),
		mDefaultTextDrawDesc(textDrawDesc)
	{}

	vec2			mStartPosition;
	vec2			mStartSize;
	TextDrawDesc	mDefaultTextDrawDesc;
} GuiDesc;

typedef struct Gui
{
	GuiDesc						mDesc;
	class UIAppComponentGui*	pGui;
	class UI*					pUI;
} Gui;

typedef struct UIManager
{
	UISettings			mSettings;
	class UIRenderer*	pUIRenderer;
	uint32_t			mDefaultFontstashID;
} UIManager;

void addUIManagerInterface(struct Renderer* pRenderer, const UISettings* pUISettings, UIManager** ppUIManager);
void removeUIManagerInterface(struct Renderer* pRenderer, UIManager* pUIManager);

void addGui(UIManager* pUIManager, const GuiDesc* pDesc, Gui** ppGui);
void removeGui(UIManager* pUIManager, Gui* pGui);
void addProperty(Gui* pUIManager, const UIProperty* pProperty, uint32_t* pID = NULL);
void addProperty(Gui* pUIManager, const UIProperty pProperty, uint32_t* pID = NULL);
void addResolutionProperty(Gui* pUIManager, uint32_t& resolutionIndex, uint32_t resCount, Resolution* pResolutions, PropertyChangedCallback onResolutionChanged, uint32_t* pId = NULL);
void removeProperty(Gui* pUIManager, uint32_t id);

void updateGui(UIManager* pUIManager, Gui* pGui, float deltaTime);

typedef struct DynamicUIControls
{
	tinystl::vector<UIProperty> mDynamicProperties;
	tinystl::vector<uint32_t>   mDynamicPropHandles;

	void ShowDynamicProperties(Gui* pGui)
	{
		for (int i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicPropHandles.push_back(0);
			addProperty(pGui, &mDynamicProperties[i], &mDynamicPropHandles[i]);
		}
	}

	void HideDynamicProperties(Gui* pGui)
	{
		for (int i = 0; i < mDynamicProperties.size(); i++)
		{
			removeProperty(pGui, mDynamicPropHandles[i]);
		}
		mDynamicPropHandles.clear();
	}

} DynamicUIControls;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void cmdUIBeginRender(struct Cmd* pCmd, UIManager* pUIManager, uint32_t renderTargetCount, struct RenderTarget** ppRenderTargets, struct RenderTarget* pDepthStencil);
void cmdUIDrawFrameTime(struct Cmd* pCmd, UIManager* pUIManager, const vec2& position, const char* pPrefix, float ms, const TextDrawDesc* pTextDrawDesc = NULL);
void cmdUIDrawText(struct Cmd* pCmd, UIManager* pUIManager, const vec2& position, const char* pText, const TextDrawDesc* pTextDrawDesc = NULL);
void cmdUIDrawTexturedQuad(struct Cmd* pCmd, UIManager* pUIManager, const vec2& position, const vec2& size, struct Texture* pTexture);
void cmdUIDrawGUI(struct Cmd* pCmd, UIManager* pUIManager, Gui* pGui);
/// Helper function to draw the gpu profiler time tree
void cmdUIDrawGpuProfileData(Cmd* pCmd, struct UIManager* pUIManager, const vec2& startPos, struct GpuProfiler* pGpuProfiler, const GpuProfileDrawDesc* pDrawDesc = NULL);
void cmdUIEndRender(struct Cmd* pCmd, UIManager* pUIManager);
