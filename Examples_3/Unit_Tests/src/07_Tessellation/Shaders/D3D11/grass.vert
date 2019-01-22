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

VS_CONTROL_POINT_OUTPUT main(VSIn In) {

	VS_CONTROL_POINT_OUTPUT Out;

	float4 V0 = mul(world, float4(In.v0.xyz, 1.0));
	Out.position = V0;

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

	//float3 faceDir = normalize(cross(float3(sinTheta, 0, cosTheta), up));	
	//float3 widthDir = normalize(cross(up, faceDir));

	float3 faceDir = normalize(cross(up, float3(sinTheta, 0, cosTheta)));
	float3 widthDir = normalize(cross(faceDir, up));
	
	Out.tesc_widthDir_x = widthDir.x;
	Out.tesc_widthDir_y = widthDir.y;
	Out.tesc_widthDir_z = widthDir.z;

	//For debug
	Out.tesc_widthDir_w = In.v1.w * 0.4;	

	return Out;
}
