/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

///////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
///////////////////////////////////////////////////////////////////////////////

#include "../../../../Common_3/OS/Interfaces/IOperatingSystem.h"
#include "AsteroidSim.h"
#include "Random.h"
#if !defined(TARGET_APPLE_ARM64) && !defined(__ANDROID__) && !defined(NX64)
#include <immintrin.h>
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#define ALIGNED_(x) __attribute__((aligned(x)))
#endif

vec3 RandomPointOnSphere(MyRandom& rng)
{
	float angleDist = rng.GetUniformDistribution(-PI, PI);
	float heightDist = rng.GetUniformDistribution(-1.0f, 1.0f);

	float angle = angleDist;
	float height = heightDist;
	float radius = sqrt(1 - height * height);

	float x = cos(angle) * radius;
	float y = sin(angle) * radius;

	return vec3(x, y, height);
}

#define NUM_COLOR_SCHEMES 6
static int const COLOR_SCHEMES[] = {
	84, 65, 42,   9,   7,   5, 84, 65, 42, 10,  4,  1,  84,  65, 42, 31, 33, 35, 
	84, 65, 42, 155, 142, 113, 81, 73, 62, 24, 24, 24, 129, 117, 94, 75, 38, 33,
};

void AsteroidSimulation::Init(
	uint32_t rngSeed, uint32_t numAsteroids, uint32_t numMeshes, uint32_t vertexCountPerMesh, uint32_t textureCount)
{
	const float orbitRadius = 450;
	const float discRadius = 120;

	float linearColorSchemes[NUM_COLOR_SCHEMES * 6];
	for (int i = 0; i < NUM_COLOR_SCHEMES * 6; ++i)
		linearColorSchemes[i] = srgbToLinearf((float)COLOR_SCHEMES[i] / 255.0f);

	MyRandom rng(rngSeed);

	uint32_t instancesPerMesh = MAX(1, numAsteroids / numMeshes);

	for (unsigned i = 0; i < numAsteroids; ++i)
	{
		float orbitRadiusDist = rng.GetNormalDistribution(orbitRadius, 0.6f * discRadius);
		float heightDist = rng.GetNormalDistribution(0, 0.4f);
		float angleDist = rng.GetUniformDistribution(-PI, PI);
		float orbitSpeedDist = rng.GetUniformDistribution(5.0f, 15.0f);
		float rotateSpeedDist = rng.GetUniformDistribution(-2.0f, 2.0f);
		float scaleDist = rng.GetNormalDistribution(1.3f, 0.7f);
		float colorSchemeDist = rng.GetNormalDistribution(0, NUM_COLOR_SCHEMES - 1);
		int   textureIndexDist = rng.GetUniformDistribution(0, textureCount - 1);

		AsteroidStatic staticAsteroid = {};
        
		staticAsteroid.scale = scaleDist;

		float orbitRadius = orbitRadiusDist;

		float    height = heightDist * discRadius;
		float    orbitAngle = angleDist;
		uint32_t meshInstance = i / instancesPerMesh;

		staticAsteroid.rotationSpeed = rotateSpeedDist / staticAsteroid.scale;
		staticAsteroid.orbitSpeed = orbitSpeedDist / (staticAsteroid.scale * orbitRadius);
		staticAsteroid.vertexStart = vertexCountPerMesh * meshInstance;

		staticAsteroid.rotationAxis = v4ToF4(normalize(vec4(RandomPointOnSphere(rng), 0.0f)));

		staticAsteroid.textureID = unsigned(textureIndexDist);

		int    colorScheme = ((int)abs(colorSchemeDist)) % NUM_COLOR_SCHEMES;
		float* c = linearColorSchemes + 6 * colorScheme;
		staticAsteroid.surfaceColor = float4(c[0], c[1], c[2], 1.0f);
		staticAsteroid.deepColor = float4(c[3], c[4], c[5], 1.0f);
		asteroidsStatic.push_back(staticAsteroid);

		AsteroidDynamic dynamicAsteroid;
		mat4            scaleMat = mat4::scale(vec3(staticAsteroid.scale));
		mat4            translate = mat4::translation(vec3(orbitRadius, height, 0));
		mat4            orbit = mat4::rotation(orbitAngle, vec3(0, 1, 0));
		dynamicAsteroid.transform = orbit * translate * scaleMat;
        dynamicAsteroid.indexCount = 0;
        dynamicAsteroid.indexStart = 0;
		asteroidsDynamic.push_back(dynamicAsteroid);
	}
}

// From http://guihaire.com/code/?p=1135
static inline float VeryApproxLog2f(float x)
{
	union
	{
		float    f;
		uint32_t i;
	} ux;
	ux.f = x;
	return (float)ux.i * 1.1920928955078125e-7f - 126.94269504f;
}

void AsteroidSimulation::Exit()
{
	asteroidsStatic.set_capacity(0);
	asteroidsDynamic.set_capacity(0);
}

