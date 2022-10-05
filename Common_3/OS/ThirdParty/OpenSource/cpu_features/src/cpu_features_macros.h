#pragma once
// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "../../../../../Application/Config.h"

#define STACK_LINE_READER_BUFFER_SIZE 1024

////////////////////////////////////////////////////////////////////////////////
// Architectures
////////////////////////////////////////////////////////////////////////////////

#if defined(__pnacl__) || defined(__CLR_VER)
#define CPU_FEATURES_ARCH_VM
#endif

////////////////////////////////////////////////////////////////////////////////
// Os
////////////////////////////////////////////////////////////////////////////////
#if defined(__linux__) && !defined(ORBIS) && !defined(PROSPERO) && !defined(ANDROID)
#define CPU_FEATURES_OS_LINUX
#endif

#if (defined(__apple__) || defined(__APPLE__) || defined(__MACH__))
#if defined(TARGET_OS_OSX)
#define CPU_FEATURES_OS_MACOS
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
// Compilers
////////////////////////////////////////////////////////////////////////////////

#if defined(__clang__)
#define CPU_FEATURES_COMPILER_CLANG
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define CPU_FEATURES_COMPILER_GCC
#endif

#if defined(_MSC_VER)
#define CPU_FEATURES_COMPILER_MSC
#endif

////////////////////////////////////////////////////////////////////////////////
// Cpp
////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
#define CPU_FEATURES_START_CPP_NAMESPACE \
  extern "C" {
#define CPU_FEATURES_END_CPP_NAMESPACE \
  }
#else
#define CPU_FEATURES_START_CPP_NAMESPACE
#define CPU_FEATURES_END_CPP_NAMESPACE
#endif
