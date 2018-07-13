/*
* Copyright (c) 2018 Confetti Interactive Inc.
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

float closestDistLocal(float3 pos) 
{
	const float sphereRadius = 0.3f;
	const float sphereDist = 2.0f*sphereRadius;
	float result = min(sdPlaneLocal(pos.xyz - float3(0, 0.0f, 0)),//plane transformation
	  	sdSphereLocal(pos.xyz - float3(0, 0.29f, 0.0f), sphereRadius));//sphere translation and scale
	  
	for (int i = 0; i < SPHERE_EACH_ROW; i++){
	  for (int j = 0; j < SPHERE_EACH_COL; j++){
	    result = min(result, sdSphereLocal(pos.xyz - float3(sphereDist*i, 0.29f, sphereDist*j), sphereRadius));
	  }
	}
	return result;
}

float calcSdfShadowFactor(float3 rayStart, float3 rayDir, float tMin, float tMax, uint mMaxIteration, float mShadowHardness)
{
	float factor = 1.0f;
	float t = tMin;
	float ph = 9999.0f;
	const float hardness = mShadowHardness;
	for (uint i = 0; i < mMaxIteration; i++) {
		float h = closestDistLocal(rayStart + rayDir*t);
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
float castRay(float3 rayStart, float3 rayDir, uint mMaxIteration)
{
	float tmin = 1.0f;
	float tmax = 20.0f;

	// bounding volume
	float tp1 = (0.0f - rayStart.y) / rayDir.y; 
	if (tp1>0.0f) 
		tmax = min(tmax, tp1);
	float tp2 = (1.0f - rayStart.y) / rayDir.y; 
	if (tp2>0.0f) {
		if (rayStart.y>1.0f) 
			tmin = max(tmin, tp2);
		else           
			tmax = min(tmax, tp2);
	}

	float t = tmin;
	for (uint i = 0; i<mMaxIteration; i++){
		float precis = 0.0005f*t;
		float res = closestDistLocal(rayStart + rayDir * t);
		if (res<precis || t>tmax) 
			break;
		t += res;
	}

	if (t>tmax) t = -1.0f;
	return t;
}

float4 renderSdf(float3 rayStart, float3 rayDir, float3 lightDir, uint mMaxIteration, float mShadowHardness)
{
	float3 color = float3(0.0f, 0.0f, 0.0f);
	float t = castRay(rayStart, rayDir, mMaxIteration);

	if (t>-0.5f)//if the ray hits an object
	{
		float3 pos = rayStart + t * rayDir;

		// material        
		float3 mat = float3(0.3f, 0.3f, 0.3f);

		// key light
		float3  lightVec = -lightDir;
		float dif = calcSdfShadowFactor(pos, lightVec, 0.01f, 3.0f, mMaxIteration, mShadowHardness);

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
	
	float4 color = renderSdf(rayStart, viewDir, normalize(lightUniformBlock.lightDirection.xyz), sdfUniformBlock.mMaxIteration ,sdfUniformBlock.mShadowHardness);
	
	return color;
}

//	SDF shadow ends here                     //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////






