/*******************************************************************************

Filename    :   Main.cpp
Content     :   Base project for mobile VR samples
Created     :   February 21, 2018
Authors     :   John Carmack, J.M.P. van Waveren, Jonathan Wright
Language    :   C++

Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include "Platform/Android/Android.h"

#include <android/window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>

#include <memory>

#include "Appl.h"
#include "VrHands.h"

//==============================================================
// android_main
//==============================================================
void android_main(struct android_app* app) {
    std::unique_ptr<OVRFW::ovrVrHands> appl =
        std::unique_ptr<OVRFW::ovrVrHands>(new OVRFW::ovrVrHands(0, 0, 0, 0));
    appl->Run(app);
}
