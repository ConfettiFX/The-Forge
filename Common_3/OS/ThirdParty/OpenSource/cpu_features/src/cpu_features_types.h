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

#include <stdint.h>

typedef struct {
	uint32_t fp : 1;          // Floating-point.
	uint32_t asimd : 1;       // Advanced SIMD.
	uint32_t evtstrm : 1;     // Generic timer generated events.
	uint32_t aes : 1;         // Hardware-accelerated Advanced Encryption Standard.
	uint32_t pmull : 1;       // Polynomial multiply long.
	uint32_t sha1 : 1;        // Hardware-accelerated SHA1.
	uint32_t sha2 : 1;        // Hardware-accelerated SHA2-256.
	uint32_t crc32 : 1;       // Hardware-accelerated CRC-32.
	uint32_t atomics : 1;     // Armv8.1 atomic instructions.
	uint32_t fphp : 1;        // Half-precision floating point support.
	uint32_t asimdhp : 1;     // Advanced SIMD half-precision support.
	uint32_t cpuid : 1;       // Access to certain ID registers.
	uint32_t asimdrdm : 1;    // Rounding Double Multiply Accumulate/Subtract.
	uint32_t jscvt : 1;       // Support for JavaScript conversion.
	uint32_t fcma : 1;        // Floating point complex numbers.
	uint32_t lrcpc : 1;       // Support for weaker release consistency.
	uint32_t dcpop : 1;       // Data persistence writeback.
	uint32_t sha3 : 1;        // Hardware-accelerated SHA3.
	uint32_t sm3 : 1;         // Hardware-accelerated SM3.
	uint32_t sm4 : 1;         // Hardware-accelerated SM4.
	uint32_t asimddp : 1;     // Dot product instruction.
	uint32_t sha512 : 1;      // Hardware-accelerated SHA512.
	uint32_t sve : 1;         // Scalable Vector Extension.
	uint32_t asimdfhm : 1;    // Additional half-precision instructions.
	uint32_t dit : 1;         // Data independent timing.
	uint32_t uscat : 1;       // Unaligned atomics support.
	uint32_t ilrcpc : 1;      // Additional support for weaker release consistency.
	uint32_t flagm : 1;       // Flag manipulation instructions.
	uint32_t ssbs : 1;        // Speculative Store Bypass Safe PSTATE bit.
	uint32_t sb : 1;          // Speculation barrier.
	uint32_t paca : 1;        // Address authentication.
	uint32_t pacg : 1;        // Generic authentication.
	uint32_t dcpodp : 1;      // Data cache clean to point of persistence.
	uint32_t sve2 : 1;        // Scalable Vector Extension (version 2).
	uint32_t sveaes : 1;      // SVE AES instructions.
	uint32_t svepmull : 1;    // SVE polynomial multiply long instructions.
	uint32_t svebitperm : 1;  // SVE bit permute instructions.
	uint32_t svesha3 : 1;     // SVE SHA3 instructions.
	uint32_t svesm4 : 1;      // SVE SM4 instructions.
	uint32_t flagm2 : 1;      // Additional flag manipulation instructions.
	uint32_t frint : 1;       // Floating point to integer rounding.
	uint32_t svei8mm : 1;     // SVE Int8 matrix multiplication instructions.
	uint32_t svef32mm : 1;    // SVE FP32 matrix multiplication instruction.
	uint32_t svef64mm : 1;    // SVE FP64 matrix multiplication instructions.
	uint32_t svebf16 : 1;     // SVE BFloat16 instructions.
	uint32_t i8mm : 1;        // Int8 matrix multiplication instructions.
	uint32_t bf16 : 1;        // BFloat16 instructions.
	uint32_t dgh : 1;         // Data Gathering Hint instruction.
	uint32_t rng : 1;         // True random number generator support.
	uint32_t bti : 1;         // Branch target identification.
	uint32_t mte : 1;         // Memory tagging extension.

	// Make sure to update Aarch64FeaturesEnum below if you add a field here.
} Aarch64Features;

