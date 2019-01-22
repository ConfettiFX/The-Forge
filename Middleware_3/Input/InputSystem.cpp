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
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"

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
#include "../../Common_3/ThirdParty/OpenSource/gainput/lib/source/gainput/touch/GainputInputDeviceTouchIos.h"
#else
#import <Cocoa/Cocoa.h>
#endif
#endif

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

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

//all the input devices we need
gainput::DeviceId   InputSystem::mMouseDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId   InputSystem::mRawMouseDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId   InputSystem::mKeyboardDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId   InputSystem::mGamepadDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId   InputSystem::mTouchDeviceID = gainput::InvalidDeviceId;
gainput::ListenerId InputSystem::mDeviceInputListnerID = -1;    //max uint instead of including headers for UINT_MAX on linux.

//system vars
bool InputSystem::mIsMouseCaptured = false;
bool InputSystem::mHideMouseCursorWhileCaptured = true;
bool InputSystem::mVirtualKeyboardActive = false;

// we should have more than one map
tinystl::vector<gainput::InputMap*>                                             InputSystem::pInputMap;
tinystl::unordered_map<uint32_t, tinystl::vector<KeyMappingDescription>>        InputSystem::mKeyMappings;
tinystl::unordered_map<uint32_t, tinystl::vector<GestureMappingDescription>>    InputSystem::mGestureMappings;
tinystl::unordered_map<uint32_t, tinystl::vector<InputSystem::UserToDeviceMap>> InputSystem::mDeviceToUserMappings;

tinystl::vector<InputSystem::InputEventHandler> InputSystem::mInputCallbacks;
tinystl::vector<uint32_t>                       InputSystem::mInputPriorities;
uint32_t                                        InputSystem::mActiveInputMap = 0;

// gainput systems
gainput::InputManager* InputSystem::pInputManager = NULL;

// Needed to send events
gainput::DeviceButtonSpec InputSystem::mButtonsDown[32];

InputSystem::DeviceInputEventListener InputSystem::mDeviceInputListener(0);

#ifdef METAL
void* InputSystem::pGainputView = NULL;
#endif

void InputSystem::Shutdown()
{
	if (mDeviceInputListnerID != -1)
		pInputManager->RemoveListener(mDeviceInputListnerID);

	for (uint32_t i = 0; i < pInputMap.size(); i++)
	{
		if (pInputMap[i])
		{
			//call destructor explicitly
			//Since we use placement new with calloc
			pInputMap[i]->~InputMap();
			conf_free(pInputMap[i]);
			pInputMap[i] = NULL;
		}
	}

#ifdef METAL
	//automatic reference counting
	//it will get deallocated.
	if (pGainputView)
		pGainputView = NULL;
#endif

	if (pInputManager)
	{
		//call destructor explicitly
		//Since we use placement new with calloc
		pInputManager->~InputManager();
		conf_free(pInputManager);
		pInputManager = NULL;
	}
	pInputMap.clear();
	mKeyMappings.clear();
	mInputCallbacks.clear();
}

