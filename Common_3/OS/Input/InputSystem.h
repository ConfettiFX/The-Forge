#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H
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

#pragma once
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../ThirdParty/OpenSource/gainput/lib/include/gainput/gainput.h"
#ifdef METAL
#ifdef TARGET_IOS
#include "../../ThirdParty/OpenSource/gainput/lib/include/gainput/GainputIos.h"
#else
#include "../../ThirdParty/OpenSource/gainput/lib/include/gainput/GainputMac.h"
#endif
#endif

#define MAX_GAIN_MULTI_TOUCHES 7
#define MAX_GAIN_GAMEPADS 10

//used to identify different devices instead of keeping track of their ids.
enum GainputDeviceType
{
	GAINPUT_DEFAULT = 0,
	GAINPUT_RAW_MOUSE = 1,
	GAINPUT_MOUSE = 1 << 1,
	GAINPUT_KEYBOARD = 1 << 2,
	GAINPUT_GAMEPAD = 1 << 3,
	GAINPUT_TOUCH = 1 << 4,
	DEVICE_COUNT = 6
};

//TODO: Add Pressure for touch at one point.
// Will need to add another dimension to values.
struct ButtonData
{
	ButtonData():
		mUserId(-1),
		mTouchIndex(~0u),
		mActiveDevicesMask(GAINPUT_DEFAULT),
		mIsPressed(false),
		mIsTriggered(false),
		mIsReleased(false),
		mEventConsumed(false),
		mCharacter(L'\0')
	{
		mValue[0] = 0;
		mValue[1] = 0;
		mPrevValue[0] = 0;
		mPrevValue[1] = 0;
		mDeltaValue[0] = 0;
		mDeltaValue[1] = 0;
	}
	ButtonData(const ButtonData& rhs):
		mUserId(rhs.mUserId),
		mTouchIndex(rhs.mTouchIndex),
		mActiveDevicesMask(rhs.mActiveDevicesMask),
		mIsPressed(rhs.mIsPressed),
		mIsTriggered(rhs.mIsTriggered),
		mIsReleased(rhs.mIsReleased),
		mEventConsumed(rhs.mEventConsumed)
	{
		mValue[0] = rhs.mValue[0];
		mValue[1] = rhs.mValue[1];
		mPrevValue[0] = rhs.mPrevValue[0];
		mPrevValue[1] = rhs.mPrevValue[1];
		mDeltaValue[0] = rhs.mDeltaValue[0];
		mDeltaValue[1] = rhs.mDeltaValue[1];
		mCharacter = rhs.mCharacter;
	}

	//User mapped id
	uint32_t mUserId;
	uint32_t mTouchIndex;

	//Determines what kind of device the input comes from
	//Windows can have both controller and mouse mapped to right stick
	//camera code changes based on that since raw mouse data is not normalized and gives us better control
	GainputDeviceType mActiveDevicesMask;

	// default button booleans
	bool mIsPressed;
	bool mIsTriggered;
	bool mIsReleased;
	// True when an event subscriber consumed this input event.
	bool mEventConsumed;

	//Value can be 1 value or 2 for 2D input.
	//User Should know based on provided key.
	float mValue[2];
	float mPrevValue[2];
	// the difference with previous frame
	// only valid for floating point buttons
	float   mDeltaValue[2];
	wchar_t mCharacter;
};

//Used for Mapping multiple keys to a joystick
//X axis goes into [0] of values in ButtonData
//Y axis goes into [1] of values in ButtonData
enum InputAxis
{
	INPUT_X_AXIS = 0,
	INPUT_Y_AXIS,
	INPUT_AXIS_COUNT
};

enum ActionMask
{
	ACTION_ACTIVE_SHIFT = 30,
	ACTION_DELTA_VALUE_SHIFT = 31,
	ACTION_ACTIVE = (1 << ACTION_ACTIVE_SHIFT),
	ACTION_DELTA_VALUE = (1 << ACTION_DELTA_VALUE_SHIFT),
	ACTION_PRESSED = ACTION_ACTIVE,
	ACTION_TRIGGERED = ACTION_ACTIVE | ACTION_DELTA_VALUE,
	ACTION_RELEASED = ACTION_DELTA_VALUE,
	DEVICE_BUTTON_MASK = 0xFFFF,
	ACTION_MASK_MASK = ACTION_ACTIVE | ACTION_DELTA_VALUE,
};

#define DEFINE_DEVICE_ACTION(key, actions) (((uint32_t)key)|((uint32_t)actions))
//#define DEFINE_DEVICE_ACTION(device, key, actions) (((device)<<16)|(key)|(actions))
//#define DEFINE_USER_INPUT(inputId, axis) (((inputId)<<4)|(axis))

