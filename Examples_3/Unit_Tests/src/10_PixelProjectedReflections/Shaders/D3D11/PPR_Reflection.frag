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

#define UINT_MAX (uint)4294967295
#define FLT_MAX  3.402823466e+38F
#define NUM_SAMPLE 1

#define MAX_PLANES 4

#define PI 3.1415926535897932384626422832795028841971
#define RADIAN 0.01745329251994329576923690768489

cbuffer cbExtendCamera : register(b0) {
	float4x4 viewMat;
	float4x4 projMat;
	float4x4 viewProjMat;
	float4x4 InvViewProjMat;

	float4 cameraWorldPos;
	float4 viewPortSize;
}


Texture2D SceneTexture : register(t1);
RWStructuredBuffer<uint> IntermediateBuffer : register(u2);

//Texture2D<uint> IntermediateBuffer : register(t2);

Texture2D DepthTexture : register(t3);

struct PlaneInfo
{
	float4x4 rotMat;
	float4 centerPoint;
	float4 size;
};

cbuffer planeInfoBuffer : register(b4) {
	PlaneInfo planeInfo[MAX_PLANES];
	uint numPlanes;
	uint pad00;
	uint pad01;
	uint pad02;
}

cbuffer cbProperties : register(b5) {

	uint renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;
	float useNormalMap;

	float intensity;
	float useFadeEffect;
	float padding01;
	float padding02;
}

SamplerState defaultSampler : register(s6);


bool intersectPlane(in uint index, in float3 worldPos, in float2 fragUV, out float4 hitPos, out float2 relfectedUVonPlanar) 
{ 
	
	//relfectedUVonPlanar = float2(0.0, 0.0);

	PlaneInfo thisPlane = planeInfo[index];

	// assuming vectors are all normalized

	//this is just for DX12, why it is transposed?
	float4x4 thisPlanesMat = thisPlane.rotMat;
	thisPlanesMat = transpose(thisPlanesMat);

	float3 normalVec = thisPlane.rotMat[2].xyz;	
	//float3 normalVec = thisPlanesMat[2].xyz;

	

	float3 centerPoint = thisPlane.centerPoint.xyz;

	float3 rO = cameraWorldPos.xyz;
	float3 rD = normalize(worldPos - rO);
	float3 rD_VS = mul((float3x3)viewMat, rD);
	

	//hitPos = float4(  normalVec, 0.0);

	if(rD_VS.z < 0.0)
		return false;	

    float denom = dot(normalVec, rD); 

    if (denom < 0.0)
	{ 
		

        float3 p0l0 = centerPoint - rO; 
        float t = dot(normalVec, p0l0) / denom; 

		if(t <= 0.0)
			return false;

		float3 hitPoint = rO + rD*t;	

		float3 gap = hitPoint - centerPoint;
		
		float xGap = dot(gap, thisPlane.rotMat[0].xyz);
		float yGap = dot(gap, thisPlane.rotMat[1].xyz);

		//float xGap = dot(gap, thisPlanesMat[0].xyz);
		//float yGap = dot(gap, thisPlanesMat[1].xyz);


		float width = thisPlane.size.x * 0.5;
		float height = thisPlane.size.y * 0.5;

		float4 reflectedPos;

		if( (abs(xGap) <= width) && (abs(yGap) <= height))
		{
			hitPos = float4(hitPoint, 1.0);
			reflectedPos = mul(viewProjMat, hitPos);
			reflectedPos /= reflectedPos.w;

			reflectedPos.xy = float2( (reflectedPos.x + 1.0) * 0.5, (1.0 - reflectedPos.y) * 0.5);

			float depth = DepthTexture.Sample(defaultSampler, reflectedPos.xy).r;

			if(depth <= reflectedPos.z)
				return false;
			
			if( reflectedPos.x < 0.0 || reflectedPos.y < 0.0  || reflectedPos.x > 1.0 || reflectedPos.y > 1.0 )
				return false;
			else
			{
				relfectedUVonPlanar = float2(xGap / width, yGap / height) * 0.5 + float2(0.5, 0.5);
				relfectedUVonPlanar *= float2(thisPlane.size.zw);

				return true; 
			}			
		}	
		else
			return false;
    } 
	else	
		return false; 
} 

