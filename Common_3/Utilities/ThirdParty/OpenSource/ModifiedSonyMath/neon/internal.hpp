/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VECTORMATH_NEON_INTERNAL_HPP
#define VECTORMATH_NEON_INTERNAL_HPP

namespace Vectormath
{
namespace Neon
{

// ========================================================
// Helper constants
// ========================================================

// Small epsilon value
static const float VECTORMATH_SLERP_TOL = 0.999f;

// Common constants used to evaluate sseSinf/cosf4/tanf4
static const float VECTORMATH_SINCOS_CC0 = -0.0013602249f;
static const float VECTORMATH_SINCOS_CC1 =  0.0416566950f;
static const float VECTORMATH_SINCOS_CC2 = -0.4999990225f;
static const float VECTORMATH_SINCOS_SC0 = -0.0001950727f;
static const float VECTORMATH_SINCOS_SC1 =  0.0083320758f;
static const float VECTORMATH_SINCOS_SC2 = -0.1666665247f;
static const float VECTORMATH_SINCOS_KC1 =  1.57079625129f;
static const float VECTORMATH_SINCOS_KC2 =  7.54978995489e-8f;

// Shorthand functions to get the unit vectors as __m128
static inline __m128 sseUnitVec1000() { return _mm_setr_ps(1.0f, 0.0f, 0.0f, 0.0f); }
static inline __m128 sseUnitVec0100() { return _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f); }
static inline __m128 sseUnitVec0010() { return _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f); }
static inline __m128 sseUnitVec0001() { return _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f); }

// ========================================================
// Internal helper types and functions
// ========================================================

typedef __m128 SSEFloat4V;
typedef __m128 SSEUint4V;
typedef __m128 SSEInt4V;

typedef struct _DSSEVec4
{
	union {
		struct {
			__m128d xy;
			__m128d zw;
		};
		struct {
			__m128i i01;
			__m128i i23;
		};
		double d[4];
		long long l[4];
		float f[8];
		int i[8];
	};
} DSSEVec4;

union DSSEDouble
{
	DSSEVec4 m256;
	double d[4];
};

union SSEFloat
{
	__m128 m128;
	float f[4];
};

union SSEInt
{
	__m128i m128;
	int i[4];
};

union SSEUint
{
	__m128i m128;
	unsigned u[4];
};

// These have to be macros because _MM_SHUFFLE() requires compile-time constants.
#define sseRor(vec, i)       (((i) % 4) ? (_mm_shuffle_ps(vec, vec, _MM_SHUFFLE((unsigned char)(i + 3) % 4, (unsigned char)(i + 2) % 4, (unsigned char)(i + 1) % 4, (unsigned char)(i + 0) % 4))) : (vec))
#define sseSplat(x, e)       _mm_shuffle_ps(x, x, _MM_SHUFFLE(e, e, e, e))
#define sseSld(vec, vec2, x) sseRor(vec, ((x) / 4))

static inline __m128 sseUintToM128(unsigned int x)
{
    union
    {
        unsigned int u;
        float f;
    } tmp;

    tmp.u = x;
    return _mm_set1_ps(tmp.f);
}

//========================================= #TheForgeDoubleUtilitiesBegin =======================================
static inline DSSEVec4 dsseVec4(const __m128d xy, const __m128d zw)
{
	DSSEVec4 result;
	result.xy = xy;
	result.zw = zw;
	return result;
}

static inline DSSEVec4 dsseVec4(const __m128i i01, const __m128i i23)
{
	DSSEVec4 result;
	result.i01 = i01;
	result.i23 = i23;
	return result;
}

