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
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"

#include "InputSystem.h"
#include "InputMappings.h"

#ifdef LINUX
#include <climits>
#endif

#ifdef METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#ifdef TARGET_IOS
#include <UIKit/UIView.h>
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
//Add Mouse wheel
**/

//all the input devices we need
gainput::DeviceId InputSystem::mMouseDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId InputSystem::mRawMouseDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId InputSystem::mKeyboardDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId InputSystem::mGamepadDeviceID = gainput::InvalidDeviceId;
gainput::DeviceId InputSystem::mTouchDeviceID = gainput::InvalidDeviceId;
gainput::ListenerId InputSystem::mDeviceInputListnerID =-1; //max uint instead of including headers for UINT_MAX on linux.

//system vars
bool InputSystem::mIsMouseCaptured = false;

// we should have more than one map
tinystl::vector<gainput::InputMap*> InputSystem::pInputMap;
tinystl::unordered_map<uint32_t, tinystl::vector<KeyMappingDescription>> InputSystem::mKeyMappings;
tinystl::unordered_map<uint32_t, tinystl::vector<InputSystem::UserToDeviceMap>> InputSystem::mDeviceToUserMappings;

tinystl::vector<InputSystem::InputEventHandler> InputSystem::mInputCallbacks;
uint32_t InputSystem::mActiveInputMap = 0;

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
	if(pGainputView)
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
#ifdef METAL
	if(pGainputView)
		pInputManager = conf_placement_new<gainput::InputManager>(conf_calloc(1, sizeof(gainput::InputManager)), pGainputView);
	else
	  pInputManager = conf_placement_new<gainput::InputManager>(conf_calloc(1, sizeof(gainput::InputManager)));
#else 
	pInputManager = conf_placement_new<gainput::InputManager>(conf_calloc(1, sizeof(gainput::InputManager)));
#endif
	ASSERT(pInputManager);

	//Set display size
	pInputManager->SetDisplaySize(width, height);
	mIsMouseCaptured = false;

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

	SetDefaultKeyMapping();

}

void InputSystem::Update(float dt)
{
	// update gainput manager
	if(pInputManager)
		pInputManager->Update();
}

void InputSystem::RegisterInputEvent(InputEventHandler callback)
{
	mInputCallbacks.push_back(callback);
}

void InputSystem::UnregisterInputEvent(InputEventHandler callback)
{
	mInputCallbacks.erase(mInputCallbacks.find(callback));
}

void InputSystem::OnInputEvent(const ButtonData& buttonData)
{
	for (InputEventHandler& callback : mInputCallbacks)
		callback(&buttonData);
}

#ifdef METAL
void InputSystem::InitSubView(void* view)
{
	if(!view)
		return;
#ifdef TARGET_IOS	
	//automatic reference counting
	//it will get deallocated.
	if(pGainputView)
		pGainputView = NULL;
	
	UIView * uiView = (__bridge UIView*)view;
	GainputView * newView = [[GainputView alloc] initWithFrame:uiView.bounds inputManager:*pInputManager];
	[uiView addSubview:newView];
	pGainputView = (__bridge void*)newView;
#else
    pGainputView = view;
#endif
}
#endif

