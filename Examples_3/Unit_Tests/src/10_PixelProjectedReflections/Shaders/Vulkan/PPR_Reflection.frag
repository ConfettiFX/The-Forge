#version 450 core

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

#define UINT_MAX 4294967295
#define FLT_MAX  3.402823466e+38F
#define NUM_SAMPLE 1

#define MAX_PLANES 4

#define PI 3.1415926535897932384626422832795028841971
#define RADIAN 0.01745329251994329576923690768489

layout(std140, set = 0, binding = 0) uniform cbExtendCamera
{
	uniform mat4 viewMat;
	uniform mat4 projMat;
	uniform mat4 viewProjMat;
	uniform mat4 InvViewProjMat;

	uniform vec4 cameraWorldPos;
	uniform vec4 viewPortSize;
};

layout(set = 0,binding = 1) uniform texture2D SceneTexture;
layout(std430, set = 0, binding = 2)restrict buffer IntermediateBuffer {
 	 uint Data[];
};

layout(set = 0, binding = 3) uniform texture2D DepthTexture;

struct PlaneInfo
{
	mat4 rotMat;
	vec4 centerPoint;
	vec4 size;
};

layout(set = 0, binding = 4) uniform planeInfoBuffer
{	
	PlaneInfo planeInfo[MAX_PLANES];
	uint numPlanes;
	uint pad00;
	uint pad01;
	uint pad02;
};

layout(set = 0, binding = 5) uniform cbProperties
{
	uint renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;
	float useNormalMap;

	float intensity;
	float useFadeEffect;
	float padding01;
	float padding02;
};

layout(set = 0, binding = 6) uniform sampler defaultSampler;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

bool intersectPlane(in uint index, in vec3 worldPos, in vec2 fragUV, out vec4 hitPos, out vec2 relfectedUVonPlanar) 
{ 
	PlaneInfo thisPlane = planeInfo[index];

	// assuming vectors are all normalized
	vec3 normalVec = thisPlane.rotMat[2].xyz;	
	vec3 centerPoint = thisPlane.centerPoint.xyz;

	vec3 rO = cameraWorldPos.xyz;
	vec3 rD = normalize(worldPos - rO);
	vec3 rD_VS = mat3(viewMat) * rD;
	
	if(rD_VS.z < 0.0)
		return false;	

    float denom = dot(normalVec, rD); 

    if (denom < 0.0)
	{ 
        vec3 p0l0 = centerPoint - rO; 
        float t = dot(normalVec, p0l0) / denom; 

		if(t <= 0.0)
			return false;

		vec3 hitPoint = rO + rD*t;	

		vec3 gap = hitPoint - centerPoint;
		
		float xGap = dot(gap, thisPlane.rotMat[0].xyz);
		float yGap = dot(gap, thisPlane.rotMat[1].xyz);

		float width = thisPlane.size.x * 0.5;
		float height = thisPlane.size.y * 0.5;

		vec4 reflectedPos;

		if( (abs(xGap) <= width) && (abs(yGap) <= height))
		{
			hitPos = vec4(hitPoint, 1.0);
			reflectedPos = viewProjMat * hitPos;
			reflectedPos /= reflectedPos.w;

			reflectedPos.xy = vec2( (reflectedPos.x + 1.0) * 0.5, (1.0 - reflectedPos.y) * 0.5);

			float depth = texture(  sampler2D(DepthTexture, defaultSampler), reflectedPos.xy).r;	

			if(depth <= reflectedPos.z)
				return false;
			
			if( reflectedPos.x < 0.0 || reflectedPos.y < 0.0  || reflectedPos.x > 1.0 || reflectedPos.y > 1.0 )
				return false;
			else
			{
				relfectedUVonPlanar = vec2(xGap / width, yGap / height) * 0.5 + vec2(0.5);
				relfectedUVonPlanar *= vec2(thisPlane.size.zw);

				return true; 
			}			
		}	
		else
			return false;
    } 
	else	
		return false; 
} 

