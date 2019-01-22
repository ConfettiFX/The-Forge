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

inline float3x3 matrix_ctor(float4x4 m)
{
	return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Fragment_Shader
{
	struct PsIn
	{
		float4 position [[position]];
		float2 texCoord;
	};
	texture2d<float> uTex0;
	sampler uSampler0;
	struct Uniforms_RootConstantSCurveInfo
	{
		float C1;
		float C2;
		float C3;
		float UseSCurve;
		float ScurveSlope;
		float ScurveScale;
		float linearScale;
		float pad0;
		uint outputMode;
	};
	constant Uniforms_RootConstantSCurveInfo & RootConstantSCurveInfo;
	const float3 PQ_N = 0.15930176;
	const float3 PQ_M = 78.84375;
	const float3 PQ_c1 = 0.8359375;
	const float3 PQ_c2 = 18.8515625;
	const float3 PQ_c3 = 18.6875;
	float3 L2PQ_float3(float3 L)
	{
		(L = pow(max(0.0, (L / (float3)(10000.0))), PQ_N));
		float3 PQ = pow(max(0.0, ((PQ_c1 + (PQ_c2 * L)) / ((float3)(1.0) + (PQ_c3 * L)))), PQ_M);
		return saturate(PQ);
	};
	float3 Rec709ToRec2020(float3 color)
	{
		const float3x3 conversion = { 0.627402, 0.329292, 0.043306, 0.069095, 0.9195440, 0.01136, 0.016394, 0.08802800, 0.8955780 };
		return ((conversion)*(color));
	};
	float3 ApplyDolbySCurve(float3 Color, const float Scale, const float Slope)
	{
		float3 pow_in = pow(abs((clamp(Color.rgb, 0.000061000000, 65504.0) / (float3)(Scale))), Slope);
		return (((float3)(RootConstantSCurveInfo.C1) + ((float3)(RootConstantSCurveInfo.C2) * pow_in)) / ((float3)(1) + ((float3)(RootConstantSCurveInfo.C3) * pow_in)));
	};
	float4 main(PsIn In)
	{
		float4 sceneColor = uTex0.sample(uSampler0, In.texCoord);
		float3 resultColor = float3(0.0, 0.0, 0.0);
		if ((RootConstantSCurveInfo.outputMode == (uint)(0)))
		{
			(resultColor = sceneColor.rgb);
		}
		else
		{
			if ((RootConstantSCurveInfo.UseSCurve > 0.5))
			{
				(resultColor = L2PQ_float3(ApplyDolbySCurve(Rec709ToRec2020(sceneColor.rgb), RootConstantSCurveInfo.ScurveScale, RootConstantSCurveInfo.ScurveSlope)));
			}
			else
			{
				(resultColor = L2PQ_float3(min((Rec709ToRec2020(sceneColor.rgb) * (float3)(RootConstantSCurveInfo.linearScale)), 10000.0)));
			}
		}
		return float4(resultColor, 1.0);
	};
	
	Fragment_Shader(
					texture2d<float> uTex0,sampler uSampler0,constant Uniforms_RootConstantSCurveInfo & RootConstantSCurveInfo) :
	uTex0(uTex0),uSampler0(uSampler0),RootConstantSCurveInfo(RootConstantSCurveInfo) {}
};


fragment float4 stageMain(
						  Fragment_Shader::PsIn In [[stage_in]],
						  texture2d<float> uTex0 [[texture(0)]],
						  sampler uSampler0 [[sampler(0)]],
						  constant Fragment_Shader::Uniforms_RootConstantSCurveInfo & RootConstantSCurveInfo [[buffer(0)]])
{
	Fragment_Shader::PsIn In0;
	In0.position = float4(In.position.xyz, 1.0 / In.position.w);
	In0.texCoord = In.texCoord;
	Fragment_Shader main(uTex0,
						 uSampler0,
						 RootConstantSCurveInfo);
	return main.main(In0);
}

