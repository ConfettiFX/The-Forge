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

//--------------------------------------------------------------------------------------------
//
// Copyright (C) 2009 - 2016 Confetti Interactive Inc.
// All rights reserved.
//
// This source may not be distributed and/or modified without expressly written permission
// from Confetti Interactive Inc.
//
//--------------------------------------------------------------------------------------------

#pragma once

#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include <cstdint>

struct AsteroidStatic
{
	float4 rotationAxis;
	float4 surfaceColor;
	float4 deepColor;

	float scale;
	float orbitSpeed;
	float rotationSpeed;

	uint32_t textureID;
	uint32_t vertexStart;
	uint32_t padding[3];
};

struct AsteroidDynamic
{
	mat4     transform;
	uint32_t indexStart;
	uint32_t indexCount;
	uint32_t padding[2];
};

struct AsteroidSimulation
{
	public:
	void Init(uint32_t rngSeed, uint32_t numAsteroids, uint32_t numMeshes, uint32_t vertexCountPerMesh, uint32_t textureCount);

	void update(float deltaTime, unsigned startIdx, unsigned endIdx, const vec3& cameraPosition);

	tinystl::vector<AsteroidStatic>  asteroidsStatic;
	tinystl::vector<AsteroidDynamic> asteroidsDynamic;
	//tinystl::vector<Asteroid> asteroids;
	int*     indexOffsets;
	unsigned numLODs;
};