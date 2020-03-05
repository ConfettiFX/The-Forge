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

#include <metal_stdlib>
using namespace metal;


struct Fragment_Shader
{
	struct PsIn
	{
		float4 position [[position]];
		float2 texCoord;
	};
	texture2d<float> SourceTexture;
	sampler repeatBillinearSampler;
	
	
	float4 main(PsIn In)
	{
		float4 sceneColor = SourceTexture.sample(repeatBillinearSampler, In.texCoord);
		float3 resultColor = float3(0.0, 0.0, 0.0);
		(resultColor = sceneColor.rgb);
		
		return float4(resultColor, 1.0);
	};
	
	Fragment_Shader(
					texture2d<float> SourceTexture,sampler repeatBillinearSampler) :
	SourceTexture(SourceTexture),repeatBillinearSampler(repeatBillinearSampler) {}
};

struct FSData {
    texture2d<float> SourceTexture [[id(0)]];
    sampler repeatBillinearSampler [[id(1)]];
};

fragment float4 stageMain(
    Fragment_Shader::PsIn In [[stage_in]],
    constant FSData& fsData     [[buffer(UPDATE_FREQ_NONE)]]
)
{
	Fragment_Shader::PsIn In0;
	In0.position = float4(In.position.xyz, 1.0 / In.position.w);
	In0.texCoord = In.texCoord;
	Fragment_Shader main(fsData.SourceTexture, fsData.repeatBillinearSampler);
	return main.main(In0);
}

