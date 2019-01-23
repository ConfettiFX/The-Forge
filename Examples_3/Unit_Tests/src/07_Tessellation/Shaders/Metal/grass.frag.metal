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

struct ControlPoint
{
    float4 position         [[attribute(0)]];
    float4 tese_v1          [[attribute(1)]];
    float4 tese_v2          [[attribute(2)]];
    float4 tese_up          [[attribute(3)]];
    float4 tese_widthDir    [[attribute(4)]];
};

struct HullOut {
    patch_control_point<ControlPoint> control_points;
};

struct DomainOut {
    float4 Position [[position]];
    float3 Normal;
    float3 WindDirection;
    float2 UV;
};

fragment float4 stageMain(DomainOut In [[stage_in]])
{
	float3 upperColor = float3(0.0,0.9,0.1);
	float3 lowerColor = float3(0.0,0.2,0.1);

	float3 sunDirection = normalize(float3(-1.0, 5.0, -3.0));

	float NoL = clamp(dot(In.Normal, sunDirection), 0.1, 1.0);

	float3 mixedColor = mix(lowerColor, upperColor, In.UV.y);
	
	return float4(mixedColor*NoL, 1.0);
}
