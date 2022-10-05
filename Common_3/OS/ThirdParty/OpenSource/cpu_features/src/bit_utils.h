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

#include <stdbool.h>
#include <stdint.h>

#include "cpu_features_macros.h"

#include "../../../../../Utilities/Interfaces/ILog.h"

CPU_FEATURES_START_CPP_NAMESPACE

inline static bool IsBitSet(uint32_t reg, uint32_t bit) {
  return (reg >> bit) & 0x1;
}

inline static uint32_t ExtractBitRange(uint32_t reg, uint32_t msb,
                                       uint32_t lsb) {
  const uint64_t bits = msb - lsb + 1ULL;
  const uint64_t mask = (1ULL << bits) - 1ULL;
  ASSERT(msb >= lsb);
  return (reg >> lsb) & mask;
}

CPU_FEATURES_END_CPP_NAMESPACE
