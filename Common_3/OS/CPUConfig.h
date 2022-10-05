#pragma once
#include <stdbool.h>

#include"../Application/Config.h"

#include "ThirdParty/OpenSource/cpu_features/src/cpu_features_types.h"

typedef enum 
{
	SIMD_SSE3, SIMD_SSE4_1, SIMD_SSE4_2, SIMD_AVX, SIMD_AVX2,
	SIMD_NEON
}SimdIntrinsic;

typedef struct 
{
	char mName[512];
	SimdIntrinsic mSimd;

	X86Features mFeaturesX86;
	X86Microarchitecture mArchitectureX86;

	Aarch64Features mFeaturesAarch64;
} CpuInfo;

#if defined(ANDROID)
#include <jni.h>
bool initCpuInfo(CpuInfo* outCpuInfo, JNIEnv* pJavaEnv);
#else
bool initCpuInfo(CpuInfo* outCpuInfo);
#endif

CpuInfo* getCpuInfo(void);