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
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IFileSystem.h"

#include "InputSystem.h"
#include "InputMappings.h"

#ifdef __linux__
#include <climits>
#endif

#ifdef METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#ifdef TARGET_IOS
#include <UIKit/UIView.h>
#include "../../ThirdParty/OpenSource/gainput/lib/source/gainput/touch/GainputInputDeviceTouchIos.h"
#else
#import <Cocoa/Cocoa.h>
#endif
#endif

#include "../Interfaces/IMemoryManager.h"

/**
//List of TODO:
//Change HandleButtonBool to mirror HandleButtonFloat and unify GetButtonData + HandleButton common logic for detecting which buttons need to be queried.
//Add potential callback for KeyMappingDescription to make it easier to map custom input to a joystick button/axis.  (Touch -> Mouse -> Joystick need to all be the same on all client code + camera code).
//Fix UI navigation and selection (Unify mouse vs joystick dpad)
//Sometimes Touch joystick gets stuck. Need to investigate further, could be caused by gainput or some bad logic in detecting release of touch.
//Add concept of virtual joystick for unifying button data. It's needed for Touch data Working with Virtual joystick in UI.
//need to add max,min to input mapping
//Add Mouse wheel
**/

//Event listener for when a button's state changes
//Will be triggered on event down, event up or event move.
class DeviceInputEventListener: public gainput::InputListener
{
	public:
	DeviceInputEventListener(int index): index_(index)
	{
	}

	bool OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue);

	bool OnDeviceButtonFloat(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue);

	bool OnDeviceButtonGesture(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, const gainput::GestureChange& gesture);

	int GetPriority() const
	{
		return index_;
	}

	private:
	int index_;
};

struct UserToDeviceMap
{
	uint32_t          userMapping;
	gainput::DeviceId deviceId;
};

//all the input devices we need
static gainput::DeviceId   mMouseDeviceID = gainput::InvalidDeviceId;
static gainput::DeviceId   mRawMouseDeviceID = gainput::InvalidDeviceId;
static gainput::DeviceId   mKeyboardDeviceID = gainput::InvalidDeviceId;
static gainput::DeviceId   mGamepadDeviceID = gainput::InvalidDeviceId;
static gainput::DeviceId   mTouchDeviceID = gainput::InvalidDeviceId;
static gainput::ListenerId mDeviceInputListnerID = -1;    //max uint instead of including headers for UINT_MAX on linux.

//system vars
static bool mIsMouseCaptured = false;
static bool mHideMouseCursorWhileCaptured = true;
static bool mVirtualKeyboardActive = false;

// we should have more than one map
static tinystl::unordered_map<uint32_t, tinystl::vector<KeyMappingDescription>>     mKeyMappings;
static tinystl::unordered_map<uint32_t, tinystl::vector<GestureMappingDescription>> mGestureMappings;
static tinystl::unordered_map<uint32_t, tinystl::vector<UserToDeviceMap>>           mDeviceToUserMappings;

static tinystl::vector<InputSystem::InputEventHandler> mInputCallbacks;
static tinystl::vector<uint32_t>                       mInputPriorities;

// gainput systems
static gainput::InputManager* pInputManager = NULL;

static DeviceInputEventListener mDeviceInputListener(0);

#ifdef METAL
void* pGainputView = NULL;
#endif

static void              OnInputEvent(ButtonData& buttonData);
static GainputDeviceType GetDeviceType(uint32_t deviceId);
static void              SetDefaultKeyMapping();
static uint32_t          GetDeviceID(GainputDeviceType deviceType);
static void              MapKey(uint32_t sourceKey, uint32_t userKey, GainputDeviceType inputDevice);
static void              FillButtonDataImmediate(const KeyMappingDescription* keyMapping, ButtonData& button);
static void              FillButtonDataFromDesc(const KeyMappingDescription* keyDesc, ButtonData& toFill, float oldValue, float newValue);

