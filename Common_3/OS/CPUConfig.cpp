#include "CPUConfig.h"

#include <stdio.h>

#include "ThirdParty/OpenSource/cpu_features/src/cpuinfo_x86.h"
#include "ThirdParty/OpenSource/cpu_features/src/cpuinfo_aarch64.h"

#define MAXCPUNAME 49

char* trimString(char* inString);

#if defined(ANDROID)
bool initCpuInfo(CpuInfo* outCpuInfo, JNIEnv* pJavaEnv)
#else
bool initCpuInfo(CpuInfo* outCpuInfo)
#endif
{
	bool result = false;
	outCpuInfo->mName[0] = '\0';

#if defined(ARCH_X86_FAMILY) && !defined(TARGET_IOS_SIMULATOR)
	X86Info info = {};

	const char* simdName = "Unknown";

	//get cpu data
	result = GetX86Info(&info);

	if (result)
	{
		//orbis and prospero do not provide cpu names
#if defined(ORBIS)
		snprintf(info.name, sizeof(info.name), "Orbis");
#elif defined(PROSPERO)
		snprintf(info.name, sizeof(info.name), "Prospero");
#else
		char cpuName[MAXCPUNAME] = "";
		FillX86BrandString(cpuName);
		char* trimmedName = trimString(cpuName);
		snprintf(info.name, sizeof(info.name), "%s", trimmedName);
#endif

		//detect simd
		if (info.features.avx2)
		{
			outCpuInfo->mSimd = SIMD_AVX2;
			simdName = "SIMD: AVX2";
		}
		else if (info.features.avx)
		{
			outCpuInfo->mSimd = SIMD_AVX;
			simdName = "SIMD: AVX";
		}
		else if (info.features.sse4_2)
		{
			outCpuInfo->mSimd = SIMD_SSE4_2;
			simdName = "SIMD: SSE4.2";
		}
		else if (info.features.sse4_1)
		{
			outCpuInfo->mSimd = SIMD_SSE4_1;
			simdName = "SIMD: SSE4.1";
		}
	}

	outCpuInfo->mFeaturesX86 = info.features;
	outCpuInfo->mArchitectureX86 = GetX86Microarchitecture(&info);

	snprintf(outCpuInfo->mName, sizeof(outCpuInfo->mName), "%s \t\t\t\t\t %s", info.name, simdName);
#endif

#if defined(ARCH_ARM64) || defined(TARGET_IOS_SIMULATOR)
	Aarch64Info info = {};

	const char* simdName = "SIMD: NEON";
	outCpuInfo->mSimd = SIMD_NEON;

	//ARM64 supported platforms by cpu_features
#if defined(ANDROID) || defined(__LINUX__) || defined(TARGET_APPLE_ARM64)
	result = GetAarch64Info(&info);
#endif

#if defined(ANDROID)
	jclass classBuild = pJavaEnv->FindClass("android/os/Build");

	jfieldID field;
	jstring jHardwareString, jBrandString, jBoardString, jModelString;
	const char* hardwareString, *brandStrirng, *boardString, *modelString;

	field = pJavaEnv->GetStaticFieldID(classBuild, "HARDWARE", "Ljava/lang/String;");
	jHardwareString = (jstring)pJavaEnv->GetStaticObjectField(classBuild, field);
	hardwareString = pJavaEnv->GetStringUTFChars(jHardwareString, 0);

	field = pJavaEnv->GetStaticFieldID(classBuild, "BRAND", "Ljava/lang/String;");
	jBrandString = (jstring)pJavaEnv->GetStaticObjectField(classBuild, field);
	brandStrirng = pJavaEnv->GetStringUTFChars(jBrandString, 0);

	field = pJavaEnv->GetStaticFieldID(classBuild, "BOARD", "Ljava/lang/String;");
	jBoardString = (jstring)pJavaEnv->GetStaticObjectField(classBuild, field);
	boardString = pJavaEnv->GetStringUTFChars(jBoardString, 0);

	field = pJavaEnv->GetStaticFieldID(classBuild, "MODEL", "Ljava/lang/String;");
	jModelString = (jstring)pJavaEnv->GetStaticObjectField(classBuild, field);
	modelString = pJavaEnv->GetStringUTFChars(jModelString, 0);

	snprintf(info.name, sizeof(info.name), "%s %s %s %s   ", hardwareString, brandStrirng, boardString, modelString);

	pJavaEnv->GetStringUTFChars(jHardwareString, 0);
	pJavaEnv->GetStringUTFChars(jBrandString, 0);
	pJavaEnv->GetStringUTFChars(jBoardString, 0);
	pJavaEnv->GetStringUTFChars(jModelString, 0);

#endif

	outCpuInfo->mFeaturesAarch64 = info.features;

	snprintf(outCpuInfo->mName, sizeof(outCpuInfo->mName), "%s\t\t\t\t\t %s", info.name, simdName);
#endif

	return result;
}

char* trimString(char* inString)
{
	//trim end
	char* trimmedString = inString + (MAXCPUNAME - 1);
	while (*trimmedString == ' ' || *trimmedString == '\0')
	{
		trimmedString--;
	}
	*trimmedString = '\0';

	//trim  start
	trimmedString = inString;
	while (*trimmedString == ' ')
	{
		trimmedString++;
	}

	return trimmedString;
}
