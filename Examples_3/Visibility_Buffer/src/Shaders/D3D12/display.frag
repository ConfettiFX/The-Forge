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

struct PsIn {
	float4 position: SV_Position;
	float2 texCoord: TEXCOORD;
};

Texture2D uTex0 : register(t0);
SamplerState uSampler0 : register(s1);

cbuffer RootConstantSCurveInfo : register(b0)
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




static const float3 PQ_N = 0.1593017578125;
static const float3 PQ_M = 78.84375;
static const float3 PQ_c1 = 0.8359375;
static const float3 PQ_c2 = 18.8515625;
static const float3 PQ_c3 = 18.6875;

float3 L2PQ_float3(float3 L)
{
	L = pow(max(0.0, L / 10000.0), PQ_N);
	float3 PQ = pow(max(0.0, ((PQ_c1 + PQ_c2 * L) / (1.0 + PQ_c3 * L))), PQ_M);
	return saturate(PQ);
}

float3 Rec709ToRec2020(float3 color)
{
	static const float3x3 conversion =
	{
		0.627402, 0.329292, 0.043306,
		0.069095, 0.919544, 0.011360,
		0.016394, 0.088028, 0.895578
	};
	return mul(conversion, color);
}

float3 ApplyDolbySCurve(float3 Color, const float Scale, const float Slope)
{
	float3 pow_in = pow(abs(clamp(Color.rgb, 0.000061f, 65504.f) / Scale), Slope);
	return (C1 + C2 * pow_in) / (1 + C3 * pow_in);
}

float4 main(PsIn In) : SV_Target
{
	float4 sceneColor = uTex0.Sample(uSampler0, In.texCoord);
	float3 resultColor = float3(0.0, 0.0, 0.0);

	if(outputMode == 0)
	{
		//SDR
		//resultColor = pow(sceneColor.rgb, 0.45454545);
		resultColor = sceneColor.rgb;
	}
	else
	{
		//HDR10

		if(UseSCurve > 0.5f)
			resultColor = L2PQ_float3(ApplyDolbySCurve(Rec709ToRec2020(sceneColor.rgb), ScurveScale, ScurveSlope));	
		else
			resultColor = L2PQ_float3( min(Rec709ToRec2020(sceneColor.rgb) * linearScale, 10000.0f));		
	}


	return float4(resultColor, 1.0);
}
