/*
* Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#define SPECULAR_EXP 10.0f

#include "shaderDefs.h"

#if USE_SHADOWS != 0
#include "varianceShadowMapping.h"
#endif

float4 Shade(uint matID, float2 uv, float3 worldPos, float3 normal)
{
	float nDotl = dot(normal, -lightDirection.xyz);
	Material mat = Materials[matID];
	float4 matColor = mat.TextureFlags & 1 ? MaterialTextures[mat.AlbedoTexID].Sample(LinearSampler, uv) : mat.Color;
	float3 viewVec = normalize(worldPos - camPosition.xyz);
	if (nDotl < 0.05f)
		nDotl = 0.05f;//set as ambient color
	float3 diffuse = lightColor.xyz * matColor.xyz * nDotl;
	float3 specular = lightColor.xyz * pow(saturate(dot(reflect(-lightDirection.xyz, normal), viewVec)), SPECULAR_EXP);
	float3 finalColor = saturate(diffuse + specular * 0.5f);

#if USE_SHADOWS != 0
	float4 shadowMapPos = mul(lightViewProj, float4(worldPos, 1.0f));
	shadowMapPos.y = -shadowMapPos.y;
	shadowMapPos.xy = (shadowMapPos.xy + 1.0f) * 0.5f;
	if (clamp(shadowMapPos.x, 0.01f, 0.99f) == shadowMapPos.x &&
		clamp(shadowMapPos.y, 0.01f, 0.99f) == shadowMapPos.y &&
		shadowMapPos.z > 0.0f)
	{
		float3 lighting = ShadowContribution(shadowMapPos.xy, shadowMapPos.z);
		finalColor *= lighting;
	}
#endif

	return float4(finalColor, matColor.a);
}