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
	// USERMACRO: SAMPLE_COUNT [1,2,4]
	struct PsIn
	{
		float4 position [[position]];
	};
	texture2d_ms<float> msaaSource;
	float4 main(PsIn In)
	{
		float4 value = float4(0.0, 0.0, 0.0, 0.0);
		uint2 texCoord = uint2(In.position.xy);
		for (int i = 0; (i < SAMPLE_COUNT); (++i))
		{
			(value += (float4)(msaaSource.read(texCoord, i)));
		}
		(value /= (float4)(SAMPLE_COUNT));
		return value;
	};
	
	Fragment_Shader(
					texture2d_ms<float> msaaSource) :
	msaaSource(msaaSource) {}
};


fragment float4 stageMain(
						  Fragment_Shader::PsIn In [[stage_in]],
						  texture2d_ms<float> msaaSource [[texture(0)]])
{
	Fragment_Shader::PsIn In0;
	In0.position = float4(In.position.xyz, 1.0 / In.position.w);
	Fragment_Shader main(msaaSource);
	return main.main(In0);
}

