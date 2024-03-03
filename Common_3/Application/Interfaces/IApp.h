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

//
// To design an application interface, think about what the user can do to the application
// change resolution, change device by switching from hardware to Warp, change MSAA, hot-swap an input device
//

/*

// when we are in windowed mode and the user wants to resize the window with the mouse
void onResize(RectDesc rect);

// this pair of functions initializes and exits once during start-up and shut-down of the application
// we open and close all the resources that will never change during the live span of an application
// in other words, they are independent from any user changes to the device and quality settings
// typically this is restricted to for example to loading and unloading geometry
bool init();
void exit();

bool parseArgs(const char* cmdLn);

// this is where we initialize the renderer and bind the OS and Graphics API to the application
bool initApp();
void exitApp();

//
// this pair of functions loads and unloads everything that need to be re-loaded in case of a device change.
// device changes can come from a user switching from hardware to Warp rendering support or switching on and off MSAA and other quality
settings
// typically shaders, textures, render targets and buffers are loaded here
bool load();
void unload();

// hot swap from input device ... mouse, keyboard, controller etc..
void initInput();
void exitInput();

// this is input / math update -> everything CPU only ... no Graphics API calls
void update(float deltaTime);

// only Graphics API draw calls and command buffer generation
void drawFrame(float deltaTime);
*/

// Device Change
// so what is going to happen if the user changes the graphics device?
// There must be a call to exitApp() initApp() to initialize the renderer from scratch ... let's say with a new MSAA settings
// then unload() needs to be called to unload all the device dependent things like shaders and textures (are they still dependent in DirectX
// 12 / Vulkan?) then load() needs to be called to load all the device dependent things from scratch again ... It needs to make sure that
// only the bare minimum of tasks is done here, so no memory allocation and deallocation apart from the necessary

// hot-swap the input device
// the user decides to switch from keyboard to controller input
// we are going to call exitInput() and then initInput()
#ifndef _IAPP_H_
#define _IAPP_H_

#include "../Config.h"

#include "../../OS/Interfaces/IOperatingSystem.h"

class FORGE_API IApp
{
public:
    virtual bool Init() = 0;
    virtual void Exit() = 0;

    virtual bool Load(ReloadDesc* pReloadDesc) = 0;
    virtual void Unload(ReloadDesc* pReloadDesc) = 0;

    virtual void Update(float deltaTime) = 0;
    virtual void Draw() = 0;

    virtual const char* GetName() = 0;

    struct Settings
    {
        /// Window width
        int32_t mWidth = -1;
        /// Window height
        int32_t mHeight = -1;
        /// monitor index
        int32_t mMonitorIndex = -1;
        /// x position for window
        int32_t mWindowX = 0;
        /// y position for window
        int32_t mWindowY = 0;
        /// Set to true if fullscreen mode has been requested
        bool    mFullScreen = false;
        /// Set to true if app wants to use an external window
        bool    mExternalWindow = false;
        /// Drag to resize enabled
        bool    mDragToResize = true;
        /// Border less window
        bool    mBorderlessWindow = false;
        /// Set to true if oversize windows requested
        bool    mAllowedOverSizeWindows = false;
        /// if settings is already initiazlied we don't fill when opening window
        bool    mInitialized = false;
        /// if requested to quit the application
        bool    mQuit = false;
        /// if benchmarking mode is enabled
        bool    mBenchmarking = false;
        /// if the window is positioned in the center of the screen
        bool    mCentered = true;
        /// if the window is focused or in foreground
        bool    mFocused = true;
        /// Force lowDPI settings for this window
        bool    mForceLowDPI = false;
        /// if the platform user interface is visible
        bool    mShowPlatformUI = true;

#ifdef __ANDROID__
        bool mVSyncEnabled = true;
#else
        bool mVSyncEnabled = false;
#endif

#if defined(TARGET_IOS)
        bool    mShowStatusBar = false;
        float   mContentScaleFactor = 0.f;
        // Use to set iPhone's/Ipad target display refresh rate.
        int32_t mMaxDisplayRefreshRate = -1;
#endif
    } mSettings;

    WindowDesc* pWindow = NULL;
    const char* pCommandLine = NULL;

    const char* pUnsupportedReason = NULL;
    bool        mUnsupported = false;

