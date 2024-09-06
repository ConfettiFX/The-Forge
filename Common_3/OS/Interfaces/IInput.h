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

#include "../../Utilities/Math/MathTypes.h"

// Low-level input interface to be implemented by each platform
// This interface can be used to implement higher-level platform independent input handling on the app level
// See Application/Input/Input.cpp - Reference implementation of platform agnostic input using input actions

enum InputEnum : uint16_t;
typedef int32_t             InputPortIndex;
static const InputPortIndex PORT_INDEX_INVALID = -1;

static const uint32_t MAX_GAMEPADS = 8;
static const float    GAMEPAD_DEADZONE_DEFAULT_MIN = 0.1f;
static const float    GAMEPAD_DEADZONE_DEFAULT_MAX = 0.01f;
typedef void (*GamepadCallback)(InputPortIndex port);

enum InputEffect
{
    INPUT_EFFECT_NONE = 0,
    INPUT_EFFECT_GPAD_RUMBLE_LOW,
    INPUT_EFFECT_GPAD_RUMBLE_HIGH,
    INPUT_EFFECT_GPAD_LIGHT,
    INPUT_EFFECT_GPAD_LIGHT_RESET,
};

union InputEffectValue
{
    float3 mLight;
    float  mRumble;
};

float              inputGetValue(InputPortIndex index, InputEnum btn);
float              inputGetLastValue(InputPortIndex index, InputEnum btn);
static inline bool inputGetValueReset(InputPortIndex index, InputEnum btn)
{
    // Button released, ...
    return inputGetLastValue(index, btn) && !inputGetValue(index, btn);
}
void      inputAddCustomBindings(const char* bindings);
void      inputRemoveCustomBinding(InputEnum binding);
InputEnum inputGetCustomBindingEnum(const char* name);

void        inputGamepadSetAddedCallback(GamepadCallback cb);
void        inputGamepadSetRemovedCallback(GamepadCallback cb);
bool        inputGamepadIsActive(InputPortIndex index);
const char* inputGamepadName(InputPortIndex index);
void        inputGamepadSetDeadzone(InputPortIndex index, InputEnum btn, float minDeadzone, float maxDeadzone);
void        inputSetEffect(InputPortIndex index, InputEffect effect, const InputEffectValue& value);

void inputGetCharInput(char32_t** pOutChars, uint32_t* pCount);
