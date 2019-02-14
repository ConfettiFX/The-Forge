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
#version 450 core

layout(early_fragment_tests) in; // critical so that stencil op has already been performed by the time we discard!

layout(location = 0) flat in vec4 WorldObjectCenterAndRadius;

layout(location = 0) out float ShadowFactorOut;

layout(set = 3, binding = 0) uniform sampler clampMiplessSampler;
layout(set = 3, binding = 2) uniform texture2D DepthBufferCopy;

layout(set = 0, binding = 0) uniform renderSettingUniformBlock
{
    vec4 WindowDimension;
};
layout(set = 1, binding = 0) uniform cameraUniformBlock
{
    mat4 View;
    mat4 Project;
    mat4 ViewProject;
    mat4 InvView;
    mat4 InvProj;
    mat4 InvViewProject;
    vec4 NDCConversionConstants[2];
    float Near;
    float FarNearDiff;
    float FarNear;
};
layout(set = 1, binding = 1) uniform lightUniformBlock
{
    mat4 lightViewProj;
    vec4 lightPosition;
    vec4 lightColor;
    vec4 lightUpVec;
    float lightRange;
};


layout(set = 2, binding = 0) uniform sdfUniformBlock
{
	float mShadowHardness;
    float mShadowHardnessRcp;
	uint mMaxIteration;
};

//sphere is centered at the origin
float sdSphereLocal(vec3 pos, float radius)
{
	return length(pos) - radius;
}

const float stepEpsilon = 0.004;

// TODO: upgrade to deal with non-uniformly scaled spheres
float closestDistLocal(in vec3 pos) 
{
    return sdSphereLocal(pos.xyz - WorldObjectCenterAndRadius.xyz, WorldObjectCenterAndRadius.w);
}

// TODO: trace in model-space against a unit sphere, will allow for SDF shadows from non-uniformly scaled spheres
float calcSdfShadowFactor(vec3 rayStart, in vec3 rayDir, in float tMin, in float tMax)
{
	float minDist = 9999999999.0;
	float t = tMin;
	for (uint i = 0; i < mMaxIteration; i++)
    {
		float h = closestDistLocal(rayStart + rayDir*t);
        if (h<minDist)
            minDist = h;
		t += h;
		if (minDist < stepEpsilon || t>tMax)
			break;
	}
	return max((minDist-stepEpsilon)*mShadowHardness,0.0);
}


float linearizeZValue(in float nonLinearValue)
{
    // Warning, reverse-Z is used!
    return FarNear/(Near+nonLinearValue*FarNearDiff);
}
vec3 reconstructPosition(in vec3 windowCoords)
{
    float linearZ = linearizeZValue(windowCoords.z);
    vec4 quasiPostProj = (NDCConversionConstants[0]+vec4(windowCoords.xy*vec2(2.0,-2.0),0.0,0.0))*linearZ;
    return (InvViewProject*quasiPostProj).xyz+NDCConversionConstants[1].xyz;
}
void main()
{
    if (gl_FrontFacing) // kill actual front faces
    {
        discard;
        //alternative solution for GPUs which violate the spec and ignore early_fragment_tests
        //albedoOut = 1.0;
        //return;
    }
    // aside from the weird bounding box rotation trick, you can SDF trace anything parametric or from 3D texture
    
    ivec2 i2uv = ivec2(gl_FragCoord.xy);
    float depth = texelFetch(sampler2D(DepthBufferCopy,clampMiplessSampler), i2uv, 0).r;
    
    vec2 f2uv = vec2(i2uv.xy) / WindowDimension.xy;
    // TODO: inverse transform these by object's world matrix and trace in model-space
    vec3 position = reconstructPosition(vec3(f2uv,depth));
    vec3 lightVec = normalize(lightPosition.xyz-position);
    
    float dif = calcSdfShadowFactor(position, lightVec, mShadowHardnessRcp, lightRange);
    ShadowFactorOut = (4.0/3.0)*dif;
}