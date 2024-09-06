/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#if !defined(ENABLE_FORGE_TOUCH_INPUT)
#error : ENABLE_FORGE_TOUCH_INPUT must be defined to include TouchInput.h
#endif

static int32_t gCursorPos[2] = {};

static void ProcessVirtualJoysticks(uint32_t width, uint32_t height, float dt, const TouchEvent& event)
{
    // Process left virtual joystick
    if (gVirtualJoystickLeft.mActive)
    {
        VirtualJoystick& joystick = gVirtualJoystickLeft;
        if (TOUCH_BEGAN == event.mPhase)
        {
            if (event.mPos[0] <= width * 0.5f && joystick.mId == TOUCH_ID_INVALID)
            {
                joystick.mId = event.mId;
                joystick.mStartPos = { (float)event.mPos[0], (float)event.mPos[1] };
                joystick.mPos = joystick.mStartPos;
                joystick.mSticks = {};
            }
        }
        else if (TOUCH_MOVED == event.mPhase && joystick.mId == event.mId)
        {
            joystick.mPos = { (float)event.mPos[0], (float)event.mPos[1] };
            const float2 offset = joystick.mPos - joystick.mStartPos;
            float        lenSq = offset.x * offset.x + offset.y * offset.y;
            const float  deadZone = joystick.mDeadZone;
            if (lenSq < deadZone * deadZone)
            {
                lenSq = 0.0f;
            }

            if (lenSq == 0.0f)
            {
                joystick.mSticks = {};
            }
            else
            {
                float  res = (float)height / width;
                float2 scaledOffset = offset / res;
                scaledOffset /= joystick.mRadius;
                scaledOffset.x = clamp(scaledOffset.x, -1.0f, 1.0f);
                scaledOffset.y = -clamp(scaledOffset.y, -1.0f, 1.0f);
                joystick.mSticks = scaledOffset;
            }
        }
        else if (TOUCH_ENDED == event.mPhase && joystick.mId == event.mId)
        {
            joystick.mPos = { (float)event.mPos[0], (float)event.mPos[1] };
            joystick.mSticks = {};
            joystick.mId = TOUCH_ID_INVALID;
        }
    }
    // Process right virtual joystick
    if (gVirtualJoystickRight.mActive)
    {
        VirtualJoystick& joystick = gVirtualJoystickRight;

        if (TOUCH_BEGAN == event.mPhase)
        {
            if (event.mPos[0] > width * 0.5f && joystick.mId == TOUCH_ID_INVALID)
            {
                joystick.mId = event.mId;
                joystick.mStartPos = { (float)event.mPos[0], (float)event.mPos[1] };
                joystick.mPos = joystick.mStartPos;
                joystick.mSticks = {};
            }
        }
        else if (TOUCH_MOVED == event.mPhase && joystick.mId == event.mId)
        {
            joystick.mSticks = {};
            joystick.mPos = { (float)event.mPos[0], (float)event.mPos[1] };
            const float2 offset = joystick.mPos - joystick.mStartPos;
            float        lenSq = offset.x * offset.x + offset.y * offset.y;
            const float  deadZone = joystick.mDeadZone;
            if (lenSq < deadZone * deadZone)
            {
                lenSq = 0.0f;
            }

            if (lenSq == 0.0f)
            {
                joystick.mSticks = {};
            }
            else
            {
                float2 scaledOffset = offset * ((float)height / width);
                joystick.mSticks = { scaledOffset.x, -scaledOffset.y };
            }

            joystick.mStartPos = joystick.mPos;
        }
        else if (TOUCH_ENDED == event.mPhase && joystick.mId == event.mId)
        {
            joystick.mPos = { (float)event.mPos[0], (float)event.mPos[1] };
            joystick.mSticks = {};
            joystick.mId = TOUCH_ID_INVALID;
        }
    }
}

