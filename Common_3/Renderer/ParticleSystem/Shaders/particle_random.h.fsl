/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

uint rand(uint seed)
{
	// Xorshift algorithm from George Marsaglia's paper
	seed = seed * seed;

	seed ^= (seed << 13);
	seed ^= (seed >> 17);
	seed ^= (seed << 5);
	return seed;
}

float randFloat_zeroToPlusOne(inout(uint) seed)
{
	seed = rand(seed);
	return float(seed) * (1.0f / 4294967296.0f);
}

float randFloat_negOneToPlusOne(inout(uint) seed)
{
	return 2.0f * randFloat_zeroToPlusOne(seed) - 1.0f;
}

float3 randFloat3_zeroToPlusOne(inout(uint) seed)
{
	float3 value;
	value.x = randFloat_zeroToPlusOne(seed);
	value.y = randFloat_zeroToPlusOne(seed);
	value.z = randFloat_zeroToPlusOne(seed);
	return value;
}

float3 randFloat3_negOneToPlusOne(inout(uint) seed)
{
	return 2.0f * randFloat3_zeroToPlusOne(seed) - float3(1.0f, 1.0f, 1.0f);
}