static inline DSSEVec4 dsseAnd(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_and_pd(a.xy, b.xy), _mm_and_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseAndNot(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_andnot_pd(a.xy, b.xy), _mm_andnot_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseOr(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_or_pd(a.xy, b.xy), _mm_or_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseXor(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_xor_pd(a.xy, b.xy), _mm_xor_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseAdd(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_add_pd(a.xy, b.xy), _mm_add_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseSub(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_sub_pd(a.xy, b.xy), _mm_sub_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseMul(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_mul_pd(a.xy, b.xy), _mm_mul_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseDiv(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_div_pd(a.xy, b.xy), _mm_div_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseSetZero()
{
	const __m128d zero = _mm_setzero_pd();
	return dsseVec4(zero, zero);
}

static inline DSSEVec4 dsseSet1(double a)
{
	const __m128d scalar = _mm_set1_pd(a);
	return dsseVec4(scalar, scalar);
}

static inline DSSEVec4 dsseSetr(double x, double y, double z, double w)
{
	return dsseVec4(_mm_setr_pd(x, y), _mm_setr_pd(z, w));
}

static inline DSSEVec4 dsseLoadu(const double* _Dp)
{
	return dsseVec4(_mm_loadu_pd(_Dp + 0), _mm_loadu_pd(_Dp + 2));
}

static inline void dsseStoreu(double* _Dp, DSSEVec4 vec)
{
	_mm_storeu_pd(_Dp + 0, vec.xy);
	_mm_storeu_pd(_Dp + 2, vec.zw);
}

static inline DSSEVec4 dsseMoveLH(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(a.xy, b.xy);
}

static inline DSSEVec4 dsseMoveHL(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(b.zw, a.zw);
}

static inline DSSEVec4 dsseMin(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_min_pd(a.xy, b.xy), _mm_min_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseMax(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_max_pd(a.xy, b.xy), _mm_max_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseLt(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_cmpgt_pd(b.xy, a.xy), _mm_cmpgt_pd(b.zw, a.zw));
}

static inline DSSEVec4 dsseLe(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_cmpge_pd(b.xy, a.xy), _mm_cmpge_pd(b.zw, a.zw));
}

static inline DSSEVec4 dsseGt(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_cmpgt_pd(a.xy, b.xy), _mm_cmpgt_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseGe(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_cmpge_pd(a.xy, b.xy), _mm_cmpge_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseEq(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_cmpeq_pd(a.xy, b.xy), _mm_cmpeq_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseNe(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_cmpneq_pd(a.xy, b.xy), _mm_cmpneq_pd(a.zw, b.zw));
}

static inline DSSEVec4 dsseUlongToVec4(unsigned long long x)
{
	union
	{
		unsigned long long u;
		double f;
	} tmp;

	tmp.u = x;
	return dsseSet1(tmp.f);
}

static inline DSSEVec4 dsseCopySign(DSSEVec4 a, DSSEVec4 b)
{
	const DSSEVec4 mask = dsseUlongToVec4(0x7FFFFFFFFFFFFFFF);
	const DSSEVec4 sign = dsseAndNot(mask, b);
	const DSSEVec4 value = dsseAnd(mask, a);
	return dsseOr(value, sign);
}

// Based off BoolInVec(bool scalar)
static inline DSSEVec4 dsseFromBool(bool scalar)
{
	return dsseUlongToVec4(-(long long)scalar);
}

static inline DSSEVec4 dsseFromFVec4(SSEFloat vec)
{
	DSSEVec4 result;
	result.d[0] = (double)vec.f[0];
	result.d[1] = (double)vec.f[1];
	result.d[2] = (double)vec.f[2];
	result.d[3] = (double)vec.f[3];
	return result;
}

static inline DSSEVec4 dsseFromIVec4(SSEInt vec)
{
	DSSEVec4 result;
	result.l[0] = (long long)vec.i[0];
	result.l[1] = (long long)vec.i[1];
	result.l[2] = (long long)vec.i[2];
	result.l[3] = (long long)vec.i[3];
	return result;
}

static inline const __m128 dsseToFVec4(DSSEVec4 vec)
{
	return _mm_setr_ps(
		(float)vec.d[0],
		(float)vec.d[1],
		(float)vec.d[3],
		(float)vec.d[4]
	);
}

static inline const __m128i dsseToIVec4(DSSEVec4 vec)
{
	return _mm_setr_epi32(
		(int)vec.l[0],
		(int)vec.l[1],
		(int)vec.l[2],
		(int)vec.l[3]
	);
}

static inline double dsseSelect4(DSSEVec4 src, uint8_t control)
{
	double result = 0.0;
	switch (control)
	{
	case 0:
		result = src.d[0];
		break;
	case 1:
		result = src.d[1];
		break;
	case 2:
		result = src.d[2];
		break;
	case 3:
		result = src.d[3];
		break;
	}
	return result;
}

static inline DSSEVec4 dsseShuffle(DSSEVec4 a, DSSEVec4 b, uint8_t flags)
{
	const uint8_t flagX = (flags & 0b00000011) >> 0;
	const uint8_t flagY = (flags & 0b00001100) >> 2;
	const uint8_t flagZ = (flags & 0b00110000) >> 4;
	const uint8_t flagW = (flags & 0b11000000) >> 6;
	return dsseSetr(dsseSelect4(a, flagX), dsseSelect4(a, flagY), dsseSelect4(b, flagZ), dsseSelect4(b, flagW));
}

#define dsseRor(vec, i)       (((i) % 4) ? (dsseShuffle(vec, vec, _MM_SHUFFLE((unsigned char)(i + 3) % 4, (unsigned char)(i + 2) % 4, (unsigned char)(i + 1) % 4, (unsigned char)(i + 0) % 4))) : (vec))
#define dsseSplat(x, e) dsseShuffle(x, x, _MM_SHUFFLE(e, e, e, e))
#define dsseSld(vec, vec2, x) dsseRor(vec, ((x) / 4))

// Shorthand functions to get the unit vectors as DSSEVec4
static inline DSSEVec4 dsseUnitVec1000() { return dsseSetr(1.0, 0.0, 0.0, 0.0); }
static inline DSSEVec4 dsseUnitVec0100() { return dsseSetr(0.0, 1.0, 0.0, 0.0); }
static inline DSSEVec4 dsseUnitVec0010() { return dsseSetr(0.0, 0.0, 1.0, 0.0); }
static inline DSSEVec4 dsseUnitVec0001() { return dsseSetr(0.0, 0.0, 0.0, 1.0); }
//========================================= #TheForgeDoubleUtilitiesEnd =======================================

static inline __m128 sseMAdd(__m128 a, __m128 b, __m128 c)
{
    return _mm_add_ps(c, _mm_mul_ps(a, b));
}

static inline DSSEVec4 dsseMAdd(DSSEVec4 a, DSSEVec4 b, DSSEVec4 c)
{
	return dsseAdd(c, dsseMul(a, b));
}

static inline __m128 sseMSub(__m128 a, __m128 b, __m128 c)
{
    return _mm_sub_ps(c, _mm_mul_ps(a, b));
}

static inline DSSEVec4 dsseMSub(DSSEVec4 a, DSSEVec4 b, DSSEVec4 c)
{
	return dsseSub(c, dsseMul(a, b));
}

static inline __m128 sseMergeH(__m128 a, __m128 b)
{
    return _mm_unpacklo_ps(a, b);
}

static inline DSSEVec4 dsseMergeH(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_shuffle_pd(a.xy, b.xy, 0b00), _mm_shuffle_pd(a.xy, b.xy, 0b11));
}

static inline __m128 sseMergeL(__m128 a, __m128 b)
{
    return _mm_unpackhi_ps(a, b);
}

static inline DSSEVec4 dsseMergeL(DSSEVec4 a, DSSEVec4 b)
{
	return dsseVec4(_mm_shuffle_pd(a.zw, b.zw, 0b00), _mm_shuffle_pd(a.zw, b.zw, 0b11));
}

static inline __m128 sseSelect(__m128 a, __m128 b, __m128 mask)
{
    return _mm_or_ps(_mm_and_ps(mask, b), _mm_andnot_ps(mask, a));
}

static inline __m128 sseSelect(__m128 a, __m128 b, const unsigned int * mask)
{
    return sseSelect(a, b, _mm_load_ps((const float *)mask));
}

static inline __m128 sseSelect(__m128 a, __m128 b, unsigned int mask)
{
    return sseSelect(a, b, _mm_set1_ps(*(float *)&mask));
}

static inline DSSEVec4 dsseSelect(DSSEVec4 a, DSSEVec4 b, DSSEVec4 mask)
{
	return dsseOr(dsseAnd(mask, b), dsseAndNot(mask, a));
}

static inline DSSEVec4 dsseSelect(DSSEVec4 a, DSSEVec4 b, const unsigned long long * mask)
{
	return dsseSelect(a, b, dsseLoadu((const double *)mask));
}

static inline DSSEVec4 dsseSelect(DSSEVec4 a, DSSEVec4 b, unsigned long long mask)
{
	return dsseSelect(a, b, dsseSet1(*(double *)&mask));
}

static inline SSEInt4V sseCvtToSignedInts(SSEFloat4V x)
{
    // Only 2^0 supported
    __m128i result = _mm_cvtps_epi32(x);
    return (__m128 &)result;
}

static inline DSSEVec4 dsseCvtToSignedLongs(DSSEVec4 n)
{
	n.l[0] = _mm_cvtsd_si64(n.xy);
	n.l[1] = _mm_cvtsd_si64(_mm_shuffle_pd(n.xy, n.xy, 0b11));
	n.l[2] = _mm_cvtsd_si64(n.zw);
	n.l[3] = _mm_cvtsd_si64(_mm_shuffle_pd(n.zw, n.zw, 0b11));
	return n;
}

static inline SSEFloat4V sseCvtToFloats(SSEInt4V x)
{
    // Only 2^0 supported
    return _mm_cvtepi32_ps((__m128i &)x);
}

static inline DSSEVec4 dsseCvtToDoubles(DSSEVec4 n)
{
	return dsseVec4(
		_mm_shuffle_pd(_mm_cvtsi64_sd(n.xy, n.l[0]), _mm_cvtsi64_sd(n.xy, n.l[1]), 0),
		_mm_shuffle_pd(_mm_cvtsi64_sd(n.zw, n.l[2]), _mm_cvtsi64_sd(n.zw, n.l[3]), 0)
	);
}

static inline __m128 sseSqrtf(__m128 x)
{
    return _mm_sqrt_ps(x);
}

static inline DSSEVec4 dsseSqrtf(DSSEVec4 x)
{
	return dsseVec4(_mm_sqrt_pd(x.xy), _mm_sqrt_pd(x.zw));
}

static inline __m128 sseRSqrtf(__m128 x)
{
    return _mm_rsqrt_ps(x);
}

static inline DSSEVec4 dsseRSqrtf(DSSEVec4 x)
{
	return dsseDiv(dsseSet1(1.0), dsseSqrtf(x));
}

static inline __m128 sseRecipf(__m128 x)
{
    return _mm_rcp_ps(x);
}

static inline DSSEVec4 dsseRecipf(DSSEVec4 x)
{
	return dsseDiv(dsseSet1(1.0), x);
}

static inline __m128 sseNegatef(__m128 x)
{
    return _mm_sub_ps(_mm_setzero_ps(), x);
}

static inline DSSEVec4 dsseNegatef(DSSEVec4 x)
{
	return dsseSub(dsseSetZero(), x);
}

static inline __m128 sseFabsf(__m128 x)
{
    return _mm_and_ps(x, sseUintToM128(0x7FFFFFFF));
}

static inline DSSEVec4 dsseFabsf(DSSEVec4 x)
{
	return dsseAnd(x, dsseUlongToVec4(0x7FFFFFFFFFFFFFFF));
}

static inline __m128 sseNewtonrapsonRSqrtf(__m128 x)
{
    const __m128 halfs  = _mm_setr_ps(0.5f, 0.5f, 0.5f, 0.5f);
    const __m128 threes = _mm_setr_ps(3.0f, 3.0f, 3.0f, 3.0f);
    const __m128 approx = _mm_rsqrt_ps(x);
    const __m128 muls   = _mm_mul_ps(_mm_mul_ps(x, approx), approx);
    return _mm_mul_ps(_mm_mul_ps(halfs, approx), _mm_sub_ps(threes, muls));
}

static inline DSSEVec4 dsseNewtonrapsonRSqrtf(DSSEVec4 x)
{
	const DSSEVec4 halfs  = dsseSet1(0.5);
	const DSSEVec4 threes = dsseSet1(3.0);
	const DSSEVec4 approx = dsseRSqrtf(x);
	const DSSEVec4 muls = dsseMul(dsseMul(x, approx), approx);
	return dsseMul(dsseMul(halfs, approx), dsseSub(threes, muls));
}

static inline __m128 sseACosf(__m128 x)
{
    const __m128 xabs = sseFabsf(x);
    const __m128 select = _mm_cmplt_ps(x, _mm_setzero_ps());
    const __m128 t1 = sseSqrtf(_mm_sub_ps(_mm_set1_ps(1.0f), xabs));

    /* Instruction counts can be reduced if the polynomial was
     * computed entirely from nested (dependent) fma's. However,
     * to reduce the number of pipeline stalls, the polygon is evaluated
     * in two halves (hi and lo).
     */
    const __m128 xabs2 = _mm_mul_ps(xabs, xabs);
    const __m128 xabs4 = _mm_mul_ps(xabs2, xabs2);

    const __m128 hi = sseMAdd(sseMAdd(sseMAdd(_mm_set1_ps(-0.0012624911f),
		xabs, _mm_set1_ps(0.0066700901f)),
        xabs, _mm_set1_ps(-0.0170881256f)),
        xabs, _mm_set1_ps(0.0308918810f));

    const __m128 lo = sseMAdd(sseMAdd(sseMAdd(_mm_set1_ps(-0.0501743046f),
		xabs, _mm_set1_ps(0.0889789874f)),
		xabs, _mm_set1_ps(-0.2145988016f)),
		xabs, _mm_set1_ps(1.5707963050f));

    const __m128 result = sseMAdd(hi, xabs4, lo);

    // Adjust the result if x is negative.
    return sseSelect(_mm_mul_ps(t1, result),               	// Positive
		sseMSub(t1, result, _mm_set1_ps(3.1415926535898f)), // Negative
		select);
}

static inline DSSEVec4 dsseACosf(DSSEVec4 x)
{
	const DSSEVec4 xabs = dsseFabsf(x);
	const DSSEVec4 select = dsseLt(x, dsseSetZero());
	const DSSEVec4 t1 = dsseSqrtf(dsseSub(dsseSet1(1.0), xabs));

	/* Instruction counts can be reduced if the polynomial was
	 * computed entirely from nested (dependent) fma's. However,
	 * to reduce the number of pipeline stalls, the polygon is evaluated
	 * in two halves (hi and lo).
	 */
	const DSSEVec4 xabs2 = dsseMul(xabs, xabs);
	const DSSEVec4 xabs4 = dsseMul(xabs2, xabs2);

	const DSSEVec4 hi = dsseMAdd(dsseMAdd(dsseMAdd(dsseSet1(-0.0012624911f),
		xabs, dsseSet1(0.0066700901f)),
		xabs, dsseSet1(-0.0170881256f)),
		xabs, dsseSet1(0.0308918810f));

	const DSSEVec4 lo = dsseMAdd(dsseMAdd(dsseMAdd(dsseSet1(-0.0501743046f),
		xabs, dsseSet1(0.0889789874f)),
		xabs, dsseSet1(-0.2145988016f)),
		xabs, dsseSet1(1.5707963050f));

	const DSSEVec4 result = dsseMAdd(hi, xabs4, lo);

	// Adjust the result if x is negative.
	return dsseSelect(dsseMul(t1, result),                // Positive
		dsseMSub(t1, result, dsseSet1(3.1415926535898f)), // Negative
		select);
}

static inline __m128 sseSinf(SSEFloat4V x)
{
    SSEFloat4V xl, xl2, xl3, res;

    // Range reduction using : xl = angle * TwoOverPi;
    //
    xl = _mm_mul_ps(x, _mm_set1_ps(0.63661977236f));

    // Find the quadrant the angle falls in
    // using:  q = (int) (ceil(abs(xl))*sign(xl))
    //
    SSEInt4V q = sseCvtToSignedInts(xl);

    // Compute an offset based on the quadrant that the angle falls in
    //
    SSEInt4V offset = _mm_and_ps(q, sseUintToM128(0x3));

    // Remainder in range [-pi/4..pi/4]
    //
    SSEFloat4V qf = sseCvtToFloats(q);
    xl = sseMSub(qf, _mm_set1_ps(VECTORMATH_SINCOS_KC2), sseMSub(qf, _mm_set1_ps(VECTORMATH_SINCOS_KC1), x));

    // Compute x^2 and x^3
    //
    xl2 = _mm_mul_ps(xl, xl);
    xl3 = _mm_mul_ps(xl2, xl);

    // Compute both the sin and cos of the angles
    // using a polynomial expression:
    //   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
    //   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
    //
    const SSEFloat4V cx =
        sseMAdd(
        	sseMAdd(
        		sseMAdd(_mm_set1_ps(VECTORMATH_SINCOS_CC0), xl2, _mm_set1_ps(VECTORMATH_SINCOS_CC1)), xl2, _mm_set1_ps(VECTORMATH_SINCOS_CC2)),
        	xl2, _mm_set1_ps(1.0f));
    const SSEFloat4V sx =
        sseMAdd(
        	sseMAdd(
        		sseMAdd(_mm_set1_ps(VECTORMATH_SINCOS_SC0), xl2, _mm_set1_ps(VECTORMATH_SINCOS_SC1)), xl2, _mm_set1_ps(VECTORMATH_SINCOS_SC2)),
        	xl3, xl);

    // Use the cosine when the offset is odd and the sin
    // when the offset is even
    //
#if defined( XBOX) || defined(ANDROID)//_mm_cmpeq_ps returns inacurate results on android 

	// _mm_cmpeq_ps returns inacurate results on XBOX. Casting to __m128i and then using _mm_cmpeq_epi32 fixes the problem.
	res = sseSelect(cx, sx, _mm_castsi128_ps(_mm_cmpeq_epi32(_mm_castps_si128(_mm_and_ps(offset, sseUintToM128(0x1))), _mm_setzero_si128())));

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	//
	return sseSelect(_mm_xor_ps(sseUintToM128(0x80000000U), res),	// Negative
		res,														// Positive
		_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_castps_si128(_mm_and_ps(offset, sseUintToM128(0x2))), _mm_setzero_si128())));
#else
    res = sseSelect(cx, sx, _mm_cmpeq_ps(_mm_and_ps(offset, sseUintToM128(0x1)), _mm_setzero_ps()));

    // Flip the sign of the result when (offset mod 4) = 1 or 2
    //
    return sseSelect(_mm_xor_ps(sseUintToM128(0x80000000U), res), 	// Negative
		res,                                         				// Positive
		_mm_cmpeq_ps(_mm_and_ps(offset, sseUintToM128(0x2)), _mm_setzero_ps()));
#endif
}

static inline DSSEVec4 dsseSinf(DSSEVec4 x)
{
	DSSEVec4 xl, xl2, xl3, res;

	// Range reduction using : xl = angle * TwoOverPi;
	//
	xl = dsseMul(x, dsseSet1(0.63661977236));

	// Find the quadrant the angle falls in
	// using:  q = (int) (ceil(abs(xl))*sign(xl))
	//
	DSSEVec4 q = dsseCvtToSignedLongs(xl);

	// Compute an offset based on the quadrant that the angle falls in
	//
	DSSEVec4 offset = dsseAnd(q, dsseUlongToVec4(0x3));

	// Remainder in range [-pi/4..pi/4]
	//
	DSSEVec4 qf = dsseCvtToDoubles(q);
	xl = dsseMSub(qf, dsseSet1(VECTORMATH_SINCOS_KC2), dsseMSub(qf, dsseSet1(VECTORMATH_SINCOS_KC1), x));

	// Compute x^2 and x^3
	//
	xl2 = dsseMul(xl, xl);
	xl3 = dsseMul(xl2, xl);

	// Compute both the sin and cos of the angles
	// using a polynomial expression:
	//   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	//
	const DSSEVec4 cx =
		dsseMAdd(
			dsseMAdd(
				dsseMAdd(dsseSet1(VECTORMATH_SINCOS_CC0), xl2, dsseSet1(VECTORMATH_SINCOS_CC1)), xl2, dsseSet1(VECTORMATH_SINCOS_CC2)),
			xl2, dsseSet1(1.0));
	const DSSEVec4 sx =
		dsseMAdd(
			dsseMAdd(
				dsseMAdd(dsseSet1(VECTORMATH_SINCOS_SC0), xl2, dsseSet1(VECTORMATH_SINCOS_SC1)), xl2, dsseSet1(VECTORMATH_SINCOS_SC2)),
			xl3, xl);

	// Use the cosine when the offset is odd and the sin
	// when the offset is even
	res = dsseSelect(cx, sx, dsseEq(dsseAnd(offset, dsseUlongToVec4(0x1)), dsseSetZero()));

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	//
	return dsseSelect(dsseXor(dsseUlongToVec4(0x8000000000000000), res), 	// Negative
		res,                                        					 	// Positive
		dsseEq(dsseAnd(offset, dsseUlongToVec4(0x2)), dsseSetZero()));
}

static inline void sseSinfCosf(SSEFloat4V x, SSEFloat4V * s, SSEFloat4V * c)
{
    SSEFloat4V xl, xl2, xl3;
    SSEInt4V offsetSin, offsetCos;

    // Range reduction using : xl = angle * TwoOverPi;
    //
    xl = _mm_mul_ps(x, _mm_set1_ps(0.63661977236f));

    // Find the quadrant the angle falls in
    // using:  q = (int) (ceil(abs(xl))*sign(xl))
    //
    SSEInt4V q = sseCvtToSignedInts(xl);

    // Compute the offset based on the quadrant that the angle falls in.
    // Add 1 to the offset for the cosine.
    //
    offsetSin = _mm_and_ps(q, sseUintToM128((int)0x3));
    __m128i temp = _mm_add_epi32(_mm_set1_epi32(1), (__m128i &)offsetSin);
    offsetCos = (__m128 &)temp;

    // Remainder in range [-pi/4..pi/4]
    //
    SSEFloat4V qf = sseCvtToFloats(q);
    xl = sseMSub(qf, _mm_set1_ps(VECTORMATH_SINCOS_KC2), sseMSub(qf, _mm_set1_ps(VECTORMATH_SINCOS_KC1), x));

    // Compute x^2 and x^3
    //
    xl2 = _mm_mul_ps(xl, xl);
    xl3 = _mm_mul_ps(xl2, xl);

    // Compute both the sin and cos of the angles
    // using a polynomial expression:
    //   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
    //   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
    //
    const SSEFloat4V cx =
        sseMAdd(
        	sseMAdd(
        		sseMAdd(_mm_set1_ps(VECTORMATH_SINCOS_CC0), xl2, _mm_set1_ps(VECTORMATH_SINCOS_CC1)), xl2, _mm_set1_ps(VECTORMATH_SINCOS_CC2)),
        	xl2, _mm_set1_ps(1.0f));
    const SSEFloat4V sx =
        sseMAdd(
        	sseMAdd(
        		sseMAdd(_mm_set1_ps(VECTORMATH_SINCOS_SC0), xl2, _mm_set1_ps(VECTORMATH_SINCOS_SC1)), xl2, _mm_set1_ps(VECTORMATH_SINCOS_SC2)),
        	xl3, xl);

    // Use the cosine when the offset is odd and the sin
    // when the offset is even
    //
#if defined(XBOX) || defined(ANDROID) //_mm_cmpeq_ps returns inacurate results on android 
	// _mm_cmpeq_ps returns inacurate results on XBOX. Casting to __m128i and then using _mm_cmpeq_epi32 fixes the problem.
	__m128 x1 = sseUintToM128(0x1);
	SSEUint4V sinMask = (SSEUint4V)_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_castps_si128(_mm_and_ps(offsetSin, x1)), _mm_setzero_si128()));
	SSEUint4V cosMask = (SSEUint4V)_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_castps_si128(_mm_and_ps(offsetCos, x1)), _mm_setzero_si128()));
#else
    SSEUint4V sinMask = (SSEUint4V)_mm_cmpeq_ps(_mm_and_ps(offsetSin, sseUintToM128(0x1)), _mm_setzero_ps());
    SSEUint4V cosMask = (SSEUint4V)_mm_cmpeq_ps(_mm_and_ps(offsetCos, sseUintToM128(0x1)), _mm_setzero_ps());
#endif
    *s = sseSelect(cx, sx, sinMask);
    *c = sseSelect(cx, sx, cosMask);

    // Flip the sign of the result when (offset mod 4) = 1 or 2
    //
#if defined( XBOX) || defined(ANDROID)//_mm_cmpeq_ps returns inacurate results on android 
	__m128 x2 = sseUintToM128(0x2);
	sinMask = (SSEUint4V)_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_castps_si128(_mm_and_ps(offsetSin, x2)), _mm_setzero_si128()));
	cosMask = (SSEUint4V)_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_castps_si128(_mm_and_ps(offsetCos, x2)), _mm_setzero_si128()));
