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

typedef struct {
	int fp : 1;          // Floating-point.
	int asimd : 1;       // Advanced SIMD.
	int evtstrm : 1;     // Generic timer generated events.
	int aes : 1;         // Hardware-accelerated Advanced Encryption Standard.
	int pmull : 1;       // Polynomial multiply long.
	int sha1 : 1;        // Hardware-accelerated SHA1.
	int sha2 : 1;        // Hardware-accelerated SHA2-256.
	int crc32 : 1;       // Hardware-accelerated CRC-32.
	int atomics : 1;     // Armv8.1 atomic instructions.
	int fphp : 1;        // Half-precision floating point support.
	int asimdhp : 1;     // Advanced SIMD half-precision support.
	int cpuid : 1;       // Access to certain ID registers.
	int asimdrdm : 1;    // Rounding Double Multiply Accumulate/Subtract.
	int jscvt : 1;       // Support for JavaScript conversion.
	int fcma : 1;        // Floating point complex numbers.
	int lrcpc : 1;       // Support for weaker release consistency.
	int dcpop : 1;       // Data persistence writeback.
	int sha3 : 1;        // Hardware-accelerated SHA3.
	int sm3 : 1;         // Hardware-accelerated SM3.
	int sm4 : 1;         // Hardware-accelerated SM4.
	int asimddp : 1;     // Dot product instruction.
	int sha512 : 1;      // Hardware-accelerated SHA512.
	int sve : 1;         // Scalable Vector Extension.
	int asimdfhm : 1;    // Additional half-precision instructions.
	int dit : 1;         // Data independent timing.
	int uscat : 1;       // Unaligned atomics support.
	int ilrcpc : 1;      // Additional support for weaker release consistency.
	int flagm : 1;       // Flag manipulation instructions.
	int ssbs : 1;        // Speculative Store Bypass Safe PSTATE bit.
	int sb : 1;          // Speculation barrier.
	int paca : 1;        // Address authentication.
	int pacg : 1;        // Generic authentication.
	int dcpodp : 1;      // Data cache clean to point of persistence.
	int sve2 : 1;        // Scalable Vector Extension (version 2).
	int sveaes : 1;      // SVE AES instructions.
	int svepmull : 1;    // SVE polynomial multiply long instructions.
	int svebitperm : 1;  // SVE bit permute instructions.
	int svesha3 : 1;     // SVE SHA3 instructions.
	int svesm4 : 1;      // SVE SM4 instructions.
	int flagm2 : 1;      // Additional flag manipulation instructions.
	int frint : 1;       // Floating point to integer rounding.
	int svei8mm : 1;     // SVE Int8 matrix multiplication instructions.
	int svef32mm : 1;    // SVE FP32 matrix multiplication instruction.
	int svef64mm : 1;    // SVE FP64 matrix multiplication instructions.
	int svebf16 : 1;     // SVE BFloat16 instructions.
	int i8mm : 1;        // Int8 matrix multiplication instructions.
	int bf16 : 1;        // BFloat16 instructions.
	int dgh : 1;         // Data Gathering Hint instruction.
	int rng : 1;         // True random number generator support.
	int bti : 1;         // Branch target identification.
	int mte : 1;         // Memory tagging extension.

	// Make sure to update Aarch64FeaturesEnum below if you add a field here.
} Aarch64Features;

typedef struct {
	int fpu : 1;
	int tsc : 1;
	int cx8 : 1;
	int clfsh : 1;
	int mmx : 1;
	int aes : 1;
	int erms : 1;
	int f16c : 1;
	int fma4 : 1;
	int fma3 : 1;
	int vaes : 1;
	int vpclmulqdq : 1;
	int bmi1 : 1;
	int hle : 1;
	int bmi2 : 1;
	int rtm : 1;
	int rdseed : 1;
	int clflushopt : 1;
	int clwb : 1;

	int sse : 1;
	int sse2 : 1;
	int sse3 : 1;
	int ssse3 : 1;
	int sse4_1 : 1;
	int sse4_2 : 1;
	int sse4a : 1;

	int avx : 1;
	int avx2 : 1;

	int avx512f : 1;
	int avx512cd : 1;
	int avx512er : 1;
	int avx512pf : 1;
	int avx512bw : 1;
	int avx512dq : 1;
	int avx512vl : 1;
	int avx512ifma : 1;
	int avx512vbmi : 1;
	int avx512vbmi2 : 1;
	int avx512vnni : 1;
	int avx512bitalg : 1;
	int avx512vpopcntdq : 1;
	int avx512_4vnniw : 1;
	int avx512_4vbmi2 : 1;
	int avx512_second_fma : 1;
	int avx512_4fmaps : 1;
	int avx512_bf16 : 1;
	int avx512_vp2intersect : 1;
	int amx_bf16 : 1;
	int amx_tile : 1;
	int amx_int8 : 1;

	int pclmulqdq : 1;
	int smx : 1;
	int sgx : 1;
	int cx16 : 1;  // aka. CMPXCHG16B
	int sha : 1;
	int popcnt : 1;
	int movbe : 1;
	int rdrnd : 1;

	int dca : 1;
	int ss : 1;
	int adx : 1;
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
