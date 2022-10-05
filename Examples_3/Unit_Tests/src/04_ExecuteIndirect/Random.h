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

#ifndef RANDOM_H
#define RANDOM_H

#include <cstdlib>
#include <cmath>

#include "../../../../Common_3/Utilities/Math/MathTypes.h"

// For producing random numbers used in the asteroids test
class MyRandom
{
	private:
	public:
	MyRandom(unsigned seed = 1) { SetSeed(seed); }

	void SetSeed(unsigned seed) { srand(seed); }

	float GetUniformDistribution(float min, float max) { return randomFloat(min, max); }

	int GetUniformDistribution(int min, int max) { return (rand() % (max - min)) + min; }

	// Using polar method
	float GetNormalDistribution(float mean, float stdDev)
	{
		static bool  hasSpare = false;    // not thread-safe
		static float spare;
		float        u, v, s;

		if (hasSpare)
		{
			hasSpare = false;
			return mean + stdDev * spare;
		}

		hasSpare = true;

		do
		{
			u = (rand() / ((float)RAND_MAX)) * 2.0f - 1.0f;
			v = (rand() / ((float)RAND_MAX)) * 2.0f - 1.0f;
			s = u * u + v * v;
		} while (s >= 1 || s == 0);

		s = sqrt((-2.0f * log(s)) / s);
		spare = v * s;
		return (mean + stdDev * u * s);
	}
};

#endif //RANDOM_H