void InputSystem::MapKey(const uint32_t& sourceKey, const uint32_t& userKey, GainputDeviceType inputDevice)
{
	gainput::DeviceId currDeviceId = GetDeviceID(inputDevice);
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

bool InputSystem::IsButtonPressed(const uint32_t& buttonId)
{
	// check if button is mapped
	if (!pInputMap[mActiveInputMap]->IsMapped(buttonId))
	{
		LOGERRORF("Button is not mapped");
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
		LOGERRORF("Button is not mapped");
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
		LOGERRORF("Button is not mapped");
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
		LOGERRORF("Button is not mapped");
		return button;
	}


	//here it means one user key maps to multiple device button
	//such as left stick with w-a-s-d
    tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[buttonId];

    if (keyMappings.size() == 0)
    {
        LOGERRORF("Couldn't find map from user to Description ");
        return button;
    }

    gainput::InputDevice * device = NULL;
    tinystl::vector<KeyMappingDescription *> descs;

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
	button.mDeviceButtonId = 0;
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
		KeyMappingDescription * desc = descs[map];
        //get device id. for now assuming one device only per type.
		uint32_t deviceId = GetDeviceID(desc->mDeviceType);
		device = pInputManager->GetDevice(deviceId);
        //how many device buttons are needed for current user key
		uint32_t deviceMappingcount = desc->mAxisCount;

		for (uint32_t i = 0; i < deviceMappingcount; i++)
		{
            //get current device button + axis + direction
			TargetMapping mapping = desc->mMappings[i];
			ASSERT(device->IsValidButtonId(mapping.mDeviceButtonId));
            //aggregate all device button ids.
			button.mDeviceButtonId |= mapping.mDeviceButtonId;
        
			gainput::ButtonType type = device->GetButtonType(mapping.mDeviceButtonId);

            //for given axis accumulate current and previous values
            //accumulate for the multiple device buttons
            //example W affecting X axis positively on left stick
            // S affecting Y axis negatively on left stick
			if (type == gainput::ButtonType::BT_FLOAT)
			{
				button.mValue[mapping.mAxis] += device->GetFloat(mapping.mDeviceButtonId) * (float)mapping.mDirection;
				button.mPrevValue[mapping.mAxis] += device->GetFloatPrevious(mapping.mDeviceButtonId) * (float)mapping.mDirection;
			}
			else
			{
				float currValue = device->GetBool(mapping.mDeviceButtonId) ? 1.0f : 0.0f;
				float prevValue = device->GetBoolPrevious(mapping.mDeviceButtonId) ? 1.0f : 0.0f;

				button.mValue[mapping.mAxis] += currValue * (float)mapping.mDirection;
				button.mPrevValue[mapping.mAxis] += prevValue;
			}
		}

		//call custom callback function
		//if (desc->pInputCallbackFn)
		//{
		//	desc->pInputCallbackFn(button);
		//}
	}

	button.mDeltaValue[0] = button.mValue[0] - button.mPrevValue[0];
	button.mDeltaValue[1] = button.mValue[1] - button.mPrevValue[1];

    //if current value is != 0 then its pressed
    //if previous value is != 0 and current value = 0 then its released
    //same with triggered but otherway around.
    float prevFrameDelta = (abs(button.mPrevValue[0]) + abs(button.mPrevValue[1]));
    bool isPressed = (abs(button.mValue[0]) + abs(button.mValue[1])) > 0.0f;

    button.mIsPressed = isPressed;
    button.mIsReleased = prevFrameDelta  > 0.f && !button.mIsPressed ? true : false;
    button.mIsTriggered = prevFrameDelta == 0.0f && button.mIsPressed ? true : false;
	
	return button;
}

uint32_t InputSystem::GetDeviceID(GainputDeviceType deviceType)
{
	switch (deviceType)
	{
	case GAINPUT_MOUSE:
		return mMouseDeviceID;
		break;
	case GAINPUT_RAW_MOUSE:
		return mRawMouseDeviceID;
		break;
	case GAINPUT_KEYBOARD:
		return mKeyboardDeviceID;
		break;
	case GAINPUT_GAMEPAD:
		return mGamepadDeviceID;
		break;
	case GAINPUT_TOUCH:
		return mTouchDeviceID;
		break;
	case DEVICE_COUNT:
		return UINT_MAX;
		break;
	default:
		break;
	}
	return UINT_MAX;
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
		gainput::InputMap * toAdd = conf_placement_new<gainput::InputMap>(conf_calloc(1, sizeof(gainput::InputMap)), (pInputManager));
		pInputMap.push_back(toAdd);
	}

	mActiveInputMap = index;
}