void InputGetVirtualJoystickData(bool* outActive, bool* outPressed, float* outRadius, float* outDeadzone, float2* outStartPos,
                                 float2* outPos)
{
    if (outActive)
    {
        *outActive = gVirtualJoystickLeft.mActive && gVirtualJoystickEnable;
    }
    if (outPressed)
    {
        *outPressed = (gVirtualJoystickLeft.mId != TOUCH_ID_INVALID);
    }
    if (outRadius)
    {
        *outRadius = gVirtualJoystickLeft.mRadius;
    }
    if (outDeadzone)
    {
        *outDeadzone = gVirtualJoystickLeft.mDeadZone;
    }
    if (outStartPos)
    {
        *outStartPos = gVirtualJoystickLeft.mStartPos;
    }
    if (outPos)
    {
        *outPos = gVirtualJoystickLeft.mPos;
    }
}
/************************************************************************/
// Touch
/************************************************************************/
struct TouchFinger
{
    TouchEvent mEvent;
    int32_t    mLastPos[2];
};

// Internal
static constexpr uint32_t MAX_TOUCH_EVENTS = 128;
static TouchEvent         gTouchEvents[MAX_TOUCH_EVENTS] = {};
static uint32_t           gTouchEventCount = 0;

static constexpr uint32_t MAX_TOUCH_FINGER_COUNT = 10;
static TouchFinger        gFingers[MAX_TOUCH_FINGER_COUNT] = {};

extern InputTouchEventCallback pCustomTouchEventFn;
extern void*                   pCustomTouchEventCallbackData;

static void AddTouchEvent(const TouchEvent& event)
{
    if (gTouchEventCount >= TF_ARRAY_COUNT(gTouchEvents))
    {
        ++gTouchEventCount;
        return;
    }

    gTouchEvents[gTouchEventCount++] = event;
}

static void ResetFinger(TouchFinger& finger)
{
    if (TOUCH_ID_INVALID == finger.mEvent.mId)
    {
        return;
    }

    finger.mEvent.mId = TOUCH_ID_INVALID;
}

static void ClearFingers()
{
    for (uint32_t i = 0; i < MAX_TOUCH_FINGER_COUNT; ++i)
    {
        TouchFinger& finger = gFingers[i];
        if (finger.mEvent.mId == TOUCH_ID_INVALID)
        {
            continue;
        }
        TouchEvent event = finger.mEvent;
        event.mPhase = TOUCH_ENDED;
        AddTouchEvent(event);
        ResetFinger(finger);
    }
}

