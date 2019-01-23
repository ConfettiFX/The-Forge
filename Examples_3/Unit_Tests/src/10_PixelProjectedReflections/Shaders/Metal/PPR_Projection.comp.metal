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
#include <metal_atomic>
#include <metal_compute>
using namespace metal;

#define MAX_PLANES 4

struct ExtendCameraData
{
	float4x4 viewMat;
	float4x4 projMat;
	float4x4 viewProjMat;
	float4x4 InvViewProjMat;

	float4 cameraWorldPos;
	float4 viewPortSize;
};

struct PlaneInfo
{
	float4x4 rotMat;
	float4 centerPoint;
	float4 size;
};

struct planeInfoBufferData
{
	PlaneInfo planeInfo[MAX_PLANES];
	uint numPlanes;
	uint pad00;
	uint pad01;
	uint pad02;
};

float getDistance(float3 planeNormal, float3 planeCenter, float3 worldPos)
{
	//plane to point
	float d = -dot(planeNormal, planeCenter);
	return (dot(planeNormal, worldPos) + d) / length(planeNormal);
}

float4 intersectPlane(uint index, float3 worldPos, float2 fragUV, planeInfoBufferData planeInfoBuffer, ExtendCameraData cbExtendCamera, depth2d<float> DepthTexture,sampler defaultSampler)
{
	float4 reflectedPos(0,0,0,0);
	PlaneInfo thisPlane = planeInfoBuffer.planeInfo[index];

	float3 normalVec = thisPlane.rotMat[2].xyz;	
	

	float3 centerPoint = thisPlane.centerPoint.xyz;
	float3 projectedWorldPos = dot(normalVec, worldPos - centerPoint) * normalVec;
	float3 target = worldPos - 2.0 * projectedWorldPos;

	//plane to point	
	float dist = getDistance(normalVec, centerPoint, target);
	
	//if target is on the upper-side of plane, false 
	if(dist >= 0.0)
	{
		return float4(0,0,0,-1);
	}

	float3 rO = cbExtendCamera.cameraWorldPos.xyz;
	float3 rD = normalize(target - rO);
	float3 rD_VS = float3x3(cbExtendCamera.viewMat[0].xyz,cbExtendCamera.viewMat[1].xyz,cbExtendCamera.viewMat[2].xyz) * rD;
		
	if(rD_VS.z < 0.0)
	{
		return float4(0,0,0,-1);
	}

    float denom = dot(normalVec, rD); 

    if (denom < 0.0)
	{ 
        float3 p0l0 = centerPoint - rO; 
        float t = dot(normalVec, p0l0) / denom; 

		if(t <= 0.0)
		{
			return float4(0,0,0,-1);
		}

		float3 hitPoint = rO + rD*t;

		float3 gap = hitPoint - centerPoint;
		
		float xGap = dot(gap, thisPlane.rotMat[0].xyz);
		float yGap = dot(gap, thisPlane.rotMat[1].xyz);

		float width = thisPlane.size.x * 0.5;
		float height = thisPlane.size.y * 0.5;

		if( (abs(xGap) <= width) && (abs(yGap) <= height))
		{
			reflectedPos = cbExtendCamera.viewProjMat * float4(hitPoint, 1.0);
			reflectedPos /= reflectedPos.w;

			reflectedPos.xy = float2( (reflectedPos.x + 1.0) * 0.5, (1.0 - reflectedPos.y) * 0.5);

			float depth = DepthTexture.sample(defaultSampler,reflectedPos.xy, 0);

			if(depth < reflectedPos.z)
			{
				return float4(0,0,0,-1);
			}
			
			if( reflectedPos.x < 0.0 || reflectedPos.y < 0.0  || reflectedPos.x > 1.0 || reflectedPos.y > 1.0 )
			{
				return float4(0,0,0,-1);
			}
			else
			{
				//check if it is also hit from other planes
				for(uint i=0; i <planeInfoBuffer.numPlanes; i++ )
				{
					if(i != index)
					{						
						PlaneInfo otherPlane = planeInfoBuffer.planeInfo[i];


						// assuming vectors are all normalized						
						float3 otherNormalVec = otherPlane.rotMat[2].xyz;
						float3 otherCenterPoint = otherPlane.centerPoint.xyz;

						float innerDenom = dot(otherNormalVec, rD); 

						if (innerDenom < 0.0)
						{ 
							float3 innerP0l0 = otherCenterPoint - rO; 
							float innerT = dot(otherNormalVec, innerP0l0) / innerDenom; 

							if(innerT <= 0.0)
								continue;
							else if(innerT < t)
							{
								float3 innerhitPoint = rO + rD*innerT;	
								float3 innergap = innerhitPoint - otherCenterPoint;
		
								float innerxGap = dot(innergap, otherPlane.rotMat[0].xyz);
								float inneryGap = dot(innergap, otherPlane.rotMat[1].xyz);

								float innerWidth = otherPlane.size.x * 0.5;
								float innerHeight = otherPlane.size.y * 0.5;

								// if it hits other planes
								if( (abs(innerxGap) <= innerWidth) && (abs(inneryGap) <= innerHeight))
								{
									return float4(0,0,0,-1);
								}								
							}	
						}
					}
				}

				return reflectedPos;
			}
		}	
		else
			return float4(0,0,0,-1);
    } 
	else
		return float4(0,0,0,-1);
} 

