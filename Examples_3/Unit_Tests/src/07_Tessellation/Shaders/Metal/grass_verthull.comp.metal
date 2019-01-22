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

struct VSIn
{
    float4 v0   [[attribute(0)]];
    float4 v1   [[attribute(1)]];
    float4 v2   [[attribute(2)]];
    float4 up   [[attribute(3)]];
};

struct VS_CONTROL_POINT_OUTPUT {
    
    float4 position [[position]];
    float4 tesc_v1;
    float4 tesc_v2;
    
    float tesc_up_x;
    float tesc_up_y;
    float tesc_up_z;
    float tesc_up_w;
    
    float tesc_widthDir_x;
    float tesc_widthDir_y;
    float tesc_widthDir_z;
    float tesc_widthDir_w;
};

// The vertex control point function
VS_CONTROL_POINT_OUTPUT VSMain(VSIn In, constant UniformData& uniforms)
{
    VS_CONTROL_POINT_OUTPUT Out;
    
    float4 V0 = uniforms.world * float4(In.v0.xyz, 1.0);
    Out.position = V0;
    Out.position.w = In.v0.w;
    
    Out.tesc_v1 = float4((uniforms.world * float4(In.v1.xyz, 1.0)).xyz, In.v1.w);
    Out.tesc_v2 = float4((uniforms.world * float4(In.v2.xyz, 1.0)).xyz, In.v2.w);
    
    float3 up = normalize(In.up.xyz);
    
    //Out.tesc_up.xyz = normalize(In.up.xyz);
    
    Out.tesc_up_x = up.x;
    Out.tesc_up_y = up.y;
    Out.tesc_up_z = up.z;
    Out.tesc_up_w = In.up.w;
    
    
    float theta = In.v0.w;
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    
    float3 faceDir = normalize(cross(up, float3(sinTheta, 0, cosTheta)));
    float3 widthDir = normalize(cross(faceDir, up));
    
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
    float4 position [[position]];
    float4 tese_v1;
    float4 tese_v2;
    float4 tese_up;
    float4 tese_widthDir;
};

// The patch control point function
HullOut HSMain(VS_CONTROL_POINT_OUTPUT ip)
{
    HullOut Output;
    
    Output.position = float4(ip.position.xyz, 1.0);
    Output.tese_v1 = ip.tesc_v1;
    Output.tese_v2 = ip.tesc_v2;
    Output.tese_up = float4(ip.tesc_up_x, ip.tesc_up_y, ip.tesc_up_z, ip.tesc_up_w);
    Output.tese_widthDir = float4(ip.tesc_widthDir_x, ip.tesc_widthDir_y, ip.tesc_widthDir_z, ip.tesc_widthDir_w);
    
    return Output;
}

// Output patch constant data.
struct PatchTess
{
    array<half, 4> Edges;
    array<half, 2> Inside;
};

// Patch Constant Function
PatchTess ConstantsHS(HullOut ip, constant UniformData& uniforms)
{
    PatchTess Output;
    
    float4 WorldPosV0 = float4(ip.position.xyz, 1.0);
    
    float near = 0.1;
    float far = 25.0;
    
    float depth = -(uniforms.view * WorldPosV0).z / (far - near);
    depth = saturate(depth);
    
    float minLevel = 1.0;
    
    depth = depth*depth;
    
    half lvl = mix(float(uniforms.gMaxTessellationLevel), minLevel, depth);
    
    Output.Inside[0] = 1.0; //horizontal
    Output.Inside[1] = lvl; //vertical
    
    Output.Edges[0] = lvl; //vertical
    Output.Edges[1] = 1.0; //horizontal
    Output.Edges[2] = lvl; //vertical
    Output.Edges[3] = 1.0; //horizontal
    
    return Output;
}

struct BladeDrawIndirect
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

//[numthreads(32, 1, 1)]
kernel void stageMain(constant VSIn* vertexInput                  [[buffer(0)]],
                      constant UniformData& GrassUniformBlock     [[buffer(1)]],
                      constant BladeDrawIndirect& drawInfo        [[buffer(2)]],
                      device PatchTess* tessellationFactorBuffer  [[buffer(3)]],
                      device HullOut* hullOutputBuffer            [[buffer(4)]],
                      uint threadId                               [[thread_position_in_grid]])
{
    if (threadId <= drawInfo.vertexCount)
    {
        // Get a piece of vertex data for every frame and produce a control-point.
        VSIn In = vertexInput[threadId];
        VS_CONTROL_POINT_OUTPUT vOut = VSMain(In, GrassUniformBlock);
        
        // Since we only have one control-point per-patch, we can execute the per-control-point and
        // per-patch functions without need for syncing.
        HullOut hOut = HSMain(vOut);
        PatchTess patchTessOut = ConstantsHS(hOut, GrassUniformBlock);
        
        // Store the results.
        hullOutputBuffer[threadId] = hOut;
        tessellationFactorBuffer[threadId] = patchTessOut;
    }
}
