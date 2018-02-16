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

#include "UI.h"
#include "UIRenderer.h"

#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IOperatingSystem.h"

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#include "../../ThirdParty/OpenSource/NuklearUI/nuklear.h"
#include "../Interfaces/IMemoryManager.h"

namespace PlatformEvents { extern bool skipMouseCapture; }

static tinystl::vector<UIAppComponentGui*> gInstances;
static Mutex gMutex;

static void uiWindowResize(const WindowResizeEventData* data)
{
  UNREF_PARAM(data);
}

#ifndef _DURANGO
static bool uiKeyboardChar(const KeyboardCharEventData* pData)
{
	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onChar(pData))
			return true;

	return false;
}

static bool uiKeyboardButton(const KeyboardButtonEventData* pData)
{
#if !defined(TARGET_IOS)
	if (pData->key == KEY_F1 && pData->pressed)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->drawGui = !gInstances[i]->drawGui;
	}
	if (pData->key == KEY_ESCAPE)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->escWasPressed = pData->pressed;
	}
    
    for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
        if (gInstances[i]->drawGui && gInstances[i]->onKey(pData))
            return true;
#else
	ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiMouseMove(const MouseMoveEventData* pData)
{
#if !defined(TARGET_IOS)
    for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
    {
        if (gInstances[i]->drawGui && gInstances[i]->onMouseMove(pData))
        {
            PlatformEvents::skipMouseCapture = gInstances[i]->onMouseMove(pData);
            return true;
        }
    }
    
    PlatformEvents::skipMouseCapture = false;
#else
    ASSERT(false && "Unsupported on target iOS");
#endif
	
	return false;
}

