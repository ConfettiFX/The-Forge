
// ================================================================================================
// -*- C++ -*-
// File: vectormath/vectormath.hpp
// Author: Guilherme R. Lampert
// Created on: 30/12/16
// Brief: This header exposes the Sony Vectormath library types and functions into the global scope.
// ================================================================================================

#ifndef VECTORMATH_HPP
#define VECTORMATH_HPP

#include "../../../../Application/Config.h"
#include "vectormath_settings.hpp"

// Sony's library includes:
#if VECTORMATH_MODE_SCE
#if defined(ORBIS)
#include "../../../../../PS4/Common_3/Utilities/ThirdParty/OpenSource/vectormath/cpp/vectormath_aos.h"
#include "../../../../../PS4/Common_3/Utilities/ThirdParty/OpenSource/vectormath/cpp/vectormath_namespace.h"
#elif defined(PROSPERO)
#include "../../../../../Prospero/Common_3/Utilities/ThirdParty/OpenSource/vectormath/cpp/vectormath_aos.h"
#include "../../../../../Prospero/Common_3/Utilities/ThirdParty/OpenSource/vectormath/cpp/vectormath_namespace.h"
#endif
#elif (VECTORMATH_CPU_HAS_SSE1_OR_BETTER && !VECTORMATH_FORCE_SCALAR_MODE) // SSE
    #include "sse/vectormath.hpp"
    using namespace Vectormath::SSE;
#elif (VECTORMATH_CPU_HAS_NEON && !VECTORMATH_FORCE_SCALAR_MODE) // NEON
	#include "neon/vectormath.hpp"
	using namespace Vectormath::Neon;
#else // !SSE
    #include "scalar/vectormath.hpp"
    using namespace Vectormath::Scalar;
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
