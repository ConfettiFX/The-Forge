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

/*
*Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
*
*Permission is hereby granted, free of charge, to any person obtaining a copy
*of this software and associated documentation files (the "Software"), to deal
*in the Software without restriction, including without limitation the rights
*to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*copies of the Software, and to permit persons to whom the Software is
*furnished to do so, subject to the following conditions:
*
*The above copyright notice and this permission notice shall be included in
*all copies or substantial portions of the Software.
*
*THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
*AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*THE SOFTWARE.
*/

#include <metal_stdlib>
using namespace metal;

#define EPSILON 1e-7f

struct CameraData
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
	int bUseEnvironmentLight;
};

struct HairData
{
	float4x4 Transform;
	uint RootColor;
	uint StrandColor;
	float ColorBias;
	float Kd;
	float Ks1;
	float Ex1;
	float Ks2;
	float Ex2;
	float FiberRadius;
	float FiberSpacing;
	uint NumVerticesPerStrand;
};

struct GlobalHairData
{
	float4 Viewport;
	float4 Gravity;
	float4 Wind;
	float TimeStep;
};

float4 GetStrandColor(int index, uint RootColor, uint StrandColor, uint NumVerticesPerStrand, float ColorBias)
{
	float4 rootColor = float4(RootColor >> 24, (RootColor >> 16) & 0xFF, (RootColor >> 8) & 0xFF, RootColor & 0xFF) * (1.0f / 255.0f);
	float4 strandColor = float4(StrandColor >> 24, (StrandColor >> 16) & 0xFF, (StrandColor >> 8) & 0xFF, StrandColor & 0xFF) * (1.0f / 255.0f);

	float strandPos = (index % NumVerticesPerStrand) / float(NumVerticesPerStrand);
	float colorWeight = 1.0f - saturate(pow(1.0f - strandPos, ColorBias));

	return mix(rootColor, strandColor, colorWeight);
}

#ifdef HAIR_SHADOW

struct VSOutput
{
	float4 Position[[position]];
};

vertex VSOutput stageMain(uint vertexID[[vertex_id]],
    device float4* GuideHairVertexPositions[[buffer(0)]],
    device float4* GuideHairVertexTangents[[buffer(1)]],
    device float* HairThicknessCoefficients[[buffer(2)]],
    constant CameraData& cbCamera [[buffer(3)]],
    constant HairData& cbHair [[buffer(4)]])
{
	uint index = vertexID / 2;

	float3 v = GuideHairVertexPositions[index].xyz;
	float3 t = GuideHairVertexTangents[index].xyz;

	v = (cbHair.Transform * float4(v, 1.0f)).xyz;
	t = normalize((cbHair.Transform * float4(t, 0.0f)).xyz);

	float3 right = normalize(cross(t, normalize(v - cbCamera.CamPos)));

	float thickness = HairThicknessCoefficients[index];

	float4 hairEdgePositions[2];
	hairEdgePositions[0] = float4(v + -right * thickness * cbHair.FiberRadius, 1.0f);
	hairEdgePositions[1] = float4(v + right * thickness * cbHair.FiberRadius, 1.0f);
	hairEdgePositions[0] = cbCamera.CamVPMatrix *  hairEdgePositions[0];
	hairEdgePositions[1] = cbCamera.CamVPMatrix *  hairEdgePositions[1];

	VSOutput output;
	output.Position = hairEdgePositions[vertexID & 1];
	return output;
}
#else

struct VSOutput
{
	float4 Position[[position]];
	float4 Tangent;
	float4 P0P1;
	float4 Color;
	float2 W0W1;
};

vertex VSOutput stageMain(uint vertexID[[vertex_id]],
    device float4* GuideHairVertexPositions[[buffer(0)]],
    device float4* GuideHairVertexTangents[[buffer(1)]],
    device float* HairThicknessCoefficients[[buffer(2)]],
    constant CameraData& cbCamera [[buffer(3)]],
    constant HairData& cbHair [[buffer(4)]],
    constant GlobalHairData& cbHairGlobal [[buffer(5)]])
{
	uint index = vertexID / 2;

	float3 v = GuideHairVertexPositions[index].xyz;
	float3 t = GuideHairVertexTangents[index].xyz;

	v = (cbHair.Transform * float4(v, 1.0f)).xyz;
	t = normalize((cbHair.Transform * float4(t, 0.0f)).xyz);

	float3 right = normalize(cross(t, normalize(v - cbCamera.CamPos)));
	float2 projRight = normalize((cbCamera.CamVPMatrix * float4(right, 0)).xy);

	float expandPixels = 0.71f;

	float thickness = HairThicknessCoefficients[index];

	float4 hairEdgePositions[2];
	hairEdgePositions[0] = float4(v + -right * thickness * cbHair.FiberRadius, 1.0f);
	hairEdgePositions[1] = float4(v + right * thickness * cbHair.FiberRadius, 1.0f);
	hairEdgePositions[0] = cbCamera.CamVPMatrix * hairEdgePositions[0];
	hairEdgePositions[1] = cbCamera.CamVPMatrix * hairEdgePositions[1];

	float dir = (vertexID & 1) ? 1.0f : -1.0f;

	VSOutput output;
	output.Position = hairEdgePositions[vertexID & 1] + dir * float4(projRight * expandPixels / cbHairGlobal.Viewport.w, 0.0f, 0.0f) * hairEdgePositions[vertexID & 1].w;
	output.Tangent = float4(t, thickness);
	output.P0P1 = float4(hairEdgePositions[0].xy, hairEdgePositions[1].xy);
	output.Color = GetStrandColor(index, cbHair.RootColor, cbHair.StrandColor, cbHair.NumVerticesPerStrand, cbHair.ColorBias);
	output.W0W1 = float2(hairEdgePositions[0].w, hairEdgePositions[1].w);
	return output;
}
#endif