void InputSystem::AddMappings(KeyMappingDescription* mappings, uint32_t mappingCount)
{
	for (uint32_t i = 0; i < mappingCount; i++)
	{
		KeyMappingDescription * mapping = &mappings[i];

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

void InputSystem::SetDefaultKeyMapping()
{
	mKeyMappings.clear();
	
#ifdef _DURANGO
	uint32_t entryCount = sizeof(gXboxMappings) / sizeof(KeyMappingDescription);
	AddMappings(gXboxMappings, entryCount);
#else
	uint32_t entryCount = sizeof(gUserKeys) / sizeof(KeyMappingDescription);
	AddMappings(gUserKeys, entryCount);
#endif


}

void InputSystem::SetMouseCapture(bool mouseCapture)
{
	mIsMouseCaptured = mouseCapture;
}

void InputSystem::UpdateSize(const uint32_t& width, const uint32_t& height)
{
	pInputManager->SetDisplaySize(width, height);
}


GainputDeviceType InputSystem::GetDeviceType(uint32_t deviceId)
{
    if (deviceId == mRawMouseDeviceID) {
        return GainputDeviceType::GAINPUT_RAW_MOUSE;
    }
    else if (deviceId == mMouseDeviceID) {
        return GainputDeviceType::GAINPUT_MOUSE;
    }
    else if (deviceId == mKeyboardDeviceID) {
        return GainputDeviceType::GAINPUT_KEYBOARD;
    }
    else if (deviceId == mTouchDeviceID) {
        return GainputDeviceType::GAINPUT_TOUCH;
    }
    else if (deviceId == mGamepadDeviceID) {
        return GainputDeviceType::GAINPUT_GAMEPAD;
    }
    
    return GainputDeviceType::GAINPUT_DEFAULT;
}


bool InputSystem::DeviceInputEventListener::OnDeviceButtonBool(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, bool oldValue, bool newValue)
{
	tinystl::vector<UserToDeviceMap> userButtons = mDeviceToUserMappings[deviceButton];
	GainputDeviceType deviceType = GetDeviceType(deviceId);

	for (uint32_t i = 0; i < userButtons.size(); i++)
	{
		if (userButtons[i].deviceId != deviceId)
			continue;

		ButtonData buttonData = GetButtonData(userButtons[i].userMapping, deviceType);
		OnInputEvent(buttonData);
	}


	return true;
}



bool InputSystem::DeviceInputEventListener::OnDeviceButtonFloat(gainput::DeviceId deviceId, gainput::DeviceButtonId deviceButton, float oldValue, float newValue)
{
	tinystl::vector<UserToDeviceMap> userButtons = mDeviceToUserMappings[deviceButton];

	GainputDeviceType deviceType = GetDeviceType(deviceId);
    gainput::InputDevice * device = NULL;

	for (uint32_t i = 0; i < userButtons.size(); i++)
	{
		if (userButtons[i].deviceId != deviceId)
			continue;

        ButtonData button = {};
        button.mUserId = userButtons[i].userMapping;
        
        // check if button is mapped
        if (!pInputMap[mActiveInputMap]->IsMapped(userButtons[i].userMapping))
        {
            LOGERRORF("Button is not mapped");
            continue;
        }
        
        //here it means one user key maps to multiple device button
        //such as left stick with w-a-s-d or left stick with touchx, touchy
        tinystl::vector<KeyMappingDescription> keyMappings = mKeyMappings[userButtons[i].userMapping];
        
        if (keyMappings.size() == 0)
        {
            LOGERRORF("Couldn't find map from user to Description ");
            continue;
        }
        
        KeyMappingDescription * desc = NULL;
        
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
                    for(uint32_t j = 0 ; j < keyMappings[i].mAxisCount ;j++)
                    {
                        if(keyMappings[i].mMappings[j].mDeviceButtonId == deviceButton)
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
        
		//reset values
        button.mDeviceButtonId = 0;
        button.mValue[0] = button.mValue[1] = 0;
        button.mPrevValue[0] = button.mPrevValue[1] = 0;
        button.mDeltaValue[0] = button.mDeltaValue[1] = 0;
        
        uint32_t deviceId = GetDeviceID(desc->mDeviceType);
        device = pInputManager->GetDevice(deviceId);
        uint32_t deviceMappingcount = desc->mAxisCount;
        
        for (uint32_t i = 0; i < deviceMappingcount; i++)
        {
            TargetMapping mapping = desc->mMappings[i];
            ASSERT(device->IsValidButtonId(mapping.mDeviceButtonId));
            
            button.mDeviceButtonId |= mapping.mDeviceButtonId;
            
            gainput::ButtonType type = device->GetButtonType(mapping.mDeviceButtonId);
            
            if (type == gainput::ButtonType::BT_FLOAT)
            {
                if(mapping.mDeviceButtonId == deviceButton)
                {
                    button.mValue[mapping.mAxis] = newValue * (float)mapping.mDirection;
                    button.mPrevValue[mapping.mAxis] = oldValue * (float)mapping.mDirection;
                }
                else
                {
                    button.mValue[mapping.mAxis] += device->GetFloat(mapping.mDeviceButtonId) * (float)mapping.mDirection;
                    button.mPrevValue[mapping.mAxis] += device->GetFloatPrevious(mapping.mDeviceButtonId) * (float)mapping.mDirection;
                }
            }
            else
            {
                float currValue = device->GetBool(mapping.mDeviceButtonId) ? 1.0f : 0.0f;
                float prevValue = device->GetBoolPrevious(mapping.mDeviceButtonId) ? 1.0f : 0.0f;
                
                button.mValue[mapping.mAxis] += currValue * (float)mapping.mDirection;
                button.mPrevValue[mapping.mAxis] += prevValue* (float)mapping.mDirection;
            }
        }
        
        button.mDeltaValue[0] = button.mValue[0] - button.mPrevValue[0];
        button.mDeltaValue[1] = button.mValue[1] - button.mPrevValue[1];
        
        float prevFrameDelta = (abs(button.mPrevValue[0]) + abs(button.mPrevValue[1]));
        bool isPressed = (abs(button.mValue[0]) + abs(button.mValue[1])) > 0.0f;
        
        button.mIsPressed = isPressed;
        button.mIsReleased = prevFrameDelta  > 0.f && !button.mIsPressed ? true : false;
        button.mIsTriggered = prevFrameDelta == 0.0f && button.mIsPressed ? true : false;
        
        //broadcast event
        OnInputEvent(button);
    }
        return true;
}


void InputSystem::WarpMouse(const float& x, const float& y)
{
	gainput::InputDevice * device = pInputManager->GetDevice(mRawMouseDeviceID);
	device->WarpMouse(x,y);
}
