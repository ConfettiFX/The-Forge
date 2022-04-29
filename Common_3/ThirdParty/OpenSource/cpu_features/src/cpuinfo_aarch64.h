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

#include "cpu_features_cache_info.h"
#include "cpu_features_macros.h"

#include "cpu_features_types.h"

#include <stdbool.h>

CPU_FEATURES_START_CPP_NAMESPACE

typedef struct {
  Aarch64Features features;
  char name[128];
  int implementer;
  int variant;
  int part;
  int revision;
} Aarch64Info;

bool GetAarch64Info(Aarch64Info* outInfo);

////////////////////////////////////////////////////////////////////////////////
// Introspection functions

typedef enum {
  AARCH64_FP,
  AARCH64_ASIMD,
  AARCH64_EVTSTRM,
  AARCH64_AES,
  AARCH64_PMULL,
  AARCH64_SHA1,
  AARCH64_SHA2,
  AARCH64_CRC32,
  AARCH64_ATOMICS,
  AARCH64_FPHP,
  AARCH64_ASIMDHP,
  AARCH64_CPUID,
  AARCH64_ASIMDRDM,
  AARCH64_JSCVT,
  AARCH64_FCMA,
  AARCH64_LRCPC,
  AARCH64_DCPOP,
  AARCH64_SHA3,
  AARCH64_SM3,
  AARCH64_SM4,
  AARCH64_ASIMDDP,
  AARCH64_SHA512,
  AARCH64_SVE,
  AARCH64_ASIMDFHM,
  AARCH64_DIT,
  AARCH64_USCAT,
  AARCH64_ILRCPC,
  AARCH64_FLAGM,
  AARCH64_SSBS,
  AARCH64_SB,
  AARCH64_PACA,
  AARCH64_PACG,
  AARCH64_DCPODP,
  AARCH64_SVE2,
  AARCH64_SVEAES,
  AARCH64_SVEPMULL,
  AARCH64_SVEBITPERM,
  AARCH64_SVESHA3,
  AARCH64_SVESM4,
  AARCH64_FLAGM2,
  AARCH64_FRINT,
  AARCH64_SVEI8MM,
  AARCH64_SVEF32MM,
  AARCH64_SVEF64MM,
  AARCH64_SVEBF16,
  AARCH64_I8MM,
  AARCH64_BF16,
  AARCH64_DGH,
  AARCH64_RNG,
  AARCH64_BTI,
  AARCH64_MTE,
  AARCH64_LAST_,
} Aarch64FeaturesEnum;

int GetAarch64FeaturesEnumValue(const Aarch64Features* features,
                                Aarch64FeaturesEnum value);

const char* GetAarch64FeaturesEnumName(Aarch64FeaturesEnum);

CPU_FEATURES_END_CPP_NAMESPACE
