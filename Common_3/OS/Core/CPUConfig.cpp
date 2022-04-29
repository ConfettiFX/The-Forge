#include "CPUConfig.h"

#if defined(ARCH_X86_FAMILY)
#include "../../ThirdParty/OpenSource/cpu_features/src/cpuinfo_x86.h"
#elif defined(ARCH_ARM64)
#include "../../ThirdParty/OpenSource/cpu_features/src/cpuinfo_aarch64.h"
#endif

#include "../../ThirdParty/OpenSource/EASTL/string.h"

#define CPUNAMELENGTH 49

#if defined(ANDROID)
bool initCpuInfo(CpuInfo* outCpuInfo, JNIEnv* pJavaEnv)
#else
bool initCpuInfo(CpuInfo* outCpuInfo)
#endif
{
	bool result = false;

	//initializing name with null
	outCpuInfo->name[0] = '\0';

#if defined(ARCH_X86_FAMILY)
	X86Info info;
	
	//all X86 platfomrs are supported by cpu_features
	result = GetX86Info(&info);

	if (result) {
		outCpuInfo->features = info.features;
		outCpuInfo->architecture = GetX86Microarchitecture(&info);

		char name[CPUNAMELENGTH];
		FillX86BrandString(name);

		//trim string
		///trim end
		char* trimmedName = name + (CPUNAMELENGTH - 1);
		while (*trimmedName == ' ' || *trimmedName == '\0') {
			*trimmedName = '\0';
			trimmedName--;
		}
		///trim  start
		trimmedName = name;
		while (*trimmedName == ' ') {
			trimmedName++;
		}

		const char* brandName = trimmedName;
		const char* simdName = "";

		//orbis and prospero do not provide cpu names
#if defined(ORBIS)
		brandName = "Orbis";
#elif defined(PROSPERO)
		brandName = "Prospero";
#endif

		if (outCpuInfo->features.avx2) {
			outCpuInfo->simdFeature = SIMD_AVX2;
			simdName = "SIMD: AVX2";
		}
		else if (outCpuInfo->features.avx) {
			outCpuInfo->simdFeature = SIMD_AVX;
			simdName = "SIMD: AVX";
		}
		else if (outCpuInfo->features.sse4_2) {
			outCpuInfo->simdFeature = SIMD_SSE4_2;
			simdName = "SIMD: SSE4.2";
		}
		else if (outCpuInfo->features.sse4_1) {
			outCpuInfo->simdFeature = SIMD_SSE4_1;
			simdName = "SIMD: SSE4.1";
		}
		else {
			outCpuInfo->simdFeature = SIMD_SSE3;
			simdName = "SIMD: SSE3";
		}

		snprintf(outCpuInfo->name, sizeof(outCpuInfo->name), "%s\t\t\t\t\t %s", brandName, simdName);
	}
	else {
		outCpuInfo->features.sse3 = -1;
		outCpuInfo->simdFeature = SIMD_SSE3;
	}

#elif defined(ARCH_ARM64)
	Aarch64Info info;

	//ARM64 supported platforms by cpu_features
#if defined(ANDROID) || defined(__LINUX__) || defined(TARGET_APPLE_ARM64)
	result = GetAarch64Info(&info);
#endif

	if (result) {
#if defined(ANDROID)
		jclass classBuild = pJavaEnv->FindClass("android/os/Build");

		jfieldID field;
		jstring jHardwareString, jBrandString, jBoardString, jModelString;
		const char *hardwareString, *brandStrirng, *boardString, *modelString;

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

		snprintf(outCpuInfo->name, sizeof(outCpuInfo->name), "%s %s %s %s %s   ", outCpuInfo->name, hardwareString, brandStrirng, boardString, modelString);

		pJavaEnv->GetStringUTFChars(jHardwareString, 0);
		pJavaEnv->GetStringUTFChars(jBrandString, 0);
		pJavaEnv->GetStringUTFChars(jBoardString, 0);
		pJavaEnv->GetStringUTFChars(jModelString, 0);

#endif
		outCpuInfo->features = info.features;
		snprintf(outCpuInfo->name, sizeof(outCpuInfo->name), "%s%s", outCpuInfo->name, info.name);

		outCpuInfo->simdFeature = SIMD_NEON;
		snprintf(outCpuInfo->name, sizeof(outCpuInfo->name), "%s%s", outCpuInfo->name, "\t\t\t\t\t SIMD: NEON ");
	}
#endif

	return result;
}