namespace InputSystem {

void Init(uint32_t width, uint32_t height)
{
	//create input manager
	pInputManager = conf_placement_new<gainput::InputManager>(conf_calloc(1, sizeof(gainput::InputManager)));
	ASSERT(pInputManager);

	//Set display size
	pInputManager->SetDisplaySize(width, height);
	mIsMouseCaptured = false;
	mVirtualKeyboardActive = false;

	//default device ids
	mMouseDeviceID = gainput::InvalidDeviceId;
	mRawMouseDeviceID = gainput::InvalidDeviceId;
	mKeyboardDeviceID = gainput::InvalidDeviceId;
	mGamepadDeviceID = gainput::InvalidDeviceId;
	mTouchDeviceID = gainput::InvalidDeviceId;
	mDeviceInputListnerID = -1;

	// create all necessary devices
	// TODO: check for failure.
	mMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>();
	mRawMouseDeviceID = pInputManager->CreateDevice<gainput::InputDeviceMouse>(mMouseDeviceID + 1, gainput::InputDeviceMouse::DV_RAW);
	mKeyboardDeviceID = pInputManager->CreateDevice<gainput::InputDeviceKeyboard>();
	mGamepadDeviceID = pInputManager->CreateDevice<gainput::InputDevicePad>();
	mTouchDeviceID = pInputManager->CreateDevice<gainput::InputDeviceTouch>();
	mDeviceInputListnerID = pInputManager->AddListener(&mDeviceInputListener);

#ifndef METAL
	// On Metal (macOS/iOS) we defer SetDefaultKeyMapping until we initialize the gainput view
	// For gesture support as they need to be registered in the view itself
	SetDefaultKeyMapping();
#endif
}

void Shutdown()
{
	if (mDeviceInputListnerID != -1) pInputManager->RemoveListener(mDeviceInputListnerID);

#ifdef METAL
	//automatic reference counting
	//it will get deallocated.
	if (pGainputView) pGainputView = NULL;
#endif

	if (pInputManager)
	{
		//call destructor explicitly
		//Since we use placement new with calloc
		pInputManager->~InputManager();
		conf_free(pInputManager);
		pInputManager = NULL;
	}
	mKeyMappings.clear();
	mInputCallbacks.clear();
}

#ifdef _WINDOWS
void HandleMessage(MSG& msg)
{
	pInputManager->HandleMessage(msg);
}
#elif defined __ANDROID__
int32_t HandleMessage(AInputEvent* msg)
{
	return pInputManager->HandleInput(msg);
}
#elif defined(__linux__)
void HandleMessage(XEvent& msg)
{
	pInputManager->HandleEvent(msg);
}
#endif

void ClearInputStates(GainputDeviceType deviceType)
{
	gainput::DeviceId deviceId = GetDeviceID(deviceType);
	if (pInputManager) pInputManager->ClearAllStates(deviceId);
}

void Update(float dt)
{
	// update gainput manager
	if (pInputManager) pInputManager->Update();
}

void RegisterInputEvent(InputEventHandler callback, uint32_t priority)
{
	uint32_t index = 0;
	for (uint32_t i = 0; i < (uint32_t)mInputPriorities.size(); ++i)
	{
		if (priority > mInputPriorities[i])
		{
			index = i;
			break;
		}
		else
		{
			++index;
		}
	}

	mInputPriorities.insert(mInputPriorities.begin() + index, priority);
	mInputCallbacks.insert(mInputCallbacks.begin() + index, callback);
}

void UnregisterInputEvent(InputEventHandler callback)
{
	InputEventHandler* it = mInputCallbacks.find(callback);
	if (it != mInputCallbacks.end())
	{
		uint64_t index = it - mInputCallbacks.begin();
		mInputCallbacks.erase(it);
		mInputPriorities.erase(mInputPriorities.begin() + index);
	}
}

#ifdef METAL
void ShutdownSubView(void* view)
{
	if (!view) return;
	//automatic reference counting
	//it will get deallocated.
	if (pGainputView)
	{
#ifndef TARGET_IOS
		GainputMacInputView* view = (GainputMacInputView*)CFBridgingRelease(pGainputView);
#else
		GainputView* view = (GainputView*)CFBridgingRelease(pGainputView);
#endif
		[view removeFromSuperview];
		pGainputView = NULL;
		view = NULL;
	}
}

void InitSubView(void* view)
{
	if (!view) return;
	ShutdownSubView(view);
#ifdef TARGET_IOS
	UIView*      mainView = (UIView*)CFBridgingRelease(view);
	GainputView* newView = [[GainputView alloc] initWithFrame:mainView.bounds inputManager:*pInputManager];
	//we want everything to resize with main view.
	[newView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleTopMargin |
								  UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin |
								  UIViewAutoresizingFlexibleBottomMargin)];
#else
	MTKView* mainView = (MTKView*)CFBridgingRelease(view);
	float retinScale = mainView.drawableSize.width / mainView.frame.size.width;
	GainputMacInputView* newView = [[GainputMacInputView alloc] initWithFrame:mainView.bounds
																	   window:mainView.window
																  retinaScale:retinScale
																 inputManager:*pInputManager];
	newView.nextKeyView = mainView;
	[newView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
#endif

	[mainView addSubview:newView];
	pGainputView = (__bridge void*)newView;

	// On Metal (macOS/iOS) we defer SetDefaultKeyMapping until we initialize the gainput view
	// For gesture support as they need to be registered in the view itself
	SetDefaultKeyMapping();
}
#endif

bool IsButtonMapped(uint32_t userKey)
{
	if (userKey > UserInputKeys::KEY_COUNT) return false;

	if (mKeyMappings.find(userKey) == mKeyMappings.end()) return false;

	return false;
}

bool GetBoolInput(uint32_t inputId)
{
	tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[inputId];

	if (keyMappings.size() == 0)
	{
		LOGWARNING("Couldn't find map from user to Description ");
		return false;
	}

	bool result = false;
	// Gather all combinations of device buttons that could affect this user key
	for (uint32_t i = 0; i < keyMappings.size(); i++)
	{
		KeyMappingDescription& keyMapping = keyMappings[i];
		uint32_t               descDeviceId = GetDeviceID(keyMapping.mDeviceType);
		gainput::InputDevice*  device = pInputManager->GetDevice(descDeviceId);
		uint32_t               button = keyMapping.mDeviceAction & DEVICE_BUTTON_MASK;
		uint32_t               actionMask = keyMapping.mDeviceAction & ACTION_MASK_MASK;

		//check that button is valid, Maybe input device has a missing button (Different gamepads)
		if (!device->IsValidButtonId(button)) continue;

		gainput::ButtonType type = device->GetButtonType(button);
		if (type == gainput::ButtonType::BT_FLOAT)
		{
			float currValue = device->GetFloat(button);
			float prevValue = device->GetFloatPrevious(button);

			result = result || (currValue != prevValue);
		}
		else
		{
			int currValue = device->GetBool(button);
			int prevValue = device->GetBoolPrevious(button);

			int      changed = currValue ^ prevValue;
			uint32_t mask = (changed << ACTION_DELTA_VALUE_SHIFT) | (currValue << ACTION_ACTIVE_SHIFT);
			result = result || (actionMask == mask);
		}
	}

	return result;
}

float GetFloatInput(uint32_t inputId, uint32_t axis)
{
	tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[inputId];

	if (keyMappings.size() == 0)
	{
		LOGWARNING("Couldn't find map from user to Description ");
		return false;
	}

	float result = 0.0f;
	// Gather all combinations of device buttons that could affect this user key
	for (uint32_t i = 0; i < keyMappings.size(); i++)
	{
		KeyMappingDescription& keyMapping = keyMappings[i];

		if (keyMapping.mAxis != axis) continue;

		uint32_t              descDeviceId = GetDeviceID(keyMapping.mDeviceType);
		gainput::InputDevice* device = pInputManager->GetDevice(descDeviceId);
		uint32_t              button = keyMapping.mDeviceAction & DEVICE_BUTTON_MASK;
		uint32_t              actionMask = keyMapping.mDeviceAction & ACTION_MASK_MASK;

		//check that button is valid, Maybe input device has a missing button (Different gamepads)
		if (!device->IsValidButtonId(button)) continue;

		gainput::ButtonType type = device->GetButtonType(button);
		if (type == gainput::ButtonType::BT_FLOAT)
		{
			float currValue = device->GetFloat(button);
			float prevValue = device->GetFloatPrevious(button);

			if (actionMask & ACTION_DELTA_VALUE)
			{
				currValue -= prevValue;
			}

			result += currValue * keyMapping.mScale;
		}
		else
		{
			int currValue = device->GetBool(button);
			int prevValue = device->GetBoolPrevious(button);

			int      changed = currValue ^ prevValue;
			uint32_t mask = (changed << ACTION_DELTA_VALUE_SHIFT) | (currValue << ACTION_ACTIVE_SHIFT);
			result += float(actionMask == mask) * keyMapping.mScale;
		}
	}

	return result;
}

void AddMappings(KeyMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings)
{
	if (!mappings || mappingCount <= 0)
	{
		LOGWARNING("No mappings were added. Provided Mapping was empty or null.");
		return;
	}

	//clear mappings data structures if required
	if (overrideMappings)
	{
		mKeyMappings.clear();
		mDeviceToUserMappings.clear();
	}

	for (uint32_t i = 0; i < mappingCount; i++)
	{
		KeyMappingDescription* mapping = &mappings[i];

		//skip uninitialized devices.
		if (GetDeviceID(mapping->mDeviceType) == gainput::InvalidDeviceId) continue;

		//create empty key mapping object if hasn't been found
		if (mKeyMappings.find(mapping->mUserId) == mKeyMappings.end()) { mKeyMappings[mapping->mUserId] = {}; }

		//add key mapping to user mapping
		mKeyMappings[mapping->mUserId].push_back(*mapping);

		//map every key + axis
		MapKey(mapping->mDeviceAction & DEVICE_BUTTON_MASK, mapping->mUserId, mapping->mDeviceType);
	}
}

void AddGestureMappings(GestureMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings)
{
	if (!mappings || mappingCount <= 0)
	{
		LOGWARNING("No mappings were added. Provided Mapping was empty or null.");
		return;
	}

	//clear mappings data structures if required
	if (overrideMappings) { mGestureMappings.clear(); }

	for (uint32_t i = 0; i < mappingCount; i++)
	{
		GestureMappingDescription* mapping = &mappings[i];

		//create empty key mapping object if hasn't been found
		if (mGestureMappings.find(mapping->mUserId) == mGestureMappings.end()) { mGestureMappings[mapping->mUserId] = {}; }

		//add key mapping to user mapping
		mGestureMappings[mapping->mUserId].push_back(*mapping);

#if defined(TARGET_IOS)
		GainputView* view = (__bridge GainputView*)pGainputView;
		[view addGestureMapping:mapping->mType forId:mapping->mUserId withConfig:mapping->mConfig];
#endif
	}
}

void SetMouseCapture(bool mouseCapture)
{
	mIsMouseCaptured = mouseCapture;
#if defined(METAL) && !defined(TARGET_IOS)
	GainputMacInputView* view = (__bridge GainputMacInputView*)(pGainputView);
	[view SetMouseCapture:(mouseCapture && mHideMouseCursorWhileCaptured)];
	view = NULL;
#endif
}

bool IsMouseCaptured()
{
	return mIsMouseCaptured;
}

void SetHideMouseCursorWhileCaptured(bool hide)
{
	mHideMouseCursorWhileCaptured = hide;
}

bool GetHideMouseCursorWhileCaptured()
{
	return mHideMouseCursorWhileCaptured;
}

void UpdateSize(uint32_t width, uint32_t height)
{
	pInputManager->SetDisplaySize(width, height);
}

void WarpMouse(float x, float y)
{
	gainput::InputDevice* device = pInputManager->GetDevice(mRawMouseDeviceID);
	device->WarpMouse(x, y);
}

void ToggleVirtualKeyboard(int keyboardType)
{
#ifdef TARGET_IOS
	if (!pGainputView) return;

	if ((keyboardType > 0) != mVirtualKeyboardActive)
		mVirtualKeyboardActive = (keyboardType > 0);
	else
		return;

	GainputView* view = (__bridge GainputView*)(pGainputView);
	[view setVirtualKeyboard:keyboardType];
#endif
}

bool IsVirtualKeyboardActive()
{
	return mVirtualKeyboardActive;
}

void GetVirtualKeyboardTextInput(char* inputBuffer, uint32_t inputBufferSize)
{
	if (!inputBuffer) return;
#ifdef TARGET_IOS
	gainput::InputDeviceTouch* touchDevice = ((gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID));
	if (touchDevice) touchDevice->GetVirtualKeyboardInput(&inputBuffer[0], inputBufferSize);
#endif
}

}    // namespace InputSystem

