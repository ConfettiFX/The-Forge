
// ================================================================================================
// -*- C++ -*-
// File: vectormath/vectormath_settings.hpp
// Author: Guilherme R. Lampert
// Created on: April 1st, 2021
// Brief: This header exposes the Sony Vectormath library settings.
// ================================================================================================

#ifndef VECTORMATH_SETTINGS_HPP
#define VECTORMATH_SETTINGS_HPP

#if (!defined(VECTORMATH_DEBUG) && (defined(DEBUG) || defined(_DEBUG)))
#define VECTORMATH_DEBUG 1
#endif // DEBUG || _DEBUG

// Detecting the availability of SSE at compile-time is a bit more involving with Visual Studio...
#if defined(_MSC_VER) && !defined(NN_NINTENDO_SDK)
    #if (defined(__AVX__) || defined(__AVX2__) || defined(_M_AMD64) || defined(_M_X64) || (_M_IX86_FP == 1) || (_M_IX86_FP == 2))
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 1
    #else // SSE support
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
    #endif // SSE support
#else // !_MSC_VER
    #if defined(__SSE__)
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 1
    #elif defined(__ANDROID__)
        #if defined(ANDROID_ARM_NEON)
            #define VECTORMATH_CPU_HAS_NEON 1
        #else
            #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
            #define VECTORMATH_CPU_HAS_NEON 0
        #endif
    #elif defined(__arm64) || defined(__aarch64__) || defined(__arm__ )
        #define VECTORMATH_CPU_HAS_NEON 1
    #else // !__SSE__
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
        #define VECTORMATH_CPU_HAS_NEON 0
    #endif // __SSE__
#endif // _MSC_VER

#define VECTORMATH_FORCE_SCALAR_MODE 0

#if defined(ORBIS) || defined(PROSPERO)
    #define VECTORMATH_MODE_SCE 1
#endif

#if defined(__APPLE__) && defined(__arm__)
    #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
    #define VECTORMATH_CPU_HAS_NEON 1
#endif

// Vectormath mode selection
#if VECTORMATH_MODE_SCE
#define VECTORMATH_MODE_SCALAR 0
#define VECTORMATH_MODE_SSE    1
#define VECTORMATH_MODE_NEON   0
#define VECTORMATH_MIN_ALIGN   16
#elif (VECTORMATH_CPU_HAS_SSE1_OR_BETTER && !VECTORMATH_FORCE_SCALAR_MODE) // SSE
#define VECTORMATH_MODE_SCALAR 0
#define VECTORMATH_MODE_SSE    1
#define VECTORMATH_MODE_NEON   0
#define VECTORMATH_MIN_ALIGN   16
#elif (VECTORMATH_CPU_HAS_NEON && !VECTORMATH_FORCE_SCALAR_MODE) // NEON
#define VECTORMATH_MODE_SCALAR 0
#define VECTORMATH_MODE_SSE    0
#define VECTORMATH_MODE_NEON   1
#define VECTORMATH_MIN_ALIGN   16
#else // !SSE
#define VECTORMATH_MODE_SCALAR 1
#define VECTORMATH_MODE_SSE    0
#define VECTORMATH_MODE_NEON   0
#define VECTORMATH_MIN_ALIGN   0
#endif // Vectormath mode selection



#endif // VECTORMATH_SETTINGS_HPP