static bool uiMouseButton(const MouseButtonEventData* pData)
{
#if !defined(TARGET_IOS)
    for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
        if (gInstances[i]->drawGui && gInstances[i]->onMouseButton(pData))
            return true;
#else
    ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiMouseWheel(const MouseWheelEventData* pData)
{
#if !defined(TARGET_IOS)
    for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
        if (gInstances[i]->drawGui && gInstances[i]->onMouseWheel(pData))
            return true;
#else
    ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}
#endif

static bool uiJoystickButton(const JoystickButtonEventData* pData)
{
#if !defined(TARGET_IOS)
	if (pData->button == BUTTON_MENU && pData->pressed)
	{
		for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
			gInstances[i]->drawGui = !gInstances[i]->drawGui;
	}

	for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
		if (gInstances[i]->drawGui && gInstances[i]->onJoystickButton(pData))
			return true;
#else
    ASSERT(false && "Unsupported on target iOS");
#endif

	return false;
}

static bool uiTouch(const TouchEventData* pData)
{
#if defined(TARGET_IOS)
    for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
        if (gInstances[i]->drawGui && gInstances[i]->onTouch(pData))
            return true;
#else
    ASSERT(false && "Unsupported on this target.");
#endif
    
    return false;
}

static bool uiTouchMove(const TouchEventData* pData)
{
#if defined(TARGET_IOS)
    for (uint32_t i = 0; i < (uint32_t)gInstances.size(); ++i)
        if (gInstances[i]->drawGui && gInstances[i]->onTouchMove(pData))
            return true;
#else
    ASSERT(false && "Unsupported on this target.");
#endif
    
    return false;
}

// UI singular property
UIProperty::UIProperty(const char* description, float& value, float min/*=0.0f*/, float max/*=1.0f*/,
                       float increment/*=0.1f*/, bool expScale/*=false*/) :
		description(description),
		type(UI_PROPERTY_FLOAT),
		flags(FLAG_VISIBLE),
		source(&value)
{
	settings.fMin = min;
	settings.fMax = max;
	settings.fIncrement = increment;
	settings.fExpScale = expScale;
}

UIProperty::UIProperty(const char* description, int steps, float& value,
                       float min/*=0.0f*/, float max/*=1.0f*/) :
		description(description),
		type(UI_PROPERTY_FLOAT),
		flags(FLAG_VISIBLE),
		source(&value)
{
	settings.fMin = min;
	settings.fMax = max;
	settings.fIncrement = (max - min) / float(steps);
	settings.fExpScale = false;
}

UIProperty::UIProperty(const char* description, int& value, int min/*=-100*/, int max/*=100*/,
                       int increment/*=1*/) :
		description(description),
		type(UI_PROPERTY_INT),
		flags(FLAG_VISIBLE),
		source(&value)
{
	settings.iMin = min;
	settings.iMax = max;
	settings.iIncrement = increment;
}

UIProperty::UIProperty(const char* description, unsigned int& value,
                       unsigned int min/*=0*/, unsigned int max/*=100*/,
                       unsigned int increment/*=1*/) :
		description(description),
		type(UI_PROPERTY_UINT),
		flags(FLAG_VISIBLE),
		source(&value)
{
	settings.uiMin = min;
	settings.uiMax = max;
	settings.uiIncrement = increment;
}

UIProperty::UIProperty(const char* description, bool& value) :
		description(description),
		type(UI_PROPERTY_BOOL),
		flags(FLAG_VISIBLE),
		source(&value)
{
}

// TODO: must fix this in order to work on all compilers due to the callback function UIButtonFn
#ifdef _WIN32
UIProperty::UIProperty(const char* description, UIButtonFn fn, void* userdata) :
		description(description),
		type(UI_PROPERTY_BUTTON),
		flags(FLAG_VISIBLE),
		source(fn)
{
	settings.pUserData = userdata;
}
#endif

UIProperty::UIProperty(const char* description, char* value, unsigned int length) :
		description(description),
		type(UI_PROPERTY_TEXTINPUT),
		flags(FLAG_VISIBLE),
		source(value)
{
	settings.sLen = length;
}

void UIProperty::setSettings(int steps, float min, float max)
{
	ASSERT(type == UI_PROPERTY_FLOAT);
	settings.fMin = min;
	settings.fMax = max;
	settings.fIncrement = (max - min) / float(steps);
}

void UIProperty::modify(int steps)
{
	switch (type)
	{
	case UI_PROPERTY_FLOAT:
	{
		float& vCurrent = *(float*)source;
		if (settings.fExpScale == false)
			vCurrent += settings.fIncrement * (float)steps; // linear scale
		else
			vCurrent = vCurrent + (vCurrent * settings.fIncrement * (float)steps); // exponential scale

		if (vCurrent > settings.fMax)
			vCurrent = settings.fMax;
		if (vCurrent < settings.fMin)
			vCurrent = settings.fMin;
		break;
	}
	case UI_PROPERTY_INT:
	{
		int& vCurrent = *(int*)source;
		vCurrent += settings.iIncrement * (int)steps;
		if (vCurrent > settings.iMax)
			vCurrent = settings.iMax;
		if (vCurrent < settings.iMin)
			vCurrent = settings.iMin;
		break;
	}
	case UI_PROPERTY_UINT:
	{
		unsigned int& vCurrent = *(unsigned int*)source;
		vCurrent += settings.uiIncrement * (int)steps;
		if (vCurrent > settings.uiMax)
			vCurrent = settings.uiMax;
		if (vCurrent < settings.uiMin)
			vCurrent = settings.uiMin;
		break;
	}
	case UI_PROPERTY_BOOL:
	{
		bool& vCurrent = *(bool*)source;
		if (steps > 0)
			vCurrent = true;
		else
			vCurrent = false;
		break;
	}
	case UI_PROPERTY_ENUM:
	{
		ASSERT(settings.eByteSize == 4); // does not support other enums than those that are 4 bytes in size (yet)
		int& vCurrent = *(int*)source;
		int i = enumComputeIndex();
		const int* source_ = ((const int*)settings.eValues);
		if (steps == 1 && settings.eNames[i + 1] != 0)
			vCurrent = source_[i + 1];
		if (steps == -1 && i != 0)
			vCurrent = source_[i - 1];
	}break;
	case UI_PROPERTY_BUTTON:
	{

	}break;
	case UI_PROPERTY_TEXTINPUT:
	{

	}break;
	}
}

int UIProperty::enumComputeIndex() const
{
	ASSERT(type == UI_PROPERTY_ENUM);
	ASSERT(settings.eByteSize == 4);

	int& vCurrent = *(int*)source;
	const int* source_ = ((const int*)settings.eValues);

	for (int i = 0; settings.eNames[i] != 0; i++)
	{
		if (source_[i] == vCurrent)
		{
			return i;
		}
	}

	return -1;
}

// UI controller
UI::UI()
{
	onPropertyChanged = 0;
}

void UI::clearProperties()
{
	properties.clear();
}

void UI::removeProperty(unsigned int idx)
{
	UIProperty& prop = properties[idx];
	prop.source = NULL;
	prop.callback = NULL;
}

unsigned int UI::addProperty(const UIProperty& prop)
{
	// Try first to fill empty property slot
	for (unsigned int i = 0; i < properties.getCount(); i++)
	{
		UIProperty& prop_slot = properties[i];
		if (prop_slot.source != NULL)
			continue;

		prop_slot = prop;
		return i;
	}

	return properties.add(prop);
}

unsigned int UI::getPropertyCount()
{
	return properties.getCount();
}

UIProperty& UI::getProperty(unsigned int idx)
{
	return properties[idx];
}

void UI::changedProperty(unsigned int idx)
{
	if (properties[idx].callback)
		properties[idx].callback(&properties[idx]);
}

bool UI::loadProperties(const char* filename)
{
	File file = {};file.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_OtherFiles);
	if (!file.IsOpen())
	{
		return false;
	}

	unsigned totalBytes = file.GetSize();
	char* buffer = (char*)conf_malloc(totalBytes);
	file.Read(buffer, totalBytes);
	file.Close();

	char ident[1024];
	char setTo[1024];
	char* start = buffer;
	char* current = start;
	while (true)
	{
		while (*current != '=' && *current != '\0')
			++current;
		if (*current == '\0')
			break;

		// copy ident
		memcpy(ident, start, current - start);
		ident[current - start] = 0;

		// go to next char
		++current;
		start = current;

		// find null char, or a new line character (\r or \n)
		while (*current != '\r' && *current != '\n' && *current != '\0')
			++current;

		// copy set to
		memcpy(setTo, start, current - start);
		setTo[current - start] = 0;

		// load current property
		int propertyID = -1;
		for (uint i = 0; i < getPropertyCount(); i++) // TODO: linear search - slow - add a hash map/binary tree here if performance issue.
		{
			if (stricmp(getProperty(i).description, ident) == 0)
			{
				propertyID = i;
				break;
			}
		}

		// if property found, parse the data
		if (propertyID != -1)
		{
			switch (getProperty(propertyID).type)
			{
			case UI_PROPERTY_FLOAT:
				*(float*)getProperty(propertyID).source = (float)atof(setTo);
				break;
			case UI_PROPERTY_INT:
				*(int*)getProperty(propertyID).source = atoi(setTo);
				break;
			case UI_PROPERTY_UINT:
				*(unsigned int*)getProperty(propertyID).source = atoi(setTo);
				break;
			case UI_PROPERTY_BOOL:
				if (stricmp(setTo, "true") == 0 || stricmp(setTo, "1") == 0)
					*(bool*)getProperty(propertyID).source = true;
				else
					*(bool*)getProperty(propertyID).source = false;
				break;
			case UI_PROPERTY_ENUM:
				ASSERT(getProperty(propertyID).settings.eByteSize == 4); // does not support other enums than those that are 4 bytes in size (yet)
				for (int i = 0; getProperty(propertyID).settings.eNames[i] != 0; i++)
				{
					if (stricmp(getProperty(propertyID).settings.eNames[i], setTo) == 0)
					{
						*(int*)getProperty(propertyID).source = ((const int*)getProperty(propertyID).settings.eValues)[i];
					}
				}

				break;
			case UI_PROPERTY_BUTTON:
			case UI_PROPERTY_TEXTINPUT:
				break;
			}
		}

		// check if this was the last segment
		if (*current == '\0')
			break;

		// check if this was a \r, we'll need to skip another character. (getting past \r\n) - for sanity, we're ignoring old style Mac files that only have \r as line ending)
		if (*current == '\r')
			++current;

		// go to next char
		++current;
		start = current;
	}

	return true;
}

