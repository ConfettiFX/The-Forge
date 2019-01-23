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
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../Common_3/ThirdParty/OpenSource/gainput/lib/include/gainput/gainput.h"
#ifdef METAL
#ifdef TARGET_IOS
#include "../../Common_3/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputIos.h"
#else
#include "../../Common_3/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputMac.h"
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
		mEventConsumed(false)
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

//Describes the InputAxis of the given device Button with a direction
//Direction will mainly be used to map W-S  and A-D to positive and negative joystick movements
struct TargetMapping
{
	InputAxis mAxis;
	int       mDirection;
	uint32_t  mDeviceButtonId;
};

//Describes all the device buttons for the given
//user key and device type
struct KeyMappingDescription
{
	uint32_t          mUserId;
	GainputDeviceType mDeviceType;

	//max of 2 axis (x,y)
	uint32_t mAxisCount;

	//max of 4 per button.
	//KEyboard has 2 for axis
	//Mouse has 1 per axis
	//Gamepad has 1 per axis
	//Touch has 1 per axis
	TargetMapping mMappings[4];

	//Callback function to translate
	//input received to desired input format
	//Converting raw mouse to -1,1
	//Linking touch input to virtual joysticks
	//InputCallbackFn pInputCallbackFn;
};

struct GestureMappingDescription
{
	uint32_t               mUserId;
	gainput::GestureType   mType;
	gainput::GestureConfig mConfig;
};

class InputSystem
{
	public:
	static void Shutdown();

	static void Init(const uint32_t& width, const uint32_t& height);
#ifdef METAL
	/**
	 ** Necessary for IOS otherwise we can't get touchInput
	 ** Need to attach a gainput View as a subview to the MTKView
	 ** MTKView is a subclass of UIView so we just need UIView.
	 **/
	static void InitSubView(void* view);
	static void ShutdownSubView(void* view);
#endif

#ifdef _WINDOWS
	static void HandleMessage(MSG& msg) { pInputManager->HandleMessage(msg); }
#elif defined __ANDROID__
	static int32_t HandleMessage(AInputEvent* msg) { return pInputManager->HandleInput(msg); }
#elif defined(__linux__)
	static void HandleMessage(XEvent& msg) { pInputManager->HandleEvent(msg); }
#endif
	//This will clear all active buttons.
	//if deviceType == GAINPUT_DEFAULT then we clear all active buttons from all active devices
	//if deviceType points to a specific deviceType then we clear all buttons related to that deviceType
	static void ClearInputStates(GainputDeviceType deviceType = GainputDeviceType::GAINPUT_DEFAULT);
	//Updates input system and broadcasts platform events
	static void Update(float dt = 0.016f);
	// Map from custom defined buttons to internal button mappings
	static void MapKey(const uint32_t& source_KEY, const uint32_t& userKey, GainputDeviceType device);
	// To switch between different key mappings.
	static void SetActiveInputMap(const uint32_t& index);
	//Call when Window size changes
	static void UpdateSize(const uint32_t& width, const uint32_t& height);

	/**
	 ** True if user key is registered.
	 ** param:  userKey -> Used defined ID for given button.
	 ** userKey is defined in the KeyMappingDescription that's being used. (From InputMappings.h)
	 **/
	static bool IsButtonMapped(const uint32_t& userKey);

	/**
	 ** True if button is down.
	 ** param: buttonId -> Used defined ID for given button.
	 ** ID was defined when calling Map or using one of the defaults defined in Init
	 **/
	static bool IsButtonPressed(const uint32_t& buttonId);
	/**
	 ** True iff button was pressed this frame and was not pressed previous
	 ** param: buttonId -> Used defined ID for given button.
	 ** ID was defined when calling Map or using one of the defaults defined in Init
	 **/
	static bool IsButtonTriggered(const uint32_t& buttonId);
	/**
	 ** True iff button was released this frame
	 ** param: buttonId -> Used defined ID for given button.
	 ** ID was defined when calling Map or using one of the defaults defined in Init
	 **/
	static bool IsButtonReleased(const uint32_t& buttonId);

	static bool IsMouseCaptured() { return mIsMouseCaptured; };

	// returns whether or not the button is mapped and has valid data
	// button data is saved in passed struct
	static ButtonData GetButtonData(const uint32_t& buttonId, const GainputDeviceType& device = GainputDeviceType::GAINPUT_DEFAULT);

	/**
	 ** The id for touch determines which touch finger to retrieve.
	 ** We'll get all the info for that finger
	 ** param: buttonId -> Used defined ID for given button.
	 ** ID was defined when calling Map or using one of the defaults defined in Init
	 **/