vec4 unPacked(in uint unpacedInfo, in vec2 dividedViewSize, out uint CoordSys)
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

	vec2 offset = vec2(0.0);

	if(CoordSys == 0)
	{
		offset = vec2( (XInt) / dividedViewSize.x, (YInt)  / dividedViewSize.y);
		//offset = vec2(XInt, YInt);
	}
	else if(CoordSys == 1)
	{
		offset = vec2( (YInt) / dividedViewSize.x, (XInt) / dividedViewSize.y);
		//offset = vec2(0.0, 1.0);
	}
	else if(CoordSys == 2)
	{
		offset = vec2( (XInt) / dividedViewSize.x, -(YInt) / dividedViewSize.y);
		//offset = vec2(0.5, 0.5);
	}
	else if(CoordSys == 3)
	{
		offset = vec2( -(YInt) / dividedViewSize.x, (XInt) / dividedViewSize.y);
		//offset = vec2(1.0, 1.0);
	}

	return vec4(offset, Xfrac, Yfrac);
}

vec4 getWorldPosition(vec2 UV, float depth)
{
	vec4 worldPos = InvViewProjMat * vec4(UV.x*2.0 - 1.0, (1.0 - UV.y) * 2.0 - 1.0, depth, 1.0);
	worldPos /= worldPos.w;

	return worldPos;
}

float fade(vec2 UV)
{
	vec2 NDC = UV * 2.0 - vec2(1.0);

	return clamp( 1.0 - max( pow( NDC.y * NDC.y, 4.0) , pow( NDC.x * NDC.x, 4.0)) , 0.0, 1.0); 
}


void main()
{	
	outColor =  vec4(0.0);
	
	ivec2 iUV = ivec2(uv.x * viewPortSize.x, uv.y * viewPortSize.y);
	
	//uint bufferInfo = imageAtomicAdd(IntermediateBuffer, iUV, 0);


	uint screenWidth = uint( viewPortSize.x );
	uint screenHeight = uint( viewPortSize.y );
		
	uint index = iUV.y * screenWidth +  iUV.x;

	uint bufferInfo = Data[index];
	
	bool bIsInterect = false;

	uint CoordSys;
	vec2 offset = unPacked(bufferInfo, viewPortSize.xy, CoordSys).xy;


	float depth = texture(sampler2D(DepthTexture, defaultSampler), uv).r;

	vec4 worldPos = getWorldPosition(uv, depth);

	vec4 HitPos_WS;
	vec2 UVforNormalMap = vec2(0.0);

	vec4 minHitPos_WS;
	vec2 minUVforNormalMap = UVforNormalMap;
	
	bool bUseNormal = useNormalMap > 0.5 ? true : false;

	float minDist = 1000000.0;

	
	int shownedReflector = -1;


	//Check if current pixel is in the bound of planars
	for(int i = 0; i < numPlanes; i++)
	{	
		if(intersectPlane( i, worldPos.xyz, uv, HitPos_WS, UVforNormalMap))
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
		//imageAtomicMax(IntermediateBuffer, iUV, UINT_MAX);
		atomicMax(Data[index], UINT_MAX);
		return;
	}	
		

	vec2 relfectedUV = uv + offset.xy;

	float offsetLen = FLT_MAX;

	if(bufferInfo < UINT_MAX)
	{		
		//values correction
		float correctionPixel = 1.0;

		if(CoordSys == 0)
			relfectedUV = relfectedUV.xy + vec2(0.0, correctionPixel/viewPortSize.y);
		else if(CoordSys == 1)
			relfectedUV = relfectedUV.xy + vec2(correctionPixel/viewPortSize.x, 0.0);
		else if(CoordSys == 2)
			relfectedUV = relfectedUV.xy - vec2(0.0, correctionPixel/viewPortSize.y);
		else if(CoordSys == 3)
			relfectedUV = relfectedUV.xy - vec2(correctionPixel/viewPortSize.x, 0.0);

		offsetLen = length(offset.xy);
	
		outColor = texture(sampler2D(SceneTexture, defaultSampler), relfectedUV);
		
		if(useFadeEffect > 0.5 )
			outColor *= fade(relfectedUV);
	
		outColor.w = offsetLen;

	}	
	else
	{
		//outColor.rgb += vec3(1.0, 0.0, 0.0);
		outColor.w = FLT_MAX;
	}
		
	//clear IntermediateBuffer
	//imageAtomicMax(IntermediateBuffer, iUV, UINT_MAX);
	atomicMax(Data[index], UINT_MAX);
}