bool UI::saveProperties(const char* filename)
{
	File file = {};
	file.Open(filename, FileMode::FM_WriteBinary, FSRoot::FSR_OtherFiles);
	if (!file.IsOpen() == 0)
	{
		file.Close();
		return false;
	}

	for (unsigned int i = 0; i < getPropertyCount(); i++)
	{
		file.Write((void*)getProperty(i).description, (unsigned)strlen(getProperty(i).description));
		file.Write((void*)"=", 1);
		const uint32_t maxSize = 200;
		char display[maxSize];
		switch (getProperty(i).type)
		{
		case UI_PROPERTY_FLOAT:
			snprintf(display, maxSize, "%.3f", *(float*)getProperty(i).source);
			break;
		case UI_PROPERTY_INT:
			snprintf(display, maxSize, "%d", *(int*)getProperty(i).source);
			break;
		case UI_PROPERTY_UINT:
			snprintf(display, maxSize, "%u", *(unsigned int*)getProperty(i).source);
			break;
		case UI_PROPERTY_BOOL:
			snprintf(display, maxSize, "%s", (*(bool*)getProperty(i).source) ? "true" : "false");
			break;
		case UI_PROPERTY_ENUM:
			ASSERT(getProperty(i).enumComputeIndex() != -1);
			snprintf(display, maxSize, "%s", getProperty(i).settings.eNames[getProperty(i).enumComputeIndex()]);
		case UI_PROPERTY_BUTTON:
		case UI_PROPERTY_TEXTINPUT:
			break;
		}
		file.Write(display, (unsigned)strlen(display));
		file.Write((void*)"\r\n", 2);
	}

	file.Close();
	return true;
}

