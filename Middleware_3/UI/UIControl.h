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
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

enum UIPropertyType
{
	UI_CONTROL_SLIDER_FLOAT,
	UI_CONTROL_SLIDER_INT,
	UI_CONTROL_SLIDER_UINT,

	UI_CONTROL_CHECKBOX,
	UI_CONTROL_RADIO_BUTTON,
	UI_CONTROL_DROPDOWN,
	UI_CONTROL_BUTTON,
	UI_CONTROL_TEXTBOX,
	UI_CONTROL_LABEL,
	UI_CONTROL_PROGRESS_BAR,
	UI_CONTROL_COLOR_SLIDER,
	UI_CONTROL_COLOR_PICKER,
	UI_CONTROL_MENU,
	UI_CONTROL_CONTEXTUAL,
};


typedef void(*UIButtonFn)(void*);
typedef void(*ControlChangedCallback)(const class UIProperty* pControl);
typedef void(*ContextMenuItemCallback)();


class UIProperty
{
public:
	enum FLAG
	{
		FLAG_NONE = 0,
		FLAG_VISIBLE = 1 << 0,
	};

	// Sliders
	UIProperty(const char* text, int steps, float& value, float min = 0.0f, float max = 1.0f, uint color = 0xAFAFAFFF, const char* tree = "none") :
		mText(text),
		mType(UI_CONTROL_SLIDER_FLOAT),
		mFlags(FLAG_VISIBLE),
		pData(&value),
		mColor(color),
		pTree(tree)
	{
		mSettings.fMin = min;
		mSettings.fMax = max;
		mSettings.fIncrement = (max - min) / float(steps);
		mSettings.fExpScale = false;
	}

	UIProperty(const char* text, float& value, float min = 0.0f, float max = 1.0f, float increment = 0.02f, bool expScale = false, uint color = 0xAFAFAFFF, const char* tree = "none")
		: mText(text)
		, mType(UI_CONTROL_SLIDER_FLOAT)
		, mFlags(FLAG_VISIBLE)
		, pData(&value)
		, mColor(color)
		, pTree(tree)
	{
		mSettings.fMin = min;
		mSettings.fMax = max;
		mSettings.fIncrement = increment;
		mSettings.fExpScale = expScale;
	}
	UIProperty(const char* text, int& value, int min = -100, int max = 100, int increment = 1, uint color = 0xAFAFAFFF, const char* tree = "none") :
		mText(text),
		mType(UI_CONTROL_SLIDER_INT),
		mFlags(FLAG_VISIBLE),
		pData(&value),
		mColor(color),
		pTree(tree)

	{
		mSettings.iMin = min;
		mSettings.iMax = max;
		mSettings.iIncrement = increment;
	}

	UIProperty(const char* text, uint& value, uint min = 0, uint max = 100, uint increment = 1, uint color = 0xAFAFAFFF, const char* tree = "none") :
		mText(text),
		mType(UI_CONTROL_SLIDER_UINT),
		mFlags(FLAG_VISIBLE),
		pData(&value),
		mColor(color),
		pTree(tree)
	{
		mSettings.uiMin = min;
		mSettings.uiMax = max;
		mSettings.uiIncrement = increment;
	}

	// CheckBox (@bExclusive=false) / RadioButton (@bExclusive=true)
	UIProperty(const char* text, bool& value, bool bExclusive = false, uint color = 0xAFAFAFFF, const char* tree = "none") :
		mText(text),
		mType(bExclusive ? UI_CONTROL_RADIO_BUTTON : UI_CONTROL_CHECKBOX),
		mFlags(FLAG_VISIBLE),
		pData(&value),
		mColor(color),
		pTree(tree)
	{
	}

	// Button
	UIProperty(const char* text, UIButtonFn fn, void* userdata, uint color = 0xAFAFAFFF, const char* tree = "none") :
		mText(text),
		mType(UI_CONTROL_BUTTON),
		mFlags(FLAG_VISIBLE),
		pData(*(void**)&fn),
		mColor(color),
		pTree(tree)
	{
		mSettings.pUserData = userdata;
	}

