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
#pragma once

//Support external config file override
#if defined(EXTERNAL_CONFIG_FILEPATH)
	#include EXTERNAL_CONFIG_FILEPATH
#else

#include <stdint.h>

//////////////////////////////////////////////
//// Compiler setup
//////////////////////////////////////////////
#if   INTPTR_MAX == 0x7FFFFFFFFFFFFFFFLL
# define PTR_SIZE 8
#elif INTPTR_MAX == 0x7FFFFFFF
# define PTR_SIZE 4
#else
#error unsupported platform
#endif

#ifdef __cplusplus
#define FORGE_CONSTEXPR constexpr
#define FORGE_EXTERN_C extern "C"
#else
#define FORGE_CONSTEXPR
#define FORGE_EXTERN_C
#endif

#if defined(_MSC_VER) && !defined(__clang__)
	#if !defined(_DEBUG) && !defined(NDEBUG)
		#define NDEBUG
	#endif

	#define UNREF_PARAM(x) (x)
	#define ALIGNAS(x) __declspec( align( x ) ) 
	#define DEFINE_ALIGNED(def, a) __declspec(align(a)) def
	#define FORGE_CALLCONV __cdecl
	
	#include <crtdbg.h>
	#define COMPILE_ASSERT(exp) _STATIC_ASSERT(exp) 

	#include <BaseTsd.h>
	typedef SSIZE_T ssize_t;

	#if defined(_M_X64)
		#define ARCH_X64
		#define ARCH_X86_FAMILY
	#elif defined(_M_IX86)
		#define ARCH_X86
		#define ARCH_X86_FAMILY
	#else
		#error "Unsupported architecture for msvc compiler"
	#endif

#elif defined(__GNUC__) || defined(__clang__)
#include <sys/types.h>
#include <assert.h>

	#ifdef __OPTIMIZE__
		// Some platforms define NDEBUG for Release builds
		#ifndef NDEBUG
		#define NDEBUG
		#endif
	#else
		#define _DEBUG
	#endif

	#ifdef __APPLE__
		#define NOREFS __unsafe_unretained
	#endif

	#define UNREF_PARAM(x) ((void)(x))
	#define ALIGNAS(x)  __attribute__ ((aligned( x )))
	#define DEFINE_ALIGNED(def, a) __attribute__((aligned(a))) def
	#define FORGE_CALLCONV
	
	#ifdef __clang__
	#define COMPILE_ASSERT(exp) _Static_assert(exp, #exp)
	#else
	#define COMPILE_ASSERT(exp) static_assert(exp, #exp)
	#endif

	#if defined(__i386__)
		#define ARCH_X86
		#define ARCH_X86_FAMILY
	#elif defined(__x86_64__)
		#define ARCH_X64
		#define ARCH_X86_FAMILY
	#elif defined(__arm__)
		#define ARCH_ARM
		#define ARCH_ARM_FAMILY
	#elif defined(__aarch64__)
		#define ARCH_ARM64
		#define ARCH_ARM_FAMILY
	#else
		#error "Unsupported architecture for gcc compiler"
	#endif

#else
#error Unknown language dialect
#endif

#ifndef SSIZE_MAX
#if PTR_SIZE == 4
#define SSIZE_MAX INT32_MAX
COMPILE_ASSERT(sizeof(ssize_t) == sizeof(int32_t));
#elif PTR_SIZE == 8
#define SSIZE_MAX INT64_MAX
COMPILE_ASSERT(sizeof(ssize_t) == sizeof(int64_t));
#endif
#endif // !SSIZE_MAX

#if defined(_MSC_VER)
#define FORGE_EXPORT __declspec(dllexport)
#define FORGE_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) // clang & gcc
#define FORGE_EXPORT __attribute__((visibility("default")))
#define FORGE_IMPORT
#endif

//////////////////////////////////////////////
//// Platform setup
//////////////////////////////////////////////
#if defined(_WIN32)

	#ifdef _GAMING_XBOX 
		#define XBOX
		#ifdef _GAMING_XBOX_SCARLETT
			#ifndef SCARLETT
				#define SCARLETT
			#endif
		#endif
	#elif !defined(_WINDOWS)
		#define _WINDOWS
	#endif

	#ifndef VC_EXTRALEAN
	#define VC_EXTRALEAN
	#endif
	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif

	#ifdef _WINDOWS
	// Restrict compilation to Windows 7 APIs
	#define NTDDI_VERSION NTDDI_WIN7
	#define WINVER _WIN32_WINNT_WIN7
	#define _WIN32_WINNT _WIN32_WINNT_WIN7
	#endif

#elif defined(__APPLE__)
	#include <TargetConditionals.h>

	#if defined(ARCH_ARM64)
		#define TARGET_APPLE_ARM64
	#endif

	#if TARGET_OS_IPHONE
		#define TARGET_IOS
	#endif

	#if TARGET_IPHONE_SIMULATOR
		#define TARGET_IOS_SIMULATOR
	#endif

#elif defined(__ANDROID__)
	#define ANDROID
	#define API_EXPORT

#elif defined(__ORBIS__)
	#define ORBIS
#elif defined(__PROSPERO__)
	#define PROSPERO
#endif

#define API_INTERFACE


//////////////////////////////////////////////
//// General options
//////////////////////////////////////////////
#define ENABLE_FORGE_SCRIPTING
#define ENABLE_FORGE_UI
#define ENABLE_FORGE_FONTS
#define ENABLE_FORGE_INPUT
#define ENABLE_ZIP_FILESYSTEM
#define ENABLE_SCREENSHOT
#define ENABLE_PROFILER
#define ENABLE_MESHOPTIMIZER
//Uncomment this to enable empty mounts
//used for absolute paths
//#define ENABLE_FS_EMPTY_MOUNT


#ifdef ENABLE_PROFILER
// Enable this if you want to have the profiler through a web browser, see PROFILE_WEBSERVER_PORT for server location
// TODO: The option doesn't function correctly
//#define ENABLE_PROFILER_WEBSERVER
#endif


// TODO: Obsolete options?
//#define ENABLE_MTUNER
//#define ENABLE_UI_PRECOMPILED_SHADERS
//#define ENABLE_TEXT_PRECOMPILED_SHADERS


//////////////////////////////////////////////
//// Build related options
//////////////////////////////////////////////

#ifndef FORGE_DEBUG
#if defined(DEBUG) || defined(_DEBUG) || defined(AUTOMATED_TESTING)
#define FORGE_DEBUG
#endif
#endif

#define ENABLE_LOGGING
#define DEFAULT_LOG_LEVEL eALL
#define ENABLE_MEMORY_TRACKING
// #define ENABLE_FORGE_STACKTRACE_DUMP

#ifdef AUTOMATED_TESTING
#if defined(NX64) || (defined(_WINDOWS) && defined(_M_X64)) || defined(ORBIS)
#define ENABLE_FORGE_STACKTRACE_DUMP
#endif
#endif


//////////////////////////////////////////////
//// Config validation
//////////////////////////////////////////////
#if !defined(ENABLE_LOGGING) && !defined(DEFAULT_LOG_LEVEL)
#define DEFAULT_LOG_LEVEL eNONE
#endif


#if defined(_DEBUG) && defined(NDEBUG)
#error "_DEBUG and NDEBUG are defined at the same time"
#endif
#endif