void UI::setPropertyFlag(unsigned int propertyId, UIProperty::FLAG flag, bool state)
{
	ASSERT(propertyId < getPropertyCount());
	unsigned int& flags = getProperty(propertyId).flags;
	flags = state ? (flags | flag) : (flags & ~flag);
}

UIAppComponentTextOnly::UIAppComponentTextOnly()
{
	fontSize = 12.0f;
	x = 5.0f;
	y = 150.0f;
	spacingX = 250.0f;
	spacingY = 20.0f;
	numberOfElements = 8;
	selectedID = 0;
	minID = 0;
	scrollOffset = 3;
    joystickFilterTickCounter = 0;
}

// Simple text based UI renderer
void UIAppComponentTextOnly::draw()
{
#if 0
	Fontstash* fnt = renderer->getFontstash(font);
	int fntID = fnt->getFontID("default");
	int maxID = minID + numberOfElements;
	if (maxID > (int)ui->getPropertyCount())
		maxID = (int)ui->getPropertyCount();
	int cnt = 0;

	for (int i = minID; i < maxID; i++)
	{
		bool selected = (i == selectedID);
		fnt->drawText(ui->getProperty(i).description, x + 2, y + spacingY*cnt + 2, fntID, selected ? 0xff000000 : 0xff000000, fontSize, 0.0f, 2.0f);
		fnt->drawText(ui->getProperty(i).description, x, y + spacingY*cnt, fntID, selected ? 0xff40ed7a : 0xffb3fce2, fontSize);

		char display[200];
		switch (ui->getProperty(i).type)
		{
		case UI_PROPERTY_FLOAT:
			sprintf(display, "%.3f", *(float*)ui->getProperty(i).source);
			break;
		case UI_PROPERTY_INT:
			sprintf(display, "%d", *(int*)ui->getProperty(i).source);
			break;
		case UI_PROPERTY_UINT:
			sprintf(display, "%u", *(unsigned int*)ui->getProperty(i).source);
			break;
		case UI_PROPERTY_BOOL:
			sprintf(display, "%s", (*(bool*)ui->getProperty(i).source) ? "true" : "false");
			break;
		case UI_PROPERTY_ENUM:
			ASSERT(ui->getProperty(i).enumComputeIndex() != -1);
			sprintf(display, "%s", ui->getProperty(i).settings.eNames[ui->getProperty(i).enumComputeIndex()]);
			break;
		case UI_PROPERTY_BUTTON:
		case UI_PROPERTY_TEXTINPUT:
			break;
		}

		fnt->drawText(display, x + spacingX, y + spacingY*cnt, fntID, selected ? 0xff40ed7a : 0xff77e0f7, fontSize);
		++cnt;
	}
#endif
}
void UIAppComponentTextOnly::setSelectedIndex(int idx)
{
	int propCount = ui->getPropertyCount();
	selectedID = idx;
	if (selectedID >= (int)propCount)
		selectedID = propCount - 1;
	if (selectedID < 0)
		selectedID = 0;

	int maxID = minID + numberOfElements;

	if (selectedID > maxID - scrollOffset)
		minID = selectedID + scrollOffset - numberOfElements;
	if (selectedID < minID + scrollOffset)
		minID = selectedID - scrollOffset + 1;

	maxID = minID + numberOfElements;

	if (minID < 0)
		minID = 0;
	if (maxID >= (int)propCount)
		minID = propCount - numberOfElements;
}
#if !defined(TARGET_IOS) && !defined(_DURANGO)
bool UIAppComponentTextOnly::onKey(const KeyboardButtonEventData* pData)
{
	if (pData->key == KEY_UP)
	{
		if (pData->pressed)
			goDirection(-1);
	}
	else if (pData->key == KEY_DOWN)
	{
		if (pData->pressed)
			goDirection(1);
	}
	else if (pData->key == KEY_RIGHT)
	{
		if (pData->pressed)
		{
			ui->getProperty(selectedID).modify(1);
			ui->changedProperty(selectedID);
			return true;
		}
	}
	else if (pData->key == KEY_LEFT)
	{
		if (pData->pressed)
		{
			ui->getProperty(selectedID).modify(-1);
			ui->changedProperty(selectedID);
			return true;
		}
	}
	return false;
}
#endif
bool UIAppComponentTextOnly::onJoystickButtonFiltered(int button, bool pressed)
{
	if (button == 7) // Xbox D-Pad Up
	{
		if (pressed)
			goDirection(-1);

		return true;
	}

	if (button == 8) // Xbox D-Pad Down
	{
		if (pressed)
			goDirection(1);

		return true;
	}

	if (button == 9) // Xbox D-Pad Left
	{
		if (pressed)
		{
			ui->getProperty(selectedID).modify(-1);
			ui->changedProperty(selectedID);
		}

		return true;
	}

	if (button == 10) // Xbox D-Pad Right
	{
		if (pressed)
		{
			ui->getProperty(selectedID).modify(1);
			ui->changedProperty(selectedID);
		}

		return true;
	}

	return 0;
}
bool UIAppComponentTextOnly::onJoystickButton(const int button, const bool pressed)
{
	if (pressed)
		joystickFilterTickCounter++;
	if (joystickFilterTickCounter > 20)
	{
		joystickFilterTickCounter = 0;
		return onJoystickButtonFiltered(button, pressed);
	}
	return false;
}
void UIAppComponentTextOnly::onDrawGUI()
{
	draw();
}

