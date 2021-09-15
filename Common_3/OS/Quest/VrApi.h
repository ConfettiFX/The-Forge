#pragma once

#if defined(QUEST_VR)

#include "../../ThirdParty/OpenSource/ovr_sdk_mobile_1.46.0/VrApi/Include/VrApi.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../Math/MathTypes.h"

bool initVrApi(android_app* pAndroidApp, JNIEnv* pJavaEnv);
void exitVrApi();
void updateVrApi();

void hook_poll_events(bool appResumed, bool windowReady, ANativeWindow* nativeWindow);
uint hook_window_width();
uint hook_window_height();

bool isHeadsetReady();
ovrMobile* getOvrContext();
ovrTracking2* getHeadsetPose();
uint getVrApiFrameIndex();
double getPredictedDisplayTime();
mat4 getHeadsetViewMatrix();
mat4 getHeadsetLeftEyeProjectionMatrix(float zNear, float zFar);
mat4 getHeadsetRightEyeProjectionMatrix(float zNear, float zFar);
void setFoveatedRendering(bool enable);
bool isFoveatedRenderingEnabled();

#endif