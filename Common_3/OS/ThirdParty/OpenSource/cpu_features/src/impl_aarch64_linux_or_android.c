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

#include "cpu_features_macros.h"

#ifdef ARCH_ARM64
#if defined(CPU_FEATURES_OS_LINUX) || defined(ANDROID)

#include "cpuinfo_aarch64.h"
#include "../../../../../Utilities/Interfaces/IFileSystem.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions for introspection.
////////////////////////////////////////////////////////////////////////////////
#define INTROSPECTION_TABLE                                                \
  LINE(AARCH64_FP, fp, "fp", AARCH64_HWCAP_FP, 0)                          \
  LINE(AARCH64_ASIMD, asimd, "asimd", AARCH64_HWCAP_ASIMD, 0)              \
  LINE(AARCH64_EVTSTRM, evtstrm, "evtstrm", AARCH64_HWCAP_EVTSTRM, 0)      \
  LINE(AARCH64_AES, aes, "aes", AARCH64_HWCAP_AES, 0)                      \
  LINE(AARCH64_PMULL, pmull, "pmull", AARCH64_HWCAP_PMULL, 0)              \
  LINE(AARCH64_SHA1, sha1, "sha1", AARCH64_HWCAP_SHA1, 0)                  \
  LINE(AARCH64_SHA2, sha2, "sha2", AARCH64_HWCAP_SHA2, 0)                  \
  LINE(AARCH64_CRC32, crc32, "crc32", AARCH64_HWCAP_CRC32, 0)              \
  LINE(AARCH64_ATOMICS, atomics, "atomics", AARCH64_HWCAP_ATOMICS, 0)      \
  LINE(AARCH64_FPHP, fphp, "fphp", AARCH64_HWCAP_FPHP, 0)                  \
  LINE(AARCH64_ASIMDHP, asimdhp, "asimdhp", AARCH64_HWCAP_ASIMDHP, 0)      \
  LINE(AARCH64_CPUID, cpuid, "cpuid", AARCH64_HWCAP_CPUID, 0)              \
  LINE(AARCH64_ASIMDRDM, asimdrdm, "asimdrdm", AARCH64_HWCAP_ASIMDRDM, 0)  \
  LINE(AARCH64_JSCVT, jscvt, "jscvt", AARCH64_HWCAP_JSCVT, 0)              \
  LINE(AARCH64_FCMA, fcma, "fcma", AARCH64_HWCAP_FCMA, 0)                  \
  LINE(AARCH64_LRCPC, lrcpc, "lrcpc", AARCH64_HWCAP_LRCPC, 0)              \
  LINE(AARCH64_DCPOP, dcpop, "dcpop", AARCH64_HWCAP_DCPOP, 0)              \
  LINE(AARCH64_SHA3, sha3, "sha3", AARCH64_HWCAP_SHA3, 0)                  \
  LINE(AARCH64_SM3, sm3, "sm3", AARCH64_HWCAP_SM3, 0)                      \
  LINE(AARCH64_SM4, sm4, "sm4", AARCH64_HWCAP_SM4, 0)                      \
  LINE(AARCH64_ASIMDDP, asimddp, "asimddp", AARCH64_HWCAP_ASIMDDP, 0)      \
  LINE(AARCH64_SHA512, sha512, "sha512", AARCH64_HWCAP_SHA512, 0)          \
  LINE(AARCH64_SVE, sve, "sve", AARCH64_HWCAP_SVE, 0)                      \
  LINE(AARCH64_ASIMDFHM, asimdfhm, "asimdfhm", AARCH64_HWCAP_ASIMDFHM, 0)  \
  LINE(AARCH64_DIT, dit, "dit", AARCH64_HWCAP_DIT, 0)                      \
  LINE(AARCH64_USCAT, uscat, "uscat", AARCH64_HWCAP_USCAT, 0)              \
  LINE(AARCH64_ILRCPC, ilrcpc, "ilrcpc", AARCH64_HWCAP_ILRCPC, 0)          \
  LINE(AARCH64_FLAGM, flagm, "flagm", AARCH64_HWCAP_FLAGM, 0)              \
  LINE(AARCH64_SSBS, ssbs, "ssbs", AARCH64_HWCAP_SSBS, 0)                  \
  LINE(AARCH64_SB, sb, "sb", AARCH64_HWCAP_SB, 0)                          \
  LINE(AARCH64_PACA, paca, "paca", AARCH64_HWCAP_PACA, 0)                  \
  LINE(AARCH64_PACG, pacg, "pacg", AARCH64_HWCAP_PACG, 0)                  \
  LINE(AARCH64_DCPODP, dcpodp, "dcpodp", 0, AARCH64_HWCAP2_DCPODP)         \
  LINE(AARCH64_SVE2, sve2, "sve2", 0, AARCH64_HWCAP2_SVE2)                 \
  LINE(AARCH64_SVEAES, sveaes, "sveaes", 0, AARCH64_HWCAP2_SVEAES)         \
  LINE(AARCH64_SVEPMULL, svepmull, "svepmull", 0, AARCH64_HWCAP2_SVEPMULL) \
  LINE(AARCH64_SVEBITPERM, svebitperm, "svebitperm", 0,                    \
       AARCH64_HWCAP2_SVEBITPERM)                                          \
  LINE(AARCH64_SVESHA3, svesha3, "svesha3", 0, AARCH64_HWCAP2_SVESHA3)     \
  LINE(AARCH64_SVESM4, svesm4, "svesm4", 0, AARCH64_HWCAP2_SVESM4)         \
  LINE(AARCH64_FLAGM2, flagm2, "flagm2", 0, AARCH64_HWCAP2_FLAGM2)         \
  LINE(AARCH64_FRINT, frint, "frint", 0, AARCH64_HWCAP2_FRINT)             \
  LINE(AARCH64_SVEI8MM, svei8mm, "svei8mm", 0, AARCH64_HWCAP2_SVEI8MM)     \
  LINE(AARCH64_SVEF32MM, svef32mm, "svef32mm", 0, AARCH64_HWCAP2_SVEF32MM) \
  LINE(AARCH64_SVEF64MM, svef64mm, "svef64mm", 0, AARCH64_HWCAP2_SVEF64MM) \
  LINE(AARCH64_SVEBF16, svebf16, "svebf16", 0, AARCH64_HWCAP2_SVEBF16)     \
  LINE(AARCH64_I8MM, i8mm, "i8mm", 0, AARCH64_HWCAP2_I8MM)                 \
  LINE(AARCH64_BF16, bf16, "bf16", 0, AARCH64_HWCAP2_BF16)                 \
  LINE(AARCH64_DGH, dgh, "dgh", 0, AARCH64_HWCAP2_DGH)                     \
  LINE(AARCH64_RNG, rng, "rng", 0, AARCH64_HWCAP2_RNG)                     \
  LINE(AARCH64_BTI, bti, "bti", 0, AARCH64_HWCAP2_BTI)                     \
  LINE(AARCH64_MTE, mte, "mte", 0, AARCH64_HWCAP2_MTE)
