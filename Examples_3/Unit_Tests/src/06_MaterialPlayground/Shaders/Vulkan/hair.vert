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

#version 460 core

#define EPSILON 1e-7f

layout(set = 0, binding = 0) uniform cbCamera
{
	mat4 CamVPMatrix;
	mat4 CamInvVPMatrix;
	vec3 CamPos;
};

layout(set = 0, binding = 1) uniform cbHair
{
	mat4 Transform;
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

layout(set = 0, binding = 2) uniform cbHairGlobal
{
	vec4 Viewport;
	vec4 Gravity;
	vec4 Wind;
	float TimeStep;
};

layout(set = 0, binding = 3) buffer GuideHairVertexPositions
{
	vec4 GuideHairVertexPosition[];
};

layout(set = 0, binding = 4) buffer GuideHairVertexTangents
{
	vec4 GuideHairVertexTangent[];
};

layout(set = 0, binding = 5) buffer HairThicknessCoefficients
{
	float HairThicknessCoefficient[];
};

vec4 GetStrandColor(int index)
{
	vec4 rootColor = vec4(RootColor >> 24, (RootColor >> 16) & 0xFF, (RootColor >> 8) & 0xFF, RootColor & 0xFF) * (1.0f / 255.0f);
	vec4 strandColor = vec4(StrandColor >> 24, (StrandColor >> 16) & 0xFF, (StrandColor >> 8) & 0xFF, StrandColor & 0xFF) * (1.0f / 255.0f);

	float strandPos = (index % NumVerticesPerStrand) / float(NumVerticesPerStrand);
	float colorWeight = 1.0f - clamp(pow(1.0f - strandPos, ColorBias), 0.0f, 1.0f);

	return mix(rootColor, strandColor, colorWeight);
}

#ifdef HAIR_SHADOW
void main()
{
	uint index = gl_VertexIndex / 2;

	vec3 v = GuideHairVertexPosition[index].xyz;
	vec3 t = GuideHairVertexTangent[index].xyz;

	v = (Transform * vec4(v, 1.0f)).xyz;
	t = normalize((Transform * vec4(t, 0.0f)).xyz);

	vec3 right = normalize(cross(t, normalize(v - CamPos)));
	vec2 projRight = normalize((CamVPMatrix * vec4(right, 0)).xy);

	float thickness = HairThicknessCoefficient[index];

	vec4 hairEdgePositions[2];
	hairEdgePositions[0] = vec4(v + -right * thickness * FiberRadius, 1.0f);
	hairEdgePositions[1] = vec4(v + right * thickness * FiberRadius, 1.0f);
	hairEdgePositions[0] = CamVPMatrix * hairEdgePositions[0];
	hairEdgePositions[1] = CamVPMatrix * hairEdgePositions[1];

	gl_Position = hairEdgePositions[gl_VertexIndex & 1];
}
#else
layout(location = 0) out vec4 Tangent;
layout(location = 1) out vec4 P0P1;
layout(location = 2) out vec4 Color;
layout(location = 3) out vec2 W0W1;

void main()
{
	uint index = gl_VertexIndex / 2;

	vec3 v = GuideHairVertexPosition[index].xyz;
	vec3 t = GuideHairVertexTangent[index].xyz;

	v = (Transform * vec4(v, 1.0f)).xyz;
	t = normalize((Transform * vec4(t, 0.0f)).xyz);

	vec3 right = normalize(cross(t, normalize(v - CamPos)));
	vec2 projRight = normalize((CamVPMatrix * vec4(right, 0)).xy);

	float expandPixels = 0.71f;

	float thickness = HairThicknessCoefficient[index];

	vec4 hairEdgePositions[2];
	hairEdgePositions[0] = vec4(v + -right * thickness * FiberRadius, 1.0f);
	hairEdgePositions[1] = vec4(v + right * thickness * FiberRadius, 1.0f);
	hairEdgePositions[0] = CamVPMatrix * hairEdgePositions[0];
	hairEdgePositions[1] = CamVPMatrix * hairEdgePositions[1];

	float dir = (gl_VertexIndex & 1) != 0 ? 1.0f : -1.0f;

	gl_Position = hairEdgePositions[gl_VertexIndex & 1] + dir * vec4(projRight * expandPixels / Viewport.w, 0.0f, 0.0f) * hairEdgePositions[gl_VertexIndex & 1].w;
	Tangent = vec4(t, thickness);
	P0P1 = vec4(hairEdgePositions[0].xy, hairEdgePositions[1].xy);
	Color = GetStrandColor(int(index));
	W0W1 = vec2(hairEdgePositions[0].w, hairEdgePositions[1].w);
}
#endif