/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

struct VSIn {	
	float4 v0: TEXCOORD0;
	float4 v1: TEXCOORD1;
	float4 v2: TEXCOORD2;
	float4 up: TEXCOORD3;
};

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

VS_CONTROL_POINT_OUTPUT VSMain(VSIn In) {

	VS_CONTROL_POINT_OUTPUT Out;

	float4 V0 = mul(world, float4(In.v0.xyz, 1.0));
	Out.position = V0;
	Out.position.w = In.v0.w;

	Out.tesc_v1 = float4( mul(world, float4(In.v1.xyz, 1.0)).xyz, In.v1.w);
	Out.tesc_v2 = float4( mul(world, float4(In.v2.xyz, 1.0)).xyz, In.v2.w);

	float3 up = normalize(In.up.xyz);

	//Out.tesc_up.xyz = normalize(In.up.xyz);

	Out.tesc_up_x = up.x;
	Out.tesc_up_y = up.y;
	Out.tesc_up_z = up.z;
	Out.tesc_up_w = In.up.w;

	
	float theta = In.v0.w;
	float sinTheta = sin(theta);
	float cosTheta = cos(theta);

	float3 faceDir = normalize(cross(float3(sinTheta, 0, cosTheta), up));	
	float3 widthDir = normalize(cross(up, faceDir));

	//float3 faceDir = normalize(cross(up, float3(sinTheta, 0, cosTheta)));
	//float3 widthDir = normalize(cross(faceDir, up));
	
	Out.tesc_widthDir_x = widthDir.x;
	Out.tesc_widthDir_y = widthDir.y;
	Out.tesc_widthDir_z = widthDir.z;

	//For debug
	Out.tesc_widthDir_w = In.v1.w * 0.4;	

	return Out;
}

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
	
	float4 WorldPosV0 = float4(ip[PatchID].position.xyz, 1.0);

	float near = 0.1;
	float far = 25.0;
	
	float depth = (mul(view, WorldPosV0)).z / (far - near);
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
HullOut HSMain(
	InputPatch<VS_CONTROL_POINT_OUTPUT, 1> ip,
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID)
{
	HullOut Output;

	uint patchIndex = i;

	Output.position = float4(ip[patchIndex].position.xyz, 1.0);
	Output.tese_v1 = ip[patchIndex].tesc_v1;
	Output.tese_v2 = ip[patchIndex].tesc_v2;
	Output.tese_up = float4(ip[patchIndex].tesc_up_x, ip[patchIndex].tesc_up_y, ip[patchIndex].tesc_up_z, ip[patchIndex].tesc_up_w);
	Output.tese_widthDir = float4(ip[patchIndex].tesc_widthDir_x, ip[patchIndex].tesc_widthDir_y, ip[patchIndex].tesc_widthDir_z, ip[patchIndex].tesc_widthDir_w);
	

	return Output;
}


struct DS_OUTPUT
{
	float4 Position: SV_Position;
	float3 Normal: NORMAL;
	float3 WindDirection: BINORMAL;
	float2 UV: TEXCOORD;
};

[domain("quad")]
DS_OUTPUT DSMain(
	PatchTess input,
	float2 UV : SV_DomainLocation,
	const OutputPatch<HullOut, 1> patch)
{
	DS_OUTPUT Output;

	float2 uv = UV;

	float4x4 viewProj = mul(proj, view);

	//6.3 Blade Geometry		
	float3 a = patch[0].position.xyz + uv.y*(patch[0].tese_v1.xyz - patch[0].position.xyz);
	float3 b = patch[0].tese_v1.xyz + uv.y*(patch[0].tese_v2.xyz - patch[0].tese_v1.xyz);
	float3 c = a + uv.y*(b - a);

	float3 t1 = patch[0].tese_widthDir.xyz; //bitangent
	float3 wt1 = t1 * patch[0].tese_v2.w * 0.5;

	float3 c0 = c - wt1;
	float3 c1 = c + wt1;

	float3 t0 = normalize(b - a);	
	
	Output.Normal =  normalize(cross(t0, t1));
	//Output.Normal = normalize(cross(t1, t0));
	

	//triagnle shape
	float t = uv.x + 0.5*uv.y - uv.x*uv.y;
	Output.Position.xyz = (1.0 - t)*c0 + t*c1;
	Output.Position = mul(viewProj, float4(Output.Position.xyz, 1.0));

	Output.UV.x = uv.x;
	Output.UV.y = uv.y;

	Output.WindDirection = patch[0].tese_widthDir.xyz;


	return Output;
}


float4 PSMain(DS_OUTPUT In) : SV_Target
{
	float3 upperColor = float3(0.0,0.9,0.1);
	float3 lowerColor = float3(0.0,0.2,0.1);

	float3 sunDirection = normalize(float3(-1.0, 5.0, -3.0));

	float NoL = clamp(dot(In.Normal, sunDirection), 0.1, 1.0);

	float3 mixedColor = lerp(lowerColor, upperColor, In.UV.y);

	
	return float4(mixedColor*NoL, 1.0);


}
