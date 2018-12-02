
// ================================================================================================
// -*- C++ -*-
// File: vectormath/vectormath.hpp
// Author: Guilherme R. Lampert
// Created on: 30/12/16
// Brief: This header exposes the Sony Vectormath library types and functions into the global scope.
// ================================================================================================

#ifndef VECTORMATH_HPP
#define VECTORMATH_HPP

#if (!defined(VECTORMATH_DEBUG) && (defined(DEBUG) || defined(_DEBUG)))
    #define VECTORMATH_DEBUG 1
#endif // DEBUG || _DEBUG

// Detecting the availability of SSE at compile-time is a bit more involving with Visual Studio...
#ifdef _MSC_VER
    #if (defined(__AVX__) || defined(__AVX2__) || defined(_M_AMD64) || defined(_M_X64) || (_M_IX86_FP == 1) || (_M_IX86_FP == 2))
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 1
    #else // SSE support
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
    #endif // SSE support
#else // !_MSC_VER
    #if defined(__SSE__)
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 1
    #else // !__SSE__
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
    #endif // __SSE__
#endif // _MSC_VER

// Sony's library includes:
#define VECTORMATH_FORCE_SCALAR_MODE 0

#if defined(TARGET_IOS) || defined(__ANDROID__)
// SSE mat4 implementation doesn't work well (produces incorrect results)
#ifdef VECTORMATH_FORCE_SCALAR_MODE
#undef VECTORMATH_FORCE_SCALAR_MODE
#endif

#define VECTORMATH_FORCE_SCALAR_MODE 1
#endif

#if (VECTORMATH_CPU_HAS_SSE1_OR_BETTER && !VECTORMATH_FORCE_SCALAR_MODE)
    #include "sse/vectormath.hpp"
    using namespace Vectormath::SSE;
    #define VECTORMATH_MODE_SCALAR 0
    #define VECTORMATH_MODE_SSE    1
#else // !SSE
    #include "scalar/vectormath.hpp"
    using namespace Vectormath::Scalar;
    #define VECTORMATH_MODE_SCALAR 1
    #define VECTORMATH_MODE_SSE    0
#endif // Vectormath mode selection

//========================================= #ConfettiMathExtensionsBegin ================================================
//========================================= #ConfettiAnimationMathExtensionsBegin =======================================

#include "soa/soa.hpp"
using namespace Vectormath::Soa;

//========================================= #ConfettiAnimationMathExtensionsEnd =======================================
//========================================= #ConfettiMathExtensionsEnd ================================================

#include "vec2d.hpp"  // - Extended 2D vector and point classes; not aligned and always in scalar floats mode.
#include "common.hpp" // - Miscellaneous helper functions.

using namespace Vectormath;

#endif // VECTORMATH_HPP
