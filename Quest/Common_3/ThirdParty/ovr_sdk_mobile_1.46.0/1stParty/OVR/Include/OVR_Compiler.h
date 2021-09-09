/************************************************************************************

PublicHeader:   OVR_Types.h
Filename    :   OVR_Compiler.h
Content     :   Compiler-specific feature identification and utilities
Created     :   June 19, 2014
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

#ifndef OVR_Compiler_h
#define OVR_Compiler_h

#pragma once

// References
//    https://gcc.gnu.org/projects/cxx0x.html
//    https://gcc.gnu.org/projects/cxx1y.html
//    http://clang.llvm.org/cxx_status.html
//    http://msdn.microsoft.com/en-us/library/hh567368.aspx
//    https://docs.google.com/spreadsheet/pub?key=0AoBblDsbooe4dHZuVTRoSTFBejk5eFBfVk1GWlE5UlE&output=html
//    http://nadeausoftware.com/articles/2012/10/c_c_tip_how_detect_compiler_name_and_version_using_compiler_predefined_macros

//-----------------------------------------------------------------------------------
// ***** Compiler
//
//  The following compilers are defined: (OVR_CC_x)
//
//     MSVC     - Microsoft Visual C/C++
//     INTEL    - Intel C++ for Linux / Windows
//     GNU      - GNU C++
//     ARM      - ARM C/C++

#if defined(__INTEL_COMPILER)
// Intel 4.0                    = 400
// Intel 5.0                    = 500
// Intel 6.0                    = 600
// Intel 8.0                    = 800
// Intel 9.0                    = 900
#define OVR_CC_INTEL __INTEL_COMPILER

#elif defined(_MSC_VER)
// MSVC 5.0                     = 1100
// MSVC 6.0                     = 1200
// MSVC 7.0 (VC2002)            = 1300
// MSVC 7.1 (VC2003)            = 1310
// MSVC 8.0 (VC2005)            = 1400
// MSVC 9.0 (VC2008)            = 1500
// MSVC 10.0 (VC2010)           = 1600
// MSVC 11.0 (VC2012)           = 1700
// MSVC 12.0 (VC2013)           = 1800
// MSVC 14.0 (VC2015)           = 1900
#define OVR_CC_MSVC _MSC_VER

#if _MSC_VER == 0x1600
#if _MSC_FULL_VER < 160040219
#error "Oculus does not support VS2010 without SP1 installed."
#endif
#endif

#elif defined(__GNUC__)
#define OVR_CC_GNU

#if (__cplusplus > 199711L)
#define OVR_CPP11
#else
#undef OVR_CPP11
#endif

#elif defined(__clang__)
#define OVR_CC_CLANG

#elif defined(__CC_ARM)
#define OVR_CC_ARM

#else
#error "Oculus does not support this Compiler"
#endif

//-----------------------------------------------------------------------------------
// ***** OVR_CC_VERSION
//
//    M = major version
//    m = minor version
//    p = patch release
//    b = build number
//
//    Compiler      Format   Example
//    ----------------------------
//    OVR_CC_GNU    Mmm      408 means GCC 4.8
//    OVR_CC_CLANG  Mmm      305 means clang 3.5
//    OVR_CC_MSVC   MMMM     1700 means VS2012
//    OVR_CC_ARM    Mmpbbb   401677 means 4.0, patch 1, build 677
//    OVR_CC_INTEL  MMmm     1210 means 12.10
//    OVR_CC_EDG    Mmm      407 means EDG 4.7
//
#if defined(OVR_CC_GNU)
#define OVR_CC_VERSION ((__GNUC__ * 100) + __GNUC_MINOR__)
#elif defined(OVR_CC_CLANG)
#define OVR_CC_VERSION ((__clang_major__ * 100) + __clang_minor__)
#elif defined(OVR_CC_MSVC)
#define OVR_CC_VERSION _MSC_VER // Question: Should we recognize _MSC_FULL_VER?
#elif defined(OVR_CC_ARM)
#define OVR_CC_VERSION __ARMCC_VERSION
#elif defined(OVR_CC_INTEL)
#if defined(__INTEL_COMPILER)
#define OVR_CC_VERSION __INTEL_COMPILER
#elif defined(__ICL)
#define OVR_CC_VERSION __ICL
#elif defined(__ICC)
#define OVR_CC_VERSION __ICC
#elif defined(__ECC)
#define OVR_CC_VERSION __ECC
#endif
#elif defined(OVR_CC_EDG)
#define OVR_CC_VERSION \
    __EDG_VERSION__ // This is a generic fallback for EDG-based compilers which aren't specified
                    // above (e.g. as OVR_CC_ARM)
#endif

#endif // header include guard
