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

#include "hwcaps.h"

#include <stdlib.h>
#include <string.h>

#include "cpu_features_macros.h"

#include "../../../../../Utilities/Interfaces/ILog.h"

static bool IsSet(const uint32_t mask, const uint32_t value) {
	if (mask == 0) return false;
	return (value & mask) == mask;
}

bool CpuFeatures_IsHwCapsSet(const HardwareCapabilities hwcaps_mask,
                             const HardwareCapabilities hwcaps) {
	return IsSet(hwcaps_mask.hwcaps, hwcaps.hwcaps) ||
           IsSet(hwcaps_mask.hwcaps2, hwcaps.hwcaps2);
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of GetElfHwcapFromGetauxval
////////////////////////////////////////////////////////////////////////////////

#define AT_HWCAP 16
#define AT_HWCAP2 26
#define AT_PLATFORM 15
#define AT_BASE_PLATFORM 24

#include <sys/auxv.h>
static unsigned long GetElfHwcapFromGetauxval(uint32_t hwcap_type) {
	return getauxval(hwcap_type);
}

// Implementation of GetHardwareCapabilities for OS that provide
// GetElfHwcapFromGetauxval().

// Fallback when getauxval is not available, retrieves hwcaps from
// "/proc/self/auxv".

// Retrieves hardware capabilities by first trying to call getauxval, if not
// available falls back to reading "/proc/self/auxv".
static unsigned long GetHardwareCapabilitiesFor(uint32_t type) {
	unsigned long hwcaps = GetElfHwcapFromGetauxval(type);
	//returning 0 doesn't set any features bits
	return hwcaps;
}

HardwareCapabilities CpuFeatures_GetHardwareCapabilities(void) {
	HardwareCapabilities capabilities;
	capabilities.hwcaps = GetHardwareCapabilitiesFor(AT_HWCAP);
	capabilities.hwcaps2 = GetHardwareCapabilitiesFor(AT_HWCAP2);
	return capabilities;
}

const char *CpuFeatures_GetPlatformPointer(void) {
	return (const char *)GetHardwareCapabilitiesFor(AT_PLATFORM);
}

const char *CpuFeatures_GetBasePlatformPointer(void) {
	return (const char *)GetHardwareCapabilitiesFor(AT_BASE_PLATFORM);
}