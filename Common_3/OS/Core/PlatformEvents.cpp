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

#include "../Interfaces/IOperatingSystem.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"

static tinystl::vector<WindowResizeEventHandler> gWindowResizeCallbacks;
static tinystl::vector<KeyboardCharEventHandler> gKeyboardCharCallbacks;
static tinystl::vector<KeyboardButtonEventHandler> gKeyboardButtonCallbacks;
static tinystl::vector<MouseMoveEventHandler> gMouseMoveCallbacks;
static tinystl::vector<RawMouseMoveEventHandler> gRawMouseMoveCallbacks;
static tinystl::vector<MouseButtonEventHandler> gMouseButtonCallbacks;
static tinystl::vector<MouseWheelEventHandler> gMouseWheelCallbacks;
static tinystl::vector<JoystickButtonEventHandler> gJoystickButtonCallbacks;
static tinystl::vector<TouchEventHandler> gTouchCallbacks;
static tinystl::vector<TouchMoveEventHandler> gTouchMoveCallbacks;

void registerWindowResizeEvent(WindowResizeEventHandler callback)
{
	gWindowResizeCallbacks.push_back(callback);
}

void unregisterWindowResizeEvent(WindowResizeEventHandler callback)
{
	gWindowResizeCallbacks.erase(gWindowResizeCallbacks.find(callback));
}

void registerKeyboardCharEvent(KeyboardCharEventHandler callback)
{
	gKeyboardCharCallbacks.push_back(callback);
}

void unregisterKeyboardCharEvent(KeyboardCharEventHandler callback)
{
	gKeyboardCharCallbacks.erase(gKeyboardCharCallbacks.find(callback));
}

void registerKeyboardButtonEvent(KeyboardButtonEventHandler callback)
{
	gKeyboardButtonCallbacks.push_back(callback);
}

void unregisterKeyboardButtonEvent(KeyboardButtonEventHandler callback)
{
	gKeyboardButtonCallbacks.erase(gKeyboardButtonCallbacks.find(callback));
}

void registerMouseMoveEvent(MouseMoveEventHandler callback)
{
	gMouseMoveCallbacks.push_back(callback);
}

void unregisterMouseMoveEvent(MouseMoveEventHandler callback)
{
	gMouseMoveCallbacks.erase(gMouseMoveCallbacks.find(callback));
}

void registerRawMouseMoveEvent(RawMouseMoveEventHandler callback)
{
	gRawMouseMoveCallbacks.push_back(callback);
}

void unregisterRawMouseMoveEvent(RawMouseMoveEventHandler callback)
{
	gRawMouseMoveCallbacks.erase(gRawMouseMoveCallbacks.find(callback));
}

void registerMouseButtonEvent(MouseButtonEventHandler callback)
{
	gMouseButtonCallbacks.push_back(callback);
}

void unregisterMouseButtonEvent(MouseButtonEventHandler callback)
{
	gMouseButtonCallbacks.erase(gMouseButtonCallbacks.find(callback));
}

void registerMouseWheelEvent(MouseWheelEventHandler callback)
{
	gMouseWheelCallbacks.push_back(callback);
}

void unregisterMouseWheelEvent(MouseWheelEventHandler callback)
{
	gMouseWheelCallbacks.erase(gMouseWheelCallbacks.find(callback));
}

void registerJoystickButtonEvent(JoystickButtonEventHandler callback)
{
	gJoystickButtonCallbacks.push_back(callback);
}

void unregisterJoystickButtonEvent(JoystickButtonEventHandler callback)
{
	gJoystickButtonCallbacks.erase(gJoystickButtonCallbacks.find(callback));
}

void registerTouchEvent(TouchEventHandler callback)
{
    gTouchCallbacks.push_back(callback);
}

void unregisterTouchEvent(TouchEventHandler callback)
{
    gTouchCallbacks.erase(gTouchCallbacks.find(callback));
}

void registerTouchMoveEvent(TouchMoveEventHandler callback)
{
    gTouchMoveCallbacks.push_back(callback);
}

void unregisterTouchMoveEvent(TouchMoveEventHandler callback)
{
    gTouchMoveCallbacks.erase(gTouchMoveCallbacks.find(callback));
}

namespace PlatformEvents
{
	bool wantsMouseCapture = false;
	bool skipMouseCapture = false;

	void onWindowResize(const WindowResizeEventData* data)
	{
		for (WindowResizeEventHandler& callback : gWindowResizeCallbacks)
			callback(data);
	}

	void onKeyboardChar(const KeyboardCharEventData* pData)
	{
		for (KeyboardCharEventHandler& callback : gKeyboardCharCallbacks)
			callback(pData);
	}

	void onKeyboardButton(const KeyboardButtonEventData* pData)
	{
		for (KeyboardButtonEventHandler& callback : gKeyboardButtonCallbacks)
			callback(pData);
	}

	void onMouseMove(const MouseMoveEventData* pData)
	{
		for (MouseMoveEventHandler& callback : gMouseMoveCallbacks)
			callback(pData);
	}

	void onRawMouseMove(const RawMouseMoveEventData* pData)
	{
		for (RawMouseMoveEventHandler& callback : gRawMouseMoveCallbacks)
			callback(pData);
	}

	void onMouseButton(const MouseButtonEventData* pData)
	{
		for (MouseButtonEventHandler& callback : gMouseButtonCallbacks)
			callback(pData);
	}

	void onMouseWheel(const MouseWheelEventData* pData)
	{
		for (MouseWheelEventHandler& callback : gMouseWheelCallbacks)
			callback(pData);
	}

	void onJoystickButton(const JoystickButtonEventData* pData)
	{
		for (JoystickButtonEventHandler& callback : gJoystickButtonCallbacks)
			callback(pData);
	}
    
    // TODO: Add multitouch event handlers.
    void onTouch(const TouchEventData* pData)
    {
        for (TouchEventHandler& callback : gTouchCallbacks)
            callback(pData);
    }
    
    void onTouchMove(const TouchEventData* pData)
    {
        for (TouchMoveEventHandler& callback : gTouchMoveCallbacks)
            callback(pData);
    }
    
} // namespace PlatformEvents

bool requestMouseCapture(bool allowCapture)
{
	bool rv = PlatformEvents::wantsMouseCapture;
	PlatformEvents::wantsMouseCapture = allowCapture;
	return rv;
}