float4 getWorldPosition(float2 UV, float depth, ExtendCameraData cbExtendCamera)
{
	float4 worldPos = cbExtendCamera.InvViewProjMat * float4(UV.x * 2.0 - 1.0, (1.0 - UV.y) * 2.0 - 1.0, depth, 1.0);
	worldPos /= worldPos.w;
	return worldPos;
}

uint packInfo(float2 offset)
{
	uint CoordSys = 0;

	uint YInt = 0;
	int YFrac = 0;
	int XInt = 0;
	int XFrac = 0;

	//define CoordSystem
	if(abs(offset.y) < abs(offset.x) )
	{
		if(offset.x < 0.0) // 3
		{
			YInt = uint(-offset.x);
			YFrac = int(fract(offset.x)*8.0);
			
			XInt = int(offset.y);
			XFrac = int(fract(offset.y)*8.0);

			CoordSys = 3;
		}
		else // 1
		{
			YInt = uint(offset.x);
			YFrac = int(fract(offset.x)*8.0);
			
			XInt = int(offset.y);
			XFrac = int(fract(offset.y)*8.0);

			CoordSys = 1;
		}
	}
	else	
	{
		if(offset.y < 0.0) // 2
		{
			YInt = uint(-offset.y);
			YFrac = int(fract(offset.y)*8.0);
			
			XInt = int(offset.x);
			XFrac = int(fract(offset.x)*8.0);

			CoordSys = 2;
		}
		else // 0
		{
			YInt = uint(offset.y);
			YFrac = int(fract(offset.y)*8.0);
			
			XInt = int(offset.x);
			XFrac = int(fract(offset.x)*8.0);

			CoordSys = 0;
		}
	}

	return  ( (YInt & 0x00000fff ) << 20) | ( (YFrac & 0x00000007) << 17) | ( (XInt & 0x00000fff) << 5) | ( (XFrac & 0x00000007 )<< 2) | CoordSys;
}

//[numthreads(128,1,1)]
kernel void stageMain(uint DTid [[thread_position_in_grid]],
					  device atomic_uint * IntermediateBuffer	[[buffer(0)]],
					  constant ExtendCameraData& cbExtendCamera		[[buffer(1)]],
					  constant planeInfoBufferData& planeInfoBuffer	[[buffer(2)]],

					  depth2d<float> DepthTexture					[[texture(0)]],
                      sampler defaultSampler						[[sampler(0)]])
{	
	uint screenWidth = uint( cbExtendCamera.viewPortSize.x );
	
	uint indexDX = DTid;
		
	uint indexY = indexDX / screenWidth;
	uint indexX = indexDX - screenWidth * indexY;

	float2 fragUV = float2( ((float)indexX) / (cbExtendCamera.viewPortSize.x), ((float)indexY) / (cbExtendCamera.viewPortSize.y) );
		
	float depth = DepthTexture.sample(defaultSampler, fragUV, 0);

	//if there is no obj
	if(depth >= 0.999)
		return;

	float4 worldPos = getWorldPosition(fragUV, depth, cbExtendCamera);
	
	float4 reflectedPos = float4(0.0, 0.0, 0.0, 0.0);
	float2 reflectedUV;
	float2 offset;

	for(uint i = 0; i < planeInfoBuffer.numPlanes; i++)
	{
		reflectedPos = intersectPlane( i, worldPos.xyz, fragUV, planeInfoBuffer, cbExtendCamera,DepthTexture,defaultSampler);
		if(reflectedPos.w > 0.0f)
		{
			reflectedUV =  float2( reflectedPos.x * cbExtendCamera.viewPortSize.x, reflectedPos.y * cbExtendCamera.viewPortSize.y);
			offset = float2( (fragUV.x - reflectedPos.x) * cbExtendCamera.viewPortSize.x, ( fragUV.y - reflectedPos.y) * cbExtendCamera.viewPortSize.y);
			
			uint newIndex = (uint)reflectedUV.x + (uint)reflectedUV.y * screenWidth;

			//pack info
			{
				uint intermediateBufferValue = packInfo(offset);
				atomic_store_explicit( &IntermediateBuffer[newIndex], intermediateBufferValue, memory_order_relaxed);
			}
		}
	}
}
