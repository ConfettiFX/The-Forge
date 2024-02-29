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

#if defined(QUEST_VR)

#include "../ThirdParty/PrivateOculus/ovr_sdk_mobile/VrApi/Include/VrApi.h"
#include "../ThirdParty/PrivateOculus/ovr_sdk_mobile/VrApi/Include/VrApi_Helpers.h"

#include "../Interfaces/IOperatingSystem.h"

#include "../../Utilities/Math/MathTypes.h"

struct QuestVR
{
    ovrMobile*         pOvr;
    ovrJava            mJava;
    double             mPredictedDisplayTime;
    unsigned long long mFrameIndex;
    ovrTracking2       mHeadsetTracking;
    uint               mEyeTextureWidth;
    uint               mEyeTextureHeight;
    mat4               mViewMatrix;
    bool               mFoveatedRenderingEnabled;
    bool               isSrgb;
};
extern QuestVR* pQuest;

bool initVrApi(android_app* pAndroidApp, JNIEnv* pJavaEnv);
void exitVrApi();
void updateVrApi();

void hook_poll_events(bool appResumed, bool windowReady, ANativeWindow* nativeWindow);

#endif