UIAppComponentGui::UIAppComponentGui(const TextDrawDesc* settings) : driver()
{
	selectedID = 0;
	minID = 0;
	numberOfElements = 8;
	scrollOffset = 3;

	mouse_enabled = false;
	escWasPressed = false;
	fontSize = settings->mFontSize;
	setTitle("Configuration");

	MutexLock lock(gMutex);
	gInstances.emplace_back(this);

	if (gInstances.size() == 1)
	{
		registerWindowResizeEvent(uiWindowResize);
#if !defined(_DURANGO) && !defined(TARGET_IOS)
		registerKeyboardCharEvent(uiKeyboardChar);
		registerKeyboardButtonEvent(uiKeyboardButton);
		registerMouseButtonEvent(uiMouseButton);
		registerMouseMoveEvent(uiMouseMove);
		registerMouseWheelEvent(uiMouseWheel);
#elif !defined(TARGET_IOS)
        registerJoystickButtonEvent(uiJoystickButton);
#else
        registerTouchEvent(uiTouch);
        registerTouchMoveEvent(uiTouchMove);
#endif
	}
}
UIAppComponentGui::~UIAppComponentGui()
{
	unload();

	MutexLock lock(gMutex);
	gInstances.erase(gInstances.find(this));
}
void UIAppComponentGui::load(
	int const  initialOffsetX,
	int const  initialOffsetY,
	uint const initialWidth,
	uint const initialHeight)
{
	driver.load(renderer, font, fontSize, cursorTexture);
	context = driver.getContext();

	initialWindowOffsetX = float(initialOffsetX);
	// the orginal implmentation had this 600 - y
	initialWindowOffsetY = 600 - float(initialOffsetY);
	initialWindowWidth = float(initialWidth);
	initialWindowHeight = float(initialHeight);
}