#define INTROSPECTION_PREFIX Aarch64
#define INTROSPECTION_ENUM_PREFIX AARCH64
#include "define_introspection_and_hwcaps.inl"

////////////////////////////////////////////////////////////////////////////////
// Implementation.
////////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>

#include "bit_utils.h"

#include <stdlib.h>

int parseNumber(const char* string)
{
	if (strncmp("0x", string, 2) == 0)
	{
		return strtol(string, NULL, 16);
	}
	else
	{
		return strtol(string, NULL, 10);
	}
}

//const char* getCpuName(int implementer, int part, int variant) {
//	switch (implementer)
//	{
//	case 'A':
//		switch (part) {
//		case 0xC05:
//			return "ARM Cortex a5";
//		case 0xC07:
//			return "ARM Cortex a7";
//		case 0xC08:
//			return "ARM Cortex a8";
//		case 0xC09:
//			return "ARM Cortex a9";
//		case 0xC0C:
//			return "ARM Cortex a12";
//		case 0xC0E:
//			return "ARM Cortex a17";
//		case 0xC0D:
//			return "ARM Cortex a12";
//		case 0xC0F:
//			return "ARM Cortex a15";
//		case 0xD01:
//			return "ARM Cortex a32";
//		case 0xD03:
//			return "ARM Cortex a53";
//		case 0xD04:
//			return "ARM Cortex a35";
//		case 0xD05:
//			if (variant == 0) {
//				return "ARM Cortex a55r0";
//			} else {
//				return "ARM Cortex a55";
//			}
//		case 0xD06:
//			return "ARM Cortex a65";
//		case 0xD07:
//			return "ARM Cortex a57";
//		case 0xD08:
//			return "ARM Cortex a72";
//		case 0xD09:
//			return "ARM Cortex a73";
//		case 0xD0A:
//			return "ARM Cortex a75";
//		case 0xD0B:
//			return "ARM Cortex a76";
//		case 0xD0C:
//			return "ARM Cortex an1";
//		case 0xD0D:
//			return "ARM Cortex a77";
//		case 0xD0E:
//			return "ARMCortex a76";
//		case 0xD41:
//			return "ARM Cortex a78";
//		case 0xD44:
//			return "ARM Cortex x1";
//		case 0xD4A:
//			return "ARM Cortex e1";
//		default:
//			switch (part >> 8) {
//			case 7:
//				return "ARM7";
//			case 9:
//				return "ARM9";
//			case 11:
//				return "ARM11";
//			default:
//				return "Unknown ARM CPU";
//			}
//		}
//
//	case 'B':
//		switch (part) {
//		case 0x00F:
//			return "Broadcom Brahma b15";
//		case 0x100:
//			return "Broadcom Brahma b53";
//		case 0x516:
//			return "Cavium Thunder X2";
//		default:
//			return "Unknown Broadcom";
//		}
//
//	case 'C':
//		switch (part) {
//		case 0x0A0:
//			return "Cavium Thunder X";
//		case 0x0A1:
//			return "Cavium Thunder X 88";
//		case 0x0A2:
//			return "Cavium Thunder X 81";
//		case 0x0A3:
//			return "Cavium Thunder X 83";
//		case 0x0AF:
//			return "Cavium Thunder X2 99";
//			break;
//		default:
//			return "Unknown Cavium";
//		}
//
//	case 'H':
//		switch (part) {
//		case 0xD01:
//			return "Huawei Taishan v110";
//		case 0xD40:
//			return "Cortex A76";
//		default:
//			return "Unknown Huawei";
//		}
//
//	case 'i':
//		switch (part >> 8) {
//		case 2:
//		case 4:
//		case 6:
//			return "Intel XScale";
//		default:
//			return "Unknown Intel";
//		}
//
//	case 'N':
//		switch (part) {
//		case 0x000:
//			return "Nvidia Denever";
//		case 0x003:
//			return "Nvidia Denever 2";
//		case 0x004:
//			return "Nvidia Carmel";
//		default:
//			return "Unknown Nvidia";
//		}
//
//	case 'P':
//		switch (part) {
//		case 0x000:
//			return "Applied Micro XGene";
//		default:
//			return "Unknown Applied Micro";
//		}
//
//	case 'Q':
//		switch (part)
//		{
//		case 0x00F:
//		case 0x02D:
//			return "Qualcomm Scorpion";
//		case 0x04D:
//		case 0x06F:
//			return "Qualcomm Krait";
//		case 0x201:
//		case 0x205:
//		case 0x211:
//			return "Qualcomm Kyro";
//		case 0x800:
//			return "ARM Cortex a73";
//		case 0x801:
//			return "ARM Cortex a53";
//		case 0x802:
//			return "ARM Cortex a75";
//		case 0x803:
//			return "ARM Cortex a55r0";
//			break;
//		case 0x804:
//			return "ARM Cortex a76";
//		case 0x805:
//			return "ARM Cortex a55";
//		case 0xC00:
//			return "Qualcomm Falkor";
//		case 0xC01:
//			return "Qualcomm Saphira";
//		default:
//			return "Unknown Qualcomm";
//		}
//
//	case 'S':
//		switch (part) {
//		case 0x001:
//			switch (variant)
//			{
//			case 0x1:
//				return "Samsung Exynos M1";
//			case 0x4:
//				return "Samsung Exynos M2";
//			default:
//				break;
//			}
//			break;
//		case 0x002:
//			switch (variant)
//			{
//			case 0x1:
//				return "Samsung Exynos M3";
//			default:
//				break;
//			}
//			break;
//		case 0x003:
//			switch (variant)
//			{
//			case 0x1:
//				return "Samsung Exynos M4";
//			default:
//				break;
//			}
//			break;
//		case 0x004:
//			switch (variant)
//			{
//			case 0x1:
//				return "Samsung Exynos M5";
//			default:
//				break;
//			}
//			break;
//		default:
//			break;
//		}
//		return "Unknown Samsung";
//
//	case 'V':
//		switch (part) {
//		case 0x581:
//		case 0x584:
//			return "Marvell PJ4";
//		default:
//			return "Unknown Marvell";
//		}
//
//	default:
//		return "Unkown CPU implementer";
//	}
//}