#else
    sinMask = _mm_cmpeq_ps(_mm_and_ps(offsetSin, sseUintToM128(0x2)), _mm_setzero_ps());
    cosMask = _mm_cmpeq_ps(_mm_and_ps(offsetCos, sseUintToM128(0x2)), _mm_setzero_ps());
#endif
    *s = sseSelect((SSEFloat4V)_mm_xor_ps(sseUintToM128(0x80000000), (SSEUint4V)*s), *s, sinMask);
    *c = sseSelect((SSEFloat4V)_mm_xor_ps(sseUintToM128(0x80000000), (SSEUint4V)*c), *c, cosMask);
}

static inline void dsseSinfCosf(DSSEVec4 x, DSSEVec4 * s, DSSEVec4 * c)
{
	DSSEVec4 xl, xl2, xl3;
	DSSEVec4 offsetSin, offsetCos;

	// Range reduction using : xl = angle * TwoOverPi;
	//
	xl = dsseMul(x, dsseSet1(0.63661977236));

	// Find the quadrant the angle falls in
	// using:  q = (int) (ceil(abs(xl))*sign(xl))
	//
	DSSEVec4 q = dsseCvtToSignedLongs(xl);

	// Compute the offset based on the quadrant that the angle falls in.
	// Add 1 to the offset for the cosine.
	//
	offsetSin = dsseAnd(q, dsseUlongToVec4((long long)0x3));
	offsetCos.i01 = _mm_add_epi64(_mm_set1_epi64x(1), offsetSin.i01);
	offsetCos.i23 = _mm_add_epi64(_mm_set1_epi64x(1), offsetSin.i23);

	// Remainder in range [-pi/4..pi/4]
	//
	DSSEVec4 qf = dsseCvtToDoubles(q);
	xl = dsseMSub(qf, dsseSet1(VECTORMATH_SINCOS_KC2), dsseMSub(qf, dsseSet1(VECTORMATH_SINCOS_KC1), x));

	// Compute x^2 and x^3
	//
	xl2 = dsseMul(xl, xl);
	xl3 = dsseMul(xl2, xl);

	// Compute both the sin and cos of the angles
	// using a polynomial expression:
	//   cx = 1.0f + xl2 * ((C0 * xl2 + C1) * xl2 + C2), and
	//   sx = xl + xl3 * ((S0 * xl2 + S1) * xl2 + S2)
	//
	const DSSEVec4 cx =
		dsseMAdd(
			dsseMAdd(
				dsseMAdd(dsseSet1(VECTORMATH_SINCOS_CC0), xl2, dsseSet1(VECTORMATH_SINCOS_CC1)), xl2, dsseSet1(VECTORMATH_SINCOS_CC2)),
			xl2, dsseSet1(1.0f));
	const DSSEVec4 sx =
		dsseMAdd(
			dsseMAdd(
				dsseMAdd(dsseSet1(VECTORMATH_SINCOS_SC0), xl2, dsseSet1(VECTORMATH_SINCOS_SC1)), xl2, dsseSet1(VECTORMATH_SINCOS_SC2)),
			xl3, xl);

	// Use the cosine when the offset is odd and the sin
	// when the offset is even
	DSSEVec4 sinMask = dsseEq(dsseAnd(offsetSin, dsseUlongToVec4(0x1)), dsseSetZero());
	DSSEVec4 cosMask = dsseEq(dsseAnd(offsetCos, dsseUlongToVec4(0x1)), dsseSetZero());

	*s = dsseSelect(cx, sx, sinMask);
	*c = dsseSelect(cx, sx, cosMask);

	// Flip the sign of the result when (offset mod 4) = 1 or 2
	sinMask = dsseEq(dsseAnd(offsetSin, dsseUlongToVec4(0x2)),dsseSetZero());
	cosMask = dsseEq(dsseAnd(offsetCos, dsseUlongToVec4(0x2)),dsseSetZero());

	*s = dsseSelect(dsseXor(dsseUlongToVec4(0x8000000000000000), *s), *s, sinMask);
	*c = dsseSelect(dsseXor(dsseUlongToVec4(0x8000000000000000), *c), *c, cosMask);
}