void UIAppComponentGui::unload()
{

}

void processJoystickDownState(UI* ui, int selectedID)
{
#if !defined(TARGET_IOS)
	if (getJoystickButtonDown(BUTTON_LEFT))
	{
		ui->getProperty(selectedID).modify(-1);
		ui->changedProperty(selectedID);
	}
	if (getJoystickButtonDown(BUTTON_RIGHT))
	{
		ui->getProperty(selectedID).modify(1);
		ui->changedProperty(selectedID);
	}
#endif
}

void UIAppComponentGui::update(float deltaTime)
{
  UNREF_PARAM(deltaTime);
	bool needKeyboardInputNextFrame = false;
	driver.clear();
	driver.processInput();
	processJoystickDownState(ui, selectedID);

	int result = nk_begin(context, title, nk_rect(initialWindowOffsetX, initialWindowOffsetY, initialWindowWidth, initialWindowHeight),
		NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE);

	struct nk_rect r = nk_window_get_bounds(context);
	if (!result)
	{
		r.h = nk_window_get_panel(context)->header_height;
	}
	windowRect = float4(r.x, r.y, r.w, r.h);
	if (result)
	{
		for (uint i = 0; i < ui->getPropertyCount(); i++)
		{
			UIProperty& prop = ui->getProperty(i);
			if (!(prop.flags & UIProperty::FLAG_VISIBLE))
				continue;

			if (!prop.source)
				continue;
			//width of window divided by max columns, taking into account padding
			float colWidth = initialWindowWidth / 3 - 12;
			int cols = 2;
			switch (prop.type)
			{
			case UI_PROPERTY_FLOAT:
			case UI_PROPERTY_INT:
			case UI_PROPERTY_UINT:
				cols = 3;
				break;
			default:
				break;
			}
			/*gui_panel_row_begin(&layout, GUI_STATIC, 20.0f, cols);
			gui_panel_row_push(&layout, 200.0f);
			gui_panel_label(&layout, prop.description, GUI_TEXT_LEFT);
			gui_panel_row_push(&layout, mainPanel->w - 200.0f - 50.0f - remainder);*/
            nk_layout_row_begin(context, NK_STATIC, 30.f, cols);
			nk_layout_row_push(context, colWidth);
			nk_label_wrap(context, prop.description);
			nk_layout_row_push(context, cols == 2 ? 2 * colWidth : colWidth);
			switch (prop.type)
			{
			case UI_PROPERTY_FLOAT:
			{
				float& currentValue = *(float*)prop.source;
				float oldValue = currentValue;
				nk_slider_float(context, prop.settings.fMin, (float*)prop.source, prop.settings.fMax, prop.settings.fIncrement);
				if (wantKeyboardInput)
					currentValue = oldValue;

				// * edit box
				char buffer[200];
				sprintf(buffer, "%.3f", currentValue);
				//nk_property_float(context, prop.description, prop.settings.fMin, (float*)prop.source, prop.settings.fMax, prop.settings.fIncrement, prop.settings.fIncrement / 10);
				nk_flags result_flags = nk_edit_string_zero_terminated(context, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buffer, 200, nk_filter_float);
				if (result_flags == NK_EDIT_ACTIVE)
					needKeyboardInputNextFrame = true;

				if (result_flags != NK_EDIT_INACTIVE)
					currentValue = (float)atof(buffer);

				if (result_flags & NK_EDIT_COMMITED || escWasPressed)
					nk_edit_unfocus(context);

				// actualize changes
				if (currentValue != oldValue)
				{
					ui->changedProperty(i);
				}

				break;
			}
			case UI_PROPERTY_INT:
			{
				int& currentValue = *(int*)prop.source;

				int oldValue = currentValue;
				nk_slider_int(context, prop.settings.iMin, (int*)prop.source, prop.settings.iMax, prop.settings.iIncrement);
				if (wantKeyboardInput)
					currentValue = oldValue;
				// * edit box
				char buffer[200];
				sprintf(buffer, "%i", currentValue);
				nk_flags result_flags = nk_edit_string_zero_terminated(context, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buffer, 200, nk_filter_decimal);
				if (result_flags == NK_EDIT_ACTIVE)
					needKeyboardInputNextFrame = true;

				if (result_flags != NK_EDIT_INACTIVE)
					currentValue = (int)atoi(buffer);

				if (result_flags & NK_EDIT_COMMITED || escWasPressed)
					nk_edit_unfocus(context);

				// actualize changes
				if (currentValue != oldValue)
				{
					ui->changedProperty(i);
				}
				break;
			}
			case UI_PROPERTY_UINT:
			{
				int& currentValue = *(int*)prop.source;
				int oldValue = currentValue;
				nk_slider_int(context, prop.settings.uiMin > 0 ? prop.settings.uiMin : 0, (int*)prop.source, prop.settings.uiMax, prop.settings.iIncrement);
				if (wantKeyboardInput)
					currentValue = oldValue;
				// * edit box
				char buffer[200];
				sprintf(buffer, "%u", currentValue);
				nk_flags result_flags = nk_edit_string_zero_terminated(context, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buffer, 200, nk_filter_decimal);
				if (result_flags == NK_EDIT_ACTIVE)
					needKeyboardInputNextFrame = true;

				if (result_flags != NK_EDIT_INACTIVE)
					currentValue = (int)atoi(buffer);

				if (result_flags & NK_EDIT_COMMITED || escWasPressed)
					nk_edit_unfocus(context);

				// actualize changes
				if (currentValue != oldValue)
				{
					ui->changedProperty(i);
				}

				break;
			}
			case UI_PROPERTY_BOOL:
			{
				bool& currentValue = *(bool*)prop.source;
				int value = (currentValue) ? 0 : 1;
				nk_checkbox_label(context, currentValue ? "True" : "False", &value);
				if (currentValue != (value == 0))
				{
					currentValue = (value == 0);
					ui->changedProperty(i);
				}
				break;
			}
			case UI_PROPERTY_ENUM:
			{
				ASSERT(prop.settings.eByteSize == 4);

				int current = prop.enumComputeIndex();
				int previous = current;
				int cnt = 0;
				for (int vi = 0; prop.settings.eNames[vi] != 0; vi++)
					cnt = (int)vi;

				nk_combobox(context, prop.settings.eNames, cnt + 1, &current, 16, nk_vec2(colWidth * 2, 16 * 5));

				if (previous != current)
				{
					*(int*)prop.source = ((int*)prop.settings.eValues)[current];
					ui->changedProperty(i);
				}
				break;
			}
			case UI_PROPERTY_BUTTON:
			{
				if (nk_button_label(context, prop.description))
				{
					if (prop.source)
						((UIButtonFn)prop.source)(prop.settings.pUserData);
					ui->changedProperty(i);
				}
				break;
			}
			case UI_PROPERTY_TEXTINPUT:
			{
				nk_flags result_flags = nk_edit_string_zero_terminated(context, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT, (char*)prop.source, prop.settings.sLen, nk_filter_ascii);
				if (result_flags == NK_EDIT_ACTIVE)
				{
					needKeyboardInputNextFrame = true;
				}

				if (result_flags & NK_EDIT_COMMITED || escWasPressed)
				{
					nk_edit_unfocus(context);
					ui->changedProperty(i);
				}
			}
			}
		}
	}
	nk_end(context);

	wantKeyboardInput = needKeyboardInputNextFrame;
}

