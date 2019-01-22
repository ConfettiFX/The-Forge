/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "TextureGen.h"
#include "NoiseOctaves.h"
#include "Random.h"

void genTextures(uint32_t texture_count, Image* out_texture)
{
	static const int textureDim = 256;

	uint32_t array_count = 3;

	uint32_t* seeds = (uint32_t*)alloca(texture_count * sizeof(uint32_t));
	{
		for (uint32_t i = 0; i < texture_count; ++i)
			seeds[i] = rand();
	}

	Image* image = out_texture;
	image->Create(ImageFormat::RGBA8, textureDim, textureDim, 1, 1, texture_count * array_count);

	for (uint32_t t = 0; t < texture_count; ++t)
	{
		MyRandom rng(seeds[t]);

		for (uint32_t a = 0; a < array_count; ++a)
		{
			float randomNoise = rng.GetUniformDistribution(0.0f, 10000.0f);
			float randomNoiseScale = rng.GetUniformDistribution(100.0f, 150.0f);
			float randomPersistence = rng.GetNormalDistribution(0.9f, 0.2f);

			// Use same parameters for each of the tri-planar projection planes/cube map faces/etc.
			float noiseScale = randomNoiseScale / float(textureDim);
			float persistence = randomPersistence;
			float seed = randomNoise;
			float strength = 1.5f;

			NoiseOctaves<4> textureNoise(persistence);

			uint      mipLevel = 0;
			uint      slice = t * array_count + a;
			uint32_t* scanline = (uint32_t*)image->GetPixels(mipLevel, slice);
			for (size_t y = 0; y < textureDim; ++y)
			{
				for (size_t x = 0; x < textureDim; ++x)
				{
					float c = textureNoise((float)x * noiseScale, (float)y * noiseScale, seed);
					c = max(0.0f, min(1.0f, (c - 0.5f) * strength + 0.5f));

					int32_t cr = (int32_t)(c * 255.0f);
					int32_t cg = (int32_t)(c * 255.0f);
					int32_t cb = (int32_t)(c * 255.0f);
					scanline[x] = (cr) << 16 | (cg) << 8 | (cb) << 0;
					//scanline[x] = cr;
				}
				scanline += image->GetWidth();
			}
		}
	}

	//*out_textures = images;
}