static inline __m128 sseVecDot3(__m128 vec0, __m128 vec1)
{
    const __m128 result = _mm_mul_ps(vec0, vec1);
    return _mm_add_ps(sseSplat(result, 0), _mm_add_ps(sseSplat(result, 1), sseSplat(result, 2)));
}

static inline DSSEVec4 dsseVecDot3(DSSEVec4 vec0, DSSEVec4 vec1)
{
	const DSSEVec4 result = dsseMul(vec0, vec1);
	return dsseAdd(dsseSplat(result, 0), dsseAdd(dsseSplat(result, 1), dsseSplat(result, 2)));
}

static inline __m128 sseVecDot4(__m128 vec0, __m128 vec1)
{
    const __m128 result = _mm_mul_ps(vec0, vec1);
    return _mm_add_ps(_mm_shuffle_ps(result, result, _MM_SHUFFLE(0, 0, 0, 0)),
		_mm_add_ps(_mm_shuffle_ps(result, result, _MM_SHUFFLE(1, 1, 1, 1)),
			_mm_add_ps(_mm_shuffle_ps(result, result, _MM_SHUFFLE(2, 2, 2, 2)), _mm_shuffle_ps(result, result, _MM_SHUFFLE(3, 3, 3, 3)))));
}

static inline DSSEVec4 dsseVecDot4(DSSEVec4 vec0, DSSEVec4 vec1)
{
	const DSSEVec4 result = dsseMul(vec0, vec1);
	const DSSEVec4 temp0 = dsseShuffle(result, result, _MM_SHUFFLE(0, 0, 0, 0));
	const DSSEVec4 temp1 = dsseShuffle(result, result, _MM_SHUFFLE(1, 1, 1, 1));
	const DSSEVec4 temp2 = dsseShuffle(result, result, _MM_SHUFFLE(2, 2, 2, 2));
	const DSSEVec4 temp3 = dsseShuffle(result, result, _MM_SHUFFLE(3, 3, 3, 3));
	return dsseAdd(temp0, dsseAdd(temp1, dsseAdd(temp2, temp3)));
}

