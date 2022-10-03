/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "VrApi.h"

#include "VrApi.h"
#include <android_native_app_glue.h>

#include "../../Application/Interfaces/IApp.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Math/MathTypes.h"

#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../Graphics/Quest/VrApiHooks.h"

#include "../../Utilities/Interfaces/IMemory.h"

QuestVR* pQuest = NULL;

#if defined(QUEST_VR)

extern Queue* pSynchronisationQueue;

bool initVrApi(android_app* pAndroidApp, JNIEnv* pJavaEnv)
{
    ovrJava java = {};
    java.Vm = pAndroidApp->activity->vm;
    java.Env = pJavaEnv;
    java.ActivityObject = pAndroidApp->activity->clazz;

    ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    initParms.GraphicsAPI = VRAPI_GRAPHICS_API_VULKAN_1;
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        LOGF(eERROR, "Failed to initialize VrApi");
        return false;
    }

    pQuest = (QuestVR*)tf_malloc(sizeof(QuestVR));
    memset(pQuest, 0, sizeof(QuestVR));
    pQuest->mJava = java;

    pQuest->mEyeTextureWidth = vrapi_GetSystemPropertyInt(&pQuest->mJava, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
    pQuest->mEyeTextureHeight = vrapi_GetSystemPropertyInt(&pQuest->mJava, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);

    return true;
}

void exitVrApi()
{
    if (pQuest->pOvr)
    {
        vrapi_LeaveVrMode(pQuest->pOvr);
        vrapi_Shutdown();
        pQuest->pOvr = NULL;
    }
    tf_free(pQuest);
}

void updateVrApi()
{
    ASSERT(pQuest);
    ASSERT(pQuest->pOvr);

    ++pQuest->mFrameIndex;
    ASSERT(vrapi_WaitFrame(pQuest->pOvr, pQuest->mFrameIndex) == ovrSuccess);

    pQuest->mPredictedDisplayTime = vrapi_GetPredictedDisplayTime(pQuest->pOvr, pQuest->mFrameIndex);
    pQuest->mHeadsetTracking = vrapi_GetPredictedTracking2(pQuest->pOvr, pQuest->mPredictedDisplayTime);

    if (pQuest)
    {
        vec3 headPosition = f3Tov3(*(float3*)&pQuest->mHeadsetTracking.HeadPose.Pose.Position);
        Quat headOrientation = *(Quat*)&pQuest->mHeadsetTracking.HeadPose.Pose.Orientation;
        // We have to invert the roll.
        // Below we convert the projection matrix the left handed which then inverts it again.
        headOrientation.setZ(-headOrientation.getZ());

        pQuest->mViewMatrix = mat4::translation(headPosition) * mat4::rotation(headOrientation); 
    }
}

void hook_poll_events(bool appResumed, bool windowReady, ANativeWindow* nativeWindow)
{
    if (appResumed && windowReady)
    {
        if (!pQuest->pOvr)
        {
            ovrModeParmsVulkan parms =
                vrapi_DefaultModeParmsVulkan(&pQuest->mJava, (unsigned long long)pSynchronisationQueue->mVulkan.pVkQueue);
            parms.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
            parms.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			if(pQuest->isSrgb)
				parms.ModeParms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
            parms.ModeParms.WindowSurface = (unsigned long long)nativeWindow;
            // Leave explicit egl objects defaulted.
            parms.ModeParms.Display = 0;
            parms.ModeParms.ShareContext = 0;

            LOGF(eINFO, "vrapi_EnterVrMode()");

            // gAssertOnVkValidationError is used to work around a bug in the ovr mobile sdk.
            // There is a fence creation struct that is not initialized in the sdk.
            // We temporarily disable asserts for this specific ovr mobile sdk function.
            extern bool gAssertOnVkValidationError;
            gAssertOnVkValidationError = false;
            pQuest->pOvr = vrapi_EnterVrMode((ovrModeParms*)&parms);
            gAssertOnVkValidationError = true;

            vrapi_SetTrackingSpace(pQuest->pOvr, VRAPI_TRACKING_SPACE_LOCAL);
        }
    }
    else if(pQuest->pOvr)
    {
        LOGF(eINFO, "vrapi_LeaveVrMode()");
        vrapi_LeaveVrMode(pQuest->pOvr);
        pQuest->pOvr = NULL;
    }
}

#endif