    void ShowUnsupportedMessage(const char* reason)
    {
        mUnsupported = true;
        pUnsupportedReason = reason;
    }

    static int          argc;
    static const char** argv;
};

#if defined(XBOX)
FORGE_API extern int DurangoMain(int argc, char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) DurangoMain(argc, argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                     \
    extern int DurangoMain(int argc, char** argv, IApp* app); \
                                                              \
    int main(int argc, char** argv)                           \
    {                                                         \
        IApp::argc = argc;                                    \
        IApp::argv = (const char**)argv;                      \
        static appClass app = {};                             \
        return DurangoMain(argc, argv, &app);                 \
    }
#elif defined(_WINDOWS)
FORGE_API extern int WindowsMain(int argc, char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) WindowsMain(argc, argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                     \
    extern int WindowsMain(int argc, char** argv, IApp* app); \
                                                              \
    int main(int argc, char** argv)                           \
    {                                                         \
        IApp::argc = argc;                                    \
        IApp::argv = (const char**)argv;                      \
        static appClass app = {};                             \
        return WindowsMain(argc, argv, &app);                 \
    }
#elif defined(TARGET_IOS)
FORGE_API extern int iOSMain(int argc, char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) iOSMain(argc, argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                 \
    extern int iOSMain(int argc, char** argv, IApp* app); \
                                                          \
    int main(int argc, char** argv)                       \
    {                                                     \
        IApp::argc = argc;                                \
        IApp::argv = (const char**)argv;                  \
        static appClass app = {};                         \
        return iOSMain(argc, argv, &app);                 \
    }
#elif defined(__APPLE__)
FORGE_API extern int macOSMain(int argc, const char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) macOSMain(argc, (const char**)argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                         \
    extern int macOSMain(int argc, const char** argv, IApp* app); \
                                                                  \
    int main(int argc, const char* argv[])                        \
    {                                                             \
        IApp::argc = argc;                                        \
        IApp::argv = argv;                                        \
        static appClass app = {};                                 \
        return macOSMain(argc, argv, &app);                       \
    }
#elif defined(__ANDROID__)
FORGE_API extern int AndroidMain(void* param, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) AndroidMain(customPtr, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)           \
    extern int AndroidMain(void* param, IApp* app); \
                                                    \
    void android_main(struct android_app* param)    \
    {                                               \
        IApp::argc = 0;                             \
        IApp::argv = NULL;                          \
        static appClass app = {};                   \
        AndroidMain(param, &app);                   \
    }

#elif defined(__linux__)
FORGE_API extern int LinuxMain(int argc, char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) LinuxMain(argc, argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                   \
    extern int LinuxMain(int argc, char** argv, IApp* app); \
                                                            \
    int main(int argc, char** argv)                         \
    {                                                       \
        IApp::argc = argc;                                  \
        IApp::argv = (const char**)argv;                    \
        static appClass app = {};                           \
        return LinuxMain(argc, argv, &app);                 \
    }
#elif defined(NX64)
FORGE_API extern void NxMain(IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) NxMain(&(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass) \
    extern void NxMain(IApp* app);        \
                                          \
    extern "C" void nnMain()              \
    {                                     \
        static appClass app = {};         \
        NxMain(&app);                     \
    }
#elif defined(ORBIS)
FORGE_API extern int OrbisMain(int argc, char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) OrbisMain(argc, argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                   \
    extern int OrbisMain(int argc, char** argv, IApp* app); \
                                                            \
    int main(int argc, char** argv)                         \
    {                                                       \
        static appClass app = {};                           \
        return OrbisMain(argc, argv, &app);                 \
    }
#elif defined(PROSPERO)
FORGE_API extern int ProsperoMain(int argc, char** argv, IApp* app);
#define RUN_APPLICATION_MAIN(argc, argv, appInstance, customPtr) ProsperoMain(argc, argv, &(appInstance))

#define DEFINE_APPLICATION_MAIN(appClass)                      \
    extern int ProsperoMain(int argc, char** argv, IApp* app); \
                                                               \
    int main(int argc, char** argv)                            \
    {                                                          \
        static appClass app = {};                              \
        return ProsperoMain(argc, argv, &app);                 \
    }
#else
#endif

#endif // _IAPP_H_
