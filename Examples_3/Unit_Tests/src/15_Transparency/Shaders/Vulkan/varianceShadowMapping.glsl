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
*
* Reference:
* https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch08.html
*/

layout (set=0, binding=15) uniform texture2D VSM;
layout (set=0, binding=16) uniform sampler VSMSampler;
#if PT_USE_CAUSTICS != 0
layout (set=0, binding=17) uniform texture2D VSMRed;
layout (set=0, binding=18) uniform texture2D VSMGreen;
layout (set=0, binding=19) uniform texture2D VSMBlue;
#endif

vec2 ComputeMoments(float depth)
{
	vec2 moments;
	moments.x = depth;
	vec2 pd = vec2(dFdx(depth), dFdy(depth));
	moments.y = depth * depth + 0.25f * dot(pd, pd);
	return moments;
}

float ChebyshevUpperBound(vec2 moments, float t)
{
	float p = float(t <= moments.x);
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, 0.001f);
	float d = t - moments.x;
	float pMax = variance / (variance + d * d);
	return max(p, pMax);
}

vec3 ShadowContribution(vec2 shadowMapPos, float distanceToLight)
{
	vec2 moments = texture(sampler2D(VSM, VSMSampler), shadowMapPos).xy;
	vec3 shadow = ChebyshevUpperBound(moments, distanceToLight).xxx;

#if PT_USE_CAUSTICS != 0
	moments = texture(sampler2D(VSMRed, VSMSampler), shadowMapPos).xy;
	shadow.r *= ChebyshevUpperBound(moments, distanceToLight);
	moments = texture(sampler2D(VSMGreen, VSMSampler), shadowMapPos).xy;
	shadow.g *= ChebyshevUpperBound(moments, distanceToLight);
	moments = texture(sampler2D(VSMBlue, VSMSampler), shadowMapPos).xy;
	shadow.b *= ChebyshevUpperBound(moments, distanceToLight);
#endif

	return shadow;
}