void UIAppComponentGui::draw(struct Texture* rendertarget)
{
	// Mouse should be the last thing to draw
	if (mouse_enabled)
	{
		//driver.push_draw_cursor_command(&layout);
	}
	
#ifdef _DURANGO
	// Search for the currently selected property and change it's color to indicate selection status
	const struct nk_command *cmd;
	for ((cmd) = nk__begin(driver.getContext()); (cmd) != 0; (cmd) = nk__next(driver.getContext(), cmd))
	{
		if (cmd->type == NK_COMMAND_TEXT)
		{
			struct nk_command_text *textCommand = (struct nk_command_text*)cmd;
			if (strcmp((const char*)textCommand->string, ui->getProperty(selectedID).description) == 0)
			{
				// change color to indicate selection status
				textCommand->foreground.r = 1;
				textCommand->foreground.g = 1;
				textCommand->foreground.b = 1;
			}
		}
	}
#endif

	driver.draw(0, rendertarget);
}

void UIAppComponentGui::setSelectedIndex(int idx)
{
	int propCount = ui->getPropertyCount();
	selectedID = idx;
	if (selectedID >= (int)propCount)
		selectedID = propCount - 1;
	if (selectedID < 0)
		selectedID = 0;

	int maxID = minID + numberOfElements;

	if (selectedID > maxID - scrollOffset)
		minID = selectedID + scrollOffset - numberOfElements;
	if (selectedID < minID + scrollOffset)
		minID = selectedID - scrollOffset + 1;

	maxID = minID + numberOfElements;

	if (minID < 0)
		minID = 0;
	if (maxID >= (int)propCount)
		minID = propCount - numberOfElements;
}

