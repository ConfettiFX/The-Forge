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

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "NuklearGUIDriver.h"
#include <cstdint>

#include "../../OS/Interfaces/IUIManager.h"

class UI
{
public:

	UI();

	unsigned int addProperty(const UIProperty& prop);
	unsigned int getPropertyCount();
	UIProperty& getProperty(unsigned int idx);
	void changedProperty(unsigned int idx);
	void clearProperties();
	void removeProperty(unsigned int idx);

	void setOnPropertyChanged(PropertyChangedCallback clb) { onPropertyChanged = clb; }
	bool saveProperties(const char* filename);
	bool loadProperties(const char* filename);

	void setPropertyFlag(unsigned int propertyId, UIProperty::FLAG flag, bool state);

protected:
	PropertyChangedCallback onPropertyChanged;
	tinystl::vector<UIProperty> properties;
};

class UIAppComponentBase
{
public:
	UIAppComponentBase() : ui(0), renderer(0), font(-1), fontSize(12.0f), cursorTexture(NULL) {}
	UI* ui;
	class UIRenderer* renderer;
	int font;
	float fontSize;
	struct Texture* cursorTexture;

	virtual void load() {}
	virtual void unload() {}

protected:
	static const uint UI_APP_COMPONENT_TITLE_MAX_SIZE = 128;
	char title[UI_APP_COMPONENT_TITLE_MAX_SIZE]; // Made protected so that client can't corrupt mem

public:
	void setTitle(const char* title_)
	{
		strncpy_s(this->title, title_, UI_APP_COMPONENT_TITLE_MAX_SIZE);
		this->title[UI_APP_COMPONENT_TITLE_MAX_SIZE - 1] = '\0';
	}
};

class UIAppComponentTextOnly : public UIAppComponentBase
{
public:
	UIAppComponentTextOnly();

	float x;
	float y;
	float spacingX;
	float spacingY;
	int numberOfElements;
	int scrollOffset;
	int selectedID;
	int minID;
	int joystickFilterTickCounter;

	void draw();

	void goDirection(int down) { setSelectedIndex(selectedID + down); }
	void setSelectedIndex(int idx);
	bool onJoystickButtonFiltered(int button, bool pressed);

#if !defined(TARGET_IOS) && !defined(_DURANGO)
	virtual bool onKey(const struct KeyboardButtonEventData*);
#endif
	virtual bool onJoystickButton(const int button, const bool pressed);
	virtual void onDrawGUI();
};

class UIAppComponentGui : public UIAppComponentBase
{
public:
	UIAppComponentGui(const TextDrawDesc* settings);
	~UIAppComponentGui();

	NuklearGUIDriver driver;
	struct nk_context* context;
	float4 windowRect;
	int edit_activeState[1024];
	char edit_activeString[256];
	float width;
	float height;

	float initialWindowOffsetX;
	float initialWindowOffsetY;
	float initialWindowWidth;
	float initialWindowHeight;

  bool mouse_enabled;										// Enable Virtual Mouse forUI
  bool escWasPressed;
  bool drawGui = true;
	bool wantKeyboardInput;

	void load(
		int const initialOffsetX,
		int const initialOffsetY,
		uint const initialWidth,
		uint const initialHeight);

	void load() { load(0, 150, 600, 550); }

	void unload();

	void update(float deltaTime);
	void draw(struct Texture* id = NULL);

	// Enable/Disable the virtual mouse drawn on the UI. If it is enabled, UI
	// will draw a vritual mouse, otherwise it will not.
	// Developer must pass in a cursor texture to have the draw cursor command
	// Work.
	void SetVirtualMouseEnabled(bool enable);

	// returns: 0: no input handled, 1: input handled, 2: property changed
	bool onChar(const struct KeyboardCharEventData*);
	bool onKey(const struct KeyboardButtonEventData*);
	bool onJoystickButton(const struct JoystickButtonEventData*);
	bool onMouseMove(const struct MouseMoveEventData*);
	bool onMouseButton(const struct MouseButtonEventData*);
    bool onMouseWheel(const struct MouseWheelEventData*);
    bool onTouch(const struct TouchEventData*);
    bool onTouchMove(const struct TouchEventData*);
	void onDrawGUI();

private:
	int selectedID;
	int minID;
	int numberOfElements;
	int scrollOffset; 

	void setSelectedIndex(int idx);
	void goDirection(int down) { setSelectedIndex(selectedID + down); }
};
