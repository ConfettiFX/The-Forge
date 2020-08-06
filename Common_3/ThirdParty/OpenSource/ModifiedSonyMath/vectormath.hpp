
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
#if defined(_MSC_VER) && !defined(NN_NINTENDO_SDK)
    #if (defined(__AVX__) || defined(__AVX2__) || defined(_M_AMD64) || defined(_M_X64) || (_M_IX86_FP == 1) || (_M_IX86_FP == 2))
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 1
    #else // SSE support
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 0
    #endif // SSE support
#else // !_MSC_VER
    #if defined(__SSE__)
        #define VECTORMATH_CPU_HAS_SSE1_OR_BETTER 1
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

// Sony's library includes:
#if VECTORMATH_MODE_SCE
#if defined(ORBIS)
#include "../../../../PS4/Common_3/ThirdParty/OpenSource/vectormath/cpp/vectormath_aos.h"
#include "../../../../PS4/Common_3/ThirdParty/OpenSource/vectormath/cpp/vectormath_namespace.h"
#elif defined(PROSPERO)
#include "../../../../Prospero/Common_3/ThirdParty/OpenSource/vectormath/cpp/vectormath_aos.h"
#include "../../../../Prospero/Common_3/ThirdParty/OpenSource/vectormath/cpp/vectormath_namespace.h"
#endif
#define VECTORMATH_MODE_SCALAR 0
#define VECTORMATH_MODE_SSE    1
#define VECTORMATH_MODE_NEON   0
#elif (VECTORMATH_CPU_HAS_SSE1_OR_BETTER && !VECTORMATH_FORCE_SCALAR_MODE) // SSE
    #include "sse/vectormath.hpp"
    using namespace Vectormath::SSE;
    #define VECTORMATH_MODE_SCALAR 0
    #define VECTORMATH_MODE_SSE    1
	#define VECTORMATH_MODE_NEON   0
#elif (VECTORMATH_CPU_HAS_NEON && !VECTORMATH_FORCE_SCALAR_MODE) // NEON
	#include "neon/vectormath.hpp"
	using namespace Vectormath::Neon;
	#define VECTORMATH_MODE_SCALAR 0
    #define VECTORMATH_MODE_SSE    0
	#define VECTORMATH_MODE_NEON   1
#else // !SSE
    #include "scalar/vectormath.hpp"
    using namespace Vectormath::Scalar;
    #define VECTORMATH_MODE_SCALAR 1
    #define VECTORMATH_MODE_SSE    0
	#define VECTORMATH_MODE_NEON   0
#endif // Vectormath mode selection

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================
#include "soa/soa.hpp"
using namespace Vectormath::Soa;
//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================

#include "vec2d.hpp"  // - Extended 2D vector and point classes; not aligned and always in scalar floats mode.
#include "common.hpp" // - Miscellaneous helper functions.

using namespace Vectormath;

#endif // VECTORMATH_HPP