static uint32_t GetFingerIndex(int32_t id)
{
    for (uint32_t i = 0; i < MAX_TOUCH_FINGER_COUNT; ++i)
    {
        TouchFinger& finger = gFingers[i];
        if (finger.mEvent.mId == id)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

static TouchFinger* FindFingerWithId(int32_t id)
{
    uint32_t fingerIdx = GetFingerIndex(id);
    if (fingerIdx == UINT32_MAX)
    {
        return NULL;
    }
    return &gFingers[fingerIdx];
}

static bool UpdateFinger(const TouchEvent& event)
{
    constexpr bool TOUCH_VALID = false;
    constexpr bool TOUCH_DISCARD = true;

    if (TOUCH_ID_INVALID == event.mId)
    {
        return TOUCH_DISCARD;
    }

    TouchFinger* finger = NULL;
    switch (event.mPhase)
    {
    case TOUCH_BEGAN:
        finger = FindFingerWithId(TOUCH_ID_INVALID);
        break;
    case TOUCH_ENDED:
    case TOUCH_MOVED:
        finger = FindFingerWithId(event.mId);
        break;
    case TOUCH_CANCELED:
        ClearFingers();
        return TOUCH_DISCARD;
    default:
        return TOUCH_DISCARD;
    }

    if (!finger)
    {
        return TOUCH_DISCARD;
    }

    finger->mEvent.mPhase = event.mPhase;

    switch (event.mPhase)
    {
    case TOUCH_BEGAN:
        finger->mEvent = event;
        finger->mLastPos[0] = event.mPos[0];
        finger->mLastPos[1] = event.mPos[1];
        break;

    case TOUCH_MOVED:
        finger->mEvent.mPos[0] = event.mPos[0];
        finger->mEvent.mPos[1] = event.mPos[1];
        break;

    case TOUCH_ENDED:
        ResetFinger(*finger);
        break;

    default:
        ASSERTFAIL("Unexpected TouchPhase %d", event.mPhase);
        break;
    }

    return TOUCH_VALID;
}

static void ProcessTouchEvents(uint32_t width, uint32_t height, float dt)
{
    gVirtualJoystickRight.mSticks = {};
    gVirtualJoystickLeft.mActive = gVirtualJoystickEnable;
    gVirtualJoystickRight.mActive = gVirtualJoystickEnable;

    uint32_t eventCount = gTouchEventCount;
    gTouchEventCount = 0;

    if (eventCount > TF_ARRAY_COUNT(gTouchEvents))
    {
        ClearFingers();
        return;
    }

    TouchEvent events[MAX_TOUCH_EVENTS] = {};
    memcpy(events, gTouchEvents, eventCount * sizeof(TouchEvent));
    for (size_t eventIdx = 0; eventIdx < eventCount; ++eventIdx)
    {
        TouchEvent& event = events[eventIdx];
        const bool  discard = UpdateFinger(event);
        if (discard)
        {
            continue;
        }

        switch (event.mPhase)
        {
        case TOUCH_BEGAN:
        case TOUCH_ENDED:
        {
            gCursorPos[0] = event.mPos[0];
            gCursorPos[1] = event.mPos[1];

            // Queue touch event that will be used by the game
            if (pCustomTouchEventFn)
            {
                pCustomTouchEventFn(&event, pCustomTouchEventCallbackData);
            }
            ProcessVirtualJoysticks(width, height, dt, event);
        }
        break;

        case TOUCH_MOVED:
        {
            bool isLastMoved = true;
            for (size_t nextEventIdx = eventIdx + 1; nextEventIdx < eventCount; ++nextEventIdx)
            {
                const TouchEvent& nextEvent = events[nextEventIdx];
                if (event.mId == nextEvent.mId && TOUCH_MOVED == nextEvent.mPhase)
                {
                    isLastMoved = false;
                    break;
                }
            }
            if (!isLastMoved)
            {
                continue;
            }
            TouchFinger* finger = FindFingerWithId(event.mId);
            if (finger)
            {
                if (finger->mLastPos[0] != finger->mEvent.mPos[0] || finger->mLastPos[1] != finger->mEvent.mPos[1])
                {
                    gCursorPos[0] = event.mPos[0];
                    gCursorPos[1] = event.mPos[1];

                    // Queue touch event that will be used by the game
                    if (pCustomTouchEventFn)
                    {
                        pCustomTouchEventFn(&event, pCustomTouchEventCallbackData);
                    }
                    ProcessVirtualJoysticks(width, height, dt, event);

                    finger->mLastPos[0] = finger->mEvent.mPos[0];
                    finger->mLastPos[1] = finger->mEvent.mPos[1];
                }
            }
            else
            {
                LOGF(eWARNING, "Missing touch finger for event id %d", event.mId);
            }
            break;
        }
        default:
            ASSERTFAIL("Unknown TouchPhase %d", event.mPhase);
            break;
        }
    }

    gInputValues[VGPAD_LX] = gVirtualJoystickLeft.mSticks.x;
    gInputValues[VGPAD_LY] = gVirtualJoystickLeft.mSticks.y;
    gInputValues[VGPAD_RX] = gVirtualJoystickRight.mSticks.x;
    gInputValues[VGPAD_RY] = gVirtualJoystickRight.mSticks.y;
}

static void ResetTouchEvents()
{
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(gFingers); ++i)
    {
        gFingers[i].mEvent.mId = TOUCH_ID_INVALID;
    }

    gVirtualJoystickLeft = {};
    gVirtualJoystickRight = {};
    gVirtualJoystickLeft.mId = TOUCH_ID_INVALID;
    gVirtualJoystickRight.mId = TOUCH_ID_INVALID;
    gVirtualJoystickLeft.mRadius = 200.0f;
}
