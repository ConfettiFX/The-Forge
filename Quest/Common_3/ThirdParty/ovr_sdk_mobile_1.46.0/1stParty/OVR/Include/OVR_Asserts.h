/************************************************************************************

Filename    :   OVR_Asserts.h
Content     :   Capture Asserts for Release and Debug builds. Send Telemetry logs in Release builds.
Created     :   July, 2019
Notes       :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.3 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.3

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/
#ifndef OVR_Asserts_h
#define OVR_Asserts_h

//-----------------------------------------------------------------------------------
// ***** OVR_DEBUG_BREAK,
//       OVR_ASSERT,
//
// Macros have effect only in debug builds.
//
// Example OVR_DEBUG_BREAK usage (note the lack of parentheses):
//     #define MY_ASSERT(expression) do { if (!(expression)) { OVR_DEBUG_BREAK; } } while(0)
//
//
// Example OVR_ASSERT usage:
//     OVR_ASSERT(count < 100);
//
//--------------------------------------------------------------------------------------------------

#ifndef OVR_BUILD_DEBUG
#define OVR_DEBUG_BREAK ((void)0)
#define OVR_ASSERT(p) ((void)0)
#else

// Microsoft Win32 specific debugging support
#if defined(OVR_OS_WIN32)
#ifdef OVR_CPU_X86
#if defined(__cplusplus_cli)
#define OVR_DEBUG_BREAK \
    do {                \
        __debugbreak(); \
    } while (0)
#elif defined(OVR_CC_GNU)
#define OVR_DEBUG_BREAK        \
    do {                       \
        OVR_ASM("int $3\n\t"); \
    } while (0)
#else
#define OVR_DEBUG_BREAK \
    do {                \
        OVR_ASM int 3   \
    } while (0)
#endif
#else
#define OVR_DEBUG_BREAK \
    do {                \
        __debugbreak(); \
    } while (0)
#endif
// Android specific debugging support
#elif defined(OVR_OS_ANDROID)
#include <android/log.h>
#define OVR_EXPAND1(s) #s
#define OVR_EXPAND(s) OVR_EXPAND1(s)
#define OVR_DEBUG_BREAK   \
    do {                  \
        __builtin_trap(); \
    } while (0)
#define OVR_ASSERT(p)                                                                            \
    do {                                                                                         \
        if (!(p)) {                                                                              \
            __android_log_write(                                                                 \
                ANDROID_LOG_WARN, "OVR", "ASSERT@ " __FILE__ "(" OVR_EXPAND(__LINE__) "): " #p); \
            OVR_DEBUG_BREAK;                                                                     \
        }                                                                                        \
    } while (0)
// Unix specific debugging support
#elif defined(OVR_CPU_X86) || defined(OVR_CPU_X86_64)
#define OVR_DEBUG_BREAK        \
    do {                       \
        OVR_ASM("int $3\n\t"); \
    } while (0)
#else
#define OVR_DEBUG_BREAK \
    do {                \
        *((int*)0) = 1; \
    } while (0)
#endif

#if !defined(OVR_ASSERT) // Android currently defines its own version of OVR_ASSERT() with logging
// This will cause compiler breakpoint
#define OVR_ASSERT(p)        \
    do {                     \
        if (!(p)) {          \
            OVR_DEBUG_BREAK; \
        }                    \
    } while (0)
#endif

#endif // OVR_DEBUG_BUILD

#endif // OVR_Asserts_h