void InputSystem::Init(const uint32_t& width, const uint32_t& height)
{
	//default input map
	mActiveInputMap = 0;
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

	//set defaults
	SetActiveInputMap(0);

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

void InputSystem::ClearInputStates(GainputDeviceType deviceType)
{
	gainput::DeviceId deviceId = GetDeviceID(deviceType);
	if (pInputManager)
		pInputManager->ClearAllStates(deviceId);
}

void InputSystem::Update(float dt)
{
	// update gainput manager
	if (pInputManager)
		pInputManager->Update();
}

void InputSystem::RegisterInputEvent(InputEventHandler callback, uint32_t priority)
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

void InputSystem::UnregisterInputEvent(InputEventHandler callback)
{
	InputEventHandler* it = mInputCallbacks.find(callback);
	if (it != mInputCallbacks.end())
	{
		uint64_t index = it - mInputCallbacks.begin();
		mInputCallbacks.erase(it);
		mInputPriorities.erase(mInputPriorities.begin() + index);
	}
}

void InputSystem::OnInputEvent(ButtonData& buttonData)
{
	for (InputEventHandler& callback : mInputCallbacks)
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

#ifdef METAL
void InputSystem::ShutdownSubView(void* view)
{
	if (!view)
		return;
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

void InputSystem::InitSubView(void* view)
{
	if (!view)
		return;
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

#ifdef METAL
	// On Metal (macOS/iOS) we defer SetDefaultKeyMapping until we initialize the gainput view
	// For gesture support as they need to be registered in the view itself
	SetDefaultKeyMapping();
#endif
}
#endif

void InputSystem::MapKey(const uint32_t& sourceKey, const uint32_t& userKey, GainputDeviceType inputDevice)
{
	gainput::DeviceId           currDeviceId = GetDeviceID(inputDevice);
	const gainput::InputDevice* device = pInputManager->GetDevice(currDeviceId);

	if (device == NULL)
	{
		LOGERROR("Unsupported Input Device");
		return;
	}

	// get button type
	gainput::ButtonType type = device->GetButtonType(sourceKey);
	if (type == gainput::ButtonType::BT_BOOL)
	{
		pInputMap[mActiveInputMap]->MapBool(userKey, currDeviceId, sourceKey);
	}
	else
	{
		pInputMap[mActiveInputMap]->MapFloat(userKey, currDeviceId, sourceKey);
	}

	//maps to lookup device ID and Input types
	UserToDeviceMap toAdd;
	toAdd.deviceId = currDeviceId;
	toAdd.userMapping = userKey;
	mDeviceToUserMappings[sourceKey].push_back(toAdd);
}

bool InputSystem::IsButtonMapped(const uint32_t& userKey)
{
	if (userKey > UserInputKeys::KEY_COUNT)
		return false;

	if (mKeyMappings.find(userKey) == mKeyMappings.end())
		return false;

	return false;
}

bool InputSystem::IsButtonPressed(const uint32_t& buttonId)
{
	// check if button is mapped
	if (!pInputMap[mActiveInputMap]->IsMapped(buttonId))
	{
		LOGWARNING("Button is not mapped");
		return false;
	}

	ButtonData button = GetButtonData(buttonId);
	return button.mIsPressed;
}

bool InputSystem::IsButtonTriggered(const uint32_t& buttonId)
{
	// check if button is mapped
	if (!pInputMap[mActiveInputMap]->IsMapped(buttonId))
	{
		LOGWARNING("Button is not mapped");
		return false;
	}

	ButtonData button = GetButtonData(buttonId);
	return button.mIsTriggered;
}

bool InputSystem::IsButtonReleased(const uint32_t& buttonId)
{
	// check if button is mapped
	if (!pInputMap[mActiveInputMap]->IsMapped(buttonId))
	{
		LOGWARNING("Button is not mapped");
		return false;
	}

	ButtonData button = GetButtonData(buttonId);
	return button.mIsReleased;
}
/*
 This can handle any kind of button(mouse Button, keyboard, gamepad)
 It also handles same user key mapped to multiple buttons
 */
ButtonData InputSystem::GetButtonData(const uint32_t& buttonId, const GainputDeviceType& deviceType)
{
	ButtonData button;
	button.mUserId = buttonId;

	// check if button is mapped
	if (!pInputMap[mActiveInputMap]->IsMapped(buttonId))
	{
		LOGWARNING("Button is not mapped");
		return button;
	}

	//here it means one user key maps to multiple device button
	//such as left stick with w-a-s-d
	tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[buttonId];

	if (keyMappings.size() == 0)
	{
		LOGWARNING("Couldn't find map from user to Description ");
		return button;
	}

	tinystl::vector<KeyMappingDescription*> descs;

	//Gather all combinations of device buttons that could affect
	//this user key
	for (uint32_t i = 0; i < keyMappings.size(); i++)
	{
		//TODO: Fix bug where
		//2 fingers are moving and we accumulate both
		//into same joystick.
		if (deviceType != GainputDeviceType::GAINPUT_DEFAULT)
		{
			if (keyMappings[i].mDeviceType == deviceType)
			{
				descs.push_back(&keyMappings[i]);
			}
		}
		else
		{
			descs.push_back(&keyMappings[i]);
		}
	}

	if (!descs.size())
		return button;

	//reset values before accumulating
	button.mActiveDevicesMask = GAINPUT_DEFAULT;
	button.mValue[0] = button.mValue[1] = 0;
	button.mPrevValue[0] = button.mPrevValue[1] = 0;
	button.mDeltaValue[0] = button.mDeltaValue[1] = 0;

	//for every combination of device button for given
	//user key
	for (uint32_t map = 0; map < descs.size(); map++)
	{
		//get current descriptor
		//defines which buttons from which device
		//defines how buttons affect an axis for joysticks and in which direction
		KeyMappingDescription* desc = descs[map];

		//This is the core function that gathers the actual input data for that specified user key
		FillButtonDataFromDesc(desc, button);
	}

	return button;
}

uint32_t InputSystem::GetDeviceID(GainputDeviceType deviceType)
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

/*
	Will be used to manage multiple input maps
	One user defined button can be mapped to different buttons
	based on situation (Main Menu, Scene, pause menu)

	param: index UID pointing to a user created mapping

	//TODO: Add correct logic to switch different input maps
*/
void InputSystem::SetActiveInputMap(const uint32_t& index)
{
	while (index >= pInputMap.size())
	{
		gainput::InputMap* toAdd = conf_placement_new<gainput::InputMap>(conf_calloc(1, sizeof(gainput::InputMap)), (pInputManager));
		pInputMap.push_back(toAdd);
	}

	mActiveInputMap = index;
}

void InputSystem::AddMappings(KeyMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings)
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
		if (pInputMap.size() > mActiveInputMap)
		{
			pInputMap[mActiveInputMap]->Clear();
		}
		mDeviceToUserMappings.clear();
	}

	for (uint32_t i = 0; i < mappingCount; i++)
	{
		KeyMappingDescription* mapping = &mappings[i];

		//skip uninitialized devices.
		if (GetDeviceID(mapping->mDeviceType) == gainput::InvalidDeviceId)
			continue;

		//create empty key mapping object if hasn't been found
		if (mKeyMappings.find(mapping->mUserId) == mKeyMappings.end())
		{
			mKeyMappings[mapping->mUserId] = {};
		}

		//add key mapping to user mapping
		mKeyMappings[mapping->mUserId].push_back(*mapping);

		//map every key + axis
		for (uint32_t map = 0; map < mapping->mAxisCount; map++)
			MapKey(mapping->mMappings[map].mDeviceButtonId, mapping->mUserId, mapping->mDeviceType);
	}
}

void InputSystem::AddGestureMappings(GestureMappingDescription* mappings, uint32_t mappingCount, bool overrideMappings)
{
	if (!mappings || mappingCount <= 0)
	{
		LOGWARNING("No mappings were added. Provided Mapping was empty or null.");
		return;
	}

	//clear mappings data structures if required
	if (overrideMappings)
	{
		mGestureMappings.clear();
	}

	for (uint32_t i = 0; i < mappingCount; i++)
	{
		GestureMappingDescription* mapping = &mappings[i];

		//create empty key mapping object if hasn't been found
		if (mGestureMappings.find(mapping->mUserId) == mGestureMappings.end())
		{
			mGestureMappings[mapping->mUserId] = {};
		}

		//add key mapping to user mapping
		mGestureMappings[mapping->mUserId].push_back(*mapping);

#if defined(TARGET_IOS)
		GainputView* view = (__bridge GainputView*)pGainputView;
		[view addGestureMapping:mapping->mType forId:mapping->mUserId withConfig:mapping->mConfig];
#endif
	}
}

void InputSystem::SetDefaultKeyMapping()
{
	mKeyMappings.clear();

#ifdef _DURANGO
	uint32_t entryCount = sizeof(gXboxMappings) / sizeof(KeyMappingDescription);
	AddMappings(gXboxMappings, entryCount);
#else
	uint32_t entryCount = sizeof(gUserKeys) / sizeof(KeyMappingDescription);
	uint32_t controllerEntryCount = sizeof(gXboxMappings) / sizeof(KeyMappingDescription);
	uint32_t gesturesEntryCount = sizeof(gGestureMappings) / sizeof(GestureMappingDescription);
	AddMappings(gUserKeys, entryCount);
	AddMappings(gXboxMappings, controllerEntryCount);
	AddGestureMappings(gGestureMappings, gesturesEntryCount);
#endif
}

void InputSystem::SetMouseCapture(bool mouseCapture)
{
	mIsMouseCaptured = mouseCapture;
#if defined(METAL) && !defined(TARGET_IOS)
	GainputMacInputView* view = (__bridge GainputMacInputView*)(pGainputView);
	[view SetMouseCapture:(mouseCapture && mHideMouseCursorWhileCaptured)];
	view = NULL;
#endif
}

void InputSystem::UpdateSize(const uint32_t& width, const uint32_t& height) { pInputManager->SetDisplaySize(width, height); }

uint32_t InputSystem::GetDisplayWidth() { return (uint32_t)pInputManager->GetDisplayWidth(); }

uint32_t InputSystem::GetDisplayHeight() { return (uint32_t)pInputManager->GetDisplayHeight(); }

GainputDeviceType InputSystem::GetDeviceType(uint32_t deviceId)
{
	if (deviceId == mRawMouseDeviceID)
	{
		return GainputDeviceType::GAINPUT_RAW_MOUSE;
	}
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

bool InputSystem::DeviceInputEventListener::OnDeviceButtonBool(
	gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
{
	return GatherInputEventButton(deviceId, deviceButton, oldValue ? 1.0f : 0.0f, newValue ? 1.0f : 0.0f);
}

bool InputSystem::DeviceInputEventListener::OnDeviceButtonFloat(
	gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
{
	return GatherInputEventButton(deviceId, deviceButton, oldValue, newValue);
}

bool InputSystem::DeviceInputEventListener::OnDeviceButtonGesture(
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

void InputSystem::WarpMouse(const float& x, const float& y)
{
	gainput::InputDevice* device = pInputManager->GetDevice(mRawMouseDeviceID);
	device->WarpMouse(x, y);
}

bool InputSystem::GatherInputEventButton(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
{
	tinystl::vector<UserToDeviceMap> userButtons = mDeviceToUserMappings[deviceButton];

	GainputDeviceType deviceType = GetDeviceType(deviceId);
	bool              isMapped = false;

	for (uint32_t i = 0; i < userButtons.size(); i++)
	{
		if (userButtons[i].deviceId != deviceId)
			continue;

		ButtonData button = {};
		button.mUserId = userButtons[i].userMapping;

		// check if button is mapped
		if (!pInputMap[mActiveInputMap]->IsMapped(userButtons[i].userMapping))
		{
			LOGWARNING("Button is not mapped");
			continue;
		}

		//here it means one user key maps to multiple device button
		//such as left stick with w-a-s-d or left stick with touchx, touchy
		tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[userButtons[i].userMapping];

		if (keyMappings.size() == 0)
		{
			LOGWARNING("Couldn't find map from user to Description ");
			continue;
		}

		KeyMappingDescription* desc = NULL;

		//for every user key mapping
		for (uint32_t i = 0; i < keyMappings.size(); i++)
		{
			//look for given device type
			if (deviceType != GainputDeviceType::GAINPUT_DEFAULT)
			{
				if (keyMappings[i].mDeviceType == deviceType)
				{
					//Find which key mapping corresponds to device button that got updated
					//we dont need to aggregate all key mappings here.
					for (uint32_t j = 0; j < keyMappings[i].mAxisCount; j++)
					{
						if (keyMappings[i].mMappings[j].mDeviceButtonId == deviceButton)
						{
							desc = &keyMappings[i];
							break;
						}
					}
				}
			}
		}

		if (!desc)
			continue;

		isMapped = true;

		//reset values
		button.mActiveDevicesMask = deviceType;
		button.mValue[0] = button.mValue[1] = 0;
		button.mPrevValue[0] = button.mPrevValue[1] = 0;
		button.mDeltaValue[0] = button.mDeltaValue[1] = 0;
		button.mCharacter = L'\0';
		button.mTouchIndex = -1;
		//This is the core function that gathers the actual input data for that specified user key
		FillButtonDataFromDesc(desc, button, oldValue, newValue, deviceId, deviceButton);

		//broadcast event
		OnInputEvent(button);
	}

	return isMapped;
}

/**
 * Given a specific Key mapping descriptor this function will fill the button data with correct input values.
 * The function can be used to fill the data from a gainput event and whenever GetButtonData is called.
 * When an event uses this function the last 4 parameters are used other default values are used.
 */
void InputSystem::FillButtonDataFromDesc(
	const KeyMappingDescription* keyMapping, ButtonData& button, float oldValue, float newValue, gainput::DeviceId eventDeviceId,
	gainput::DeviceButtonId eventDeviceButton)
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
	uint32_t deviceMappingcount = keyMapping->mAxisCount;
	bool     currentDeviceActive = false;
	//For every device button that is used for this mapping descriptor
	for (uint32_t i = 0; i < deviceMappingcount; i++)
	{
		//get mappings which holds:
		//device button to gather input from,
		//Axis (X or Y)
		//Direction (Positive or negative)
		//TODO : Add descriptor for input range and type of input (Normalized, Absolute, Relative)
		TargetMapping mapping = keyMapping->mMappings[i];

		//check that button is valid, Maybe input device has a missing button (Different gamepads)
		if (!device->IsValidButtonId(mapping.mDeviceButtonId))
			continue;

		gainput::ButtonType type = device->GetButtonType(mapping.mDeviceButtonId);
		if (type == gainput::ButtonType::BT_FLOAT)
		{
			//if we are calling this function from an gainput event listener we only want to use the values updated from the event.
			//This enables us to receive every input event received in a single frame (Think multiple mouse events in same frame)
			if (eventDeviceButton != gainput::InvalidDeviceId && mapping.mDeviceButtonId == eventDeviceButton)
			{
				//if current button has any effect on the current user button
				//then add device as active to active devices mask
				if (newValue != 0.0f)
					currentDeviceActive = true;

				//for raw mouse we need to check delta to update button data
				//otherwise we get accumulated movement values.
				if (keyMapping->mDeviceType == GAINPUT_RAW_MOUSE && newValue == oldValue)
					continue;

				button.mValue[mapping.mAxis] = newValue * (float)mapping.mDirection;
				button.mPrevValue[mapping.mAxis] = oldValue * (float)mapping.mDirection;
			}
			else
			{
				float currValue = device->GetFloat(mapping.mDeviceButtonId);
				float prevValue = device->GetFloatPrevious(mapping.mDeviceButtonId);

				//for raw mouse we need to check delta to update button data
				//otherwise we get accumulated movement values.
				if (keyMapping->mDeviceType == GAINPUT_RAW_MOUSE && currValue == prevValue)
					continue;

				if (currValue != 0.0f)
					currentDeviceActive = true;

				button.mValue[mapping.mAxis] += currValue * (float)mapping.mDirection;
				button.mPrevValue[mapping.mAxis] += prevValue * (float)mapping.mDirection;
			}
		}
		else
		{
			if (eventDeviceButton != gainput::InvalidDeviceId && mapping.mDeviceButtonId == eventDeviceButton)
			{
				if (newValue != 0.0f)
					currentDeviceActive = true;

				button.mValue[mapping.mAxis] = newValue * (float)mapping.mDirection;
				button.mPrevValue[mapping.mAxis] = oldValue * (float)mapping.mDirection;
			}
			else
			{
				float currValue = device->GetBool(mapping.mDeviceButtonId) ? 1.0f : 0.0f;
				float prevValue = device->GetBoolPrevious(mapping.mDeviceButtonId) ? 1.0f : 0.0f;

				if (currValue != 0.0f)
					currentDeviceActive = true;

				button.mValue[mapping.mAxis] += currValue * (float)mapping.mDirection;
				button.mPrevValue[mapping.mAxis] += prevValue * (float)mapping.mDirection;
			}
		}
	}

	//if button is pressed because of this device then add it's type to the mask
	if (currentDeviceActive)
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
		gainput::DeviceButtonId devButton = eventDeviceButton;
		if (devButton != gainput::InvalidDeviceButtonId)
			devButton = keyMapping->mMappings[0].mDeviceButtonId;
		if (devButton != gainput::InvalidDeviceButtonId && devButton < gainput::TouchCount_)
		{
			//4 fingers max, harccoded for now.
			//get touch index
			uint touchIndex = (eventDeviceButton - gainput::TouchButton::Touch0Down) / 4;
			//get the correct TouchXDown enum based on touch index
			isPressed = device->GetBool(gainput::TouchButton::Touch0Down + touchIndex * 4);
			// Cache the touch index
			button.mTouchIndex = touchIndex;
		}
	}

	button.mIsPressed = isPressed;
	button.mIsReleased = prevFrameDelta > 0.f && !button.mIsPressed ? true : false;
	button.mIsTriggered = prevFrameDelta == 0.0f && button.mIsPressed ? true : false;

	if (keyMapping->mUserId == KEY_CHAR && mKeyboardDeviceID != gainput::InvalidDeviceId && !button.mIsReleased && button.mIsTriggered)
		button.mCharacter =
			(wchar_t)((gainput::InputDeviceKeyboard*)pInputManager->GetDevice(mKeyboardDeviceID))->GetNextCharacter(eventDeviceButton);
}

void InputSystem::ToggleVirtualTouchKeyboard(int keyboardType)
{
#ifdef TARGET_IOS
	if (!pGainputView)
		return;

	if ((keyboardType > 0) != mVirtualKeyboardActive)
		mVirtualKeyboardActive = (keyboardType > 0);
	else
		return;

	GainputView* view = (__bridge GainputView*)(pGainputView);
	[view setVirtualKeyboard:keyboardType];
#endif
}

void InputSystem::GetVirtualKeyboardTextInput(char* inputBuffer, uint32_t inputBufferSize)
{
	if (!inputBuffer)
		return;
#ifdef TARGET_IOS
	gainput::InputDeviceTouch* touchDevice = ((gainput::InputDeviceTouch*)pInputManager->GetDevice(mTouchDeviceID));
	if (touchDevice)
		touchDevice->GetVirtualKeyboardInput(&inputBuffer[0], inputBufferSize);
#endif
}