typedef struct {
	uint32_t fpu : 1;
	uint32_t tsc : 1;
	uint32_t cx8 : 1;
	uint32_t clfsh : 1;
	uint32_t mmx : 1;
	uint32_t aes : 1;
	uint32_t erms : 1;
	uint32_t f16c : 1;
	uint32_t fma4 : 1;
	uint32_t fma3 : 1;
	uint32_t vaes : 1;
	uint32_t vpclmulqdq : 1;
	uint32_t bmi1 : 1;
	uint32_t hle : 1;
	uint32_t bmi2 : 1;
	uint32_t rtm : 1;
	uint32_t rdseed : 1;
	uint32_t clflushopt : 1;
	uint32_t clwb : 1;

	uint32_t sse : 1;
	uint32_t sse2 : 1;
	uint32_t sse3 : 1;
	uint32_t ssse3 : 1;
	uint32_t sse4_1 : 1;
	uint32_t sse4_2 : 1;
	uint32_t sse4a : 1;

	uint32_t avx : 1;
	uint32_t avx2 : 1;

	uint32_t avx512f : 1;
	uint32_t avx512cd : 1;
	uint32_t avx512er : 1;
	uint32_t avx512pf : 1;
	uint32_t avx512bw : 1;
	uint32_t avx512dq : 1;
	uint32_t avx512vl : 1;
	uint32_t avx512ifma : 1;
	uint32_t avx512vbmi : 1;
	uint32_t avx512vbmi2 : 1;
	uint32_t avx512vnni : 1;
	uint32_t avx512bitalg : 1;
	uint32_t avx512vpopcntdq : 1;
	uint32_t avx512_4vnniw : 1;
	uint32_t avx512_4vbmi2 : 1;
	uint32_t avx512_second_fma : 1;
	uint32_t avx512_4fmaps : 1;
	uint32_t avx512_bf16 : 1;
	uint32_t avx512_vp2intersect : 1;
	uint32_t amx_bf16 : 1;
	uint32_t amx_tile : 1;
	uint32_t amx_int8 : 1;

	uint32_t pclmulqdq : 1;
	uint32_t smx : 1;
	uint32_t sgx : 1;
	uint32_t cx16 : 1;  // aka. CMPXCHG16B
	uint32_t sha : 1;
	uint32_t popcnt : 1;
	uint32_t movbe : 1;
	uint32_t rdrnd : 1;

	uint32_t dca : 1;
	uint32_t ss : 1;
	uint32_t adx : 1;
	// Make sure to update X86FeaturesEnum below if you add a field here.
} X86Features;

typedef enum {
	X86_UNKNOWN,
	INTEL_80486,       // 80486
	INTEL_P5,          // P5
	INTEL_LAKEMONT,    // LAKEMONT
	INTEL_CORE,        // CORE
	INTEL_PNR,         // PENRYN
	INTEL_NHM,         // NEHALEM
	INTEL_ATOM_BNL,    // BONNELL
	INTEL_WSM,         // WESTMERE
	INTEL_SNB,         // SANDYBRIDGE
	INTEL_IVB,         // IVYBRIDGE
	INTEL_ATOM_SMT,    // SILVERMONT
	INTEL_HSW,         // HASWELL
	INTEL_BDW,         // BROADWELL
	INTEL_SKL,         // SKYLAKE
	INTEL_ATOM_GMT,    // GOLDMONT
	INTEL_KBL,         // KABY LAKE
	INTEL_CFL,         // COFFEE LAKE
	INTEL_WHL,         // WHISKEY LAKE
	INTEL_CNL,         // CANNON LAKE
	INTEL_ICL,         // ICE LAKE
	INTEL_TGL,         // TIGER LAKE
	INTEL_SPR,         // SAPPHIRE RAPIDS
	INTEL_ADL,         // ALDER LAKE
	INTEL_RCL,         // ROCKET LAKE
	INTEL_KNIGHTS_M,   // KNIGHTS MILL
	INTEL_KNIGHTS_L,   // KNIGHTS LANDING
	INTEL_KNIGHTS_F,   // KNIGHTS FERRY
	INTEL_KNIGHTS_C,   // KNIGHTS CORNER
	INTEL_NETBURST,    // NETBURST
	AMD_HAMMER,        // K8  HAMMER
	AMD_K10,           // K10
	AMD_K11,           // K11
	AMD_K12,           // K12
	AMD_BOBCAT,        // K14 BOBCAT
	AMD_PILEDRIVER,    // K15 PILEDRIVER
	AMD_STREAMROLLER,  // K15 STREAMROLLER
	AMD_EXCAVATOR,     // K15 EXCAVATOR
	AMD_BULLDOZER,     // K15 BULLDOZER
	AMD_JAGUAR,        // K16 JAGUAR
	AMD_PUMA,          // K16 PUMA
	AMD_ZEN,           // K17 ZEN
	AMD_ZEN_PLUS,      // K17 ZEN+
	AMD_ZEN2,          // K17 ZEN 2
	AMD_ZEN3,          // K19 ZEN 3
	X86_MICROARCHITECTURE_LAST_,
} X86Microarchitecture;