	// TextBox
	UIProperty(const char* text, char* value, uint length, uint color = 0xAFAFAFFF, const char* tree = "none") :
		mText(text),
		mType(UI_CONTROL_TEXTBOX),
		mFlags(FLAG_VISIBLE),
		pData(value),
		mColor(color)
	{
		mSettings.sLen = length;
	}

	// Label
	UIProperty(const char* text, uint color = 0xAFAFAFFF, const char* tree = "none")
		: mText(text)
		, mType(UI_CONTROL_LABEL)
		, mFlags(FLAG_VISIBLE)
		, mColor(color)
		, pTree(tree)
	{}

	// Progress Bar
	UIProperty(const char* text, size_t& progressValue, size_t progressMax, const char* tree = "none") :
		mText(text)
		, mType(UI_CONTROL_PROGRESS_BAR)
		, mFlags(FLAG_VISIBLE)
		, pData(&progressValue)
		, mColor(0xAFAFAFFF)
		, pTree(tree)
	{
		mSettings.maxProgress = progressMax;
	}

	// Color Slider/Picker
	//  @controlType has to be one of the two: UI_CONTROL_COLOR_SLIDER or UI_CONTROL_COLOR_PICKER
	UIProperty(const char* text, uint& colorValue, UIPropertyType controlType = UI_CONTROL_COLOR_SLIDER, const char* tree = "none") 
		: mText(text)
		, mType(controlType)
		, mFlags(FLAG_VISIBLE)
		, pData(&colorValue)
		, mColor(0xAFAFAFFF)
		, pTree(tree)
	{
		// this UI control is for color picking, and the UIControlType should be one of the two below.
		ASSERT(mType == UI_CONTROL_COLOR_PICKER || mType == UI_CONTROL_COLOR_SLIDER);
		mSettings.colorMode = 0;
	}

	// Menu
	UIProperty(const char* text, UIPropertyType controlType, const char* tree = "none")
		: mText(text)
		, mType(controlType)
		, mFlags(FLAG_VISIBLE)
		, mColor(0xAFAFAFFF)
		, pTree(tree)
	{}

	// Contextual / Context Menu
	UIProperty(const char** pContextItems, int numContextItems, ContextMenuItemCallback* pCallbacks = NULL, const char* tree = "none") :
		mText("")
		, mType(UI_CONTROL_CONTEXTUAL)
		, mFlags(FLAG_VISIBLE)
		, mColor(0xAFAFAFFF)
		, pTree(tree)
	{
		mSettings.pContextItems = pContextItems;
		mSettings.numContextItems = numContextItems;
		mSettings.pfnCallbacks = pCallbacks;
	}

	// Dropdown
	template <class T>
	UIProperty
	(
		const char* text
		, T& value
		, const char** enumNames
		, const T* enumValues
		, ControlChangedCallback callback = NULL
		, uint color = 0xAFAFAFFF
		, const char* tree = "none"
	)
		: mText(text)
		, mType(UI_CONTROL_DROPDOWN)
		, mFlags(FLAG_VISIBLE)
		, pData(&value)	// data?
		, pCallback(callback)
		, mColor(color)
		, pTree(tree)
	{
		mSettings.eByteSize = sizeof(T);
		mSettings.eNames = enumNames;
		mSettings.eValues = (const void*)enumValues;
		memset(uiState, 0, sizeof(uiState));
	}

	void setSettings(int steps, float min, float max)
	{
		ASSERT(mType == UI_CONTROL_SLIDER_FLOAT);
		mSettings.fMin = min;
		mSettings.fMax = max;
		mSettings.fIncrement = (max - min) / float(steps);
	}
	//*********************************************************/
	// The order of member variables declaration must match the order in which they are initialized in the constructor. 
	// Otherwise g++ compiler will rearrange the initializer and variables may be initialized to unknown value
	// For more info, search for keyword [-Wreorder]
	//*********************************************************/
	tinystl::string mText;
	UIPropertyType mType;
	uint mFlags;
	void* pData;
	ControlChangedCallback pCallback = NULL;
	uint mColor;
	const char* pTree = "none";

	bool mShouldDraw = false;

