#pragma once
// Copyright 2017 Google LLC
// Copyright 2020 Intel Corporation
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

#include "cpu_features_cache_info.h"
#include "cpu_features_macros.h"

#include "cpu_features_types.h"

CPU_FEATURES_START_CPP_NAMESPACE

// CPUID Vendors
#define CPU_FEATURES_VENDOR_GENUINE_INTEL "GenuineIntel"
#define CPU_FEATURES_VENDOR_AUTHENTIC_AMD "AuthenticAMD"
#define CPU_FEATURES_VENDOR_HYGON_GENUINE "HygonGenuine"

// See https://en.wikipedia.org/wiki/CPUID for a list of x86 cpu features.
// The field names are based on the short name provided in the wikipedia tables.

typedef struct {
  X86Features features;
  char name[128];
  int family;
  int model;
  int stepping;
  char vendor[13];  // 0 terminated string
} X86Info;

// Calls cpuid puts data into outputCpuInfo and return a result
 bool GetX86Info(X86Info* outInfo);

// Returns cache hierarchy informations.
// Can call cpuid multiple times.
// Only works on Intel CPU at the moment.
CacheInfo GetX86CacheInfo(void);

// Returns the underlying microarchitecture by looking at X86Info's vendor,
// family and model.
X86Microarchitecture GetX86Microarchitecture(const X86Info* info);

// Calls cpuid and fills the brand_string.
// - brand_string *must* be of size 49 (beware of array decaying).
// - brand_string will be zero terminated.
void FillX86BrandString(char brand_string[49]);

////////////////////////////////////////////////////////////////////////////////
// Introspection functions

typedef enum {
  X86_FPU,
  X86_TSC,
  X86_CX8,
  X86_CLFSH,
  X86_MMX,
  X86_AES,
  X86_ERMS,
  X86_F16C,
  X86_FMA4,
  X86_FMA3,
  X86_VAES,
  X86_VPCLMULQDQ,
  X86_BMI1,
  X86_HLE,
  X86_BMI2,
  X86_RTM,
  X86_RDSEED,
  X86_CLFLUSHOPT,
  X86_CLWB,
  X86_SSE,
  X86_SSE2,
  X86_SSE3,
  X86_SSSE3,
  X86_SSE4_1,
  X86_SSE4_2,
  X86_SSE4A,
  X86_AVX,
  X86_AVX2,
  X86_AVX512F,
  X86_AVX512CD,
  X86_AVX512ER,
  X86_AVX512PF,
  X86_AVX512BW,
  X86_AVX512DQ,
  X86_AVX512VL,
  X86_AVX512IFMA,
  X86_AVX512VBMI,
  X86_AVX512VBMI2,
  X86_AVX512VNNI,
  X86_AVX512BITALG,
  X86_AVX512VPOPCNTDQ,
  X86_AVX512_4VNNIW,
  X86_AVX512_4VBMI2,
  X86_AVX512_SECOND_FMA,
  X86_AVX512_4FMAPS,
  X86_AVX512_BF16,
  X86_AVX512_VP2INTERSECT,
  X86_AMX_BF16,
  X86_AMX_TILE,
  X86_AMX_INT8,
  X86_PCLMULQDQ,
  X86_SMX,
  X86_SGX,
  X86_CX16,
  X86_SHA,
  X86_POPCNT,
  X86_MOVBE,
  X86_RDRND,
  X86_DCA,
  X86_SS,
  X86_ADX,
  X86_LAST_,
} X86FeaturesEnum;

int GetX86FeaturesEnumValue(const X86Features* features, X86FeaturesEnum value);

const char* GetX86FeaturesEnumName(X86FeaturesEnum);

const char* GetX86MicroarchitectureName(X86Microarchitecture);

CPU_FEATURES_END_CPP_NAMESPACE