void AsteroidSimulation::Update(float deltaTime, unsigned startIdx, unsigned endIdx, const vec3& cameraPosition)
{
	//taken from intel demo
	static const float minSubdivSizeLog2 = log2f(0.0019f);

	for (unsigned i = startIdx; i < endIdx; ++i)
	{
		AsteroidStatic&  staticAsteroid = asteroidsStatic[i];
		AsteroidDynamic& dynamicAsteroid = asteroidsDynamic[i];

		ALIGNED_(16) mat4 orbit = mat4::rotation(staticAsteroid.orbitSpeed * deltaTime, vec3(0, 1, 0));
		ALIGNED_(16)
		mat4 rotate = mat4::rotation(
			staticAsteroid.rotationSpeed * deltaTime,
			vec3(staticAsteroid.rotationAxis.getX(), staticAsteroid.rotationAxis.getY(), staticAsteroid.rotationAxis.getZ()));

#if defined(XBOX) || defined(TARGET_IOS) || defined(NX64)
		// XBoxOne/iOS don't support some of these SSE instructions.
		// 0xC000001D: Illegal Instruction
		// Implement it without SSE

		dynamicAsteroid.transform = orbit * dynamicAsteroid.transform * rotate;
#else
		ALIGNED_(16) float orbit0[4] = { orbit[0][0], orbit[0][1], orbit[0][2], orbit[0][3] };
		ALIGNED_(16) float orbit1[4] = { orbit[1][0], orbit[1][1], orbit[1][2], orbit[1][3] };
		ALIGNED_(16) float orbit2[4] = { orbit[2][0], orbit[2][1], orbit[2][2], orbit[2][3] };
		ALIGNED_(16) float orbit3[4] = { orbit[3][0], orbit[3][1], orbit[3][2], orbit[3][3] };

		ALIGNED_(16) float rotate0[4] = { rotate[0][0], rotate[0][1], rotate[0][2], rotate[0][3] };
		ALIGNED_(16) float rotate1[4] = { rotate[1][0], rotate[1][1], rotate[1][2], rotate[1][3] };
		ALIGNED_(16) float rotate2[4] = { rotate[2][0], rotate[2][1], rotate[2][2], rotate[2][3] };
		ALIGNED_(16) float rotate3[4] = { rotate[3][0], rotate[3][1], rotate[3][2], rotate[3][3] };

		ALIGNED_(16)
		float transform0[4] = { dynamicAsteroid.transform[0][0], dynamicAsteroid.transform[0][1], dynamicAsteroid.transform[0][2],
								dynamicAsteroid.transform[0][3] };
		ALIGNED_(16)
		float transform1[4] = { dynamicAsteroid.transform[1][0], dynamicAsteroid.transform[1][1], dynamicAsteroid.transform[1][2],
								dynamicAsteroid.transform[1][3] };
		ALIGNED_(16)
		float transform2[4] = { dynamicAsteroid.transform[2][0], dynamicAsteroid.transform[2][1], dynamicAsteroid.transform[2][2],
								dynamicAsteroid.transform[2][3] };
		ALIGNED_(16)
		float transform3[4] = { dynamicAsteroid.transform[3][0], dynamicAsteroid.transform[3][1], dynamicAsteroid.transform[3][2],
								dynamicAsteroid.transform[3][3] };

		// Need to guarantee that passed pointer is 16-bit aligned for _mm_load_ps
		const __m128 orbitSSE[4] = { _mm_load_ps(orbit0), _mm_load_ps(orbit1), _mm_load_ps(orbit2), _mm_load_ps(orbit3) };

		const __m128 rotateSSE[4] = { _mm_load_ps(rotate0), _mm_load_ps(rotate1), _mm_load_ps(rotate2), _mm_load_ps(rotate3) };

		__m128 transformSSE[4] = { _mm_loadu_ps(transform0), _mm_loadu_ps(transform1), _mm_loadu_ps(transform2), _mm_loadu_ps(transform3) };
		__m128 intermediateSSE[4];

		for (int i = 0; i < 4; ++i)
		{
			//From GLM
			const __m128 v = transformSSE[i];
			const __m128 vx = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0));
			const __m128 vy = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1));
			const __m128 vz = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2));
			const __m128 vw = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
			intermediateSSE[i] =
				sseMAdd(orbitSSE[0], vx, sseMAdd(orbitSSE[1], vy, sseMAdd(orbitSSE[2], vz, _mm_mul_ps(orbitSSE[3], vw))));
		}
		for (int i = 0; i < 4; ++i)
		{
			//From GLM
			const __m128 v = rotateSSE[i];
			const __m128 vx = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0));
			const __m128 vy = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1));
			const __m128 vz = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2));
			const __m128 vw = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
			transformSSE[i] = sseMAdd(
				intermediateSSE[0], vx,
				sseMAdd(intermediateSSE[1], vy, sseMAdd(intermediateSSE[2], vz, _mm_mul_ps(intermediateSSE[3], vw))));
		}
		_mm_store_ps(transform0, transformSSE[0]);
		_mm_store_ps(transform1, transformSSE[1]);
		_mm_store_ps(transform2, transformSSE[2]);
		_mm_store_ps(transform3, transformSSE[3]);

		for (uint32_t i = 0; i < 4; i++)
		{
			dynamicAsteroid.transform[0][i] = transform0[i];
			dynamicAsteroid.transform[1][i] = transform1[i];
			dynamicAsteroid.transform[2][i] = transform2[i];
			dynamicAsteroid.transform[3][i] = transform3[i];
		}
#endif
		vec3     position = dynamicAsteroid.transform.getTranslation();
		float    distanceToEye = length(position - cameraPosition);
		float    relativeScreenSizeLog2 = VeryApproxLog2f(staticAsteroid.scale / distanceToEye);
		float    LODfloat = max(0.f, relativeScreenSizeLog2 - minSubdivSizeLog2);
		unsigned LOD = min(numLODs - 1, unsigned(LODfloat));

		dynamicAsteroid.indexStart = indexOffsets[LOD];
		dynamicAsteroid.indexCount = indexOffsets[LOD + 1] - dynamicAsteroid.indexStart;
	}
}
