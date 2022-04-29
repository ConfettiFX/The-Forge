#pragma once
#include <stdbool.h>

#include"../Core/Config.h"

#include "../../ThirdParty/OpenSource/cpu_features/src/cpu_features_types.h"

typedef enum {
	SIMD_SSE3, SIMD_SSE4_1, SIMD_SSE4_2, SIMD_AVX, SIMD_AVX2,
	SIMD_NEON
}SimdIntrinsic;

typedef struct {
	char name[512];
	SimdIntrinsic simdFeature;

#if defined(ARCH_X86_FAMILY)
	X86Features features;
	X86Microarchitecture architecture;
#elif defined(ARCH_ARM64)
	Aarch64Features features;
#endif
} CpuInfo;

#if defined(ANDROID)
#include <jni.h>
bool initCpuInfo(CpuInfo* outCpuInfo, JNIEnv* pJavaEnv);
#else
bool initCpuInfo(CpuInfo* outCpuInfo);
#endif

CpuInfo* getCpuInfo(void);