bool GatherInputEventButton(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
{
	tinystl::vector<UserToDeviceMap>& userButtons = mDeviceToUserMappings[deviceButton];

	GainputDeviceType deviceType = GetDeviceType(deviceId);
	bool              isMapped = false;
	
	if (deviceType == GainputDeviceType::GAINPUT_DEFAULT)
	{
		return false;
	}

	for (uint32_t i = 0; i < userButtons.size(); i++)
	{
		if (userButtons[i].deviceId != deviceId) continue;

		ButtonData button = {};
		button.mUserId = userButtons[i].userMapping;

		//here it means one user key maps to multiple device button
		//such as left stick with w-a-s-d or left stick with touchx, touchy
		tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[userButtons[i].userMapping];

		if (keyMappings.size() == 0)
		{
			LOGWARNING("Couldn't find map from user to Description ");
			continue;
		}

		//for every user key mapping
		for (uint32_t i = 0; i < keyMappings.size(); i++)
		{
			//look for given device type
			if (keyMappings[i].mDeviceType != deviceType)
			{
				continue;
			}
			if ((keyMappings[i].mDeviceAction & DEVICE_BUTTON_MASK) == deviceButton)
			{
				FillButtonDataFromDesc(&keyMappings[i], button, oldValue, newValue);
			}
			else
			{
				FillButtonDataImmediate(&keyMappings[i], button);
			}
		}

		isMapped = true;
		//broadcast event
		OnInputEvent(button);
	}

	return isMapped;
}

static uint32_t GetDeviceID(GainputDeviceType deviceType)
{
	switch (deviceType)
	{
		case GAINPUT_MOUSE: return mMouseDeviceID; break;
		case GAINPUT_RAW_MOUSE: return mRawMouseDeviceID; break;
		case GAINPUT_KEYBOARD: return mKeyboardDeviceID; break;
		case GAINPUT_GAMEPAD: return mGamepadDeviceID; break;
		case GAINPUT_TOUCH: return mTouchDeviceID; break;
		case GAINPUT_DEFAULT:
		case DEVICE_COUNT: return gainput::InvalidDeviceButtonId; break;
		default: break;
	}
	return gainput::InvalidDeviceButtonId;
}

static void OnInputEvent(ButtonData& buttonData)
{
	for (InputSystem::InputEventHandler& callback : mInputCallbacks)
	{
		// Check if event was consumed
		if (callback(&buttonData))
		{
			// If it was consumed then
			// Update buttonData to reflect consumed event for
			// callbacks with lesser priority
			buttonData.mEventConsumed = true;
		}
	}
}

static void SetDefaultKeyMapping()
{
	mKeyMappings.clear();

#ifdef _DURANGO
	uint32_t entryCount = sizeof(gXboxMappings) / sizeof(KeyMappingDescription);
	InputSystem::AddMappings(gXboxMappings, entryCount);
#else
	uint32_t entryCount = sizeof(gUserKeys) / sizeof(KeyMappingDescription);
	uint32_t controllerEntryCount = sizeof(gXboxMappings) / sizeof(KeyMappingDescription);
	uint32_t gesturesEntryCount = sizeof(gGestureMappings) / sizeof(GestureMappingDescription);
	InputSystem::AddMappings(gUserKeys, entryCount);
	InputSystem::AddMappings(gXboxMappings, controllerEntryCount);
	InputSystem::AddGestureMappings(gGestureMappings, gesturesEntryCount);
#endif
}

static GainputDeviceType GetDeviceType(uint32_t deviceId)
{
	if (deviceId == mRawMouseDeviceID) { return GainputDeviceType::GAINPUT_RAW_MOUSE; }
	else if (deviceId == mMouseDeviceID)
	{
		return GainputDeviceType::GAINPUT_MOUSE;
	}
	else if (deviceId == mKeyboardDeviceID)
	{
		return GainputDeviceType::GAINPUT_KEYBOARD;
	}
	else if (deviceId == mTouchDeviceID)
	{
		return GainputDeviceType::GAINPUT_TOUCH;
	}
	else if (deviceId == mGamepadDeviceID)
	{
		return GainputDeviceType::GAINPUT_GAMEPAD;
	}

	return GainputDeviceType::GAINPUT_DEFAULT;
}

static void MapKey(uint32_t sourceKey, uint32_t userKey, GainputDeviceType inputDevice)
{
	gainput::DeviceId           currDeviceId = GetDeviceID(inputDevice);
	const gainput::InputDevice* device = pInputManager->GetDevice(currDeviceId);

	if (device == NULL)
	{
		LOGERROR("Unsupported Input Device");
		return;
	}

	//maps to lookup device ID and Input types
	UserToDeviceMap toAdd;
	toAdd.deviceId = currDeviceId;
	toAdd.userMapping = userKey;
	mDeviceToUserMappings[sourceKey].push_back(toAdd);
}

static void FillButtonDataImmediate(const KeyMappingDescription* keyMapping, ButtonData& button)
{
	ASSERT(keyMapping != NULL);

	//get device associated with the input mapping descriptor.
	uint32_t              descDeviceId = GetDeviceID(keyMapping->mDeviceType);
	gainput::InputDevice* device = pInputManager->GetDevice(descDeviceId);
	//this determines how many device buttons affects the current user defined key.
	//for example
	//Pads have 4 axes (one key for each direction)
	//Joysticks and mouse wheel have 2 axes
	//Single keyboard buttons or mouse button have a single axis.
	uint32_t deviceButton = keyMapping->mDeviceAction & DEVICE_BUTTON_MASK;

	//check that button is valid, Maybe input device has a missing button (Different gamepads)
	if (!device->IsValidButtonId(deviceButton)) return;

	gainput::ButtonType type = device->GetButtonType(deviceButton);
	if (type == gainput::ButtonType::BT_FLOAT)
	{
		float currValue = device->GetFloat(deviceButton);
		float prevValue = device->GetFloatPrevious(deviceButton);

		//for raw mouse we need to check delta to update button data
		//otherwise we get accumulated movement values.
		if (keyMapping->mDeviceType == GAINPUT_RAW_MOUSE && currValue == prevValue) return;

		button.mValue[keyMapping->mAxis] += currValue * keyMapping->mScale;
		button.mPrevValue[keyMapping->mAxis] += prevValue * keyMapping->mScale;
	}
	else
	{
		float currValue = device->GetBool(deviceButton) ? 1.0f : 0.0f;
		float prevValue = device->GetBoolPrevious(deviceButton) ? 1.0f : 0.0f;

		button.mValue[keyMapping->mAxis] += currValue * keyMapping->mScale;
		button.mPrevValue[keyMapping->mAxis] += prevValue * keyMapping->mScale;
	}

	button.mDeltaValue[0] = button.mValue[0] - button.mPrevValue[0];
	button.mDeltaValue[1] = button.mValue[1] - button.mPrevValue[1];

	float prevFrameDelta = (abs(button.mPrevValue[0]) + abs(button.mPrevValue[1]));
	bool  isPressed = (abs(button.mValue[0]) + abs(button.mValue[1])) > 0.0f;

	button.mIsPressed = isPressed;
	button.mIsReleased = prevFrameDelta > 0.f && (!button.mIsPressed ? true : false);
	button.mIsTriggered = prevFrameDelta == 0.0f && (button.mIsPressed ? true : false);

	//if button is pressed because of this device then add it's type to the mask
	if (button.mIsPressed || button.mIsReleased || button.mIsTriggered)
	{
		button.mActiveDevicesMask = (GainputDeviceType)(button.mActiveDevicesMask | keyMapping->mDeviceType);
	}
}

/**
* Given a specific Key mapping descriptor this function will fill the button data with correct input values.
* The function can be used to fill the data from a gainput event.
*/
static void FillButtonDataFromDesc(const KeyMappingDescription* keyMapping, ButtonData& button, float oldValue, float newValue)
{
	ASSERT(keyMapping != NULL);

	//get device associated with the input mapping descriptor.
	uint32_t              descDeviceId = GetDeviceID(keyMapping->mDeviceType);
	gainput::InputDevice* device = pInputManager->GetDevice(descDeviceId);
	//this determines how many device buttons affects the current user defined key.
	//for example
	//Pads have 4 axes (one key for each direction)
	//Joysticks and mouse wheel have 2 axes
	//Single keyboard buttons or mouse button have a single axis.
	uint32_t deviceButton = keyMapping->mDeviceAction & DEVICE_BUTTON_MASK;

	//check that button is valid, Maybe input device has a missing button (Different gamepads)
	if (!device->IsValidButtonId(deviceButton)) return;

	gainput::ButtonType type = device->GetButtonType(deviceButton);
	if (type == gainput::ButtonType::BT_FLOAT)
	{
		//for raw mouse we need to check delta to update button data
		//otherwise we get accumulated movement values.
		if (keyMapping->mDeviceType == GAINPUT_RAW_MOUSE && newValue == oldValue) return;

		button.mValue[keyMapping->mAxis] = newValue * keyMapping->mScale;
		button.mPrevValue[keyMapping->mAxis] = oldValue * keyMapping->mScale;
	}
	else
	{
		button.mValue[keyMapping->mAxis] = newValue * keyMapping->mScale;
		button.mPrevValue[keyMapping->mAxis] = oldValue * keyMapping->mScale;
	}

	//if button is pressed because of this device then add it's type to the mask
	button.mActiveDevicesMask = (GainputDeviceType)(button.mActiveDevicesMask | keyMapping->mDeviceType);

	button.mDeltaValue[0] = button.mValue[0] - button.mPrevValue[0];
	button.mDeltaValue[1] = button.mValue[1] - button.mPrevValue[1];

	float prevFrameDelta = (abs(button.mPrevValue[0]) + abs(button.mPrevValue[1]));
	bool  isPressed = (abs(button.mValue[0]) + abs(button.mValue[1])) > 0.0f;

	//for Touch we use Touch0Down(1,2,3 based on touch index)
	//instead of input value for isPressed.
	if (button.mActiveDevicesMask & GAINPUT_TOUCH)
	{
		//Get a valid device button
		//if not from event try from mapping
		//if at this stage that means that a device button had a state change
		if (deviceButton < gainput::TouchCount_)
		{
			//4 fingers max, harccoded for now.
			//get touch index
			uint touchIndex = (deviceButton - gainput::TouchButton::Touch0Down) / 4;
			//get the correct TouchXDown enum based on touch index
			isPressed = device->GetBool(gainput::TouchButton::Touch0Down + touchIndex * 4);
			// Cache the touch index
			button.mTouchIndex = touchIndex;
		}
	}

	button.mIsPressed = isPressed;
	button.mIsReleased = prevFrameDelta > 0.f && (!button.mIsPressed ? true : false);
	button.mIsTriggered = prevFrameDelta == 0.0f && (button.mIsPressed ? true : false);

	if (keyMapping->mUserId == KEY_CHAR && mKeyboardDeviceID != gainput::InvalidDeviceId && !button.mIsReleased && button.mIsTriggered)
		button.mCharacter =
			(wchar_t)((gainput::InputDeviceKeyboard*)pInputManager->GetDevice(mKeyboardDeviceID))->GetNextCharacter(deviceButton);
}

bool DeviceInputEventListener::OnDeviceButtonBool(
	gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
{
	return GatherInputEventButton(deviceId, deviceButton, oldValue ? 1.0f : 0.0f, newValue ? 1.0f : 0.0f);
}

bool DeviceInputEventListener::OnDeviceButtonFloat(
	gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
{
	return GatherInputEventButton(deviceId, deviceButton, oldValue, newValue);
}

bool DeviceInputEventListener::OnDeviceButtonGesture(
	gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, const gainput::GestureChange& gesture)
{
#if defined(TARGET_IOS)
	ButtonData buttonData = {};
	buttonData.mActiveDevicesMask = GAINPUT_TOUCH;
	buttonData.mUserId = deviceButton;
	if (gesture.type == gainput::GesturePan)
	{
		buttonData.mValue[INPUT_X_AXIS] = gesture.translation[0];
		buttonData.mValue[INPUT_Y_AXIS] = gesture.translation[1];
	}
	else if (gesture.type == gainput::GesturePinch)
	{
		buttonData.mValue[INPUT_X_AXIS] = gesture.velocity;
		buttonData.mValue[INPUT_Y_AXIS] = gesture.scale;
		buttonData.mDeltaValue[INPUT_X_AXIS] = gesture.distance[0];
		buttonData.mDeltaValue[INPUT_Y_AXIS] = gesture.distance[1];
	}
	else if (gesture.type == gainput::GestureTap)
	{
		buttonData.mValue[INPUT_X_AXIS] = gesture.position[0];
		buttonData.mValue[INPUT_Y_AXIS] = gesture.position[1];
	}

	OnInputEvent(buttonData);
#endif
	return true;
}
