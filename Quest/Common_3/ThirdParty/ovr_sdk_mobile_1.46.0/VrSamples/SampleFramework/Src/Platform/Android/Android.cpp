/*******************************************************************************

   Filename	:   Android.h
   Content	:   Android-specific types for VRSamples
   Created	:   March 20, 2018
   Authors	:   Jonathan Wright
   Language	:   C++ 2011

   Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include "Android.h"

#include <pthread.h>
#include <sys/prctl.h>

namespace OVRFW {

ovrAndroidContext::ovrAndroidContext() {
    Java.Vm = nullptr;
    Java.Env = nullptr;
    Java.ActivityObject = 0;
}

void ovrAndroidContext::Init(JavaVM* vm, jobject activityObject, const char* threadName) {
    Java.Vm = vm;
    Java.ActivityObject = activityObject;
    Java.Env = nullptr;

    Java.Vm->AttachCurrentThread(&Java.Env, nullptr);

    // AttachCurrentThread resets the thread name
    prctl(PR_SET_NAME, (long)threadName, 0, 0, 0);
}

void ovrAndroidContext::Shutdown() {
    if (Java.Vm != nullptr) {
        Java.Vm->DetachCurrentThread();
    }
    Java.Vm = nullptr;
    Java.Env = nullptr;
    Java.ActivityObject = nullptr;
}

} // namespace OVRFW