//Describes all the device buttons for the given user key and device type
struct KeyMappingDescription
{
	uint32_t  mUserId;
	InputAxis mAxis;

	GainputDeviceType mDeviceType;
	uint32_t          mDeviceAction;

	float mScale;
};

struct GestureMappingDescription
{
	uint32_t               mUserId;
	gainput::GestureType   mType;
	gainput::GestureConfig mConfig;
};

namespace InputSystem
{
#ifndef NO_GAINPUT
	void Init(uint32_t width, uint32_t height);
	void Shutdown();

#ifdef METAL
	/**
	 ** Necessary for IOS otherwise we can't get touchInput
	 ** Need to attach a gainput View as a subview to the MTKView
	 ** MTKView is a subclass of UIView so we just need UIView.
	 **/
	void InitSubView(void* view);
	void ShutdownSubView(void* view);
#endif

#ifdef _WINDOWS
	void HandleMessage(MSG& msg);
#elif defined __ANDROID__
	int32_t HandleMessage(AInputEvent* msg);
#elif defined(__linux__)
	void HandleMessage(XEvent& msg);
#endif

	typedef bool(*InputEventHandler)(const ButtonData* data);
	void RegisterInputEvent(InputEventHandler callback, uint32_t priority = 0);
	void UnregisterInputEvent(InputEventHandler callback);

	//This will clear all active buttons.
	//if deviceType == GAINPUT_DEFAULT then we clear all active buttons from all active devices
	//if deviceType points to a specific deviceType then we clear all buttons related to that deviceType
	void ClearInputStates(GainputDeviceType deviceType = GainputDeviceType::GAINPUT_DEFAULT);
	//Updates input system and broadcasts platform events
	void Update(float dt = 0.016f);
	//Call when Window size changes
	void UpdateSize(uint32_t width, uint32_t height);

	void WarpMouse(float x, float y);
	bool IsMouseCaptured();
	void SetMouseCapture(bool mouseCapture);
	void SetHideMouseCursorWhileCaptured(bool hide);
	bool GetHideMouseCursorWhileCaptured();

	void ToggleVirtualKeyboard(int enabled);
	bool IsVirtualKeyboardActive();
	void GetVirtualKeyboardTextInput(char* inputBuffer, uint32_t inputBufferSize);

	//helper function to reduce code ducplication. Given an array of key mappings will add all keys to input map.
	//This determines which device/Hardware keys are mapped to which user enums
	void AddMappings(KeyMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings = false);
	void AddGestureMappings(GestureMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings = false);
	bool IsButtonMapped(uint32_t inputId);
	bool GetBoolInput(uint32_t inputId);
	float GetFloatInput(uint32_t inputId, uint32_t axis = 0);
#else
	void Init(uint32_t width, uint32_t height) {}
	void Shutdown() {}

#ifdef METAL
	void InitSubView(void* view) {}
	void ShutdownSubView(void* view) {}
#endif

#ifdef _WINDOWS
	void HandleMessage(MSG& msg) {}
#elif defined __ANDROID__
	int32_t HandleMessage(AInputEvent* msg) { return 0; }
#elif defined(__linux__)
	void HandleMessage(XEvent& msg) {}
#endif

	typedef bool(*InputEventHandler)(const ButtonData* data);
	void RegisterInputEvent(InputEventHandler callback, uint32_t priority = 0) {}
	void UnregisterInputEvent(InputEventHandler callback) {}

	void ClearInputStates(GainputDeviceType deviceType = GainputDeviceType::GAINPUT_DEFAULT) {}
	void Update(float dt = 0.016f) {}
	void UpdateSize(uint32_t width, uint32_t height) {}

	void WarpMouse(float x, float y) {}
	bool IsMouseCaptured() { return false; }
	void SetMouseCapture(bool mouseCapture) {}
	void SetHideMouseCursorWhileCaptured(bool hide) {}
	bool GetHideMouseCursorWhileCaptured() { return false; }

	void ToggleVirtualKeyboard(int enabled) {}
	bool IsVirtualKeyboardActive() { return false; }
	void GetVirtualKeyboardTextInput(char* inputBuffer, uint32_t inputBufferSize) {}

	void  AddMappings(KeyMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings = false) {}
	void  AddGestureMappings(GestureMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings = false) {}
	bool  IsButtonMapped(uint32_t inputId) { return false; }
	bool  GetBoolInput(uint32_t inputId) { return false; }
	float GetFloatInput(uint32_t inputId, uint32_t axis = 0) { return 0.0; }
#endif
};

#endif
