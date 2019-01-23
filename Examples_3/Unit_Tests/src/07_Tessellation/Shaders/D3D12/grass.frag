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

struct DS_OUTPUT
{
	float4 Position: SV_Position;
	float3 Normal: NORMAL;
	float3 WindDirection: BINORMAL;
	float2 UV: TEXCOORD;
};

float4 main(DS_OUTPUT In) : SV_Target
{
	float3 upperColor = float3(0.0,0.9,0.1);
	float3 lowerColor = float3(0.0,0.2,0.1);

	float3 sunDirection = normalize(float3(-1.0, 5.0, -3.0));

	float NoL = clamp(dot(In.Normal, sunDirection), 0.1, 1.0);

	float3 mixedColor = lerp(lowerColor, upperColor, In.UV.y);

	
	return float4(mixedColor*NoL, 1.0);


}
