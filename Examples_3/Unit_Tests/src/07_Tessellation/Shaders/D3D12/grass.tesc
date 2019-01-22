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

cbuffer GrassUniformBlock : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 invView;
	float4x4 proj;
	float4x4 viewProj;

	float deltaTime;
	float totalTime;

	int gWindMode;
	int gMaxTessellationLevel;

	float windSpeed;
	float windWidth;
	float windStrength;
}

struct VS_CONTROL_POINT_OUTPUT {

	float4 position: POSITION;
	float4 tesc_v1: COLOR;
	float4 tesc_v2: NORMAL;

	float tesc_up_x: TESSFACTOR0;
	float tesc_up_y : TESSFACTOR1;
	float tesc_up_z : TESSFACTOR2;
	float tesc_up_w : TESSFACTOR3;

	float tesc_widthDir_x : TESSFACTOR4;
	float tesc_widthDir_y : TESSFACTOR5;
	float tesc_widthDir_z : TESSFACTOR6;
	float tesc_widthDir_w : TESSFACTOR7;
	
};

// Output control point
struct HullOut
{
	float4 position : POSITION;
	float4 tese_v1: NORMAL0;
	float4 tese_v2: NORMAL1;
	float4 tese_up: NORMAL2;
	float4 tese_widthDir: NORMAL3;
};

// Output patch constant data.
struct PatchTess
{
	float Edges[4]        : SV_TessFactor;
	float Inside[2]       : SV_InsideTessFactor;	
};

// Patch Constant Function
PatchTess ConstantsHS(InputPatch<VS_CONTROL_POINT_OUTPUT, 1> ip, uint PatchID : SV_PrimitiveID)
{
	PatchTess Output;
	
	float4 WorldPosV0 = ip[PatchID].position; //float4(ip[PatchID].position.xyz, 1.0);

	float near = 0.1;
	float far = 25.0;
	
	float depth = -(mul(view, WorldPosV0)).z / (far - near);
	depth = saturate(depth);

	float minLevel = 1.0;

	depth = depth*depth;

	float level = lerp(float(gMaxTessellationLevel), minLevel, depth);	
	
	Output.Inside[0] = 1.0; //horizontal
	Output.Inside[1] = level; //vertical

	Output.Edges[0] = level; //vertical
	Output.Edges[1] = 1.0; //horizontal
	Output.Edges[2] = level; //vertical
	Output.Edges[3] = 1.0; //horizontal
	
	return Output;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(1)]
[patchconstantfunc("ConstantsHS")]
[maxtessfactor(10.0f)]
HullOut main(
	InputPatch<VS_CONTROL_POINT_OUTPUT, 1> ip,
	uint i : SV_OutputControlPointID)
{
	HullOut Output;

	uint patchIndex = i;

	Output.position = ip[patchIndex].position; //float4(ip[patchIndex].position.xyz, 1.0);
	Output.tese_v1 = ip[patchIndex].tesc_v1;
	Output.tese_v2 = ip[patchIndex].tesc_v2;
	Output.tese_up = float4(ip[patchIndex].tesc_up_x, ip[patchIndex].tesc_up_y, ip[patchIndex].tesc_up_z, ip[patchIndex].tesc_up_w);
	Output.tese_widthDir = float4(ip[patchIndex].tesc_widthDir_x, ip[patchIndex].tesc_widthDir_y, ip[patchIndex].tesc_widthDir_z, ip[patchIndex].tesc_widthDir_w);
	

	return Output;
}
