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

#pragma once
#include "simplexnoise1234.h"

// Very simple multi-octave simplex noise helper
// Returns noise in the range [0, 1] vs. the usual [-1, 1]
template <size_t N = 4>
class NoiseOctaves
{
	private:
	float mWeights[N];
	float mWeightNorm;

	public:
	NoiseOctaves(float persistence = 0.5f)
	{
		float weightSum = 0.0f;
		for (size_t i = 0; i < N; ++i)
		{
			mWeights[i] = persistence;
			weightSum += persistence;
			persistence *= persistence;
		}
		mWeightNorm = 0.5f / weightSum;    // Will normalize to [-0.5, 0.5]
	}

	// Returns [0, 1]
	float operator()(float x, float y, float z) const
	{
		float r = 0.0f;
		for (size_t i = 0; i < N; ++i)
		{
			r += mWeights[i] * snoise3(x, y, z);
			x *= 2.0f;
			y *= 2.0f;
			z *= 2.0f;
		}
		return r * mWeightNorm + 0.5f;
	}

	// Returns [0, 1]
	float operator()(float x, float y, float z, float w) const
	{
		float r = 0.0f;
		for (size_t i = 0; i < N; ++i)
		{
			r += mWeights[i] * snoise4(x, y, z, w);
			x *= 2.0f;
			y *= 2.0f;
			z *= 2.0f;
			w *= 2.0f;
		}
		return r * mWeightNorm + 0.5f;
	}
};
