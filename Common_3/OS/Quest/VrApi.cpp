/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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
#include "../../ThirdParty/OpenSource/ovr_sdk_mobile_1.46.0/VrApi/Include/VrApi_Helpers.h"
#include <android_native_app_glue.h>

#include "../Interfaces/IApp.h"
#include "../Interfaces/ILog.h"
#include "../Math/MathTypes.h"

#include "../../Renderer/IRenderer.h"
#include "../../Renderer/Quest/VrApiHooks.h"

#include "../Interfaces/IMemory.h"

struct QuestVR
{
    ovrMobile* pOvr;
    ovrJava mJava;
    double mPredictedDisplayTime;
    unsigned long long mFrameIndex;
    ovrTracking2 mHeadsetTracking;
    uint mEyeTextureWidth;
    uint mEyeTextureHeight;
    mat4 mViewMatrix;
    bool mFoveatedRenderingEnabled;
};

QuestVR* pQuest = NULL;

#if defined(QUEST_VR)
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

bool isHeadsetReady()
{
    return pQuest->pOvr != NULL;
}

void hook_poll_events(bool appResumed, bool windowReady, ANativeWindow* nativeWindow)
{
    if (appResumed && windowReady)
    {
        if (!pQuest->pOvr)
        {
            ovrModeParmsVulkan parms =
                vrapi_DefaultModeParmsVulkan(&pQuest->mJava, (unsigned long long)getSynchronisationQueue()->mVulkan.pVkQueue);
            parms.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;
            parms.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            parms.ModeParms.WindowSurface = (unsigned long long)nativeWindow;
            // Leave explicit egl objects defaulted.
            parms.ModeParms.Display = 0;
            parms.ModeParms.ShareContext = 0;

            LOGF(eINFO, "vrapi_EnterVrMode()");

            pQuest->pOvr = vrapi_EnterVrMode((ovrModeParms*)&parms);

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

ovrMobile* getOvrContext()
{
    ASSERT(pQuest);
    ASSERT(pQuest->pOvr);
    return pQuest->pOvr;
}

ovrTracking2* getHeadsetPose()
{
    ASSERT(pQuest);
    return &pQuest->mHeadsetTracking;
}

uint getVrApiFrameIndex()
{
    ASSERT(pQuest);
    return pQuest->mFrameIndex;
}

double getPredictedDisplayTime()
{
    ASSERT(pQuest);
    return pQuest->mPredictedDisplayTime;
}

uint hook_window_width()
{
    ASSERT(pQuest);
    return pQuest->mEyeTextureWidth;
}

uint hook_window_height()
{
    ASSERT(pQuest);
    return pQuest->mEyeTextureHeight;
}

mat4 getHeadsetViewMatrix()
{
    ASSERT(pQuest);
    return pQuest->mViewMatrix;
}

mat4 getHeadsetLeftEyeProjectionMatrix(float zNear, float zFar)
{
    ASSERT(pQuest);
    float4 fov;
    ovrMatrix4f_ExtractFov(&pQuest->mHeadsetTracking.Eye[VRAPI_EYE_LEFT].ProjectionMatrix, &fov.x, &fov.y, &fov.z, &fov.w);
    return mat4::perspectiveAsymmetricFov(fov.x, fov.y, fov.z, fov.w, zNear, zFar);
}

mat4 getHeadsetRightEyeProjectionMatrix(float zNear, float zFar)
{
    ASSERT(pQuest);
    float4 fov;
    ovrMatrix4f_ExtractFov(&pQuest->mHeadsetTracking.Eye[VRAPI_EYE_RIGHT].ProjectionMatrix, &fov.x, &fov.y, &fov.z, &fov.w);
    return mat4::perspectiveAsymmetricFov(fov.x, fov.y, fov.z, fov.w, zNear, zFar);
}

void setFoveatedRendering(bool enabled)
{
    ASSERT(pQuest);
    vrapi_SetPropertyInt(&pQuest->mJava, VRAPI_DYNAMIC_FOVEATION_ENABLED, enabled);
    vrapi_SetPropertyInt(&pQuest->mJava, VRAPI_FOVEATION_LEVEL, 4);
    pQuest->mFoveatedRenderingEnabled = enabled;
}

bool isFoveatedRenderingEnabled()
{
    return pQuest->mFoveatedRenderingEnabled;
}
#endif