	// Anonymous structures generates warnings in C++11. 
	// See discussion here for more info: https://stackoverflow.com/questions/2253878/why-does-c-disallow-anonymous-structs
#pragma warning( push )
#pragma warning( disable : 4201) // warning C4201: nonstandard extension used: nameless struct/union
	union Settings
	{
		struct	// slider data
		{
			float fMin;
			float fMax;
			float fIncrement;
			bool fExpScale;
		};
		struct // slider data
		{
			int iMin;
			int iMax;
			int iIncrement;
		};
		struct // slider data
		{
			uint uiMin;
			uint uiMax;
			uint uiIncrement;
		};
		struct // dropdown data
		{
			const char** eNames;
			const void* eValues;
			int eByteSize;
		};
		struct // label data?
		{
			uint sLen;
		};
		struct // progress bar data
		{
			size_t maxProgress;
			// int editable;
		};
		struct // color picker
		{
			// enum color_mode { COL_RGB, COL_HSV };
			int colorMode;
		};
		struct // context menu items
		{
			const char** pContextItems;	// name arr
			ContextMenuItemCallback* pfnCallbacks;	// function callback arr
			int numContextItems;
		};
		struct // generic
		{
			void* pUserData;
		};
	} mSettings;
#pragma warning( pop ) 

	char uiState[8];

	void* clbCustom;
	bool(*clbVisible)(void*);

	void modify(int steps)
	{
		switch (mType)
		{
		case UI_CONTROL_SLIDER_FLOAT:
		{
			float& vCurrent = *(float*)pData;
			if (mSettings.fExpScale == false)
				vCurrent += mSettings.fIncrement * (float)steps; // linear scale
			else
				vCurrent = vCurrent + (vCurrent * mSettings.fIncrement * (float)steps); // exponential scale

			if (vCurrent > mSettings.fMax)
				vCurrent = mSettings.fMax;
			if (vCurrent < mSettings.fMin)
				vCurrent = mSettings.fMin;
			break;
		}
		case UI_CONTROL_SLIDER_INT:
		{
			int& vCurrent = *(int*)pData;
			vCurrent += mSettings.iIncrement * (int)steps;
			if (vCurrent > mSettings.iMax)
				vCurrent = mSettings.iMax;
			if (vCurrent < mSettings.iMin)
				vCurrent = mSettings.iMin;
			break;
		}
		case UI_CONTROL_SLIDER_UINT:
		{
			uint& vCurrent = *(uint*)pData;
			vCurrent += mSettings.uiIncrement * (int)steps;
			if (vCurrent > mSettings.uiMax)
				vCurrent = mSettings.uiMax;
			if (vCurrent < mSettings.uiMin)
				vCurrent = mSettings.uiMin;
			break;
		}
		case UI_CONTROL_CHECKBOX:
		{
			bool& vCurrent = *(bool*)pData;
			if (steps > 0)
				vCurrent = true;
			else
				vCurrent = false;
			break;
		}
		case UI_CONTROL_DROPDOWN:
		{
			ASSERT(mSettings.eByteSize == 4); // does not support other enums than those that are 4 bytes in size (yet)
			int& vCurrent = *(int*)pData;
			int i = enumComputeIndex();
			const int* source_ = ((const int*)mSettings.eValues);
			if (steps == 1 && mSettings.eNames[i + 1] != 0)
				vCurrent = source_[i + 1];
			if (steps == -1 && i != 0)
				vCurrent = source_[i - 1];
		}break;
		case UI_CONTROL_BUTTON:
		case UI_CONTROL_TEXTBOX:
		case UI_CONTROL_LABEL:
		case UI_CONTROL_PROGRESS_BAR:
		case UI_CONTROL_RADIO_BUTTON:
		case UI_CONTROL_COLOR_SLIDER:
		case UI_CONTROL_COLOR_PICKER:
		case UI_CONTROL_MENU:
		case UI_CONTROL_CONTEXTUAL:
			break;
		}
	}

	int enumComputeIndex() const
	{
		ASSERT(mType == UI_CONTROL_DROPDOWN);
		ASSERT(mSettings.eByteSize == 4);

		int& vCurrent = *(int*)pData;
		const int* source_ = ((const int*)mSettings.eValues);

		for (int i = 0; mSettings.eNames[i] != 0; i++)
		{
			if (source_[i] == vCurrent)
			{
				return i;
			}
		}

		return -1;
	}
};
