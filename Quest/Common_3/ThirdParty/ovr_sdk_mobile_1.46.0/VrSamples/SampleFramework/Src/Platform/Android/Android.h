/*******************************************************************************

Filename	:   Android.h
Content		:   Android-specific types for VRSamples
Created		:   March 20, 2018
Authors		:   Jonathan Wright
Language	:   C++

Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#pragma once

#include "../AppContext.h"
#include <android/native_window_jni.h> // for native window JNI
#include "VrApi_Types.h"

namespace OVRFW {

class ovrAndroidContext : public ovrAppContext {
   public:
    ovrAndroidContext();

    virtual const ovrContext* ContextForVrApi() const override {
        return reinterpret_cast<const ovrContext*>(&Java);
    }

    void Init(JavaVM* vm, jobject activityObject, const char* threadName);
    void Shutdown();

   private:
    ovrJava Java;
};

} // namespace OVRFW
