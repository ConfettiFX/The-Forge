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

///////////////////////////////////////////////////////////////////////////////
//	SDF shadow starts here
#define PI 3.141592657589793f
#define SPHERE_EACH_ROW 5 
#define SPHERE_EACH_COL 5 

struct VSOutput
{
    float4 position [[position]];
};

struct LightUniform
{
    float4x4 lightViewProj;
    float4 lightDirection;
    float4 lightColor;
};
struct SdfUniformBlock
{
 	float4x4 mViewInverse;
	float4 mCameraPosition;
	float mShadowHardness;
	uint mMaxIteration;
	float2 mWindowDimension;
	float mSphereRadius;
	float mRadsRot;
};


//sphere is centered at the origin
float sdSphereLocal(float3 pos, float radius)
{
	return length(pos) - radius;
}

//plane is xz plane
float sdPlaneLocal(float3 pos)
{
	return pos.y;
}

float3x3 AngleAxis3x3(float angle, float3 axis)
{
	float c, s;
	c = cos(angle);
	s = sin(angle);

	float t = 1 - c;
	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	return float3x3(
		t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,
		t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,
		t * x * z - s * y,  t * y * z + s * x,  t * z * z + c
	);
}

float closestDistLocal(float3 pos, float mSphereRadius, float mRadsRot)
{
	const float sphereRadius = mSphereRadius;
	const float sphereDist = 3.0f*sphereRadius;

	float result = 100000.f;

	float3 curTrans = float3(
		-sphereDist*(SPHERE_EACH_ROW - 1)/2.f,
		sphereRadius * 2.3f,
		-sphereDist*(SPHERE_EACH_COL - 1)/2.f);

	for (int i = 0; i < SPHERE_EACH_ROW; i++)
	{
		curTrans.x = (-sphereDist * (SPHERE_EACH_ROW - 1) / 2.f);

		for (int j = 0; j < SPHERE_EACH_COL; j++)
		{
			float3x3 rot = AngleAxis3x3(mRadsRot, float3(0.f, 1.f, 0.f));
			result = min(result, sdSphereLocal(pos.xyz - (curTrans * rot), sphereDist / 2.3f));

			curTrans.x += sphereDist;
		}

		curTrans.z += sphereDist;
	}

	result = min(result, sdPlaneLocal(pos.xyz)); // plane

	return result;
}

float calcSdfShadowFactor(float3 rayStart, float3 rayDir, float tMin, float tMax, uint mMaxIteration, float mShadowHardness, float mSphereRadius, float mRadsRot)
{
	float factor = 1.0f;
	float t = tMin;
	float ph = 9999.0f;
	const float hardness = mShadowHardness;
	for (uint i = 0; i < mMaxIteration; i++) {
		float h = closestDistLocal(rayStart + rayDir*t, mSphereRadius, mRadsRot);
		float y = h * h / (5.0f*ph);
		float d = sqrt(h*h - y * y);
		factor = min( factor, hardness*d/max(0.0f,t-y) );
        ph = h;

		t += h;
		if (factor < 0.0001f || t>tMax){
			break;
		}
	}
	return clamp(factor+0.2f,0.0f,1.0f);
}

float castRay(float3 rayStart, float3 rayDir, uint mMaxIteration, float mSphereRadius, float mRadsRot)
{
	float tmin = 1.0;
	float tmax = 200.0;

	const float bottomY = -0.01f;
	const float topY = mSphereRadius * 2.3f + mSphereRadius + 0.5f;

	// bounding volume
	float tp1 = (bottomY - rayStart.y) / rayDir.y;
	if (tp1>bottomY)
		tmax = min(tmax, tp1);

	float tp2 = (topY - rayStart.y) / rayDir.y;
	if (tp2>0.0) {
		if (rayStart.y>topY)
			tmin = max(tmin, tp2);
		else
			tmax = min(tmax, tp2);
	}

	float t = tmin;
	for (uint i = 0; i<mMaxIteration; i++){
		float precis = 0.0005f*t;
		float res = closestDistLocal(rayStart + rayDir * t, mSphereRadius, mRadsRot);
		if (res<precis || t>tmax) 
			break;
		t += res;
	}

	if (t>tmax) t = -1.0f;
	return t;
}

float4 renderSdf(float3 rayStart, float3 rayDir, float3 lightDir, uint mMaxIteration, float mShadowHardness, float mSphereRadius, float mRadsRot)
{
	float3 color = float3(0.0f, 0.0f, 0.0f);
	float t = castRay(rayStart, rayDir, mMaxIteration, mSphereRadius, mRadsRot);

	if (t>-0.5f)//if the ray hits an object
	{
		float3 pos = rayStart + t * rayDir;

		// material        
		float3 mat = float3(0.3f, 0.3f, 0.3f);

		// key light
		float3  lightVec = -lightDir;
		float dif = calcSdfShadowFactor(pos, lightVec, 0.01f, 3.0f, mMaxIteration, mShadowHardness, mSphereRadius, mRadsRot);

		color = mat * 4.0*dif*float3(1,1,1);
	}
	else 
	{
		color = float3(1,1,1);
	}

	return float4(color, 1);
}

// Pixel shader
fragment float4 stageMain(VSOutput input                                         [[stage_in]],
                         constant LightUniform& lightUniformBlock                [[buffer(0)]],
						 constant SdfUniformBlock& sdfUniformBlock				 [[buffer(1)]])
{
	float3 rayStart = sdfUniformBlock.mCameraPosition.xyz;
	float2 fragCoord = input.position.xy;
	
  	float mn = 213.0f/(1920.0f/sdfUniformBlock.mWindowDimension.x);
  	float2 p = (-sdfUniformBlock.mWindowDimension + 2*fragCoord)/float2(mn, -mn); 
	float3 viewDir = (sdfUniformBlock.mViewInverse * float4(normalize( float3(p.xy/3.f, 3)),0)).xyz;
	
	float4 color = renderSdf(rayStart, viewDir, normalize(lightUniformBlock.lightDirection.xyz), sdfUniformBlock.mMaxIteration ,sdfUniformBlock.mShadowHardness, sdfUniformBlock.mSphereRadius, sdfUniformBlock.mRadsRot);
	
	return color;
}

//	SDF shadow ends here                     //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////






