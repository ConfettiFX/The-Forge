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

#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/IMiddleware.h"

#ifndef _WIN32
#include <unistd.h>  // for sleep()
#include <time.h> // for CLOCK_REALTIME
#include <cstring> // for memset
#endif

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

enum UIMaxFontSize
{
	UI_MAX_FONT_SIZE_UNDEFINED = 0, // Undefined size, will defaults to use UI_MAX_FONT_SIZE_512
	UI_MAX_FONT_SIZE_128 = 128, // Max font size is 12.8f
	UI_MAX_FONT_SIZE_256 = 256, // Max font size is 25.6f
	UI_MAX_FONT_SIZE_512 = 512, // Max font size is 51.2f
	UI_MAX_FONT_SIZE_1024 = 1024 // Max font size is 102.4f
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
	PropertyChangedCallback callback = nullptr;

// Anonymous structures generates warnings in C++11. 
// See discussion here for more info: https://stackoverflow.com/questions/2253878/why-does-c-disallow-anonymous-structs
#pragma warning( push )
#pragma warning( disable : 4201) // warning C4201: nonstandard extension used: nameless struct/union
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
#pragma warning( pop ) 

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

class GuiComponent
{
public:
	uint32_t	AddProperty(const UIProperty& prop);
	void		RemoveProperty(uint32_t propID);

	struct GuiComponentImpl* pImpl;
};
/************************************************************************/
// Helper Class for removing and adding properties easily
/************************************************************************/
typedef struct DynamicUIControls
{
	tinystl::vector<UIProperty> mDynamicProperties;
	tinystl::vector<uint32_t>   mDynamicPropHandles;

	void ShowDynamicProperties(GuiComponent* pGui)
	{
		for (int i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicPropHandles.push_back(0);
			mDynamicPropHandles[i] = pGui->AddProperty(mDynamicProperties[i]);
		}
	}

	void HideDynamicProperties(GuiComponent* pGui)
	{
		for (int i = 0; i < mDynamicProperties.size(); i++)
		{
			pGui->RemoveProperty(mDynamicPropHandles[i]);
		}
		mDynamicPropHandles.clear();
	}

} DynamicUIControls;
/************************************************************************/
// Abstract interface for handling GUI
/************************************************************************/
class GUIDriver
{
public:
	virtual bool init() = 0;
	virtual void exit() = 0;

	virtual bool load(class UIRenderer* renderer, int fontID, float fontSize, struct Texture* cursorTexture = 0, float uiwidth = 600, float uiheight = 400) = 0;
	virtual void unload() = 0;

	virtual void clear() = 0;
	virtual void processInput() = 0;
	virtual void window(const char* pTitle, float x, float y, float z, float w, float& oX, float& oY, float& oW, float& oH, class UIProperty* pProps, unsigned int propCount) = 0;
	virtual void draw(Cmd* q) = 0;
	virtual void setFontCalibration(float offset, float heightScale) = 0;

	virtual void onChar(const struct KeyboardCharEventData* data) = 0;
	virtual void onKey(const struct KeyboardButtonEventData* data) = 0;
	virtual bool onJoystick(int button, bool down) = 0;
	virtual void onMouseMove(const struct MouseMoveEventData* data) = 0;
	virtual void onMouseClick(const struct MouseButtonEventData* data) = 0;
	virtual void onMouseScroll(const struct MouseWheelEventData* data) = 0;
	virtual void onTouch(const struct TouchEventData* data) = 0;
	virtual void onTouchMove(const struct TouchEventData* data) = 0;
};
/************************************************************************/
// UI interface for App
/************************************************************************/
class UIApp : public IMiddleware
{
public:
	bool			Init(Renderer* renderer);
	void			Exit();

	bool			Load(RenderTarget** rts);
	void			Unload();

	void			Update(float deltaTime);
	void			Draw(Cmd* cmd);

	uint32_t		LoadFont(const char* pFontPath);
	GuiComponent*	AddGuiComponent(const char* pTitle, const GuiDesc* pDesc);
	void			RemoveGuiComponent(GuiComponent* pComponent);

	void			Gui(GuiComponent* pGui);
	/************************************************************************/
	// Data
	/************************************************************************/
	struct UIAppImpl*	pImpl;
};