//static void FillProcCpuInfoData(Aarch64Info* const info)
//{
//	// Handling Linux platform through /proc/cpuinfo.
//
//	FileStream fs = {};
//	if (fsOpenStreamFromPath(RD_SYSTEM, "cpuinfo", FM_READ, NULL, &fs))
//	{
//		char key[STACK_LINE_READER_BUFFER_SIZE];
//		char val[STACK_LINE_READER_BUFFER_SIZE];
//		while (fscanf(fs.pFile, "%s : %[^\n]s", key, val) != EOF)
//		{
//			if (strcmp("implementer", key) == 0)
//			{
//				info->implementer = parseNumber(val);
//			}
//
//			else if (strcmp("variant", key) == 0)
//			{
//				info->variant = parseNumber(val);
//			}
//
//			else if (strcmp("part", key) == 0)
//			{
//				info->part = parseNumber(val);
//			}
//
//			else if (strcmp("revision", key) == 0)
//			{
//				info->revision = parseNumber(val);
//			}
//		}
//
//		fsCloseStream(&fs);
//	}
//}

bool GetAarch64Info(Aarch64Info* outInfo) {
  //we will only fetch cpu info form /proc/cpuinfo.
  //FillProcCpuInfoData(outInfo);
  //strcpy(outInfo->name, getCpuName(outInfo->implementer, outInfo->part, outInfo->variant));

  const HardwareCapabilities hwcaps = CpuFeatures_GetHardwareCapabilities();
  //if both hwcaps are zeros then GetAarch64Info failed
  bool result = hwcaps.hwcaps | hwcaps.hwcaps2;
  for (size_t i = 0; i < AARCH64_LAST_; ++i) {
    if (CpuFeatures_IsHwCapsSet(kHardwareCapabilities[i], hwcaps)) {
      kSetters[i](&outInfo->features, true);
    }
  }

  return result;
}

#endif  // defined(CPU_FEATURES_OS_LINUX) || defined(ANDROID)
#endif  // ARCH_ARM64
