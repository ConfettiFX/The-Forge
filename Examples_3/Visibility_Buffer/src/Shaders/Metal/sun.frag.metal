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

#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
	struct PsIn
	{
		float4 Position [[position]];
	};
	struct Uniforms_RootConstantSunMatrices
	{
		float4x4 projView;
		float4x4 model;
		float4 mLightColor;
	};
	constant Uniforms_RootConstantSunMatrices & RootConstantSunMatrices;
	float4 main(PsIn In)
	{
		return float4((RootConstantSunMatrices.mLightColor.rgb * (float3)(RootConstantSunMatrices.mLightColor.a)), 1.0);
	};
	
	Fragment_Shader(
					constant Uniforms_RootConstantSunMatrices & RootConstantSunMatrices) :
	RootConstantSunMatrices(RootConstantSunMatrices) {}
};


fragment float4 stageMain(
						  Fragment_Shader::PsIn In [[stage_in]],
						  constant Fragment_Shader::Uniforms_RootConstantSunMatrices & RootConstantSunMatrices [[buffer(1)]])
{
	Fragment_Shader::PsIn In0;
	In0.Position = float4(In.Position.xyz, 1.0 / In.Position.w);
	Fragment_Shader main(RootConstantSunMatrices);
	return main.main(In0);
}