	static void SetMouseCapture(bool mouseCapture);

	static void SetHideMouseCursorWhileCaptured(bool hide) { mHideMouseCursorWhileCaptured = hide; }
	static bool GetHideMouseCursorWhileCaptured() { return mHideMouseCursorWhileCaptured; }

	static uint32_t GetDisplayWidth();
	static uint32_t GetDisplayHeight();

	typedef bool (*InputEventHandler)(const ButtonData* data);
	static void RegisterInputEvent(InputEventHandler callback, uint32_t priority = 0);
	static void UnregisterInputEvent(InputEventHandler callback);
	//helper function to reset mouse position when it goes to screen edge
	static void WarpMouse(const float& x, const float& y);

	static void ToggleVirtualTouchKeyboard(int enabled);
	static bool IsVirtualKeyboardActive() { return mVirtualKeyboardActive; }
	static void GetVirtualKeyboardTextInput(char* inputBuffer, uint32_t inputBufferSize);

	//helper function to reduce code ducplication. Given an array of key mappings will add all keys to input map.
	//This determines which device/Hardware keys are mapped to which user enums
	static void AddMappings(KeyMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings = false);

	static void AddGestureMappings(GestureMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings = false);

	private:
	// callback broadcasters for platform events
	// When a subscriber consumes an input the buttonData is modified to reflect it.
	// buttonData.mEventConsumed becomes true.
	static void OnInputEvent(ButtonData& buttonData);

	//Event listener for when a button's state changes
	//Will be triggered on event down, event up or event move.
	class DeviceInputEventListener: public gainput::InputListener
	{
		public:
		DeviceInputEventListener(int index): index_(index) {}

		bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue);

		bool OnDeviceButtonFloat(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue);

		bool OnDeviceButtonGesture(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, const gainput::GestureChange& gesture);

		int GetPriority() const { return index_; }

		private:
		int index_;
	};

	static GainputDeviceType GetDeviceType(uint32_t deviceId);

	//helper functions for default mappings
	static void SetDefaultKeyMapping();

	// gainput systems
	static gainput::InputManager* pInputManager;

	// we should have more than one map
	static tinystl::vector<gainput::InputMap*> pInputMap;
	static uint32_t                            mActiveInputMap;

	//needed for macOS/iOS to retrieve touch data
#ifdef METAL
	static void* pGainputView;
#endif

	static bool GatherInputEventButton(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue);
	static void FillButtonDataFromDesc(
		const KeyMappingDescription* keyDesc, ButtonData& toFill, float oldValue = 0.0, float newVallue = 0.0f,
		gainput::DeviceId deviceId = gainput::InvalidDeviceId, gainput::DeviceButtonId deviceButton = gainput::InvalidDeviceButtonId);
	static uint32_t GetDeviceID(GainputDeviceType deviceType);

	// What if we have more than one mouse, more than one keyboard
	// more than one gamepad or more than one touch input (probably only valid for gamepads (2 players))
	static gainput::DeviceId mMouseDeviceID;
	static gainput::DeviceId mRawMouseDeviceID;
	static gainput::DeviceId mKeyboardDeviceID;
	static gainput::DeviceId mGamepadDeviceID;
	static gainput::DeviceId mTouchDeviceID;

	// Needed to send events
	static gainput::DeviceButtonSpec mButtonsDown[32];

	//For mouse capture
	static bool mIsMouseCaptured;
	static bool mHideMouseCursorWhileCaptured;

	//for Input events
	static tinystl::vector<InputEventHandler> mInputCallbacks;
	static tinystl::vector<uint32_t>          mInputPriorities;

	//input listener to gainput mainly for triggers/releases, touch and mouse
	static DeviceInputEventListener mDeviceInputListener;
	//required to unregister the event listener
	static gainput::ListenerId mDeviceInputListnerID;

	static tinystl::unordered_map<uint32_t, tinystl::vector<KeyMappingDescription> >     mKeyMappings;
	static tinystl::unordered_map<uint32_t, tinystl::vector<GestureMappingDescription> > mGestureMappings;

	struct UserToDeviceMap
	{
		uint32_t          userMapping;
		gainput::DeviceId deviceId;
	};

	//Map holding device to user mapping
	//[device ID, vector of all user mapped ids]
	static tinystl::unordered_map<uint32_t, tinystl::vector<UserToDeviceMap> > mDeviceToUserMappings;

	static bool mVirtualKeyboardActive;
};

#endif
