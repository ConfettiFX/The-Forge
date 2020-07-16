/*
* Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#pragma once

struct Renderer;
struct RenderTarget;
struct Cmd;
struct PipelineCache;

#include "IOperatingSystem.h"

class IMiddleware
{
	public:
	// Our init function should only be called once
	// The middleware has to keep these pointers
	virtual bool Init(Renderer* renderer, PipelineCache* pCache = NULL) = 0;
	virtual void Exit() = 0;

	// When app is loaded, app is provided of the render targets to load
	// App is responsible to keep track of these render targets until load is called again
	virtual bool Load(RenderTarget** rts, uint32_t count = 1) = 0;
	virtual void Unload() = 0;

	virtual void Update(float deltaTime) = 0;
	virtual void Draw(Cmd* cmd) = 0;
};