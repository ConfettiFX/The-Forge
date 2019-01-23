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

cbuffer GrassUniformBlock : register(b0, space0) {

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

struct HullOut {

    float4 position : POSITION;
    float4 tese_v1 : NORMAL0;
    float4 tese_v2 : NORMAL1;
    float4 tese_up : NORMAL2;
    float4 tese_widthDir : NORMAL3;
};

struct PatchTess {

    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;
};

struct DS_OUTPUT {

    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float3 WindDirection : BINORMAL;
    float2 UV : TEXCOORD;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
DS_OUTPUT main(PatchTess input, float2 UV : SV_DomainLocation, OutputPatch<HullOut, 1> patch) {

    DS_OUTPUT Output;

    float2 uv = UV;

    //float4x4 viewProj = mul(proj, view);

    float3 a = (((patch[0]).position).xyz + ((uv).y * (((patch[0]).tese_v1).xyz - ((patch[0]).position).xyz)));
    float3 b = (((patch[0]).tese_v1).xyz + ((uv).y * (((patch[0]).tese_v2).xyz - ((patch[0]).tese_v1).xyz)));
    float3 c = (a + ((uv).y * (b - a)));

    float3 t1 = ((patch[0]).tese_widthDir).xyz;
    float3 wt1 = ((t1 * ((patch[0]).tese_v2).w) * 0.500000);

    float3 c0 = (c - wt1);
    float3 c1 = (c + wt1);

    float3 t0 = normalize((b - a));

    ((Output).Normal = normalize(cross(t1, t0)));

    float t = (((uv).x + (0.500000 * (uv).y)) - ((uv).x * (uv).y));
    (((Output).Position).xyz = (((1.000000 - t) * c0) + (t * c1)));
    ((Output).Position = mul(viewProj, float4(((Output).Position).xyz, 1.000000)));

    (((Output).UV).x = (uv).x);
    (((Output).UV).y = (uv).y);

    ((Output).WindDirection = ((patch[0]).tese_widthDir).xyz);

    return Output;
}