static inline __m128 sseVecCross(__m128 vec0, __m128 vec1)
{
    __m128 tmp0, tmp1, tmp2, tmp3, result;
    tmp0 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
    tmp1 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
    tmp2 = _mm_shuffle_ps(vec0, vec0, _MM_SHUFFLE(3, 1, 0, 2));
    tmp3 = _mm_shuffle_ps(vec1, vec1, _MM_SHUFFLE(3, 0, 2, 1));
    result = _mm_mul_ps(tmp0, tmp1);
    result = sseMSub(tmp2, tmp3, result);
    return result;
}

static inline DSSEVec4 dsseVecCross(DSSEVec4 vec0, DSSEVec4 vec1)
{
	DSSEVec4 tmp0, tmp1, tmp2, tmp3, result;
	tmp0 = dsseShuffle(vec0, vec0, _MM_SHUFFLE(3, 0, 2, 1));
	tmp1 = dsseShuffle(vec1, vec1, _MM_SHUFFLE(3, 1, 0, 2));
	tmp2 = dsseShuffle(vec0, vec0, _MM_SHUFFLE(3, 1, 0, 2));
	tmp3 = dsseShuffle(vec1, vec1, _MM_SHUFFLE(3, 0, 2, 1));
	result = dsseMul(tmp0, tmp1);
	result = dsseMSub(tmp2, tmp3, result);
	return result;
}

static inline __m128 sseVecInsert(__m128 dst, __m128 src, int slot)
{
    SSEFloat d;
    SSEFloat s;
    d.m128 = dst;
    s.m128 = src;
    d.f[slot] = s.f[slot];
    return d.m128;
}

static inline DSSEVec4 dsseVecInsert(DSSEVec4 dst, DSSEVec4 src, int slot)
{
	DSSEDouble d;
	DSSEDouble s;
	d.m256 = dst;
	s.m256 = src;
	d.d[slot] = s.d[slot];
	return d.m256;
}

static inline void sseVecSetElement(__m128 & vec, float scalar, int slot)
{
    ((float *)&(vec))[slot] = scalar;
}

static inline void dsseVecSetElement(DSSEVec4 & vec, double scalar, int slot)
{
	((double *)&(vec))[slot] = scalar;
}

} // namespace Neon
} // namespace Vectormath

#endif // VECTORMATH_NEON_INTERNAL_HPP
