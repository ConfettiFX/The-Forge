/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IInput.h"
#include "../Input/InputCommon.h"

#include "../../Utilities/Interfaces/IMemory.h"

#include <openxr/openxr_platform.h>
#include <xr_linear.h>

#define GAMEPAD_INDEX          0
#define CONTROLLER_LEFT_INDEX  0
#define CONTROLLER_RIGHT_INDEX 1
#define CONTROLLER_COUNT       2
#define EMULATE_DPAD_THRESHOLD 0.9

struct OpenXRInputAction
{
    XrAction                 mAction;
    XrActionSuggestedBinding mBinding;
    XrSpace                  mSpace;
};

struct ControllerPoseData
{
    bool    mIsValid;
    Vector3 mPosition;
    Vector3 mDirection;
};

typedef struct OpenXRInputBindings
{
    XrActionSet        mActionSet;
    bool               mActionSetBound;
    OpenXRInputAction  mButtonActions[GPAD_BTN_COUNT];
    OpenXRInputAction  mAxisActions[GPAD_AXIS_COUNT];
    OpenXRInputAction  mControllerPoseActions[CONTROLLER_COUNT];
    ControllerPoseData mControllerPose[CONTROLLER_COUNT];
} OpenXRInputBindings;

OpenXRInputBindings gXrInput;

// TODO: Use plain C once IInput doesn't depend on C++
extern "C"
{
    extern XrInstance    GetCurrentXRInstance();
    extern XrSession     GetCurrentXRSession();
    extern XrFrameState* GetCurrentXRFrameState();
    extern XrSpace       GetXRLocalSpace();
    extern XrResult      VerifyOXRResult(XrResult res, const char* func, const char* cmd, bool assertOnErr);
}

// OpenXR specific debugging macro to check function results (similar to CHECK_VKRESULT)
#define CHECK_OXRRESULT(func) VerifyOXRResult(func, __FUNCTION__, #func, true)

XrPath ToOpenXRPath(const char* pathStr)
{
    XrPath result = XR_NULL_PATH;
    CHECK_OXRRESULT(xrStringToPath(GetCurrentXRInstance(), pathStr, &result));
    return result;
}

OpenXRInputAction AddAction(const char* actionName, const char* actionPath, XrActionType actionType)
{
    OpenXRInputAction  result;
    XrActionCreateInfo actionCreateInfo = { XR_TYPE_ACTION_CREATE_INFO };
    actionCreateInfo.actionType = actionType;
    strcpy(actionCreateInfo.actionName, actionName);
    strcpy(actionCreateInfo.localizedActionName, actionName);
    CHECK_OXRRESULT(xrCreateAction(gXrInput.mActionSet, &actionCreateInfo, &result.mAction));
    result.mBinding = { result.mAction, ToOpenXRPath(actionPath) };
    return result;
}

void RemoveAction(OpenXRInputAction& action)
{
    if (action.mSpace != XR_NULL_HANDLE)
    {
        xrDestroySpace(action.mSpace);
    }

    if (action.mAction != XR_NULL_HANDLE)
    {
        xrDestroyAction(action.mAction);
    }
}

void AttachActionSpace(OpenXRInputAction& action)
{
    XrActionSpaceCreateInfo actionSpaceCreateInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
    XrPosef_CreateIdentity(&actionSpaceCreateInfo.poseInActionSpace);
    actionSpaceCreateInfo.action = action.mAction;
    CHECK_OXRRESULT(xrCreateActionSpace(GetCurrentXRSession(), &actionSpaceCreateInfo, &action.mSpace));
}

