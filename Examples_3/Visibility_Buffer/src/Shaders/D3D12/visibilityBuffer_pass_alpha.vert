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

#include "shader_defs.h"
#include "packing.h"

#ifndef CONF_EARLY_DEPTH_STENCIL
#define CONF_EARLY_DEPTH_STENCIL
#endif

struct VsInAlphaTested
{
	float3 position : POSITION;
	uint texCoord : TEXCOORD;
};

ConstantBuffer<PerFrameConstants> uniforms : register(b0);

struct PsInAlphaTested
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD0;
};

PsInAlphaTested main(VsInAlphaTested In)
{
	PsInAlphaTested Out;
	Out.position = mul(uniforms.transform[VIEW_CAMERA].mvp, float4(In.position, 1.0f));
	Out.texCoord = unpack2Floats(In.texCoord);
	return Out;
}
