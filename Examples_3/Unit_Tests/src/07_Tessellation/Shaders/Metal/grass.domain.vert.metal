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

struct UniformData
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
};

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

// Domain shader (vertex shader on Metal).
[[patch(quad, 1)]]
vertex DomainOut stageMain(HullOut patch                           [[stage_in]],
                           float2 UV                               [[position_in_patch]],
                           constant UniformData& GrassUniformBlock [[buffer(1)]])
{
    DomainOut Output;
    
    float2 uv = UV;
    
    //6.3 Blade Geometry
    float3 a = patch.control_points[0].position.xyz + uv.y*(patch.control_points[0].tese_v1.xyz - patch.control_points[0].position.xyz);
    float3 b = patch.control_points[0].tese_v1.xyz + uv.y*(patch.control_points[0].tese_v2.xyz - patch.control_points[0].tese_v1.xyz);
    float3 c = a + uv.y*(b - a);
    
    float3 t1 = patch.control_points[0].tese_widthDir.xyz; //bitangent
    float3 wt1 = t1 * patch.control_points[0].tese_v2.w * 0.5;
    
    float3 c0 = c - wt1;
    float3 c1 = c + wt1;
    
    float3 t0 = normalize(b - a);
    
    Output.Normal = normalize(cross(t1, t0));
    
    // Triangle shape
    float t = uv.x + 0.5*uv.y - uv.x*uv.y;
    Output.Position.xyz = (1.0 - t)*c0 + t*c1;
    Output.Position = GrassUniformBlock.viewProj * float4(Output.Position.xyz, 1.0);
    
    Output.UV.x = uv.x;
    Output.UV.y = uv.y;
    
    Output.WindDirection = patch.control_points[0].tese_widthDir.xyz;
    
    return Output;
}