void AddInputActions()
{
    // Initialize XrActions. These represent events attached to an input source. We already provide an interface to create action bindings,
    // so these will be mapped 1-1 to buttons or axes depending on their type. No high level events are handled here.
    {
        for (uint32_t i = 0; i < GPAD_BTN_COUNT; ++i)
        {
            gXrInput.mButtonActions[i].mAction = XR_NULL_HANDLE;
        }

        for (uint32_t i = 0; i < GPAD_AXIS_COUNT; ++i)
        {
            gXrInput.mAxisActions[i].mAction = XR_NULL_HANDLE;
        }

        gXrInput.mButtonActions[GPAD_A - GPAD_BTN_FIRST] =
            AddAction("gpad_a", "/user/hand/right/input/a/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_B - GPAD_BTN_FIRST] =
            AddAction("gpad_b", "/user/hand/right/input/b/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_X - GPAD_BTN_FIRST] =
            AddAction("gpad_x", "/user/hand/left/input/x/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_Y - GPAD_BTN_FIRST] =
            AddAction("gpad_y", "/user/hand/left/input/y/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_START - GPAD_BTN_FIRST] =
            AddAction("gpad_start", "/user/hand/left/input/menu/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_L3 - GPAD_BTN_FIRST] =
            AddAction("gpad_l3", "/user/hand/left/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_R3 - GPAD_BTN_FIRST] =
            AddAction("gpad_r3", "/user/hand/right/input/thumbstick/click", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_L1 - GPAD_BTN_FIRST] =
            AddAction("gpad_l1", "/user/hand/left/input/trigger/value", XR_ACTION_TYPE_BOOLEAN_INPUT);
        gXrInput.mButtonActions[GPAD_R1 - GPAD_BTN_FIRST] =
            AddAction("gpad_r1", "/user/hand/right/input/trigger/value", XR_ACTION_TYPE_BOOLEAN_INPUT);

        gXrInput.mAxisActions[GPAD_LX - GPAD_AXIS_FIRST] =
            AddAction("gpad_lx", "/user/hand/left/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT);
        gXrInput.mAxisActions[GPAD_LY - GPAD_AXIS_FIRST] =
            AddAction("gpad_ly", "/user/hand/left/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT);
        gXrInput.mAxisActions[GPAD_RX - GPAD_AXIS_FIRST] =
            AddAction("gpad_rx", "/user/hand/right/input/thumbstick/x", XR_ACTION_TYPE_FLOAT_INPUT);
        gXrInput.mAxisActions[GPAD_RY - GPAD_AXIS_FIRST] =
            AddAction("gpad_ry", "/user/hand/right/input/thumbstick/y", XR_ACTION_TYPE_FLOAT_INPUT);
        gXrInput.mAxisActions[GPAD_L2 - GPAD_AXIS_FIRST] =
            AddAction("gpad_l2", "/user/hand/left/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT);
        gXrInput.mAxisActions[GPAD_R2 - GPAD_AXIS_FIRST] =
            AddAction("gpad_r2", "/user/hand/right/input/squeeze/value", XR_ACTION_TYPE_FLOAT_INPUT);

        gXrInput.mControllerPoseActions[CONTROLLER_LEFT_INDEX] =
            AddAction("hand_post_left", "/user/hand/left/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT);
        gXrInput.mControllerPoseActions[CONTROLLER_RIGHT_INDEX] =
            AddAction("hand_post_right", "/user/hand/right/input/grip/pose", XR_ACTION_TYPE_POSE_INPUT);
    }

    // Suggest Touch Controller bindings, these are exclusive to Oculus although the XR runtime might map it to other devices. We just
    // support Quest, for now.
    {
        uint32_t                 totalActionCount = 0;
        XrActionSuggestedBinding bindings[GPAD_BTN_COUNT + GPAD_AXIS_COUNT + 2];
        for (uint32_t i = 0; i < GPAD_BTN_COUNT; ++i)
        {
            if (gXrInput.mButtonActions[i].mAction != XR_NULL_HANDLE)
            {
                bindings[totalActionCount] = gXrInput.mButtonActions[i].mBinding;
                ++totalActionCount;
            }
        }
        for (uint32_t i = 0; i < GPAD_AXIS_COUNT; ++i)
        {
            if (gXrInput.mAxisActions[i].mAction != XR_NULL_HANDLE)
            {
                bindings[totalActionCount] = gXrInput.mAxisActions[i].mBinding;
                ++totalActionCount;
            }
        }

        for (uint32_t i = 0; i < CONTROLLER_COUNT; ++i)
        {
            bindings[totalActionCount] = gXrInput.mControllerPoseActions[i].mBinding;
            ++totalActionCount;
        }

        XrInteractionProfileSuggestedBinding suggestedBindings = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
        suggestedBindings.interactionProfile = ToOpenXRPath("/interaction_profiles/oculus/touch_controller");
        suggestedBindings.countSuggestedBindings = totalActionCount;
        suggestedBindings.suggestedBindings = bindings;
        CHECK_OXRRESULT(xrSuggestInteractionProfileBindings(GetCurrentXRInstance(), &suggestedBindings));
    }
}

void RemoveInputActions()
{
    for (uint32_t i = 0; i < GPAD_BTN_COUNT; ++i)
    {
        RemoveAction(gXrInput.mButtonActions[i]);
    }

    for (uint32_t i = 0; i < GPAD_AXIS_COUNT; ++i)
    {
        RemoveAction(gXrInput.mAxisActions[i]);
    }

    RemoveAction(gXrInput.mControllerPoseActions[CONTROLLER_LEFT_INDEX]);
    RemoveAction(gXrInput.mControllerPoseActions[CONTROLLER_RIGHT_INDEX]);
}

/************************************************************************/
// Platform implementation
/************************************************************************/
void platformInitInput(JNIEnv* env, jobject activity)
{
    // Create action set, this will be the context for all input actions for the current session
    {
        XrActionSetCreateInfo actionSetCreateInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy(actionSetCreateInfo.actionSetName, "the_forge_actions");
        strcpy(actionSetCreateInfo.localizedActionSetName, "The Forge - Actions");
        actionSetCreateInfo.priority = 0;
        CHECK_OXRRESULT(xrCreateActionSet(GetCurrentXRInstance(), &actionSetCreateInfo, &gXrInput.mActionSet));
    }

    InputInitCommon();

    AddInputActions();

    gXrInput.mActionSetBound = false;
}

void platformExitInput(JNIEnv* env)
{
    gXrInput.mActionSetBound = false;
    RemoveInputActions();
    xrDestroyActionSet(gXrInput.mActionSet);
}

void platformUpdateLastInputState(android_app* app)
{
    memcpy(gLastInputValues, gInputValues, sizeof(gInputValues));

    gInputValues[MOUSE_WHEEL_UP] = false;
    gInputValues[MOUSE_WHEEL_DOWN] = false;
    gInputValues[MOUSE_DX] = 0;
    gInputValues[MOUSE_DY] = 0;
}

XrActionStateBoolean GetActionStateBoolean(OpenXRInputAction& action)
{
    XrActionStateBoolean state = { XR_TYPE_ACTION_STATE_BOOLEAN };
    state.isActive = false;
    state.currentState = false;
    if (action.mAction != XR_NULL_HANDLE)
    {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = action.mAction;
        CHECK_OXRRESULT(xrGetActionStateBoolean(GetCurrentXRSession(), &getInfo, &state));
    }
    return state;
}

XrActionStateFloat GetActionStateFloat(OpenXRInputAction& action)
{
    XrActionStateFloat state = { XR_TYPE_ACTION_STATE_FLOAT };
    state.isActive = false;
    state.currentState = 0.0f;
    if (action.mAction != XR_NULL_HANDLE)
    {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = action.mAction;
        CHECK_OXRRESULT(xrGetActionStateFloat(GetCurrentXRSession(), &getInfo, &state));
    }
    return state;
}

ControllerPoseData GetActionPose(OpenXRInputAction& action)
{
    ControllerPoseData result;
    result.mIsValid = false;

    XrFrameState* pFrameState = GetCurrentXRFrameState();

    bool isFrameValid = pFrameState != NULL && pFrameState->predictedDisplayTime > 0;

    if (action.mAction != XR_NULL_HANDLE && isFrameValid)
    {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = action.mAction;
        XrActionStatePose state = { XR_TYPE_ACTION_STATE_POSE };

        CHECK_OXRRESULT(xrGetActionStatePose(GetCurrentXRSession(), &getInfo, &state));

        // Pose will always be relative to mLocalSpace
        XrSpaceLocation location = { XR_TYPE_SPACE_LOCATION };
        CHECK_OXRRESULT(xrLocateSpace(action.mSpace, GetXRLocalSpace(), pFrameState->predictedDisplayTime, &location));

        if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0)
        {
            XrQuaternionf& orientation = location.pose.orientation;
            XrVector3f&    position = location.pose.position;
            result.mIsValid = true;
            result.mPosition = Vector3(position.x, position.y, position.z);
            Quat rotation = Quat(orientation.x, orientation.y, orientation.z, orientation.w);

            // In the XR_REFERENCE_SPACE_TYPE_LOCAL space (0,0,-1) is forward
            Vector3 direction = Vector3(0.0f, 0.0f, -1.0f);
            result.mDirection = rotate(rotation, direction);
        }
    }

    return result;
}

void platformUpdateInput(android_app* app, JNIEnv* env, uint32_t width, uint32_t height, float dt)
{
    Gamepad& gamePad = gGamepads[GAMEPAD_INDEX];

    // Attach action set to current session
    if (!gXrInput.mActionSetBound)
    {
        XrSessionActionSetsAttachInfo actionSetsAttachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        actionSetsAttachInfo.countActionSets = 1;
        actionSetsAttachInfo.actionSets = &gXrInput.mActionSet;
        CHECK_OXRRESULT(xrAttachSessionActionSets(GetCurrentXRSession(), &actionSetsAttachInfo));
        gXrInput.mActionSetBound = true;
        gamePad.mActive = true;
        gamePad.pName = "Oculus Touch Controller";

        AttachActionSpace(gXrInput.mControllerPoseActions[CONTROLLER_LEFT_INDEX]);
        AttachActionSpace(gXrInput.mControllerPoseActions[CONTROLLER_RIGHT_INDEX]);
    }

    XrActionsSyncInfo syncInfo = { XR_TYPE_ACTIONS_SYNC_INFO };
    syncInfo.countActiveActionSets = 1;
    XrActiveActionSet activeActionSet = { gXrInput.mActionSet, XR_NULL_PATH };
    syncInfo.activeActionSets = &activeActionSet;
    CHECK_OXRRESULT(xrSyncActions(GetCurrentXRSession(), &syncInfo));

    for (uint32_t i = 0; i < GPAD_BTN_COUNT; ++i)
    {
        XrActionStateBoolean state = GetActionStateBoolean(gXrInput.mButtonActions[i]);
        gamePad.mButtons[i] = state.currentState;
    }

    for (uint32_t i = 0; i < GPAD_AXIS_COUNT; ++i)
    {
        XrActionStateFloat state = GetActionStateFloat(gXrInput.mAxisActions[i]);
        gamePad.mAxis[i] = state.currentState;
    }

    // Emulate D-pad with left analog stick, since Touch Controllers don't have one
    float currentVerticalAxis = gamePad.mAxis[GPAD_LY - GPAD_AXIS_FIRST];
    float currentHorizontalAxis = gamePad.mAxis[GPAD_LX - GPAD_AXIS_FIRST];
    gamePad.mButtons[GPAD_UP - GPAD_BTN_FIRST] = currentVerticalAxis > EMULATE_DPAD_THRESHOLD;
    gamePad.mButtons[GPAD_DOWN - GPAD_BTN_FIRST] = currentVerticalAxis < -EMULATE_DPAD_THRESHOLD;
    gamePad.mButtons[GPAD_LEFT - GPAD_BTN_FIRST] = currentHorizontalAxis < -EMULATE_DPAD_THRESHOLD;
    gamePad.mButtons[GPAD_RIGHT - GPAD_BTN_FIRST] = currentHorizontalAxis > EMULATE_DPAD_THRESHOLD;

    // Update controller poses
    gXrInput.mControllerPose[CONTROLLER_LEFT_INDEX] = GetActionPose(gXrInput.mControllerPoseActions[CONTROLLER_LEFT_INDEX]);
    gXrInput.mControllerPose[CONTROLLER_RIGHT_INDEX] = GetActionPose(gXrInput.mControllerPoseActions[CONTROLLER_RIGHT_INDEX]);

    ControllerPoseData& leftCtrl = gXrInput.mControllerPose[CONTROLLER_LEFT_INDEX];
    gInputValues[VRCTRL_LTR] = leftCtrl.mIsValid ? 1.0f : 0.0f;
    gInputValues[VRCTRL_LPX] = leftCtrl.mPosition.getX();
    gInputValues[VRCTRL_LPY] = leftCtrl.mPosition.getY();
    gInputValues[VRCTRL_LPZ] = leftCtrl.mPosition.getZ();
    gInputValues[VRCTRL_LDX] = leftCtrl.mDirection.getX();
    gInputValues[VRCTRL_LDY] = leftCtrl.mDirection.getY();
    gInputValues[VRCTRL_LDZ] = leftCtrl.mDirection.getZ();

    ControllerPoseData& rightCtrl = gXrInput.mControllerPose[CONTROLLER_RIGHT_INDEX];
    gInputValues[VRCTRL_RTR] = rightCtrl.mIsValid ? 1.0f : 0.0f;
    gInputValues[VRCTRL_RPX] = rightCtrl.mPosition.getX();
    gInputValues[VRCTRL_RPY] = rightCtrl.mPosition.getY();
    gInputValues[VRCTRL_RPZ] = rightCtrl.mPosition.getZ();
    gInputValues[VRCTRL_RDX] = rightCtrl.mDirection.getX();
    gInputValues[VRCTRL_RDY] = rightCtrl.mDirection.getY();
    gInputValues[VRCTRL_RDZ] = rightCtrl.mDirection.getZ();

    gDeltaTime = dt;
}

void platformInputOnStart(JNIEnv* env) {}

void platformInputOnStop(JNIEnv* env) {}
