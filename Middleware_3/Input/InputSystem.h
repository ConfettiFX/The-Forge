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
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

#include "../../Common_3/ThirdParty/OpenSource/gainput/lib/include/gainput/gainput.h"
#ifdef TARGET_IOS
#include "../../Common_3/ThirdParty/OpenSource/gainput/lib/include/gainput/GainputIos.h"
#endif

#define MAX_GAIN_MULTI_TOUCHES 7
#define MAX_GAIN_GAMEPADS 10

//used to identify different devices instead of keeping track of their ids.
enum GainputDeviceType
{
	GAINPUT_DEFAULT = 0,
    GAINPUT_RAW_MOUSE,
	GAINPUT_MOUSE,
	GAINPUT_KEYBOARD,
	GAINPUT_GAMEPAD,
	GAINPUT_TOUCH,
	DEVICE_COUNT
};

//TODO: Add Pressure for touch at one point.
// Will need to add another dimension to values.
struct ButtonData
{
	//User mapped id
	uint32_t mUserId;
	//Id mapped on device itself
	//useful for characters and numbers
	uint32_t mDeviceButtonId;

	// default button booleans
	bool mIsPressed;
	bool mIsTriggered;
	bool mIsReleased;

	//Value can be 1 value or 2 for 2D input.
	//User Should know based on provided key.
	float mValue[2];
	float mPrevValue[2];
	// the difference with previous frame
	// only valid for floating point buttons
	float mDeltaValue[2];
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
	int mDirection;
	uint32_t mDeviceButtonId;
};

//Describes all the device buttons for the given 
//user key and device type
struct KeyMappingDescription
{
	uint32_t mUserId;
	GainputDeviceType mDeviceType;

	//max of 2 axis (x,y)
	uint32_t mAxisCount;

	//max of 4 per button. 
	//KEyboard has 2 for axis
	//Mouse has 1 per axis
	//Gamepad has 1 per axis
	//Touch has 1 per axis
	TargetMapping mMappings[4];
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
#endif

#ifdef _WINDOWS
	static void HandleMessage(MSG& msg) { pInputManager->HandleMessage(msg); }
#elif defined(LINUX)
	static void HandleMessage(XEvent& msg) { pInputManager->HandleEvent(msg); }
#endif

	//Updates input system and broadcasts platform events
	static void Update(float dt = 0.016f);
	// Map from custom defined buttons to internal button mappings
	static void MapKey(const uint32_t& source_KEY, const uint32_t& userKey, GainputDeviceType device);
	// To switch between different key mappings.
	static void SetActiveInputMap(const uint32_t& index);
	//Call when Window size changes
	static void UpdateSize(const uint32_t& width,const uint32_t& height);
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


	typedef bool(*InputEventHandler)(const ButtonData* data);
	static void RegisterInputEvent(InputEventHandler callback);
	static void UnregisterInputEvent(InputEventHandler callback);
	//helper function to reset mouse position when it goes to screen edge
	static void WarpMouse(const float& x, const float& y);
 private:

	//Event listener for when a button's state changes
	//Will be triggered on event down, event up or event move.
	class DeviceInputEventListener : public gainput::InputListener
	{
	public:
		DeviceInputEventListener(int index) : index_(index) { }

		bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue);

		bool OnDeviceButtonFloat(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue);

		int GetPriority() const
		{
			return index_;
		}

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
	static uint32_t mActiveInputMap;

	//needed for iOS to retrieve touch data
#ifdef METAL
	static void * pGainputView;
#endif
	
	//callback broadcasters for platform events
	static void OnInputEvent(const ButtonData& buttonData);

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

	//for Input events
	static tinystl::vector<InputEventHandler> mInputCallbacks;

	//input listener to gainput mainly for triggers/releases, touch and mouse
	static DeviceInputEventListener mDeviceInputListener;
	//required to unregister the event listener
	static gainput::ListenerId mDeviceInputListnerID;

	static tinystl::unordered_map<uint32_t, tinystl::vector<KeyMappingDescription>> mKeyMappings;
    
    struct UserToDeviceMap
    {
        uint32_t userMapping;
        gainput::DeviceId deviceId;
    };
    
    //Map holding device to user mapping
    //[device ID, vector of all user mapped ids]
    static tinystl::unordered_map<uint32_t, tinystl::vector<UserToDeviceMap>> mDeviceToUserMappings;
};