void UIAppComponentGui::SetVirtualMouseEnabled(bool enable)
{
	mouse_enabled = enable;
}

bool UIAppComponentGui::onChar(const KeyboardCharEventData* pData)
{
	driver.onChar(pData);
	return wantKeyboardInput;
}
bool UIAppComponentGui::onKey(const KeyboardButtonEventData* pData)
{
  UNREF_PARAM(pData);
	return wantKeyboardInput;
}
bool UIAppComponentGui::onJoystickButton(const struct JoystickButtonEventData* pData)
{
	driver.onJoystick(pData->button, pData->pressed);

	if (pData->button == BUTTON_UP)
	{
		if (pData->pressed)
			goDirection(-1);

		return true;
	}

	if (pData->button == BUTTON_DOWN)
	{
		if (pData->pressed)
			goDirection(1);

		return true;
	}

	return false;
}
bool UIAppComponentGui::onMouseMove(const MouseMoveEventData* pData)
{
	driver.onMouseMove(pData);

	nk_context* context_ = driver.getContext();
	if (context_)
	{
		struct nk_rect r = nk_rect(windowRect.getX(), windowRect.getY(), windowRect.getZ(), windowRect.getW());
		if (NK_INBOX(pData->x, pData->y, r.x, r.y, r.w, r.h))
		{
			return true;
		}
	}
	return false;
}
bool UIAppComponentGui::onMouseButton(const MouseButtonEventData* pData)
{
	nk_context* context_ = driver.getContext();
	if (context_)
	{
		struct nk_rect r = nk_rect(windowRect.getX(), windowRect.getY(), windowRect.getZ(), windowRect.getW());
		if (NK_INBOX(pData->x, pData->y, r.x, r.y, r.w, r.h))
		{
			driver.onMouseClick(pData);
			return true;
		}
		// should allways let the mouse be released
		else if (pData->pressed == false)
		{
			driver.onMouseClick(pData);
		}

	}
	return false;
}
bool UIAppComponentGui::onMouseWheel(const MouseWheelEventData* pData)
{
	nk_context* context_ = driver.getContext();
	if (context_)
	{
		struct nk_rect r = nk_rect(windowRect.getX(), windowRect.getY(), windowRect.getZ(), windowRect.getW());
		if (NK_INBOX(pData->x, pData->y, r.x, r.y, r.w, r.h))
		{
			driver.onMouseScroll(pData);
			return true;
		}
	}
	return false;
}
bool UIAppComponentGui::onTouch(const struct TouchEventData*)
{
    return false; // NuklearUI doesn't support touch events.
}
bool UIAppComponentGui::onTouchMove(const struct TouchEventData*)
{
    return false; // NuklearUI doesn't support touch events.
}
void UIAppComponentGui::onDrawGUI()
{
	if (drawGui)
		draw();
}