float4 unPacked(in uint unpacedInfo, in float2 dividedViewSize, out uint CoordSys)
{
	float YInt = float(unpacedInfo >> 20);
	int YFrac = int( (unpacedInfo & 0x000E0000) >> 17 );
	
	uint uXInt = (unpacedInfo & 0x00010000) >> 16;

	float XInt = 0.0;

	if(uXInt == 0)
	{
		XInt = float( int(  (unpacedInfo & 0x0001FFE0) >> 5 ));
	}
	else
	{
		XInt = float(int( ((unpacedInfo & 0x0001FFE0) >> 5) | 0xFFFFF000));
	}
	
	int XFrac = int( (unpacedInfo & 0x0000001C) >> 2 );

	float Yfrac = YFrac * 0.125;
	float Xfrac = XFrac * 0.125;
	
	CoordSys = unpacedInfo & 0x00000003;

	float2 offset = float2(0.0, 0.0);

	if(CoordSys == 0)
	{
		offset = float2( (XInt) / dividedViewSize.x, (YInt)  / dividedViewSize.y);
		//offset = float2(XInt, YInt);
	}
	else if(CoordSys == 1)
	{
		offset = float2( (YInt) / dividedViewSize.x, (XInt) / dividedViewSize.y);
		//offset = float2(0.0, 1.0);
	}
	else if(CoordSys == 2)
	{
		offset = float2( (XInt) / dividedViewSize.x, -(YInt) / dividedViewSize.y);
		//offset = float2(0.5, 0.5);
	}
	else if(CoordSys == 3)
	{
		offset = float2( -(YInt) / dividedViewSize.x, (XInt) / dividedViewSize.y);
		//offset = float2(1.0, 1.0);
	}

	return float4(offset, Xfrac, Yfrac);
}

float4 getWorldPosition(float2 UV, float depth)
{
	float4 worldPos = mul( InvViewProjMat, float4(UV.x*2.0 - 1.0, (1.0 - UV.y) * 2.0 - 1.0, depth, 1.0));
	worldPos /= worldPos.w;

	return worldPos;
}

float fade(float2 UV)
{
	float2 NDC = UV * 2.0 - float2(1.0, 1.0);

	return clamp( 1.0 - max( pow( NDC.y * NDC.y, 4.0) , pow( NDC.x * NDC.x, 4.0)) , 0.0, 1.0); 
}

struct VSOutput {
	float4 Position : SV_POSITION;	
	float2 uv:    TEXCOORD0;
};


float4 main(VSOutput input) : SV_TARGET
{	
	float4 outColor =  float4(0.0, 0.0, 0.0, 0.0);
	
	uint screenWidth = uint( viewPortSize.x );
	uint screenHeight = uint( viewPortSize.y );
	
	uint indexY = uint( input.uv.y * viewPortSize.y );
	uint indexX = uint( input.uv.x * viewPortSize.x );
	
	uint index = indexY * screenWidth +  indexX;

	uint bufferInfo = IntermediateBuffer[index];	
	//InterlockedAdd(IntermediateBuffer[index], 0, bufferInfo);

	bool bIsInterect = false;

	uint CoordSys;
	float2 offset = unPacked( bufferInfo , viewPortSize.xy, CoordSys).xy;

	float depth =  DepthTexture.Sample(defaultSampler, input.uv).r;

	float4 worldPos = getWorldPosition(input.uv, depth);

	float4 HitPos_WS;
	float2 UVforNormalMap = float2(0.0, 0.0);

	float4 minHitPos_WS;
	float2 minUVforNormalMap = UVforNormalMap;
	
	bool bUseNormal = useNormalMap > 0.5 ? true : false;

	float minDist = 1000000.0;

	
	int shownedReflector = -1;


	//Check if current pixel is in the bound of planars
	for(uint i = 0; i < numPlanes; i++)
	{	
		if(intersectPlane( i, worldPos.xyz, input.uv, HitPos_WS, UVforNormalMap))
		{
			float localDist =  distance(HitPos_WS.xyz, cameraWorldPos.xyz);
			if( localDist <  minDist )
			{
				minDist = localDist;
				minHitPos_WS = HitPos_WS;
				minUVforNormalMap = UVforNormalMap;
				shownedReflector = i;
			}
			bIsInterect = true;
		}
	}
	

	//If is not in the boundary of planar, exit
	if(!bIsInterect)
	{
		//clear IntermediateBuffer
		InterlockedMax(IntermediateBuffer[ index ] , UINT_MAX );
		return outColor;
	}	
		

	float2 relfectedUV = input.uv + offset.xy;

	float offsetLen = FLT_MAX;

	if( bufferInfo < UINT_MAX)
	{
		//values correction
		float correctionPixel = 1.0;

		if(CoordSys == 0)
			relfectedUV = relfectedUV.xy + float2(0.0, correctionPixel/viewPortSize.y);
		else if(CoordSys == 1)
			relfectedUV = relfectedUV.xy + float2(correctionPixel/viewPortSize.x, 0.0);
		else if(CoordSys == 2)
			relfectedUV = relfectedUV.xy - float2(0.0, correctionPixel/viewPortSize.y);
		else if(CoordSys == 3)
			relfectedUV = relfectedUV.xy - float2(correctionPixel/viewPortSize.x, 0.0);

		offsetLen = length(offset.xy);
	
		outColor = SceneTexture.SampleLevel(defaultSampler, relfectedUV, 0);
		
		if(useFadeEffect > 0.5 )
			outColor *= fade(relfectedUV);
	
		outColor.w = offsetLen;	
		
	}
	else
	{
		outColor.w = FLT_MAX;
	}

	//clear IntermediateBuffer	
	InterlockedMax(IntermediateBuffer[ index ] , UINT_MAX);
	return outColor;
}
